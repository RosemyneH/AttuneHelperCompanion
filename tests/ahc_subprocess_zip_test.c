#include "ahc_process.h"

#include <stdio.h>

#ifndef AHC_TEST_ZIP_FIXTURE_DIR
#define AHC_TEST_ZIP_FIXTURE_DIR "."
#endif

int main(void)
{
    char path[1024];
    (void)snprintf(path, sizeof path, "%s/zip_benign.zip", AHC_TEST_ZIP_FIXTURE_DIR);
    if (!ahc_preflight_zip_safe(path)) {
        fprintf(stderr, "FAIL: expected benign zip to pass preflight: %s\n", path);
        return 1;
    }
    (void)snprintf(path, sizeof path, "%s/zip_traverse.zip", AHC_TEST_ZIP_FIXTURE_DIR);
    if (ahc_preflight_zip_safe(path)) {
        fprintf(stderr, "FAIL: expected path-traversal zip to fail preflight: %s\n", path);
        return 1;
    }
    return 0;
}
