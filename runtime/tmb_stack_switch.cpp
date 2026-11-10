// The game's deliberate stack-pointer switches, declared to BozoEngine's kernel. Native callbacks (interrupt and alarm
// handlers) run on the interrupted thread's own stack, below its live stack pointer (the engine's owned handler
// stacks). On a PS2 they run on the kernel's stack, so TMB's hand-written code may use SP as an ordinary register:
// - hierTraverseAsm saves SP at s0 + 0x270 (0x1d5438), sets it to 0 (0x1d5450), loads a pointer into it (0x1d547c)
//   and restores it from s0 + 0x270 on its two exits (0x1d5718, and 0x1d5734 in a branch's delay slot);
// - sysSetSp / sysRestoreSp move SP to a work stack and back.
// Each switch parks the real SP with the kernel first (Kernel::parkThreadStack) and resumes it once restored, so a
// callback taken in between still finds the thread's stack. Native graphics only: the historical checker keeps the
// original instructions and the recorded hosts' handler stacks.

#include "tmb_port.h"
#include "bozo/ee/kernel.h"

#include <algorithm>
#include <array>

namespace tmb::stackSwitch
{
    namespace
    {
        constexpr uint32_t kParkSp = 0x001d5450u;                         // addu sp, zero, zero
        constexpr uint32_t kRestoreSp = 0x001d5718u, kRestoreSpDelay = 0x001d5734u; // ld sp, 0x270(s0)
        constexpr uint32_t kSysSetSp = 0x001eeae0u, kSysRestoreSp = 0x001eeaf0u;

        // bozo:native kStackSwitch (literals: tools/tmb_overrides_list.py reads them)
        constexpr uint32_t kStackSwitch[] = {0x001d5450u, 0x001d5718u, 0x001d5734u};
        static_assert(kStackSwitch[0] == kParkSp && kStackSwitch[1] == kRestoreSp && kStackSwitch[2] == kRestoreSpDelay);

        void sysSetSpOverride(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            const uint32_t sp = getRegU32(ctx, 29);
            if (bozo::port::nativeGraphics(rt))
                bozo::port::kernel(rt).parkThreadStack(sp);
            setRegU64(ctx, 2, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(sp)))); // move v0, sp
            ctx->r[29].ud[0] = ctx->r[4].ud[0];                                                         // move sp, a0
            ctx->pc = getRegU32(ctx, 31);
            (void)ram;
        }

        void sysRestoreSpOverride(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            ctx->r[29].ud[0] = ctx->r[4].ud[0]; // move sp, a0
            if (bozo::port::nativeGraphics(rt))
                bozo::port::kernel(rt).resumeThreadStack(getRegU32(ctx, 29));
            ctx->pc = getRegU32(ctx, 31);
            (void)ram;
        }
    }

    bool instruction(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc)
    {
        if (!std::binary_search(std::begin(kStackSwitch), std::end(kStackSwitch), pc))
            return false;
        if (pc == kParkSp)
        {
            bozo::port::kernel(rt).parkThreadStack(getRegU32(ctx, 29));
            ctx->r[29].ud[0] = 0;
            return true;
        }
        ctx->r[29].ud[0] = bozo::load<uint64_t>(ram, ctx, rt, getRegU32(ctx, 16) + 0x270u);
        bozo::port::kernel(rt).resumeThreadStack(getRegU32(ctx, 29));
        return true;
    }

    void install(Registry &runtime)
    {
        runtime.registerFunction(kSysSetSp, &sysSetSpOverride);
        runtime.registerFunction(kSysRestoreSp, &sysRestoreSpOverride);
    }
}
