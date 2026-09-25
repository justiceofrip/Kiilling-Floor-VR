#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
namespace kfvr::gameplay {
struct TrackedPose { std::array<float,3> position{};std::array<float,4> rotation{0,0,0,1}; };
struct PosePacket { std::uint32_t actorEpoch=0,sequence=0;std::uint16_t flags=0;std::array<TrackedPose,3> targets{}; };
// Versioned little-endian format: header 16 + 3 * (millimetre xyz + quaternion xyzw).
inline constexpr std::size_t PoseBytes=58;
using PoseBuffer=std::array<std::byte,PoseBytes>;
bool EncodePose(const PosePacket&,PoseBuffer&);
bool DecodePose(std::span<const std::byte>,PosePacket&);
class PoseGate {
public:
    explicit PoseGate(std::uint32_t ownerEpoch):epoch_(ownerEpoch){}
    bool Accept(const PosePacket&,double serverSeconds);
private:
    std::uint32_t epoch_;PosePacket last_{};double lastTime_=0;bool initialized_=false;
};
}
