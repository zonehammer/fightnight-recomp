// Camera diagnostics (test build): the game's camera director code
// (0x82318000-0x8231F000, the class with "CameraDirector") is counted call by
// call. Every second in which the set of functions called changes, or every
// 10 s, a line goes to fightnight.log ("CamTrace"), so a log of the camera
// going wrong shows which of its code ran differently from then on. The first
// few small values each function is given (r4, r5: shot / camera ids) are
// logged too. FNC_CAMTRACE=0 turns it off.
#include "game.h"
#include "log.h"
#include "os.h"
#include "ppc_context.h"

#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <utility>

#if GAME_FNC
namespace
{
    constexpr int kCount = 138;
    const uint32_t kAddr[kCount] = { 0x823180D8, 0x823181C8, 0x823182A0, 0x82318430, 0x823184A0, 0x823184C8, 0x82318510, 0x82318528, 0x82318540, 0x82318558, 0x823185D8, 0x823185E8, 0x82318708, 0x82318788, 0x82318828, 0x82318870, 0x823188A0, 0x82318920, 0x823189D0, 0x82318A40, 0x82318AB8, 0x82318B08, 0x82318B68, 0x82318BD0, 0x82318C30, 0x82318C58, 0x82318CA0, 0x82318D18, 0x82318D88, 0x82318E30, 0x82318E68, 0x82318F28, 0x82318F70, 0x823191D0, 0x82319220, 0x82319268, 0x82319300, 0x82319348, 0x82319390, 0x82319420, 0x82319818, 0x823198A0, 0x823198F0, 0x82319940, 0x82319BE8, 0x82319CC0, 0x82319D38, 0x82319DC8, 0x82319E10, 0x82319F38, 0x8231A0E8, 0x8231A298, 0x8231A418, 0x8231A4F0, 0x8231A5D8, 0x8231A658, 0x8231A708, 0x8231A828, 0x8231A9A8, 0x8231A9E0, 0x8231AA80, 0x8231AB28, 0x8231AB88, 0x8231AC10, 0x8231AC60, 0x8231ACF0, 0x8231B510, 0x8231B5E8, 0x8231B8A0, 0x8231B9A0, 0x8231BEC8, 0x8231C048, 0x8231C120, 0x8231C220, 0x8231C2A0, 0x8231C510, 0x8231C568, 0x8231C6F0, 0x8231C750, 0x8231C828, 0x8231C888, 0x8231C8E8, 0x8231C938, 0x8231C9D0, 0x8231CA58, 0x8231CA90, 0x8231CB58, 0x8231CBC0, 0x8231CC98, 0x8231CD18, 0x8231CD70, 0x8231D160, 0x8231D1B0, 0x8231D288, 0x8231D2E0, 0x8231D458, 0x8231D4C8, 0x8231D688, 0x8231D708, 0x8231DAB8, 0x8231DB70, 0x8231DCF0, 0x8231DDF0, 0x8231DE48, 0x8231E020, 0x8231E090, 0x8231E0E0, 0x8231E160, 0x8231E258, 0x8231E328, 0x8231E378, 0x8231E3C0, 0x8231E448, 0x8231E520, 0x8231E650, 0x8231E660, 0x8231E680, 0x8231E690, 0x8231E758, 0x8231E768, 0x8231E8E8, 0x8231E950, 0x8231E970, 0x8231E9F0, 0x8231EA38, 0x8231EA90, 0x8231EAF0, 0x8231EB60, 0x8231EBA8, 0x8231EBE0, 0x8231EC00, 0x8231EC88, 0x8231ED00, 0x8231ED50, 0x8231EDE8, 0x8231EEC0, 0x8231EF58, 0x8231EFD8 };
    std::atomic<uint32_t> s_calls[kCount];
    std::mutex s_argLock;
    std::set<std::pair<int, uint64_t>> s_args;

    bool Enabled()
    {
        static const bool on = [] {
            const char* v = std::getenv("FNC_CAMTRACE");
            return !(v && v[0] == '0');
        }();
        return on;
    }

    float LdF(uint8_t* base, uint32_t a)
    {
        uint32_t v = PPC_LOAD_U32(a);
        float f;
        std::memcpy(&f, &v, 4);
        return f;
    }

    // Details of the two places the trace pointed at (prison fight, camera
    // stuck after an event 5 was sent and sub_8231C9D0 ran twice a frame).
    void Detail(uint32_t addr, PPCContext& ctx, uint8_t* base)
    {
        static std::atomic<int> lines{ 0 };
        if (lines.load(std::memory_order_relaxed) > 3000)
            return;
        if (addr == 0x8231C9D0)
        {
            // The director's current camera (+8 of the object at +4): used if its
            // weights (+80, +4) are above 0, otherwise a default camera.
            uint32_t self = ctx.r3.u32, inner = self ? PPC_LOAD_U32(self + 4) : 0;
            uint32_t cam = inner ? PPC_LOAD_U32(inner + 8) : 0;
            float w80 = cam ? LdF(base, cam + 80) : 0, w4 = cam ? LdF(base, cam + 4) : 0;
            bool used = cam && w80 > 0 && w4 > 0;
            struct Last { uint32_t self, cam; bool used; };
            static Last last[4];
            static std::mutex lock;
            std::lock_guard g(lock);
            int slot = -1;
            for (int k = 0; k < 4; k++)
                if (last[k].self == self) slot = k;
            if (slot < 0)
                for (int k = 0; k < 4; k++)
                    if (!last[k].self) { slot = k; last[k] = { self, 0xFFFFFFFF, false }; break; }
            if (slot >= 0 && (last[slot].cam != cam || last[slot].used != used))
            {
                last[slot].cam = cam;
                last[slot].used = used;
                lines++;
                wet::Log("CamTrace: director %08X camera %08X (weights %.3f %.3f) -> %s camera", self, cam, w80, w4,
                         used ? "its" : "the DEFAULT");
            }
        }
        else if (addr == 0x82319CC0)
        {
            uint32_t inner = ctx.r3.u32 ? PPC_LOAD_U32(ctx.r3.u32 + 4) : 0;
            uint32_t r9 = inner ? PPC_LOAD_U32(inner + 12) : 0;
            float gate = r9 ? LdF(base, r9 + 12) : 0;
            lines++;
            wet::Log("CamTrace: event %d (r5 %d, f1 %.3f) to %08X%s", int(ctx.r4.s32), int(ctx.r5.s32), ctx.f1.f64,
                     ctx.r3.u32, gate > 0 ? " - ignored (busy)" : "");
        }
    }

    void Note(int i, uint32_t r4, uint32_t r5)
    {
        s_calls[i].fetch_add(1, std::memory_order_relaxed);
        if (r4 < 64 || r5 < 64)
        {
            uint64_t key = (uint64_t(r4 < 64 ? r4 : 0xFFFF) << 32) | (r5 < 64 ? r5 : 0xFFFF);
            std::lock_guard lock(s_argLock);
            if (s_args.size() < 4000 && s_args.insert({ i, key }).second)
                wet::Log("CamTrace: sub_%08X(r4=%s, r5=%s) first time", kAddr[i],
                         r4 < 64 ? std::to_string(r4).c_str() : "-", r5 < 64 ? std::to_string(r5).c_str() : "-");
        }
    }
}

namespace wet::fnc
{
    // Once a second (FrameTick).
    void CamTraceTick()
    {
        if (!Enabled())
            return;
        static uint64_t last = 0;
        static int ticks = 0;
        static std::string lastSet;
        uint64_t now = os::MonotonicNs();
        if (now - last < 1000000000ull)
            return;
        last = now;
        std::string set, counts;
        char buf[32];
        for (int i = 0; i < kCount; i++)
        {
            uint32_t n = s_calls[i].exchange(0, std::memory_order_relaxed);
            if (!n)
                continue;
            std::snprintf(buf, sizeof buf, " %X", kAddr[i] & 0xFFFF);
            set += buf;
            std::snprintf(buf, sizeof buf, " %X:%u", kAddr[i] & 0xFFFF, n);
            counts += buf;
        }
        if (set != lastSet || ++ticks >= 10)
        {
            ticks = 0;
            wet::Log("CamTrace:%s", counts.empty() ? " (none)" : counts.c_str());
        }
        lastSet = set;
    }
}

#define CAM_HOOK(i, name)                                                  \
    extern "C" void __imp__##name(PPCContext& ctx, uint8_t* base);        \
    PPC_FUNC(name)                                                         \
    {                                                                      \
        if (Enabled())                                                     \
        {                                                                  \
            Note(i, ctx.r4.u32, ctx.r5.u32);                               \
            if (kAddr[i] == 0x8231C9D0 || kAddr[i] == 0x82319CC0)          \
                Detail(kAddr[i], ctx, base);                               \
        }                                                                  \
        __imp__##name(ctx, base);                                          \
    }

CAM_HOOK(0, sub_823180D8)
CAM_HOOK(1, sub_823181C8)
CAM_HOOK(2, sub_823182A0)
CAM_HOOK(3, sub_82318430)
CAM_HOOK(4, sub_823184A0)
CAM_HOOK(5, sub_823184C8)
CAM_HOOK(6, sub_82318510)
CAM_HOOK(7, sub_82318528)
CAM_HOOK(8, sub_82318540)
CAM_HOOK(9, sub_82318558)
CAM_HOOK(10, sub_823185D8)
CAM_HOOK(11, sub_823185E8)
CAM_HOOK(12, sub_82318708)
CAM_HOOK(13, sub_82318788)
CAM_HOOK(14, sub_82318828)
CAM_HOOK(15, sub_82318870)
CAM_HOOK(16, sub_823188A0)
CAM_HOOK(17, sub_82318920)
CAM_HOOK(18, sub_823189D0)
CAM_HOOK(19, sub_82318A40)
CAM_HOOK(20, sub_82318AB8)
CAM_HOOK(21, sub_82318B08)
CAM_HOOK(22, sub_82318B68)
CAM_HOOK(23, sub_82318BD0)
CAM_HOOK(24, sub_82318C30)
CAM_HOOK(25, sub_82318C58)
CAM_HOOK(26, sub_82318CA0)
CAM_HOOK(27, sub_82318D18)
CAM_HOOK(28, sub_82318D88)
CAM_HOOK(29, sub_82318E30)
CAM_HOOK(30, sub_82318E68)
CAM_HOOK(31, sub_82318F28)
CAM_HOOK(32, sub_82318F70)
CAM_HOOK(33, sub_823191D0)
CAM_HOOK(34, sub_82319220)
CAM_HOOK(35, sub_82319268)
CAM_HOOK(36, sub_82319300)
CAM_HOOK(37, sub_82319348)
CAM_HOOK(38, sub_82319390)
CAM_HOOK(39, sub_82319420)
CAM_HOOK(40, sub_82319818)
CAM_HOOK(41, sub_823198A0)
CAM_HOOK(42, sub_823198F0)
CAM_HOOK(43, sub_82319940)
CAM_HOOK(44, sub_82319BE8)
CAM_HOOK(45, sub_82319CC0)
CAM_HOOK(46, sub_82319D38)
CAM_HOOK(47, sub_82319DC8)
CAM_HOOK(48, sub_82319E10)
CAM_HOOK(49, sub_82319F38)
CAM_HOOK(50, sub_8231A0E8)
CAM_HOOK(51, sub_8231A298)
CAM_HOOK(52, sub_8231A418)
CAM_HOOK(53, sub_8231A4F0)
CAM_HOOK(54, sub_8231A5D8)
CAM_HOOK(55, sub_8231A658)
CAM_HOOK(56, sub_8231A708)
CAM_HOOK(57, sub_8231A828)
CAM_HOOK(58, sub_8231A9A8)
CAM_HOOK(59, sub_8231A9E0)
CAM_HOOK(60, sub_8231AA80)
CAM_HOOK(61, sub_8231AB28)
CAM_HOOK(62, sub_8231AB88)
CAM_HOOK(63, sub_8231AC10)
CAM_HOOK(64, sub_8231AC60)
CAM_HOOK(65, sub_8231ACF0)
CAM_HOOK(66, sub_8231B510)
CAM_HOOK(67, sub_8231B5E8)
CAM_HOOK(68, sub_8231B8A0)
CAM_HOOK(69, sub_8231B9A0)
CAM_HOOK(70, sub_8231BEC8)
CAM_HOOK(71, sub_8231C048)
CAM_HOOK(72, sub_8231C120)
CAM_HOOK(73, sub_8231C220)
CAM_HOOK(74, sub_8231C2A0)
CAM_HOOK(75, sub_8231C510)
CAM_HOOK(76, sub_8231C568)
CAM_HOOK(77, sub_8231C6F0)
CAM_HOOK(78, sub_8231C750)
CAM_HOOK(79, sub_8231C828)
CAM_HOOK(80, sub_8231C888)
CAM_HOOK(81, sub_8231C8E8)
CAM_HOOK(82, sub_8231C938)
CAM_HOOK(83, sub_8231C9D0)
CAM_HOOK(84, sub_8231CA58)
CAM_HOOK(85, sub_8231CA90)
CAM_HOOK(86, sub_8231CB58)
CAM_HOOK(87, sub_8231CBC0)
CAM_HOOK(88, sub_8231CC98)
CAM_HOOK(89, sub_8231CD18)
CAM_HOOK(90, sub_8231CD70)
CAM_HOOK(91, sub_8231D160)
CAM_HOOK(92, sub_8231D1B0)
CAM_HOOK(93, sub_8231D288)
CAM_HOOK(94, sub_8231D2E0)
CAM_HOOK(95, sub_8231D458)
CAM_HOOK(96, sub_8231D4C8)
CAM_HOOK(97, sub_8231D688)
CAM_HOOK(98, sub_8231D708)
CAM_HOOK(99, sub_8231DAB8)
CAM_HOOK(100, sub_8231DB70)
CAM_HOOK(101, sub_8231DCF0)
CAM_HOOK(102, sub_8231DDF0)
CAM_HOOK(103, sub_8231DE48)
CAM_HOOK(104, sub_8231E020)
CAM_HOOK(105, sub_8231E090)
CAM_HOOK(106, sub_8231E0E0)
CAM_HOOK(107, sub_8231E160)
CAM_HOOK(108, sub_8231E258)
CAM_HOOK(109, sub_8231E328)
CAM_HOOK(110, sub_8231E378)
CAM_HOOK(111, sub_8231E3C0)
CAM_HOOK(112, sub_8231E448)
CAM_HOOK(113, sub_8231E520)
CAM_HOOK(114, sub_8231E650)
CAM_HOOK(115, sub_8231E660)
CAM_HOOK(116, sub_8231E680)
CAM_HOOK(117, sub_8231E690)
CAM_HOOK(118, sub_8231E758)
CAM_HOOK(119, sub_8231E768)
CAM_HOOK(120, sub_8231E8E8)
CAM_HOOK(121, sub_8231E950)
CAM_HOOK(122, sub_8231E970)
CAM_HOOK(123, sub_8231E9F0)
CAM_HOOK(124, sub_8231EA38)
CAM_HOOK(125, sub_8231EA90)
CAM_HOOK(126, sub_8231EAF0)
CAM_HOOK(127, sub_8231EB60)
CAM_HOOK(128, sub_8231EBA8)
CAM_HOOK(129, sub_8231EBE0)
CAM_HOOK(130, sub_8231EC00)
CAM_HOOK(131, sub_8231EC88)
CAM_HOOK(132, sub_8231ED00)
CAM_HOOK(133, sub_8231ED50)
CAM_HOOK(134, sub_8231EDE8)
CAM_HOOK(135, sub_8231EEC0)
CAM_HOOK(136, sub_8231EF58)
CAM_HOOK(137, sub_8231EFD8)
#endif
