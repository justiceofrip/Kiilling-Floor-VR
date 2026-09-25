#include "Test.h"
#include "kfvr/interaction/RigPose.h"
#include <array>
using namespace kfvr::interaction;
int main(){
    std::array<std::int32_t,9> parents{-1,0,1,2,3,0,5,6,7};
    CHECK(ValidateArms(parents,{1,2,4},{5,6,8}));CHECK(!ValidateArms(parents,{1,2,4},{1,6,8}));CHECK(!ValidateArms(parents,{1,2,5},{5,6,8}));
    parents[0]=4;CHECK(!ValidateArms(parents,{1,2,4},{5,6,8}));parents[0]=-1;parents[4]=99;CHECK(!ValidateArms(parents,{1,2,4},{5,6,8}));
    kfvr::math::Matrix4 identity{};for(int i=0;i<4;++i)identity.values[i][i]=1;
    std::array<kfvr::math::Matrix4,9> bones;for(auto& b:bones)b=identity;auto posed=identity;posed.values[3][0]=.4f;
    AnimationWriteCache cache;const std::array<BoneWrite,1> writes{BoneWrite{4,posed}};
    for(unsigned frame=0;frame<1000;++frame){CHECK(cache.Apply(1,bones,writes));CHECK(!cache.Apply(1,bones,writes));CHECK(Near(bones[4].values[3][0],.4f));cache.Restore(1,bones);CHECK(bones[4].values==identity.values);}
    CHECK(cache.Apply(1,bones,writes));bones[4].values[3][0]=.8f;cache.Restore(1,bones);CHECK(Near(bones[4].values[3][0],.8f)); // native writer takes precedence
    bones[4]=identity;CHECK(cache.Apply(1,bones,writes));cache.Restore(2,bones);CHECK(Near(bones[4].values[3][0],.4f)); // never restore another actor
    const std::array<BoneWrite,2> invalid{BoneWrite{1,posed},BoneWrite{99,posed}};CHECK(!cache.Apply(2,bones,invalid));CHECK(bones[1].values==identity.values);
    const std::array<BoneWrite,2> duplicate{BoneWrite{1,posed},BoneWrite{1,posed}};CHECK(!cache.Apply(2,bones,duplicate));return 0;
}