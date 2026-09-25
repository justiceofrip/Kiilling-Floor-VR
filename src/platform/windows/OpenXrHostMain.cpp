#include "kfvr/xr/OpenXrHost.h"
#include "kfvr/xr/DiagnosticScene.h"
#include "kfvr/ipc/RemoteFrameProvider.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
std::string Json(const std::string& input){std::string out="\"";const char* hex="0123456789abcdef";for(unsigned char c:input){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'"';}
}
int wmain(int argc,wchar_t** argv){
    try {
        wchar_t module[32768]{};const auto length=GetModuleFileNameW(nullptr,module,32768);if(!length||length>=32768)throw std::runtime_error("Cannot locate host executable");
        kfvr::xr::DiagnosticScene diagnostic;kfvr::ipc::RemoteFrameProvider remote;bool testScene=false,ipcMode=false;kfvr::xr::HostOptions options;options.loaderPath=std::filesystem::path(module).parent_path()/L"runtime/openxr/win64/openxr_loader.dll";
        for(int i=1;i<argc;++i){const std::wstring arg=argv[i];if(arg==L"--probe")options.probeOnly=true;else if(arg==L"--session")options.probeOnly=false;else if(arg==L"--test-scene"){options.probeOnly=false;testScene=true;options.provider=&diagnostic;}
            else if(arg==L"--ipc"){options.probeOnly=false;ipcMode=true;options.provider=&remote;}
            else if(arg==L"--frame-budget"&&i+1<argc){if(!remote.SetBudget(std::stoul(argv[++i])))throw std::runtime_error("Frame budget must be 1..50 ms");}
            else if(arg==L"--seconds"&&i+1<argc){options.seconds=std::stoul(argv[++i]);if(!options.seconds||options.seconds>300)throw std::runtime_error("Seconds must be 1..300");}
            else if(arg==L"--loader"&&i+1<argc)options.loaderPath=std::filesystem::absolute(argv[++i]);
            else throw std::runtime_error("Usage: KFVRXrHost [--probe | --session | --test-scene | --ipc] [--seconds 15] [--loader path] [--frame-budget 8]");}
        if(ipcMode){if(testScene||options.probeOnly)throw std::runtime_error("IPC mode cannot be combined with probe/test-scene");
            if(!remote.Create())throw std::runtime_error("Cannot create frame channel");std::wcerr<<L"frame_channel="<<remote.Token()<<L'\n';}
        const auto report=kfvr::xr::RunOpenXrHost(options);
        std::cout<<std::boolalpha<<"{\n  \"state\":"<<Json(report.okay?(options.probeOnly?"runtime_ready":"session_observed"):"runtime_unavailable")
            <<",\n  \"runtime\":"<<Json(report.runtimeName)<<",\n  \"system\":"<<Json(report.systemName)<<",\n  \"error\":"<<Json(report.error)
            <<",\n  \"instance_created\":"<<report.instanceCreated<<",\n  \"system_available\":"<<report.systemAvailable<<",\n  \"session_created\":"<<report.sessionCreated
            <<",\n  \"adapter_luid_low\":"<<report.requirements.adapterLow<<",\n  \"adapter_luid_high\":"<<report.requirements.adapterHigh
            <<",\n  \"minimum_feature_level\":"<<report.minimumFeatureLevel<<",\n  \"eye_width\":"<<report.requirements.width<<",\n  \"eye_height\":"<<report.requirements.height
            <<",\n  \"format\":"<<report.requirements.format<<",\n  \"waited_frames\":"<<report.waitedFrames<<",\n  \"ended_frames\":"<<report.endedFrames
            <<",\n  \"valid_tracking_frames\":"<<report.validTrackingFrames<<",\n  \"submitted_pairs\":"<<report.submittedPairs<<",\n  \"rejected_pairs\":"<<report.rejectedPairs
            <<",\n  \"suggested_controller_profiles\":"<<report.suggestedControllerProfiles<<",\n  \"valid_controller_samples\":"<<report.validControllerSamples
            <<",\n  \"last_session_state\":"<<report.lastSessionState<<",\n  \"diagnostic_scene\":"<<testScene<<",\n  \"diagnostic_error\":"<<Json(diagnostic.Error())<<",\n  \"ipc_mode\":"<<ipcMode<<",\n  \"native_kf_connected\":false,\n  \"headset_visuals_verified\":false\n}\n";
        return report.okay?0:2;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
