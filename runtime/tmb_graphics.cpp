// Native graphics completion for the USA executable. Meshes, particles, overlays and movie pictures are already
// copied into the native renderer at their game hooks. The old hardware-only output instructions have no consumer.
// Replace those instructions before they reach MMIO; never maintain channel registers or synthesize DMA interrupts.
// The checker host deliberately executes the original instructions for historical call-log comparisons.
#include "tmb_port.h"
#include "bozo/libs/libraries.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace tmb::math { bool hudInstruction(uint8_t *, R5900Context *, Runtime *, uint32_t); }
namespace tmb::math { bool vehicleInstruction(uint8_t *, R5900Context *, Runtime *, uint32_t); }

namespace tmb::graphics
{
    namespace
    {
        // bozo:native kGraphicsOutput
        constexpr uint32_t kGraphicsOutput[] = {
            // HUD, shell loading, dialogs and menus (their native builders submit the actual draws).
            0x0013e03cu, 0x0013e044u, 0x0013e054u, 0x0013e060u,
            0x00162e34u, 0x00162e40u, 0x00162e50u, 0x00162e60u,
            0x00163a28u, 0x00163a30u, 0x00163a40u, 0x00163a50u,
            0x0018f35cu, 0x0018f368u, 0x0018f374u, 0x0018f384u,
            0x0019f88cu, 0x0019f894u, 0x0019f8a4u, 0x0019f8bcu,
            // Select the native VU0 calculations directly; there is no microcode upload.
            0x0015e820u, 0x0015e834u, 0x0015e838u, 0x0015e840u,
            0x001c1b58u, 0x001c1b64u, 0x001c1b74u, 0x001c1b88u,
            // Native presentation owns the display buffers and compositing.
            0x001c1804u, 0x001c181cu, 0x001c1854u, 0x001c1874u, 0x001c1880u,
            0x001d6754u, 0x001d675cu, 0x001d6764u, 0x001d676cu, 0x001d6774u,
            0x001d678cu, 0x001d6798u,
            // Movie field parity comes from the native frame clock.
            0x001c18b0u, 0x001c1a78u,
            // Fog, font and view setup. Their data is still built for the native material/overlay hooks.
            0x001c3aa4u, 0x001c3aacu, 0x001c3abcu, 0x001c3ac8u,
            0x001c40bcu, 0x001c40c4u, 0x001c40d4u, 0x001c40e4u,
            0x001d2fbcu, 0x001d2fc8u, 0x001d2fd8u, 0x001d2fe8u,
            0x001d300cu, 0x001d3018u, 0x001d3020u, 0x001d3034u,
            0x001d4dc4u, 0x001d4dc8u, 0x001d4dd0u, 0x001d4dd4u,
            0x001d4df4u, 0x001d4e00u, 0x001d4e08u, 0x001d4e10u,
            // Object traversal and particles: completion means the native renderer has copied their data.
            0x001d3cc4u, 0x001d3ce0u, 0x001d3cecu, 0x001d3cf8u, 0x001d3d04u,
            0x001d55fcu, 0x001d5608u, 0x001d5620u, 0x001d5628u, 0x001d5634u,
            0x001e9a74u, 0x001e9a80u, 0x001e9a90u,
            // Superseded split-view/queue flushes, movie uploads and loading-bar output.
            0x001d2958u, 0x001d2960u, 0x001d2970u, 0x001d2978u,
            0x001d2988u, 0x001d298cu, 0x001d2990u,
            0x001d50e8u, 0x001d511cu, 0x001d5128u,
            0x001df850u, 0x001df858u, 0x001df868u,
            0x001fc4bcu, 0x001fc4c4u, 0x001fc4d0u, 0x001fc4dcu, 0x001fc4ecu,
            0x00202184u, 0x00202190u, 0x0020219cu, 0x002021a8u,
            0x00202364u, 0x00202374u, 0x0020237cu, 0x00202388u, 0x00202394u,
            // A native submission has no outstanding hardware transfer (BC0 completion tests).
            0x001d47ccu, 0x001d47e8u, 0x001d55c8u,
        };

        constexpr auto kSortedGraphicsOutput = [] {
            auto sites = std::to_array(kGraphicsOutput);
            std::sort(sites.begin(), sites.end());
            return sites;
        }();

        bool instruction(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc)
        {
            if (tmb::math::hudInstruction(ram, ctx, rt, pc)) return true;
            if (tmb::math::vehicleInstruction(ram, ctx, rt, pc)) return true;
            if (!std::binary_search(kSortedGraphicsOutput.begin(), kSortedGraphicsOutput.end(), pc))
                return false;
            if (pc == 0x001c18b0u || pc == 0x001c1a78u)
                setRegU64(ctx, pc == 0x001c18b0u ? 3 : 2, (bozo::port::vblankCount(rt) & 1u) << 13);
            else if (pc == 0x0015e840u || pc == 0x001d3034u)
            {
                auto &vu = bozo::port::libraries(rt).vu0;
                vu.image = bozo::libs::Vu0Image::Hierarchy;
                vu.hierarchyHigh = true;
            }
            else if (pc == 0x001c1b88u)
                bozo::port::libraries(rt).vu0.image = bozo::libs::Vu0Image::TwoD;
            return true;
        }

        constexpr uint32_t kFlushObjects = 0x001d50b0u, kWaitView = 0x001d47e8u, kInstallDmaHandler = 0x001d5150u;

        void flushObjects(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            if (!bozo::port::nativeGraphics(rt))
            {
                hierFlushObjQ__Fv_0x1d50b0(ram, ctx, rt);
                return;
            }
            // The view was submitted by hierCheckVifCompleteOverride. Release the game's two queued-view flags
            // directly, instead of kicking a dummy channel and waiting for hierDmaHandler to flush them.
            const uint64_t cleared = 0;
            std::memcpy(ram + 0x004fb8a8u, &cleared, sizeof(cleared));
            ctx->pc = getRegU32(ctx, 31);
        }

        void waitView(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            if (!bozo::port::nativeGraphics(rt))
                hierWaitForVif1__Fv_0x1d47e8(ram, ctx, rt);
            else
                ctx->pc = getRegU32(ctx, 31);
        }

        void installHandler(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            if (!bozo::port::nativeGraphics(rt))
                hierSetDmaIntHandler__Fv_0x1d5150(ram, ctx, rt);
            else
            {
                setReturnS32(ctx, 0);
                ctx->pc = getRegU32(ctx, 31);
            }
        }
    }

    void install(Registry &runtime)
    {
        runtime.nativeGraphics(&instruction);
        runtime.registerFunction(kFlushObjects, &flushObjects);
        runtime.registerFunction(kWaitView, &waitView);
        runtime.registerFunction(kInstallDmaHandler, &installHandler);
    }
}
