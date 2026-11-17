// F3 (diagnostics): the game's memory to frames\memdump_<n>.bin, taken at the
// next VdSwap (between frames, from the game's own thread). The file is a
// list of [guest address (u32 LE), size (u32 LE), bytes]: the 512 MB of
// physical memory (as its 0xA0000000 view), then every committed range of the
// virtual and system heaps (0x10000000-0x7F000000).
#include "log.h"
#include "memory.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wet
{
    namespace
    {
        std::atomic<int> s_requests{ 0 };

        void Put(std::FILE* f, uint32_t guest, const uint8_t* bytes, uint32_t size)
        {
            std::fwrite(&guest, 4, 1, f);
            std::fwrite(&size, 4, 1, f);
            std::fwrite(bytes, 1, size, f);
        }
    }

    void RequestMemoryDump() { s_requests.fetch_add(1, std::memory_order_relaxed); }

    // Called once a frame (VdSwap).
    void MemoryDumpPoll()
    {
        if (s_requests.load(std::memory_order_relaxed) == 0)
            return;
        s_requests.store(0, std::memory_order_relaxed);
        static int s_count = 0;
        std::error_code ec;
        std::filesystem::create_directories("frames", ec);
        char path[64];
        std::snprintf(path, sizeof(path), "frames/memdump_%d.bin", ++s_count);
        std::FILE* f = std::fopen(path, "wb");
        if (!f)
        {
            Log("Memory dump: can't write %s", path);
            return;
        }
        uint64_t total = 512ull << 20;
        Put(f, 0xA0000000u, PhysicalHostView(), 512u << 20);
        int ranges = 1;
#ifdef _WIN32
        uint8_t* base = g_mem.base;
        uint64_t a = 0x10000000ull;
        while (a < 0x7F000000ull)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (!VirtualQuery(base + a, &mbi, sizeof(mbi)))
                break;
            uint64_t start = uint64_t(static_cast<uint8_t*>(mbi.BaseAddress) - base);
            uint64_t end = start + mbi.RegionSize;
            if (end > 0x7F000000ull)
                end = 0x7F000000ull;
            if (start < a)
                start = a;
            bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
            if (readable && end > start)
            {
                Put(f, uint32_t(start), base + start, uint32_t(end - start));
                total += end - start;
                ranges++;
            }
            if (end <= a)
                break;
            a = end;
        }
#endif
        std::fclose(f);
        Log("Memory dump: %s (%d ranges, %llu MB)", path, ranges, (unsigned long long)(total >> 20));
    }
}
