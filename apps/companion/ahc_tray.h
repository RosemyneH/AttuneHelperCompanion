#ifndef AHC_TRAY_H
#define AHC_TRAY_H

#include <stdint.h>

#define AHC_TRAYA_SHOW 1u
#define AHC_TRAYA_EXIT 2u
#define AHC_TRAYA_BACKGROUND 4u

void ahc_tray_install(void *raylib_hwnd);
void ahc_tray_shutdown(void *raylib_hwnd);
uint32_t ahc_tray_consume_actions(void);
void ahc_tray_request_background(void);

#endif
