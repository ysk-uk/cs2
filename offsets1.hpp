// 由 cs2-dumper 工具自动生成（https://github.com/a2x/cs2-dumper）
// 生成时间：2026-08-21 02:23:50 UTC
#pragma once                          // 头文件保护宏，防止头文件被重复包含
#include <cstddef>                    // 引入标准库，提供 std::ptrdiff_t 等指针相关类型
#include <cstdint>                    // 引入标准库，提供 int8_t/uint64_t 等标准整数类型

namespace cs2_dumper {
    namespace offsets {
   
        namespace client_dll {
            // 含义：CSGO输入系统全局对象偏移，存储鼠标/键盘输入状态、视角操作底层数据
            constexpr std::ptrdiff_t dwCSGOInput = 0x23BFB20;
            // 含义：实体列表（旧兼容接口）偏移，用于遍历所有游戏实体（玩家、武器、道具等）
            constexpr std::ptrdiff_t dwEntityList = 0x2555050;
            // 含义：游戏实体系统全局对象偏移，CS2 新版实体管理核心接口，替代旧版实体列表
            constexpr std::ptrdiff_t dwGameEntitySystem = 0x2555050;
            // 含义：实体系统中「最高实体索引」的成员偏移，用于实体遍历时的边界判断
            constexpr std::ptrdiff_t dwGameEntitySystem_highestEntityIndex = 0x2090;
            // 含义：游戏规则全局对象偏移，存储回合状态、胜负判定、游戏模式等全局规则数据
            constexpr std::ptrdiff_t dwGameRules = 0x23A9BD8;
            // 含义：全局变量结构体偏移，存储游戏时间、帧率、当前地图等全局运行参数
            constexpr std::ptrdiff_t dwGlobalVars = 0x2095D48;
            // 含义：发光管理器偏移，控制实体发光（Glow）效果，是发光透视功能的核心地址
            constexpr std::ptrdiff_t dwGlowManager = 0x23A6908;
            // 含义：本地玩家控制器（PlayerController）偏移，代表本地玩家的控制层实体
            constexpr std::ptrdiff_t dwLocalPlayerController = 0x2384DB0;
            // 含义：本地玩家 Pawn 偏移，代表本地玩家的游戏角色实体（包含坐标、血量、骨骼等核心数据）
            constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x23AA118;
            // 含义：已安放 C4 炸弹的实体偏移，用于获取炸弹位置、爆炸倒计时等数据
            constexpr std::ptrdiff_t dwPlantedC4 = 0x2374898;
            // 含义：预测系统偏移，存储本地玩家的预测数据，用于消除本地位同步闪烁
            constexpr std::ptrdiff_t dwPrediction = 0x23AA020;
            // 含义：鼠标灵敏度设置对象偏移，存储游戏内灵敏度配置
            constexpr std::ptrdiff_t dwSensitivity = 0x23A7428;
            // 含义：灵敏度对象中「实际灵敏度数值」的成员偏移
            constexpr std::ptrdiff_t dwSensitivity_sensitivity = 0x58;
            // 含义：视角角度偏移，存储玩家的俯仰/偏航视角，是自瞄功能的核心地址
            constexpr std::ptrdiff_t dwViewAngles = 0x23C01A8;
            // 含义：视图矩阵偏移，存储 3D 世界坐标转 2D 屏幕坐标的转换矩阵，ESP 透视的核心数据
            constexpr std::ptrdiff_t dwViewMatrix = 0x23AF550;
            // 含义：视图渲染对象偏移，游戏画面渲染的全局管理器
            constexpr std::ptrdiff_t dwViewRender = 0x23AF5A8;
            // 含义：C4 武器实体偏移，对应玩家背包中携带的 C4 炸弹武器数据
            constexpr std::ptrdiff_t dwWeaponC4 = 0x2322DA0;
        }

        // ==============================================
        // 模块：engine2.dll（引擎核心模块）
        // 负责网络通信、窗口管理、引擎底层渲染、服务器连接
        // ==============================================
        namespace engine2_dll {
            // 含义：游戏构建版本号偏移，用于校验游戏版本、判断偏移是否过期
            constexpr std::ptrdiff_t dwBuildNumber = 0x60F594;
            // 含义：网络游戏客户端对象偏移，管理客户端与服务器的全部网络通信
            constexpr std::ptrdiff_t dwNetworkGameClient = 0x90D4B0;
            // 含义：网络客户端中「客户端 tick 计数」的成员偏移，本地客户端的帧计数
            constexpr std::ptrdiff_t dwNetworkGameClient_clientTickCount = 0x378;
            // 含义：网络客户端中「delta tick」成员偏移，客户端与服务器的 tick 差值
            constexpr std::ptrdiff_t dwNetworkGameClient_deltaTick = 0x24C;
            // 含义：网络客户端中「后台地图」标志位偏移，判断地图是否处于后台加载状态
            constexpr std::ptrdiff_t dwNetworkGameClient_isBackgroundMap = 0x2C141F;
            // 含义：网络客户端中「本地玩家索引」成员偏移，标识本地玩家在服务器中的槽位
            constexpr std::ptrdiff_t dwNetworkGameClient_localPlayer = 0xF8;
            // 含义：网络客户端中「最大客户端数」成员偏移，当前服务器支持的最大玩家数量
            constexpr std::ptrdiff_t dwNetworkGameClient_maxClients = 0x240;
            // 含义：网络客户端中「服务器 tick 计数」成员偏移，服务器端的帧计数
            constexpr std::ptrdiff_t dwNetworkGameClient_serverTickCount = 0x24C;
            // 含义：网络客户端中「登录状态」成员偏移，判断客户端连接、进房的阶段状态
            constexpr std::ptrdiff_t dwNetworkGameClient_signOnState = 0x230;
            // 含义：游戏窗口高度偏移，获取当前游戏窗口的像素高度
            constexpr std::ptrdiff_t dwWindowHeight = 0x9118DC;
            // 含义：游戏窗口宽度偏移，获取当前游戏窗口的像素宽度
            constexpr std::ptrdiff_t dwWindowWidth = 0x9118D8;
        }

        // ==============================================
        // 模块：inputsystem.dll（输入系统模块）
        // 管理键盘、鼠标等硬件输入的采集与分发
        // ==============================================
        namespace inputsystem_dll {
            // 含义：输入系统全局对象偏移，可用于读取输入状态或模拟键鼠输入
            constexpr std::ptrdiff_t dwInputSystem = 0x45BA0;
        }

        // ==============================================
        // 模块：matchmaking.dll（匹配模块）
        // 负责游戏匹配、对局类型、段位系统相关逻辑
        // ==============================================
        namespace matchmaking_dll {
            // 含义：游戏类型列表偏移，存储所有可用的游戏模式、对局类型数据
            constexpr std::ptrdiff_t dwGameTypes = 0x1ADF80;
        }

        // ==============================================
        // 模块：soundsystem.dll（音效系统模块）
        // 负责游戏内音效播放、3D 音频定位
        // ==============================================
        namespace soundsystem_dll {
            // 含义：音效系统全局对象偏移，管理所有游戏音效的播放与空间定位
            constexpr std::ptrdiff_t dwSoundSystem = 0x54B5D0;
            // 含义：音效系统中「引擎视图数据」成员偏移，存储音频定位所需的视角位置信息
            constexpr std::ptrdiff_t dwSoundSystem_engineViewData = 0x7C;
        }
    }
}
