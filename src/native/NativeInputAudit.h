#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>
#include <filesystem>
namespace kfvr::native {
struct MovementDemand {bool valid=false,turnValid=false;float forward=0,strafe=0,turnDegrees=0;std::uint64_t sequence=0;std::int64_t time=0;};
using MovementSource=MovementDemand(*)();
void SetMovementSource(MovementSource source);
bool StartTurningAudit(unsigned rotationOffset,unsigned rotationName,const unsigned char* expected);
bool StartInputAudit(unsigned viewport,unsigned controller,const std::array<unsigned,10>& offsets,unsigned tickName,const unsigned char* expected,bool(*world)());
bool BeginInputAuditEvent(void* self,void* function,void* parameters) noexcept;
void EndInputAuditEvent() noexcept;
bool StopInputAudit() noexcept;
void WriteInputAudit(const std::filesystem::path&);
}
