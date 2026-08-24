#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>
#include "offsets.hpp"
#include "esp.h"
#include "client_dll.hpp"
#include "buttons.hpp"
#include "trigger.h"

#pragma comment(lib, "winmm.lib")

#define SAFE_READ_BEGIN __try {
#define SAFE_READ_END } __except(EXCEPTION_EXECUTE_HANDLER) { continue; }

DWORD WINAPI trigger(LPVOID lpParam) {
    timeBeginPeriod(1);

    while (!GetModuleHandleW(L"client.dll")) {
        Sleep(500);
    }

    const auto client = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
    DWORD last_shot_time = 0;
    const DWORD shot_interval = 600;

    while (true) {
        Sleep(1);

        SAFE_READ_BEGIN

            uintptr_t local_ctrl = *reinterpret_cast<uintptr_t*>(
                client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
        if (!local_ctrl) continue;

        uint32_t local_hPawn = *reinterpret_cast<uint32_t*>(
            local_ctrl + cs2_dumper::schemas::client_dll::CBasePlayerController::m_hPawn);
        if (local_hPawn == 0 || local_hPawn == 0xFFFFFFFFU) continue;

        uintptr_t local_pawn = GetBaseEntityFromHandle(local_hPawn, client);
        if (!local_pawn) continue;

        int local_health = *reinterpret_cast<int*>(
            local_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
        if (local_health <= 0) continue;

        int local_team = *reinterpret_cast<int*>(
            local_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

        // FIX: m_iIDEntIndex 是 handle，不是 index
        uint32_t cross_ent_handle = *reinterpret_cast<uint32_t*>(
            local_pawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iIDEntIndex);
        if (cross_ent_handle == 0 || cross_ent_handle == 0xFFFFFFFFU) continue;

        uintptr_t target_pawn = GetBaseEntityFromHandle(cross_ent_handle, client);
        if (!target_pawn) continue;

        int target_team = *reinterpret_cast<int*>(
            target_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
        if (target_team == local_team) continue;

        int target_health = *reinterpret_cast<int*>(
            target_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
        if (target_health <= 0 || target_health > 100) continue;

        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
        {
            DWORD current_time = GetTickCount();
            if (current_time - last_shot_time >= shot_interval)
            {
                int* attack = reinterpret_cast<int*>(client + cs2_dumper::buttons::attack);
                // FIX: CS2 attack values
                *attack = 65537;
                Sleep(25);
                *attack = 256;

                last_shot_time = current_time;
            }
        }

        SAFE_READ_END
    }

    timeEndPeriod(1);
    return 0;
}
