#pragma once
#include <cstdint>
namespace kfvr::native {
struct NativeProbeConfig {
 std::uint32_t bytes=828,version=3,durationMs=12000,maxFrames=180;
 std::uint32_t controller=0,controllerIndex=0,viewport=0,level=0,levelIndex=0;
 std::uint32_t levelOffset=0,playerOffset=0,netModeOffset=0;
 std::uint32_t targetTrial=0;
 std::uint8_t prologues[9][16]{};
 std::uint8_t targetPrologues[7][16]{};
 wchar_t reportPath[260]{};
};
static_assert(sizeof(NativeProbeConfig)==828);
}
