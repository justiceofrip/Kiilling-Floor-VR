#pragma once
#include <Windows.h>
#include <filesystem>
namespace kfvr::native {
void SetCameraMethods(void* constructor,void* destructor,void* renderer);
bool RenderDiagnosticCamera(void* original,void* target,void* ri,float offset);
void ProbeCameras(void* original,void* target);
void WriteCameraReport(const std::filesystem::path& folder);
}
