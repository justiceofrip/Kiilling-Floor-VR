#include <iostream>
#include "kfvr/xr/OpenXrHost.h"
#include "kfvr/graphics/D3D11SharedPair.h"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>
#include "Actions.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace kfvr::xr {
using Microsoft::WRL::ComPtr;
namespace {
#define XR_FUNCTIONS(F) \
 F(DestroyInstance) F(GetInstanceProperties) F(GetSystem) F(GetSystemProperties) \
 F(GetD3D11GraphicsRequirementsKHR) F(EnumerateViewConfigurationViews) F(EnumerateEnvironmentBlendModes) \
 F(CreateSession) F(DestroySession) F(PollEvent) F(BeginSession) F(EndSession) F(RequestExitSession) \
 F(CreateReferenceSpace) F(DestroySpace) F(LocateSpace) F(LocateViews) \
 F(EnumerateSwapchainFormats) F(CreateSwapchain) F(DestroySwapchain) F(EnumerateSwapchainImages) \
 F(AcquireSwapchainImage) F(WaitSwapchainImage) F(ReleaseSwapchainImage) F(WaitFrame) F(BeginFrame) F(EndFrame)
struct Api {
    PFN_xrGetInstanceProcAddr get=nullptr;
#define FIELD(name) PFN_xr##name name=nullptr;
    XR_FUNCTIONS(FIELD)
#undef FIELD
    template<class T>void Load(XrInstance instance,const char* name,T& function){
        PFN_xrVoidFunction raw=nullptr;const auto result=get(instance,name,&raw);
        if(XR_FAILED(result)||!raw)throw std::runtime_error(std::string("Cannot resolve ")+name);
        function=reinterpret_cast<T>(raw);
    }
    void LoadInstance(XrInstance instance){
#define LOAD(name) Load(instance,"xr" #name,name);
        XR_FUNCTIONS(LOAD)
#undef LOAD
    }
};
void Check(XrResult result,const char* operation){if(XR_FAILED(result))throw std::runtime_error(std::string(operation)+" XrResult="+std::to_string(result));}
void Hr(HRESULT result,const char* operation){if(FAILED(result))throw std::runtime_error(std::string(operation)+" HRESULT="+std::to_string(std::uint32_t(result)));}
void Require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
math::Pose Pose(const XrPosef& pose){return {{pose.position.x,pose.position.y,pose.position.z},{pose.orientation.x,pose.orientation.y,pose.orientation.z,pose.orientation.w}};}
struct Host {
    Api api;
    Actions actions;IInputConsumer* inputConsumer=nullptr;
    HMODULE loader=nullptr;
    XrInstance instance=XR_NULL_HANDLE;
    XrSystemId system=XR_NULL_SYSTEM_ID;
    XrSession session=XR_NULL_HANDLE;
    XrSpace local=XR_NULL_HANDLE,headSpace=XR_NULL_HANDLE;
    XrSessionState sessionState=XR_SESSION_STATE_UNKNOWN;
    std::array<XrSwapchain,2> swapchains{};
    std::array<std::vector<XrSwapchainImageD3D11KHR>,2> images;
    std::array<bool,2> acquired{},waited{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    graphics::D3D11PairConsumer consumer;
    graphics::TextureDescriptor openedDescriptor{};
    runtime::RetainedProjection retained;
    std::array<XrCompositionLayerProjectionView,2> retainedViews{};
    std::uint64_t emptyRun=0;
    HostReport& report;
    bool running=false,exit=false,frameOpen=false,referenceValid=false;
    XrTime frameTime=0;
    std::vector<XrTime> spaceChanges;
    math::Pose referenceHead{};
    std::uint64_t trackingGeneration=0,spaceGeneration=1;
    unsigned poseLogSamples=0;
    explicit Host(HostReport& r):report(r){}
    ~Host(){
        AbortFrame();consumer.Reset();
        if(inputConsumer)inputConsumer->Input({});actions.Reset();
        for(auto chain:swapchains)if(chain&&api.DestroySwapchain)api.DestroySwapchain(chain);
        if(headSpace&&api.DestroySpace)api.DestroySpace(headSpace);
        if(local&&api.DestroySpace)api.DestroySpace(local);
        if(session&&api.DestroySession)api.DestroySession(session);
        context.Reset();device.Reset();
        if(instance&&api.DestroyInstance)api.DestroyInstance(instance);
        if(loader)FreeLibrary(loader);
    }
    void AbortFrame()noexcept {
        for(unsigned eye=0;eye<2;++eye)if(waited[eye]&&api.ReleaseSwapchainImage){
            XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            api.ReleaseSwapchainImage(swapchains[eye],&release);waited[eye]=acquired[eye]=false;
        }
        if(frameOpen&&api.EndFrame){
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frameTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            api.EndFrame(session,&end);frameOpen=false;
        }
    }
    void Initialize(const HostOptions& options){
        inputConsumer=options.inputConsumer;
        Require(options.loaderPath.is_absolute(),"OpenXR loader requires an absolute path");
        loader=LoadLibraryExW(options.loaderPath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        Require(loader!=nullptr,"Cannot load the local x64 OpenXR loader");
        api.get=reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(loader,"xrGetInstanceProcAddr"));
        Require(api.get!=nullptr,"OpenXR loader has no xrGetInstanceProcAddr");
        PFN_xrEnumerateInstanceExtensionProperties enumerate=nullptr;api.Load(XR_NULL_HANDLE,"xrEnumerateInstanceExtensionProperties",enumerate);
        uint32_t count=0;Check(enumerate(nullptr,0,&count,nullptr),"Enumerate extension count");Require(count<=4096,"Invalid OpenXR extension count");
        std::vector<XrExtensionProperties> extensions(count,{XR_TYPE_EXTENSION_PROPERTIES});
        Check(enumerate(nullptr,count,&count,extensions.data()),"Enumerate extensions");
        bool d3d11=false;for(const auto& extension:extensions)d3d11|=std::strcmp(extension.extensionName,XR_KHR_D3D11_ENABLE_EXTENSION_NAME)==0;
        Require(d3d11,"Active runtime does not expose XR_KHR_D3D11_enable");
        PFN_xrCreateInstance create=nullptr;api.Load(XR_NULL_HANDLE,"xrCreateInstance",create);
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(info.applicationInfo.applicationName,"KFVR KFVR Host");strcpy_s(info.applicationInfo.engineName,"KFVR Modular");
        info.applicationInfo.applicationVersion=1;info.applicationInfo.engineVersion=1;
        // 2142 compatibility lesson: core 1.0 + advertised extensions, not an
        // unnecessary 1.1 requirement merely because the headers are newer.
        info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
        const char* enabled[]={XR_KHR_D3D11_ENABLE_EXTENSION_NAME};info.enabledExtensionCount=1;info.enabledExtensionNames=enabled;
        Check(create(&info,&instance),"Create OpenXR instance");report.instanceCreated=true;api.LoadInstance(instance);
        XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};Check(api.GetInstanceProperties(instance,&props),"Runtime properties");report.runtimeName=props.runtimeName;
        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};systemInfo.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        Check(api.GetSystem(instance,&systemInfo,&system),"Find HMD system");report.systemAvailable=true;
        XrSystemProperties systemProps{XR_TYPE_SYSTEM_PROPERTIES};Check(api.GetSystemProperties(instance,system,&systemProps),"System properties");report.systemName=systemProps.systemName;
        XrGraphicsRequirementsD3D11KHR graphics{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        Check(api.GetD3D11GraphicsRequirementsKHR(instance,system,&graphics),"D3D11 graphics requirements");
        report.requirements.adapterLow=graphics.adapterLuid.LowPart;report.requirements.adapterHigh=graphics.adapterLuid.HighPart;report.minimumFeatureLevel=graphics.minFeatureLevel;
        std::array<XrViewConfigurationView,2> viewInfo{{{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
        Check(api.EnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,viewInfo.data()),"Stereo view configuration");
        Require(count==2,"Runtime must expose two primary stereo views");
        auto& req=report.requirements;req.width=(std::max)(viewInfo[0].recommendedImageRectWidth,viewInfo[1].recommendedImageRectWidth);
        req.height=(std::max)(viewInfo[0].recommendedImageRectHeight,viewInfo[1].recommendedImageRectHeight);
        for(const auto& view:viewInfo)Require(req.width&&req.height&&req.width<=view.maxImageRectWidth&&req.height<=view.maxImageRectHeight,"Incompatible per-eye image sizes");
        Require(options.renderScalePercent>=25&&options.renderScalePercent<=100,"Eye render scale must be 25..100 percent");
        if(options.renderScalePercent!=100){req.width=(std::max)(64u,(req.width*options.renderScalePercent/100)&~3u);req.height=(std::max)(64u,(req.height*options.renderScalePercent/100)&~3u);}
        if(options.probeOnly)return;
        Require(req.width<=16384&&req.height<=16384,"Runtime eye dimensions exceed the D3D11 bridge limits");
        std::array<XrEnvironmentBlendMode,16> modes{};
        Check(api.EnumerateEnvironmentBlendModes(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,UINT(modes.size()),&count,modes.data()),"Environment blend modes");
        Require(count<=modes.size()&&std::find(modes.begin(),modes.begin()+count,XR_ENVIRONMENT_BLEND_MODE_OPAQUE)!=modes.begin()+count,"Opaque HMD presentation required");
        ComPtr<IDXGIFactory1> factory;Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");ComPtr<IDXGIAdapter1> selected;
        for(UINT index=0;;++index){ComPtr<IDXGIAdapter1> candidate;const auto result=factory->EnumAdapters1(index,&candidate);
            if(result==DXGI_ERROR_NOT_FOUND)break;Hr(result,"Enumerate graphics adapter");DXGI_ADAPTER_DESC1 desc{};Hr(candidate->GetDesc1(&desc),"Graphics adapter properties");
            if(desc.AdapterLuid.LowPart==graphics.adapterLuid.LowPart&&desc.AdapterLuid.HighPart==graphics.adapterLuid.HighPart){selected=candidate;break;}}
        Require(bool(selected),"OpenXR-required graphics adapter not found");
        std::vector<D3D_FEATURE_LEVEL> levels;for(auto level:{D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0})if(level>=graphics.minFeatureLevel)levels.push_back(level);
        Require(!levels.empty(),"Runtime requires an unsupported D3D11 feature level");D3D_FEATURE_LEVEL level{};
        Hr(D3D11CreateDevice(selected.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels.data(),UINT(levels.size()),D3D11_SDK_VERSION,&device,&level,&context),"Create XR graphics device");
        XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};binding.device=device.Get();
        XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};sessionInfo.next=&binding;sessionInfo.systemId=system;
        Check(api.CreateSession(instance,&sessionInfo,&session),"Create OpenXR session");report.sessionCreated=true;
        actions.Start(api.get,instance,session);report.suggestedControllerProfiles=actions.suggestedProfiles;
        XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};spaceInfo.poseInReferenceSpace.orientation.w=1;spaceInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
        Check(api.CreateReferenceSpace(session,&spaceInfo,&local),"Create LOCAL reference space");spaceInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
        Check(api.CreateReferenceSpace(session,&spaceInfo,&headSpace),"Create VIEW reference space");
        Check(api.EnumerateSwapchainFormats(session,0,&count,nullptr),"Swapchain format count");Require(count<=4096,"Invalid swapchain format count");
        std::vector<int64_t> formats(count);Check(api.EnumerateSwapchainFormats(session,count,&count,formats.data()),"Swapchain formats");
        Require(count<=formats.size(),"Invalid returned swapchain format count");
        formats.resize(count);report.advertisedSwapchainFormats=formats;report.swapchainFormatsEnumerated=true;
        req.format=graphics::SelectSwapchainFormat(formats);
        // Emit once before swapchain creation so unsupported-format failures
        // retain the actual runtime capability list as well as the final report.
        std::cerr<<"swapchain_formats={\"advertised\":[";
        for(std::size_t i=0;i<formats.size();++i){if(i)std::cerr<<',';std::cerr<<formats[i];}
        std::cerr<<"],\"selected\":"<<req.format<<'}'<<std::endl;
        Require(req.format!=0,"Runtime has no supported RGBA8/BGRA8 swapchain format");
        for(unsigned eye=0;eye<2;++eye){
            XrSwapchainCreateInfo chain{XR_TYPE_SWAPCHAIN_CREATE_INFO};chain.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            chain.format=req.format;chain.sampleCount=1;chain.width=req.width;chain.height=req.height;chain.faceCount=chain.arraySize=chain.mipCount=1;
            Check(api.CreateSwapchain(session,&chain,&swapchains[eye]),"Create eye swapchain");
            Check(api.EnumerateSwapchainImages(swapchains[eye],0,&count,nullptr),"Eye image count");Require(count&&count<=64,"Invalid eye image count");
            images[eye].resize(count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
            Check(api.EnumerateSwapchainImages(swapchains[eye],count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images[eye].data())),"Eye images");
        }
    }
    void Events(){
        while(true){XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};const auto result=api.PollEvent(instance,&event);
            if(result==XR_EVENT_UNAVAILABLE)break;Check(result,"Poll OpenXR event");
            if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING){exit=true;return;}
            if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){
                const auto& change=reinterpret_cast<const XrEventDataReferenceSpaceChangePending&>(event);
                if(change.session==session&&change.referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL)spaceChanges.push_back(change.changeTime);
            }
            if(event.type!=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)continue;
            const auto& change=reinterpret_cast<const XrEventDataSessionStateChanged&>(event);if(change.session!=session)continue;
            sessionState=change.state;report.lastSessionState=std::int32_t(sessionState);std::cerr<<"xr_session_state="<<int(sessionState)<<std::endl;
            if(sessionState==XR_SESSION_STATE_READY&&!running){XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                Check(api.BeginSession(session,&begin),"Begin OpenXR session");running=true;referenceValid=false;++spaceGeneration;}
            if(sessionState==XR_SESSION_STATE_STOPPING&&running){Check(api.EndSession(session),"End OpenXR session");running=false;}
            if(sessionState==XR_SESSION_STATE_EXITING||sessionState==XR_SESSION_STATE_LOSS_PENDING)exit=true;
        }
    }
    void Frame(IFrameProvider* provider){
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState frame{XR_TYPE_FRAME_STATE};
        Check(api.WaitFrame(session,&wait,&frame),"Wait frame");++report.waitedFrames;frameTime=frame.predictedDisplayTime;
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};Check(api.BeginFrame(session,&begin),"Begin frame");frameOpen=true;
        for(auto at=spaceChanges.begin();at!=spaceChanges.end();)if(*at<=frameTime){++spaceGeneration;referenceValid=false;at=spaceChanges.erase(at);}else ++at;
        if(sessionState==XR_SESSION_STATE_FOCUSED)++report.focusedFrames;
        const auto controls=actions.Sample(local,frameTime,spaceGeneration,sessionState==XR_SESSION_STATE_FOCUSED);
        for(const auto& hand:controls.hands)if(hand.gripValid)++report.validControllerSamples;
        actions.Feedback(controls,inputConsumer?inputConsumer->Input(controls):std::array<HapticPulse,2>{});
        std::array<XrCompositionLayerProjectionView,2> projectionViews{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=local;layer.viewCount=2;layer.views=projectionViews.data();
        bool submit=false,fresh=false,trackingEligible=false;
        if(frame.shouldRender){
            std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};XrViewState viewState{XR_TYPE_VIEW_STATE};
            XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=frameTime;locate.space=local;uint32_t count=0;
            Check(api.LocateViews(session,&locate,&viewState,2,&count,views.data()),"Locate stereo views");
            XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};Check(api.LocateSpace(headSpace,local,frameTime,&head),"Locate head");
            constexpr auto validViews=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;
            constexpr auto validHead=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if(count==2&&(viewState.viewStateFlags&validViews)==validViews&&(head.locationFlags&validHead)==validHead){
                runtime::TrackingFrame tracking{};tracking.generation=++trackingGeneration;tracking.spaceGeneration=spaceGeneration;tracking.predictedNs=frameTime;
                tracking.focused=sessionState==XR_SESSION_STATE_FOCUSED;tracking.headValid=true;tracking.head=Pose(head.pose);
                if(!referenceValid&&tracking.focused){
                    const auto levelReference=math::MakeYawReferencePose(tracking.head);
                    if(levelReference){referenceHead=*levelReference;referenceValid=true;}
                }
                tracking.referenceHead=referenceHead;
                bool valid=referenceValid&&bool(math::MakeRelativePose(tracking.referenceHead,tracking.head));
                for(unsigned eye=0;eye<2;++eye){tracking.eyes[eye]=Pose(views[eye].pose);const auto& fov=views[eye].fov;
                    tracking.fov[eye]={std::tan(fov.angleLeft),std::tan(fov.angleRight),std::tan(fov.angleUp),std::tan(fov.angleDown)};
                    valid&=bool(math::MakeRelativePose(tracking.referenceHead,tracking.eyes[eye]))&&bool(math::MakeLhProjectionFromFovTangents(tracking.fov[eye],.05f,100.f));}
                tracking.headValid=valid;trackingEligible=valid&&tracking.focused;if(valid)++report.validTrackingFrames;
                // Bounded incremental evidence survives a game exit before the
                // final report. Same predicted time/reference for head and hands.
                if(trackingEligible&&poseLogSamples<60&&(poseLogSamples==0||tracking.generation%30==0)){
                    auto pose=[](const math::Pose& p){std::cerr<<'['<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','<<p.orientation.x<<','<<p.orientation.y<<','<<p.orientation.z<<','<<p.orientation.w<<']';};
                    std::cerr<<"tracking_pose={\"generation\":"<<tracking.generation<<",\"space\":"<<tracking.spaceGeneration<<",\"reference\":";pose(tracking.referenceHead);
                    std::cerr<<",\"head\":";pose(tracking.head);
                    for(unsigned hand=0;hand<2;++hand){
                        const auto& input=controls.hands[hand];
                        std::cerr<<(hand?",\"right_valid\":":",\"left_valid\":")<<(input.gripValid?"true":"false");
                        std::cerr<<(hand?",\"right_grip\":":",\"left_grip\":");pose(input.gripPose);
                        std::cerr<<(hand?",\"right_aim_valid\":":",\"left_aim_valid\":")<<(input.aimValid?"true":"false");
                        std::cerr<<(hand?",\"right_aim\":":",\"left_aim\":");pose(input.aimPose);
                    }
                    std::cerr<<'}'<<std::endl;++poseLogSamples;
                }
                graphics::TextureDescriptor descriptor{};graphics::PairTicket ticket{};
                if(valid&&tracking.focused&&provider&&provider->TryGetPair(report.requirements,tracking,descriptor,ticket)){
                    struct Feedback {IFrameProvider& provider;const graphics::PairTicket& ticket;bool consumed=false;~Feedback(){provider.PairConsumed(ticket,consumed);}} feedback{*provider,ticket};
                    if(runtime::PairMatchesFrame(report.requirements,tracking,descriptor,ticket)){
                        if(!graphics::Valid(openedDescriptor)||std::memcmp(&descriptor,&openedDescriptor,sizeof(descriptor))!=0){
                            if(consumer.Open(device.Get(),descriptor)==graphics::TransferResult::Ok)openedDescriptor=descriptor;else openedDescriptor={};
                        }
                        if(graphics::Valid(openedDescriptor)){
                            // Any attempted swapchain update invalidates the previous pair.
                            // A failed partial copy must never be reused as complete stereo.
                            retained.Clear();
                            std::array<graphics::TextureSlice,2> destinations{};
                            for(unsigned eye=0;eye<2;++eye){uint32_t index=0;XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                                Check(api.AcquireSwapchainImage(swapchains[eye],&acquire,&index),"Acquire eye image");acquired[eye]=true;
                                XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};imageWait.timeout=50000000;
                                const auto ready=api.WaitSwapchainImage(swapchains[eye],&imageWait);Check(ready,"Wait eye image");Require(ready!=XR_TIMEOUT_EXPIRED,"OpenXR eye image wait timed out");waited[eye]=true;
                                Require(index<images[eye].size(),"Invalid acquired eye index");destinations[eye]={images[eye][index].texture,0};
                                auto& projection=projectionViews[eye];projection.pose=views[eye].pose;projection.fov=views[eye].fov;
                                projection.subImage.swapchain=swapchains[eye];projection.subImage.imageRect.extent={std::int32_t(report.requirements.width),std::int32_t(report.requirements.height)};
                            }
                            submit=consumer.Copy(ticket,destinations)==graphics::TransferResult::Ok;feedback.consumed=submit;
                            for(unsigned eye=0;eye<2;++eye){XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};Check(api.ReleaseSwapchainImage(swapchains[eye],&release),"Release eye image");waited[eye]=acquired[eye]=false;}
                        }
                    }
                    if(!submit)++report.rejectedPairs;
                    if(submit){fresh=true;retained.Remember(report.requirements,tracking,descriptor,ticket);retainedViews=projectionViews;}
                }
                if(!submit&&retained.CanPresent(report.requirements,tracking)){
                    // xrEndFrame uses each swapchain's most recently released image.
                    // Keep its actual render poses/FOVs, never substitute new poses.
                    projectionViews=retainedViews;submit=true;
                }
            }
        }
        if(!trackingEligible)retained.Clear();
        const auto* base=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frameTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        if(submit){end.layerCount=1;end.layers=&base;}
        const auto result=api.EndFrame(session,&end);frameOpen=false;Check(result,"End frame");++report.endedFrames;
        if(fresh){if(report.submittedPairs)report.maxEmptyBetweenFresh=(std::max)(report.maxEmptyBetweenFresh,emptyRun);++report.submittedPairs;}
        else if(submit)++report.reprojectedFrames;
        else ++report.emptyFrames;
        if(submit)emptyRun=0;else ++emptyRun;
    }
    void Run(const HostOptions& options){
        const auto start=std::chrono::steady_clock::now();bool requested=false,firstPair=false;auto shutdown=start,visibleStart=start;
        while(!exit){Events();if(exit)break;const auto now=std::chrono::steady_clock::now();
            if(!firstPair&&report.submittedPairs){firstPair=true;visibleStart=now;}
            const bool elapsed=options.waitForFirstPair?
                (firstPair?now-visibleStart>=std::chrono::seconds(options.seconds):now-start>=std::chrono::seconds(45)):
                now-start>=std::chrono::seconds(options.seconds);
            if(!requested&&elapsed){
                if(!running)break;Check(api.RequestExitSession(session),"Request session exit");requested=true;shutdown=now;
            }
            if(requested&&(!running||now-shutdown>=std::chrono::seconds(2)))break;
            if(running)Frame(options.provider);else Sleep(10);
        }
    }
};
}
HostReport RunOpenXrHost(const HostOptions& options)noexcept {
    HostReport report;
    try {Host host(report);host.Initialize(options);if(!options.probeOnly)host.Run(options);report.okay=true;}
    catch(const std::exception& error){report.error=error.what();}
    catch(...){report.error="Unknown OpenXR host failure";}
    return report;
}
}
