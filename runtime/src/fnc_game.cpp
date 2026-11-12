// Fight Night Champion: game-specific hooks.
#include "game.h"
#include "kernel/kernel.h"
#include "log.h"
#include "options.h"
#include "ppc_context.h"

#include <atomic>
#include <cmath>
#include <cstring>

#if GAME_FNC
using namespace wet;

// Frame rate. The game runs two loop objects (sub_827C6380: r3 the loop, f1
// its rate in Hz, r5 1 for the render loop): the simulation at a fixed 60 Hz
// and rendering at 30 Hz. Only the render loop's rate follows the fps option
// (the idea of the Xenia Canary "Unlock FPS" patch by illusion), so the game
// itself keeps its speed. The loop keeps its rate at +64 (float) and its
// period at +72 (u64 nanoseconds), updated when the option changes.
static std::atomic<uint32_t> s_renderLoop{0};
static std::atomic<int> s_renderRate{0};

static int WantedRenderRate()
{
    int fps = options::Fps();
    if (fps == 0)
        fps = options::MonitorRefreshRate();
    return fps < 30 ? 30 : fps;
}

static void ApplyRenderRate(uint32_t loop, int rate)
{
    uint8_t* base = g_mem.base;
    float f = float(rate);
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    PPC_STORE_U32(loop + 64, bits);
    PPC_STORE_U64(loop + 72, uint64_t(1e9 / rate + 0.5));
}

extern "C" void __imp__sub_827C6380(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_827C6380)
{
    bool render = ctx.r5.u32 == 1 && std::fabs(ctx.f1.f64 - 30.0) < 0.01;
    int rate = 0;
    if (render)
    {
        rate = WantedRenderRate();
        ctx.f1.f64 = double(rate);
        s_renderLoop = ctx.r3.u32;
        s_renderRate = rate;
        Log("Frame rate: render loop at %d Hz (the game's own: 30), simulation unchanged", rate);
    }
    __imp__sub_827C6380(ctx, base);
}

namespace wet::fnc
{
    // Every frame (VdSwap): follows changes of the fps option (F6, overlay).
    void FrameTick()
    {
        uint32_t loop = s_renderLoop.load(std::memory_order_relaxed);
        if (!loop)
            return;
        int rate = WantedRenderRate();
        if (rate != s_renderRate.load(std::memory_order_relaxed))
        {
            s_renderRate = rate;
            ApplyRenderRate(loop, rate);
            Log("Frame rate: render loop now %d Hz", rate);
        }
    }
}
#endif
