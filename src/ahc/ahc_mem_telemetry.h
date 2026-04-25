#ifndef AHC_MEM_TELEMETRY_H
#define AHC_MEM_TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool ahc_mem_telemetry_init(void);
bool ahc_mem_telemetry_working_set_bytes(uint64_t *out_bytes);
bool ahc_mem_telemetry_peak_working_set_bytes(uint64_t *out_bytes);
void ahc_mem_telemetry_reset_peak(void);

#endif
