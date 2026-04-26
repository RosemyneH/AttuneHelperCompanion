#include "ahc_credentials.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define AHC_CRED_PATH_MAX 768

static void cred_path(const char *config_dir, char *out, size_t out_n)
{
    if (config_dir && config_dir[0]) {
#if defined(_WIN32)
        (void)snprintf(out, out_n, "%s\\wow_autologin.cred", config_dir);
#else
        (void)snprintf(out, out_n, "%s/wow_autologin.cred", config_dir);
#endif
    } else {
        out[0] = '\0';
    }
}

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")

bool ahc_credential_store_wow_password(const char *config_dir, const char *utf8_password)
{
    if (!config_dir || !config_dir[0] || !utf8_password) {
        return false;
    }
    char path[AHC_CRED_PATH_MAX];
    cred_path(config_dir, path, sizeof(path));
    DATA_BLOB in;
    DATA_BLOB outb;
    in.pbData = (BYTE *)(utf8_password);
    in.cbData = (DWORD)strlen(utf8_password) + 1u;
    if (!CryptProtectData(&in, L"AttuneHelperWow", NULL, NULL, NULL, 0, &outb)) {
        return false;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        LocalFree(outb.pbData);
        return false;
    }
    size_t w = fwrite(outb.pbData, 1, outb.cbData, f);
    LocalFree(outb.pbData);
    fclose(f);
    return w == outb.cbData;
}

bool ahc_credential_load_wow_password(const char *config_dir, char *out, size_t out_size)
{
    if (!config_dir || !config_dir[0] || !out || out_size < 2u) {
        return false;
    }
    char path[AHC_CRED_PATH_MAX];
    cred_path(config_dir, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (long)(1 << 20)) {
        fclose(f);
        return false;
    }
    BYTE *enc = (BYTE *)malloc((size_t)sz);
    if (!enc) {
        fclose(f);
        return false;
    }
    if (fread(enc, 1, (size_t)sz, f) != (size_t)sz) {
        free(enc);
        fclose(f);
        return false;
    }
    fclose(f);
    DATA_BLOB in2;
    DATA_BLOB out2;
    in2.pbData = enc;
    in2.cbData = (DWORD)sz;
    if (!CryptUnprotectData(&in2, NULL, NULL, NULL, NULL, 0, &out2)) {
        free(enc);
        return false;
    }
    free(enc);
    if (out2.cbData + 1u > out_size) {
        LocalFree(out2.pbData);
        return false;
    }
    memcpy(out, out2.pbData, out2.cbData);
    out[out2.cbData] = '\0';
    LocalFree(out2.pbData);
    return true;
}

void ahc_credential_clear_wow_password(const char *config_dir)
{
    char path[AHC_CRED_PATH_MAX];
    cred_path(config_dir, path, sizeof(path));
    (void)remove(path);
}

bool ahc_credential_wow_password_exists(const char *config_dir)
{
    if (!config_dir || !config_dir[0]) {
        return false;
    }
    char path[AHC_CRED_PATH_MAX];
    cred_path(config_dir, path, sizeof(path));
    if (!path[0]) {
        return false;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

#elif defined(__ANDROID__)

bool ahc_credential_store_wow_password(const char *config_dir, const char *utf8_password)
{
    (void)config_dir;
    (void)utf8_password;
    return false;
}

bool ahc_credential_load_wow_password(const char *config_dir, char *out, size_t out_size)
{
    (void)config_dir;
    (void)out;
    (void)out_size;
    return false;
}

void ahc_credential_clear_wow_password(const char *config_dir)
{
    (void)config_dir;
}

bool ahc_credential_wow_password_exists(const char *config_dir)
{
    (void)config_dir;
    return false;
}

#else

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

static bool w_file(const char *path, const void *data, size_t len, mode_t mode)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    size_t w = fwrite(data, 1, len, f);
    fclose(f);
    if (w != len) {
        (void)remove(path);
        return false;
    }
    if (chmod(path, mode) != 0) {
        return false;
    }
    return true;
}

bool ahc_credential_store_wow_password(const char *config_dir, const char *utf8_password)
{
    if (!config_dir || !config_dir[0] || !utf8_password) {
        return false;
    }
    char path[AHC_CRED_PATH_MAX];
    cred_path(config_dir, path, sizeof(path));
    return w_file(
        path,
        utf8_password,
        strlen(utf8_password) + 1u,
        (mode_t)0600
    );
}

bool ahc_credential_load_wow_password(const char *config_dir, char *out, size_t out_size)
{
    if (!config_dir || !config_dir[0] || !out || out_size < 2u) {
        return false;
    }
    char path[AHC_CRED_PATH_MAX];
    cred_path(config_dir, path, sizeof(path));
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    (void)st;
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    size_t n = fread(out, 1, out_size - 1, f);
    fclose(f);
    if (n == 0) {
        return false;
    }
    out[n] = '\0';
    return out[0] != '\0';
}

void ahc_credential_clear_wow_password(const char *config_dir)
{
    char path[AHC_CRED_PATH_MAX];
    cred_path(config_dir, path, sizeof(path));
    (void)remove(path);
}

bool ahc_credential_wow_password_exists(const char *config_dir)
{
    if (!config_dir || !config_dir[0]) {
        return false;
    }
    char path[AHC_CRED_PATH_MAX];
    cred_path(config_dir, path, sizeof(path));
    if (!path[0]) {
        return false;
    }
    return access(path, F_OK) == 0;
}

#endif
