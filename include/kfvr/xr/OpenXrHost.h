#pragma once
#include "kfvr/runtime/FrameProvider.h"
#include <filesystem>
#include "kfvr/xr/ControllerInput.h"
#include <string>
namespace kfvr::xr {
using IFrameProvider=runtime::IFrameProvider;
struct HostOptions {
    std::filesystem::path loaderPath;
    bool probeOnly=true;
    unsigned seconds=15;
    IFrameProvider* provider=nullptr;
    IInputConsumer* inputConsumer=nullptr;
};
struct HostReport {
    bool instanceCreated=false,systemAvailable=false,sessionCreated=false,okay=false;
    std::string runtimeName,systemName,error;
    runtime::PresentationRequirements requirements{};
    std::uint32_t minimumFeatureLevel=0;
    std::uint64_t waitedFrames=0,endedFrames=0,validTrackingFrames=0,submittedPairs=0,rejectedPairs=0;
    std::int32_t lastSessionState=0;
    unsigned suggestedControllerProfiles=0;
    std::uint64_t validControllerSamples=0;
};
// Uses the installed active runtime; never changes the registry or game settings.
HostReport RunOpenXrHost(const HostOptions&) noexcept;
}
