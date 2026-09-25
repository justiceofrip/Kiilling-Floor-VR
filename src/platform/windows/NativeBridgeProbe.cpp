#include "kfvr/ipc/FrameChannel.h"
#include "kfvr/graphics/D3D11SharedPair.h"
#include <Windows.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
using namespace kfvr;using Microsoft::WRL::ComPtr;
namespace {
void Require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void Hr(HRESULT hr,const char* message){Require(SUCCEEDED(hr),message);}
std::vector<unsigned> Pixels(ID3D11DeviceContext* context,ID3D11Texture2D* output,ID3D11Texture2D* staging,unsigned eye,unsigned width,unsigned height){
 context->CopySubresourceRegion(staging,0,0,0,0,output,eye,nullptr);D3D11_MAPPED_SUBRESOURCE map{};Hr(context->Map(staging,0,D3D11_MAP_READ,0,&map),"Map received native eye");
 std::vector<unsigned> pixels(std::size_t(width)*height);for(unsigned y=0;y<height;++y)std::memcpy(pixels.data()+std::size_t(y)*width,static_cast<const char*>(map.pData)+std::size_t(y)*map.RowPitch,width*4);context->Unmap(staging,0);return pixels;
}
void Bitmap(const std::filesystem::path& path,const std::vector<unsigned>& pixels,unsigned width,unsigned height){
 auto bgra=pixels;for(auto& p:bgra)p=(p&0xff00ff00)|((p&0xff)<<16)|((p>>16)&0xff);
 BITMAPFILEHEADER f{};BITMAPINFOHEADER i{};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(i);f.bfSize=f.bfOffBits+DWORD(bgra.size()*4);i.biSize=sizeof(i);i.biWidth=LONG(width);i.biHeight=-LONG(height);i.biPlanes=1;i.biBitCount=32;i.biCompression=BI_RGB;
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&i),sizeof(i));out.write(reinterpret_cast<const char*>(bgra.data()),bgra.size()*4);
}
}
int wmain(int argc,wchar_t** argv){try{
 Require(argc==2,"Usage: KFVRNativeBridgeProbe output-directory");const auto folder=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(folder);
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
 Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&device,&level,&context),"Create native receiving device");
 ComPtr<IDXGIDevice> dxgi;Hr(device.As(&dxgi),"DXGI device");ComPtr<IDXGIAdapter> adapter;Hr(dxgi->GetAdapter(&adapter),"Adapter");DXGI_ADAPTER_DESC ad{};Hr(adapter->GetDesc(&ad),"Adapter descriptor");
 const runtime::PresentationRequirements req{640,480,29,ad.AdapterLuid.LowPart,ad.AdapterLuid.HighPart};
 D3D11_TEXTURE2D_DESC desc{};desc.Width=req.width;desc.Height=req.height;desc.MipLevels=1;desc.ArraySize=2;desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> output,staging;Hr(device->CreateTexture2D(&desc,nullptr,&output),"Create receiving stereo targets");desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Hr(device->CreateTexture2D(&desc,nullptr,&staging),"Create receiving readback");
 ipc::FrameChannel channel;Require(channel.CreateHost(),"Create native receiving channel");std::wcout<<channel.Token()<<std::endl;
 graphics::D3D11PairConsumer consumer;graphics::TextureDescriptor opened{};unsigned pairs=0,timeouts=0,minColors=UINT_MAX,minDifferences=UINT_MAX;std::uint64_t lastFrame=0;std::vector<double> latencies;
 const auto start=GetTickCount64();LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
 for(std::uint64_t attempt=1;pairs<12&&GetTickCount64()-start<25000;++attempt){
  runtime::TrackingFrame tracking{};tracking.generation=0x100000000ULL+attempt;tracking.spaceGeneration=0x200000001ULL;tracking.predictedNs=10000000000LL+std::int64_t(attempt);tracking.focused=tracking.headValid=true;
  tracking.head.position.x=float(pairs%4)*.04f;tracking.eyes={tracking.head,tracking.head};tracking.eyes[0].position.x-=.032f;tracking.eyes[1].position.x+=.032f;tracking.fov={math::FovTangents{-1.05f,.95f,.75f,-.75f},math::FovTangents{-.95f,1.05f,.75f,-.75f}};
  graphics::TextureDescriptor wire;graphics::PairTicket ticket;LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);
  const auto result=channel.RequestPair(req,tracking,50,wire,ticket);if(result!=ipc::ChannelResult::Ok){++timeouts;Sleep(1);continue;}
  Require(runtime::PairMatchesFrame(req,tracking,wire,ticket),"Received pair must match exact tracking sample");Require(ticket.frameId>lastFrame,"Received stale native frame");lastFrame=ticket.frameId;
  if(std::memcmp(&wire,&opened,sizeof(wire))){Require(consumer.Open(device.Get(),wire)==graphics::TransferResult::Ok,"Open native shared textures");opened=wire;}
  const bool copied=consumer.Copy(ticket,{graphics::TextureSlice{output.Get(),0},graphics::TextureSlice{output.Get(),1}})==graphics::TransferResult::Ok;channel.Feedback(ticket,copied);Require(copied,"Copy native shared eye pair");QueryPerformanceCounter(&end);latencies.push_back(double(end.QuadPart-begin.QuadPart)*1000/double(frequency.QuadPart));
  const auto left=Pixels(context.Get(),output.Get(),staging.Get(),0,req.width,req.height),right=Pixels(context.Get(),output.Get(),staging.Get(),1,req.width,req.height);
  for(const auto* pixels:{&left,&right}){std::set<unsigned> colors(pixels->begin(),pixels->end());minColors=std::min(minColors,unsigned(colors.size()));}
  unsigned different=0;for(std::size_t i=0;i<left.size();++i)different+=left[i]!=right[i];minDifferences=std::min(minDifferences,different);
  Require(minColors>64&&minDifferences>1024,"Missing native geometry or differing eye views");
  if(pairs==0||pairs==11){Bitmap(folder/(pairs?L"last-left.bmp":L"first-left.bmp"),left,req.width,req.height);Bitmap(folder/(pairs?L"last-right.bmp":L"first-right.bmp"),right,req.width,req.height);}
  ++pairs;Sleep(2);
 }
 // Leave time for producer to observe the final consumed acknowledgement.
 Sleep(40);channel.Close();double sum=0,max=0;for(double v:latencies){sum+=v;max=std::max(max,v);}
 std::ofstream report(folder/L"receiver.json");report<<"{\"pairs\":"<<pairs<<",\"timeouts_or_busy\":"<<timeouts<<",\"minimum_eye_colors\":"<<minColors<<",\"minimum_differing_eye_pixels\":"<<minDifferences<<",\"width\":"<<req.width<<",\"height\":"<<req.height<<",\"request_to_copy_average_ms\":"<<(pairs?sum/pairs:0)<<",\"request_to_copy_max_ms\":"<<max<<",\"synthetic_tracking\":true,\"headset_presented\":false}\n";report.close();Require(pairs==12,"Did not receive twelve fresh native stereo pairs");std::cout<<"Received twelve live KF stereo pairs across x86 to x64\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
