#pragma once
#include "kfvr/runtime/FrameProvider.h"
#include <filesystem>
#include "kfvr/xr/ControllerInput.h"
#include <string>
#include <vector>
namespace kfvr::xr {
using IFrameProvider=runtime::IFrameProvider;
struct HostOptions {
    std::filesystem::path loaderPath;
    bool probeOnly=true,waitForFirstPair=false;
    unsigned seconds=15;
    unsigned renderScalePercent=100;
    IFrameProvider* provider=nullptr;
    IInputConsumer* inputConsumer=nullptr;
};
struct HostReport {
    bool instanceCreated=false,systemAvailable=false,sessionCreated=false,okay=false;
    std::string runtimeName,systemName,error;
    runtime::PresentationRequirements requirements{};
    std::uint32_t minimumFeatureLevel=0;
    // Enumeration requires a session. A session-free --probe leaves this false.
    bool swapchainFormatsEnumerated=false;
    std::vector<std::int64_t> advertisedSwapchainFormats;
    std::uint64_t waitedFrames=0,endedFrames=0,validTrackingFrames=0,submittedPairs=0,rejectedPairs=0;
    std::int32_t lastSessionState=0;
    unsigned suggestedControllerProfiles=0;
    std::uint64_t validControllerSamples=0,focusedFrames=0,reprojectedFrames=0,emptyFrames=0,maxEmptyBetweenFresh=0;
};
// Uses the installed active runtime; never changes the registry or game settings.
HostReport RunOpenXrHost(const HostOptions&) noexcept;
}
