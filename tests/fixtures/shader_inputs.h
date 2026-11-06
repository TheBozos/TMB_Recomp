// SPDX-License-Identifier: MIT
// Test-only decoding of the retired packed shader input, independent of native producers.
#pragma once
#include "shader_reference.h"
#include "tmb_shading.h"

namespace shader_fixture
{
    inline tmb::shading::Parameters decode(const tmb::shader_reference::VuImage &image)
    {
        tmb::shading::Parameters p;
        for (unsigned i = 0; i < 4; ++i)
            for (unsigned k = 0; k < 4; ++k)
            {
                p.object[i][k] = image.f(i,k);
                p.camera[i][k] = image.f(9+i,k);
                p.clipCamera[i][k] = image.f(13+i,k);
            }
        for (unsigned k = 0; k < 4; ++k)
        {
            p.tint[k] = image.f(5,k); p.colourOffset[k] = image.f(20,k);
            p.point.lights[k].attenuation = image.f(175,k);
            for (unsigned i = 0; i < 3; ++i)
                p.point.lights[k].colour[i] = k == 3 ? image.f(172+i,3) : image.f(172+k,i);
        }
        for (unsigned i = 0; i < 3; ++i)
        {
            for (unsigned k = 0; k < 3; ++k)
            {
                p.directional[i].direction[k] = image.f(6+k,i);
                p.directional[i].colour[k] = image.f(17+i,k);
                p.point.lights[i].direction[k] = image.f(165+k,i);
                p.point.lights[i].position[k] = image.f(169+k,i);
            }
            p.point.lights[i].coneOffset = image.f(168,i);
        }
        p.textureOffset = {image.f(4,0), image.f(4,1)};
        p.depthScale = image.f(4,2); p.fogOffset = image.f(4,3);
        p.fog = {image.f(21,0), image.f(21,1), image.f(21,2), image.f(21,3)};
        p.morphWeight = image.f(31,0);
        return p;
    }
}
