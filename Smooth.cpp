#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "offsets.hpp"
#include "client_dll.hpp"
#include "esp.h"
#include <cmath>
#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>
#include "imgui_d11/imgui.h"
#include "gui.h"
#include "Vector.h"

constexpr float M_PI = 3.14159265358979323846f;

// ==================== 工具函数：角度归一化 ====================
static float NormalizeAngle(float ang)
{
	while (ang > 180.0f)  ang -= 360.0f;
	while (ang < -180.0f) ang += 360.0f;
	return ang;
}

// ==================== 工具函数：向量转欧拉角 ====================
static void VectorToAngle(const Vector3& dir, float& outPitch, float& outYaw)
{
	float hypotenuse = std::sqrt(dir.x * dir.x + dir.y * dir.y);
	outYaw = std::atan2(dir.y, dir.x) * (180.0f / M_PI);
	outPitch = -std::atan2(dir.z, hypotenuse) * (180.0f / M_PI);
}

// ==================== 工具函数：计算加权 FOV（更贴合实战） ====================
static float CalcFov(float curPitch, float curYaw, float tarPitch, float tarYaw)
{
	float dp = std::fabs(NormalizeAngle(tarPitch - curPitch));
	float dy = std::fabs(NormalizeAngle(tarYaw - curYaw));
	//pitch 权重降低，垂直方向灵敏度通常更低，优先按水平角度选目标
	return std::sqrt(dy * dy + dp * dp * 0.6f);
}

// ==================== 筛选最佳目标 ====================
std::optional<Vector3> FindBestTarget(uintptr_t client, uintptr_t localPawn, float aimFov)
{
	auto localTeam = *reinterpret_cast<int*>(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
	auto localEyeOpt = GetEyePos(localPawn);
	if (!localEyeOpt.has_value()) return std::nullopt;
	Vector3 localEye = localEyeOpt.value();

	float* viewAngles = reinterpret_cast<float*>(client + cs2_dumper::offsets::client_dll::dwViewAngles);
	float curPitch = viewAngles[0];
	float curYaw = viewAngles[1];

	float bestScore = 9999.0f;
	std::optional<Vector3> bestTarget = std::nullopt;

	// 遍历实体：0~63 是玩家控制器范围
	for (int i = 0; i < 64; i++)
	{
		uintptr_t playerCo = GetBaseEntity(i, client);
		if (!playerCo) continue;

		uint32_t hPawn = *reinterpret_cast<uint32_t*>(playerCo + cs2_dumper::schemas::client_dll::CBasePlayerController::m_hPawn);
		if (hPawn == 0 || hPawn == 0xFFFFFFFFU) continue;

		uintptr_t playerPawn = GetBaseEntityFromHandle(hPawn, client);
		if (!playerPawn || playerPawn == localPawn) continue;

		// 基础过滤：队友、死亡
		int team = *reinterpret_cast<int*>(playerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
		int health = *reinterpret_cast<int*>(playerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
		if (team == localTeam || health <= 0 || health > 100) continue;

		auto enemyEyeOpt = GetEyePos(playerPawn);
		if (!enemyEyeOpt.has_value()) continue;
		Vector3 enemyEye = enemyEyeOpt.value();

		// 瞄准点：头部中心（下移1单位，避免瞄头顶）
		Vector3 aimPoint = enemyEye;
		aimPoint.z -= 1.0f;

		// 计算目标角度
		Vector3 dir = { aimPoint.x - localEye.x, aimPoint.y - localEye.y, aimPoint.z - localEye.z };
		float tarPitch, tarYaw;
		VectorToAngle(dir, tarPitch, tarYaw);

		// 加权FOV计算：水平方向权重更高，更符合实战瞄准习惯
		float deltaP = std::fabs(NormalizeAngle(tarPitch - curPitch));
		float deltaY = std::fabs(NormalizeAngle(tarYaw - curYaw));
		float fov = std::sqrt(deltaY * deltaY + deltaP * deltaP * 0.6f);

		// 超出最大FOV直接跳过
		if (fov > aimFov) continue;

		// ===== 优化：综合评分选目标 =====
		// 相同FOV下，距离更近的敌人优先级更高
		float distance = dir.Length();
		float score = fov + distance * 0.05f;

		if (score < bestScore)
		{
			bestScore = score;
			bestTarget = aimPoint;
		}
	}
	return bestTarget;
}
// ==================== 核心：自适应平滑自瞄 ====================
void SmoothAim(Vector3 localEye, Vector3 targetHead, float maxSmooth, float deadzone)
{
	const auto client = reinterpret_cast<uintptr_t>(GetModuleHandle(L"client.dll"));
	float* viewAnglesPtr = reinterpret_cast<float*>(client + cs2_dumper::offsets::client_dll::dwViewAngles);

	float curPitch = viewAnglesPtr[0];
	float curYaw = viewAnglesPtr[1];

	Vector3 dir = {
	targetHead.x - localEye.x,
	targetHead.y - localEye.y,
	targetHead.z - localEye.z
	};

	float tarPitch, tarYaw;
	VectorToAngle(dir, tarPitch, tarYaw);

	float deltaPitch = NormalizeAngle(tarPitch - curPitch);
	float deltaYaw = NormalizeAngle(tarYaw - curYaw);

	// 死区过滤
	if (std::fabs(deltaPitch) < deadzone) deltaPitch = 0.0f;
	if (std::fabs(deltaYaw) < deadzone) deltaYaw = 0.0f;

	// 自适应平滑系数
	float totalDelta = std::sqrt(deltaPitch * deltaPitch + deltaYaw * deltaYaw);
	float smoothFactor = maxSmooth;
	if (totalDelta < 10.0f)
	{
		smoothFactor = maxSmooth * (totalDelta / 10.0f) * 0.7f + 0.02f;
	}

	float newPitch = curPitch + deltaPitch * smoothFactor;
	float newYaw = curYaw + deltaYaw * smoothFactor;

	newPitch = std::clamp(newPitch, -89.0f, 89.0f);
	newYaw = NormalizeAngle(newYaw);

	// ===========================================

	viewAnglesPtr[0] = newPitch;
	viewAnglesPtr[1] = newYaw;

}

// ==================== 自瞄主线程 ====================
DWORD WINAPI Aim_Start(LPVOID lpParam)
{
	// 提高时钟精度，循环更稳定

	while (true)
	{
		if (!zeroflick::visuals::Aim_Start)
		{
			Sleep(1);
			continue;
		}

		const auto client = reinterpret_cast<uintptr_t>(GetModuleHandle(L"client.dll"));
		uintptr_t localPawn = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
		if (!localPawn)
		{
			Sleep(1);
			continue;
		}

		// 配置参数
		const float aimFov = 35.0f;
		const float maxSmooth = 0.12f;  // 最大平滑系数，大角度拉枪速度
		const float deadzone = 0.15f;   // 死区调小，精度更高

		auto targetOpt = FindBestTarget(client, localPawn, aimFov);
		if (!targetOpt.has_value())
		{
			Sleep(1);
			continue;
		}

		auto localEyeOpt = GetEyePos(localPawn);
		if (!localEyeOpt.has_value())
		{
			Sleep(1);
			continue;
		}

		SmoothAim(localEyeOpt.value(), targetOpt.value(), maxSmooth, deadzone);

		// 高精度短延时，比纯 Sleep (1) 更稳
		Sleep(0);

	}

	return 0;

}