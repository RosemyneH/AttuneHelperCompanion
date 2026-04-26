#include "ahc_tray.h"

#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#define AHC_TRAYMSG (WM_APP + 100)
#define AHC_TRAY_CLASS "AttuneHelperCompanionTray"
#define AHC_APP_ICON_ID 1

static WNDPROC s_old_wnd;
static HWND s_tray_hwnd;
static NOTIFYICONDATAA s_nid;
static HICON s_icon_big;
static HICON s_icon_small;
static int s_destroy_icon_big;
static int s_destroy_icon_small;
static int s_tray_added;
static int s_class_registered;
static volatile LONG s_pending;

static void ahc_tray_or_pending(LONG bits)
{
    LONG old;
    do {
        old = s_pending;
    } while (InterlockedCompareExchange(&s_pending, old | bits, old) != old);
}

static void ahc_tray_show_menu(HWND owner)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuA(menu, MF_STRING, 1, "Open");
    AppendMenuA(menu, MF_STRING, 2, "Hide to tray");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, 3, "Close");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(owner);
    int cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, owner, 0);
    DestroyMenu(menu);
    PostMessageA(owner, WM_NULL, 0, 0);
    if (cmd == 1) {
        ahc_tray_or_pending((LONG)AHC_TRAYA_SHOW);
    } else if (cmd == 2) {
        ahc_tray_or_pending((LONG)AHC_TRAYA_BACKGROUND);
    } else if (cmd == 3) {
        ahc_tray_or_pending((LONG)AHC_TRAYA_EXIT);
    }
}

static LRESULT CALLBACK ahc_tray_owner_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    (void)wparam;
    if (msg == AHC_TRAYMSG) {
        if (lparam == WM_LBUTTONUP || lparam == WM_LBUTTONDBLCLK) {
            ahc_tray_or_pending((LONG)AHC_TRAYA_SHOW);
        } else if (lparam == WM_RBUTTONUP) {
            ahc_tray_show_menu(hwnd);
        }
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static LRESULT CALLBACK ahc_raylib_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_SYSCOMMAND) {
        if ((wparam & 0xfff0) == SC_CLOSE) {
            ahc_tray_or_pending((LONG)AHC_TRAYA_BACKGROUND);
            return 0;
        }
        if ((wparam & 0xfff0) == SC_MINIMIZE) {
            ahc_tray_or_pending((LONG)AHC_TRAYA_BACKGROUND);
            return 0;
        }
    }

    if (msg == WM_CLOSE) {
        ahc_tray_or_pending((LONG)AHC_TRAYA_BACKGROUND);
        return 0;
    }
    if (msg == WM_SIZE && wparam == SIZE_MINIMIZED) {
        ahc_tray_or_pending((LONG)AHC_TRAYA_BACKGROUND);
        return 0;
    }

    if (s_old_wnd) {
        return CallWindowProcA(s_old_wnd, hwnd, msg, wparam, lparam);
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static HWND ahc_tray_create_owner_window(void)
{
    HINSTANCE instance = GetModuleHandleA(NULL);
    if (!s_class_registered) {
        WNDCLASSA wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = ahc_tray_owner_wndproc;
        wc.hInstance = instance;
        wc.lpszClassName = AHC_TRAY_CLASS;
        if (!RegisterClassA(&wc)) {
            return NULL;
        }
        s_class_registered = 1;
    }

    return CreateWindowExA(
        0,
        AHC_TRAY_CLASS,
        "Attune Helper Companion Tray",
        0,
        0,
        0,
        0,
        0,
        NULL,
        NULL,
        instance,
        NULL);
}

static HICON ahc_load_app_icon(int width, int height, int *destroy_icon)
{
    HICON icon = NULL;
    HINSTANCE instance = GetModuleHandleA(NULL);
    if (destroy_icon) {
        *destroy_icon = 0;
    }
    icon = (HICON)LoadImageA(
        instance,
        MAKEINTRESOURCEA(AHC_APP_ICON_ID),
        IMAGE_ICON,
        width,
        height,
        LR_DEFAULTCOLOR);
    if (icon) {
        if (destroy_icon) {
            *destroy_icon = 1;
        }
        return icon;
    }
    icon = LoadIconA(instance, MAKEINTRESOURCEA(AHC_APP_ICON_ID));
    if (icon) {
        return icon;
    }
    return LoadIconA(NULL, IDI_APPLICATION);
}

void ahc_tray_install(void *raylib_hwnd)
{
    HWND w = (HWND)raylib_hwnd;
    if (!w) {
        return;
    }
    if (!s_old_wnd) {
        s_old_wnd = (WNDPROC)SetWindowLongPtrA(w, GWLP_WNDPROC, (LONG_PTR)ahc_raylib_wndproc);
    }
    if (!s_tray_hwnd) {
        s_tray_hwnd = ahc_tray_create_owner_window();
    }
    if (!s_icon_big) {
        s_icon_big = ahc_load_app_icon(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), &s_destroy_icon_big);
    }
    if (!s_icon_small) {
        s_icon_small =
            ahc_load_app_icon(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), &s_destroy_icon_small);
    }
    if (s_icon_big) {
        SendMessageA(w, WM_SETICON, ICON_BIG, (LPARAM)s_icon_big);
        SetClassLongPtrA(w, GCLP_HICON, (LONG_PTR)s_icon_big);
    }
    if (s_icon_small) {
        SendMessageA(w, WM_SETICON, ICON_SMALL, (LPARAM)s_icon_small);
        SetClassLongPtrA(w, GCLP_HICONSM, (LONG_PTR)s_icon_small);
    }

    ZeroMemory(&s_nid, sizeof(s_nid));
    s_nid.cbSize = sizeof(s_nid);
    s_nid.hWnd = s_tray_hwnd ? s_tray_hwnd : w;
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
    s_nid.hIcon = s_icon_small ? s_icon_small : (s_icon_big ? s_icon_big : LoadIconA(0, IDI_APPLICATION));
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
    if (s_tray_hwnd) {
        DestroyWindow(s_tray_hwnd);
        s_tray_hwnd = NULL;
    }
    if (s_class_registered) {
        UnregisterClassA(AHC_TRAY_CLASS, GetModuleHandleA(NULL));
        s_class_registered = 0;
    }
    if (s_destroy_icon_big && s_icon_big) {
        DestroyIcon(s_icon_big);
    }
    if (s_destroy_icon_small && s_icon_small) {
        DestroyIcon(s_icon_small);
    }
    s_icon_big = NULL;
    s_icon_small = NULL;
    s_destroy_icon_big = 0;
    s_destroy_icon_small = 0;
    ZeroMemory(&s_nid, sizeof(s_nid));
}

uint32_t ahc_tray_consume_actions(void)
{
    if (s_tray_hwnd) {
        MSG msg;
        while (PeekMessageA(&msg, s_tray_hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
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
