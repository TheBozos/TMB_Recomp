// SPDX-License-Identifier: MIT
#pragma once
#include <array>

namespace tmb::shading
{
    using Vector3 = std::array<float, 3>;
    using Colour = std::array<float, 4>;
    using Matrix = std::array<Colour, 4>; // column-major

    struct DirectionalLight { Vector3 direction{}, colour{}; };
    struct PointLight
    {
        Vector3 direction{}, position{}, colour{};
        float coneOffset = 0, attenuation = 0;
    };
    struct PointLighting
    {
        std::array<PointLight, 4> lights{};
        bool enabled = false;
        unsigned count = 0; // selection retains four; the shipped shader uses the first three
    };
    struct Fog { float base = 0, limit = 0, wavePhase = 0, inverseDepth = 0; };
    static_assert(sizeof(Matrix) == 16 * sizeof(float));
    static_assert(sizeof(Fog) == 4 * sizeof(float));
    struct Parameters
    {
        Matrix object{}, camera{}, clipCamera{};
        std::array<DirectionalLight, 3> directional{};
        PointLighting point{};
        Colour tint{}, colourOffset{};
        std::array<float, 2> textureOffset{};
        float depthScale = 0, fogOffset = 0, morphWeight = 0;
        Fog fog{};
    };
    // The two explicit dependencies shared by successive objects; owned by the scene's run.
    struct RetainedParameters
    {
        Matrix opaqueObject{};
        std::array<Vector3, 3> opaqueDirections{};
        float morphWeight = 0;
    };
}
