// TMB's runtime on BozoEngine (phase 4): what every runtime file includes. The context helpers (getRegU32,
// setReturnS32, SET_GPR_U32...), the generated functions by name (tmb_functions.h, made by bozo-recomp from the ELF
// with TMB's externals), and the port API (bozo/port.h: registration, the kernel, the EE clock, the frame end).
//
// The names the fork gave these keep working where it was cheap: an override's third argument is still spelled
// PS2Runtime (BozoEngine's opaque Runtime), and the install functions take the registry as `runtime`, so their
// runtime.registerFunction(...) lines are unchanged.
#pragma once

#include "bozo/context.h"
#include "bozo/port.h"
#include "bozo/recomp.h"
#include "tmb_functions.h"

#include <cstdint>
#include <string>

using PS2Runtime = Runtime;

namespace tmb
{
    using Registry = bozo::port::Registry;

    // The disc image that boots this executable: TMB_ISO, else an .iso found walking up from the folder given (the
    // ELF's or the executable's) whose SYSTEM.CNF names `elfName` (runtime/tmb_overrides.cpp).
    std::string findDiscImage(const std::string &fromFolder, const std::string &elfName);

    // CVT.W.S as the R5900 makes it: toward zero, saturating at the int32 range (the fork's FPU_CVT_W_S).
    inline int32_t cvtWS(float v)
    {
        if (v >= 2147483648.0f)
            return INT32_MAX;
        if (v <= -2147483648.0f)
            return INT32_MIN;
        return static_cast<int32_t>(v);
    }
}

// FPU helpers the particles use (the fork's ps2_runtime_macros.h names), on BozoEngine's PS2 float semantics.
inline float ps2Float(uint32_t bits)
{
    float f;
    __builtin_memcpy(&f, &bits, 4);
    return f;
}
inline uint32_t ps2Bits(float f)
{
    uint32_t b;
    __builtin_memcpy(&b, &f, 4);
    return b;
}
