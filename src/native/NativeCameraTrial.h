#pragma once
#include <Windows.h>
#include "NativeProbeConfig.h"
#include <filesystem>
#include "kfvr/engine/Ue2Camera.h"
#include "kfvr/runtime/FrameProvider.h"
namespace kfvr::native {
struct TrackedBody {math::Matrix4 camera{};math::Vec3 pawnLocation{};float eyeHeight=0;ue2::Rotator heading{};};
void ConfigureTrackedBody(const NativeProbeConfig&);
std::optional<TrackedBody> ReadTrackedBody();
void SetCameraMethods(void* constructor,void* destructor,void* renderer);
bool RenderDiagnosticCamera(void* original,void* target,void* ri,float offset,unsigned mode,ue2::SceneViewData& captured);
bool RenderTrackedCamera(void* original,void* target,void* ri,const runtime::TrackingFrame&,unsigned eye,unsigned width,unsigned height,float unitsPerMetre);
void ProbeCameras(void* original,void* target);
void WriteCameraReport(const std::filesystem::path& folder);
}
