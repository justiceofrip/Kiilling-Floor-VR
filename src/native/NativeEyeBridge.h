#pragma once
#include "kfvr/ipc/FrameChannel.h"
#include "kfvr/graphics/D3D9EyeTransfer.h"
#include <wrl/client.h>
#include <filesystem>
namespace kfvr::native {
using NativeSurfaces=std::array<Microsoft::WRL::ComPtr<IDirect3DSurface9>,2>;
enum class EyeRenderResult {Complete,FailedRestored,FailedUnsafe};
enum class EyeBridgeResult {NoPair,PairReady,DesktopReplaced,FailedRestored,FailedUnsafe};
using EyeRenderer=EyeRenderResult(*)(void*,void*,const runtime::PresentationRequirements&,const runtime::TrackingFrame&,NativeSurfaces&);
bool StartEyeBridge(const wchar_t* token,bool suppressDesktop=false);
bool EyeBridgeDesktopReady() noexcept;
bool EyeBridgeStateUnsafe() noexcept;
EyeBridgeResult EyeBridgeLevel(void* scene,void* ri,IDirect3DDevice9*,EyeRenderer,bool replaceDesktop=false) noexcept;
void EyeBridgePresent(bool worldValid) noexcept;
bool ReadEyeBridgeControls(xr::ControllerFrame&,std::uint64_t&) noexcept;
bool SendEyeBridgeHaptics(std::uint64_t,const std::array<xr::HapticPulse,2>&) noexcept;
void StopEyeBridge() noexcept;
bool EyeBridgeClosed() noexcept;
void WriteEyeBridgeReport(const std::filesystem::path&);
}
