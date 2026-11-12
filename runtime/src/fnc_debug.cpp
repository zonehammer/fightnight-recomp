// Fight Night Champion: boot diagnostics (removed once the game runs).
#include "game.h"
#include "kernel/kernel.h"
#include "kernel/threads.h"
#include "log.h"
#include "ppc_context.h"

#if GAME_FNC
using namespace wet;

// EA T3db: a database query (sub_82EB5748: r3 database, r4 query, r8 the
// result's status - its word at +4 is the error code). A null result is
// used without a check by its callers (the first boot crashed on one).
extern "C" void __imp__sub_82EB5748(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82EB5748)
{
    uint32_t db = ctx.r3.u32, query = ctx.r4.u32, status = ctx.r8.u32;
    __imp__sub_82EB5748(ctx, base);
    if (ctx.r3.u32 == 0)
    {
        static int logged = 0;
        if (logged++ < 30)
        {
            char text[200] = "";
            if (query)
            {
                // The query, if it's text.
                uint32_t n = 0;
                for (; n < 120; n++)
                {
                    uint8_t c = *HostPtr(query + n);
                    if (c == 0 || c < 9 || c > 126)
                        break;
                    text[n] = char(c);
                }
                text[n] = 0;
            }
            Log("T3db: query on database %08X failed with error %u (status at %08X); query %08X \"%s\"%s", db,
                status ? Ld32(status + 4) : 0xFFFFFFFFu, status, query, text, CurrentGuestStack().c_str());
        }
    }
}
#endif
