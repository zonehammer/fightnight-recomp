// Coincident positions (diagnostics): the last Champion Mode fight entered
// from the story starts with the fighters and the ring ropes made of NaNs.
// The first sign is sub_82413DE0 (and sub_824358C8) finding an object exactly
// at its target's position: it stores (its point at +1904) - (the position
// of the object at +1604, through its virtual method) at +1920, which is
// all zero there and never in a normal fight. The first few times that
// happens the objects are logged ("Zero direction") and, once, a memory dump
// is taken (frames\memdump_<n>.bin, as F3) to see what they are.
#include "game.h"
#include "log.h"
#include "ppc_context.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

namespace wet { void RequestMemoryDump(); }   // memdump.cpp

#if GAME_FNC
namespace
{
    std::atomic<int> s_hits{ 0 };

    std::string Words(uint8_t* base, uint32_t address, int count)
    {
        std::string s;
        char w[12];
        for (int i = 0; i < count; i++)
        {
            std::snprintf(w, sizeof(w), "%08X ", PPC_LOAD_U32(address + i * 4));
            s += w;
        }
        return s;
    }
}

extern "C" void __imp__sub_82413DE0(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82413DE0)
{
    uint32_t self = ctx.r3.u32;
    __imp__sub_82413DE0(ctx, base);
    if (!self || self < 0x40000000u)
        return;
    uint32_t dx = PPC_LOAD_U32(self + 1920), dy = PPC_LOAD_U32(self + 1924), dz = PPC_LOAD_U32(self + 1928);
    if (((dx | dy | dz) & 0x7FFFFFFFu) != 0)
        return;
    int n = s_hits.fetch_add(1, std::memory_order_relaxed);
    if (n >= 6)
        return;
    uint32_t target = PPC_LOAD_U32(self + 1604);
    wet::Log("Zero direction #%d: object %08X (vtable %08X), target %08X (vtable %08X)", n + 1, self,
             PPC_LOAD_U32(self), target, target ? PPC_LOAD_U32(target) : 0);
    wet::Log("Zero direction: object +1888: %s", Words(base, self + 1888, 16).c_str());
    if (target)
        wet::Log("Zero direction: target +0: %s", Words(base, target, 32).c_str());
    if (n == 0)
        wet::RequestMemoryDump();
}
#endif
