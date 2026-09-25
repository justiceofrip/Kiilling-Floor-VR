#include "NativeCameraTrial.h"
#include "kfvr/engine/Ue2Camera.h"
#include "kfvr/interaction/TrackingMath.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
namespace kfvr::native {
namespace {
using Constructor=void(__thiscall*)(void*,void*,void*,void*,math::Vec3,ue2::Rotator,float);
using Destructor=void(__thiscall*)(void*);
Constructor construct=nullptr;Destructor destroy=nullptr;using Renderer=void(__thiscall*)(void*,void*);Renderer render=nullptr;
struct CameraCase {math::Vec3 origin{};ue2::Rotator rotation{};float fov=0,cameraError=0,viewError=0,inverseError=0,productError=0;std::array<math::Matrix4,6> matrices{};bool finite=true,canary=true,fields=false;};
struct Trial {std::atomic<bool> ready=false;bool attempted=false,originalUnchanged=false,prerequisites=false;std::array<CameraCase,5> cases{};} trial;
bool Read(unsigned at,void* out,unsigned bytes){SIZE_T got=0;return at>=0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),out,bytes,&got)&&got==bytes;}
unsigned U32(unsigned at){unsigned v=0;Read(at,&v,4);return v;}
template<class T>T Field(const void* at,unsigned offset){T result{};std::memcpy(&result,static_cast<const unsigned char*>(at)+offset,sizeof(result));return result;}
float Error(const math::Matrix4& a,const math::Matrix4& b){float e=0;for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)e=std::max(e,std::abs(a.values[i][j]-b.values[i][j]));return e;}
}
void SetCameraMethods(void* constructor,void* destructor,void* renderer){construct=reinterpret_cast<Constructor>(constructor);destroy=reinterpret_cast<Destructor>(destructor);render=reinterpret_cast<Renderer>(renderer);}
void ProbeCameras(void* original,void* target){
 if(trial.attempted)return;trial.attempted=true;
 std::array<unsigned char,0x1e4> before{},after{};const auto address=reinterpret_cast<unsigned>(original);
 if(!construct||!destroy||!Read(address,before.data(),unsigned(before.size()))){trial.ready=true;return;}
 const auto viewport=Field<unsigned>(before.data(),4),actor=Field<unsigned>(before.data(),0x1c4),level=Field<unsigned>(before.data(),0x1bc),model=Field<unsigned>(before.data(),0x1c0);
 // Refuse constructor paths that would lazily build shared level/model data.
 if(!viewport||!actor||!level||!model||!U32(model+0xe0)||!U32(level+0x1154)||U32(U32(viewport+0x34)+0x9c)!=level){trial.ready=true;return;}
 auto origin=Field<math::Vec3>(before.data(),0x194);auto rotation=Field<ue2::Rotator>(before.data(),0x1d4);const float fov=Field<float>(before.data(),0x1e0);
 auto camera=ue2::CameraWorld(origin,rotation);if(!camera||!std::isfinite(fov)||fov<10||fov>150){trial.ready=true;return;}trial.prerequisites=true;
 auto identity=math::Matrix4{};for(unsigned i=0;i<4;++i)identity.values[i][i]=1;
 for(unsigned index=0;index<trial.cases.size();++index){
  auto& row=trial.cases[index];row.origin=origin;row.rotation=rotation;row.fov=fov;
  // Diagnostic world-unit separation only; no claim of calibrated physical IPD.
  const float offset=index==1?-1.6f:index==2?1.6f:0;
  row.origin.x+=offset*camera->values[0][0];row.origin.y+=offset*camera->values[0][1];row.origin.z+=offset*camera->values[0][2];
  if(index>=3){row.rotation.pitch=1024;row.rotation.yaw=-8192;row.rotation.roll=512;}if(index==4)row.fov=110;
  alignas(16) std::array<unsigned char,0x240> storage;storage.fill(0xcd);auto* scene=storage.data()+16;
  construct(scene,reinterpret_cast<void*>(viewport),target,reinterpret_cast<void*>(actor),row.origin,row.rotation,row.fov);
  std::memcpy(row.matrices.data(),scene+0x14,sizeof(row.matrices));
  row.fields=Field<unsigned>(scene,4)==viewport&&Field<unsigned>(scene,8)==reinterpret_cast<unsigned>(target)&&Field<unsigned>(scene,0x1c4)==actor&&std::memcmp(scene+0x194,&row.origin,12)==0&&std::memcmp(scene+0x1d4,&row.rotation,12)==0&&Field<float>(scene,0x1e0)==row.fov;
  for(const auto& m:row.matrices)for(const auto& r:m.values)for(float v:r)row.finite&=std::isfinite(v);
  auto expected=ue2::CameraWorld(row.origin,row.rotation);auto view=expected?interaction::InverseRigid(*expected):std::nullopt;
  if(expected&&view&&row.finite){row.cameraError=Error(row.matrices[1],*expected);row.viewError=Error(row.matrices[0],*view);row.inverseError=Error(interaction::Multiply(row.matrices[2],row.matrices[3]),identity);row.productError=Error(interaction::Multiply(row.matrices[0],row.matrices[2]),row.matrices[4]);}
  else row.finite=false;
  destroy(scene);
  for(unsigned i=0;i<16;++i)row.canary&=storage[i]==0xcd;
  for(unsigned i=16+0x1e4;i<storage.size();++i)row.canary&=storage[i]==0xcd;
 }
 trial.originalUnchanged=Read(address,after.data(),unsigned(after.size()))&&before==after;trial.ready.store(true,std::memory_order_release);
}
bool RenderDiagnosticCamera(void* original,void* target,void* ri,float offset){
 if(!render||!trial.ready.load(std::memory_order_acquire)||!trial.prerequisites||!trial.originalUnchanged)return false;
 for(const auto& row:trial.cases)if(!row.canary||!row.fields||!row.finite||row.cameraError>=.02f||row.viewError>=.02f||row.inverseError>=.02f||row.productError>=.02f)return false;
 std::array<unsigned char,0x1e4> before{},after{};const auto address=reinterpret_cast<unsigned>(original);if(!Read(address,before.data(),unsigned(before.size())))return false;
 auto origin=Field<math::Vec3>(before.data(),0x194);const auto rotation=Field<ue2::Rotator>(before.data(),0x1d4);auto camera=ue2::CameraWorld(origin,rotation);if(!camera)return false;
 origin.x+=offset*camera->values[0][0];origin.y+=offset*camera->values[0][1];origin.z+=offset*camera->values[0][2];
 alignas(16) std::array<unsigned char,0x240> storage;storage.fill(0xcd);auto* scene=storage.data()+16;
 construct(scene,reinterpret_cast<void*>(Field<unsigned>(before.data(),4)),target,reinterpret_cast<void*>(Field<unsigned>(before.data(),0x1c4)),origin,rotation,Field<float>(before.data(),0x1e0));
 // The native constructor uses viewport dimensions for these differentials.
 // Correct only our private node to the dimensions of its 512x384 target.
 const auto inverse=Field<math::Matrix4>(scene,0x154);math::Vec3 dx{},dy{};
 dx={2*inverse.values[0][0]/512,2*inverse.values[0][1]/512,2*inverse.values[0][2]/512};
 dy={2*inverse.values[1][0]/384,2*inverse.values[1][1]/384,2*inverse.values[1][2]/384};
 std::memcpy(scene+0x1a0,&dx,sizeof(dx));std::memcpy(scene+0x1ac,&dy,sizeof(dy));
 render(scene,ri);destroy(scene);
 bool intact=Read(address,after.data(),unsigned(after.size()))&&before==after;
 for(unsigned i=0;i<16;++i)intact&=storage[i]==0xcd;
 for(unsigned i=16+0x1e4;i<storage.size();++i)intact&=storage[i]==0xcd;
 return intact;
}
void WriteCameraReport(const std::filesystem::path& folder){
 std::ofstream out(folder/L"native-camera-trial.json");if(!trial.ready.load(std::memory_order_acquire)){out<<"{\"ready\":false}\n";return;}
 out<<std::boolalpha<<"{\"ready\":true,\"prerequisites\":"<<trial.prerequisites<<",\"original_scene_unchanged\":"<<trial.originalUnchanged<<",\"render_replayed\":false,\"world_scale_calibrated\":false,\"cases\":[";
 bool first=true;for(const auto& row:trial.cases){if(!first)out<<',';first=false;out.precision(9);out<<"{\"fields_match\":"<<row.fields<<",\"canaries_intact\":"<<row.canary<<",\"finite\":"<<row.finite<<",\"camera_max_error\":"<<row.cameraError<<",\"view_max_error\":"<<row.viewError<<",\"projection_inverse_max_error\":"<<row.inverseError<<",\"view_projection_max_error\":"<<row.productError<<",\"origin\":["<<row.origin.x<<','<<row.origin.y<<','<<row.origin.z<<"],\"rotation\":["<<row.rotation.pitch<<','<<row.rotation.yaw<<','<<row.rotation.roll<<"],\"fov\":"<<row.fov<<",\"matrices\":[";
 bool firstValue=true;for(const auto& matrix:row.matrices)for(const auto& r:matrix.values)for(float value:r){if(!firstValue)out<<',';firstValue=false;if(std::isfinite(value))out<<value;else out<<"null";}out<<"]}";}
 out<<"]}\n";
}
}
