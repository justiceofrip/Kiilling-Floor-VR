#pragma once
#include <cstdint>
namespace kfvr::interaction {
class SnapTurn {
public: float Update(bool enabled,float axis,std::int64_t time,float degrees) noexcept;
private: bool armed=false;std::int64_t previous=0;
};
class NativeTurnPulse {
public:
 bool Queue(std::uint64_t owner,float degrees,std::int64_t sample,std::uint64_t now) noexcept;
 float Consume(std::uint64_t owner,bool focused,std::uint64_t now) noexcept;
 void Cancel() noexcept {degrees=0;}
private:
 std::uint64_t owner=0,queuedAt=0;std::int64_t lastSample=0;float degrees=0;
};
// LOCAL-space height is a session reference, separate from saved preferences.
// Explicit recenter defines the current posture as upright, even at y == 0.
class StandingHeightReference {
public:
 void Ensure(float current,float configured) noexcept;
 bool Recenter(float current) noexcept;
 float Drop(float current) const noexcept {return ready?height-current:0.f;}
 void Reset() noexcept {*this={};}
private: bool ready=false;float height=0;
};
// The core requests a posture; the game adapter owns held/toggle semantics.
// KFVR does not inherit Refractor's prone key or stance numbering.
enum class Posture { Standing, Crouched, Prone };
class PhysicalStance {
public:
 Posture Update(bool active,float drop,bool supportsCrouch,bool supportsProne,std::int64_t time) noexcept;
 void Reset() noexcept {*this={};}
private: Posture target=Posture::Standing,candidate=Posture::Standing;std::int64_t since=0,last=0;
};
}