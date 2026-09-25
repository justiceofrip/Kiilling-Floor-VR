#pragma once
#include "kfvr/math/StereoMath.h"
#include <array>
#include <optional>
namespace kfvr::math {
// Canonical LH row-vector coordinates, distances in the caller's world units.
// A conservative symmetric volume enclosing both complete eye frusta. Adapters
// with native multiview culling may instead build independent eye visibility.
struct StereoCullEnvelope {
    Matrix4 world{};
    FovTangents fov{};
    float nearPlane=0,farPlane=0;
};
std::optional<StereoCullEnvelope> EncloseStereoFrusta(const Matrix4& centerCamera,
    const std::array<Matrix4,2>& eyeCameras,const std::array<FovTangents,2>& eyeFov,
    float nearPlane,float farPlane) noexcept;
}
