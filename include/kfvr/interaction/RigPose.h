#pragma once
#include "TrackingMath.h"
#include <cstdint>
#include <span>
#include <vector>
namespace kfvr::interaction {
struct ArmJoints {std::uint32_t shoulder=0,elbow=0,wrist=0;};
// Indices/topology are supplied by a verified game profile, never assumed from
// a 70/80-bone Refractor rig. Parents use -1 for roots.
bool ValidateArms(std::span<const std::int32_t> parents,ArmJoints left,ArmJoints right) noexcept;
struct BoneWrite {std::uint32_t index=0;math::Matrix4 transform{};};
class AnimationWriteCache {
public:
    // Validation completes before any output changes. The adapter calls Restore
    // BEFORE native animation evaluation, then Apply AFTER it completes.
    bool Apply(std::uint64_t skeletonGeneration,std::span<math::Matrix4> bones,std::span<const BoneWrite> writes);
    void Restore(std::uint64_t skeletonGeneration,std::span<math::Matrix4> bones) noexcept;
    void Reset() noexcept {generation_=0;entries_.clear();}
private:
    struct Entry {std::uint32_t index=0;math::Matrix4 native{},written{};};
    std::uint64_t generation_=0;std::vector<Entry> entries_;
};
}