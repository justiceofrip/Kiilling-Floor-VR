#include "kfvr/math/StereoCulling.h"
#include "kfvr/interaction/TrackingMath.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace kfvr::math {
std::optional<StereoCullEnvelope> EncloseStereoFrusta(const Matrix4& center,
    const std::array<Matrix4,2>& eyes,const std::array<FovTangents,2>& fov,float nearPlane,float farPlane)noexcept {
    const auto inverse=interaction::InverseAnimatedTransform(center);if(!inverse)return {};
    float closest=std::numeric_limits<float>::max(),farthest=0,horizontal=0,vertical=0;
    for(unsigned eye=0;eye<2;++eye){
        if(!interaction::InverseRigid(eyes[eye])||!MakeLhProjectionFromFovTangents(fov[eye],nearPlane,farPlane))return {};
        const auto relative=interaction::Multiply(eyes[eye],*inverse);
        // Every frustum is convex. With positive depth, the extrema of x/z and
        // y/z occur at vertices, so all six enclosing planes cover its interior.
        for(float depth:{nearPlane,farPlane})for(float x:{fov[eye].left,fov[eye].right})for(float y:{fov[eye].down,fov[eye].up}){
            const auto point=TransformRowVector({x*depth,y*depth,depth,1},relative);
            if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)||point.z<=0)return {};
            closest=(std::min)(closest,point.z);farthest=(std::max)(farthest,point.z);
            horizontal=(std::max)(horizontal,std::abs(point.x/point.z));vertical=(std::max)(vertical,std::abs(point.y/point.z));
        }
    }
    // Outward margin absorbs float matrix composition/plane normalization error.
    horizontal=horizontal*1.0001f+.0001f;vertical=vertical*1.0001f+.0001f;
    StereoCullEnvelope out{center,{-horizontal,horizontal,vertical,-vertical},closest*.9999f,farthest*1.0001f};
    if(!MakeLhProjectionFromFovTangents(out.fov,out.nearPlane,out.farPlane))return {};return out;
}
}
