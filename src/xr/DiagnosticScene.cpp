#include "kfvr/xr/DiagnosticScene.h"
#include "kfvr/graphics/D3D11SharedPair.h"
#include "kfvr/interaction/TrackingMath.h"
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace kfvr::xr {
using Microsoft::WRL::ComPtr;
namespace {
void Hr(HRESULT hr,const char* message){if(FAILED(hr))throw std::runtime_error(std::string(message)+" HRESULT="+std::to_string(std::uint32_t(hr)));}
struct Vertex{float x,y,z,r,g,b,a;};
using Point=std::array<float,3>;using Color=std::array<float,3>;
void Quad(std::vector<Vertex>& v,Point a,Point b,Point c,Point d,Color color){for(const auto p:{a,b,c,a,c,d})v.push_back({p[0],p[1],p[2],color[0],color[1],color[2],1});}
std::array<unsigned,7> Glyph(char c){switch(c){
 case 'K':return {17,18,20,24,20,18,17};case 'F':return {31,16,16,30,16,16,16};
 case 'N':return {17,25,25,21,19,19,17};case 'H':return {17,17,17,31,17,17,17};
 case 'B':return {30,17,17,30,17,17,30};case 'C':return {14,17,16,16,16,17,14};case '2':return {14,17,1,2,4,8,31};
 case 'V':return {17,17,17,17,17,10,4};case 'R':return {30,17,17,30,20,18,17};case 'D':return {30,17,17,17,17,17,30};
 case 'I':return {31,4,4,4,4,4,31};case 'S':return {15,16,16,14,1,1,30};case 'P':return {30,17,17,30,16,16,16};
 case 'L':return {16,16,16,16,16,16,31};case 'A':return {14,17,17,31,17,17,17};case 'Y':return {17,17,10,4,4,4,4};
 case 'T':return {31,4,4,4,4,4,4};case 'E':return {31,16,16,30,16,16,31};default:return {};}}
void Text(std::vector<Vertex>& v,const char* text,float y,float z,float scale){
 const float start=-float(std::strlen(text))*6*scale/2;unsigned i=0;
 for(const char* c=text;*c;++c,++i){const auto glyph=Glyph(*c);for(unsigned row=0;row<7;++row)for(unsigned col=0;col<5;++col)if(glyph[row]&(1u<<(4-col))){
  const float x=start+(float(i)*6+float(col))*scale;const float top=y-float(row)*scale;
  Quad(v,{x,top,z},{x+scale*.85f,top,z},{x+scale*.85f,top-scale*.85f,z},{x,top-scale*.85f,z},{.7f,.9f,1.f});}}
}
std::vector<Vertex> Room(){
 std::vector<Vertex> v;Quad(v,{-6,0,-6},{6,0,-6},{6,0,8},{-6,0,8},{.025f,.03f,.04f});
 for(int i=-5;i<=5;++i){const float at=float(i),t=.012f;Quad(v,{at-t,.005f,-5},{at+t,.005f,-5},{at+t,.005f,7},{at-t,.005f,7},{.09f,.12f,.15f});Quad(v,{-5,.006f,at-t},{5,.006f,at-t},{5,.006f,at+t},{-5,.006f,at+t},{.09f,.12f,.15f});}
 const float x=.3f,y=1.2f,z=2.1f;
 Quad(v,{-x,y-x,z-x},{x,y-x,z-x},{x,y+x,z-x},{-x,y+x,z-x},{.06f,.32f,.75f});
 Quad(v,{x,y-x,z-x},{x,y-x,z+x},{x,y+x,z+x},{x,y+x,z-x},{.75f,.11f,.04f});
 Quad(v,{-x,y-x,z+x},{-x,y-x,z-x},{-x,y+x,z-x},{-x,y+x,z+x},{.03f,.6f,.2f});
 Quad(v,{-x,y+x,z-x},{x,y+x,z-x},{x,y+x,z+x},{-x,y+x,z+x},{.6f,.45f,.05f});
 Quad(v,{x,y-x,z+x},{-x,y-x,z+x},{-x,y+x,z+x},{x,y+x,z+x},{.3f,.06f,.6f});
 Quad(v,{-x,y-x,z+x},{x,y-x,z+x},{x,y-x,z-x},{-x,y-x,z-x},{.2f,.2f,.25f});
 Text(v,"KFVR",2.45f,3.2f,.038f);Text(v,"SYNTHETIC TEST",2.05f,3.2f,.024f);return v;
}
constexpr char Shader[]=R"(
cbuffer Camera : register(b0) { row_major float4x4 vp; }
struct In {float3 position:POSITION;float4 color:COLOR;};
struct Out {float4 position:SV_Position;float4 color:COLOR;};
Out vs(In i){Out o;o.position=mul(float4(i.position,1),vp);o.color=i.color;return o;}
float4 ps(Out i):SV_Target{return i.color;}
)";
}
struct DiagnosticScene::State {
 runtime::PresentationRequirements requirements{};ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
 ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11InputLayout> layout;
 ComPtr<ID3D11Buffer> vertices,constants;ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11DepthStencilState> depthState;
 std::array<ComPtr<ID3D11Texture2D>,2> color;std::array<ComPtr<ID3D11RenderTargetView>,2> targets;
 ComPtr<ID3D11Texture2D> depth;ComPtr<ID3D11DepthStencilView> depthView;
 graphics::D3D11PairProducer producer;UINT count=0;std::uint64_t frameId=0,epoch=0;
 explicit State(const runtime::PresentationRequirements& req):requirements(req){
  ComPtr<IDXGIFactory1> factory;Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"Diagnostic DXGI factory");ComPtr<IDXGIAdapter1> selected;
  for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> candidate;const auto hr=factory->EnumAdapters1(i,&candidate);if(hr==DXGI_ERROR_NOT_FOUND)break;Hr(hr,"Diagnostic adapter");DXGI_ADAPTER_DESC1 d{};Hr(candidate->GetDesc1(&d),"Diagnostic adapter identity");if(d.AdapterLuid.LowPart==req.adapterLow&&d.AdapterLuid.HighPart==req.adapterHigh){selected=candidate;break;}}
  if(!selected)throw std::runtime_error("Diagnostic adapter missing");const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};D3D_FEATURE_LEVEL level{};
  Hr(D3D11CreateDevice(selected.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&device,&level,&context),"Diagnostic D3D11 device");
  ComPtr<ID3DBlob> vertex,pixel,error;Hr(D3DCompile(Shader,sizeof(Shader)-1,"KFVR VR display test",nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vertex,&error),"Diagnostic vertex shader compile");
  Hr(D3DCompile(Shader,sizeof(Shader)-1,"KFVR VR display test",nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&pixel,&error),"Diagnostic pixel shader compile");
  Hr(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs),"Diagnostic vertex shader");Hr(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps),"Diagnostic pixel shader");
  const D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}};
  Hr(device->CreateInputLayout(elements,2,vertex->GetBufferPointer(),vertex->GetBufferSize(),&layout),"Diagnostic input layout");
  const auto mesh=Room();count=UINT(mesh.size());D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=UINT(mesh.size()*sizeof(Vertex));buffer.Usage=D3D11_USAGE_IMMUTABLE;buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA data{mesh.data(),0,0};Hr(device->CreateBuffer(&buffer,&data,&vertices),"Diagnostic geometry");buffer={};buffer.ByteWidth=64;buffer.Usage=D3D11_USAGE_DEFAULT;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;Hr(device->CreateBuffer(&buffer,nullptr,&constants),"Diagnostic camera constants");
  D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;Hr(device->CreateRasterizerState(&rd,&raster),"Diagnostic raster state");
  D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=TRUE;ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;ds.DepthFunc=D3D11_COMPARISON_LESS;Hr(device->CreateDepthStencilState(&ds,&depthState),"Diagnostic depth state");
  D3D11_TEXTURE2D_DESC texture{};texture.Width=req.width;texture.Height=req.height;texture.MipLevels=texture.ArraySize=texture.SampleDesc.Count=1;texture.Format=DXGI_FORMAT(req.format);texture.Usage=D3D11_USAGE_DEFAULT;texture.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  for(unsigned eye=0;eye<2;++eye){Hr(device->CreateTexture2D(&texture,nullptr,&color[eye]),"Diagnostic eye texture");Hr(device->CreateRenderTargetView(color[eye].Get(),nullptr,&targets[eye]),"Diagnostic eye target");}
  texture.Format=DXGI_FORMAT_D32_FLOAT;texture.BindFlags=D3D11_BIND_DEPTH_STENCIL;Hr(device->CreateTexture2D(&texture,nullptr,&depth),"Diagnostic depth texture");Hr(device->CreateDepthStencilView(depth.Get(),nullptr,&depthView),"Diagnostic depth view");
 }
 void Draw(const runtime::TrackingFrame& tracking){
  UINT stride=sizeof(Vertex),offset=0;auto* vb=vertices.Get();context->IASetVertexBuffers(0,1,&vb,&stride,&offset);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetInputLayout(layout.Get());
  context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);auto* cb=constants.Get();context->VSSetConstantBuffers(0,1,&cb);context->RSSetState(raster.Get());context->OMSetDepthStencilState(depthState.Get(),0);
  D3D11_VIEWPORT viewport{0,0,float(requirements.width),float(requirements.height),0,1};context->RSSetViewports(1,&viewport);
  math::Matrix4 base{};for(unsigned i=0;i<4;++i)base.values[i][i]=1;base.values[3][1]=1.6f;
  for(unsigned eye=0;eye<2;++eye){auto* target=targets[eye].Get();context->OMSetRenderTargets(1,&target,depthView.Get());const float clear[]={.012f,.018f,.027f,1};context->ClearRenderTargetView(target,clear);context->ClearDepthStencilView(depthView.Get(),D3D11_CLEAR_DEPTH,1,0);
   const auto camera=math::ComposeRuntimeHeadWithLhCamera(base,tracking.referenceHead,tracking.eyes[eye],1);
   const auto projection=math::MakeLhProjectionFromFovTangents(tracking.fov[eye],.05f,50);
   const auto view=camera?interaction::InverseRigid(*camera):std::nullopt;if(!view||!projection)throw std::runtime_error("Invalid diagnostic camera");
   const auto vp=interaction::Multiply(*view,*projection);context->UpdateSubresource(constants.Get(),0,nullptr,&vp,0,0);context->Draw(count,0);
  }
  context->OMSetRenderTargets(0,nullptr,nullptr);
 }
};
DiagnosticScene::DiagnosticScene()=default;DiagnosticScene::~DiagnosticScene()=default;
const std::string& DiagnosticScene::Error()const noexcept{return error_;}
bool DiagnosticScene::TryGetPair(const runtime::PresentationRequirements& req,const runtime::TrackingFrame& tracking,graphics::TextureDescriptor& descriptor,graphics::PairTicket& ticket)noexcept{
 try {
  if(!state_||std::memcmp(&state_->requirements,&req,sizeof(req))!=0)state_=std::make_unique<State>(req);auto& s=*state_;
  if(!graphics::Valid(s.producer.Descriptor()))if(s.producer.Create(s.device.Get(),req.width,req.height,DXGI_FORMAT(req.format),++s.epoch)!=graphics::TransferResult::Ok)throw std::runtime_error("Diagnostic shared texture creation failed");
  s.Draw(tracking);const std::array<graphics::TextureSlice,2> sources={graphics::TextureSlice{s.color[0].Get(),0},graphics::TextureSlice{s.color[1].Get(),0}};
  if(s.producer.Publish(sources,{++s.frameId,tracking.spaceGeneration,tracking.generation,tracking.predictedNs},ticket)!=graphics::TransferResult::Ok)throw std::runtime_error("Diagnostic pair publication failed");
  descriptor=s.producer.Descriptor();return true;
 }catch(const std::exception& e){error_=e.what();if(state_)state_->producer.Reset();return false;}catch(...){error_="Diagnostic scene failed";return false;}
}
void DiagnosticScene::PairConsumed(const graphics::PairTicket&,bool consumed)noexcept{if(!consumed&&state_)state_->producer.Reset();}
}
