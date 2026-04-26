#ifndef AHC_CREDENTIALS_H
#define AHC_CREDENTIALS_H

#include <stdbool.h>
#include <stddef.h>

#define AHC_CREDENTIAL_PASSWORD_MAX 300

/**
 * Windows: DPAPI blob. Linux: 0600 file. Android: JNI (stub in plain C for now).
 */
bool ahc_credential_store_wow_password(const char *config_dir, const char *utf8_password);
bool ahc_credential_load_wow_password(const char *config_dir, char *out, size_t out_size);
bool ahc_credential_wow_password_exists(const char *config_dir);
void ahc_credential_clear_wow_password(const char *config_dir);

#endif
