// Fight Night Champion: boot diagnostics (removed once the game runs).
#include "game.h"
#include "kernel/kernel.h"
#include "kernel/threads.h"
#include "log.h"
#include "ppc_context.h"

#include <cstdio>
#include <string>

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
            if (db && logged == 1)
            {
                // The database's tables: a list from +12, next at +4, the
                // table's short name at +32.
                std::string names;
                uint32_t count = 0;
                for (uint32_t t = Ld32(db + 12); t && count < 120; t = Ld32(t + 4), count++)
                {
                    char nm[8];
                    uint32_t v = Ld32(t + 32);
                    snprintf(nm, sizeof nm, "%c%c%c%c", char(v >> 24), char(v >> 16), char(v >> 8), char(v));
                    names += ' ';
                    names += nm;
                    if (count < 3 || t == 0)
                    {
                        char b[160];
                        snprintf(b, sizeof b, "(%08X: %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X)", t, Ld32(t),
                                 Ld32(t + 4), Ld32(t + 8), Ld32(t + 12), Ld32(t + 16), Ld32(t + 20), Ld32(t + 24),
                                 Ld32(t + 28), Ld32(t + 36), Ld32(t + 40));
                        names += b;
                    }
                }
                std::string head;
                for (uint32_t i = 0; i < 64; i += 4)
                {
                    char b[12];
                    snprintf(b, sizeof b, " %08X", Ld32(db + i));
                    head += b;
                }
                Log("T3db: database %08X:%s", db, head.c_str());
                Log("T3db: %u tables:%s", count, names.c_str());
            }
        }
    }
}
#endif

#if GAME_FNC
// T3db's database load (sub_82EB6238) and the per-table loaders it calls
// (r5: the table's short name); errors logged.
static void LogT3dbError(const char* what, uint32_t name, uint32_t err)
{
    static int logged = 0;
    if (err && logged++ < 40)
        Log("T3db: %s %c%c%c%c -> error %u%s", what, char(name >> 24), char(name >> 16), char(name >> 8), char(name), err,
            CurrentGuestStack().c_str());
}
extern "C" void __imp__sub_82EB6238(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82EB6238)
{
    __imp__sub_82EB6238(ctx, base);
    Log("T3db: database load -> %u", ctx.r3.u32);
}
extern "C" void __imp__sub_82EB4F00(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82EB4F00)
{
    uint32_t name = ctx.r5.u32;
    __imp__sub_82EB4F00(ctx, base);
    LogT3dbError("new table", name, ctx.r3.u32);
}
extern "C" void __imp__sub_82EB4B78(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82EB4B78)
{
    uint32_t name = ctx.r5.u32;
    __imp__sub_82EB4B78(ctx, base);
    LogT3dbError("reload table", name, ctx.r3.u32);
}
extern "C" void __imp__sub_82EAB130(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82EAB130)
{
    uint32_t size = ctx.r5.u32;
    __imp__sub_82EAB130(ctx, base);
    static int logged = 0;
    if (ctx.r3.u32 && logged++ < 20)
        Log("T3db: stream read of %u bytes -> error %u%s", size, ctx.r3.u32, CurrentGuestStack().c_str());
}
#endif
