#include "addons/addon_manifest.h"
#include "ahc/ahc_mem_telemetry.h"
#include "attune/attune_snapshot.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

static uint64_t qpc_now(void)
{
#if defined(_WIN32)
    LARGE_INTEGER c;
    if (!QueryPerformanceCounter(&c)) {
        return 0u;
    }
    return (uint64_t)c.QuadPart;
#else
    return 0u;
#endif
}

static uint64_t qpc_freq(void)
{
#if defined(_WIN32)
    LARGE_INTEGER f;
    if (!QueryPerformanceFrequency(&f) || f.QuadPart == 0) {
        return 1u;
    }
    return (uint64_t)f.QuadPart;
#else
    return 1u;
#endif
}

static int print_usage(const char *argv0)
{
    fprintf(
        stderr,
        "Usage: %s <manifest.json> <snapshot_file.lua> <iterations>\n",
        argv0
    );
    return 2;
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        return print_usage(argv[0]);
    }
    const char *path_manifest = argv[1];
    const char *path_snapshot = argv[2];
    int iter = (int)strtol(argv[3], NULL, 10);
    if (iter < 1) {
        return print_usage(argv[0]);
    }
    ahc_mem_telemetry_init();
    ahc_mem_telemetry_reset_peak();

    uint64_t t0p = qpc_now();
    for (int i = 0; i < iter; i++) {
        AhcAddonManifest m;
        memset(&m, 0, sizeof m);
        if (!ahc_addon_manifest_load_file(path_manifest, &m)) {
            fprintf(
                stderr,
                "ahc_addon_manifest_load_file failed: %s\n",
                path_manifest
            );
            return 1;
        }
        ahc_addon_manifest_free(&m);
    }
    uint64_t t1p = qpc_now();

    uint64_t t0s = qpc_now();
    for (int j = 0; j < iter; j++) {
        AhcDailyAttuneSnapshot sn;
        if (!ahc_parse_daily_attune_snapshot_file(path_snapshot, &sn)) {
            fprintf(
                stderr,
                "ahc_parse_daily_attune_snapshot_file failed: %s\n",
                path_snapshot
            );
            return 1;
        }
        (void)sn;
    }
    uint64_t t1s = qpc_now();

    AhcAddonManifest m;
    if (!ahc_addon_manifest_load_file(path_manifest, &m)) {
        fprintf(
            stderr,
            "ahc_addon_manifest_load_file (metrics) failed: %s\n",
            path_manifest
        );
        return 1;
    }
    const size_t arena_b = ahc_addon_manifest_storage_bytes(&m);
    ahc_addon_manifest_free(&m);

    uint64_t f = qpc_freq();
    double s_manifest
        = (t1p > t0p) ? (double)(t1p - t0p) * 1000.0 / (double)(f) : 0.0;
    double s_snap
        = (t1s > t0s) ? (double)(t1s - t0s) * 1000.0 / (double)(f) : 0.0;

    uint64_t wss = 0;
    uint64_t peak = 0;
    ahc_mem_telemetry_working_set_bytes(&wss);
    ahc_mem_telemetry_peak_working_set_bytes(&peak);

    printf("iterations=%d\n", iter);
    printf("manifest total_ms=%.3f (per call %.6f)\n", s_manifest, s_manifest / (double)iter);
    printf("snapshot total_ms=%.3f (per call %.6f)\n", s_snap, s_snap / (double)iter);
    printf("manifest_arena_bytes_last_load=%" PRIu64 " (approx)\n", (uint64_t)arena_b);
    printf("working_set_bytes (best effort)=%" PRIu64 "\n", wss);
    printf("os_peak_working_set_bytes (process)=%" PRIu64 "\n", peak);
    return 0;
}
