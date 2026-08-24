#pragma once
#include "Vector.h"
#include <cstdint>
#include <optional>

// ============== 平滑自瞄函数声明 ==============
// 找最佳瞄准目标：返回敌人眼睛坐标，失败返回空
std::optional<Vector3> FindBestTarget(uintptr_t client, uintptr_t localPawn, float aimFov);

// 平滑自瞄核心：传入目标眼睛坐标，自动计算并写入视角
void SmoothAim(Vector3 localEye, Vector3 targetHead, float smoothStrength, float deadzone);

// 自瞄主入口，每帧调用一次
#pragma once
//新增这一行
DWORD WINAPI Aim_Start(LPVOID lpParam);

