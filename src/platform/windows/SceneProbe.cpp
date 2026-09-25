#include "kfvr/xr/DiagnosticScene.h"
#include "kfvr/graphics/D3D11SharedPair.h"
#include <Windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using Microsoft::WRL::ComPtr;
void Require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void Hr(HRESULT hr,const char* text){Require(SUCCEEDED(hr),text);}
void Bitmap(const std::filesystem::path& path,UINT width,UINT height,std::vector<std::uint32_t> pixels){
 for(auto& pixel:pixels)pixel=(pixel&0xff00ff00u)|((pixel&255)<<16)|((pixel>>16)&255);
 BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+DWORD(pixels.size()*4);
 BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=LONG(width);info.biHeight=-LONG(height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()*4));Require(bool(out),"Write scene preview");
}
int wmain(int argc,wchar_t** argv){try {
 Require(argc==2,"Usage: KFVRSceneProbe output-directory");const auto folder=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(folder);
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
 const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&device,&level,&context),"Create scene verification device");
 ComPtr<IDXGIDevice> dxgi;Hr(device.As(&dxgi),"DXGI device");ComPtr<IDXGIAdapter> adapter;Hr(dxgi->GetAdapter(&adapter),"Adapter");DXGI_ADAPTER_DESC ad{};Hr(adapter->GetDesc(&ad),"Adapter identity");
 kfvr::runtime::PresentationRequirements req{960,1024,29,ad.AdapterLuid.LowPart,ad.AdapterLuid.HighPart};
 D3D11_TEXTURE2D_DESC desc{};desc.Width=req.width;desc.Height=req.height;desc.MipLevels=1;desc.ArraySize=2;desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> output;Hr(device->CreateTexture2D(&desc,nullptr,&output),"Create scene output");
 desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ComPtr<ID3D11Texture2D> staging;Hr(device->CreateTexture2D(&desc,nullptr,&staging),"Create scene readback");
 kfvr::xr::DiagnosticScene scene;kfvr::graphics::D3D11PairConsumer consumer;
 std::vector<std::uint32_t> originalLeft,originalRight;
 for(unsigned frame=1;frame<=2;++frame){
  kfvr::runtime::TrackingFrame tracking{};tracking.generation=frame;tracking.spaceGeneration=1;tracking.predictedNs=1000000+frame;tracking.focused=tracking.headValid=true;
  tracking.eyes[0].position.x=-.032f;tracking.eyes[1].position.x=.032f;tracking.fov={kfvr::math::FovTangents{-1,1,1,-1},kfvr::math::FovTangents{-1,1,1,-1}};
  if(frame==2){tracking.head.position.x=.2f;for(auto& eye:tracking.eyes)eye.position.x+=.2f;}
  kfvr::graphics::TextureDescriptor wire;kfvr::graphics::PairTicket ticket;
  Require(scene.TryGetPair(req,tracking,wire,ticket),scene.Error().c_str());Require(kfvr::runtime::PairMatchesFrame(req,tracking,wire,ticket),"Scene frame identity");
  if(frame==1)Require(consumer.Open(device.Get(),wire)==kfvr::graphics::TransferResult::Ok,"Open scene resources");
  const std::array<kfvr::graphics::TextureSlice,2> slices={kfvr::graphics::TextureSlice{output.Get(),0},kfvr::graphics::TextureSlice{output.Get(),1}};
  const bool copied=consumer.Copy(ticket,slices)==kfvr::graphics::TransferResult::Ok;scene.PairConsumed(ticket,copied);Require(copied,"Copy scene pair into typeless XR-style targets");
  for(unsigned eye=0;eye<2;++eye){context->CopySubresourceRegion(staging.Get(),0,0,0,0,output.Get(),eye,nullptr);D3D11_MAPPED_SUBRESOURCE map{};Hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map),"Read scene pixels");
   std::vector<std::uint32_t> pixels(std::size_t(req.width)*req.height);for(UINT row=0;row<req.height;++row)std::memcpy(pixels.data()+std::size_t(row)*req.width,static_cast<const char*>(map.pData)+std::size_t(row)*map.RowPitch,req.width*4);context->Unmap(staging.Get(),0);
   unsigned different=0;for(auto pixel:pixels)different+=pixel!=pixels[0];Require(different>pixels.size()/20,"Scene geometry missing");
   if(frame==1){(eye?originalRight:originalLeft)=pixels;Bitmap(folder/(eye?L"right-eye.bmp":L"left-eye.bmp"),req.width,req.height,pixels);}else Require(pixels!=(eye?originalRight:originalLeft),"Head translation did not change scene");
  }
 }
 Require(originalLeft!=originalRight,"Stereo eyes are identical");
 std::cout<<"{\"state\":\"verified\",\"stereo_parallax\":true,\"head_translation\":true,\"srgb_to_typeless_copy\":true,\"headset_used\":false,\"game_connected\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
