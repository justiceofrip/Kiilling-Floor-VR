#include "NativeCameraTrial.h"
#include "kfvr/engine/Ue2Camera.h"
#include "kfvr/interaction/TrackingMath.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <type_traits>
#include <cstddef>
namespace kfvr::native {
namespace {
using Constructor=void(__thiscall*)(void*,void*,void*,void*,math::Vec3,ue2::Rotator,float);
using Destructor=void(__thiscall*)(void*);
Constructor construct=nullptr;Destructor destroy=nullptr;using Renderer=void(__thiscall*)(void*,void*);Renderer render=nullptr;
struct CameraCase {math::Vec3 origin{};ue2::Rotator rotation{};float fov=0,derivedError=0,cameraError=0,viewError=0,inverseError=0,productError=0;std::array<math::Matrix4,6> matrices{};bool finite=true,canary=true,fields=false;};
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
  auto derived=ue2::MakeSceneView(row.matrices[1],row.matrices[2],U32(viewport+0x94),U32(viewport+0x98));
  if(!derived)row.finite=false;else {
   std::array<float,106> nativeValues{},computed{};
   static_assert(sizeof(ue2::SceneViewData)==424&&std::is_trivially_copyable_v<ue2::SceneViewData>);
   static_assert(offsetof(ue2::SceneViewData,origin)==384&&offsetof(ue2::SceneViewData,determinant)==420);
   std::memcpy(nativeValues.data(),scene+0x14,424);std::memcpy(computed.data(),&*derived,424);
   for(unsigned i=0;i<nativeValues.size();++i){row.finite&=std::isfinite(nativeValues[i])&&std::isfinite(computed[i]);row.derivedError=std::max(row.derivedError,std::abs(nativeValues[i]-computed[i]));}
  }
  destroy(scene);
  for(unsigned i=0;i<16;++i)row.canary&=storage[i]==0xcd;
  for(unsigned i=16+0x1e4;i<storage.size();++i)row.canary&=storage[i]==0xcd;
 }
 trial.originalUnchanged=Read(address,after.data(),unsigned(after.size()))&&before==after;trial.ready.store(true,std::memory_order_release);
}
bool RenderDiagnosticCamera(void* original,void* target,void* ri,float offset,unsigned mode,ue2::SceneViewData& captured){
 if(!render||!trial.ready.load(std::memory_order_acquire)||!trial.prerequisites||!trial.originalUnchanged)return false;
 for(const auto& row:trial.cases)if(!row.canary||!row.fields||!row.finite||row.cameraError>=.02f||row.viewError>=.02f||row.inverseError>=.02f||row.productError>=.02f||row.derivedError>=.02f)return false;
 std::array<unsigned char,0x1e4> before{},after{};const auto address=reinterpret_cast<unsigned>(original);if(!Read(address,before.data(),unsigned(before.size())))return false;
 auto origin=Field<math::Vec3>(before.data(),0x194);auto rotation=Field<ue2::Rotator>(before.data(),0x1d4);auto camera=ue2::CameraWorld(origin,rotation);if(!camera)return false;
 origin.x+=offset*camera->values[0][0];origin.y+=offset*camera->values[0][1];origin.z+=offset*camera->values[0][2];
 std::optional<ue2::EyeMatrices> trackedEye;
 if(mode==3){
  // A deterministic pose exercises the actual native path without claiming
  // physical headset tracking or treating 50 UU/m as calibrated game scale.
  math::Pose head;head.position={.12f,.08f,-.10f};const float half=.175f,axisLength=std::sqrt(.18f*.18f+.94f*.94f+.29f*.29f),s=std::sin(half)/axisLength;
  head.orientation={.18f*s,.94f*s,.29f*s,std::cos(half)};auto eyes=math::ComputeEyePoses(head,.064f);if(!eyes)return false;
  const auto nativeProjection=Field<math::Matrix4>(before.data(),0x94);
  const float nearZ=-nativeProjection.values[3][2]/nativeProjection.values[2][2],farZ=-nativeProjection.values[3][2]/(nativeProjection.values[2][2]-1);
  const float t=std::tan(Field<float>(before.data(),0x1e0)*3.14159265358979323846f/360);
  trackedEye=ue2::BuildEye(Field<math::Matrix4>(before.data(),0x54),{},offset<0?eyes->left:eyes->right,{(offset<0?-1.05f:-.95f)*t,(offset<0?.95f:1.05f)*t,.78f*t,-.72f*t},50,nearZ,farZ);
  if(!trackedEye)return false;auto nativeRotation=ue2::CameraRotation(trackedEye->camera);if(!nativeRotation)return false;
  rotation=*nativeRotation;const auto& p=trackedEye->camera.values[3];origin={p[0],p[1],p[2]};
 }
 alignas(16) std::array<unsigned char,0x240> storage;storage.fill(0xcd);auto* scene=storage.data()+16;
 construct(scene,reinterpret_cast<void*>(Field<unsigned>(before.data(),4)),target,reinterpret_cast<void*>(Field<unsigned>(before.data(),0x1c4)),origin,rotation,Field<float>(before.data(),0x1e0));
 // Replace all six related matrices and their derived visibility data together.
 auto sourceCamera=Field<math::Matrix4>(scene,0x54);auto projection=Field<math::Matrix4>(scene,0x94);
 if(mode==2){
  const float t=std::tan(Field<float>(before.data(),0x1e0)*3.14159265358979323846f/360);
  const math::FovTangents fov{(offset<0?-1.05f:-.95f)*t,(offset<0?.95f:1.05f)*t,.78f*t,-.72f*t};
  const float nearZ=-projection.values[3][2]/projection.values[2][2],farZ=-projection.values[3][2]/(projection.values[2][2]-1);
  auto shifted=math::MakeLhProjectionFromFovTangents(fov,nearZ,farZ);if(!shifted){destroy(scene);return false;}projection=*shifted;
 }
 if(trackedEye){sourceCamera=trackedEye->camera;projection=trackedEye->projection;}
 auto complete=ue2::MakeSceneView(sourceCamera,projection,512,384);if(!complete){destroy(scene);return false;}
 captured=*complete;std::memcpy(scene+0x14,&*complete,sizeof(*complete));
 render(scene,ri);destroy(scene);
 bool intact=Read(address,after.data(),unsigned(after.size()))&&before==after;
 for(unsigned i=0;i<16;++i)intact&=storage[i]==0xcd;
 for(unsigned i=16+0x1e4;i<storage.size();++i)intact&=storage[i]==0xcd;
 return intact;
}
void WriteCameraReport(const std::filesystem::path& folder){
 std::ofstream out(folder/L"native-camera-trial.json");if(!trial.ready.load(std::memory_order_acquire)){out<<"{\"ready\":false}\n";return;}
 out<<std::boolalpha<<"{\"ready\":true,\"prerequisites\":"<<trial.prerequisites<<",\"original_scene_unchanged\":"<<trial.originalUnchanged<<",\"render_replayed\":false,\"world_scale_calibrated\":false,\"cases\":[";
 bool first=true;for(const auto& row:trial.cases){if(!first)out<<',';first=false;out.precision(9);out<<"{\"fields_match\":"<<row.fields<<",\"canaries_intact\":"<<row.canary<<",\"finite\":"<<row.finite<<",\"derived_max_error\":"<<row.derivedError<<",\"camera_max_error\":"<<row.cameraError<<",\"view_max_error\":"<<row.viewError<<",\"projection_inverse_max_error\":"<<row.inverseError<<",\"view_projection_max_error\":"<<row.productError<<",\"origin\":["<<row.origin.x<<','<<row.origin.y<<','<<row.origin.z<<"],\"rotation\":["<<row.rotation.pitch<<','<<row.rotation.yaw<<','<<row.rotation.roll<<"],\"fov\":"<<row.fov<<",\"matrices\":[";
 bool firstValue=true;for(const auto& matrix:row.matrices)for(const auto& r:matrix.values)for(float value:r){if(!firstValue)out<<',';firstValue=false;if(std::isfinite(value))out<<value;else out<<"null";}out<<"]}";}
 out<<"]}\n";
}
}
