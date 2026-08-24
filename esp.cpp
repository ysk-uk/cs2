#define NOMINMAX  // 禁止Windows.h定义max/min宏
#define WIN32_LEAN_AND_MEAN
#include "gui.h"
#include "offsets.hpp"
#include "client_dll.hpp"
#include <Windows.h>
#include"Vector.h"
#include <cmath>
#include <optional>
#include <imgui.h>
#include <cstdio>
#include <list>
#include <vector>   // 缺失这个头文件，std::vector直接报不存在
#include "esp.h"

// ============================================================================
//  GetBaseEntity  根据实体索引获取实体对象地址
//  (FIXED: chunk array at entityList + 0x10, not + 0x80)
//  BUG WAS: + 16  →  wrong, should be + 2 (or + 0x10 in byte offset)
// ============================================================================
/// <summary>
/// 通过实体索引从EntityList实体列表获取实体地址
/// CS2 Source2实体列表分块存储(chunk)，索引高9位找chunk块，低9位找块内实体
/// </summary>
/// <param name="index">实体数组索引</param>
/// <param name="client">client.dll模块基址</param>
/// <returns>实体对象uintptr_t地址，0代表无效</returns>
uintptr_t GetBaseEntity(int index, uintptr_t client)
{
	// 读取实体列表根指针 dwEntityList
	auto enListBase = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwEntityList);
	if (enListBase == 0) {
		return 0;
	}
	// FIXED: was + 16, now + 2 (chunk array starts at offset 0x10 = 2 * sizeof(uintptr_t))
	// index >>9：取索引高9位，定位到对应的chunk块；0x10是chunk数组起始偏移，每个chunk指针占8字节
	auto entityChunk = *reinterpret_cast<uintptr_t*>(enListBase + 0x10 + 0x8 * (index >> 9));
	if (entityChunk == 0) {
		return 0;
	}
	// index & 0x1FF：取索引低9位，在chunk内部偏移，每个实体条目0x70字节
	return *reinterpret_cast<uintptr_t*>(entityChunk + 0x70 * (index & 0x1FF));
}

/// <summary>
/// 通过实体句柄Handle解析出实体真实地址
/// 句柄低15位 = 实体索引；高部分为版本号
/// </summary>
/// <param name="uHandle">实体句柄 m_hXXX</param>
/// <param name="client">client.dll基址</param>
/// <returns>实体地址，0无效</returns>
uintptr_t GetBaseEntityFromHandle(uint32_t uHandle, uintptr_t client)
{
	if (uHandle == 0 || uHandle == 0xFFFFFFFF) return 0;

	const uintptr_t entListBase = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwEntityList);
	if (entListBase == 0) {
		return 0;
	}
	const int nIndex = uHandle & 0x7FFF; // 句柄掩码取出实体索引
	// FIXED: was + 16, now + 2
	auto entitylistbase = *reinterpret_cast<uintptr_t*>(entListBase + 0x10 + 0x8 * (nIndex >> 9));
	if (entitylistbase == 0) {
		return 0;
	}
	return *reinterpret_cast<std::uintptr_t*>(entitylistbase + 0x70 * (uHandle & 0x1FF));
}

// ============================================================================
//  GetEyePos 获取pawn眼睛世界3D坐标
// ============================================================================
/// <summary>
/// 获取玩家pawn眼睛世界坐标 = m_vOldOrigin脚底坐标 + m_vecViewOffset眼睛偏移
/// </summary>
/// <param name="addr">C_BasePlayerPawn实体地址</param>
/// <returns>std::optional<Vector3>，失败返回nullopt</returns>
std::optional<Vector3> GetEyePos(uintptr_t addr) noexcept {
	if (addr == 0) return std::nullopt;
	// 读取pawn脚底世界坐标
	auto* Origin = reinterpret_cast<Vector3*>(addr + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
	// 读取眼睛相对脚底偏移，站立约(0,0,64)，蹲下数值自动变化
	auto* Viewoffset = reinterpret_cast<Vector3*>(addr + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset);
	Vector3 LocalEye = *Origin + *Viewoffset;

	// 校验坐标是否为合法浮点数，防止NaN/inf脏数据
	if (!std::isfinite(LocalEye.x) || !std::isfinite(LocalEye.y) || !std::isfinite(LocalEye.z))
		return std::nullopt;

	// 坐标长度过小判定为无效
	if (LocalEye.Length() < 0.1f)
		return std::nullopt;

	return LocalEye;

}

/// <summary>
/// 根据骨骼索引读取骨骼关节世界坐标
/// </summary>
/// <param name="addr">pawn实体地址</param>
/// <param name="index">骨骼索引BoneIndex</param>
/// <returns>骨骼3D世界坐标Vector3</returns>
Vector3 BonePos(uintptr_t addr, int32_t index)
{
	int32_t d = 32 * index; // 每根骨骼矩阵存储步长32字节
	uintptr_t address{};
	// 获取GameSceneNode节点
	address = *reinterpret_cast<uintptr_t*>(addr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
	if (!address)
	{
		return Vector3();
	}
	// 获取模型骨骼数组基地址
	auto BoneArray = cs2_dumper::schemas::client_dll::CSkeletonInstance::m_modelState + 0x80;
	address = *reinterpret_cast<uintptr_t*>(address + BoneArray);
	if (!address)
	{
		return Vector3();
	}
	// 取出骨骼世界坐标
	return *reinterpret_cast<Vector3*>(address + d);
}

// ============================================================================
//  WorldToScreen 3D世界坐标转2D屏幕像素坐标 W2S
// ============================================================================
/// <summary>
/// 复刻D3D透视变换，把游戏世界3D点转为屏幕2D像素
/// </summary>
/// <param name="pWorldPos">输入3D世界坐标</param>
/// <param name="pScreenPos">输出屏幕坐标，z无用置0</param>
/// <param name="pMatrixPtr">ViewMatrix视图矩阵指针，16个float</param>
/// <param name="pWinWidth">窗口宽度像素</param>
/// <param name="pWinHeight">窗口高度像素</param>
/// <returns>false=点在相机背后，不应该绘制；true转换成功</returns>
bool WorldToScreen(Vector3 pWorldPos, Vector3& pScreenPos, float* pMatrixPtr, const FLOAT pWinWidth, const FLOAT pWinHeight)
{
	float matrix2[4][4];

	memcpy(matrix2, pMatrixPtr, 16 * sizeof(float));

	const float mX{ pWinWidth / 2 }; // 屏幕半宽
	const float mY{ pWinHeight / 2 }; // 屏幕半高

	// w分量，用于透视除法；w过小代表物体在摄像机背后
	const float w{
		matrix2[3][0] * pWorldPos.x +
		matrix2[3][1] * pWorldPos.y +
		matrix2[3][2] * pWorldPos.z +
		matrix2[3][3] };

	if (w < 0.65f) return false;
	// 矩阵相乘得到相机空间坐标
	const float x{
		matrix2[0][0] * pWorldPos.x +
		matrix2[0][1] * pWorldPos.y +
		matrix2[0][2] * pWorldPos.z +
		matrix2[0][3] };

	const float y{
	 matrix2[1][0] * pWorldPos.x +
			matrix2[1][1] * pWorldPos.y +
		matrix2[1][2] * pWorldPos.z +
			matrix2[1][3] };

	// 透视除法 + NDC转屏幕像素，DX坐标系Y轴翻转
	pScreenPos.x = (mX + mX * x / w);
	pScreenPos.y = (mY - mY * y / w);
	pScreenPos.z = 0;

	return true;
}

/// <summary>
/// 骨骼点列表转换屏幕坐标，循环绘制线段
/// </summary>
/// <param name="list">世界坐标骨骼点集合</param>
/// <param name="Color">线条颜色ImColor</param>
/// <param name="Matrix">ViewMatrix视图矩阵</param>
void DrawLine(std::vector<Vector3> list, ImColor Color, float* Matrix)
{
	Vector3 drawpos;
	std::vector<Vector3> Drawlist{};
	// 全部3D骨骼点转为屏幕2D点，过滤W2S失败(相机背后)的点
	for (int i = 0; i < list.size(); ++i)
	{
		if (!WorldToScreen(list[i], drawpos, Matrix, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y))
			continue;

		Drawlist.push_back(drawpos);
	}
	// 依次绘制相邻两点之间的线段
	for (int i = 1; i < Drawlist.size(); ++i)
	{
		ImGui::GetBackgroundDrawList()->AddLine(ImVec2(Drawlist[i].x, Drawlist[i].y), ImVec2(Drawlist[i - 1].x, Drawlist[i - 1].y), Color);
	}
}

/// <summary>
/// 骨骼ESP入口函数：组装身体各部分骨骼点，调用DrawLine绘制人形骨架
/// </summary>
/// <param name="pawn">目标玩家Pawn实体地址</param>
/// <param name="BoneColor">骨骼线条颜色</param>
/// <param name="Matrix">ViewMatrix视图矩阵</param>
void Bone_Start(uintptr_t pawn, ImColor BoneColor, float* Matrix) {

	BoneDrawList.clear();
	BoneDrawList.clear();
	// 头部 -> 脖子 -> 脊柱 -> 骨盆
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::head));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::neck_0));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::spine_2));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::pelvis));
	DrawLine(BoneDrawList, BoneColor, Matrix);

	// 左上肢：脖子 → 左上臂 → 左小臂 → 左手
	BoneDrawList.clear();
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::neck_0));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::arm_upper_L));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::arm_lower_L));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::hand_L));
	DrawLine(BoneDrawList, BoneColor, Matrix);

	// 右上肢：脖子 → 右上臂 → 右小臂 → 右手
	BoneDrawList.clear();
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::neck_0));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::arm_upper_R));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::arm_lower_R));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::hand_R));
	DrawLine(BoneDrawList, BoneColor, Matrix);

	// 左下肢：骨盆 → 左大腿 → 左小腿 → 左脚踝
	BoneDrawList.clear();
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::pelvis));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::leg_upper_L));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::leg_lower_L));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::ankle_L));
	DrawLine(BoneDrawList, BoneColor, Matrix);

	// 右下肢：骨盆 → 右大腿 → 右小腿 → 右脚踝
	BoneDrawList.clear();
	BoneDrawList.clear();
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::pelvis));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::leg_upper_R));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::leg_lower_R));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::ankle_R));
	DrawLine(BoneDrawList, BoneColor, Matrix);
}






// ============================================================================
//  ESP_Start  ESP主循环入口 (with debug counters)
// ============================================================================
/// <summary>
/// ESP主逻辑，每一帧调用；遍历实体列表，过滤敌人，W2S转换，绘制方框、骨骼
/// </summary>
void ESP_Start() {
	// 获取client.dll模块基址
	const auto client = reinterpret_cast<uintptr_t>(GetModuleHandle(L"client.dll"));

	// 读取本地玩家Pawn实体 dwLocalPlayerPawn
	auto local_player_pawn = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
	if (local_player_pawn == 0) {
		return;
	}

	// 读取本地玩家血量、队伍号
	int local_health = *reinterpret_cast<int*>(local_player_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
	auto localplay_team = *reinterpret_cast<int*>(local_player_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

	// 读取ViewMatrix视图矩阵，每一帧必须重新读取，镜头转动矩阵会变化
	auto Matrix = reinterpret_cast<float*>(client + cs2_dumper::offsets::client_dll::dwViewMatrix);
	if (!Matrix) {
		return;
	}


	// 遍历玩家Controller，最大64个玩家
	for (int i = 0; i < 64; i++) {

		auto player_co = GetBaseEntity(i, client);

		if (!player_co) {
			continue;
		}
	

		// Controller读取m_hPawn句柄，拿到对应的pawn实体
		auto player_hpawn = *reinterpret_cast<uint32_t*>(player_co + cs2_dumper::schemas::client_dll::CBasePlayerController::m_hPawn);
		if (player_hpawn == 0 || player_hpawn == 0xFFFFFFFFU) continue;

		auto player_pawn = GetBaseEntityFromHandle(player_hpawn, client);

		// 过滤空pawn、过滤自己本地pawn
		if (player_pawn == 0 || player_pawn == local_player_pawn || player_hpawn == 0) {
			if (player_pawn == local_player_pawn) 
			continue;
		}
	

		// 读取敌人队伍编号，友军直接跳过
		auto player_team = *reinterpret_cast<int*>(player_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
		if (localplay_team == player_team) {
		
			continue;
		}

		// 读取血量，死亡直接跳过
		auto player_health = *reinterpret_cast<int*>(player_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
		if (player_health <= 0) {
		
			continue;
		}
	

		// 读取敌人脚底世界坐标
		auto player_Origin = *reinterpret_cast<Vector3*>(player_pawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
		// 获取敌人眼睛世界坐标
		auto play_eyepos_op_vec = GetEyePos(player_pawn);
		if (!play_eyepos_op_vec.has_value()) {
			continue;
		}

		auto play_eyepos = play_eyepos_op_vec.value();
		// ImGui窗口宽高，用于W2S坐标换算
		const float w = ImGui::GetIO().DisplaySize.x;
		const float h = ImGui::GetIO().DisplaySize.y;
		Vector3 head_pos_2d{};	// 敌人眼睛屏幕2D点
		Vector3 abs_pos_2d{};	// 敌人脚底屏幕2D点

		// 3D转2D屏幕，转换失败代表在相机背后，直接跳过绘制
		if (!WorldToScreen(player_Origin, abs_pos_2d, Matrix, w, h)) continue;
		if (!WorldToScreen(play_eyepos, head_pos_2d, Matrix, w, h)) continue;
	
		// 计算方框像素宽高；*1.5f为放大系数，会让方框比真实人物更高
		const float height = ::abs(head_pos_2d.y - abs_pos_2d.y) * 1.5f;
		const float width = height * 0.5f;
		// 方框左上角X、Y坐标
		const float x = head_pos_2d.x - (width / 2.f);
		const float y = head_pos_2d.y - (width / 2.5f);

		// 绘制方框，开关zeroflick::visuals::box控制
		if (zeroflick::visuals::box) { ImGui::GetBackgroundDrawList()->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), ImColor(255, 0, 0, 255), 0.f, 0, 2.0f); }
		// 绘制骨骼ESP，开关zeroflick::visuals::Bone_Start控制
		if (zeroflick::visuals::Bone_Start && player_pawn != 0) { Bone_Start(player_pawn, ImColor(155, 155, 155, 255), Matrix); }
	}
}
