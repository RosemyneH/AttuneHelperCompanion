#include "addons/addon_profile_codec.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    AhcAddonProfile profile;
    memset(&profile, 0, sizeof(profile));
    snprintf(profile.name, sizeof(profile.name), "%s", "Starter Map Pack");
    snprintf(profile.addon_ids[0], sizeof(profile.addon_ids[0]), "%s", "attune-helper");
    snprintf(profile.addon_ids[1], sizeof(profile.addon_ids[1]), "%s", "mapster-synastria-hosting");
    profile.addon_count = 2u;

    char code[4096];
    int n = ahc_profile_encode(&profile, code, sizeof(code));
    if (n <= 0 || strncmp(code, AHC_PROFILE_CODE_PREFIX, sizeof(AHC_PROFILE_CODE_PREFIX) - 1u) != 0) {
        fprintf(stderr, "profile encode failed\n");
        return 1;
    }

    AhcAddonProfile decoded;
    memset(&decoded, 0, sizeof(decoded));
    if (ahc_profile_decode(code, &decoded) != 0) {
        fprintf(stderr, "profile decode failed\n");
        return 1;
    }
    if (strcmp(decoded.name, profile.name) != 0 || decoded.addon_count != profile.addon_count) {
        fprintf(stderr, "profile header mismatch\n");
        return 1;
    }
    for (size_t i = 0; i < profile.addon_count; i++) {
        if (strcmp(decoded.addon_ids[i], profile.addon_ids[i]) != 0) {
            fprintf(stderr, "profile id mismatch at %zu\n", i);
            return 1;
        }
    }
    if (ahc_profile_decode("AHC1:not-profile", &decoded) == 0) {
        fprintf(stderr, "accepted wrong prefix\n");
        return 1;
    }

    memset(&profile, 0, sizeof(profile));
    snprintf(profile.name, sizeof(profile.name), "%s", "Large Add-on Profile");
    profile.addon_count = 128u;
    for (size_t i = 0; i < profile.addon_count; i++) {
        snprintf(profile.addon_ids[i], sizeof(profile.addon_ids[i]), "manual-addon-folder-%03zu", i);
    }
    char large_code[AHC_PROFILE_CODE_CAPACITY];
    n = ahc_profile_encode(&profile, large_code, sizeof(large_code));
    if (n <= 0) {
        fprintf(stderr, "large profile encode failed\n");
        return 1;
    }
    memset(&decoded, 0, sizeof(decoded));
    if (ahc_profile_decode(large_code, &decoded) != 0 || decoded.addon_count != profile.addon_count) {
        fprintf(stderr, "large profile decode mismatch\n");
        return 1;
    }
    for (size_t i = 0; i < profile.addon_count; i++) {
        if (strcmp(decoded.addon_ids[i], profile.addon_ids[i]) != 0) {
            fprintf(stderr, "large profile id mismatch at %zu\n", i);
            return 1;
        }
    }
    return 0;
}
