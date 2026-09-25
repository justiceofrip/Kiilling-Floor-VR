#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace kfvr::graphics {
// Little-endian Windows wire types: no pointers, HANDLEs, bools, size_t or STL
// containers with dynamic storage. These are identical in x86 and x64 builds.
inline constexpr std::uint32_t TextureMagic=0x3156464b; // KFVR1
inline constexpr std::uint32_t TicketMagic=0x31545646;  // FVT1
inline constexpr std::uint32_t TextureProtocol=1;
struct alignas(8) TextureDescriptor {
    std::uint32_t magic=TextureMagic, version=TextureProtocol, bytes=64, format=0;
    std::uint32_t width=0, height=0, adapterLow=0;
    std::int32_t adapterHigh=0;
    std::uint64_t resourceEpoch=0;
    std::array<std::uint8_t,16> session{};
    std::array<std::uint32_t,2> reserved{};
};
struct FrameMetadata {
    std::uint64_t frameId=0, spaceGeneration=0, trackingGeneration=0;
    // Predicted display time in the shared OpenXR time domain, not wall time.
    std::int64_t predictedNs=0;
};
struct alignas(8) PairTicket {
    std::uint32_t magic=TicketMagic, version=TextureProtocol, bytes=96, reserved0=0;
    std::uint64_t resourceEpoch=0, sequence=0, frameId=0;
    std::uint64_t spaceGeneration=0, trackingGeneration=0;
    std::int64_t predictedNs=0;
    std::array<std::uint8_t,16> session{};
    std::array<std::uint64_t,2> reserved{};
};
static_assert(sizeof(TextureDescriptor)==64 && offsetof(TextureDescriptor,session)==40);
static_assert(sizeof(PairTicket)==96 && offsetof(PairTicket,sequence)==24 && offsetof(PairTicket,session)==64);
static_assert(std::is_trivially_copyable_v<TextureDescriptor> && std::is_trivially_copyable_v<PairTicket>);
// A GPU copy preserves bytes. It does not convert gamma or swap color channels.
// XR runtimes may return typeless backing textures for a typed swapchain format.
inline bool CopyCompatibleFormats(std::uint32_t a,std::uint32_t b) noexcept {
    const auto group=[](std::uint32_t f){return (f==27||f==28||f==29)?1:(f==90||f==87||f==91)?2:0;};
    return group(a)!=0 && group(a)==group(b);
}
inline bool Valid(const TextureDescriptor& d) noexcept {
    bool any=false;for(auto b:d.session)any|=b!=0;
    // Resolved, typed RGBA8/BGRA8, with explicit linear or sRGB interpretation.
    return d.magic==TextureMagic && d.version==TextureProtocol && d.bytes==sizeof(d) &&
        (d.format==28 || d.format==29 || d.format==87 || d.format==91) && d.width && d.width<=16384 &&
        d.height && d.height<=16384 && d.resourceEpoch && any && !d.reserved[0] && !d.reserved[1];
}
inline bool Valid(const PairTicket& t,const TextureDescriptor& d) noexcept {
    return Valid(d) && t.magic==TicketMagic && t.version==TextureProtocol && t.bytes==sizeof(t) &&
        t.resourceEpoch==d.resourceEpoch && t.session==d.session && t.sequence &&
        t.sequence<=0x7fffffffffffffffULL && t.frameId && t.spaceGeneration &&
        t.trackingGeneration && t.predictedNs>0 && !t.reserved0 && !t.reserved[0] && !t.reserved[1];
}
}
