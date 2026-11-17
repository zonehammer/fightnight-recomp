// Lua errors (diagnostics): the game's scripts (Champion Mode's scripted fights,
// the camera logic, ...) run on Lua 5.1, whose errors a retail build never
// shows. Every one ends in luaD_throw (sub_82842758, from luaG_errormsg
// sub_82844D50, which runtime errors reach through addinfo sub_82844C40); the
// message is the value on top of the Lua stack. Logged to fightnight.log as
// "Lua error" (the first 200), then thrown as usual.
#include "game.h"
#include "log.h"
#include "ppc_context.h"

#include <atomic>
#include <cstdint>
#include <string>

#if GAME_FNC
namespace
{
    std::atomic<int> s_logged{ 0 };

    const char* CodeName(uint32_t code)
    {
        switch (code)
        {
        case 1: return "yield";
        case 2: return "runtime";
        case 3: return "syntax";
        case 4: return "out of memory";
        case 5: return "error in error handling";
        default: return "?";
        }
    }
}

// luaD_throw(lua_State* L, int errcode)
extern "C" void __imp__sub_82842758(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82842758)
{
    int n = s_logged.fetch_add(1, std::memory_order_relaxed);
    if (n < 200)
    {
        uint32_t L = ctx.r3.u32, code = ctx.r4.u32;
        std::string text = "(no message)";
        // lua_State: top at +8. TValue: 16 bytes, Value at +0, tt at +8
        // (LUA_TSTRING 4). TString: len at +12, characters at +16.
        uint32_t top = L ? PPC_LOAD_U32(L + 8) : 0;
        if (top >= 16)
        {
            uint32_t tt = PPC_LOAD_U32(top - 16 + 8);
            uint32_t ts = PPC_LOAD_U32(top - 16);
            if (tt == 4 && ts)
            {
                uint32_t len = PPC_LOAD_U32(ts + 12);
                if (len > 400)
                    len = 400;
                text.assign(reinterpret_cast<const char*>(base + ts + 16), len);
            }
            else
            {
                text = "(message of Lua type " + std::to_string(tt) + ")";
            }
        }
        wet::Log("Lua error (%s, state %08X, lr %08X): %s", CodeName(code), L, uint32_t(ctx.lr), text.c_str());
        if (n == 199)
            wet::Log("Lua error: no more are logged");
    }
    __imp__sub_82842758(ctx, base);
}
#endif
