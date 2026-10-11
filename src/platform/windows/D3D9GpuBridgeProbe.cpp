#include "kfvr/graphics/D3D9GpuPair.h"
#include <d3d9on12.h>
// Pixel fixture and cross-process consumer adapted from our D3D9BridgeProbe.
// The existing CPU probe and public shared-pair protocol remain unchanged.
#include <Windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace kfvr::graphics;using Microsoft::WRL::ComPtr;
namespace {
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void Hr(HRESULT hr,const char* why){if(FAILED(hr))throw std::runtime_error(std::string(why)+" HRESULT="+std::to_string(std::uint32_t(hr)));}
struct Handle {HANDLE h=nullptr;~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
struct Window {HWND h=nullptr;Window(){h=CreateWindowExW(0,L"STATIC",L"KFVR isolated GPU bridge test",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Require(h!=nullptr,"Create hidden probe window");}~Window(){if(h)DestroyWindow(h);}};
struct D11 {ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;explicit D11(LUID luid){ComPtr<IDXGIFactory1> factory;Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"Create DXGI factory");ComPtr<IDXGIAdapter1> selected;for(UINT index=0;;++index){ComPtr<IDXGIAdapter1> a;if(factory->EnumAdapters1(index,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};Hr(a->GetDesc1(&d),"Adapter description");if(d.AdapterLuid.LowPart==luid.LowPart&&d.AdapterLuid.HighPart==luid.HighPart){selected=a;break;}}Require(bool(selected),"Find matching D3D9 adapter in DXGI");const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};D3D_FEATURE_LEVEL level{};Hr(D3D11CreateDevice(selected.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&device,&level,&context),"Create transport D3D11 device");}};
struct Request {TextureDescriptor descriptor;PairTicket ticket;std::uint32_t sourceFormat=0,reserved=0;};
struct Reply {std::uint32_t magic=0x3946504b,status=1,bits=sizeof(void*)*8,reserved=0;};
static_assert(sizeof(Request)==168&&sizeof(Reply)==16);
bool Read(HANDLE pipe,void* data,DWORD bytes){auto* p=static_cast<char*>(data);while(bytes){DWORD got=0;if(!ReadFile(pipe,p,bytes,&got,nullptr)||!got)return false;p+=got;bytes-=got;}return true;}
bool Write(HANDLE pipe,const void* data,DWORD bytes){auto* p=static_cast<const char*>(data);while(bytes){DWORD n=0;if(!WriteFile(pipe,p,bytes,&n,nullptr)||!n)return false;p+=n;bytes-=n;}return true;}
std::uint32_t Color(std::uint64_t frame,unsigned eye,unsigned quadrant,bool alpha){const auto a=alpha?80+quadrant*30:255;return (a<<24)|(((30+unsigned(frame)*17+eye*89)&255)<<16)|((55+quadrant*33)<<8)|(20+eye*41+quadrant*13);}
int Consume(){Reply reply;try{Request request;Require(Read(GetStdHandle(STD_INPUT_HANDLE),&request,sizeof(request)),"Read surface probe request");Require(Valid(request.descriptor)&&Valid(request.ticket,request.descriptor)&&!request.reserved,"Validate surface packet");const auto& d=request.descriptor;D11 output({d.adapterLow,d.adapterHigh});D3D11PairConsumer consumer;Require(consumer.Open(output.device.Get(),d)==TransferResult::Ok,"Open D3D9-origin pair");
 D3D11_TEXTURE2D_DESC desc{};desc.Width=d.width;desc.Height=d.height;desc.MipLevels=1;desc.ArraySize=2;desc.Format=d.format==29?DXGI_FORMAT_R8G8B8A8_TYPELESS:d.format==91?DXGI_FORMAT_B8G8R8A8_TYPELESS:DXGI_FORMAT(d.format);desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;ComPtr<ID3D11Texture2D> eyes,staging;Hr(output.device->CreateTexture2D(&desc,nullptr,&eyes),"Create consumer eye array");desc.ArraySize=1;desc.Format=DXGI_FORMAT(d.format);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Hr(output.device->CreateTexture2D(&desc,nullptr,&staging),"Create consumer pixel readback");
 Require(consumer.Copy(request.ticket,{TextureSlice{eyes.Get(),0},TextureSlice{eyes.Get(),1}})==TransferResult::Ok,"Copy completed D3D9 pair");
 for(unsigned eye=0;eye<2;++eye){output.context->CopySubresourceRegion(staging.Get(),0,0,0,0,eyes.Get(),eye,nullptr);D3D11_MAPPED_SUBRESOURCE mapped{};Hr(output.context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"Map consumer pixels");bool valid=true;for(UINT y=0;y<d.height;++y){auto* row=reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(mapped.pData)+std::size_t(y)*mapped.RowPitch);for(UINT x=0;x<d.width;++x){const unsigned quadrant=(x>=d.width/2?1:0)+(y>=d.height/2?2:0);auto expected=Color(request.ticket.frameId,eye,quadrant,request.sourceFormat==D3DFMT_A8R8G8B8);if(d.format==28||d.format==29)expected=(expected&0xff00ff00)|((expected&255)<<16)|((expected>>16)&255);if(valid&&row[x]!=expected){std::cerr<<"pixel_mismatch frame="<<request.ticket.frameId<<" eye="<<eye<<" format="<<d.format<<" x="<<x<<" y="<<y<<" expected=0x"<<std::hex<<expected<<" actual=0x"<<row[x]<<std::dec<<std::endl;valid=false;}}}output.context->Unmap(staging.Get(),0);Require(valid,"D3D9 pair pixels/alpha/channel order differ");}
 Require(consumer.Copy(request.ticket,{TextureSlice{eyes.Get(),0},TextureSlice{eyes.Get(),1}})==TransferResult::Invalid,"Reject repeated consumer ticket");
 reply.status=0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';}Write(GetStdHandle(STD_OUTPUT_HANDLE),&reply,sizeof(reply));return int(reply.status);}
void ChildVerify(const std::filesystem::path& exe,const Request& request,unsigned bits){SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};Handle inputRead,inputWrite,outputRead,outputWrite,process,thread;Require(CreatePipe(&inputRead.h,&inputWrite.h,&sa,0)&&CreatePipe(&outputRead.h,&outputWrite.h,&sa,0),"Create probe pipes");Require(SetHandleInformation(inputWrite.h,HANDLE_FLAG_INHERIT,0)&&SetHandleInformation(outputRead.h,HANDLE_FLAG_INHERIT,0),"Restrict parent pipe inheritance");STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=inputRead.h;si.hStdOutput=outputWrite.h;si.hStdError=GetStdHandle(STD_ERROR_HANDLE);PROCESS_INFORMATION pi{};std::wstring command=L"\""+exe.wstring()+L"\" --consume";Require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi),"Launch independent consumer");process.h=pi.hProcess;thread.h=pi.hThread;CloseHandle(inputRead.h);inputRead.h=nullptr;CloseHandle(outputWrite.h);outputWrite.h=nullptr;
 Require(Write(inputWrite.h,&request,sizeof(request)),"Send complete pair to consumer");if(WaitForSingleObject(process.h,12000)!=WAIT_OBJECT_0){TerminateProcess(process.h,3);throw std::runtime_error("Isolated surface consumer timeout");}Reply reply{};Require(Read(outputRead.h,&reply,sizeof(reply))&&reply.magic==0x3946504b&&reply.status==0&&reply.bits==bits,"Cross-process pixel verification failed");}
void Fill(IDirect3DDevice9* device,IDirect3DSurface9* surface,UINT width,UINT height,std::uint64_t frame,unsigned eye,bool alpha){for(unsigned q=0;q<4;++q){RECT rect{LONG(q&1?width/2:0),LONG(q&2?height/2:0),LONG(q&1?width:width/2),LONG(q&2?height:height/2)};Hr(device->ColorFill(surface,&rect,Color(frame,eye,q,alpha)),"Fill distinct test eye quadrants");}}
struct Stats {unsigned published=0,verified=0,discarded=0,busyRetries=0;std::vector<double> times;};
void Pairs(IDirect3DDevice9* device,ID3D11Device* upload,const std::filesystem::path& consumer,UINT width,UINT height,DXGI_FORMAT format,unsigned first,unsigned count,bool exercise,Stats& stats){
 D3D9GpuPair bridge;
 Require(bridge.Create(device,upload,4096,4096,D3DFMT_A8R8G8B8,format,1)==TransferResult::Invalid,"Reject resource budget overflow");
 Require(bridge.Create(device,upload,width,height,D3DFMT_X8R8G8B8,format,1)==TransferResult::Invalid,"Reject unsupported X8 source");
 const auto created=bridge.Create(device,upload,width,height,D3DFMT_A8R8G8B8,format,first);
 if(created!=TransferResult::Ok)Hr(bridge.LastError(),"Create GPU transport");Require(created==TransferResult::Ok,"Create GPU transport result");
 std::array<ComPtr<IDirect3DSurface9>,2> surfaces;for(auto& surface:surfaces)Hr(device->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&surface,nullptr),"Create native eye surfaces");
 ComPtr<IDirect3DSurface9> original;Hr(device->GetRenderTarget(0,&original),"Save original target");D3DVIEWPORT9 viewport{};Hr(device->GetViewport(&viewport),"Save original viewport");
 TextureDescriptor discarded{};bool recreated=false;
 for(unsigned step=0;step<count;++step){const std::uint64_t frame=first+step;for(unsigned eye=0;eye<2;++eye)Fill(device,surfaces[eye].Get(),width,height,frame,eye,true);PairTicket ticket;
  const FrameMetadata metadata{frame,0x200000003ULL,0x100000002ULL,9000000000LL};
  Require(bridge.Publish({surfaces[0].Get(),surfaces[0].Get()},metadata,ticket)==TransferResult::Invalid,"Reject aliased eyes");Require(bridge.Publish({surfaces[0].Get(),nullptr},metadata,ticket)==TransferResult::Invalid,"Reject missing eye");
  const auto deadline=GetTickCount64()+3000;TransferResult result;
  do {result=bridge.Publish({surfaces[0].Get(),surfaces[1].Get()},metadata,ticket);if(result==TransferResult::Busy){++stats.busyRetries;Require(GetTickCount64()<deadline,"Allocator completion/backpressure timeout");Sleep(1);}}while(result==TransferResult::Busy);
  if(result!=TransferResult::Ok){std::cerr<<"publish_failure frame="<<frame<<" result="<<unsigned(result)<<" HRESULT=0x"<<std::hex<<std::uint32_t(bridge.LastError())<<std::dec<<std::endl;throw std::runtime_error("GPU publish failed");}
  ++stats.published;stats.times.push_back(bridge.LastTransferMs());const auto desc=bridge.Descriptor();Require(desc.format==unsigned(format)&&desc.width==width&&desc.height==height,"Typed format/dimensions preserved");
  if(recreated){Require(desc.session!=discarded.session&&desc.resourceEpoch>discarded.resourceEpoch,"Discard recreates resource identity");recreated=false;}
  PairTicket busy;Require(bridge.Publish({surfaces[0].Get(),surfaces[1].Get()},{frame+1,2,3,100},busy)==TransferResult::Busy,"Pending pair applies backpressure");auto wrong=ticket;++wrong.sequence;Require(!bridge.PairConsumed(wrong,true),"Wrong feedback rejected");
  ComPtr<IDirect3DSurface9> after;Hr(device->GetRenderTarget(0,&after),"Read bound native target");D3DVIEWPORT9 current{};Hr(device->GetViewport(&current),"Read native viewport");Require(original.Get()==after.Get()&&std::memcmp(&viewport,&current,sizeof(viewport))==0,"Native bindings preserved");
  if(exercise&&(step==5||step==13)){discarded=desc;Require(bridge.PairConsumed(ticket,false),"Discard unconsumed pair");++stats.discarded;recreated=true;continue;}
  if(exercise&&step+1==count)break; // Reset must drain a still-pending publication.
  ChildVerify(consumer,{desc,ticket,std::uint32_t(D3DFMT_A8R8G8B8),0},64);Require(bridge.PairConsumed(ticket,true),"Acknowledge pixel-verified pair");++stats.verified;
  Require(bridge.Publish({surfaces[0].Get(),surfaces[1].Get()},metadata,busy)==TransferResult::Invalid,"Repeated producer frame rejected");
 }
 Require(bridge.Reset(),"GPU bridge drains/releases during Reset");Require(!Valid(bridge.Descriptor()),"Reset clears transfer descriptor");
}
int Produce(const std::filesystem::path& consumer,UINT width,UINT height,DXGI_FORMAT format){
 Require(sizeof(void*)==4,"Producer must be x86");DWORD type=0;Require(GetBinaryTypeW(consumer.c_str(),&type)&&type==SCS_64BIT_BINARY,"Consumer must be x64");
 Window window;const auto module=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Require(module!=nullptr,"Load system D3D9");const auto create=reinterpret_cast<PFN_Direct3DCreate9On12>(GetProcAddress(module,"Direct3DCreate9On12"));Require(create!=nullptr,"Non-Ex On12 factory available");
 D3D9ON12_ARGS args{};args.Enable9On12=TRUE;ComPtr<IDirect3D9> api;api.Attach(create(D3D_SDK_VERSION,&args,1));Require(bool(api),"Create ordinary On12 enumerator");
 D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.h;pp.Windowed=TRUE;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
 ComPtr<IDirect3DDevice9> device;Hr(api->CreateDevice(0,D3DDEVTYPE_HAL,window.h,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,&device),"Create non-Ex On12 device");ComPtr<IDirect3DDevice9Ex> ex;Require(FAILED(device.As(&ex)),"Preserve original API");Require(D3D9GpuPair::Supported(device.Get()),"GPU bridge recognizes On12");
 ComPtr<IDirect3DDevice9On12> interop;Hr(device.As(&interop),"Get On12 interface");ComPtr<ID3D12Device> d12;Hr(interop->GetD3D12Device(IID_PPV_ARGS(&d12)),"Get native adapter device");D11 upload(d12->GetAdapterLuid());
 Stats stats;Pairs(device.Get(),upload.device.Get(),consumer,width,height,format,1,21,true,stats);Hr(device->Reset(&pp),"Native Reset after transfer/surface cleanup");Pairs(device.Get(),upload.device.Get(),consumer,width,height,format,22,1,false,stats);
 auto sorted=stats.times;std::sort(sorted.begin(),sorted.end());double sum=0;for(auto ms:sorted)sum+=ms;
 std::cout<<"{\"state\":\"verified\",\"producer_bits\":32,\"consumer_bits\":64,\"d3d9on12\":true,\"d3d9ex\":false,\"width\":"<<width<<",\"height\":"<<height<<",\"format\":"<<unsigned(format)<<",\"published_pairs\":"<<stats.published<<",\"pixel_verified_pairs\":"<<stats.verified<<",\"discarded_pairs\":"<<stats.discarded<<",\"pending_pair_reset\":true,\"allocator_busy_retries\":"<<stats.busyRetries<<",\"encoded_bytes_and_alpha_exact\":true,\"existing_keyed_mutex_protocol\":true,\"native_bindings_unchanged\":true,\"device_reset\":true,\"producer_cpu_image_readback\":false,\"producer_cpu_image_upload\":false,\"consumer_readback_verification_only\":true,\"timing_scope\":\"CPU_publish_call_including_GPU_queue_submission_not_GPU_duration\",\"publish_ms\":{\"min\":"<<sorted.front()<<",\"median\":"<<sorted[sorted.size()/2]<<",\"mean\":"<<sum/sorted.size()<<",\"max\":"<<sorted.back()<<"},\"raw_publish_ms\":[";
 for(std::size_t i=0;i<stats.times.size();++i){if(i)std::cout<<',';std::cout<<stats.times[i];}std::cout<<"],\"game_connected\":false,\"headset_used\":false}"<<std::endl;return 0;
}
int Supervise(const std::filesystem::path& consumer,UINT width,UINT height,DXGI_FORMAT format){
 std::wstring self(32768,L'\0');const auto size=GetModuleFileNameW(nullptr,self.data(),DWORD(self.size()));Require(size&&size<self.size(),"Resolve supervisor executable");self.resize(size);
 Handle job,process,thread;job.h=CreateJobObjectW(nullptr,nullptr);Require(job.h!=nullptr,"Create bounded GPU job");JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;Require(SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits)),"Bound producer/consumer lifetime");
 std::wstring command=L"\""+self+L"\" --produce --consumer-exe \""+consumer.wstring()+L"\" --width "+std::to_wstring(width)+L" --height "+std::to_wstring(height)+L" --format "+std::to_wstring(unsigned(format));
 STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);si.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);si.hStdError=GetStdHandle(STD_ERROR_HANDLE);PROCESS_INFORMATION pi{};
 Require(CreateProcessW(self.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&si,&pi),"Launch bounded GPU producer");process.h=pi.hProcess;thread.h=pi.hThread;
 if(!AssignProcessToJobObject(job.h,process.h)){TerminateProcess(process.h,3);throw std::runtime_error("Assign GPU process job");}Require(ResumeThread(thread.h)!=DWORD(-1),"Resume GPU producer");Require(WaitForSingleObject(process.h,90000)==WAIT_OBJECT_0,"GPU bridge proof exceeded 90 seconds");DWORD code=1;Require(GetExitCodeProcess(process.h,&code),"Read proof result");return int(code);
}
unsigned Number(const wchar_t* text){const std::wstring value(text);Require(!value.empty()&&value.size()<=4&&value.find_first_not_of(L"0123456789")==std::wstring::npos,"Numeric argument");return unsigned(std::stoul(value));}
}
int wmain(int argc,wchar_t** argv){try{
 if(argc==2&&std::wstring(argv[1])==L"--consume")return Consume();
 bool worker=false;UINT width=137,height=93;DXGI_FORMAT format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;std::filesystem::path consumer;
 for(int i=1;i<argc;++i){const std::wstring flag(argv[i]);if(flag==L"--produce"){worker=true;continue;}Require(i+1<argc,"Argument value required");if(flag==L"--consumer-exe")consumer=std::filesystem::absolute(argv[++i]);else if(flag==L"--width")width=Number(argv[++i]);else if(flag==L"--height")height=Number(argv[++i]);else if(flag==L"--format")format=DXGI_FORMAT(Number(argv[++i]));else throw std::runtime_error("Unknown argument");}
 Require(!consumer.empty()&&width>=8&&width<=4096&&height>=8&&height<=4096&&(format==28||format==29||format==87||format==91),"Usage: KFVRD3D9GpuBridgeProbe --consumer-exe x64-path [--width 2500 --height 2676 --format 28|29|87|91]");return worker?Produce(consumer,width,height,format):Supervise(consumer,width,height,format);
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
