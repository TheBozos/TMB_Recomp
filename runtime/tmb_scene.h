// SPDX-License-Identifier: MIT
// Renderer hierarchy records. No processor registers, addressable VU memory or microprogram state.
#pragma once
#include "tmb_hierarchy_math.h"
#include "tmb_shader_parameters.h"
#include "bozo/runtime.h"
#include <memory>

namespace tmb::scene
{
    using Vector = math::Vector;
    using Basis = std::array<Vector, 3>;
    struct Frame { Basis rotation{}, outer{}, inner{}, directions{}; bool valid = false; unsigned groups = 0; };
    struct PointLight { Vector sphere{}, direction{}, colour{}; };
    using Lighting = shading::PointLighting;
    struct Scene
    {
        Vector viewPosition{}, position{}, sphere{}, nearRange{};
        Basis viewOuter{}, viewInner{}, viewDirections{};
        std::array<Frame, 256> frames{};
        math::Matrix object{};
        Basis objectDirections{};
        Lighting lighting{};
        shading::RetainedParameters renderer{};
        std::array<PointLight, 8> points{};
        unsigned pointCount = 0, pointWrites = 0, ambientMask = 0, selectedFrame = 0;
        uint32_t nearMargin = 0, outside = 0, inside = 0, distance = 0;

        Frame &frame(unsigned index);
        void root();
        void coordinate(const Basis &, const Vector &);
        void child(unsigned parent, const Basis &, const Vector &);
        void fov(unsigned index, const Vector &bounds);
        void project(unsigned index);
        void selectLights();
        void scaledDistance(const Vector &offset, uint32_t scale);
        void translate(const Vector &offset, bool macro);
    };
    // A player host owns one scene for its run. Bindings are confined to its game thread and removed on shutdown.
    class Binding
    {
        Runtime *runtime_;
        std::unique_ptr<Scene> scene_;
    public:
        explicit Binding(Runtime *);
        ~Binding();
        Binding(const Binding &) = delete;
        Binding &operator=(const Binding &) = delete;
    };
    Scene &get(Runtime *);
    bool instruction(uint8_t *, R5900Context *, Runtime *, uint32_t);
}
