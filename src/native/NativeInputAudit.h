#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>
#include <filesystem>
namespace kfvr::native {
using MovementSource=bool(*)(float&,float&,std::uint64_t&,std::int64_t&);
void SetMovementSource(MovementSource source);
bool StartInputAudit(unsigned viewport,unsigned controller,const std::array<unsigned,10>& offsets,unsigned tickName,const unsigned char* expected,bool(*world)());
bool BeginInputAuditEvent(void* self,void* function) noexcept;
void EndInputAuditEvent() noexcept;
bool StopInputAudit() noexcept;
void WriteInputAudit(const std::filesystem::path&);
}
