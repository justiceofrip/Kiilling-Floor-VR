#include "Test.h"
#include "kfvr/runtime/FrameCoordinator.h"
using namespace kfvr::runtime;
struct Adapter final:IRenderAdapter {
    Capabilities caps{true,true,true,true,true};int renders=0,saved=0,restored=0,prepared=0;bool prepareOk=true;int failEye=-1;bool owns=true,saveOk=true,loseAfterLeft=false,alias=false,badEpoch=false;
    Capabilities GetCapabilities()const noexcept override{return caps;}
    bool StillOwns(const WorldFrame&)const noexcept override{return owns;}
    bool SaveState(const WorldFrame&)noexcept override{++saved;return saveOk;}
    bool PrepareViews(const WorldFrame&,const StereoViewSet& views)noexcept override{++prepared;return prepareOk&&views.culling&&views.culling->fov.right>1.f;}
    bool RenderEye(const WorldFrame& w,unsigned eye,const EyeView&,EyeImage& image)noexcept override{
        ++renders;if(int(eye)==failEye)return false;image={alias?10:10+std::uint64_t(eye),w.frameId,badEpoch?w.deviceEpoch+1:w.deviceEpoch,0,1200,1300,28};if(loseAfterLeft&&eye==0)owns=false;return true;}
    void RestoreState()noexcept override{++restored;}
};
struct Sink final:IStereoSink {Adapter& adapter;int submitted=0;bool okay=true,restoredFirst=false;explicit Sink(Adapter& a):adapter(a){}
    bool Submit(const WorldFrame&,const TrackingFrame&,const std::array<EyeImage,2>&)noexcept override{++submitted;restoredFirst=adapter.restored>0;return okay;}};
int main(){
    WorldFrame w{};w.frameId=1;w.owner=2;w.deviceEpoch=3;w.spaceGeneration=4;w.predictedNs=1000000000;w.scene=Scene::Playing;w.nearPlane=.04f;w.farPlane=300;w.worldUnitsPerMeter=1;for(int i=0;i<4;++i)w.camera.values[i][i]=1;
    TrackingFrame t{};t.generation=1;t.spaceGeneration=4;t.predictedNs=w.predictedNs;t.focused=t.headValid=true;t.eyes[0].position.x=-.032f;t.eyes[1].position.x=.032f;t.fov={kfvr::math::FovTangents{-1,1,1,-1},kfvr::math::FovTangents{-1,1,1,-1}};
    {Adapter a;Sink s(a);FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::Submitted);CHECK(a.prepared==1&&a.renders==2&&a.restored==1&&s.submitted==1&&s.restoredFirst);CHECK(c.Render(a,s,w,t)==FrameResult::Duplicate);CHECK(a.renders==2);}
    for(int eye=0;eye<2;++eye){Adapter a;a.failEye=eye;Sink s(a);FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::EyeFailed);CHECK(a.restored==1&&s.submitted==0);CHECK(c.Render(a,s,w,t)==FrameResult::Duplicate);}
    {Adapter a;a.loseAfterLeft=true;Sink s(a);FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::OwnerChanged);CHECK(a.restored==1&&a.renders==1&&!s.submitted);}
    {Adapter a;a.saveOk=false;Sink s(a);FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::CaptureFailed);CHECK(!a.renders&&!a.restored);}
    {Adapter a;a.caps.renderOnly=false;Sink s(a);FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::Unsupported);CHECK(!a.saved);}
    {Adapter a;Sink s(a);FrameCoordinator c;auto bad=t;bad.predictedNs-=150000001;CHECK(c.Render(a,s,w,bad)==FrameResult::InvalidTracking);bad=t;bad.spaceGeneration++;CHECK(c.Render(a,s,w,bad)==FrameResult::InvalidTracking);bad=t;bad.focused=false;CHECK(c.Render(a,s,w,bad)==FrameResult::InvalidTracking);CHECK(!a.saved);}
    {Adapter a;Sink s(a);FrameCoordinator c;auto bad=w;bad.scene=Scene::Loading;CHECK(c.Render(a,s,bad,t)==FrameResult::Inactive);bad=w;bad.farPlane=.01f;CHECK(c.Render(a,s,bad,t)==FrameResult::InvalidCamera);bad=w;bad.worldUnitsPerMeter=NAN;CHECK(c.Render(a,s,bad,t)==FrameResult::InvalidCamera);CHECK(!a.saved);}
    {Adapter a;a.alias=true;Sink s(a);FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::InvalidImages);CHECK(a.restored==1&&!s.submitted);}
    {Adapter a;a.badEpoch=true;Sink s(a);FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::InvalidImages);}
    {Adapter a;Sink s(a);s.okay=false;FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::SubmissionFailed);CHECK(a.restored==1);}
    {Adapter a;a.prepareOk=false;Sink s(a);FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::VisibilityFailed);CHECK(a.prepared==1&&!a.renders&&a.restored==1&&!s.submitted);CHECK(c.Render(a,s,w,t)==FrameResult::Duplicate);}
    {Adapter a;a.caps.stereoVisibility=false;Sink s(a);FrameCoordinator c;CHECK(c.Render(a,s,w,t)==FrameResult::Unsupported);CHECK(!a.saved&&!a.prepared);}
    return 0;
}