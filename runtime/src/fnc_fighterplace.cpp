// The last Champion Mode fight (Frost), entered from the story, started with
// no fighters and no ring ropes - only the referee - since 0.1.0 (RPCS3 had
// the same on PS3). The fighters' controllers (sub_82413DE0, sub_824358C8)
// did their first update while both fighters (vtable 820301F0, position at
// +96) were still at the origin; they are placed in their corners, at
// (-5, 0, -5) and (5, 0, 5), a frame later. The zero directions from that
// update became NaNs that stayed in the fighters' and ropes' physics for the
// whole fight, so neither was drawn (a checkpoint reload places them first,
// which is why losing the fight fixed it). Both updates now wait while their
// fighter is exactly at (0, 0, 0) - one frame, in practice.
// WET_FNC_NO_PLACE_WAIT=1 turns this off.
#include "game.h"
#include "log.h"
#include "ppc_context.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>

#if GAME_FNC
namespace
{
    std::atomic<int> s_waits{ 0 };

    const bool s_noWait = [] {
        const char* v = std::getenv("WET_FNC_NO_PLACE_WAIT");
        return v && v[0] == '1';
    }();

    // A fighter (vtable 820301F0) still at exactly the origin.
    bool Unplaced(uint8_t* base, uint32_t fighter)
    {
        if (s_noWait || fighter < 0x40000000u || PPC_LOAD_U32(fighter) != 0x820301F0u)
            return false;
        uint32_t x = PPC_LOAD_U32(fighter + 96), y = PPC_LOAD_U32(fighter + 100), z = PPC_LOAD_U32(fighter + 104);
        return ((x | y | z) & 0x7FFFFFFFu) == 0;
    }

    void NoteWait(uint32_t fighter)
    {
        if (s_waits.fetch_add(1, std::memory_order_relaxed) < 4)
            wet::Log("FNC: waiting for the fighter %08X to be placed", fighter);
    }
}

// Point at +1904 minus the position of the fighter at +1604, kept at +1920.
extern "C" void __imp__sub_82413DE0(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82413DE0)
{
    uint32_t self = ctx.r3.u32;
    if (self >= 0x40000000u)
    {
        uint32_t fighter = PPC_LOAD_U32(self + 1604);
        if (Unplaced(base, fighter))
        {
            NoteWait(fighter);
            return;
        }
    }
    __imp__sub_82413DE0(ctx, base);
}

// The same distances from the fighter at +32 (points at +544 and +560).
extern "C" void __imp__sub_824358C8(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_824358C8)
{
    uint32_t self = ctx.r3.u32;
    if (self >= 0x40000000u)
    {
        uint32_t fighter = PPC_LOAD_U32(self + 32);
        if (Unplaced(base, fighter))
        {
            NoteWait(fighter);
            return;
        }
    }
    __imp__sub_824358C8(ctx, base);
}
#endif
