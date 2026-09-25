#pragma once
#include "kfvr/ipc/FrameChannel.h"
#include "kfvr/graphics/D3D9CpuPair.h"
#include <wrl/client.h>
#include <filesystem>
namespace kfvr::native {
using NativeSurfaces=std::array<Microsoft::WRL::ComPtr<IDirect3DSurface9>,2>;
using EyeRenderer=bool(*)(void*,void*,const runtime::PresentationRequirements&,const runtime::TrackingFrame&,NativeSurfaces&);
bool StartEyeBridge(const wchar_t* token);
void EyeBridgeLevel(void* scene,void* ri,IDirect3DDevice9*,EyeRenderer) noexcept;
void EyeBridgePresent(bool worldValid) noexcept;
void StopEyeBridge() noexcept;
bool EyeBridgeClosed() noexcept;
void WriteEyeBridgeReport(const std::filesystem::path&);
}
