#pragma once
#include <Windows.h>
#include <filesystem>
#include "kfvr/engine/Ue2Camera.h"
#include "kfvr/runtime/FrameProvider.h"
namespace kfvr::native {
void SetCameraMethods(void* constructor,void* destructor,void* renderer);
bool RenderDiagnosticCamera(void* original,void* target,void* ri,float offset,unsigned mode,ue2::SceneViewData& captured);
bool RenderTrackedCamera(void* original,void* target,void* ri,const runtime::TrackingFrame&,unsigned eye,unsigned width,unsigned height,float unitsPerMetre);
void ProbeCameras(void* original,void* target);
void WriteCameraReport(const std::filesystem::path& folder);
}
