/* =============================================================================
// BORDERless
//
// Hide (and restore) window borders and/or menu bar.
//
// Some (legacy) applications display horrible ugly borders
// in full screen mode on Windows 10 (8? 8.1? 11?). The main purpose
// of this little utility is to toggle these borders off individually
// for each window using a custom mask and restore back, if needed.
// The effect can be applied to regular windows too, but results
// sometimes can be unpredictable, depending on mask used.
//
// As a bonus feature it can also toggle on/off regular
// window menu bars. This is handy for achieving uniform
// look and feel in applications where fixed menu bar
// is annoying and/or undesirable.
//
// https://buymeacoff.ee/ubihazard
// -------------------------------------------------------------------------- */

#ifndef UNICODE
/* Enable Unicode in WinAPI */
#define UNICODE
#endif

#ifndef _UNICODE
/* Enable Unicode in C runtime */
#define _UNICODE
#endif

#ifndef _WIN32_WINNT
/* Enable Windows 7 features */
#define _WIN32_WINNT 0x0601
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <Windows.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <wchar.h>

/* -------------------------------------------------------------------------- */

/* C array */
#define numof(arr) (sizeof(arr) / sizeof(arr[0]))
#define arrnew(type, num) malloc ((num) * sizeof(type))
#define arrsize(arr, num) ((num) * sizeof((arr)[0]))
#define arrnewsize(arr, num) realloc (arr, arrsize (arr, num))
#define arrcopy(dst, src, num) memcpy (dst, src, arrsize (dst, num))
#define arrmove(dst, src, num) memmove (dst, src, arrsize (dst, num))
#define arrzero(arr, num) memset (arr, 0, arrsize (arr, num))
#define objzero(obj) arrzero (obj, 1)

/* C constant string */
#define cstrlen(str) (sizeof(str) / sizeof(str[0]) - 1)
#define cstrniequ(str, cstr) (_wcsnicmp (str, cstr, cstrlen(cstr)) == 0)

/* Additional character tests */
#define iswalphab(c) ((c) >= 'a' && (c) <= 'z')
#define iswdigit19(c) ((c) >= '1' && (c) <= '9')
#define iswdigit09(c) iswdigit (c)

/* BORDERless is DPI-aware! Huh. */
#define DPIX(v) MulDiv (v, dpix, 72)
#define DPIY(v) MulDiv (v, dpiy, 72)

/* -------------------------------------------------------------------------- */

#define MDT_DEFAULT 3
typedef HRESULT WINAPI GetDpiForMonitor_fn (HMONITOR hmonitor, int dpiType, UINT* dpiX, UINT* dpiY);
static GetDpiForMonitor_fn* GetDpiForMonitor;

/* -------------------------------------------------------------------------- */

#define APP_ID L"1710edcb-f650-41c4-9bbb-e1741e68aadd"
#define APP_CLASSNAME L"BORDERLESS"
#define APP_TITLE L"BORDERless"
#define APP_VERSION L"1.0"

static HINSTANCE app_instance;
static HMODULE lib_shcore;
static HANDLE mutex;

static HWND wnd_main;
static HWND cbox_hkey_hide_border;
static HWND edit_hkey_hide_border;
static HWND cbox_hkey_hide_menu;
static HWND edit_hkey_hide_menu;
static HWND cbox_coffee;
static HMENU menu_popup;

static HFONT font_gui;
static WNDPROC edit_wnd_proc;

static int dpix, dpiy;
static int font_size;
static bool first_run;
static bool show_coffee = true;

/* Default masks for hiding borders */
static LONG style_mask = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU
| WS_THICKFRAME;
static LONG style_ex_mask = WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE
| WS_EX_DLGMODALFRAME;

/* -------------------------------------------------------------------------- */

#define parse_mask parse_hex

static inline unsigned long parse_hex (const wchar_t* const str)
{
  wchar_t** end = (wchar_t**)&str;
  return wcstoul (str, end, 16);
}

static inline bool get_key_state (UINT const key)
{
  return GetKeyState (key) >> 8;
}

static inline bool is_maximized (const HWND wnd)
{
  WINDOWPLACEMENT place = {.length = sizeof(place)};
  GetWindowPlacement (wnd, &place);
  return place.showCmd == SW_SHOWMAXIMIZED;
}

static inline bool is_minimized (const HWND wnd)
{
  WINDOWPLACEMENT place = {.length = sizeof(place)};
  GetWindowPlacement (wnd, &place);
  return place.showCmd == SW_SHOWMINIMIZED;
}

static inline bool is_visible (const HWND wnd)
{
  return IsWindowVisible (wnd);
}

static inline void desktop_size (int* const w, int* const h)
{
  RECT r;
  GetWindowRect (GetDesktopWindow(), &r);
  w[0] = r.right - r.left;
  h[0] = r.bottom - r.top;
}

static bool is_windows81 (void)
{
  OSVERSIONINFOW info = {.dwOSVersionInfoSize = sizeof(info)};
  GetVersionExW (&info);
  if (info.dwMajorVersion > 6) return true;
  if (info.dwMajorVersion == 6) return info.dwMinorVersion >= 3;
  return false;
}

static void shell_run (const wchar_t* const cmd)
{
  ShellExecuteW (NULL, L"open", cmd, NULL, NULL, SW_NORMAL);
}

/* -----------------------------------------------------------------------------
// Hide border */

struct border_store_item {
  HWND wnd;
  LONG style;
  LONG style_ex;
};

static size_t border_store_size;
static struct border_store_item* border_store;

static void repaint_window (const HWND wnd)
{
  RECT r;
  if (is_maximized (wnd)) return;
  GetWindowRect (wnd, &r);
  MoveWindow (wnd, r.left, r.top, r.right - r.left - 1, r.bottom - r.top - 1, FALSE);
  MoveWindow (wnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
}

static bool remove_border (const HWND wnd)
{
  if (wnd == wnd_main) return false;

  const LONG style = GetWindowLongW (wnd, GWL_STYLE);
  if (style == 0) return false;
  const LONG style_ex = GetWindowLongW (wnd, GWL_EXSTYLE);
  if (style_ex == 0) return false;

  /* See if border is to be hidden or restored */
  bool hide = true;
  struct border_store_item* runner = border_store;
  while (runner != border_store + border_store_size) {
    if (runner->wnd == wnd) {
      hide = false;
      break;
    }
    ++runner;
  }

  if (hide) {
    void* const newptr = arrnewsize (border_store, border_store_size + 1);
    if (newptr == NULL) return false;
    border_store = newptr;
    border_store[border_store_size++] = (struct border_store_item){
      .wnd = wnd,
      .style = style,
      .style_ex = style_ex
    };

    SetWindowLongW (wnd, GWL_EXSTYLE, style_ex & ~style_ex_mask);
    SetWindowLongW (wnd, GWL_STYLE, style & ~style_mask);
    repaint_window (wnd);
  } else {
    SetWindowLongW (wnd, GWL_STYLE, runner->style);
    SetWindowLongW (wnd, GWL_EXSTYLE, runner->style_ex);
    repaint_window (wnd);

    arrmove (runner, runner + 1, (border_store_size - (runner + 1 - border_store)));
    void* const newptr = arrnewsize (border_store, --border_store_size);
    if (newptr != NULL || border_store_size == 0) border_store = newptr;
  }

  return true;
}

/* -----------------------------------------------------------------------------
// Hide menu */

struct top_level_test {
  const HWND wnd;
  bool is_top_level;
};

static BOOL CALLBACK enum_top_level (HWND const wnd, LPARAM const lparam)
{
  struct top_level_test* const test = (struct top_level_test*)lparam;
  if (test->wnd == wnd) {
    test->is_top_level = true;
    return FALSE;
  }
  return TRUE;
}

struct menu_store_item {
  HWND wnd;
  HMENU menu;
};

static size_t menu_store_size;
static struct menu_store_item* menu_store;

static bool remove_menu (const HWND wnd)
{
  if (wnd == wnd_main) return false;

  /* Find out if the window is a top-level window first */
  struct top_level_test test = {.wnd = wnd};
  EnumWindows (&enum_top_level, (LPARAM)&test);
  if (!test.is_top_level) return false;

  /* See if menu is to be hidden or restored */
  bool hide = true;
  struct menu_store_item* runner = menu_store;
  while (runner != menu_store + menu_store_size) {
    if (runner->wnd == wnd) {
      hide = false;
      break;
    }
    ++runner;
  }

  if (hide) {
    HMENU const menu = GetMenu (wnd);
    if (menu != NULL) {
      void* const newptr = arrnewsize (menu_store, menu_store_size + 1);
      if (newptr == NULL) return false;
      menu_store = newptr;
      menu_store[menu_store_size++] = (struct menu_store_item){
        .wnd = wnd,
        .menu = menu
      };
      SetMenu (wnd, NULL);
    }
  } else {
    SetMenu (wnd, runner->menu);
    arrmove (runner, runner + 1, (menu_store_size - (runner + 1 - menu_store)));
    void* const newptr = arrnewsize (menu_store, --menu_store_size);
    if (newptr != NULL || menu_store_size == 0) menu_store = newptr;
  }

  return true;
}

/* -----------------------------------------------------------------------------
// Hotkey */

static const wchar_t hkey_str_off[]    = L"Off+";
static const wchar_t hkey_str_ctrl[]   = L"Ctrl+";
static const wchar_t hkey_str_alt[]    = L"Alt+";
static const wchar_t hkey_str_shift[]  = L"Shift+";
static const wchar_t hkey_str_win[]    = L"Win+";
static const wchar_t hkey_str_ins[]    = L"Insert";
static const wchar_t hkey_str_del[]    = L"Delete";
static const wchar_t hkey_str_home[]   = L"Home";
static const wchar_t hkey_str_end[]    = L"End";
static const wchar_t hkey_str_pgup[]   = L"PageUp";
static const wchar_t hkey_str_pgdn[]   = L"PageDown";
static const wchar_t hkey_str_num[]    = L"Num";
static const wchar_t hkey_str_div[]    = L"Divide";
static const wchar_t hkey_str_mul[]    = L"Multiply";
static const wchar_t hkey_str_sub[]    = L"Subtract";
static const wchar_t hkey_str_add[]    = L"Add";
static const wchar_t hkey_str_dec[]    = L"Decimal";
static const wchar_t hkey_str_left[]   = L"Left";
static const wchar_t hkey_str_up[]     = L"Up";
static const wchar_t hkey_str_right[]  = L"Right";
static const wchar_t hkey_str_down[]   = L"Down";
static const wchar_t hkey_str_tab[]    = L"Tab";
static const wchar_t hkey_str_bckspc[] = L"Backspace";

/* -------------------------------------------------------------------------- */

struct hotkey {
  /* Currently set state */
  union {
    struct {
      char ctrl, alt, shift, win;
    };
    int mod;
  };
  UINT code;
  /* Last working state. If currently set state fails to register,
  // this is restored. Is last working state fails as well,
  // the default combination is restored. If even that
  // fails, the hotkey gets disabled. */
  union {
    struct {
      char wctrl, walt, wshift, wwin;
    };
    int wmod;
  };
  UINT wcode;
  /* Hotkey registration */
  bool disabled;
  bool clear;
  bool set;
  bool reg;
  int id;
};

struct hotkey hkey_border;
struct hotkey hkey_border_def = (struct hotkey){
  .ctrl = false, .alt = true, .shift = false, .win = false,
  .code = 'B', .id = 0
};
struct hotkey hkey_menu;
struct hotkey hkey_menu_def = (struct hotkey){
  .ctrl = false, .alt = true, .shift = false, .win = false,
  .code = 'M', .id = 1
};

static inline void hotkey_save (struct hotkey* const hkey)
{
  hkey->wmod = hkey->mod;
  hkey->wcode = hkey->code;
}

static inline void hotkey_restore (struct hotkey* const hkey)
{
  hkey->mod = hkey->wmod;
  hkey->code = hkey->wcode;
}

static inline bool hotkey_is_set (const struct hotkey* const hkey)
{
  return (hkey->code >= 0x70 && hkey->code <= 0x87)
  || (hkey->code && hkey->mod);
}

static inline int hotkey_mod_to_int (const struct hotkey* const hkey)
{
  return (MOD_CONTROL * hkey->ctrl) | (MOD_ALT * hkey->alt)
  | (MOD_SHIFT * hkey->shift) | (MOD_WIN * hkey->win)
  | MOD_NOREPEAT;
}

static void hotkey_failed (const HWND wnd)
{
  MessageBoxW (wnd, L"Couldn't set the hotkey. Perhaps it is being used by another application?"
  , APP_TITLE, MB_APPLMODAL | MB_ICONWARNING | MB_OK);
}

static bool hotkey_unregister (HWND const wnd, struct hotkey* const hkey)
{
  if (hkey->reg) {
    if (!UnregisterHotKey (wnd, hkey->id)) return false;
    hkey->reg = false;
  }
  return true;
}

static bool hotkey_register (HWND const wnd, struct hotkey* const hkey)
{
  if (!hotkey_unregister (wnd, hkey)) return false;
  if (hkey->disabled || hkey->code == 0) return true;
  if (!RegisterHotKey (wnd, hkey->id, hotkey_mod_to_int (hkey), hkey->code)) {
    if (hkey->wcode != 0) {
      if (hkey->wcode != hkey->code || hkey->wmod != hkey->mod) {
        hotkey_restore (hkey);
        if (!RegisterHotKey (wnd, hkey->id, hotkey_mod_to_int (hkey), hkey->code)) {
          hkey->disabled = true;
        } else hkey->reg = true;
      } else hkey->disabled = true;
    }
    return false;
  }
  /* Remember keystroke as it was successfully registered */
  hotkey_save (hkey);
  hkey->reg = true;
  return true;
}

static void hotkey_to_str (wchar_t* const str, const struct hotkey* const hkey
, const bool conf)
{
#define hkeychar(c) *s++ = c; s[0] = '\0'
#define hkeystr(cstr) wcscpy (s, cstr); s += cstrlen(cstr)
  wchar_t* s = str;
  if (conf && hkey->disabled) {wcscpy (s, hkey_str_off); s += cstrlen(hkey_str_off);}
  /* Modifiers */
  if (hkey->ctrl)  {wcscpy (s, hkey_str_ctrl); s += cstrlen(hkey_str_ctrl);}
  if (hkey->alt)   {wcscpy (s, hkey_str_alt); s += cstrlen(hkey_str_alt);}
  if (hkey->shift) {wcscpy (s, hkey_str_shift); s += cstrlen(hkey_str_shift);}
  if (hkey->win)   {wcscpy (s, hkey_str_win); s += cstrlen(hkey_str_win);}
  /* Actual key */
  if (hkey->code) {
    const UINT key = hkey->code;
    /* Functional */
    if      (key >= 0x70 && key <= 0x87) {*s++ = 'F'; _itow (key - 0x70 + 1, s, 10);}
    /* Numpad */
    else if (key >= 0x60 && key <= 0x69) {wcscpy (s, hkey_str_num); s += cstrlen(hkey_str_num); _itow (key - 0x60, s, 10);}
    else switch (key) {
    /* ;: */case VK_OEM_1: hkeychar (';'); break;
    /* /? */case VK_OEM_2: hkeychar ('/'); break;
    /* `~ */case VK_OEM_3: hkeychar ('`'); break;
    /* [{ */case VK_OEM_4: hkeychar ('['); break;
    /* \| */case VK_OEM_5: hkeychar('\\'); break;
    /* ]} */case VK_OEM_6: hkeychar (']'); break;
    /* '" */case VK_OEM_7: hkeychar('\''); break;
    /* -_ */case VK_OEM_MINUS:  hkeychar ('-'); break;
    /* =+ */case VK_OEM_PLUS:   hkeychar ('='); break;
    /* ,< */case VK_OEM_COMMA:  hkeychar (','); break;
    /* .> */case VK_OEM_PERIOD: hkeychar ('.'); break;
    /* Numpad arithmetic operators and decimal separator */
    case VK_DIVIDE:   hkeystr (hkey_str_div); break;
    case VK_MULTIPLY: hkeystr (hkey_str_mul); break;
    case VK_SUBTRACT: hkeystr (hkey_str_sub); break;
    case VK_ADD:      hkeystr (hkey_str_add); break;
    case VK_DECIMAL:  hkeystr (hkey_str_dec); break;
    /* Insert & Delete */
    case VK_INSERT:   hkeystr (hkey_str_ins); break;
    case VK_DELETE:   hkeystr (hkey_str_del); break;
    /* Navigation */
    case VK_HOME:     hkeystr (hkey_str_home); break;
    case VK_END:      hkeystr (hkey_str_end);  break;
    case VK_PRIOR:    hkeystr (hkey_str_pgup); break;
    case VK_NEXT:     hkeystr (hkey_str_pgdn); break;
    case VK_LEFT:     hkeystr (hkey_str_left); break;
    case VK_UP:       hkeystr (hkey_str_up);   break;
    case VK_RIGHT:    hkeystr (hkey_str_right);break;
    case VK_DOWN:     hkeystr (hkey_str_down); break;
    case VK_TAB:      hkeystr (hkey_str_tab);  break;
    /* Backspace */
    case VK_BACK:   hkeystr (hkey_str_bckspc); break;
    /* Alphanumeric */
    default: *s++ = key; *s = '\0';
    }
  } else {
    if (conf) {
      str[0] = '\0';
    } else {
      if (s == str) s[0] = '\0';
      else *--s = '\0';
    }
  }
#undef hkeychar
#undef hkeystr
}

/* -----------------------------------------------------------------------------
// Hotkey edit box control */

static void update_keys (struct hotkey* const hkey, UINT const key
, bool const state)
{
  if (hkey->set) return;
  /* Modifiers: here `state` is unreliable... */
  hkey->ctrl  = get_key_state (VK_CONTROL) || get_key_state (VK_LCONTROL) || get_key_state (VK_RCONTROL);
  hkey->alt   = get_key_state (VK_MENU)    || get_key_state (VK_LMENU)    || get_key_state (VK_RMENU);
  hkey->shift = get_key_state (VK_SHIFT)   || get_key_state (VK_LSHIFT)   || get_key_state (VK_RSHIFT);
  hkey->win   = get_key_state (VK_LWIN)    || get_key_state (VK_RWIN);
  /*   A..Z                            0..9 */
  if ((key >= 0x41 && key <= 0x5a) || (key >= 0x30 && key <= 0x39)
  /*   Numpad 0..9                     F1..F24 */
  ||  (key >= 0x60 && key <= 0x69) || (key >= 0x70 && key <= 0x87)) {
is_key:
    hkey->code = key * state;
    goto done;
  }
  switch (key) {
  /* ;: */case VK_OEM_1:
  /* /? */case VK_OEM_2:
  /* `~ */case VK_OEM_3:
  /* [{ */case VK_OEM_4:
  /* \| */case VK_OEM_5:
  /* ]} */case VK_OEM_6:
  /* '" */case VK_OEM_7:
  /* -_ */case VK_OEM_MINUS:
  /* =+ */case VK_OEM_PLUS:
  /* ,< */case VK_OEM_COMMA:
  /* .> */case VK_OEM_PERIOD:
  /* Numpad arithmetic operators and decimal separator */
  case VK_MULTIPLY:
  case VK_DIVIDE:
  case VK_SUBTRACT:
  case VK_ADD:
  case VK_DECIMAL:
  /* Insert & Delete */
  case VK_INSERT:
  case VK_DELETE:
  /* Navigation */
  case VK_HOME:
  case VK_END:
  case VK_PRIOR:
  case VK_NEXT:
  //case VK_LEFT:
  //case VK_UP:
  //case VK_RIGHT:
  //case VK_DOWN:
  //case VK_TAB:
  /* Backspace */
  case VK_BACK: goto is_key;
  }
done:
  hkey->set = hotkey_is_set (hkey);
}

static void update_hotkey (const HWND wnd, const struct hotkey* const hkey)
{
  wchar_t str[256];
  hotkey_to_str (str, hkey, false);
  SetWindowTextW (wnd, str);
  PostMessageW (wnd, EM_SETSEL, 0, 0);
}

static LRESULT CALLBACK edit_hkey_wnd_proc (HWND const wnd, UINT const msg
, WPARAM const wParam, LPARAM const lParam)
{
  struct hotkey* const hkey = wnd == edit_hkey_hide_border
  ? &hkey_border : &hkey_menu;
  switch (msg) {
  case WM_SYSCOMMAND: return 0; // turn off dumbass beep when pressing Alt+<Key>
  case WM_SETFOCUS:
    hkey->clear = true;
    break;
  case WM_KILLFOCUS:
    if (hkey->set) {
      hkey->set = false;
      if (!hotkey_register (wnd_main, hkey)) hotkey_failed (wnd_main);
    } else hotkey_restore (hkey);
    update_hotkey (wnd, hkey);
    break;
  case WM_SYSKEYDOWN:
  case WM_KEYDOWN: {
    const UINT key = wParam;
    if (key <= VK_XBUTTON2) return 0;
    if (key == VK_ESCAPE) {
      EnableWindow (wnd, FALSE);
      EnableWindow (wnd, TRUE);
      return 0;
    }
    if (hkey->clear) {
      hkey->clear = false;
      hkey->mod = hkey->code = 0;
    }
    update_keys (hkey, key, true);
    update_hotkey (wnd, hkey);
    return 0;
  }
  case WM_SYSKEYUP:
  case WM_KEYUP: {
    const UINT key = wParam;
    if (key <= VK_XBUTTON2) return 0;
    if (key == VK_ESCAPE) return 0;
    update_keys (hkey, key, false);
    update_hotkey (wnd, hkey);
    if (hkey->set) {
      /* Combination is set: de-focus edit control.
      // If user wants to re-map the key they
      // need to focus and press again. */
      EnableWindow (wnd, FALSE);
      EnableWindow (wnd, TRUE);
    }
    return 0;
  }}
  return CallWindowProcW (edit_wnd_proc, wnd, msg, wParam, lParam);
}

/* -----------------------------------------------------------------------------
// Configuration file */

static bool parse_hotkey (const wchar_t** const str, struct hotkey* const hkey
, int const id)
{
#define hkeymod(s, cstr, mod) if (cstrniequ (s, cstr)) {hkey->mod = true; s += cstrlen (cstr); continue;}
#define hkeycode(s, cstr, vk) if (cstrniequ (s, cstr)) {if (hkey->code) {str[0] = s; return false;} hkey->code = vk; s += cstrlen (cstr); continue;}
#define hkeynum09(s) if (cstrniequ (s, hkey_str_num)) {if (hkey->code || !iswdigit09 (s[cstrlen (hkey_str_num)])) {str[0] = s; return false;} hkey->code = VK_NUMPAD0 + s[0] - '0'; s += cstrlen (hkey_str_num) + 1; continue;}
#define hkeychar(s, c, vk) if (s[0] == c) {if (hkey->code) {str[0] = s; return false;} hkey->code = vk; ++s; continue;}
#define hkeychar2(s, c1, c2, vk) if (s[0] == c1 || s[0] == c2) {if (hkey->code) {str[0] = s; return false;} hkey->code = vk; ++s; continue;}
#define hkeycharaz(s) if (iswalphab (s[0] | 0x20)) {if (hkey->code) {str[0] = s; return false;} hkey->code = s[0] & ~0x20; ++s; continue;}
#define hkeychar09(s) if (iswdigit09 (s[0])) {if (hkey->code) {str[0] = s; return false;} hkey->code = s[0]; ++s; continue;}
#define hkeyfunc(s) if ((s[0] | 0x20) == 'f' && iswdigit19 (s[1])) {if (hkey->code) {str[0] = s; return false;} int c = s[1] - '0'; if (iswdigit09 (s[2])) c = c * 10 + s[2] - '0'; if (c > 24) {str[0] = s; return false;} hkey->code = VK_F1 - 1 + c; s += 2 + c > 9; continue;}
  objzero (hkey);
  hkey->id = id;
  const wchar_t* s = str[0];
  while (s[0] != '\0') {
    if (cstrniequ (s, hkey_str_off)) {hkey->disabled = true; s += cstrlen (hkey_str_off); continue;}
    /* Modifiers */
    hkeymod (s, hkey_str_ctrl, ctrl);
    hkeymod (s, hkey_str_alt, alt);
    hkeymod (s, hkey_str_shift, shift);
    hkeymod (s, hkey_str_win, win);
    /* A..Z & 0..9 */
    hkeycharaz (s);
    hkeychar09 (s);
    /* ;: */hkeychar2 (s, ';', ':', VK_OEM_1);
    /* /? */hkeychar2 (s, '/', '?', VK_OEM_2);
    /* `~ */hkeychar2 (s, '`', '~', VK_OEM_3);
    /* [{ */hkeychar2 (s, '[', '{', VK_OEM_4);
    /* \| */hkeychar2 (s,'\\', '|', VK_OEM_5);
    /* ]} */hkeychar2 (s, ']', '}', VK_OEM_6);
    /* '" */hkeychar2 (s,'\'', '"', VK_OEM_7);
    /* -_ */hkeychar2 (s, '-', '_', VK_OEM_MINUS);
    /* =+ */hkeychar2 (s, '=', '+', VK_OEM_PLUS);
    /* ,< */hkeychar2 (s, ',', '<', VK_OEM_COMMA);
    /* .> */hkeychar2 (s, '.', '>', VK_OEM_PERIOD);
    /* Numpad 0..9 */
    hkeynum09 (s);
    /* Numpad arithmetic operators and decimal separator */
    hkeycode (s, hkey_str_ins, VK_DIVIDE);
    hkeycode (s, hkey_str_mul, VK_MULTIPLY);
    hkeycode (s, hkey_str_sub, VK_SUBTRACT);
    hkeycode (s, hkey_str_add, VK_ADD);
    hkeycode (s, hkey_str_dec, VK_DECIMAL);
    /* F1..F24 */
    hkeyfunc (s);
    /* Insert & Delete */
    hkeycode (s, hkey_str_ins, VK_INSERT);
    hkeycode (s, hkey_str_del, VK_DELETE);
    /* Navigation */
    hkeycode (s, hkey_str_home, VK_HOME);
    hkeycode (s, hkey_str_end,  VK_END);
    hkeycode (s, hkey_str_pgup, VK_PRIOR);
    hkeycode (s, hkey_str_pgdn, VK_NEXT);
    //hkeycode (s, hkey_str_left,  VK_LEFT);
    //hkeycode (s, hkey_str_up,    VK_UP);
    //hkeycode (s, hkey_str_right, VK_RIGHT);
    //hkeycode (s, hkey_str_down,  VK_DOWN);
    //hkeycode (s, hkey_str_tab,   VK_TAB);
    /* Backspace */
    hkeycode (s, hkey_str_bckspc, VK_BACK);
    /* Unknown */
    str[0] = s;
    return false;
  }
  str[0] = s;
  return hotkey_is_set (hkey);
#undef hkeymod
#undef hkeycode
#undef hkeynum09
#undef hkeychar2
#undef hkeychar
#undef hkeycharaz
#undef hkeychar09
#undef hkeyfunc
}

static bool read_config (const wchar_t* const path)
{
#define read_line(line) do {\
  if (fgetws (line, numof(line), f) == NULL) {\
    bool eof = feof (f);\
    fclose (f);\
    if (eof) return true;\
    return false;\
  }\
  if (wcslen (line) != 0 && line[wcslen (line) - 1] == '\n') line[wcslen (line) - 1] = '\0';\
} while (0)
  struct __stat64 stat;
  if (_wstat64 (path, &stat) == -1) return false;
  if (stat.st_size & 1) return false;

  wchar_t line[256] = {0};
  FILE* f = _wfopen (path, L"r,ccs=UTF-16LE");
  if (f == NULL) return false;

  /* Border hide hotkey */
  read_line (line);
  const wchar_t* l = line;
  if (!parse_hotkey (&l, &hkey_border, hkey_border_def.id)
  ) hkey_border = hkey_border_def;

  /* Menu hide hotkey */
  read_line (line);
  l = line;
  if (!parse_hotkey (&l, &hkey_menu, hkey_menu_def.id)
  ) hkey_menu = hkey_menu_def;

  /* Border hide style masks */
  read_line (line);
  style_mask = parse_mask (line);
  read_line (line);
  style_ex_mask = parse_mask (line);

  /* Coffee button */
  read_line (line);
  show_coffee = wcscmp (line, L"true") == 0;

  fclose (f);
  return true;
#undef read_line
}

static bool config_save (const wchar_t* const path)
{
#define write_line(line) do {\
  fputws (line, f);\
  fputwc ('\n', f);\
} while (0)
  wchar_t line[256] = {0};
  FILE* f = _wfopen (path, L"wt+,ccs=UTF-16LE");
  if (f == NULL) return false;

  /* Border hide hotkey */
  hotkey_to_str (line, &hkey_border, true);
  write_line (line);

  /* Menu hide hotkey */
  hotkey_to_str (line, &hkey_menu, true);
  write_line (line);

  /* Border hide style masks */
  line[0] = '0'; line[1] = 'x';
  _i64tow (style_mask, line + 2, 16);
  write_line (line);
  line[0] = '0'; line[1] = 'x';
  _i64tow (style_ex_mask, line + 2, 16);
  write_line (line);

  /* Coffee button */
  wcscpy (line, show_coffee ? L"true" : L"false");
  write_line (line);

  fclose (f);
  return true;
#undef write_line
}

/* -----------------------------------------------------------------------------
// Tray icon */

#define TRAY_ICON_ID 1
#define TRAY_ICON_MSG WM_APP

static void tray_icon_add (HWND const wnd)
{
  NOTIFYICONDATA nid = {
    .hIcon            = LoadIconW (app_instance, L"TRAYICON"),
    .hWnd             = wnd,
    .uID              = TRAY_ICON_ID,
    .uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP,
    .uCallbackMessage = TRAY_ICON_MSG
  };
  wcscpy (nid.szTip, APP_TITLE L" " APP_VERSION);
  Shell_NotifyIconW (NIM_ADD, &nid);
}

static void tray_icon_remove (HWND const wnd)
{
  NOTIFYICONDATA nid = {
    .hWnd = wnd,
    .uID  = TRAY_ICON_ID
  };
  Shell_NotifyIconW (NIM_DELETE, &nid);
}

/* -----------------------------------------------------------------------------
// Popup menu */

#define ID_CONFIGURE 1001
#define ID_DONATE 1002
#define ID_EXIT 1003

#define ID_ENABLE_BORDER 2001
#define ID_ENABLE_MENU 2002
#define ID_DISABLE_COFFEE 2003

static void popup_show (HWND const wnd, HMENU const popup, const POINT* xy)
{
  SendMessageW (wnd, WM_INITMENUPOPUP, (WPARAM)popup, 0);

  /* Get cursor position */
  POINT pt;
  if (xy == NULL) {
    GetCursorPos (&pt);
    xy = &pt;
  }

  /* Show popup menu */
  const WORD cmd = TrackPopupMenu (popup
  , TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY
  , xy->x, xy->y, 0, wnd, NULL);

  /* Execute corresponding command */
  SendMessageW (wnd, WM_COMMAND, cmd, 0);
}

/* -----------------------------------------------------------------------------
// Main window */

static HFONT create_font (void)
{
  font_size = MulDiv (9, dpiy, 72);
  return CreateFontW (-font_size, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE
  , DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY
  , DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));
}

static void font_set (HWND const wnd, HFONT const font)
{
  SendMessageW (wnd, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
  SendMessageW (cbox_hkey_hide_border, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
  SendMessageW (edit_hkey_hide_border, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
  SendMessageW (cbox_hkey_hide_menu, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
  SendMessageW (edit_hkey_hide_menu, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
  SendMessageW (cbox_coffee, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
}

static void wnd_main_layout (int const width, int const height)
{
  MoveWindow (cbox_hkey_hide_border, DPIX(8), DPIY(8), width - DPIX(16), DPIY(12), true);
  MoveWindow (edit_hkey_hide_border, DPIX(8), DPIY(8 + 12 + 3), width - DPIX(16), DPIY(16), true);
  MoveWindow (cbox_hkey_hide_menu, DPIX(8), DPIY(8 + 12 + 3 + 16 + 6), width - DPIX(16), DPIY(12), true);
  MoveWindow (edit_hkey_hide_menu, DPIX(8), DPIY(8 + 12 + 3 + 16 + 6 + 12 + 3), width - DPIX(16), DPIY(16), true);
  MoveWindow (cbox_coffee, DPIX(8), DPIY(8 + 12 + 3 + 16 + 6 + 12 + 3 + 16 + 6 + 3), width - DPIX(16), DPIY(16), true);
}

/* -------------------------------------------------------------------------- */

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02e0
#endif

static LRESULT CALLBACK wnd_main_proc (HWND const wnd, UINT const msg
, WPARAM const wparam, LPARAM const lparam)
{
  int err_code = EXIT_SUCCESS;
  switch (msg) {
  case WM_SYSCOMMAND: if (wparam == SC_KEYMENU) return 0; break;
  case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_WANTTAB;
  case WM_CREATE: {
    wnd_main = wnd;

    /* Create controls */
    cbox_hkey_hide_border = CreateWindowW (L"BUTTON", L"", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP
    , 0, 0, 0, 0, wnd, (HMENU)ID_ENABLE_BORDER, NULL, NULL);
    SetWindowTextW (cbox_hkey_hide_border, L"Toggle border:");
    edit_hkey_hide_border = CreateWindowW (L"EDIT", L"", WS_BORDER | WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY
    , 0, 0, 0, 0, wnd, NULL, NULL, NULL);
    edit_wnd_proc = (WNDPROC)SetWindowLongW (edit_hkey_hide_border, GWLP_WNDPROC, (LONG)&edit_hkey_wnd_proc);
    cbox_hkey_hide_menu = CreateWindowW (L"BUTTON", L"", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP
    , 0, 0, 0, 16, wnd, (HMENU)ID_ENABLE_MENU, NULL, NULL);
    SetWindowTextW (cbox_hkey_hide_menu, L"Toggle menu:");
    edit_hkey_hide_menu = CreateWindowW (L"EDIT", L"", WS_BORDER | WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY
    , 0, 0, 0, 0, wnd, NULL, NULL, NULL);
    SetWindowLongW (edit_hkey_hide_menu, GWLP_WNDPROC, (LONG)&edit_hkey_wnd_proc);
    cbox_coffee = CreateWindowW (L"BUTTON", L"", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP
    , 0, 0, 0, 0, wnd, (HMENU)ID_DISABLE_COFFEE, NULL, NULL);
    SetWindowTextW (cbox_coffee, L"Hide donation menu entry");

    /* Create popup menu for tray icon */
    menu_popup = CreatePopupMenu();
    AppendMenuW (menu_popup, MF_STRING, ID_CONFIGURE, L"&Configure...");
    if (show_coffee) AppendMenuW (menu_popup, MF_STRING, ID_DONATE, L"Co&ffee...");
    AppendMenuW (menu_popup, MF_SEPARATOR, 0, NULL);
    AppendMenuW (menu_popup, MF_STRING, ID_EXIT, L"E&xit");
    SetMenuDefaultItem (menu_popup, ID_CONFIGURE, FALSE);

    if (!cbox_hkey_hide_border || !edit_hkey_hide_border
    ||  !cbox_hkey_hide_menu || !edit_hkey_hide_menu
    ||  !cbox_coffee || !menu_popup) {
      err_code = EXIT_FAILURE;
      goto failure;
    }

    /* Obtain current DPI */
    if (is_windows81()) {
      /* Windows 10 1607: `GetDpiForSystem()`
      // Windows 10 1803: `GetSystemDpiForProcess()` */
      UINT dpix_out, dpiy_out;
      if (GetDpiForMonitor (MonitorFromWindow (wnd, MONITOR_DEFAULTTONEAREST)
      , MDT_DEFAULT, &dpix_out, &dpiy_out) != S_OK) goto dpi_fallback;
      dpix = dpix_out;
      dpiy = dpiy_out;
    } else {
dpi_fallback:
      dpix = GetDeviceCaps (GetDC (GetDesktopWindow()), LOGPIXELSX);
      dpiy = GetDeviceCaps (GetDC (GetDesktopWindow()), LOGPIXELSY);
    }

    /* Get the proper font */
    font_gui = create_font();
    if (font_gui == NULL) {
      err_code = EXIT_FAILURE;
      goto failure;
    }
    font_set (wnd, font_gui);

    /* Register global hotkeys */
    hotkey_register (wnd, &hkey_border);
    hotkey_register (wnd, &hkey_menu);

    /* Do not disable hotkey edit boxes if their corresponding
    // check box is unticked. Otherwise conflicting hotkey
    // would be impossible to edit and would remain
    // permanently disabled. */
    SendMessageW (cbox_hkey_hide_border, BM_SETCHECK, hkey_border.disabled
    ? BST_UNCHECKED : BST_CHECKED, 0);
    update_hotkey (edit_hkey_hide_border, &hkey_border);
    SendMessageW (cbox_hkey_hide_menu, BM_SETCHECK, hkey_menu.disabled
    ? BST_UNCHECKED : BST_CHECKED, 0);
    update_hotkey (edit_hkey_hide_menu, &hkey_menu);
    SendMessageW (cbox_coffee, BM_SETCHECK, show_coffee
    ? BST_UNCHECKED : BST_CHECKED, 0);

    /* Show tray icon */
    tray_icon_add (wnd);

    /* Trigger resize on initial show
    // to give controls real dimensions */
    int desktopWidth, desktopHeight;
    desktop_size (&desktopWidth, &desktopHeight);
    MoveWindow (wnd, desktopWidth - DPIX(240), desktopHeight / 2
    , DPIX(200), DPIY(140), TRUE);

    return 0;
  }
  /* DPI-awareness */
  case WM_DPICHANGED: {
    dpix = LOWORD(wparam);
    dpiy = HIWORD(wparam);

    /* Recreate GUI font for new DPI */
    HFONT const new_font = create_font();
    font_set (wnd, new_font);
    DeleteObject (font_gui);
    font_gui = new_font;

    const RECT* const new_pos = (RECT*)lparam;
    MoveWindow (wnd, new_pos->left, new_pos->top
    , new_pos->right - new_pos->left
    , new_pos->bottom - new_pos->top
    , true);

    return 0;
  }
  /* Window layout */
  case WM_SIZE:
    const int width = LOWORD(lparam);
    const int height = HIWORD(lparam);
    wnd_main_layout (width, height);
    return 0;
  /* Respond to global hotkeys */
  case WM_HOTKEY:
    if      (wparam == hkey_border.id) remove_border (GetForegroundWindow());
    else if (wparam == hkey_menu.id)   remove_menu (GetForegroundWindow());
    return 0;
  /* Window destruction */
  case WM_CLOSE:
    ShowWindow (wnd, SW_HIDE);
    return 0;
  case WM_DESTROY:
    hotkey_unregister (wnd, &hkey_border);
    hotkey_unregister (wnd, &hkey_menu);
    tray_icon_remove (wnd);
    DestroyMenu (menu_popup);
    PostQuitMessage (err_code);
    return 0;
  case WM_NCDESTROY:
    DeleteObject (font_gui);
    return 0;
  /* Controls and popup menu */
  case WM_COMMAND:
    switch (LOWORD (wparam)) {
    /* Popup menu actions */
    case ID_CONFIGURE:
      ShowWindow (wnd, SW_RESTORE);
      SetForegroundWindow (wnd);
      break;
    case ID_DONATE:
      shell_run (L"https://www.buymeacoffee.com/ubihazard");
      break;
    case ID_EXIT:
      SendMessageW (wnd, WM_CLOSE, 0, 0);
failure:
      DestroyWindow (wnd);
      break;
    /* Check boxes */
    case ID_ENABLE_BORDER:
      if (hkey_border.disabled) {
        hkey_border.disabled = false;
        if (!hotkey_register (wnd, &hkey_border)) {
          hotkey_failed (wnd);
          break;
        }
        SendMessageW (cbox_hkey_hide_border, BM_SETCHECK, BST_CHECKED, 0);
      } else {
        if (!hotkey_unregister (wnd, &hkey_border)) break;
        hkey_border.disabled = true;
        SendMessageW (cbox_hkey_hide_border, BM_SETCHECK, BST_UNCHECKED, 0);
      }
      break;
    case ID_ENABLE_MENU:
      if (hkey_menu.disabled) {
        hkey_menu.disabled = false;
        if (!hotkey_register (wnd, &hkey_menu)) {
          hotkey_failed (wnd);
          break;
        }
        SendMessageW (cbox_hkey_hide_menu, BM_SETCHECK, BST_CHECKED, 0);
      } else {
        if (!hotkey_unregister (wnd, &hkey_menu)) break;
        hkey_menu.disabled = true;
        SendMessageW (cbox_hkey_hide_menu, BM_SETCHECK, BST_UNCHECKED, 0);
      }
      break;
    case ID_DISABLE_COFFEE:
      if (show_coffee) {
        show_coffee = false;
        SendMessageW (cbox_coffee, BM_SETCHECK, BST_CHECKED, 0);
        RemoveMenu (menu_popup, ID_DONATE, MF_BYCOMMAND);
      } else {
        show_coffee = true;
        SendMessageW (cbox_coffee, BM_SETCHECK, BST_UNCHECKED, 0);
        InsertMenuW (menu_popup, 1, MF_STRING | MF_BYPOSITION, ID_DONATE, L"Co&ffee...");
      }
      break;
    }
    return 0;
  /* Tray icon */
  case WM_APP:
    switch (lparam) {
    case WM_LBUTTONUP:
      if (is_visible (wnd)) {
        ShowWindow (wnd, SW_HIDE);
      } else {
        ShowWindow (wnd, SW_RESTORE);
        SetForegroundWindow (wnd);
      }
      break;
    case WM_RBUTTONUP:
      popup_show (wnd, menu_popup, NULL);
      PostMessageW (wnd, WM_APP + 1, 0, 0);
      break;
    }
    return 0;
  /* Do not paint read-only edit boxes in disabled color */
  case WM_CTLCOLORSTATIC: {
    const HWND h = (HWND)lparam;
    if (h == edit_hkey_hide_border || h == edit_hkey_hide_menu) {
      /*const struct hotkey* const hkey = h == edit_hkey_hide_border
      ? &hkey_border : &hkey_menu;
      if (!hkey->disabled) */return (LRESULT)GetSysColorBrush (COLOR_WINDOW);
    } else {
      SetBkMode ((HDC)wparam, TRANSPARENT);
      return (LRESULT)GetStockObject (NULL_BRUSH);
    }
    break;
  }}
  return DefWindowProcW (wnd, msg, wparam, lparam);
}

/* ========================================================================== */

int WINAPI wWinMain (HINSTANCE const inst, HINSTANCE const prev
, wchar_t* const cmd, int const show)
{
#ifndef NDEBUG
  AllocConsole();
  freopen ("CONIN$", "r", stdin);
  freopen ("CONOUT$", "w", stdout);
  freopen ("CONOUT$", "w", stderr);
#endif

  app_instance = inst;

  if (CoInitializeEx (NULL, COINIT_APARTMENTTHREADED
  | COINIT_DISABLE_OLE1DDE) != S_OK) return EXIT_FAILURE;

  /* Allow only one instance */
  if ((mutex = CreateMutexW (NULL, TRUE, APP_ID)) == NULL) {
    return EXIT_FAILURE;
  }

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
failure_early:
    CloseHandle (mutex);
    return EXIT_FAILURE;
  }

  /* Since we want to support Windows 7,
  // have to load some routines at runtime */
  lib_shcore = LoadLibraryW (L"shcore");
  GetDpiForMonitor_fn* const GetDpiForMonitor = (GetDpiForMonitor_fn*)GetProcAddress (lib_shcore, "GetDpiForMonitor");
  if (GetDpiForMonitor == NULL) goto failure_early;

  /* Read configuration */
  first_run = !read_config (L".\\config");

  if (first_run) {
    hkey_border = hkey_border_def;
    hkey_menu = hkey_menu_def;
  }

  hotkey_save (&hkey_border);
  hotkey_save (&hkey_menu);

  /* Create main window */
  WNDCLASSEX wclx = {
    .cbSize      = sizeof (wclx),
    .style       = 0,
    .lpfnWndProc = &wnd_main_proc,
    .cbClsExtra  = 0,
    .cbWndExtra  = 0,
    .hInstance   = inst,
    .hIcon       = LoadIconW (app_instance, L"MAINICON"),
    .hIconSm     = LoadIconW (app_instance, L"TRAYICON"),
    .hCursor     = LoadCursorW (NULL, IDC_ARROW),
    .hbrBackground = GetSysColorBrush (COLOR_BTNFACE),
    .lpszMenuName  = NULL,
    .lpszClassName = APP_CLASSNAME
  };
  RegisterClassExW (&wclx);

  CreateWindowW (APP_CLASSNAME, APP_TITLE
  , WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | (WS_VISIBLE * first_run)
  , CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, inst, NULL);
  if (wnd_main == NULL) goto failure;

  /* Enter GUI message loop */
  MSG msg;
  while (GetMessageW (&msg, NULL, 0, 0)) {
    /* `IsDialogMessage()` steals the escape key.
    // MSDN is silent about how to prevent this,
    // other than making this hack. */
    if ((msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
    ||  !IsDialogMessageW (wnd_main, &msg)) {
      TranslateMessage (&msg);
      DispatchMessageW (&msg);
    }
  }

  /* Create configuration file */
  config_save (L".\\config");

  /* Free remaining resources */
failure:
  UnregisterClassW (APP_CLASSNAME, inst);
  FreeLibrary (lib_shcore);
  CloseHandle (mutex);

  return msg.wParam;
}
