#pragma once
#include <Windows.h>
#include <filesystem>
#include "kfvr/engine/Ue2Camera.h"
namespace kfvr::native {
void SetCameraMethods(void* constructor,void* destructor,void* renderer);
bool RenderDiagnosticCamera(void* original,void* target,void* ri,float offset,unsigned mode,ue2::SceneViewData& captured);
void ProbeCameras(void* original,void* target);
void WriteCameraReport(const std::filesystem::path& folder);
}
