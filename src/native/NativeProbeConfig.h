#pragma once
#include <cstdint>
namespace kfvr::native {
struct NativeProbeConfig {
 std::uint32_t bytes=1048,version=8,durationMs=12000,maxFrames=180;
 std::uint32_t controller=0,controllerIndex=0,viewport=0,level=0,levelIndex=0;
 std::uint32_t levelOffset=0,playerOffset=0,netModeOffset=0;
 // stereoTrial: 0 disabled, 1 parallel native, 2 asymmetric, 3 synthetic head pose.
 std::uint32_t targetTrial=0,cameraTrial=0,stereoTrial=0;
 std::uint8_t prologues[9][16]{};
 std::uint8_t targetPrologues[7][16]{};
 std::uint8_t cameraPrologues[3][16]{};
 wchar_t reportPath[260]{};
 // Explicit opt-in bounded IPC trial, scale remains diagnostic until calibrated.
 std::uint32_t bridgeTrial=0;float unitsPerMetre=50;wchar_t channelToken[34]{};
 std::uint32_t inputTrial=0,inputOffsets[10]{},playerTickName=0;std::uint8_t inputPrologue[16]{};
 std::uint32_t movementTrial=0,pawnOffset=0,healthOffset=0,pawnControllerOffset=0,pauserOffset=0,locationOffset=0;
};
static_assert(sizeof(NativeProbeConfig)==1048);
}
