// SPDX-License-Identifier: MIT
// Audited projection/weather helper boundaries; no VU register or memory transport.
#include "tmb_port.h"
#include "tmb_projection_math.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace tmb::math
{
    namespace
    {
        constexpr uint32_t kPointRows = 0x1da238, kWeatherIdentity = 0x1db0f0;
        constexpr uint32_t kScreenReturn = 0x1dc438, kWeatherPointReturn = 0x1e8cfc, kWeatherIdentityReturn = 0x1e8c48;
        struct Statistics
        {
            uint64_t screen{}, weather{}, identity{}, checked{};
            ~Statistics()
            {
                std::fprintf(stderr,"[native:projection] screen %llu weather %llu identity %llu original comparisons %llu\n",
                    (unsigned long long)screen,(unsigned long long)weather,(unsigned long long)identity,(unsigned long long)checked);
            }
        } statistics;
        uint8_t *span(Runtime *rt, uint32_t address, uint32_t bytes)
        {
            address &= ~15u;
            const auto offset = bozo::GuestMemory::flatOffset(address,bytes);
            if (offset < 0 || uint64_t(offset)+bytes > bozo::kFlatVu0Offset)
                throw bozo::libs::BadAccess{address,bytes};
            return rt->memory.flat()+offset;
        }
        bool checking()
        {
            static const bool enabled = [] { const char *v=std::getenv("TMB_NATIVE_PROJECTION_CHECK"); return v && *v=='1'; }();
            return enabled;
        }
        template <typename Record> void compare(const Record &result, uint8_t *out, uint8_t *ram,
                                                R5900Context *ctx, Runtime *rt, RecompiledFunction original)
        {
            if (!checking()) return;
            Record saved, expected;
            std::memcpy(&saved,out,sizeof(saved));
            auto copied = *ctx;
            original(ram,&copied,rt);
            std::memcpy(&expected,out,sizeof(expected));
            std::memcpy(out,&saved,sizeof(saved));
            if (std::memcmp(&result,&expected,sizeof(result))) throw std::runtime_error("native projection record differs from original");
            ++statistics.checked;
        }
        void pointRows(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            const uint32_t caller = getRegU32(ctx,31);
            if (!bozo::port::nativeGraphics(rt) || (caller!=kScreenReturn && caller!=kWeatherPointReturn))
            { mathfMulVec4x4__FPA3_fP8_fvectorT1_0x1da238(ram,ctx,rt); return; }
            Matrix matrix;
            Vector point;
            std::memcpy(&matrix,span(rt,getRegU32(ctx,4),64),64);
            std::memcpy(&point,span(rt,getRegU32(ctx,5),16),16);
            auto *out = span(rt,getRegU32(ctx,6),16);
            const auto result = projectPointRows(matrix,point);
            compare(result,out,ram,ctx,rt,mathfMulVec4x4__FPA3_fP8_fvectorT1_0x1da238);
            std::memcpy(out,&result,16);
            if (caller==kScreenReturn) ++statistics.screen; else ++statistics.weather;
            ctx->pc = caller;
        }
        void weatherIdentity(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            if (!bozo::port::nativeGraphics(rt) || getRegU32(ctx,31)!=kWeatherIdentityReturn)
            { mathfUnitMatrixAsm__FPA3_f_0x1db0f0(ram,ctx,rt); return; }
            auto *out = span(rt,getRegU32(ctx,4),64);
            const auto result = identityMatrix();
            compare(result,out,ram,ctx,rt,mathfUnitMatrixAsm__FPA3_f_0x1db0f0);
            std::memcpy(out,&result,64);
            // The original helper also returns these constant records in its scalar CPU ABI.
            setRegU64(ctx,7,getRegU64(ctx,4));
            for (unsigned i=0;i<4;++i) std::memcpy(&ctx->r[5-i],&result.column[i],16);
            ++statistics.identity;
            ctx->pc = getRegU32(ctx,31);
        }
    }
    void installProjection(Registry &runtime)
    {
        runtime.registerFunction(kPointRows,&pointRows);
        runtime.registerFunction(kWeatherIdentity,&weatherIdentity);
    }
}
