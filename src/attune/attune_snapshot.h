#ifndef AHC_ATTUNE_SNAPSHOT_H
#define AHC_ATTUNE_SNAPSHOT_H

#include <stdbool.h>

#define AHC_DATE_CAPACITY 16

typedef struct AhcDailyAttuneSnapshot {
    bool found;
    char date[AHC_DATE_CAPACITY];
    int account;
    int warforged;
    int lightforged;
    int titanforged;
} AhcDailyAttuneSnapshot;

bool ahc_parse_daily_attune_snapshot(const char *text, AhcDailyAttuneSnapshot *out);
bool ahc_parse_daily_attune_snapshot_file(const char *path, AhcDailyAttuneSnapshot *out);

#endif
