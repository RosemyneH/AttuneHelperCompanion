#ifndef AHC_ADDON_PROFILE_CODEC_H
#define AHC_ADDON_PROFILE_CODEC_H

#include <stddef.h>

#define AHC_PROFILE_CODE_PREFIX "AHC-P1:"
#define AHC_PROFILE_MAX_ADDONS 512
#define AHC_PROFILE_CODE_CAPACITY (128U * 1024U)
#define AHC_PROFILE_ID_CAPACITY 128
#define AHC_PROFILE_NAME_CAPACITY 128

typedef struct AhcAddonProfile {
    char name[AHC_PROFILE_NAME_CAPACITY];
    char addon_ids[AHC_PROFILE_MAX_ADDONS][AHC_PROFILE_ID_CAPACITY];
    size_t addon_count;
} AhcAddonProfile;

int ahc_profile_encode(const AhcAddonProfile *profile, char *out, size_t out_cap);
int ahc_profile_decode(const char *code, AhcAddonProfile *out);

#endif
