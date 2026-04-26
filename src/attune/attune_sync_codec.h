#ifndef AHC_ATTUNE_SYNC_CODEC_H
#define AHC_ATTUNE_SYNC_CODEC_H

#include "attune/attune_snapshot.h"

#include <stddef.h>

#define AHC_SYNC_BULK_PREFIX "AHC1:"
#define AHC_SYNC_QR_PREFIX "AHC-Q1:"
#define AHC_SYNC_MULTI_QR_PREFIX "AHC-Q2:"
#define AHC_SYNC_MULTI_QR_MAX_DAYS 30

int ahc_sync_encode_full_history(const AhcDailyAttuneSnapshot *snapshots, size_t count, char *out, size_t out_cap);

int ahc_sync_encode_one_day_qr(const AhcDailyAttuneSnapshot *s, char *out, size_t out_cap);

int ahc_sync_encode_multi_day_qr(const AhcDailyAttuneSnapshot *snapshots, size_t count, char *out, size_t out_cap);

#endif
