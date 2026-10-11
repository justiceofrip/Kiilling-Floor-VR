#include "Test.h"
#include "kfvr/graphics/SharedTextureProtocol.h"
using namespace kfvr::graphics;
int main(){
    TextureDescriptor d{};CHECK(!Valid(d));d.width=32;d.height=16;d.format=28;d.resourceEpoch=1;d.session[0]=1;CHECK(Valid(d));
    for(unsigned i=0;i<7;++i){auto bad=d;switch(i){case 0:bad.version++;break;case 1:bad.bytes--;break;case 2:bad.width=16385;break;case 3:bad.height=0;break;case 4:bad.format=0;break;case 5:bad.reserved[1]=1;break;case 6:bad.session={};break;}CHECK(!Valid(bad));}
    PairTicket t{};CHECK(!Valid(t,d));t.resourceEpoch=d.resourceEpoch;t.session=d.session;t.sequence=1;t.frameId=1;t.spaceGeneration=1;t.trackingGeneration=1;t.predictedNs=1;CHECK(Valid(t,d));
    for(unsigned i=0;i<10;++i){auto bad=t;switch(i){case 0:bad.version++;break;case 1:bad.bytes--;break;case 2:bad.resourceEpoch++;break;case 3:bad.session[15]++;break;case 4:bad.sequence=0x8000000000000000ULL;break;case 5:bad.frameId=0;break;case 6:bad.predictedNs=-1;break;case 7:bad.reserved0=1;break;case 8:bad.reserved[1]=1;break;case 9:bad.trackingGeneration=0;break;}CHECK(!Valid(bad,d));}
    t.sequence=0x7fffffffffffffffULL;CHECK(Valid(t,d));
    for(const auto format:{29u,91u}){auto srgb=d;srgb.format=format;CHECK(Valid(srgb));}
    CHECK(CopyCompatibleFormats(27,29));CHECK(CopyCompatibleFormats(28,29));CHECK(CopyCompatibleFormats(90,91));CHECK(!CopyCompatibleFormats(29,91));CHECK(!CopyCompatibleFormats(27,27+3));CHECK(!CopyCompatibleFormats(0,0));
    const auto select=[](std::initializer_list<std::int64_t> advertised){
        return SelectSwapchainFormat(std::span<const std::int64_t>(advertised.begin(),advertised.size()));
    };
    CHECK(select({})==0);
    CHECK(select({0,-1,10,27,90,0x10000005bLL})==0); // Unknown and typeless are not swapchain choices.
    CHECK(select({29})==29);CHECK(select({28})==28); // RGBA-only runtimes remain supported.
    CHECK(select({91})==91);CHECK(select({87})==87);
    CHECK(select({87,29})==29);CHECK(select({29,87})==29); // Do not trade sRGB for native byte order.
    CHECK(select({28,91})==91);CHECK(select({91,28})==91);
    CHECK(select({28,87})==28);CHECK(select({87,28})==28);
    CHECK(select({29,91})==29);CHECK(select({91,29})==29);
    CHECK(select({28,29,87,91})==29);CHECK(select({91,87,29,28})==29);
    CHECK(select({-91,0x10000005bLL,29,29,27})==29); // Exact advertised values, including fallback.
    return 0;
}
