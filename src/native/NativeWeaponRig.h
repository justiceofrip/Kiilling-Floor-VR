#pragma once
#include "NativeProbeConfig.h"
#include "kfvr/runtime/FrameProvider.h"
#include <filesystem>
namespace kfvr::native {
void ConfigureWeaponRig(const NativeProbeConfig&);
void CaptureWeaponAnchor(const runtime::TrackingFrame&);
void UpdateWeaponRig(void* actor,void* function,bool worldActive) noexcept;
bool StopWeaponRig() noexcept;
void WriteWeaponRigReport(const std::filesystem::path&);
}
