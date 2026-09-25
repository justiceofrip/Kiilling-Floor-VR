#pragma once
#include <Windows.h>
#include <array>
#include <filesystem>
namespace kfvr::native {
bool StartInputAudit(unsigned viewport,unsigned controller,const std::array<unsigned,10>& offsets,unsigned tickName,const unsigned char* expected,bool(*world)());
void InputAuditEvent(void* self,void* function,bool after) noexcept;
bool StopInputAudit() noexcept;
void WriteInputAudit(const std::filesystem::path&);
}
