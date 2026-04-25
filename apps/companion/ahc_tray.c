#include "ahc_tray.h"

#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#define AHC_TRAYMSG (WM_USER + 100)

static WNDPROC s_old_wnd;
static NOTIFYICONDATAA s_nid;
static int s_tray_added;
static volatile LONG s_pending;

static void ahc_tray_or_pending(LONG bits)
{
    LONG old;
    do {
        old = s_pending;
    } while (InterlockedCompareExchange(&s_pending, old | bits, old) != old);
}

LRESULT CALLBACK ahc_tray_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_SYSCOMMAND) {
        if ((wparam & 0xfff0) == SC_CLOSE) {
            ahc_tray_or_pending((LONG)AHC_TRAYA_BACKGROUND);
            return 0;
        }
    }

    if (msg == WM_CLOSE) {
        ahc_tray_or_pending((LONG)AHC_TRAYA_BACKGROUND);
        return 0;
    }

    if (msg == AHC_TRAYMSG) {
        if (lparam == WM_LBUTTONUP || lparam == WM_LBUTTONDBLCLK) {
            ahc_tray_or_pending((LONG)AHC_TRAYA_SHOW);
        } else if (lparam == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            if (menu) {
                AppendMenuA(menu, MF_STRING, 1, "Open");
                AppendMenuA(menu, MF_STRING, 2, "Exit");
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, hwnd, 0);
                DestroyMenu(menu);
                PostMessageA(hwnd, WM_NULL, 0, 0);
                if (cmd == 1) {
                    ahc_tray_or_pending((LONG)AHC_TRAYA_SHOW);
                } else if (cmd == 2) {
                    ahc_tray_or_pending((LONG)AHC_TRAYA_EXIT);
                }
            }
        }
        return 0;
    }

    if (s_old_wnd) {
        return CallWindowProcA(s_old_wnd, hwnd, msg, wparam, lparam);
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

void ahc_tray_install(void *raylib_hwnd)
{
    HWND w = (HWND)raylib_hwnd;
    if (!w) {
        return;
    }
    s_old_wnd = (WNDPROC)SetWindowLongPtrA(w, GWLP_WNDPROC, (LONG_PTR)ahc_tray_wndproc);

    ZeroMemory(&s_nid, sizeof(s_nid));
    s_nid.cbSize = sizeof(s_nid);
    s_nid.hWnd = w;
    s_nid.uID = 1;
    s_nid.uFlags = (UINT)(NIF_MESSAGE | NIF_TIP);
    s_nid.uCallbackMessage = AHC_TRAYMSG;
#if defined(_MSC_VER)
    strncpy_s(
        s_nid.szTip, sizeof(s_nid.szTip), "Attune Helper (snapshot sync continues)", _TRUNCATE);
#else
    strncpy(s_nid.szTip, "Attune Helper (snapshot sync continues)", sizeof(s_nid.szTip) - 1);
    s_nid.szTip[sizeof(s_nid.szTip) - 1] = '\0';
#endif
    s_nid.hIcon = LoadIconA(0, IDI_APPLICATION);
    s_nid.uFlags = (UINT)(s_nid.uFlags | NIF_ICON);

    s_tray_added = (Shell_NotifyIconA(NIM_ADD, &s_nid) != 0) ? 1 : 0;
    s_pending = 0;
}

void ahc_tray_shutdown(void *raylib_hwnd)
{
    HWND w = (HWND)raylib_hwnd;
    if (s_tray_added) {
        Shell_NotifyIconA(NIM_DELETE, &s_nid);
        s_tray_added = 0;
    }
    if (w && s_old_wnd) {
        SetWindowLongPtrA(w, GWLP_WNDPROC, (LONG_PTR)s_old_wnd);
    }
    s_old_wnd = 0;
    ZeroMemory(&s_nid, sizeof(s_nid));
}

uint32_t ahc_tray_consume_actions(void)
{
    return (uint32_t)InterlockedExchange(&s_pending, 0);
}

void ahc_tray_request_background(void)
{
    ahc_tray_or_pending((LONG)AHC_TRAYA_BACKGROUND);
}

#else

static uint32_t s_nw_pending;

void ahc_tray_install(void *raylib_hwnd) { (void)raylib_hwnd; s_nw_pending = 0; }
void ahc_tray_shutdown(void *raylib_hwnd) { (void)raylib_hwnd; }
uint32_t ahc_tray_consume_actions(void)
{
    uint32_t t = s_nw_pending;
    s_nw_pending = 0;
    return t;
}
void ahc_tray_request_background(void) { s_nw_pending |= AHC_TRAYA_BACKGROUND; }

#endif
