// WET_NAN_TRACE=1 (diagnostics): the recompiler wraps every floating-point
// instruction (vector add/sub/mul/madd/dot/min/max/estimates, scalar
// add/sub/mul/div/madd/sqrt/estimates); with this on, one that makes a NaN out
// of NaN-free inputs calls WetNanHitV / WetNanHitF with its guest address.
// Each address is logged once ("NaN made at"), with the time, the result and
// the inputs, so the first NaN of a chain (the last Champion Mode fight's
// fighters and ropes end up as NaNs) shows which instruction made it.
// Vector lanes are printed as stored on the host (guest element 3 first).
#include "log.h"
#include "os.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

int g_wetNanTrace = [] {
    const char* v = std::getenv("WET_NAN_TRACE");
    return v && v[0] == '1' ? 1 : 0;
}();

namespace
{
    std::mutex s_lock;
    std::unordered_map<uint32_t, uint32_t> s_sites;   // address -> times seen
    uint64_t s_startNs = 0;

    // True the first time an address reports.
    bool FirstTime(uint32_t address, double& seconds, uint32_t& count)
    {
        std::lock_guard g(s_lock);
        if (!s_startNs)
            s_startNs = wet::os::MonotonicNs();
        seconds = double(wet::os::MonotonicNs() - s_startNs) / 1e9;
        uint32_t& n = s_sites[address];
        n++;
        count = uint32_t(s_sites.size());
        return n == 1;
    }

    uint32_t Bits(float f)
    {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        return u;
    }
}

void WetNanHitV(uint32_t address, const float* out, const float (*in)[4], int inputs)
{
    double t;
    uint32_t sites;
    if (!FirstTime(address, t, sites) || sites > 5000)
        return;
    char text[600];
    int len = std::snprintf(text, sizeof(text), "NaN made at %08X (vector, %.3f s after the first): out %08X %08X %08X %08X",
                            address, t, Bits(out[0]), Bits(out[1]), Bits(out[2]), Bits(out[3]));
    for (int k = 0; k < inputs && len < int(sizeof(text)) - 120; k++)
        len += std::snprintf(text + len, sizeof(text) - len, " | in%d %g %g %g %g", k, in[k][0], in[k][1], in[k][2], in[k][3]);
    wet::Log("%s", text);
    if (sites == 5000)
        wet::Log("NaN trace: 5000 places logged, no more");
}

void WetNanHitF(uint32_t address, double out, const double* in, int inputs)
{
    double t;
    uint32_t sites;
    if (!FirstTime(address, t, sites) || sites > 5000)
        return;
    char text[400];
    int len = std::snprintf(text, sizeof(text), "NaN made at %08X (scalar, %.3f s after the first): out %g", address, t, out);
    for (int k = 0; k < inputs && len < int(sizeof(text)) - 60; k++)
        len += std::snprintf(text + len, sizeof(text) - len, " | in%d %.9g", k, in[k]);
    wet::Log("%s", text);
}
