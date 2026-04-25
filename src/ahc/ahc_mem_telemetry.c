#include "ahc/ahc_mem_telemetry.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

static bool s_inited;
#if defined(_WIN32)
static HANDLE s_process;
#endif

bool ahc_mem_telemetry_init(void)
{
    if (s_inited) {
        return true;
    }
#if defined(_WIN32)
    s_process = GetCurrentProcess();
#endif
    s_inited = true;
    return true;
}

#if defined(_WIN32)
static bool get_counters(uint64_t *work, uint64_t *peak)
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    memset(&pmc, 0, sizeof(pmc));
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(s_process, (PROCESS_MEMORY_COUNTERS *)&pmc, pmc.cb)) {
        return false;
    }
    if (work) {
        *work = (uint64_t)pmc.WorkingSetSize;
    }
    if (peak) {
        *peak = (uint64_t)pmc.PeakWorkingSetSize;
    }
    return true;
}
#endif

bool ahc_mem_telemetry_working_set_bytes(uint64_t *out_bytes)
{
    if (!out_bytes) {
        return false;
    }
#if defined(_WIN32)
    if (!s_inited) {
        ahc_mem_telemetry_init();
    }
    return get_counters(out_bytes, NULL);
#else
    (void)out_bytes;
    return false;
#endif
}

bool ahc_mem_telemetry_peak_working_set_bytes(uint64_t *out_bytes)
{
    if (!out_bytes) {
        return false;
    }
#if defined(_WIN32)
    if (!s_inited) {
        ahc_mem_telemetry_init();
    }
    return get_counters(NULL, out_bytes);
#else
    (void)out_bytes;
    return false;
#endif
}

void ahc_mem_telemetry_reset_peak(void)
{
#if defined(_WIN32)
    if (s_inited) {
        EmptyWorkingSet(s_process);
    }
#endif
}
