#pragma once
#include "kfvr/xr/OpenXrHost.h"
#include <memory>
namespace kfvr::xr {
// Deliberately labeled synthetic room; never substitutes for KFVR rendering.
class DiagnosticScene final:public IFrameProvider {
public:
    DiagnosticScene();~DiagnosticScene();
    bool TryGetPair(const runtime::PresentationRequirements&,const runtime::TrackingFrame&,
        graphics::TextureDescriptor&,graphics::PairTicket&) noexcept override;
    void PairConsumed(const graphics::PairTicket&,bool consumed) noexcept override;
    const std::string& Error()const noexcept;
private:struct State;std::unique_ptr<State> state_;std::string error_;
};
}
