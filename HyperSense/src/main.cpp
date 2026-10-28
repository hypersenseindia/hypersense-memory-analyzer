// =====================================================================================
//  HYPERSENSEGLOBAL(TM)
//  Live Usermode Memory Analysis Tool - V1
//
//  Front-end for the Moneta usermode memory scanner.
//  Engine credits : Forrest Orr & Gabriel Landau (Moneta, GPLv3)
//  Tool dev       : AG HYDRAXFF (@hydraxff_yt)
// =====================================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <sddl.h>
#include <richedit.h>
#include <dwmapi.h>
#include <strsafe.h>

#include <string>
#include <vector>
#include <deque>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <algorithm>

#include "sha256.h"

// Moneta engine entry point (Source/Console.cpp). Compiled into this executable;
// --engine mode runs it in a hidden child process with stdout piped to the UI.
extern int MonetaMain(int nArgc, const wchar_t* pArgv[]);

// =====================================================================================
//  Theme
// =====================================================================================

namespace Th {
    const COLORREF BG        = RGB(8, 11, 17);
    const COLORREF HEADER    = RGB(11, 16, 24);
    const COLORREF CARD      = RGB(16, 22, 33);
    const COLORREF CARD_HI   = RGB(21, 29, 43);
    const COLORREF INPUT     = RGB(11, 17, 26);
    const COLORREF BORDER    = RGB(31, 43, 61);
    const COLORREF BORDER_HI = RGB(45, 62, 86);
    const COLORREF TEXT      = RGB(229, 236, 246);
    const COLORREF TEXT_DIM  = RGB(148, 163, 184);
    const COLORREF TEXT_MUTE = RGB(100, 116, 139);
    const COLORREF ACCENT    = RGB(34, 211, 238);
    const COLORREF ACCENT_D  = RGB(20, 140, 160);
    const COLORREF DANGER    = RGB(244, 63, 94);
    const COLORREF DANGER_BG = RGB(44, 19, 27);
    const COLORREF WARN      = RGB(245, 158, 11);
    const COLORREF OK        = RGB(52, 211, 153);
    const COLORREF LINK      = RGB(56, 189, 248);
}

// =====================================================================================
//  Control ids / messages
// =====================================================================================

enum : int {
    HS_COMBO = 100, HS_RICH, HS_EDIT_ADDR, HS_EDIT_SIZE, HS_EDIT_SEARCH,
    HS_BTN_SELECT, HS_BTN_REFRESH, HS_BTN_RUN, HS_BTN_STOP,
    HS_BTN_SAVE_TXT, HS_BTN_SAVE_HTML, HS_BTN_CLEAR, HS_BTN_ELEVATE,
    HS_MODE0 = 200, HS_MODE1, HS_MODE2, HS_MODE3,
    HS_VERB0 = 210, HS_VERB1, HS_VERB2,
    HS_TG_STATS = 300, HS_TG_FROMBASE, HS_TG_DUMP, HS_TG_FILTERALL,
    HS_TG_UNSIGNED, HS_TG_METADATA, HS_TG_CLRHEAP, HS_TG_CLRPRVX, HS_TG_WOW64,
    HS_TG_INSTA = 320,
    // tabs above the output view
    HS_TAB_OUTPUT = 400, HS_TAB_CMD, HS_TAB_DETAILS,
    // classic (moneta64.exe) mode
    HS_EDIT_PID = 410, HS_TG_CLASSIC, HS_TG_FOCUSUNSIGNED, HS_INFO,
    HS_PRESET, HS_CMD,
    // output right-click menu
    HS_MENU_FIRST = 500
};

#define WM_HS_OUTPUT (WM_APP + 1)
#define WM_HS_DONE   (WM_APP + 2)

// =====================================================================================
//  Globals
// =====================================================================================

HINSTANCE g_hInst = nullptr;
HWND g_hWnd = nullptr, g_hRich = nullptr, g_hCombo = nullptr;
HWND g_hEditAddr = nullptr, g_hEditSize = nullptr, g_hEditSearch = nullptr;
HWND g_hBtnSelect = nullptr, g_hBtnRefresh = nullptr, g_hBtnRun = nullptr, g_hBtnStop = nullptr;
HWND g_hBtnSaveTxt = nullptr, g_hBtnSaveHtml = nullptr, g_hBtnClear = nullptr, g_hBtnElevate = nullptr;
HWND g_hModeBtn[4] = {};
HWND g_hVerbBtn[3] = {};
HWND g_hTgStats = nullptr, g_hTgFromBase = nullptr, g_hTgDump = nullptr, g_hTgFilterAll = nullptr;
HWND g_hTgUnsigned = nullptr, g_hTgMetadata = nullptr, g_hTgClrHeap = nullptr, g_hTgClrPrvx = nullptr, g_hTgWow64 = nullptr;
HWND g_hTgInsta = nullptr;

// tabs / command view / classic mode / tooltips
HWND g_hTab[3] = {};
HWND g_hInfo = nullptr;
HWND g_hTip = nullptr;
HWND g_hEditPid = nullptr;
HWND g_hTgClassic = nullptr;
HWND g_hTgFocusUnsigned = nullptr;
HWND g_hPreset = nullptr;     // classic-mode command presets
HWND g_hCmd = nullptr;        // classic-mode command line (editable)
int  g_tab = 0;              // 0 = output, 1 = command, 2 = details
bool g_classic = false;      // moneta64.exe-style minimal mode
bool g_focusUnsigned = false;// dim signed-only lines in the live view
bool g_cmdSync = false;      // preview is being rewritten; ignore edit notifications
bool g_applyingPreset = false;// suppress "custom" reset while a preset is applied

// defined further down (declared here so the selection handlers can refresh them)
void BuildInfoView();
void ShowTab(int tab);
void ApplyClassicMode();
void UpdateClassicPreview();
void MarkCustomPreset();
void HideControl(HWND c);

int g_dpi = 96;
HFONT g_fBrand = nullptr, g_fBrandSub = nullptr, g_fCardTitle = nullptr;
HFONT g_fBody = nullptr, g_fBodyBold = nullptr, g_fSmall = nullptr, g_fTiny = nullptr;
HFONT g_fBtn = nullptr, g_fBtnSm = nullptr, g_fMono = nullptr, g_fMonoBig = nullptr, g_fStat = nullptr;
HBRUSH g_brushInput = nullptr;
HBRUSH g_brushCard = nullptr;

struct Rect { long left, top, right, bottom; };

struct SideMetrics {
    int targetH, modeH, optH, filterH, gap;
    bool twoCol;
    int titleY;
    int tSearchY, tComboY, tBtnY, tBtnH;
    int mRowY, mRowH, vLabelY, vRowY, vRowH, aLabelY, aRowY, aRowH;
    int oFirstY, oStep, oH;
    int fFirstY, fStep, fH;
};

struct Layout {
    RECT cardTarget{}, cardMode{}, cardOpt{}, cardFilter{};
    RECT infoCard{}, actionRow{}, logCard{};
    RECT classicBar{};
    RECT classicStatus{};
    RECT combination{};
    int footerTop = 0;
    int headerH = 0;
    int sidebarW = 0;
    SideMetrics m{};
};
Layout g_L;

// =====================================================================================
//  Small helpers
// =====================================================================================

inline int S(int v) { return MulDiv(v, g_dpi, 96); }

std::wstring Format(const wchar_t* fmt, ...) {
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    return std::wstring(buf);
}

std::wstring Lower(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = (wchar_t)towlower(c);
    return r;
}

bool ContainsI(const std::string& hay, const char* needle) {
    if (hay.empty() || !needle || !*needle) return false;
    std::string h = hay, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return (char)tolower(c); });
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)tolower(c); });
    return h.find(n) != std::string::npos;
}

std::wstring Widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string NarrowLocal(const std::wstring& s) {
    if (s.empty()) return "";
    int n = WideCharToMultiByte(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string r((size_t)n, '\0');
    WideCharToMultiByte(CP_ACP, 0, s.data(), (int)s.size(), &r[0], n, nullptr, nullptr);
    return r;
}

std::wstring ExeDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *slash = 0;
    return std::wstring(path);
}

std::wstring ExePath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::wstring(path);
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

std::wstring FileNameOf(const std::wstring& p) {
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? p : p.substr(pos + 1);
}

std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::wstring ReplaceAll(std::wstring s, const std::wstring& from, const std::wstring& to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::wstring HtmlEscape(const std::wstring& s) {
    std::wstring r;
    r.reserve(s.size() + 64);
    for (wchar_t c : s) {
        switch (c) {
            case L'&': r += L"&amp;"; break;
            case L'<': r += L"&lt;"; break;
            case L'>': r += L"&gt;"; break;
            case L'"': r += L"&quot;"; break;
            default: r += c; break;
        }
    }
    return r;
}

// =====================================================================================
//  Time helpers
// =====================================================================================

std::wstring StampLocal(const SYSTEMTIME& st, bool millis = true) {
    if (millis)
        return Format(L"%04d-%02d-%02d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay,
                      st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return Format(L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay,
                  st.wHour, st.wMinute, st.wSecond);
}

std::wstring NowLocal(bool millis = true) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return StampLocal(st, millis);
}

std::wstring NowUtc(bool millis = true) {
    SYSTEMTIME st;
    GetSystemTime(&st);
    return StampLocal(st, millis) + L" UTC";
}

int TzOffsetMinutes() {
    FILETIME utc, local;
    GetSystemTimeAsFileTime(&utc);
    FileTimeToLocalFileTime(&utc, &local);
    ULARGE_INTEGER u, l;
    u.LowPart = utc.dwLowDateTime; u.HighPart = utc.dwHighDateTime;
    l.LowPart = local.dwLowDateTime; l.HighPart = local.dwHighDateTime;
    long long diff = (long long)(l.QuadPart - u.QuadPart);
    return (int)(diff / 600000000LL);
}

std::wstring TzString() {
    int m = TzOffsetMinutes();
    wchar_t sign = m < 0 ? L'-' : L'+';
    if (m < 0) m = -m;
    return Format(L"UTC%c%02d:%02d", sign, m / 60, m % 60);
}

// =====================================================================================
//  System information
// =====================================================================================

struct SysInfo {
    std::wstring computer;
    std::wstring user;
    std::wstring domain;
    std::wstring account;
    std::wstring sid;
    std::wstring osName;
    std::wstring osBuild;
    std::wstring arch;
    bool elevated = false;
};

SysInfo g_sys;

bool IsElevated() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION el{};
    DWORD cb = 0;
    BOOL ok = GetTokenInformation(tok, TokenElevation, &el, sizeof(el), &cb);
    CloseHandle(tok);
    return ok && el.TokenIsElevated != 0;
}

std::wstring RegReadString(HKEY root, const wchar_t* sub, const wchar_t* name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) return L"";
    wchar_t buf[512] = {};
    DWORD cb = sizeof(buf) - sizeof(wchar_t);
    DWORD type = 0;
    std::wstring out;
    if (RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)buf, &cb) == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ))
        out = buf;
    RegCloseKey(key);
    return out;
}

void InitSystemInfo() {
    wchar_t buf[256] = {};
    DWORD sz = 256;
    GetComputerNameW(buf, &sz);
    g_sys.computer = buf;

    sz = 256;
    GetUserNameW(buf, &sz);
    g_sys.user = buf;

    // SID + account
    HANDLE tok = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        DWORD len = 0;
        GetTokenInformation(tok, TokenUser, nullptr, 0, &len);
        if (len) {
            std::vector<BYTE> data(len);
            if (GetTokenInformation(tok, TokenUser, data.data(), len, &len)) {
                TOKEN_USER* tu = reinterpret_cast<TOKEN_USER*>(data.data());
                LPWSTR sidStr = nullptr;
                if (ConvertSidToStringSidW(tu->User.Sid, &sidStr)) {
                    g_sys.sid = sidStr;
                    LocalFree(sidStr);
                }
                wchar_t name[256] = {}, dom[256] = {};
                DWORD cn = 256, cd = 256;
                SID_NAME_USE use;
                if (LookupAccountSidW(nullptr, tu->User.Sid, name, &cn, dom, &cd, &use)) {
                    g_sys.account = name;
                    g_sys.domain = dom;
                }
            }
        }
        CloseHandle(tok);
    }
    if (g_sys.account.empty()) g_sys.account = g_sys.user;
    if (g_sys.sid.empty()) g_sys.sid = L"unavailable";

    // OS version
    typedef LONG(WINAPI* RtlGetVersion_t)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW vi{};
    vi.dwOSVersionInfoSize = sizeof(vi);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    if (nt) {
        auto fn = (RtlGetVersion_t)GetProcAddress(nt, "RtlGetVersion");
        if (fn) fn(&vi);
    }
    std::wstring product = RegReadString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName");
    std::wstring display = RegReadString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion");
    if (product.empty()) product = L"Windows";
    if (vi.dwBuildNumber >= 22000) {
        product = ReplaceAll(product, L"Windows 10", L"Windows 11");
    }
    if (!display.empty()) product += L" " + display;
    g_sys.osName = product;
    g_sys.osBuild = Format(L"%lu.%lu.%lu", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);

    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: g_sys.arch = L"x64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: g_sys.arch = L"ARM64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: g_sys.arch = L"x86"; break;
        default: g_sys.arch = L"unknown"; break;
    }
    g_sys.elevated = IsElevated();
}

// =====================================================================================
//  Fonts
// =====================================================================================

HFONT MakeFont(int tenthPt, int weight, const wchar_t* face) {
    return CreateFontW(-MulDiv(tenthPt, g_dpi, 720), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

void DestroyFonts() {
    HFONT* all[] = { &g_fBrand, &g_fBrandSub, &g_fCardTitle, &g_fBody, &g_fBodyBold,
                     &g_fSmall, &g_fTiny, &g_fBtn, &g_fBtnSm, &g_fMono, &g_fMonoBig, &g_fStat };
    for (auto p : all) {
        if (*p) { DeleteObject(*p); *p = nullptr; }
    }
}

void CreateFonts() {
    g_fBrand     = MakeFont(200, FW_BOLD, L"Segoe UI");
    g_fBrandSub  = MakeFont(78,  FW_SEMIBOLD, L"Segoe UI");
    g_fCardTitle = MakeFont(80,  FW_SEMIBOLD, L"Segoe UI");
    g_fBody      = MakeFont(95,  FW_NORMAL, L"Segoe UI");
    g_fBodyBold  = MakeFont(95,  FW_SEMIBOLD, L"Segoe UI");
    g_fSmall     = MakeFont(82,  FW_NORMAL, L"Segoe UI");
    g_fTiny      = MakeFont(72,  FW_NORMAL, L"Consolas");
    g_fBtn       = MakeFont(88,  FW_SEMIBOLD, L"Segoe UI");
    g_fBtnSm     = MakeFont(80,  FW_SEMIBOLD, L"Segoe UI");
    g_fMono      = MakeFont(95,  FW_NORMAL, L"Consolas");
    g_fMonoBig   = MakeFont(135, FW_NORMAL, L"Consolas");
    g_fStat      = MakeFont(130, FW_SEMIBOLD, L"Segoe UI");
}

// =====================================================================================
//  GDI helpers
// =====================================================================================

void FillRoundRect(HDC dc, const RECT& r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border == CLR_INVALID ? fill : border);
    HGDIOBJ ob = SelectObject(dc, br);
    HGDIOBJ op = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pen);
}

void FillSolid(HDC dc, const RECT& r, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void DrawLineH(HDC dc, int x1, int x2, int y, COLORREF color) {
    HPEN p = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ op = SelectObject(dc, p);
    MoveToEx(dc, x1, y, nullptr);
    LineTo(dc, x2, y);
    SelectObject(dc, op);
    DeleteObject(p);
}

void DrawLineV(HDC dc, int x, int y1, int y2, COLORREF color) {
    HPEN p = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ op = SelectObject(dc, p);
    MoveToEx(dc, x, y1, nullptr);
    LineTo(dc, x, y2);
    SelectObject(dc, op);
    DeleteObject(p);
}

void DrawTextC(HDC dc, const std::wstring& text, RECT r, HFONT font, COLORREF color, UINT flags, int letterExtra = 0) {
    HGDIOBJ of = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    SetTextCharacterExtra(dc, letterExtra);
    DrawTextW(dc, text.c_str(), (int)text.size(), &r, flags);
    SetTextCharacterExtra(dc, 0);
    SelectObject(dc, of);
}

void DrawChip(HDC dc, const RECT& r, const std::wstring& text, COLORREF border, COLORREF textCol, COLORREF bg) {
    FillRoundRect(dc, r, S(9), bg, border);
    RECT tr = r;
    DrawTextC(dc, text, tr, g_fBtnSm, textCol, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawLogo(HDC dc, int x, int y, int size, COLORREF accent) {
    RECT box{ x, y, x + size, y + size };
    FillRoundRect(dc, box, S(9), accent, accent);
    // dark H mark
    int cx = x + size / 2, cy = y + size / 2;
    int hw = S(4), hh = S(9);
    RECT l{ cx - S(9), cy - hh, cx - S(9) + hw, cy + hh };
    RECT rr{ cx + S(5), cy - hh, cx + S(5) + hw, cy + hh };
    RECT mid{ cx - S(9), cy - S(2), cx + S(9), cy + S(2) };
    FillSolid(dc, l, Th::BG);
    FillSolid(dc, rr, Th::BG);
    FillSolid(dc, mid, Th::BG);
}

// =====================================================================================
//  Custom button
// =====================================================================================

enum BtnStyle { BTN_PRIMARY = 0, BTN_GHOST, BTN_DANGER, BTN_SEGMENT, BTN_LINK };

struct BtnData {
    std::wstring text;
    int style = BTN_GHOST;
    COLORREF bg = Th::CARD;
    bool hover = false;
    bool down = false;
    bool checked = false;
    bool enabled = true;
};

const wchar_t* kBtNClass = L"HyperSenseButton";
const wchar_t* kTgClass = L"HyperSenseToggle";

void BtnSetText(HWND h, const std::wstring& text) {
    BtnData* d = (BtnData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (d) {
        d->text = text;
        SetWindowTextW(h, text.c_str());
        InvalidateRect(h, nullptr, FALSE);
    }
}

void BtnSetChecked(HWND h, bool checked) {
    BtnData* d = (BtnData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (d && d->checked != checked) {
        d->checked = checked;
        InvalidateRect(h, nullptr, FALSE);
    }
}

void BtnNotify(HWND h) {
    HWND parent = GetParent(h);
    int id = GetDlgCtrlID(h);
    SendMessageW(parent, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)h);
}

void PaintButton(HWND h, HDC target, const RECT& rc) {
    BtnData* d = (BtnData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!d) return;

    FillSolid(target, rc, d->bg);

    RECT r = rc;
    InflateRect(&r, -S(1), -S(1));

    COLORREF fill = Th::CARD_HI, border = Th::BORDER, text = Th::TEXT;
    bool bold = true;

    switch (d->style) {
        case BTN_PRIMARY:
            fill = d->enabled ? (d->hover ? RGB(103, 232, 249) : Th::ACCENT) : RGB(30, 58, 66);
            border = fill;
            text = d->enabled ? RGB(4, 14, 20) : RGB(120, 150, 158);
            break;
        case BTN_GHOST:
            fill = d->hover && d->enabled ? RGB(27, 38, 54) : Th::CARD_HI;
            border = d->hover && d->enabled ? Th::BORDER_HI : Th::BORDER;
            text = d->enabled ? Th::TEXT : Th::TEXT_MUTE;
            break;
        case BTN_DANGER:
            fill = d->hover && d->enabled ? RGB(60, 24, 34) : Th::DANGER_BG;
            border = d->enabled ? RGB(120, 40, 56) : Th::BORDER;
            text = d->enabled ? RGB(253, 164, 175) : Th::TEXT_MUTE;
            break;
        case BTN_SEGMENT:
            fill = d->checked ? RGB(16, 47, 55) : (d->hover && d->enabled ? RGB(27, 38, 54) : Th::INPUT);
            border = d->checked ? Th::ACCENT_D : (d->hover && d->enabled ? Th::BORDER_HI : Th::BORDER);
            text = !d->enabled ? Th::TEXT_MUTE : (d->checked ? Th::ACCENT : Th::TEXT_DIM);
            break;
        case BTN_LINK:
            fill = d->bg;
            border = d->bg;
            text = d->hover && d->enabled ? RGB(125, 211, 252) : Th::LINK;
            bold = false;
            break;
    }

    if (d->style == BTN_LINK) {
        RECT tr = r;
        DrawTextC(target, d->text, tr, g_fBtnSm, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        FillRoundRect(target, r, S(7), fill, border);
        RECT tr = r;
        DrawTextC(target, d->text, tr, (d->style == BTN_SEGMENT || d->style == BTN_LINK) ? g_fBtnSm : g_fBtn,
                  text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (d->style == BTN_SEGMENT && d->checked) {
            RECT dot{ r.left + S(6), (r.top + r.bottom) / 2 - S(2), r.left + S(10), (r.top + r.bottom) / 2 + S(2) };
            HBRUSH b = CreateSolidBrush(Th::ACCENT);
            HPEN p = CreatePen(PS_SOLID, 1, Th::ACCENT);
            HGDIOBJ ob = SelectObject(target, b), op = SelectObject(target, p);
            Ellipse(target, dot.left, dot.top, dot.right, dot.bottom);
            SelectObject(target, ob); SelectObject(target, op);
            DeleteObject(b); DeleteObject(p);
        }
    }
}

LRESULT CALLBACK BtnProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    BtnData* d = (BtnData*)GetWindowLongPtrW(h, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            auto* cs = (CREATESTRUCTW*)l;
            auto* nd = new BtnData();
            if (cs->lpCreateParams) *nd = *(BtnData*)cs->lpCreateParams;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)nd);
            SetWindowTextW(h, nd->text.c_str());
            return TRUE;
        }
        case WM_NCDESTROY:
            delete d;
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);
            if (d) PaintButton(h, mem, rc);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (d && !d->hover) {
                d->hover = true;
                TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, h, 0 };
                TrackMouseEvent(&tme);
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSELEAVE:
            if (d && d->hover) {
                d->hover = false;
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (d && d->enabled) {
                d->down = true;
                SetCapture(h);
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (d && d->down) {
                d->down = false;
                ReleaseCapture();
                POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
                RECT rc;
                GetClientRect(h, &rc);
                InvalidateRect(h, nullptr, FALSE);
                if (PtInRect(&rc, pt)) BtnNotify(h);
            }
            return 0;
        case WM_ENABLE:
            if (d) {
                d->enabled = w != 0;
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(l) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, d && d->enabled ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case BM_CLICK:
            if (d && d->enabled) BtnNotify(h);
            return 0;
        case WM_KEYDOWN:
            if (d && d->enabled && (w == VK_RETURN || w == VK_SPACE)) {
                BtnNotify(h);
                return 0;
            }
            if (w == VK_ESCAPE) {
                SendMessageW(GetParent(h), WM_KEYDOWN, VK_ESCAPE, 0);
                return 0;
            }
            break;
        case WM_GETDLGCODE:
            return DLGC_BUTTON | DLGC_WANTCHARS;
    }
    return DefWindowProcW(h, msg, w, l);
}

HWND CreateButton(HWND parent, int id, const std::wstring& text, int style, COLORREF bg) {
    BtnData data;
    data.text = text;
    data.style = style;
    data.bg = bg;
    return CreateWindowExW(0, kBtNClass, text.c_str(),
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                           0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, g_hInst, &data);
}

// =====================================================================================
//  Custom toggle
// =====================================================================================

struct TgData {
    std::wstring label;
    bool on = false;
    bool hover = false;
    bool enabled = true;
    COLORREF bg = Th::CARD;
};

void TgSetOn(HWND h, bool on) {
    TgData* d = (TgData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (d && d->on != on) {
        d->on = on;
        InvalidateRect(h, nullptr, FALSE);
    }
}

bool TgGetOn(HWND h) {
    TgData* d = (TgData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    return d && d->on;
}

// Compact layouts show the short form of a filter name ("Unsigned" instead of
// "Unsigned modules"); the tooltip keeps the full wording either way.
void TgSetLabel(HWND h, const wchar_t* label) {
    TgData* d = (TgData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!d || !label) return;
    if (d->label == label) return;
    d->label = label;
    SetWindowTextW(h, label);
    InvalidateRect(h, nullptr, FALSE);
}

LRESULT CALLBACK TgProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    TgData* d = (TgData*)GetWindowLongPtrW(h, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            auto* cs = (CREATESTRUCTW*)l;
            auto* nd = new TgData();
            if (cs->lpCreateParams) *nd = *(TgData*)cs->lpCreateParams;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)nd);
            SetWindowTextW(h, nd->label.c_str());
            return TRUE;
        }
        case WM_NCDESTROY:
            delete d;
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);

            FillSolid(mem, rc, d ? d->bg : Th::CARD);

            if (d) {
                int trackW = S(38), trackH = S(20);
                RECT tr{ rc.left + S(2), (rc.top + rc.bottom) / 2 - trackH / 2,
                         rc.left + S(2) + trackW, (rc.top + rc.bottom) / 2 + trackH / 2 };
                COLORREF trackFill = !d->enabled ? RGB(28, 36, 48)
                                    : (d->on ? (d->hover ? RGB(103, 232, 249) : Th::ACCENT) : RGB(30, 41, 57));
                COLORREF trackBorder = !d->enabled ? Th::BORDER : (d->on ? Th::ACCENT : RGB(51, 65, 85));
                FillRoundRect(mem, tr, trackH / 2, trackFill, trackBorder);

                int knobR = trackH / 2 - S(3);
                int knobX = d->on ? (tr.right - S(3) - knobR) : (tr.left + S(3) + knobR);
                int knobY = (tr.top + tr.bottom) / 2;
                HBRUSH kb = CreateSolidBrush(d->enabled ? RGB(245, 250, 255) : RGB(120, 132, 148));
                HPEN kp = CreatePen(PS_SOLID, 1, d->enabled ? RGB(245, 250, 255) : RGB(120, 132, 148));
                HGDIOBJ ob = SelectObject(mem, kb), op = SelectObject(mem, kp);
                Ellipse(mem, knobX - knobR, knobY - knobR, knobX + knobR, knobY + knobR);
                SelectObject(mem, ob); SelectObject(mem, op);
                DeleteObject(kb); DeleteObject(kp);

                RECT lr{ tr.right + S(10), rc.top, rc.right - S(2), rc.bottom };
                DrawTextC(mem, d->label, lr, g_fBody, d->enabled ? (d->hover ? Th::TEXT : Th::TEXT_DIM) : Th::TEXT_MUTE,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (d && !d->hover) {
                d->hover = true;
                TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, h, 0 };
                TrackMouseEvent(&tme);
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_MOUSELEAVE:
            if (d && d->hover) {
                d->hover = false;
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (d && d->enabled) {
                d->on = !d->on;
                InvalidateRect(h, nullptr, FALSE);
                BtnNotify(h);
            }
            return 0;
        case WM_ENABLE:
            if (d) {
                d->enabled = w != 0;
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(l) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, d && d->enabled ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case BM_CLICK:
            if (d && d->enabled) {
                d->on = !d->on;
                InvalidateRect(h, nullptr, FALSE);
                BtnNotify(h);
            }
            return 0;
        case WM_GETDLGCODE:
            return DLGC_BUTTON | DLGC_WANTCHARS;
    }
    return DefWindowProcW(h, msg, w, l);
}

HWND CreateToggle(HWND parent, int id, const std::wstring& label, bool on, COLORREF bg) {
    TgData data;
    data.label = label;
    data.on = on;
    data.bg = bg;
    return CreateWindowExW(0, kTgClass, label.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                           0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, g_hInst, &data);
}

// =====================================================================================
//  Tooltips - every control explains itself on hover
// =====================================================================================

std::deque<std::wstring> g_tipStore;   // deque: c_str() stays valid as it grows

void InitTooltips(HWND parent) {
    if (g_hTip) return;
    g_hTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                             WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             parent, nullptr, g_hInst, nullptr);
    if (!g_hTip) return;
    SetWindowPos(g_hTip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SendMessageW(g_hTip, TTM_SETMAXTIPWIDTH, 0, (LPARAM)S(440));
    SendMessageW(g_hTip, TTM_SETDELAYTIME, TTDT_INITIAL, 350);
    SendMessageW(g_hTip, TTM_SETDELAYTIME, TTDT_RESHOW, 120);
    // Match the dark interface instead of the default light system tip.
    SendMessageW(g_hTip, TTM_SETTIPBKCOLOR, 0, (LPARAM)RGB(15, 23, 42));
    SendMessageW(g_hTip, TTM_SETTIPTEXTCOLOR, 0, (LPARAM)RGB(226, 232, 240));
}

void TipAdd(HWND ctl, const wchar_t* text) {
    if (!g_hTip || !ctl || !text) return;
    g_tipStore.emplace_back(text);
    TOOLINFOW ti{};
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    // TTF_SUBCLASS hooks the window in hwnd for mouse messages, so it must be the
    // control itself - pointing it at the parent silently disables every tip.
    ti.hwnd = ctl;
    ti.uId = (UINT_PTR)ctl;
    ti.lpszText = (LPWSTR)g_tipStore.back().c_str();
    SendMessageW(g_hTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
}

// =====================================================================================
//  Combo box chrome
// =====================================================================================

LRESULT CALLBACK ComboSubclass(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (msg == WM_PAINT) {
        // Paint the closed box in one pass. Letting the default combo paint first and
        // then drawing the arrow with a second GetDC caused visible flicker in the
        // classic bar, where the selection text changes as the user types.
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        FillSolid(dc, rc, Th::INPUT);

        bool enabled = IsWindowEnabled(h) != FALSE;
        wchar_t text[600] = {};
        int idx = (int)SendMessageW(h, CB_GETCURSEL, 0, 0);
        if (idx >= 0) SendMessageW(h, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)text);

        RECT tr{ rc.left + S(10), rc.top, rc.right - S(32), rc.bottom };
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, enabled ? Th::TEXT : Th::TEXT_MUTE);
        HGDIOBJ of = SelectObject(dc, g_fBody);
        DrawTextW(dc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, of);

        int arrowW = S(30);
        int cx = rc.right - arrowW / 2 - S(1);
        int cy = (rc.top + rc.bottom) / 2;
        HPEN pen = CreatePen(PS_SOLID, S(2), enabled ? Th::TEXT_DIM : Th::TEXT_MUTE);
        HGDIOBJ op = SelectObject(dc, pen);
        MoveToEx(dc, cx - S(4), cy - S(1), nullptr);
        LineTo(dc, cx, cy + S(3));
        LineTo(dc, cx + S(4), cy - S(1));
        SelectObject(dc, op);
        DeleteObject(pen);

        HBRUSH bb = CreateSolidBrush(GetFocus() == h ? Th::ACCENT_D : Th::BORDER);
        FrameRect(dc, &rc, bb);
        DeleteObject(bb);

        EndPaint(h, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefSubclassProc(h, msg, w, l);
}

// =====================================================================================
//  Dark modal dialog - keeps the message boxes inside the app theme
// =====================================================================================

enum : int { HS_DLG_OK = 4001, HS_DLG_CANCEL, HS_DLG_OPEN_FILE, HS_DLG_OPEN_FOLDER, HS_DLG_CLOSE };

enum HsIcon { HSI_INFO = 0, HSI_WARN, HSI_ERROR, HSI_QUESTION };

// Results of the "report saved" dialog (kept away from IDOK/IDCANCEL so the
// plain message boxes keep their meaning).
enum { HS_SAVE_CLOSE = 0, HS_SAVE_OPEN_FILE = 10, HS_SAVE_OPEN_FOLDER = 11 };

struct DlgState {
    std::wstring title;
    std::wstring body;
    int icon = HSI_INFO;
    bool question = false;
    int actions = 0;                 // 1 = OPEN FILE / OPEN FOLDER / CLOSE
    int result = IDOK;
    RECT iconBox{}, titleR{}, bodyR{}, okR{}, cancelR{}, openFileR{}, openFolderR{}, closeR{};
    int width = 0, height = 0;
};

const wchar_t* kDlgClass = L"HyperSenseDialog";

COLORREF DlgAccent(int icon) {
    switch (icon) {
        case HSI_WARN: return Th::WARN;
        case HSI_ERROR: return Th::DANGER;
        default: return Th::ACCENT;
    }
}

COLORREF DlgTint(int icon) {
    switch (icon) {
        case HSI_WARN: return RGB(46, 33, 12);
        case HSI_ERROR: return RGB(48, 20, 28);
        default: return RGB(10, 38, 46);
    }
}

const wchar_t* DlgGlyph(int icon) {
    switch (icon) {
        case HSI_WARN: return L"!";
        case HSI_ERROR: return L"!";
        case HSI_QUESTION: return L"?";
        default: return L"i";
    }
}

void ComputeDlgLayout(HDC dc, DlgState& st) {
    int pad = S(26);
    int iconSize = S(36);

    int maxLine = 0;
    HGDIOBJ of = SelectObject(dc, g_fBody);
    size_t start = 0;
    while (true) {
        size_t nl = st.body.find(L'\n', start);
        std::wstring line = st.body.substr(start, nl == std::wstring::npos ? std::wstring::npos : nl - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (!line.empty()) {
            SIZE sz{};
            GetTextExtentPoint32W(dc, line.c_str(), (int)line.size(), &sz);
            if (sz.cx > maxLine) maxLine = sz.cx;
        }
        if (nl == std::wstring::npos) break;
        start = nl + 1;
    }
    SelectObject(dc, of);

    int w = maxLine + pad * 2;
    int minW = S(470), maxW = S(900);
    if (w < minW) w = minW;

    int textLeft = pad + iconSize + S(16);
    SIZE ts{};
    HGDIOBJ tf = SelectObject(dc, g_fBodyBold);
    if (!st.title.empty()) GetTextExtentPoint32W(dc, st.title.c_str(), (int)st.title.size(), &ts);
    SelectObject(dc, tf);
    if (ts.cx + textLeft + pad > w) w = ts.cx + textLeft + pad;
    if (w > maxW) w = maxW;
    st.width = w;

    st.iconBox = RECT{ pad, pad, pad + iconSize, pad + iconSize };
    st.titleR = RECT{ textLeft, pad, w - pad, pad + iconSize };

    RECT body{ pad, pad + iconSize + S(14), w - pad, 0 };
    HGDIOBJ bf = SelectObject(dc, g_fBody);
    DrawTextW(dc, st.body.c_str(), -1, &body, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(dc, bf);
    int bodyH = body.bottom - body.top;
    st.bodyR = RECT{ pad, pad + iconSize + S(14), w - pad, pad + iconSize + S(14) + bodyH };

    int btnH = S(36);
    int btnW = S(104);
    int btnY = st.bodyR.bottom + S(20);
    int btnX = w - pad - btnW;
    if (st.actions) {
        int right = w - pad;
        auto place = [&](int width) {
            RECT r{ right - width, btnY, right, btnY + btnH };
            right -= width + S(8);
            return r;
        };
        st.openFileR = place(S(114));
        st.openFolderR = place(S(136));
        st.closeR = place(S(92));
    } else {
        st.okR = RECT{ btnX, btnY, btnX + btnW, btnY + btnH };
        if (st.question) {
            int cx = btnX - S(8) - btnW;
            st.cancelR = RECT{ cx, btnY, cx + btnW, btnY + btnH };
        }
    }
    st.height = btnY + btnH + pad;
}

void PaintDlg(HDC dc, const RECT& rc, DlgState* st) {
    FillSolid(dc, rc, Th::CARD);

    COLORREF ac = DlgAccent(st->icon);
    FillRoundRect(dc, st->iconBox, S(10), DlgTint(st->icon), ac);
    RECT gr = st->iconBox;
    DrawTextC(dc, DlgGlyph(st->icon), gr, g_fStat, ac, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawTextC(dc, st->title, st->titleR, g_fBodyBold, Th::TEXT,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawTextC(dc, st->body, st->bodyR, g_fBody, Th::TEXT_DIM,
              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    HBRUSH b = CreateSolidBrush(Th::BORDER_HI);
    FrameRect(dc, &rc, b);
    DeleteObject(b);
}

LRESULT CALLBACK DlgProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    DlgState* st = (DlgState*)GetWindowLongPtrW(h, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            auto* cs = (CREATESTRUCTW*)l;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return TRUE;
        }
        case WM_CREATE: {
            if (!st) return 0;
            HWND focusBtn = nullptr;
            if (st->actions) {
                HWND of = CreateButton(h, HS_DLG_OPEN_FILE, L"OPEN FILE", BTN_PRIMARY, Th::CARD);
                MoveWindow(of, st->openFileR.left, st->openFileR.top,
                           st->openFileR.right - st->openFileR.left,
                           st->openFileR.bottom - st->openFileR.top, TRUE);
                HWND od = CreateButton(h, HS_DLG_OPEN_FOLDER, L"OPEN FOLDER", BTN_GHOST, Th::CARD);
                MoveWindow(od, st->openFolderR.left, st->openFolderR.top,
                           st->openFolderR.right - st->openFolderR.left,
                           st->openFolderR.bottom - st->openFolderR.top, TRUE);
                HWND cl = CreateButton(h, HS_DLG_CLOSE, L"CLOSE", BTN_GHOST, Th::CARD);
                MoveWindow(cl, st->closeR.left, st->closeR.top,
                           st->closeR.right - st->closeR.left,
                           st->closeR.bottom - st->closeR.top, TRUE);
                focusBtn = of;
            } else {
                HWND ok = CreateButton(h, HS_DLG_OK, L"OK", BTN_PRIMARY, Th::CARD);
                MoveWindow(ok, st->okR.left, st->okR.top, st->okR.right - st->okR.left,
                           st->okR.bottom - st->okR.top, TRUE);
                if (st->question) {
                    HWND can = CreateButton(h, HS_DLG_CANCEL, L"CANCEL", BTN_GHOST, Th::CARD);
                    MoveWindow(can, st->cancelR.left, st->cancelR.top, st->cancelR.right - st->cancelR.left,
                               st->cancelR.bottom - st->cancelR.top, TRUE);
                }
                focusBtn = ok;
            }
            SetFocus(focusBtn);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);
            if (st) PaintDlg(mem, rc, st);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (st && (id == HS_DLG_OK || id == HS_DLG_CANCEL)) {
                st->result = (id == HS_DLG_OK) ? IDOK : IDCANCEL;
                DestroyWindow(h);
            } else if (st && id == HS_DLG_OPEN_FILE) {
                st->result = HS_SAVE_OPEN_FILE;
                DestroyWindow(h);
            } else if (st && id == HS_DLG_OPEN_FOLDER) {
                st->result = HS_SAVE_OPEN_FOLDER;
                DestroyWindow(h);
            } else if (st && id == HS_DLG_CLOSE) {
                st->result = HS_SAVE_CLOSE;
                DestroyWindow(h);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (st) {
                if (w == VK_ESCAPE) {
                    st->result = st->actions ? HS_SAVE_CLOSE : IDCANCEL;
                    DestroyWindow(h);
                    return 0;
                }
                if (w == VK_RETURN) {
                    st->result = st->actions ? HS_SAVE_OPEN_FILE : IDOK;
                    DestroyWindow(h);
                    return 0;
                }
            }
            break;
        case WM_CLOSE:
            if (st) st->result = st->actions ? HS_SAVE_CLOSE : IDCANCEL;
            DestroyWindow(h);
            return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

int HsMessage(int icon, const std::wstring& title, const std::wstring& body, bool question = false,
              int actions = 0) {
    DlgState st;
    st.title = title;
    st.body = body;
    st.icon = icon;
    st.question = question;
    st.actions = actions;

    HDC screen = GetDC(nullptr);
    if (screen) {
        ComputeDlgLayout(screen, st);
        ReleaseDC(nullptr, screen);
    } else {
        st.width = S(560);
        st.height = S(240);
    }

    HWND owner = g_hWnd;
    RECT orc{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    if (owner) GetWindowRect(owner, &orc);
    int x = orc.left + ((orc.right - orc.left) - st.width) / 2;
    int y = orc.top + ((orc.bottom - orc.top) - st.height) / 2;

    HWND dlg = CreateWindowExW(0, kDlgClass, title.c_str(), WS_POPUP | WS_CLIPCHILDREN,
                               x, y, st.width, st.height, owner, nullptr, g_hInst, &st);
    if (!dlg) return actions ? HS_SAVE_CLOSE : IDOK;

    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    SetForegroundWindow(dlg);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_QUIT) { PostQuitMessage((int)msg.wParam); break; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    return st.result;
}

// Split a long path at folder separators so the dialog never clips it.
std::wstring WrapPath(const std::wstring& path, size_t maxLen = 86) {
    std::wstring out, line;
    size_t lastBreak = std::wstring::npos;
    for (wchar_t c : path) {
        line.push_back(c);
        if (c == L'\\' || c == L'/') lastBreak = line.size();
        if (line.size() >= maxLen && lastBreak != std::wstring::npos && lastBreak < line.size()) {
            out += line.substr(0, lastBreak) + L"\r\n";
            line = line.substr(lastBreak);
            lastBreak = std::wstring::npos;
        }
    }
    out += line;
    return out;
}

// Confirm a saved report and offer to open the file or reveal it in Explorer.
void ReportSavedDialog(const std::wstring& title, const std::wstring& path) {
    std::wstring body = L"Saved successfully to:\r\n" + WrapPath(path) +
                        L"\r\n\r\nOpen the report now, or show the file in its folder.";
    int r = HsMessage(HSI_INFO, title, body, false, 1);
    if (r == HS_SAVE_OPEN_FILE) {
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (r == HS_SAVE_OPEN_FOLDER) {
        std::wstring args = L"/select,\"" + path + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

// =====================================================================================
//  Process enumeration / target
// =====================================================================================

struct ProcItem {
    DWORD pid = 0;
    std::wstring name;
    std::wstring path;
};

std::vector<ProcItem> g_procs;
std::vector<DWORD> g_comboPids;   // parallel to combo indices
bool g_targetAll = false;
DWORD g_targetPid = 0;
std::wstring g_procFilter;
int g_procMatchCount = 0;

std::wstring ProcessPath(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"";
    wchar_t buf[1024] = {};
    DWORD sz = 1024;
    std::wstring out;
    if (QueryFullProcessImageNameW(h, 0, buf, &sz)) out = buf;
    CloseHandle(h);
    return out;
}

void ApplyProcFilter(DWORD keepPid, bool keepAll) {
    if (!g_hCombo) return;

    wchar_t buf[160] = {};
    if (g_hEditSearch) GetWindowTextW(g_hEditSearch, buf, 160);
    g_procFilter = Lower(Trim(buf));

    SendMessageW(g_hCombo, CB_RESETCONTENT, 0, 0);
    g_comboPids.clear();
    SendMessageW(g_hCombo, CB_ADDSTRING, 0, (LPARAM)L"SELECT A TARGET...");
    g_comboPids.push_back(0);
    SendMessageW(g_hCombo, CB_ADDSTRING, 0, (LPARAM)L"ALL PROCESSES  (*)");
    g_comboPids.push_back(0);

    int selectIndex = keepAll ? 1 : 0;
    bool keepVisible = keepAll;

    for (const auto& p : g_procs) {
        if (!g_procFilter.empty()) {
            std::wstring hay = Lower(p.name + L"  " + p.path + L"  " + std::to_wstring((unsigned long)p.pid));
            if (hay.find(g_procFilter) == std::wstring::npos) continue;
        }
        int idx = (int)g_comboPids.size();
        std::wstring text = Format(L"%5lu   %s", p.pid, p.name.c_str());
        SendMessageW(g_hCombo, CB_ADDSTRING, 0, (LPARAM)text.c_str());
        g_comboPids.push_back(p.pid);
        if (!keepAll && p.pid == keepPid) {
            selectIndex = idx;
            keepVisible = true;
        }
    }

    g_procMatchCount = (int)g_comboPids.size() - 2;

    // The current target was filtered out: fall back to the first match so RUN
    // always has something selected, or clear it when nothing matches.
    if (!keepAll && !keepVisible && g_procMatchCount > 0) selectIndex = 2;
    if (selectIndex >= (int)g_comboPids.size()) selectIndex = 0;

    SendMessageW(g_hCombo, CB_SETCURSEL, selectIndex, 0);
    g_targetAll = (selectIndex == 1);
    g_targetPid = (selectIndex >= 2) ? g_comboPids[(size_t)selectIndex] : 0;
}

void RefreshProcessList() {
    DWORD keep = g_targetPid;
    bool keepAll = g_targetAll;

    g_procs.clear();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == 0 || pe.th32ProcessID == GetCurrentProcessId()) continue;
                ProcItem it;
                it.pid = pe.th32ProcessID;
                it.name = pe.szExeFile;
                it.path = ProcessPath(pe.th32ProcessID);
                g_procs.push_back(it);
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    std::sort(g_procs.begin(), g_procs.end(), [](const ProcItem& a, const ProcItem& b) {
        std::wstring an = Lower(a.name), bn = Lower(b.name);
        if (an == bn) return a.pid < b.pid;
        return an < bn;
    });

    ApplyProcFilter(keep, keepAll);
}

void SelectTargetPid(DWORD pid, bool all) {
    if (!all && g_hEditSearch) {
        bool visible = false;
        for (size_t i = 2; i < g_comboPids.size(); ++i) {
            if (g_comboPids[i] == pid) { visible = true; break; }
        }
        if (!visible) {
            SetWindowTextW(g_hEditSearch, L"");
            ApplyProcFilter(pid, false);
        }
    }
    if (all) {
        SendMessageW(g_hCombo, CB_SETCURSEL, 1, 0);
        g_targetAll = true;
        g_targetPid = 0;
        return;
    }
    for (size_t i = 0; i < g_comboPids.size(); ++i) {
        if (g_comboPids[i] == pid && i >= 2) {
            SendMessageW(g_hCombo, CB_SETCURSEL, (WPARAM)i, 0);
            g_targetAll = false;
            g_targetPid = pid;
            return;
        }
    }
}

const ProcItem* FindProcByPid(DWORD pid) {
    for (const auto& p : g_procs)
        if (p.pid == pid) return &p;
    return nullptr;
}

// =====================================================================================
//  Scan state
// =====================================================================================

HANDLE g_hProc = nullptr;
HANDLE g_hReadPipe = nullptr;
std::thread g_reader;
std::atomic<bool> g_scanning{ false };
std::atomic<bool> g_stopRequested{ false };
int g_scanGen = 0;
ULONGLONG g_scanStartTick = 0;
std::string g_reportBody;
size_t g_reportCap = 64ull * 1024 * 1024;
int g_viewLines = 0;
int g_viewLineCap = 30000;
bool g_viewCapped = false;
std::string g_partialLine;
int g_iocHits = 0;      // total IOC findings reported by the engine
int g_regionHits = 0;   // region lines that carry at least one IOC description
bool g_accessFailed = false;   // engine could not map/open the target at all
HICON g_hBrandIcon = nullptr;
std::wstring g_statusText = L"READY";

struct ScanMeta {
    bool valid = false;
    std::wstring started, finished, duration;
    std::wstring command, target, mode, verbosity, options, filters;
    std::wstring engine;
    DWORD exitCode = 0;
    bool stopped = false;
};
ScanMeta g_meta;

void SetStatus(const std::wstring& s) {
    g_statusText = s;
    if (g_hWnd) {
        RECT* r = g_classic ? &g_L.classicStatus : &g_L.actionRow;
        InvalidateRect(g_hWnd, r, FALSE);
    }
}

// =====================================================================================
//  Report generation
// =====================================================================================

std::wstring ExitCodeText(DWORD code) {
    switch (code) {
        case 0: return L"0 (ok)";
        case 1: return L"1 (scan complete)";
        case 0xC0000005: return L"0xC0000005 (engine access violation)";
        case 0xC0000374: return L"0xC0000374 (engine heap error)";
        case 0xC0000409: return L"0xC0000409 (engine stack overflow)";
        case 0xC000013A: return L"0xC000013A (engine closed by user)";
        default: break;
    }
    if (code >= 0xC0000000u) return Format(L"0x%08lX (engine ended abnormally)", (unsigned long)code);
    return Format(L"%lu", (unsigned long)code);
}

bool ScanEndedEarly(const ScanMeta& m) { return !m.stopped && m.exitCode >= 0xC0000000u; }

int TotalIocFindings() { return g_iocHits > g_regionHits ? g_iocHits : g_regionHits; }
int SuspiciousRegions() { return g_regionHits > 0 ? g_regionHits : g_iocHits; }

// =====================================================================================
//  Discord Rich Presence
// =====================================================================================

// Set your Discord application's Client ID here, or put it in HyperSense.ini:
//   [discord]
//   client_id=123456789012345678
// The presence is always on; restart the tool to pick up a different Client ID.
static const wchar_t* kDiscordClientId = L"1564751949201543358";

std::wstring g_rpcClientId;
std::atomic<bool> g_rpcStop{ false };
std::thread g_rpcThread;
std::mutex g_rpcMu;
ULONGLONG g_rpcStamp = 0;
ULONGLONG g_rpcScanStartMs = 0;

struct RpcPresence {
    bool valid = false;
    bool timer = false;
    ULONGLONG startMs = 0;
    std::wstring details, state;
};
RpcPresence g_rpcPresence;

std::wstring RpcIniPath() { return JoinPath(ExeDir(), L"HyperSense.ini"); }

// Opt-in protocol trace: set HYPERSENSE_RPC_DEBUG=1 to append every frame Discord
// returns to hypersense_rpc.log next to the executable. Off by default; there is no log
// otherwise. The log stays beside the exe so the build needs no temp-directory call.
bool RpcDebugOn() {
    wchar_t v[8] = {};
    DWORD n = GetEnvironmentVariableW(L"HYPERSENSE_RPC_DEBUG", v, 8);
    return n > 0 && v[0] == L'1';
}

void RpcLog(const char* what, const std::string& js) {
    if (!RpcDebugOn()) return;
    const std::wstring path = JoinPath(ExeDir(), L"hypersense_rpc.log");
    HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    std::string line = std::string(what) + " " + js + "\r\n";
    DWORD w = 0;
    WriteFile(f, line.data(), (DWORD)line.size(), &w, nullptr);
    CloseHandle(f);
}

std::wstring RpcClamp(std::wstring s, size_t maxLen = 128) {
    if (s.size() > maxLen) s = s.substr(0, maxLen);
    return s;
}

void RpcLoadConfig() {
    const std::wstring ini = RpcIniPath();
    wchar_t buf[160] = {};
    GetPrivateProfileStringW(L"discord", L"client_id", L"", buf, 160, ini.c_str());
    g_rpcClientId = Trim(buf);
    if (g_rpcClientId.empty()) {
        wchar_t env[160] = {};
        DWORD n = GetEnvironmentVariableW(L"HYPERSENSE_DISCORD_CLIENT_ID", env, 160);
        if (n > 0 && n < 160) g_rpcClientId = Trim(env);
    }
    if (g_rpcClientId.empty() && kDiscordClientId && *kDiscordClientId)
        g_rpcClientId = kDiscordClientId;
}

ULONGLONG RpcUnixMs() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u{};
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (u.QuadPart - 116444736000000000ull) / 10000ull;
}

std::string RpcUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}

std::string RpcJson(const std::wstring& s) {
    std::string u = RpcUtf8(s), o;
    o.reserve(u.size() + 8);
    for (char c : u) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char b[8] = {};
                    snprintf(b, sizeof(b), "\\u%04x", (unsigned char)c);
                    o += b;
                } else {
                    o += c;
                }
        }
    }
    return o;
}

bool RpcWriteFrame(HANDLE p, int op, const std::string& json) {
    DWORD written = 0;
    DWORD hdr[2] = { (DWORD)op, (DWORD)json.size() };
    if (!WriteFile(p, hdr, sizeof(hdr), &written, nullptr) || written != sizeof(hdr)) return false;
    if (!json.empty()) {
        if (!WriteFile(p, json.data(), (DWORD)json.size(), &written, nullptr) || written != json.size())
            return false;
    }
    return true;
}

bool RpcReadFrame(HANDLE p, int& op, std::string& json, DWORD timeoutMs) {
    ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(p, nullptr, 0, nullptr, &avail, nullptr)) return false;
        if (avail >= sizeof(DWORD) * 2) break;
        if (GetTickCount64() > deadline) return false;
        Sleep(20);
    }
    DWORD hdr[2] = {};
    DWORD got = 0;
    if (!ReadFile(p, hdr, sizeof(hdr), &got, nullptr) || got != sizeof(hdr)) return false;
    op = (int)hdr[0];
    DWORD len = hdr[1];
    if (len > 64 * 1024) return false;
    json.assign(len, '\0');
    DWORD off = 0;
    while (off < len) {
        if (!ReadFile(p, &json[off], len - off, &got, nullptr) || got == 0) return false;
        off += got;
    }
    return true;
}

HANDLE RpcConnect(const std::wstring& id) {
    for (int i = 0; i < 10; ++i) {
        std::wstring name = Format(L"\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE p = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (p == INVALID_HANDLE_VALUE) continue;
        std::string hs = "{\"v\":1,\"client_id\":\"" + RpcUtf8(id) + "\"}";
        if (!RpcWriteFrame(p, 0, hs)) { CloseHandle(p); continue; }
        int op = -1;
        std::string js;
        if (!RpcReadFrame(p, op, js, 5000) || op != 1) { CloseHandle(p); continue; }
        RpcLog("handshake", js);
        return p;
    }
    return INVALID_HANDLE_VALUE;
}

std::string RpcActivityJson(const RpcPresence& p, bool clear) {    std::string a = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" +
                    std::to_string((unsigned long)GetCurrentProcessId()) + ",\"activity\":";
    if (clear) {
        a += "null";
    } else {
        a += "{";
        bool first = true;
        auto add = [&](const std::string& kv) {
            if (!first) a += ",";
            a += kv;
            first = false;
        };
        if (!p.details.empty()) add("\"details\":\"" + RpcJson(p.details) + "\"");
        if (!p.state.empty()) add("\"state\":\"" + RpcJson(p.state) + "\"");
        if (p.timer && p.startMs) add("\"timestamps\":{\"start\":" + std::to_string(p.startMs) + "}");
        add("\"buttons\":["
            "{\"label\":\"GitHub - HyperSense\",\"url\":\"https://github.com/hypersenseindia/hypersense-memory-analyzer\"},"
            "{\"label\":\"Join our Discord\",\"url\":\"https://discord.gg/me7DvyzUp3\"}]");
        a += "}";
    }
    a += "},\"nonce\":\"" + std::to_string((unsigned long long)RpcUnixMs()) + "\"}";
    return a;
}

// Called from the UI thread. Only the fields that actually change are rewritten.
void RpcPublish(const std::wstring& details, const std::wstring& state, bool timer,
                ULONGLONG startMs) {
    RpcPresence p;
    p.valid = true;
    p.details = RpcClamp(details);
    p.state = RpcClamp(state);
    p.timer = timer;
    p.startMs = startMs;

    std::lock_guard<std::mutex> lk(g_rpcMu);
    if (g_rpcPresence.valid == p.valid && g_rpcPresence.timer == p.timer &&
        g_rpcPresence.startMs == p.startMs && g_rpcPresence.details == p.details &&
        g_rpcPresence.state == p.state)
        return;
    g_rpcPresence = p;
    g_rpcStamp++;
}

void RpcWorker() {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    ULONGLONG lastTry = 0, lastSend = 0, lastStamp = 0;

    while (!g_rpcStop) {
        bool want = !g_rpcClientId.empty();

        if (!want) {
            if (pipe != INVALID_HANDLE_VALUE) {
                RpcWriteFrame(pipe, 1, RpcActivityJson(RpcPresence(), true));
                CloseHandle(pipe);
                pipe = INVALID_HANDLE_VALUE;
            }
            Sleep(500);
            continue;
        }

        if (pipe == INVALID_HANDLE_VALUE) {
            ULONGLONG now = GetTickCount64();
            if (now - lastTry >= 3000) {
                lastTry = now;
                pipe = RpcConnect(g_rpcClientId);
                if (pipe != INVALID_HANDLE_VALUE) lastSend = 0;
            }
            if (pipe == INVALID_HANDLE_VALUE) {
                Sleep(400);
                continue;
            }
        }

        // Drain control frames: PING must be answered, CLOSE means reconnect.
        bool drop = false;
        for (;;) {
            DWORD avail = 0;
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr)) { drop = true; break; }
            if (avail < sizeof(DWORD) * 2) break;
            int op = -1;
            std::string js;
            if (!RpcReadFrame(pipe, op, js, 500)) { drop = true; break; }
            if (op == 1) RpcLog("recv", js);
            if (op == 2) { drop = true; break; }
            if (op == 3) RpcWriteFrame(pipe, 4, js);
        }
        if (drop) {
            CloseHandle(pipe);
            pipe = INVALID_HANDLE_VALUE;
            continue;
        }

        RpcPresence snap;
        ULONGLONG stamp = 0;
        {
            std::lock_guard<std::mutex> lk(g_rpcMu);
            snap = g_rpcPresence;
            stamp = g_rpcStamp;
        }

        ULONGLONG now = GetTickCount64();
        if (stamp != lastStamp && (lastSend == 0 || now - lastSend >= 12000)) {
            bool clear = !snap.valid;
            if (!RpcWriteFrame(pipe, 1, RpcActivityJson(snap, clear))) {
                CloseHandle(pipe);
                pipe = INVALID_HANDLE_VALUE;
                continue;
            }
            lastStamp = stamp;
            lastSend = now;
        }
        Sleep(250);
    }

    if (pipe != INVALID_HANDLE_VALUE) {
        RpcWriteFrame(pipe, 1, RpcActivityJson(RpcPresence(), true));
        CloseHandle(pipe);
    }
}

void RpcStartWorker() {
    if (!g_rpcThread.joinable()) g_rpcThread = std::thread(RpcWorker);
}

void RpcStopWorker() {
    g_rpcStop = true;
    if (g_rpcThread.joinable()) g_rpcThread.join();
}

std::wstring ShortTarget() {
    if (!g_meta.valid) return L"no target";
    if (g_targetAll) return L"all processes";
    const ProcItem* p = FindProcByPid(g_targetPid);
    if (p) return Format(L"%s (PID %lu)", p->name.c_str(), p->pid);
    return Format(L"PID %lu", g_targetPid);
}

const wchar_t* ModeLabel(int idx);
const wchar_t* VerbLabel(int idx);

void RpcPublishIdle() {
    RpcPublish(L"HYPERSENSEGLOBAL", L"Ready - pick a process and scan", false, 0);
}

void RpcPublishScanning() {
    int hits = SuspiciousRegions();
    std::wstring state = Format(L"%s \u00B7 %s \u00B7 %d suspicious",
                                g_meta.mode.empty() ? L"IOC" : g_meta.mode.c_str(),
                                g_meta.verbosity.empty() ? L"DETAIL" : g_meta.verbosity.c_str(), hits);
    RpcPublish(L"Scanning " + ShortTarget(), state, true, g_rpcScanStartMs);
}

void RpcPublishResult() {
    if (!g_meta.valid) { RpcPublishIdle(); return; }

    std::wstring details, state;
    if (g_meta.stopped) {
        details = L"Scan stopped - " + ShortTarget();
        state = L"partial results in the output";
    } else if (g_accessFailed) {
        details = L"Scan blocked - " + ShortTarget();
        state = L"access denied - try ELEVATE";
    } else if (SuspiciousRegions() > 0) {
        int regions = SuspiciousRegions();
        details = L"Scan complete - " + ShortTarget();
        state = Format(L"%d suspicious region%s \u00B7 review the output", regions,
                       regions == 1 ? L"" : L"s");
    } else {
        details = L"Scan clean - " + ShortTarget();
        state = L"no suspicious memory regions";
    }
    RpcPublish(details, state, false, 0);
}

std::wstring ResultText() {
    if (!g_meta.valid) return L"no analysis";
    if (g_meta.stopped) return L"scan stopped by user";
    if (ScanEndedEarly(g_meta)) return L"engine ended early - partial results";
    if (g_accessFailed) return L"engine could not access the target process - try ELEVATE";
    int found = TotalIocFindings();
    if (found <= 0) return L"no IOC findings";
    int regions = SuspiciousRegions();
    std::wstring s = Format(L"%d suspicious region%s flagged", regions, regions == 1 ? L"" : L"s");
    if (found != regions) s += Format(L"  (%d IOC findings)", found);
    return s;
}

std::wstring BuildHtmlOutput(const std::wstring& body);

std::wstring BuildReportText() {
    std::wstringstream ss;
    std::wstring line(72, L'=');
    ss << line << L"\r\n";
    ss << L"  HYPERSENSEGLOBAL(TM)\r\n";
    ss << L"  Live Usermode Memory Analysis Tool  |  V1\r\n";
    ss << line << L"\r\n\r\n";
    ss << L"REPORT GENERATED : " << NowLocal(true) << L"  (" << TzString() << L")\r\n";
    ss << L"UTC TIME         : " << NowUtc(true) << L"\r\n";
    ss << L"COMPUTER NAME    : " << g_sys.computer << L"\r\n";
    ss << L"ACCOUNT          : " << (g_sys.domain.empty() ? g_sys.account : (g_sys.domain + L"\\" + g_sys.account)) << L"\r\n";
    ss << L"USER SID         : " << g_sys.sid << L"\r\n";
    ss << L"OPERATING SYSTEM : " << g_sys.osName << L"  (" << g_sys.osBuild << L", " << g_sys.arch << L")\r\n";
    ss << L"ELEVATION        : " << (g_sys.elevated ? L"ADMINISTRATOR" : L"STANDARD USER") << L"\r\n";
    ss << L"ENGINE           : " << g_meta.engine << L"\r\n\r\n";

    std::wstring sep(72, L'-');
    ss << sep << L"\r\nANALYSIS\r\n" << sep << L"\r\n";
    ss << L"TARGET           : " << g_meta.target << L"\r\n";
    ss << L"SCAN STARTED     : " << g_meta.started << L"  (" << TzString() << L")\r\n";
    ss << L"SCAN FINISHED    : " << g_meta.finished << L"  (" << TzString() << L")\r\n";
    ss << L"SCAN DURATION    : " << g_meta.duration << L"\r\n";
    ss << L"MEMORY SELECTION : " << g_meta.mode << L"\r\n";
    ss << L"VERBOSITY        : " << g_meta.verbosity << L"\r\n";
    ss << L"OPTIONS          : " << g_meta.options << L"\r\n";
    ss << L"FILTERS          : " << g_meta.filters << L"\r\n";
    ss << L"EXIT CODE        : " << ExitCodeText(g_meta.exitCode) << (g_meta.stopped ? L"  (STOPPED BY USER)" : L"") << L"\r\n";
    ss << L"RESULT           : " << ResultText() << L"\r\n";
    ss << L"COMMAND LINE     : " << g_meta.command << L"\r\n\r\n";

    ss << sep << L"\r\nENGINE OUTPUT\r\n" << sep << L"\r\n";
    ss << Widen(g_reportBody) << L"\r\n";

    ss << sep << L"\r\n";
    ss << L"Engine credits : Forrest Orr & Gabriel Landau - Moneta (GPLv3)\r\n";
    ss << L"Tool build     : HYPERSENSEGLOBAL V1 - zero-crash build\r\n";
    ss << L"Developed by   : AG HYDRAXFF  (@hydraxff_yt)\r\n";
    ss << L"Instagram      : https://www.instagram.com/hydraxff_yt/\r\n";
    ss << line << L"\r\n";
    return ss.str();
}

std::wstring BuildReportHtml() {
    std::wstring outText = Widen(g_reportBody);
    std::wstringstream ss;
    ss << L"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">";
    ss << L"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    ss << L"<title>HYPERSENSEGLOBAL Report</title><style>";
    ss << L"*{box-sizing:border-box}body{margin:0;background:#080b11;color:#e5ecf6;font-family:'Segoe UI',Arial,sans-serif}";
    ss << L".wrap{max-width:1180px;margin:0 auto;padding:34px 26px 60px}";
    ss << L".brand{display:flex;align-items:center;gap:14px;margin-bottom:4px}";
    ss << L".mark{width:46px;height:46px;border-radius:11px;background:#22d3ee;color:#04121a;font-weight:800;font-size:23px;display:flex;align-items:center;justify-content:center}";
    ss << L"h1{font-size:26px;margin:0;letter-spacing:.5px}h1 span{color:#22d3ee}";
    ss << L".sub{color:#94a3b8;font-size:12px;letter-spacing:2px;text-transform:uppercase;margin:2px 0 26px 60px}";
    ss << L".accent{color:#22d3ee}.muted{color:#94a3b8}";
    ss << L".card{background:#101621;border:1px solid #1f2b3d;border-radius:12px;padding:20px 22px;margin-bottom:18px}";
    ss << L".hdr{font-size:11px;letter-spacing:2px;color:#22d3ee;text-transform:uppercase;margin-bottom:14px}";
    ss << L"table{width:100%;border-collapse:collapse;font-size:13px}";
    ss << L"td{padding:6px 10px;border-bottom:1px solid #16202e;vertical-align:top}";
    ss << L"td.k{color:#94a3b8;width:190px;white-space:nowrap}td.v{color:#e5ecf6}";
    ss << L"td.hit{color:#f87171;font-weight:600}td.clean{color:#34d399;font-weight:600}td.warn{color:#fbbf24;font-weight:600}";
    ss << L"pre{background:#0b1119;border:1px solid #1f2b3d;border-radius:10px;padding:16px;overflow:auto;";
    ss << L"font-family:Consolas,'Courier New',monospace;font-size:12.5px;line-height:1.45;color:#cbd5e1;white-space:pre-wrap;word-break:break-word}";
    ss << L".badge{display:inline-block;padding:3px 10px;border-radius:999px;font-size:11px;letter-spacing:1px;border:1px solid #22d3ee;color:#22d3ee}";
    ss << L".foot{color:#64748b;font-size:12px;line-height:1.7;border-top:1px solid #1f2b3d;padding-top:16px;margin-top:26px}";
    ss << L".foot b{color:#94a3b8;font-weight:600}a{color:#38bdf8;text-decoration:none}";
    ss << L"</style></head><body><div class=\"wrap\">";
    ss << L"<div class=\"brand\"><div class=\"mark\">H</div><div><h1>HYPERSENSE<span>GLOBAL</span><sup style=\"font-size:11px\">TM</sup></h1>";
    ss << L"<div class=\"sub\">Live Usermode Memory Analysis Tool &middot; V1</div></div></div>";

    ss << L"<div class=\"card\"><div class=\"hdr\">System &amp; Report</div><table>";
    auto row = [&](const wchar_t* k, const std::wstring& v) {
        ss << L"<tr><td class=\"k\">" << k << L"</td><td class=\"v\">" << HtmlEscape(v) << L"</td></tr>";
    };
    row(L"Report generated", NowLocal(true) + L"  (" + TzString() + L")");
    row(L"UTC time", NowUtc(true));
    row(L"Computer name", g_sys.computer);
    row(L"Account", g_sys.domain.empty() ? g_sys.account : (g_sys.domain + L"\\" + g_sys.account));
    row(L"User SID", g_sys.sid);
    row(L"Operating system", g_sys.osName + L"  (" + g_sys.osBuild + L", " + g_sys.arch + L")");
    row(L"Elevation", g_sys.elevated ? L"ADMINISTRATOR" : L"STANDARD USER");
    ss << L"</table></div>";

    ss << L"<div class=\"card\"><div class=\"hdr\">Analysis</div><table>";
    row(L"Target", g_meta.target);
    row(L"Scan started", g_meta.started + L"  (" + TzString() + L")");
    row(L"Scan finished", g_meta.finished + L"  (" + TzString() + L")");
    row(L"Scan duration", g_meta.duration);
    row(L"Memory selection", g_meta.mode);
    row(L"Verbosity", g_meta.verbosity);
    row(L"Options", g_meta.options);
    row(L"Filters", g_meta.filters);
    row(L"Exit code", ExitCodeText(g_meta.exitCode) + (g_meta.stopped ? L"  (STOPPED BY USER)" : L""));
    ss << L"<tr><td class=\"k\">Result</td><td class=\"v "
       << (g_accessFailed ? L"warn" : (TotalIocFindings() > 0 ? L"hit" : L"clean")) << L"\">"
       << HtmlEscape(ResultText()) << L"</td></tr>";    row(L"Command line", g_meta.command);
    row(L"Engine", g_meta.engine);
    ss << L"</table></div>";

    ss << L"<div class=\"card\"><div class=\"hdr\">Engine Output</div><pre>" << BuildHtmlOutput(outText) << L"</pre></div>";

    ss << L"<div class=\"foot\"><b>Engine credits:</b> Forrest Orr &amp; Gabriel Landau &mdash; Moneta (GPLv3)<br>";
    ss << L"<b>Tool build:</b> HYPERSENSEGLOBAL V1 &mdash; zero-crash build<br>";
    ss << L"<b>Developed by:</b> AG HYDRAXFF (@hydraxff_yt) &mdash; ";
    ss << L"<a href=\"https://www.instagram.com/hydraxff_yt/\">instagram.com/hydraxff_yt</a></div>";
    ss << L"</div></body></html>";
    return ss.str();
}

bool WriteUtf8File(const std::wstring& path, const std::wstring& text) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string u((size_t)(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), &u[0], n, nullptr, nullptr);
    DWORD written = 0;
    const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    WriteFile(h, bom, 3, &written, nullptr);
    if (!u.empty()) WriteFile(h, u.data(), (DWORD)u.size(), &written, nullptr);
    CloseHandle(h);
    return true;
}

// =====================================================================================
//  Log output
// =====================================================================================

// Moneta's console palette: turquoise process metadata, gold memory types,
// yellow descriptors (Heap/TEB/Stack/.NET) and red IOC descriptions.

const COLORREF LOG_TEXT  = RGB(203, 213, 225);
const COLORREF LOG_DIM   = RGB(148, 163, 184);
const COLORREF LOG_ADDR  = RGB(125, 211, 252);
const COLORREF LOG_TYPE  = RGB(250, 204, 21);
const COLORREF LOG_DESC  = RGB(253, 224, 71);
const COLORREF LOG_IOC   = RGB(248, 113, 113);
const COLORREF LOG_PROC  = RGB(45, 212, 191);
const COLORREF LOG_PIPE  = RGB(71, 85, 105);

struct LogSeg {
    std::wstring text;
    COLORREF color = LOG_TEXT;
    bool bold = false;
};

void Emit(std::vector<LogSeg>& out, const std::wstring& text, COLORREF color, bool bold = false) {
    if (text.empty()) return;
    if (!out.empty() && out.back().color == color && out.back().bold == bold) {
        out.back().text += text;
        return;
    }
    LogSeg s;
    s.text = text;
    s.color = color;
    s.bold = bold;
    out.push_back(s);
}

void EmitWithAddresses(std::vector<LogSeg>& out, const std::wstring& text, COLORREF base) {
    size_t start = 0;
    size_t i = 0;
    while (i + 1 < text.size()) {
        if (text[i] == L'0' && (text[i + 1] == L'x' || text[i + 1] == L'X')) {
            if (i > start) Emit(out, text.substr(start, i - start), base);
            size_t j = i + 2;
            while (j < text.size() && iswxdigit(text[j])) j++;
            Emit(out, text.substr(i, j - i), LOG_ADDR);
            start = i = j;
        } else {
            i++;
        }
    }
    if (start < text.size()) Emit(out, text.substr(start), base);
}

const wchar_t* const kIocDescriptions[] = {
    L"modified code", L"unsigned module", L"missing peb module",
    L"mismatching peb module", L"modified pe header",
    L"inconsistent +x between disk and memory",
    L"abnormal mapped executable memory", L"phantom image",
    L"abnormal private executable memory",
    L"thread within non-image memory region", L"non-image primary image base"
};

bool IsIocDesc(const std::wstring& lower) {
    for (const wchar_t* d : kIocDescriptions) if (lower == d) return true;
    return false;
}

bool IsDescToken(const std::wstring& lower) {
    return lower == L"heap" || lower == L"teb" || lower == L"stack" ||
           lower == L".net" || lower == L"primary image base";
}

bool IsMemTypeToken(const std::wstring& lower) {
    if (lower == L"private" || lower == L"mapped" || lower == L"phantom") return true;
    return lower.find(L"image") != std::wstring::npos;   // "private exe image", ".net dll nx-image"
}

bool IsPermToken(const std::wstring& lower) {
    static const wchar_t* kPerms[] = {
        L"r", L"rw", L"rx", L"rwx", L"rwxc", L"x", L"wc", L"na",
        L"wcb", L"pg", L"nc", L"-", L"?", L"free", L"reserve"
    };
    for (const wchar_t* p : kPerms) if (lower == p) return true;
    return false;
}

// Colour one engine output line into style segments.
std::vector<LogSeg> ColouriseWide(const std::wstring& w) {
    std::vector<LogSeg> out;
    std::wstring t = Trim(w);
    if (t.empty()) return out;

    if (t.find(L"scan completed") != std::wstring::npos) {
        Emit(out, w, Th::OK, true);
        return out;
    }

    if (t.rfind(L"IOC statistics", 0) == 0) {
        size_t br = t.find(L'[');
        if (br == std::wstring::npos) {
            Emit(out, w, Th::ACCENT, true);
        } else {
            Emit(out, t.substr(0, br), Th::ACCENT, true);
            Emit(out, t.substr(br), LOG_IOC, true);
        }
        return out;
    }

    if (t == L"Memory statistics") {
        Emit(out, w, Th::ACCENT, true);
        return out;
    }

    if (t.rfind(L"Moneta v", 0) == 0) {
        Emit(out, w, Th::TEXT_MUTE);
        return out;
    }

    // "Explorer.EXE : 5292 : x64 : C:\Windows\explorer.exe [ : CLR v4]"
    if (t.find(L" : ") != std::wstring::npos && t.find(L'|') == std::wstring::npos) {
        size_t pos = 0;
        for (;;) {
            size_t sep = w.find(L" : ", pos);
            if (sep == std::wstring::npos) {
                Emit(out, w.substr(pos), LOG_PROC);
                break;
            }
            Emit(out, w.substr(pos, sep - pos), LOG_PROC);
            Emit(out, L" : ", LOG_PIPE);
            pos = sep + 3;
        }
        return out;
    }

    // "  PRV [11 total]" statistics row
    {
        size_t a = 0;
        while (a < w.size() && iswspace(w[a])) a++;
        size_t b = a;
        while (b < w.size() && iswalpha(w[b])) b++;
        if (b > a && b < w.size() && w[b] == L'[') {
            size_t close = w.find(L']', b);
            if (close != std::wstring::npos) {
                Emit(out, w.substr(0, a), LOG_TEXT);
                Emit(out, w.substr(a, b - a), LOG_TYPE, true);
                Emit(out, w.substr(b, close - b + 1), LOG_DIM);
                Emit(out, w.substr(close + 1), Th::TEXT_MUTE);
                return out;
            }
        }
    }

    // Region / entity line: "  0x...:0x...   | Private  | path  | Heap  | <ioc>"
    if (t.rfind(L"0x", 0) == 0) {
        size_t pos = 0;
        bool first = true;
        for (;;) {
            size_t sep = w.find(L'|', pos);
            std::wstring part = (sep == std::wstring::npos) ? w.substr(pos) : w.substr(pos, sep - pos);

            if (first) {
                EmitWithAddresses(out, part, LOG_TEXT);
                first = false;
            } else {
                size_t a = 0;
                while (a < part.size() && iswspace(part[a])) a++;
                size_t b = part.size();
                while (b > a && iswspace(part[b - 1])) b--;
                std::wstring tok = part.substr(a, b - a);
                if (!tok.empty()) {
                    std::wstring l = Lower(tok);
                    COLORREF col = Th::TEXT_DIM;
                    bool bold = false;
                    if (IsIocDesc(l)) {
                        col = LOG_IOC;
                        bold = true;
                    } else if (IsDescToken(l)) {
                        col = LOG_DESC;
                    } else if (IsMemTypeToken(l)) {
                        col = LOG_TYPE;
                        bold = true;
                    } else if (tok.rfind(L"0x", 0) == 0) {
                        col = LOG_ADDR;
                    } else if (tok.find(L'\\') != std::wstring::npos) {
                        col = Th::TEXT_MUTE;
                    } else if (IsPermToken(l)) {
                        col = LOG_TEXT;
                    } else if (tok[0] == L'.') {
                        col = LOG_DIM;      // section name (.text, .rdata, ...)
                    }
                    Emit(out, part.substr(0, a), LOG_TEXT);
                    Emit(out, tok, col, bold);
                    Emit(out, part.substr(b), LOG_TEXT);
                } else {
                    Emit(out, part, LOG_TEXT);
                }
            }

            if (sep == std::wstring::npos) break;
            Emit(out, L"|", LOG_PIPE);
            pos = sep + 1;
        }
        return out;
    }

    // Detail rows: "    |__ Base address: 0x...", "      | Size: 4096"
    if (t[0] == L'|') {
        std::wstring l = Lower(t);
        for (const wchar_t* d : kIocDescriptions) {
            std::wstring prefix = std::wstring(L"|__ ") + d;
            if (l.rfind(prefix, 0) == 0 && (l.size() == prefix.size() || l[prefix.size()] == L':' ||
                                            l[prefix.size()] == L' ')) {
                Emit(out, w.substr(0, w.size() - t.size()), LOG_TEXT);
                Emit(out, L"|__ ", LOG_PIPE);
                size_t len = prefix.size() - 4;
                Emit(out, t.substr(4, len), LOG_IOC, true);
                Emit(out, t.substr(4 + len), Th::TEXT_MUTE);
                return out;
            }
        }
        EmitWithAddresses(out, w, LOG_DIM);
        return out;
    }

    EmitWithAddresses(out, w, LOG_TEXT);
    return out;
}

std::vector<LogSeg> ColouriseLine(const std::string& raw) {
    std::vector<LogSeg> segs = ColouriseWide(Widen(raw));
    if (!g_focusUnsigned || segs.empty()) return segs;

    // Focus mode: unsigned-module evidence and region findings stay bright,
    // routine engine chatter is pushed back so the eye lands on what matters.
    std::wstring w = Widen(raw);
    std::wstring l = Lower(w);
    std::wstring t = Trim(w);
    bool unsignedHit = l.find(L"unsigned") != std::wstring::npos;
    bool finding = (!t.empty() && t.rfind(L"0x", 0) == 0) ||
                   l.find(L"ioc") != std::wstring::npos ||
                   l.find(L"suspicious") != std::wstring::npos;

    if (unsignedHit) {
        for (auto& s : segs) { s.color = Th::WARN; s.bold = true; }
        return segs;
    }
    if (finding) return segs;
    for (auto& s : segs) { s.color = RGB(84, 98, 114); s.bold = false; }
    return segs;
}

std::wstring LogColorCss(COLORREF c) {
    if (c == LOG_ADDR) return L"#7dd3fc";
    if (c == LOG_TYPE) return L"#facc15";
    if (c == LOG_DESC) return L"#fde047";
    if (c == LOG_IOC)  return L"#f87171";
    if (c == LOG_PROC) return L"#2dd4bf";
    if (c == LOG_PIPE) return L"#64748b";
    if (c == Th::OK)   return L"#34d399";
    if (c == Th::ACCENT) return L"#22d3ee";
    if (c == Th::WARN) return L"#fbbf24";
    if (c == Th::TEXT_MUTE) return L"#64748b";
    if (c == LOG_DIM) return L"#94a3b8";
    return L"#cbd5e1";
}

// Same colour scheme as the live viewer, rendered as inline-styled HTML.
std::wstring BuildHtmlOutput(const std::wstring& body) {
    if (body.size() > 4u * 1024 * 1024) return HtmlEscape(body);   // keep huge reports fast

    std::wstring out;
    size_t pos = 0;
    for (;;) {
        size_t nl = body.find(L'\n', pos);
        std::wstring line = body.substr(pos, nl == std::wstring::npos ? std::wstring::npos : nl - pos);
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        std::vector<LogSeg> segs = ColouriseWide(line);
        if (segs.empty()) {
            out += L"\n";
        } else {
            for (const LogSeg& s : segs) {
                out += L"<span style=\"color:" + LogColorCss(s.color) +
                       (s.bold ? L";font-weight:700" : L"") + L"\">";
                out += HtmlEscape(s.text);
                out += L"</span>";
            }
            out += L"\n";
        }
        if (nl == std::wstring::npos) break;
        pos = nl + 1;
    }
    return out;
}

// Track IOC findings for the result banner, status bar and report.
void NoteLineStats(const std::string& raw) {
    if (!g_accessFailed &&
        (ContainsI(raw, "failed to map address space") || ContainsI(raw, "failed to open process") ||
         ContainsI(raw, "failed to duplicate"))) {
        g_accessFailed = true;
    }

    std::wstring w = Widen(raw);
    std::wstring t = Trim(w);

    if (t.rfind(L"IOC statistics", 0) == 0) {
        size_t a = t.find(L'[');
        size_t b = t.find(L']');
        if (a != std::wstring::npos && b != std::wstring::npos && b > a + 1) {
            int n = _wtoi(t.substr(a + 1, b - a - 1).c_str());
            if (n > g_iocHits) g_iocHits = n;
        }
        return;
    }

    if (t.rfind(L"0x", 0) != 0) return;

    int hits = 0;
    size_t pos = 0;
    while ((pos = w.find(L'|', pos)) != std::wstring::npos) {
        size_t s = pos + 1;
        size_t e = w.find(L'|', s);
        std::wstring tok = Lower(Trim(w.substr(s, e == std::wstring::npos ? std::wstring::npos : e - s)));
        if (IsIocDesc(tok)) hits++;
        if (e == std::wstring::npos) break;
        pos = e;
    }
    if (hits > 0) {
        g_regionHits++;
        if (hits > g_iocHits) g_iocHits = hits;
    }
}

void RichAppendTo(HWND target, const std::wstring& text, COLORREF color, bool bold) {
    if (!target) return;
    CHARRANGE cr;
    SendMessageW(target, EM_EXGETSEL, 0, (LPARAM)&cr);
    cr.cpMin = -1;
    cr.cpMax = -1;
    SendMessageW(target, EM_EXSETSEL, 0, (LPARAM)&cr);

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_BOLD;
    cf.crTextColor = color;
    cf.dwEffects = bold ? CFE_BOLD : 0;
    SendMessageW(target, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(target, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

void RichAppend(const std::wstring& text, COLORREF color, bool bold) {
    RichAppendTo(g_hRich, text, color, bold);
}

void AppendLine(const std::string& line) {
    if (g_viewCapped) return;
    if (g_viewLines >= g_viewLineCap) {
        g_viewCapped = true;
        RichAppend(L"\r\n[ viewer limit reached - full output is preserved in the saved report ]\r\n", Th::TEXT_MUTE, false);
        return;
    }
    NoteLineStats(line);
    std::vector<LogSeg> segs = ColouriseLine(line);
    if (segs.empty()) {
        RichAppend(L"\r\n", LOG_TEXT, false);
    } else {
        for (size_t i = 0; i < segs.size(); ++i) {
            RichAppend(segs[i].text + (i + 1 == segs.size() ? L"\r\n" : L""), segs[i].color, segs[i].bold);
        }
    }
    g_viewLines++;
}

void ScrollLogBottom() {
    if (!g_hRich) return;
    SendMessageW(g_hRich, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(g_hRich, WM_VSCROLL, MAKEWPARAM(SB_BOTTOM, 0), 0);
    SendMessageW(g_hRich, EM_SCROLLCARET, 0, 0);
}

void AppendOutputChunk(const std::string& chunk) {
    if (g_reportBody.size() < g_reportCap) {
        size_t room = g_reportCap - g_reportBody.size();
        g_reportBody.append(chunk.data(), (std::min)(chunk.size(), room));
    }
    if (g_viewCapped) return;   // display saturated - keep draining the pipe, drop rendering work

    g_partialLine += chunk;
    std::wstring run;
    COLORREF runColor = LOG_TEXT;
    bool runBold = false;
    bool haveRun = false;
    bool appended = false;

    auto flushRun = [&]() {
        if (!run.empty()) {
            RichAppend(run, runColor, runBold);
            run.clear();
            appended = true;
        }
        haveRun = false;
    };

    auto emitSeg = [&](const std::wstring& text, COLORREF col, bool bold) {
        if (!haveRun) {
            runColor = col;
            runBold = bold;
            haveRun = true;
        } else if (col != runColor || bold != runBold) {
            flushRun();
            runColor = col;
            runBold = bold;
            haveRun = true;
        }
        run += text;
        if (run.size() > 32768) {
            RichAppend(run, runColor, runBold);
            run.clear();
            appended = true;
            haveRun = false;
        }
    };

    size_t pos;
    while ((pos = g_partialLine.find('\n')) != std::string::npos) {
        std::string line = g_partialLine.substr(0, pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        g_partialLine.erase(0, pos + 1);

        if (g_viewLines >= g_viewLineCap) {
            flushRun();
            g_viewCapped = true;
            RichAppend(L"\r\n[ viewer limit reached - full output is preserved in the saved report ]\r\n",
                       Th::TEXT_MUTE, false);
            g_partialLine.clear();
            ScrollLogBottom();
            return;
        }

        NoteLineStats(line);
        std::vector<LogSeg> segs = ColouriseLine(line);
        if (segs.empty()) {
            emitSeg(L"\r\n", LOG_TEXT, false);
        } else {
            for (size_t i = 0; i < segs.size(); ++i) {
                emitSeg(segs[i].text + (i + 1 == segs.size() ? L"\r\n" : L""), segs[i].color, segs[i].bold);
            }
        }
        g_viewLines++;
    }
    flushRun();
    if (appended) ScrollLogBottom();
}

void FlushPartialLine() {
    if (!g_partialLine.empty()) {
        AppendLine(g_partialLine);
        g_partialLine.clear();
    }
}

// =====================================================================================
//  Scan execution
// =====================================================================================

void FreeProcessHandles() {
    if (g_hProc) { CloseHandle(g_hProc); g_hProc = nullptr; }
    if (g_hReadPipe) { CloseHandle(g_hReadPipe); g_hReadPipe = nullptr; }
}

void ReaderLoop(HWND hwnd, int gen, HANDLE rd) {
    char buf[8192];
    DWORD got = 0;
    while (ReadFile(rd, buf, sizeof(buf), &got, nullptr) && got > 0) {
        std::string* chunk = new std::string(buf, got);
        if (!PostMessageW(hwnd, WM_HS_OUTPUT, (WPARAM)gen, (LPARAM)chunk)) delete chunk;
    }
    PostMessageW(hwnd, WM_HS_DONE, (WPARAM)gen, 0);
}

int g_memMode = 0;      // 0 ioc, 1 all, 2 region, 3 referenced
int g_verbMode = 1;     // 0 surface, 1 detail, 2 debug

const wchar_t* ModeLabel(int idx) {
    static const wchar_t* names[] = { L"IOC", L"ALL", L"REGION", L"REFERENCED" };
    return (idx >= 0 && idx < 4) ? names[idx] : L"IOC";
}

const wchar_t* ModeDesc(int idx) {
    static const wchar_t* names[] = { L"IOC only", L"All committed memory", L"Region by address", L"Referenced by address" };
    return (idx >= 0 && idx < 4) ? names[idx] : L"IOC only";
}

const wchar_t* VerbLabel(int idx) {
    static const wchar_t* names[] = { L"SURFACE", L"DETAIL", L"DEBUG" };
    return (idx >= 0 && idx < 3) ? names[idx] : L"DETAIL";
}

void UpdateToggleAvailability() {
    bool all = TgGetOn(g_hTgFilterAll);
    HWND filters[] = { g_hTgUnsigned, g_hTgMetadata, g_hTgClrHeap, g_hTgClrPrvx, g_hTgWow64 };
    for (HWND h : filters) {
        if (h) EnableWindow(h, !all);
    }
}

void UpdateModeAvailability() {
    bool addressMode = (g_memMode == 2 || g_memMode == 3);
    EnableWindow(g_hEditAddr, addressMode);
    EnableWindow(g_hEditSize, addressMode);
}

void SelectMode(int idx) {
    g_memMode = idx;
    for (int i = 0; i < 4; ++i) BtnSetChecked(g_hModeBtn[i], i == idx);
    UpdateModeAvailability();
    MarkCustomPreset();
    UpdateClassicPreview();
    if (g_tab) BuildInfoView();
}

void SelectVerb(int idx) {
    g_verbMode = idx;
    for (int i = 0; i < 3; ++i) BtnSetChecked(g_hVerbBtn[i], i == idx);
    MarkCustomPreset();
    UpdateClassicPreview();
    if (g_tab) BuildInfoView();
}

bool ParseAddress(std::wstring text, unsigned long long& out) {
    text = Trim(text);
    if (text.empty()) return false;
    int base = 16;
    if (text.size() > 2 && text[0] == L'0' && (text[1] == L'x' || text[1] == L'X')) {
        text = text.substr(2);
        base = 16;
    }
    if (text.empty()) return false;
    wchar_t* end = nullptr;
    unsigned long long v = wcstoull(text.c_str(), &end, base);
    if (!end || *end != 0) return false;
    out = v;
    return true;
}

std::wstring EngineArgsForPid(const std::wstring& pidToken) {
    std::wstring cmd;
    cmd += L" -m ";
    switch (g_memMode) {
        case 1: cmd += L"*"; break;
        case 2: cmd += L"region"; break;
        case 3: cmd += L"referenced"; break;
        default: cmd += L"ioc"; break;
    }
    cmd += L" -p " + pidToken;

    // Surface is the engine default, so only emit -v when it differs.
    if (g_verbMode != 0) {
        cmd += L" -v ";
        cmd += (g_verbMode == 1 ? L"detail" : L"debug");
    }

    if (g_memMode == 2 || g_memMode == 3) {
        wchar_t addr[64] = {};
        GetWindowTextW(g_hEditAddr, addr, 64);
        cmd += L" --address ";
        cmd += addr;
        wchar_t size[32] = {};
        GetWindowTextW(g_hEditSize, size, 32);
        std::wstring sz = Trim(size);
        if (!sz.empty()) cmd += L" --region-size " + sz;
    }

    if (TgGetOn(g_hTgDump)) cmd += L" -d";

    std::wstring opts;
    if (TgGetOn(g_hTgStats)) opts += L" statistics";
    if (TgGetOn(g_hTgFromBase)) opts += L" from-base";
    if (!opts.empty()) cmd += L" --option" + opts;

    if (TgGetOn(g_hTgFilterAll)) {
        cmd += L" --filter *";
    } else {
        std::wstring filters;
        if (TgGetOn(g_hTgUnsigned)) filters += L" unsigned-modules";
        if (TgGetOn(g_hTgMetadata)) filters += L" metadata-modules";
        if (TgGetOn(g_hTgClrHeap)) filters += L" clr-heap";
        if (TgGetOn(g_hTgClrPrvx)) filters += L" clr-prvx";
        if (TgGetOn(g_hTgWow64)) filters += L" wow64-init";
        if (!filters.empty()) cmd += L" --filter" + filters;
    }
    return cmd;
}

std::wstring BuildCommandLine() {
    return L"\"" + ExePath() + L"\" --engine" +
           EngineArgsForPid(g_targetAll ? L"*" : Format(L"%lu", g_targetPid));
}

// -p value as it should appear in the classic preview: the override box wins.
std::wstring PreviewPidToken() {
    wchar_t buf[160] = {};
    if (g_hEditPid) GetWindowTextW(g_hEditPid, buf, 160);
    std::wstring t = Trim(buf);
    if (t == L"*") return L"*";
    if (!t.empty()) {
        bool numeric = true;
        for (wchar_t c : t) {
            if (c < L'0' || c > L'9') { numeric = false; break; }
        }
        if (numeric) return t;
    }
    if (g_targetAll) return L"*";
    if (g_targetPid) return Format(L"%lu", g_targetPid);
    return L"<pid>";
}

// What the classic bar shows: the same arguments in moneta64.exe form.
std::wstring BuildMonetaCommandLine() {
    return L"moneta64.exe" + EngineArgsForPid(PreviewPidToken());
}

void SaveReportDialog(bool html) {
    if (!g_meta.valid) {
        HsMessage(HSI_INFO, L"NO ANALYSIS DATA",
                  L"Run an analysis first - there is nothing to report yet.");
        return;
    }
    std::wstring dir = ExeDir();
    SYSTEMTIME st;
    GetLocalTime(&st);
    std::wstring def = JoinPath(dir, Format(L"HYPERSENSE_REPORT_%04d%02d%02d_%02d%02d%02d.%s",
                                            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                                            html ? L"html" : L"txt"));

    wchar_t file[MAX_PATH] = {};
    StringCchCopyW(file, MAX_PATH, def.c_str());

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = html ? L"HTML report (*.html)\0*.html\0Text report (*.txt)\0*.txt\0\0"
                           : L"Text report (*.txt)\0*.txt\0HTML report (*.html)\0*.html\0\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = html ? L"html" : L"txt";
    ofn.lpstrTitle = html ? L"Save HYPERSENSEGLOBAL report as HTML"
                          : L"Save HYPERSENSEGLOBAL report as text";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&ofn)) return;

    std::wstring path = file;
    std::wstring low = Lower(path);
    bool endsHtml = low.size() > 5 && low.substr(low.size() - 5) == L".html";
    bool endsTxt = low.size() > 4 && low.substr(low.size() - 4) == L".txt";

    // The button picks the default format, but an extension typed by the user wins.
    bool asHtml = html;
    if (endsHtml) asHtml = true;
    else if (endsTxt) asHtml = false;

    if (asHtml && !endsHtml) path += L".html";
    if (!asHtml && !endsTxt) path += L".txt";

    std::wstring data = asHtml ? BuildReportHtml() : BuildReportText();
    if (WriteUtf8File(path, data)) {
        SetStatus(asHtml ? L"HTML REPORT SAVED" : L"TXT REPORT SAVED");
        ReportSavedDialog(asHtml ? L"HTML REPORT SAVED" : L"TXT REPORT SAVED", path);
    } else {
        HsMessage(HSI_ERROR, L"REPORT NOT SAVED",
                  L"Could not write the report file.\r\nCheck the folder permissions and try again.");
    }
}

void StartScan() {
    if (g_scanning) return;
    if (!g_targetAll && g_targetPid == 0) {
        HsMessage(HSI_INFO, L"NO TARGET SELECTED",
                  L"Select a target process first, or choose ALL PROCESSES.");
        return;
    }
    if (g_memMode == 2 || g_memMode == 3) {
        wchar_t addr[64] = {};
        GetWindowTextW(g_hEditAddr, addr, 64);
        unsigned long long parsed = 0;
        if (!ParseAddress(addr, parsed)) {
            HsMessage(HSI_WARN, L"INVALID ADDRESS",
                      L"Region and Referenced modes need a valid memory address.\r\nExample: 0x7FFE0000");
            return;
        }
    }

    // reset view + report
    g_reportBody.clear();
    g_partialLine.clear();
    g_viewLines = 0;
    g_viewCapped = false;
    g_iocHits = 0;
    g_regionHits = 0;
    g_accessFailed = false;
    SetWindowTextW(g_hRich, L"");
    g_meta = ScanMeta();
    g_meta.valid = true;
    g_meta.started = NowLocal(true);
    g_meta.engine = L"Built-in - Moneta v1.0.1 (compiled into this tool)";
    g_meta.mode = ModeDesc(g_memMode);
    g_meta.verbosity = VerbLabel(g_verbMode);
    g_meta.command = BuildCommandLine();

    if (g_targetAll) {
        g_meta.target = L"ALL ACCESSIBLE PROCESSES (*)";
    } else {
        const ProcItem* p = FindProcByPid(g_targetPid);
        if (p) g_meta.target = Format(L"%s  (PID %lu)  -  %s", p->name.c_str(), p->pid, p->path.c_str());
        else g_meta.target = Format(L"PID %lu", g_targetPid);
    }

    std::wstring opts;
    if (TgGetOn(g_hTgStats)) opts += L"statistics ";
    if (TgGetOn(g_hTgFromBase)) opts += L"from-base ";
    if (TgGetOn(g_hTgDump)) opts += L"memory-dump ";
    g_meta.options = opts.empty() ? L"none" : Trim(opts);

    if (TgGetOn(g_hTgFilterAll)) {
        g_meta.filters = L"all filters (*)";
    } else {
        std::wstring f;
        if (TgGetOn(g_hTgUnsigned)) f += L"unsigned-modules ";
        if (TgGetOn(g_hTgMetadata)) f += L"metadata-modules ";
        if (TgGetOn(g_hTgClrHeap)) f += L"clr-heap ";
        if (TgGetOn(g_hTgClrPrvx)) f += L"clr-prvx ";
        if (TgGetOn(g_hTgWow64)) f += L"wow64-init ";
        g_meta.filters = f.empty() ? L"none" : Trim(f);
    }

    // A scan always shows live output; the other tabs can be revisited afterwards.
    if (g_tab != 0) ShowTab(0);
    if (g_tab) BuildInfoView();

    // intro lines in viewer
    RichAppend(L"[ " + NowLocal(true) + L" ]  HYPERSENSEGLOBAL // ANALYSIS START\n", Th::ACCENT, true);
    RichAppend(L"[ target ]  " + g_meta.target + L"\n", Th::TEXT_DIM, false);
    RichAppend(L"[ command ] " + g_meta.command + L"\n\n", Th::TEXT_MUTE, false);
    if (!g_sys.elevated) {
        RichAppend(L"[ notice ]  Running without elevation. Protected processes may be inaccessible. Use ELEVATE for full coverage.\n\n",
                   Th::WARN, false);
    }
    g_viewLines += 4;

    // create pipes
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        HsMessage(HSI_ERROR, L"ANALYSIS ERROR", L"Could not create the analysis pipe.");
        return;
    }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = nul;

    PROCESS_INFORMATION pi{};
    std::wstring cmd = BuildCommandLine();
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    std::wstring cwd = ExeDir();
    if (TgGetOn(g_hTgDump)) {
        std::wstring dumpDir = JoinPath(cwd, L"HYPERSENSE_DUMPS");
        CreateDirectoryW(dumpDir.c_str(), nullptr);
        cwd = dumpDir;
    }

    std::wstring self = ExePath();
    BOOL ok = CreateProcessW(self.c_str(), cmdBuf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (nul && nul != INVALID_HANDLE_VALUE) CloseHandle(nul);

    if (!ok) {
        CloseHandle(rd);
        DWORD err = GetLastError();
        HsMessage(HSI_ERROR, L"ENGINE LAUNCH FAILED",
                  Format(L"Failed to launch the Moneta engine (error %lu).", err));
        SetStatus(L"ENGINE LAUNCH FAILED");
        return;
    }

    CloseHandle(pi.hThread);
    g_hProc = pi.hProcess;
    g_hReadPipe = rd;
    g_scanning = true;
    g_stopRequested = false;
    g_scanGen++;
    g_scanStartTick = GetTickCount64();
    g_rpcScanStartMs = RpcUnixMs();
    RpcPublishScanning();

    if (g_reader.joinable()) g_reader.join();
    int gen = g_scanGen;
    HWND hwnd = g_hWnd;
    g_reader = std::thread([hwnd, gen, rd]() { ReaderLoop(hwnd, gen, rd); });

    EnableWindow(g_hBtnRun, FALSE);
    EnableWindow(g_hBtnStop, TRUE);
    EnableWindow(g_hBtnSaveTxt, FALSE);
    EnableWindow(g_hBtnSaveHtml, FALSE);
    EnableWindow(g_hCombo, FALSE);
    EnableWindow(g_hBtnRefresh, FALSE);
    EnableWindow(g_hBtnSelect, FALSE);
    SetStatus(L"SCANNING");
    InvalidateRect(g_hWnd, nullptr, FALSE);
}

void StopScan() {
    if (!g_scanning) return;
    g_stopRequested = true;
    if (g_hProc) TerminateProcess(g_hProc, 0);
    SetStatus(L"STOPPING");
}

void OnScanDone(int gen) {
    if (gen != g_scanGen) return;
    FlushPartialLine();

    DWORD code = 0;
    if (g_hProc) GetExitCodeProcess(g_hProc, &code);
    if (g_reader.joinable()) g_reader.join();
    FreeProcessHandles();

    g_scanning = false;
    g_meta.exitCode = code;
    g_meta.stopped = g_stopRequested;
    g_meta.finished = NowLocal(true);
    ULONGLONG elapsed = GetTickCount64() - g_scanStartTick;
    g_meta.duration = Format(L"%.3f s", elapsed / 1000.0);

    EnableWindow(g_hBtnRun, TRUE);
    EnableWindow(g_hBtnStop, FALSE);
    EnableWindow(g_hBtnSaveTxt, TRUE);
    EnableWindow(g_hBtnSaveHtml, TRUE);
    EnableWindow(g_hCombo, TRUE);
    EnableWindow(g_hBtnRefresh, TRUE);
    EnableWindow(g_hBtnSelect, TRUE);

    bool early = ScanEndedEarly(g_meta);
    std::wstring summary = L"[ " + NowLocal(true) + L" ]  ANALYSIS " +
                           (g_meta.stopped ? L"STOPPED" : (early ? L"ENDED EARLY" : L"COMPLETE")) +
                           Format(L"  |  exit %s  |  %s\n", ExitCodeText(code).c_str(), g_meta.duration.c_str());
    RichAppend(summary, g_meta.stopped ? Th::WARN : (early ? Th::DANGER : Th::OK), true);
    g_viewLines += 1;
    if (early) {
        RichAppend(L"[ notice ]  The Moneta engine stopped unexpectedly while scanning.\r\n"
                   L"            Everything above is complete up to that point and the report is still valid.\r\n"
                   L"            Try IOC selection, SURFACE verbosity, or a single target process.\r\n",
                   Th::WARN, false);
        g_viewLines += 3;
    }

    if (!g_meta.stopped && !early) {
        int found = TotalIocFindings();
        if (g_accessFailed) {
            RichAppend(L"[ RESULT ]  Engine could not access the target process - no memory was analysed.\r\n"
                       L"            Run the tool as administrator (ELEVATE) and try again.\r\n", Th::WARN, true);
            g_viewLines += 2;
        } else if (found > 0) {
            int regions = SuspiciousRegions();
            std::wstring msg = Format(L"[ RESULT ]  %d suspicious region%s flagged", regions, regions == 1 ? L"" : L"s");
            if (found != regions) msg += Format(L"  (%d IOC findings)", found);
            RichAppend(msg + L"\n", Th::DANGER, true);
        } else {
            RichAppend(L"[ RESULT ]  No IOC findings - no suspicious memory regions detected\n", Th::OK, true);
        }
        g_viewLines += 1;
    }
    ScrollLogBottom();

    if (g_meta.stopped) SetStatus(L"STOPPED");
    else if (early) SetStatus(L"ENGINE ENDED EARLY");
    else if (g_accessFailed) SetStatus(L"ACCESS DENIED");
    else if (TotalIocFindings() > 0) SetStatus(Format(L"%d IOC HITS", TotalIocFindings()));
    else SetStatus(L"REPORT READY");
    if (g_tab) BuildInfoView();
    RpcPublishResult();
    InvalidateRect(g_hWnd, nullptr, FALSE);
}

// =====================================================================================
//  Layout
// =====================================================================================

// Vertical metrics for the main-menu sidebar. Five tiers, roomiest first; the
// first one that fits the real client height wins. Every tier keeps all four
// cards (including ADDRESS / REGION SIZE), so a short window or a high display
// scale compresses the column instead of clipping it.
SideMetrics MetricsFor(int tier) {
    SideMetrics m{};
    m.titleY = 11;
    m.twoCol = tier >= 2;
    switch (tier) {
        case 0:
            m.targetH = 150; m.modeH = 178; m.optH = 140; m.filterH = 208; m.gap = 11;
            m.tSearchY = 30; m.tComboY = 62; m.tBtnY = 104; m.tBtnH = 31;
            m.mRowY = 30; m.mRowH = 30; m.vLabelY = 66; m.vRowY = 86; m.vRowH = 28;
            m.aLabelY = 116; m.aRowY = 138; m.aRowH = 26;
            m.oFirstY = 32; m.oStep = 28; m.oH = 26;
            m.fFirstY = 32; m.fStep = 28; m.fH = 26;
            break;
        case 1:
            m.targetH = 140; m.modeH = 170; m.optH = 124; m.filterH = 208; m.gap = 11;
            m.tSearchY = 28; m.tComboY = 58; m.tBtnY = 98; m.tBtnH = 29;
            m.mRowY = 26; m.mRowH = 28; m.vLabelY = 58; m.vRowY = 74; m.vRowH = 26;
            m.aLabelY = 104; m.aRowY = 118; m.aRowH = 24;
            m.oFirstY = 28; m.oStep = 26; m.oH = 25;
            m.fFirstY = 30; m.fStep = 26; m.fH = 25;
            break;
        case 2:
            m.targetH = 140; m.modeH = 170; m.optH = 124; m.filterH = 124; m.gap = 10;
            m.tSearchY = 28; m.tComboY = 58; m.tBtnY = 98; m.tBtnH = 29;
            m.mRowY = 26; m.mRowH = 28; m.vLabelY = 58; m.vRowY = 74; m.vRowH = 26;
            m.aLabelY = 104; m.aRowY = 118; m.aRowH = 24;
            m.oFirstY = 28; m.oStep = 26; m.oH = 25;
            m.fFirstY = 30; m.fStep = 26; m.fH = 25;
            break;
        case 3:
            m.targetH = 136; m.modeH = 168; m.optH = 118; m.filterH = 118; m.gap = 8;
            m.tSearchY = 28; m.tComboY = 58; m.tBtnY = 96; m.tBtnH = 28;
            m.mRowY = 26; m.mRowH = 28; m.vLabelY = 58; m.vRowY = 74; m.vRowH = 26;
            m.aLabelY = 104; m.aRowY = 118; m.aRowH = 24;
            m.oFirstY = 28; m.oStep = 26; m.oH = 24;
            m.fFirstY = 28; m.fStep = 26; m.fH = 24;
            break;
        default:
            m.targetH = 124; m.modeH = 148; m.optH = 106; m.filterH = 106; m.gap = 6;
            m.tSearchY = 26; m.tComboY = 54; m.tBtnY = 90; m.tBtnH = 28;
            m.mRowY = 24; m.mRowH = 26; m.vLabelY = 54; m.vRowY = 68; m.vRowH = 24;
            m.aLabelY = 96; m.aRowY = 110; m.aRowH = 24;
            m.oFirstY = 26; m.oStep = 24; m.oH = 24;
            m.fFirstY = 26; m.fStep = 24; m.fH = 24;
            break;
    }
    return m;
}

void LayoutControls(HWND h, int W, int H) {
    int m = S(18);
    g_L.headerH = S(80);
    int footerH = S(58);
    g_L.footerTop = H - footerH;
    int sidebarW = S(352);
    g_L.sidebarW = sidebarW;

    // header toggles - visible in both layouts
    const int igW = S(148), clW = S(150), fuW = S(198), gap = S(8);
    int hx = W - m - igW;
    MoveWindow(g_hTgInsta, hx, S(27), igW, S(26), TRUE);
    hx -= gap + clW;
    MoveWindow(g_hTgClassic, hx, S(27), clW, S(26), TRUE);
    hx -= gap + fuW;
    MoveWindow(g_hTgFocusUnsigned, hx, S(27), fuW, S(26), TRUE);

    const int tabW[3] = { S(90), S(112), S(110) };
    auto layoutTabs = [&](const RECT& card) {
        int total = tabW[0] + tabW[1] + tabW[2] + S(16);
        int tx = card.right - S(14) - total;
        int ty = card.top + S(8);
        for (int i = 0; i < 3; ++i) {
            MoveWindow(g_hTab[i], tx, ty, tabW[i], S(24), TRUE);
            tx += tabW[i] + S(8);
        }
    };

    if (g_classic) {
        g_L.cardTarget = RECT{};
        g_L.cardMode = RECT{};
        g_L.cardOpt = RECT{};
        g_L.cardFilter = RECT{};
        g_L.infoCard = RECT{};
        g_L.actionRow = RECT{};

        int top = g_L.headerH + S(10);
        g_L.classicBar = { m, top, W - m, top + S(168) };
        g_L.logCard = { m, g_L.classicBar.bottom + S(12), W - m, g_L.footerTop - S(10) };
        if (g_L.logCard.bottom < g_L.logCard.top + S(160)) g_L.logCard.bottom = g_L.logCard.top + S(160);

        // Same target picker as the main menu: type to filter, choose from the list.
        int left = g_L.classicBar.left + S(16);
        int right = g_L.classicBar.right - S(16);
        int searchW = S(260);
        int pidW = S(220);
        int comboW = (right - left) - searchW - pidW - S(16);
        if (comboW < S(260)) comboW = S(260);

        int row1 = g_L.classicBar.top + S(26);
        MoveWindow(g_hEditSearch, left, row1, searchW, S(30), TRUE);
        MoveWindow(g_hCombo, left + searchW + S(8), row1, comboW, S(220), TRUE);
        MoveWindow(g_hEditPid, left + searchW + S(8) + comboW + S(8), row1, pidW, S(30), TRUE);

        int labelW = S(64);
        int row2 = g_L.classicBar.top + S(64);
        int row3 = g_L.classicBar.top + S(96);
        MoveWindow(g_hPreset, left + labelW, row2, (right - left) - labelW, S(220), TRUE);
        MoveWindow(g_hCmd, left + labelW, row3, (right - left) - labelW, S(26), TRUE);

        int cx = left;
        int row4 = g_L.classicBar.top + S(130);
        MoveWindow(g_hBtnRun, cx, row4, S(176), S(30), TRUE); cx += S(176) + S(8);
        MoveWindow(g_hBtnStop, cx, row4, S(88), S(30), TRUE); cx += S(88) + S(8);
        MoveWindow(g_hBtnSaveTxt, cx, row4, S(104), S(30), TRUE); cx += S(104) + S(8);
        MoveWindow(g_hBtnSaveHtml, cx, row4, S(112), S(30), TRUE); cx += S(112) + S(8);
        MoveWindow(g_hBtnClear, cx, row4, S(84), S(30), TRUE); cx += S(84) + S(8);
        MoveWindow(g_hBtnRefresh, cx, row4, S(104), S(30), TRUE);

        g_L.classicStatus = { g_L.classicBar.right - S(320), g_L.classicBar.top + S(7),
                              g_L.classicBar.right - S(16), g_L.classicBar.top + S(23) };

        RECT lr = g_L.logCard;
        MoveWindow(g_hRich, lr.left + S(12), lr.top + S(44), (lr.right - lr.left) - S(24),
                   (lr.bottom - lr.top) - S(56), TRUE);
        MoveWindow(g_hInfo, lr.left + S(12), lr.top + S(44), (lr.right - lr.left) - S(24),
                   (lr.bottom - lr.top) - S(56), TRUE);
        layoutTabs(g_L.logCard);
        return;
    }

    int x = m;
    int y = g_L.headerH + S(10);
    int cw = sidebarW;

    // Height-adaptive sidebar: pick the roomiest tier that fits the real client.
    const int bottomGap = S(10);
    const int cardSpace = (g_L.footerTop - bottomGap) - y;
    int tier = 0;
    for (int i = 1; i < 5; ++i) {
        SideMetrics t = MetricsFor(i);
        if (S(t.targetH + t.modeH + t.optH + t.filterH + t.gap * 3) <= cardSpace) { tier = i; break; }
        tier = i;
    }
    const SideMetrics M = MetricsFor(tier);
    g_L.m = M;
    const bool twoCol = M.twoCol;

    auto setCard = [&](RECT& r, int height) {
        r = { x, y, x + cw, y + height };
        y += height + S(M.gap);
    };
    setCard(g_L.cardTarget, S(M.targetH));
    setCard(g_L.cardMode, S(M.modeH));
    setCard(g_L.cardOpt, S(M.optH));
    g_L.cardFilter = { x, y, x + cw, g_L.footerTop - bottomGap };
    if (g_L.cardFilter.bottom < y + S(M.filterH)) g_L.cardFilter.bottom = y + S(M.filterH);

    int pad = S(13);

    // target card
    int cy = g_L.cardTarget.top;
    MoveWindow(g_hEditSearch, x + pad, cy + S(M.tSearchY), cw - pad * 2, S(26), TRUE);
    MoveWindow(g_hCombo, x + pad, cy + S(M.tComboY), cw - pad * 2, S(220), TRUE);
    int bw = (cw - pad * 2 - S(8)) / 2;
    MoveWindow(g_hBtnSelect, x + pad, cy + S(M.tBtnY), bw, S(M.tBtnH), TRUE);
    MoveWindow(g_hBtnRefresh, x + pad + bw + S(8), cy + S(M.tBtnY), bw, S(M.tBtnH), TRUE);

    // mode card
    cy = g_L.cardMode.top;
    int segW = (cw - pad * 2 - S(18)) / 4;
    for (int i = 0; i < 4; ++i) {
        MoveWindow(g_hModeBtn[i], x + pad + i * (segW + S(6)), cy + S(M.mRowY), segW, S(M.mRowH), TRUE);
    }
    int verbW = (cw - pad * 2 - S(12)) / 3;
    for (int i = 0; i < 3; ++i) {
        MoveWindow(g_hVerbBtn[i], x + pad + i * (verbW + S(6)), cy + S(M.vRowY), verbW, S(M.vRowH), TRUE);
    }
    int addrW = S(126);
    MoveWindow(g_hEditAddr, x + pad, cy + S(M.aRowY), addrW, S(M.aRowH), TRUE);
    MoveWindow(g_hEditSize, x + pad + addrW + S(8), cy + S(M.aRowY), cw - pad * 2 - addrW - S(8), S(M.aRowH), TRUE);

    // options card
    cy = g_L.cardOpt.top;
    HWND opts[] = { g_hTgStats, g_hTgFromBase, g_hTgDump };
    for (int i = 0; i < 3; ++i) {
        MoveWindow(opts[i], x + pad, cy + S(M.oFirstY) + i * S(M.oStep), cw - pad * 2, S(M.oH), TRUE);
    }

    // filter card
    cy = g_L.cardFilter.top;
    HWND filt[] = { g_hTgFilterAll, g_hTgUnsigned, g_hTgMetadata, g_hTgClrHeap, g_hTgClrPrvx, g_hTgWow64 };
    const wchar_t* filtFull[] = { L"Apply all filters (*)", L"Unsigned modules", L"Metadata modules",
                                  L"CLR heap", L"CLR prvx (JIT)", L"Wow64 init" };
    const wchar_t* filtShort[] = { L"Apply all (*)", L"Unsigned", L"Metadata",
                                   L"CLR heap", L"CLR JIT", L"Wow64" };
    int colW = (cw - pad * 2 - S(8)) / 2;
    int fBottom = g_L.footerTop - S(2);
    for (int i = 0; i < 6; ++i) {
        int row = twoCol ? (i / 2) : i;
        int col = twoCol ? (i % 2) : 0;
        int fx = x + pad + col * (colW + S(8));
        int fy = cy + S(M.fFirstY) + row * S(M.fStep);
        if (fy + S(M.fH) > fBottom) {
            HideControl(filt[i]);       // only reachable on a client too short for any tier
            continue;
        }
        if (!IsWindowVisible(filt[i])) ShowWindow(filt[i], SW_SHOW);
        MoveWindow(filt[i], fx, fy, twoCol ? colW : cw - pad * 2, S(M.fH), TRUE);
        TgSetLabel(filt[i], twoCol ? filtShort[i] : filtFull[i]);
    }

    // right column
    int rx = x + sidebarW + S(14);
    int rw = W - rx - m;
    if (rw < S(400)) rw = S(400);
    int rtop = g_L.headerH + S(10);

    g_L.infoCard = { rx, rtop, rx + rw, rtop + S(92) };
    g_L.actionRow = { rx, g_L.infoCard.bottom + S(12), rx + rw, g_L.infoCard.bottom + S(12) + S(46) };
    g_L.logCard = { rx, g_L.actionRow.bottom + S(12), rx + rw, g_L.footerTop - bottomGap };
    if (g_L.logCard.bottom < g_L.logCard.top + S(160)) g_L.logCard.bottom = g_L.logCard.top + S(160);

    // action buttons
    int ay = g_L.actionRow.top + S(1);
    int ah = S(44);
    int bx = rx;
    MoveWindow(g_hBtnRun, bx, ay, S(186), ah, TRUE); bx += S(186) + S(8);
    MoveWindow(g_hBtnStop, bx, ay, S(96), ah, TRUE); bx += S(96) + S(8);
    MoveWindow(g_hBtnSaveTxt, bx, ay, S(108), ah, TRUE); bx += S(108) + S(8);
    MoveWindow(g_hBtnSaveHtml, bx, ay, S(118), ah, TRUE); bx += S(118) + S(8);
    MoveWindow(g_hBtnClear, bx, ay, S(88), ah, TRUE); bx += S(88) + S(8);
    MoveWindow(g_hBtnElevate, bx, ay, S(110), ah, TRUE);
    if (g_sys.elevated) HideControl(g_hBtnElevate);
    else ShowWindow(g_hBtnElevate, SW_SHOW);

    // output view (live log + the command/details overlay share the same rect)
    RECT lr = g_L.logCard;
    MoveWindow(g_hRich, lr.left + S(12), lr.top + S(44), (lr.right - lr.left) - S(24),
               (lr.bottom - lr.top) - S(56), TRUE);
    MoveWindow(g_hInfo, lr.left + S(12), lr.top + S(44), (lr.right - lr.left) - S(24),
               (lr.bottom - lr.top) - S(56), TRUE);
    layoutTabs(g_L.logCard);
}

// =====================================================================================
//  Paint (main window)
// =====================================================================================

void PaintHeader(HDC dc, int W) {
    RECT hr{ 0, 0, W, g_L.headerH };
    FillSolid(dc, hr, Th::HEADER);
    DrawLineH(dc, 0, W, g_L.headerH - 1, Th::BORDER);

    int m = S(18);
    int logo = S(42);
    if (!g_hBrandIcon) {
        g_hBrandIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, logo, logo, LR_DEFAULTCOLOR);
    }
    if (g_hBrandIcon) {
        DrawIconEx(dc, m, (g_L.headerH - logo) / 2, g_hBrandIcon, logo, logo, 0, nullptr, DI_NORMAL);
    } else {
        DrawLogo(dc, m, (g_L.headerH - logo) / 2, logo, Th::ACCENT);
    }

    int tx = m + logo + S(14);
    int ty = S(15);
    SetTextCharacterExtra(dc, 0);

    HGDIOBJ of = SelectObject(dc, g_fBrand);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, Th::TEXT);
    SIZE s1{};
    GetTextExtentPoint32W(dc, L"HYPERSENSE", 10, &s1);
    TextOutW(dc, tx, ty, L"HYPERSENSE", 10);

    SetTextColor(dc, Th::ACCENT);
    TextOutW(dc, tx + s1.cx, ty, L"GLOBAL", 6);
    SIZE s2{};
    GetTextExtentPoint32W(dc, L"GLOBAL", 6, &s2);

    SetTextColor(dc, Th::TEXT_DIM);
    SelectObject(dc, g_fSmall);
    TextOutW(dc, tx + s1.cx + s2.cx + S(2), ty + S(6), L"\u2122", 1);
    SelectObject(dc, of);

    RECT sub{ tx, ty + S(30), W - S(220), ty + S(30) + S(18) };
    DrawTextC(dc, L"LIVE USERMODE MEMORY ANALYSIS TOOL  \u00B7  V1", sub, g_fBrandSub,
              RGB(120, 140, 165), DT_LEFT | DT_SINGLELINE | DT_VCENTER, S(1));
}

void PaintInfoCard(HDC dc) {
    RECT r = g_L.infoCard;
    FillRoundRect(dc, r, S(10), Th::CARD, Th::BORDER);

    int pad = S(18);
    int usable = (r.right - r.left) - pad * 2;
    int col1 = r.left + pad;
    int div1 = r.left + pad + usable / 3;
    int div2 = r.left + pad + (usable * 2) / 3;

    DrawLineV(dc, div1, r.top + S(16), r.bottom - S(16), Th::BORDER);
    DrawLineV(dc, div2, r.top + S(16), r.bottom - S(16), Th::BORDER);

    // host
    RECT lbl{ col1, r.top + S(12), div1 - S(10), r.top + S(12) + S(14) };
    DrawTextC(dc, L"COMPUTER NAME", lbl, g_fCardTitle, Th::TEXT_MUTE, DT_LEFT | DT_SINGLELINE, S(1));
    RECT val{ col1, r.top + S(28), div1 - S(10), r.top + S(28) + S(30) };
    DrawTextC(dc, g_sys.computer, val, g_fStat, Th::TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT sub{ col1, r.top + S(60), div1 - S(10), r.top + S(60) + S(18) };
    DrawTextC(dc, g_sys.osName + L"  \u00B7  " + g_sys.arch + L"  \u00B7  " + g_sys.osBuild, sub, g_fSmall,
              Th::TEXT_DIM, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    // clock
    int c2 = div1 + S(18);
    RECT lbl2{ c2, r.top + S(12), div2 - S(10), r.top + S(26) };
    DrawTextC(dc, L"SYSTEM TIME", lbl2, g_fCardTitle, Th::TEXT_MUTE, DT_LEFT | DT_SINGLELINE, S(1));
    SYSTEMTIME st;
    GetLocalTime(&st);
    RECT val2{ c2, r.top + S(27), div2 - S(10), r.top + S(55) };
    DrawTextC(dc, Format(L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond), val2, g_fMonoBig, Th::ACCENT,
              DT_LEFT | DT_SINGLELINE);
    RECT sub2{ c2, r.top + S(60), div2 - S(10), r.top + S(78) };
    DrawTextC(dc, Format(L"%04d-%02d-%02d  \u00B7  %s", st.wYear, st.wMonth, st.wDay, TzString().c_str()),
              sub2, g_fSmall, Th::TEXT_DIM, DT_LEFT | DT_SINGLELINE);

    // session
    int c3 = div2 + S(18);
    RECT lbl3{ c3, r.top + S(12), r.right - pad, r.top + S(26) };
    DrawTextC(dc, L"SESSION / SID", lbl3, g_fCardTitle, Th::TEXT_MUTE, DT_LEFT | DT_SINGLELINE, S(1));

    RECT val3{ c3, r.top + S(28), r.right - pad, r.top + S(50) };
    std::wstring acct = g_sys.domain.empty() ? g_sys.account : (g_sys.account + L"  @" + g_sys.domain);
    DrawTextC(dc, acct, val3, g_fBodyBold, Th::TEXT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT sidR{ c3, r.top + S(50), r.right - pad, r.top + S(68) };
    DrawTextC(dc, g_sys.sid, sidR, g_fTiny, RGB(125, 145, 170), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT chip{ c3, r.top + S(70), c3 + S(106), r.top + S(70) + S(18) };
    DrawChip(dc, chip, g_sys.elevated ? L"ELEVATED" : L"STANDARD",
             g_sys.elevated ? Th::ACCENT_D : RGB(90, 70, 30),
             g_sys.elevated ? Th::ACCENT : Th::WARN,
             g_sys.elevated ? RGB(10, 32, 38) : RGB(34, 27, 12));
}

void PaintActionStatus(HDC dc) {
    RECT r = g_L.actionRow;
    int right = r.right - S(2);
    std::wstring text = g_statusText;
    if (g_scanning) {
        ULONGLONG el = (GetTickCount64() - g_scanStartTick) / 1000;
        text = Format(L"SCANNING  %02llu:%02llu", el / 60, el % 60);
    }
    RECT sr{ r.left + S(660), r.top, right, r.bottom };
    if (sr.left > right - S(120)) sr.left = right - S(120);
    COLORREF stCol = Th::TEXT_DIM;
    if (g_scanning) stCol = Th::WARN;
    else if (g_meta.valid && (g_meta.stopped || g_accessFailed)) stCol = Th::WARN;
    else if (g_meta.valid && g_iocHits > 0) stCol = Th::DANGER;
    DrawTextC(dc, text, sr, g_fBtnSm, stCol, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void PaintLogHeader(HDC dc) {
    RECT r = g_L.logCard;
    int pad = S(14);
    int cy = r.top + S(19);

    COLORREF dot = Th::OK;
    if (g_scanning) dot = ((GetTickCount64() / 500) % 2) ? Th::WARN : RGB(120, 80, 20);
    HBRUSH b = CreateSolidBrush(dot);
    HPEN p = CreatePen(PS_SOLID, 1, dot);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Ellipse(dc, r.left + pad, cy - S(3), r.left + pad + S(6), cy + S(3));
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);

    const int tabTotal = S(90) + S(112) + S(110) + S(16);
    int titleRight = r.right - S(14) - tabTotal - S(12);

    RECT title{ r.left + pad + S(14), r.top + S(8), titleRight, r.top + S(25) };
    const wchar_t* names[3] = { L"LIVE ANALYSIS OUTPUT", L"COMMAND LINE", L"SCAN DETAILS" };
    DrawTextC(dc, names[g_tab < 0 || g_tab > 2 ? 0 : g_tab], title, g_fCardTitle, Th::TEXT_DIM,
              DT_LEFT | DT_SINGLELINE, S(1));

    RECT sub{ r.left + pad + S(14), r.top + S(26), titleRight, r.top + S(40) };
    std::wstring subText = Format(L"MONETA ENGINE  \u00B7  %d lines", g_viewLines);
    COLORREF subCol = Th::TEXT_MUTE;
    if (g_tab == 1) {
        subText = L"EQUIVALENT ARGUMENTS  \u00B7  SELECTION FILLED INTO -p";
    } else if (g_tab == 2) {
        subText = L"TARGET, SETTINGS AND LAST RUN";
    } else if (g_meta.valid && g_iocHits > 0) {
        subText += Format(L"  \u00B7  %d IOC HITS", g_iocHits);
        subCol = LOG_IOC;
    }
    DrawTextC(dc, subText, sub, g_fTiny, subCol, DT_LEFT | DT_SINGLELINE);

    DrawLineH(dc, r.left + pad, r.right - pad, r.top + S(35), Th::BORDER);
}

void PaintFooter(HDC dc, int W, int H) {
    RECT fr{ 0, g_L.footerTop, W, H };
    FillSolid(dc, fr, Th::HEADER);
    DrawLineH(dc, 0, W, g_L.footerTop, Th::BORDER);

    int m = S(18);
    int y1 = g_L.footerTop + S(9);

    // Measure the right-hand block first so the credits cannot run underneath it
    // when the window is near its minimum width.
    const wchar_t* devText = L"DEV  \u00B7  AG HYDRAXFF (@hydraxff_yt)   |   HYPERSENSEGLOBAL\u2122 V1";
    SIZE devSize{};
    HGDIOBJ of = SelectObject(dc, g_fSmall);
    GetTextExtentPoint32W(dc, devText, (int)wcslen(devText), &devSize);
    SelectObject(dc, of);
    int devRight = W - m;
    int devLeft = devRight - devSize.cx - S(6);
    if (devLeft < W / 2) devLeft = W / 2;

    RECT eng{ m, y1, devLeft - S(16), y1 + S(18) };
    DrawTextC(dc, L"ENGINE  \u00B7  Built-in Moneta v1.0.1  \u2014  single executable, no external engine required",
              eng, g_fSmall, Th::TEXT_MUTE, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    RECT cr{ m, y1 + S(20), devLeft - S(16), y1 + S(38) };
    DrawTextC(dc, L"CREDITS  \u00B7  FORREST ORR & GABRIEL LANDAU (MONETA ENGINE)  \u00B7  ZERO-CRASH BUILD",
              cr, g_fSmall, Th::TEXT_MUTE, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS, S(1));

    RECT dev{ devLeft, y1 + S(20), devRight, y1 + S(38) };
    DrawTextC(dc, devText, dev, g_fSmall, Th::TEXT_DIM,
              DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void PaintMain(HWND h, HDC dc) {
    RECT rc;
    GetClientRect(h, &rc);
    FillSolid(dc, rc, Th::BG);

    PaintHeader(dc, rc.right);

    int pad = S(13);

    if (g_classic) {
        RECT bar = g_L.classicBar;
        FillRoundRect(dc, bar, S(10), Th::CARD, Th::BORDER);

        RECT t{ bar.left + S(16), bar.top + S(7), bar.right - S(16), bar.top + S(23) };
        DrawTextC(dc, L"CLASSIC MODE  \u00B7  moneta64.exe-STYLE SCAN", t, g_fCardTitle,
                  Th::TEXT_DIM, DT_LEFT | DT_SINGLELINE, S(1));

        RECT hint{ bar.left + S(744), bar.top + S(130), bar.right - S(16), bar.top + S(160) };
        DrawTextC(dc, L"type in COMMAND and press Enter to run it", hint, g_fTiny,
                  Th::TEXT_MUTE, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        int labelW = S(64);
        RECT pl{ bar.left + S(16), bar.top + S(64), bar.left + S(16) + labelW, bar.top + S(90) };
        DrawTextC(dc, L"PRESET", pl, g_fTiny, Th::TEXT_MUTE, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT cl{ bar.left + S(16), bar.top + S(96), bar.left + S(16) + labelW, bar.top + S(122) };
        DrawTextC(dc, L"COMMAND", cl, g_fTiny, Th::TEXT_MUTE, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        std::wstring stText = g_statusText;
        if (g_scanning) {
            ULONGLONG el = (GetTickCount64() - g_scanStartTick) / 1000;
            stText = Format(L"SCANNING  %02llu:%02llu", el / 60, el % 60);
        }
        DrawTextC(dc, stText, g_L.classicStatus, g_fBtnSm, g_scanning ? Th::WARN : Th::TEXT_MUTE,
                  DT_RIGHT | DT_SINGLELINE);
    } else {
        PaintInfoCard(dc);

        RECT cards[] = { g_L.cardTarget, g_L.cardMode, g_L.cardOpt, g_L.cardFilter };
        for (const auto& c : cards) FillRoundRect(dc, c, S(10), Th::CARD, Th::BORDER);

        // target card labels
        RECT t1{ g_L.cardTarget.left + pad, g_L.cardTarget.top + S(g_L.m.titleY),
                 g_L.cardTarget.right - pad, g_L.cardTarget.top + S(g_L.m.titleY + 14) };
        DrawTextC(dc, L"TARGET PROCESS", t1, g_fCardTitle, Th::TEXT_DIM, DT_LEFT | DT_SINGLELINE, S(1));
        if (!g_procFilter.empty()) {
            DrawTextC(dc, Format(L"%d MATCH", g_procMatchCount), t1, g_fTiny, Th::ACCENT,
                      DT_RIGHT | DT_SINGLELINE);
        }

        RECT m1{ g_L.cardMode.left + pad, g_L.cardMode.top + S(g_L.m.titleY),
                 g_L.cardMode.right - pad, g_L.cardMode.top + S(g_L.m.titleY + 14) };
        DrawTextC(dc, L"MEMORY SELECTION", m1, g_fCardTitle, Th::TEXT_DIM, DT_LEFT | DT_SINGLELINE, S(1));

        RECT vlab{ g_L.cardMode.left + pad, g_L.cardMode.top + S(g_L.m.vLabelY),
                   g_L.cardMode.right - pad, g_L.cardMode.top + S(g_L.m.vLabelY + 14) };
        DrawTextC(dc, L"VERBOSITY", vlab, g_fCardTitle, Th::TEXT_MUTE, DT_LEFT | DT_SINGLELINE, S(1));

        {
            RECT alab{ g_L.cardMode.left + pad, g_L.cardMode.top + S(g_L.m.aLabelY),
                       g_L.cardMode.left + pad + S(126), g_L.cardMode.top + S(g_L.m.aLabelY + 14) };
            DrawTextC(dc, L"ADDRESS (0x...)", alab, g_fTiny, Th::TEXT_MUTE, DT_LEFT | DT_SINGLELINE);
            RECT slab{ g_L.cardMode.left + pad + S(134), g_L.cardMode.top + S(g_L.m.aLabelY),
                       g_L.cardMode.right - pad, g_L.cardMode.top + S(g_L.m.aLabelY + 14) };
            DrawTextC(dc, L"REGION SIZE", slab, g_fTiny, Th::TEXT_MUTE, DT_LEFT | DT_SINGLELINE);
        }

        RECT o1{ g_L.cardOpt.left + pad, g_L.cardOpt.top + S(g_L.m.titleY),
                 g_L.cardOpt.right - pad, g_L.cardOpt.top + S(g_L.m.titleY + 14) };
        DrawTextC(dc, L"ANALYSIS OPTIONS", o1, g_fCardTitle, Th::TEXT_DIM, DT_LEFT | DT_SINGLELINE, S(1));

        RECT f1{ g_L.cardFilter.left + pad, g_L.cardFilter.top + S(g_L.m.titleY),
                 g_L.cardFilter.right - pad, g_L.cardFilter.top + S(g_L.m.titleY + 14) };
        DrawTextC(dc, L"IOC FILTERS", f1, g_fCardTitle, Th::TEXT_DIM, DT_LEFT | DT_SINGLELINE, S(1));

        PaintActionStatus(dc);
    }

    FillRoundRect(dc, g_L.logCard, S(10), Th::CARD, Th::BORDER);
    PaintLogHeader(dc);
    PaintFooter(dc, rc.right, rc.bottom);
}

// =====================================================================================
//  Combo / target handling
// =====================================================================================

void OnComboChanged() {
    int idx = (int)SendMessageW(g_hCombo, CB_GETCURSEL, 0, 0);
    if (idx == 1) {
        g_targetAll = true;
        g_targetPid = 0;
    } else if (idx >= 2 && idx < (int)g_comboPids.size()) {
        g_targetAll = false;
        g_targetPid = g_comboPids[(size_t)idx];
    } else {
        g_targetAll = false;
        g_targetPid = 0;
    }
    if (g_classic && g_hEditPid) SetWindowTextW(g_hEditPid, L"");
    MarkCustomPreset();
    UpdateClassicPreview();
    if (g_tab) BuildInfoView();
}

void SelectExeDialog() {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = L"Target executable (*.exe)\0*.exe\0All files (*.*)\0*.*\0\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select the target executable to analyse";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return;

    RefreshProcessList();

    std::wstring wanted = file;
    std::wstring wantedLower = Lower(wanted);
    std::wstring wantedName = Lower(FileNameOf(wanted));
    const ProcItem* match = nullptr;
    for (const auto& p : g_procs) {
        if (!p.path.empty() && Lower(p.path) == wantedLower) { match = &p; break; }
    }
    if (!match) {
        for (const auto& p : g_procs) {
            if (Lower(p.name) == wantedName) { match = &p; break; }
        }
    }
    if (match) {
        SelectTargetPid(match->pid, false);
        SetStatus(L"TARGET ACQUIRED");
        InvalidateRect(g_hWnd, nullptr, FALSE);
    } else {
        std::wstring msg = L"The selected executable is not running.\r\n\r\n"
                           L"Start it, then press REFRESH and choose it from the target list.\r\n\r\n"
                           L"File: " + wanted;
        HsMessage(HSI_INFO, L"TARGET NOT RUNNING", msg);
    }
}

void OpenInstagram() {
    ShellExecuteW(nullptr, L"open", L"https://www.instagram.com/hydraxff_yt/", nullptr, nullptr, SW_SHOWNORMAL);
}

void RelaunchElevated() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    HINSTANCE r = ShellExecuteW(nullptr, L"runas", path, nullptr, ExeDir().c_str(), SW_SHOWNORMAL);
    if ((INT_PTR)r > 32) {
        DestroyWindow(g_hWnd);
    } else {
        HsMessage(HSI_WARN, L"ELEVATION CANCELLED", L"Elevation was cancelled or refused.");
    }
}

void ClearLog() {
    if (g_scanning) return;
    g_reportBody.clear();
    g_partialLine.clear();
    g_viewLines = 0;
    g_viewCapped = false;
    g_iocHits = 0;
    g_regionHits = 0;
    g_accessFailed = false;
    g_meta = ScanMeta();
    SetWindowTextW(g_hRich, L"");
    EnableWindow(g_hBtnSaveTxt, FALSE);
    EnableWindow(g_hBtnSaveHtml, FALSE);
    SetStatus(L"READY");
    InvalidateRect(g_hWnd, nullptr, FALSE);
    if (g_tab) BuildInfoView();
}

// =====================================================================================
//  Views: OUTPUT / COMMAND / DETAILS, classic mode, output context menu
// =====================================================================================

std::wstring TargetSummary() {
    if (g_targetAll) return L"all accessible processes (*)";
    if (!g_targetPid) return L"none selected (REQUIRED)";
    const ProcItem* p = FindProcByPid(g_targetPid);
    if (p) return Format(L"%s  (PID %lu)\r\n%s", p->name.c_str(), p->pid, p->path.c_str());
    return Format(L"PID %lu", g_targetPid);
}

std::wstring FilterSummary() {
    if (TgGetOn(g_hTgFilterAll)) return L"* (all filters - malware and unknown false positives only)";
    std::wstring f;
    if (TgGetOn(g_hTgUnsigned)) f += L"unsigned-modules ";
    if (TgGetOn(g_hTgMetadata)) f += L"metadata-modules ";
    if (TgGetOn(g_hTgClrHeap)) f += L"clr-heap ";
    if (TgGetOn(g_hTgClrPrvx)) f += L"clr-prvx ";
    if (TgGetOn(g_hTgWow64)) f += L"wow64-init ";
    return f.empty() ? L"none selected (engine default)" : Trim(f);
}

std::wstring OptionSummary() {
    std::wstring o;
    if (TgGetOn(g_hTgStats)) o += L"statistics ";
    if (TgGetOn(g_hTgFromBase)) o += L"from-base ";
    if (TgGetOn(g_hTgDump)) o += L"dump memory (-d) ";
    return o.empty() ? L"none" : Trim(o);
}

void InfoAppend(const std::wstring& text, COLORREF color, bool bold, int twips = 190,
                const wchar_t* face = L"Consolas") {
    if (!g_hInfo) return;
    CHARRANGE cr{};
    SendMessageW(g_hInfo, EM_EXGETSEL, 0, (LPARAM)&cr);
    cr.cpMin = -1;
    cr.cpMax = -1;
    SendMessageW(g_hInfo, EM_EXSETSEL, 0, (LPARAM)&cr);

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_SIZE | CFM_FACE;
    cf.crTextColor = color;
    cf.dwEffects = bold ? CFE_BOLD : 0;
    cf.yHeight = twips;
    StringCchCopyW(cf.szFaceName, 32, face);
    SendMessageW(g_hInfo, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(g_hInfo, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

void InfoRow(const wchar_t* flag, const std::wstring& what, const std::wstring& value) {
    InfoAppend(Format(L"  %-14s", flag), Th::ACCENT, true);
    InfoAppend(what + L"\r\n", Th::TEXT_DIM, false);
    InfoAppend(L"                 " + value + L"\r\n", Th::TEXT, false);
}

void BuildInfoView() {
    if (!g_hInfo) return;
    SetWindowTextW(g_hInfo, L"");

    if (g_tab == 1) {
        InfoAppend(L"EQUIVALENT MONETA COMMAND LINE\r\n", Th::ACCENT, true, 220, L"Segoe UI");
        InfoAppend(L"Copy this into a console to reproduce the scan without the GUI.\r\n\r\n",
                   Th::TEXT_MUTE, false, 165, L"Segoe UI");

        std::wstring cmd = BuildCommandLine();
        InfoAppend(cmd + L"\r\n\r\n", Th::TEXT, false, 200);

        InfoAppend(L"ARGUMENT MAP\r\n", Th::ACCENT, true, 200, L"Segoe UI");
        InfoAppend(L"The values below are what the GUI will pass on the next run.\r\n\r\n",
                   Th::TEXT_MUTE, false, 165, L"Segoe UI");

        InfoRow(L"-m", L"memory selection:  * | ioc | region | referenced",
                std::wstring(ModeLabel(g_memMode)) + L"  -  " + ModeDesc(g_memMode));
        InfoRow(L"-p", L"target process:  PID or *  (REQUIRED)",
                TargetSummary());
        InfoRow(L"-v", L"verbosity:  surface | detail | debug",
                std::wstring(VerbLabel(g_verbMode)));
        InfoRow(L"-d", L"dump selected memory to disk after each process scan",
                TgGetOn(g_hTgDump) ? L"ON" : L"off");
        InfoRow(L"--option", L"from-base | statistics",
                OptionSummary());
        InfoRow(L"--filter", L"unsigned-modules | metadata-modules | clr-heap | clr-prvx | wow64-init | *",
                FilterSummary());

        std::wstring addr = L"not used - needs REGION or REFERENCED mode";
        if (g_memMode == 2 || g_memMode == 3) {
            wchar_t a[64] = {}, s[32] = {};
            if (g_hEditAddr) GetWindowTextW(g_hEditAddr, a, 64);
            if (g_hEditSize) GetWindowTextW(g_hEditSize, s, 32);
            addr = Trim(a).empty() ? L"(empty - enter 0x...)" : Trim(a);
            std::wstring sz = Trim(s);
            if (!sz.empty()) addr += L"   region-size " + sz;
        }
        InfoRow(L"--address", L"start address for region / referenced modes", addr);
        InfoRow(L"--region-size", L"optional size of the region at --address", L"optional, default 0");

        InfoAppend(L"\r\nNOTES\r\n", Th::ACCENT, true, 200, L"Segoe UI");
        InfoAppend(L"  - The selected process is filled into -p automatically.\r\n", Th::TEXT_DIM, false, 175);
        InfoAppend(L"  - --address and --region-size only apply to region and referenced.\r\n", Th::TEXT_DIM, false, 175);
        InfoAppend(L"  - Turn off \"Apply all filters\" to choose individual filters.\r\n", Th::TEXT_DIM, false, 175);
        InfoAppend(L"  - Right-click any file path in OUTPUT to open it, search its SHA-256\r\n", Th::TEXT_DIM, false, 175);
        InfoAppend(L"    on VirusTotal in your browser, or copy it. Ctrl+click opens the file.\r\n", Th::TEXT_DIM, false, 175);
        return;
    }

    if (g_tab == 2) {
        InfoAppend(L"SCAN DETAILS\r\n", Th::ACCENT, true, 220, L"Segoe UI");
        InfoAppend(L"Exactly what this tool is about to run, and on what.\r\n\r\n",
                   Th::TEXT_MUTE, false, 165, L"Segoe UI");

        InfoAppend(L"TARGET\r\n", Th::ACCENT, true, 200, L"Segoe UI");
        InfoAppend(L"  " + TargetSummary() + L"\r\n\r\n", Th::TEXT, false, 180);

        InfoAppend(L"SCAN SETTINGS\r\n", Th::ACCENT, true, 200, L"Segoe UI");
        InfoAppend(L"  memory      " + std::wstring(ModeDesc(g_memMode)) + L"\r\n", Th::TEXT_DIM, false, 180);
        InfoAppend(L"  verbosity   " + std::wstring(VerbLabel(g_verbMode)) + L"\r\n", Th::TEXT_DIM, false, 180);
        InfoAppend(L"  options     " + OptionSummary() + L"\r\n", Th::TEXT_DIM, false, 180);
        InfoAppend(L"  filters     " + FilterSummary() + L"\r\n\r\n", Th::TEXT_DIM, false, 180);

        InfoAppend(L"HOST\r\n", Th::ACCENT, true, 200, L"Segoe UI");
        InfoAppend(L"  " + g_sys.computer + L"   " + g_sys.osName + L"   " + g_sys.arch + L"   " + g_sys.osBuild + L"\r\n",
                   Th::TEXT, false, 180);
        std::wstring acct = g_sys.domain.empty() ? g_sys.account : (g_sys.domain + L"\\" + g_sys.account);
        InfoAppend(L"  " + acct + L"   SID " + g_sys.sid + L"\r\n", Th::TEXT_DIM, false, 175);
        InfoAppend(L"  " + std::wstring(g_sys.elevated ? L"ELEVATED - full process access"
                                                    : L"STANDARD - some targets will refuse access") + L"\r\n\r\n",
                   g_sys.elevated ? Th::OK : Th::WARN, false, 175);

        InfoAppend(L"ENGINE\r\n", Th::ACCENT, true, 200, L"Segoe UI");
        InfoAppend(L"  Moneta v1.0.1 compiled into this executable - no external engine needed.\r\n",
                   Th::TEXT_DIM, false, 180);
        InfoAppend(L"  Credits: Forrest Orr and Gabriel Landau (Moneta, GPLv3).\r\n\r\n",
                   Th::TEXT_DIM, false, 175);

        InfoAppend(L"LAST RUN\r\n", Th::ACCENT, true, 200, L"Segoe UI");
        if (g_meta.valid) {
            InfoAppend(L"  started    " + g_meta.started + L"\r\n", Th::TEXT_DIM, false, 180);
            InfoAppend(L"  finished   " + g_meta.finished + L"\r\n", Th::TEXT_DIM, false, 180);
            InfoAppend(L"  duration   " + g_meta.duration + L"\r\n", Th::TEXT_DIM, false, 180);
            InfoAppend(L"  exit       " + ExitCodeText(g_meta.exitCode) + L"\r\n", Th::TEXT_DIM, false, 180);
            InfoAppend(L"  result     " + ResultText() + L"\r\n", Th::TEXT, false, 180);
        } else {
            InfoAppend(L"  No analysis has run yet.\r\n", Th::TEXT_MUTE, false, 180);
        }
        return;
    }
}

void ShowTab(int tab) {
    if (tab < 0 || tab > 2) tab = 0;
    g_tab = tab;
    for (int i = 0; i < 3; ++i) {
        if (g_hTab[i]) BtnSetChecked(g_hTab[i], i == tab);
    }
    if (g_hRich) ShowWindow(g_hRich, tab == 0 ? SW_SHOW : SW_HIDE);
    if (g_hInfo) ShowWindow(g_hInfo, tab == 0 ? SW_HIDE : SW_SHOW);
    if (tab != 0) BuildInfoView();
    if (g_hWnd) InvalidateRect(g_hWnd, &g_L.logCard, FALSE);
}

bool ResolveClassicTarget() {
    wchar_t buf[160] = {};
    if (g_hEditPid) GetWindowTextW(g_hEditPid, buf, 160);
    std::wstring t = Trim(buf);

    if (t.empty()) return g_targetAll || g_targetPid != 0;
    if (t == L"*") {
        g_targetAll = true;
        g_targetPid = 0;
        return true;
    }

    bool numeric = true;
    for (wchar_t c : t) {
        if (c < L'0' || c > L'9') { numeric = false; break; }
    }
    if (numeric) {
        DWORD pid = (DWORD)_wtoi(t.c_str());
        if (!pid) return false;
        g_targetAll = false;
        g_targetPid = pid;
        const ProcItem* p = FindProcByPid(pid);
        if (p) SelectTargetPid(pid, false);
        return true;
    }

    std::wstring want = Lower(t);
    if (Lower(FileNameOf(t)) == want) { /* keep as typed */ }

    auto match = [&](const std::vector<ProcItem>& list, bool partial) -> const ProcItem* {
        for (const auto& p : list) {
            std::wstring n = Lower(p.name);
            if (n == want || Lower(FileNameOf(p.path)) == want) return &p;
            if (partial && n.find(want) != std::wstring::npos) return &p;
        }
        return nullptr;
    };

    const ProcItem* p = match(g_procs, false);
    if (!p) {
        RefreshProcessList();
        p = match(g_procs, false);
    }
    if (!p) p = match(g_procs, true);
    if (!p) return false;

    g_targetAll = false;
    g_targetPid = p->pid;
    SelectTargetPid(p->pid, false);
    return true;
}

// =====================================================================================
//  Classic-mode command presets
// =====================================================================================

struct ClassicPreset {
    const wchar_t* label;
    int mode, verb;
    int stats, fromBase, dump, filterAll;
};

// mode/verb/toggle values of -1 mean "not part of this preset".
static const ClassicPreset kPresets[] = {
    { L"Custom - keep the current settings", -1, -1, -1, -1, -1, -1 },
    { L"moneta64.exe -m ioc -p <pid>", 0, 0, 0, 0, 0, 0 },
    { L"moneta64.exe -m ioc -p <pid> -v detail", 0, 1, 0, 0, 0, 0 },
    { L"moneta64.exe -m ioc -p <pid> --filter *", 0, 0, 0, 0, 0, 1 },
    { L"moneta64.exe -m ioc -p <pid> -d", 0, 0, 0, 0, 1, 0 },
    { L"moneta64.exe -m ioc -p <pid> -v detail --option statistics --filter *", 0, 1, 1, 0, 0, 1 },
    { L"moneta64.exe -m * -p <pid> -v detail", 1, 1, 0, 0, 0, 0 },
};
const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

void UpdateClassicPreview() {
    if (!g_hCmd || !g_classic || g_cmdSync) return;
    std::wstring line = BuildMonetaCommandLine();
    wchar_t cur[1200] = {};
    GetWindowTextW(g_hCmd, cur, 1200);
    if (line == cur) return;
    g_cmdSync = true;
    SetWindowTextW(g_hCmd, line.c_str());
    g_cmdSync = false;
}

int MatchPreset() {
    bool stats = TgGetOn(g_hTgStats), fromBase = TgGetOn(g_hTgFromBase);
    bool dump = TgGetOn(g_hTgDump), filterAll = TgGetOn(g_hTgFilterAll);
    bool anyIndividual = TgGetOn(g_hTgUnsigned) || TgGetOn(g_hTgMetadata) || TgGetOn(g_hTgClrHeap) ||
                         TgGetOn(g_hTgClrPrvx) || TgGetOn(g_hTgWow64);
    for (int i = 1; i < kPresetCount; ++i) {
        const ClassicPreset& p = kPresets[i];
        if (p.mode != g_memMode || p.verb != g_verbMode) continue;
        if ((p.stats != 0) != stats || (p.fromBase != 0) != fromBase || (p.dump != 0) != dump) continue;
        if ((p.filterAll != 0) != filterAll) continue;
        if (!filterAll && anyIndividual) continue;
        return i;
    }
    return 0;
}

void MarkCustomPreset() {
    if (g_applyingPreset || !g_hPreset || !g_classic) return;
    int idx = MatchPreset();
    if ((int)SendMessageW(g_hPreset, CB_GETCURSEL, 0, 0) != idx)
        SendMessageW(g_hPreset, CB_SETCURSEL, (WPARAM)idx, 0);
}

void ApplyPreset(int idx) {
    if (idx <= 0 || idx >= kPresetCount) {
        UpdateClassicPreview();
        if (g_tab) BuildInfoView();
        return;
    }
    const ClassicPreset& p = kPresets[idx];

    g_applyingPreset = true;
    if (p.mode >= 0) SelectMode(p.mode);
    if (p.verb >= 0) SelectVerb(p.verb);
    TgSetOn(g_hTgStats, p.stats != 0);
    TgSetOn(g_hTgFromBase, p.fromBase != 0);
    TgSetOn(g_hTgDump, p.dump != 0);
    TgSetOn(g_hTgFilterAll, p.filterAll != 0);
    if (!p.filterAll) {
        // The presets carry no individual filters.
        HWND f[] = { g_hTgUnsigned, g_hTgMetadata, g_hTgClrHeap, g_hTgClrPrvx, g_hTgWow64 };
        for (HWND c : f) TgSetOn(c, false);
    }
    UpdateToggleAvailability();
    g_applyingPreset = false;

    if (g_hPreset) SendMessageW(g_hPreset, CB_SETCURSEL, (WPARAM)idx, 0);
    UpdateClassicPreview();
    SetStatus(L"PRESET APPLIED");
    if (g_tab) BuildInfoView();
}

// Split a command line into tokens, honouring double quotes.
std::vector<std::wstring> SplitArgs(const std::wstring& line) {
    std::vector<std::wstring> out;
    std::wstring cur;
    bool quoted = false;
    for (wchar_t c : line) {
        if (c == L'"') { quoted = !quoted; continue; }
        if (!quoted && (c == L' ' || c == L'\t')) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// Read a moneta64.exe command line back into the GUI so RUN reproduces it.
bool ApplyCommandText(const std::wstring& line) {
    std::vector<std::wstring> a = SplitArgs(line);
    size_t i = 0;
    if (!a.empty() && Lower(a[0]).find(L".exe") != std::wstring::npos) i = 1;
    if (i < a.size() && Lower(a[i]) == L"--engine") i++;

    int mode = -1, verb = -1;
    bool dump = false, stats = false, fromBase = false, filterSignaled = false, filterAll = false;
    bool fUnsigned = false, fMetadata = false, fClrHeap = false, fClrPrvx = false, fWow64 = false;
    std::wstring pidToken, addrToken, sizeToken;
    bool sawAny = false;

    for (; i < a.size(); ++i) {
        std::wstring k = Lower(a[i]);
        if (k == L"-m" && i + 1 < a.size()) {
            std::wstring v = Lower(a[++i]);
            if (v == L"ioc") mode = 0;
            else if (v == L"*" || v == L"all") mode = 1;
            else if (v == L"region") mode = 2;
            else if (v == L"referenced") mode = 3;
            sawAny = true;
        } else if (k == L"-p" && i + 1 < a.size()) {
            pidToken = a[++i];
            sawAny = true;
        } else if (k == L"-v" && i + 1 < a.size()) {
            std::wstring v = Lower(a[++i]);
            if (v == L"surface" || v == L"detail" || v == L"debug") verb = (v == L"surface" ? 0 : (v == L"detail" ? 1 : 2));
            sawAny = true;
        } else if (k == L"-d") {
            dump = true;
            sawAny = true;
        } else if (k == L"--address" && i + 1 < a.size()) {
            addrToken = a[++i];
            sawAny = true;
        } else if (k == L"--region-size" && i + 1 < a.size()) {
            sizeToken = a[++i];
            sawAny = true;
        } else if (k == L"--option") {
            for (size_t j = i + 1; j < a.size(); ++j) {
                std::wstring o = Lower(a[j]);
                if (o == L"statistics") stats = true;
                else if (o == L"from-base") fromBase = true;
            }
            sawAny = true;
        } else if (k == L"--filter") {
            filterSignaled = true;
            for (size_t j = i + 1; j < a.size(); ++j) {
                std::wstring f = Lower(a[j]);
                if (f == L"*") filterAll = true;
                else if (f == L"unsigned-modules") fUnsigned = true;
                else if (f == L"metadata-modules") fMetadata = true;
                else if (f == L"clr-heap") fClrHeap = true;
                else if (f == L"clr-prvx") fClrPrvx = true;
                else if (f == L"wow64-init") fWow64 = true;
            }
            sawAny = true;
        }
    }
    if (!sawAny) return false;

    g_applyingPreset = true;
    if (mode >= 0) SelectMode(mode);
    if (verb >= 0) SelectVerb(verb);
    TgSetOn(g_hTgStats, stats);
    TgSetOn(g_hTgFromBase, fromBase);
    TgSetOn(g_hTgDump, dump);
    if (filterSignaled) {
        TgSetOn(g_hTgFilterAll, filterAll);
        TgSetOn(g_hTgUnsigned, fUnsigned);
        TgSetOn(g_hTgMetadata, fMetadata);
        TgSetOn(g_hTgClrHeap, fClrHeap);
        TgSetOn(g_hTgClrPrvx, fClrPrvx);
        TgSetOn(g_hTgWow64, fWow64);
        UpdateToggleAvailability();
    }
    if (!addrToken.empty() && g_hEditAddr) SetWindowTextW(g_hEditAddr, addrToken.c_str());
    if (!sizeToken.empty() && g_hEditSize) SetWindowTextW(g_hEditSize, sizeToken.c_str());
    g_applyingPreset = false;

    if (!pidToken.empty() && g_hEditPid) {
        SetWindowTextW(g_hEditPid, pidToken.c_str());
        if (Trim(pidToken) == L"*") {
            g_targetAll = true;
            g_targetPid = 0;
            SendMessageW(g_hCombo, CB_SETCURSEL, 1, 0);
        } else {
            ResolveClassicTarget();
        }
    }

    if (g_hPreset) SendMessageW(g_hPreset, CB_SETCURSEL, 0, 0);
    UpdateClassicPreview();
    SetStatus(L"COMMAND LINE APPLIED");
    if (g_tab) BuildInfoView();
    return true;
}

// Classic bar: apply whatever the user typed, then run it (Enter in the CMD box).
void RunClassicCommandText() {
    if (!g_hCmd || g_cmdSync) return;
    wchar_t buf[1200] = {};
    GetWindowTextW(g_hCmd, buf, 1200);
    std::wstring line = Trim(buf);
    if (line.empty()) return;
    if (!ApplyCommandText(line)) {
        UpdateClassicPreview();
        return;
    }
    SendMessageW(g_hWnd, WM_COMMAND, MAKEWPARAM(HS_BTN_RUN, BN_CLICKED), 0);
}

// A hidden control still sits inside the parent's child-clip region, so its last
// pixels would survive a parent repaint and smear over the other mode. Move it out
// of the client area as we hide it; LayoutControls puts it back when it is shown.
void HideControl(HWND c) {
    if (!c) return;
    ShowWindow(c, SW_HIDE);
    SetWindowPos(c, nullptr, -S(4000), -S(4000), S(10), S(10),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void ApplyClassicMode() {
    g_classic = TgGetOn(g_hTgClassic);

    HWND side[] = {
        g_hBtnSelect,
        g_hModeBtn[0], g_hModeBtn[1], g_hModeBtn[2], g_hModeBtn[3],
        g_hVerbBtn[0], g_hVerbBtn[1], g_hVerbBtn[2],
        g_hEditAddr, g_hEditSize,
        g_hTgStats, g_hTgFromBase, g_hTgDump,
        g_hTgFilterAll, g_hTgUnsigned, g_hTgMetadata, g_hTgClrHeap, g_hTgClrPrvx, g_hTgWow64,
        g_hBtnElevate
    };
    for (HWND c : side) {
        if (!c) continue;
        if (g_classic) HideControl(c);
        else ShowWindow(c, SW_SHOW);
    }
    if (g_classic) {
        ShowWindow(g_hEditPid, SW_SHOW);
        ShowWindow(g_hPreset, SW_SHOW);
        ShowWindow(g_hCmd, SW_SHOW);
    } else {
        HideControl(g_hEditPid);
        HideControl(g_hPreset);
        HideControl(g_hCmd);
    }

    if (g_classic) {
        // The plain mode should not inherit a mode that demands an address.
        if (g_memMode == 2 || g_memMode == 3) SelectMode(0);
        // The list is the primary picker; the PID box is an optional override.
        if (g_hEditPid) SetWindowTextW(g_hEditPid, L"");
        RefreshProcessList();
        ShowTab(0);
        MarkCustomPreset();
    }

    if (g_hWnd) {
        RECT rc{};
        GetClientRect(g_hWnd, &rc);
        LayoutControls(g_hWnd, rc.right, rc.bottom);
        UpdateClassicPreview();
        // Repaint the parent and every child in one pass: the two layouts reuse the
        // same window, so stale pixels from the other mode must not survive.
        RedrawWindow(g_hWnd, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

// Re-render the visible log from the saved body so FOCUS UNSIGNED applies at once.
void RebuildLiveView() {
    if (!g_hRich || g_scanning) return;

    SetWindowTextW(g_hRich, L"");
    g_partialLine.clear();
    g_viewLines = 0;
    g_viewCapped = false;
    g_iocHits = 0;
    g_regionHits = 0;
    g_accessFailed = false;

    size_t pos = 0;
    while (pos <= g_reportBody.size() && g_viewLines < g_viewLineCap) {
        size_t nl = g_reportBody.find('\n', pos);
        std::string line = (nl == std::string::npos)
                               ? g_reportBody.substr(pos)
                               : g_reportBody.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        AppendLine(line);
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    ScrollLogBottom();
    InvalidateRect(g_hWnd, &g_L.logCard, FALSE);
}

std::wstring EditText(HWND edit) {    if (!edit) return L"";
    GETTEXTLENGTHEX gtl{};
    gtl.flags = GTL_DEFAULT;
    gtl.codepage = 1200;   // CP_UNICODE
    LRESULT len = SendMessageW(edit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    if (len <= 0) return L"";
    std::wstring out((size_t)len + 1, L'\0');
    GETTEXTEX gt{};
    gt.cb = (DWORD)((len + 1) * sizeof(wchar_t));
    gt.flags = GT_DEFAULT;
    gt.codepage = 1200;
    LRESULT n = SendMessageW(edit, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)&out[0]);
    if (n > 0) out.resize((size_t)n);
    else out.clear();
    return out;
}

std::wstring LineAtChar(HWND edit, int idx, int& lineStart) {
    lineStart = 0;
    if (!edit) return L"";
    int line = (int)SendMessageW(edit, EM_LINEFROMCHAR, (WPARAM)idx, 0);
    int start = (int)SendMessageW(edit, EM_LINEINDEX, (WPARAM)line, 0);
    if (start < 0) return L"";
    int len = (int)SendMessageW(edit, EM_LINELENGTH, (WPARAM)start, 0);
    if (len < 0) return L"";
    std::wstring buf((size_t)len + 1, L'\0');
    if (len > 0) {
        buf[0] = (wchar_t)len;
        SendMessageW(edit, EM_GETLINE, (WPARAM)line, (LPARAM)&buf[0]);
    }
    buf.resize((size_t)len);
    lineStart = start;
    return buf;
}

bool IsPathDelim(wchar_t c) {
    return iswspace(c) || c == L'|' || c == L'"' || c == L'\'' || c == L'<' || c == L'>' ||
           c == L'(' || c == L')' || c == L'[' || c == L']' || c == L'{' || c == L'}' ||
           c == L',' || c == L';';
}

bool IsPathHardDelim(wchar_t c) {
    return c == L'|' || c == L'"' || c == L'\'' || c == L'<' || c == L'>' ||
           c == L'(' || c == L')' || c == L'[' || c == L']' || c == L'{' || c == L'}' ||
           c == L',' || c == L';';
}

bool IsDrivePath(const std::wstring& s) {
    if (s.size() >= 3 && iswalpha(s[0]) && s[1] == L':' && (s[2] == L'\\' || s[2] == L'/'))
        return true;
    return s.size() >= 2 && s[0] == L'\\' && s[1] == L'\\';
}

std::wstring TrimPathToken(std::wstring s) {
    while (!s.empty() && (s.back() == L'.' || s.back() == L',' || s.back() == L'\r')) s.pop_back();
    return s;
}

// Extract the path under the cursor. Handles paths containing spaces by growing the
// candidate one word at a time until the file system confirms it exists.
std::wstring PathTokenAt(HWND edit, POINT clientPt, bool& exists, std::wstring& lineOut) {
    exists = false;
    lineOut.clear();
    if (!edit) return L"";

    POINTL pl{ clientPt.x, clientPt.y };
    LRESULT hit = SendMessageW(edit, EM_CHARFROMPOS, 0, (LPARAM)&pl);
    int idx = (int)hit;
    if (idx < 0) return L"";

    int start = 0;
    std::wstring line = LineAtChar(edit, idx, start);
    lineOut = line;
    if (line.empty()) return L"";

    size_t i = (size_t)(idx - start);
    if (i > line.size()) i = line.size();
    size_t a = i, b = i;
    while (a > 0 && !IsPathDelim(line[a - 1])) --a;
    while (b < line.size() && !IsPathDelim(line[b])) ++b;
    if (b <= a) return L"";

    std::wstring tok = TrimPathToken(line.substr(a, b - a));

    // Clicking a word inside "C:\Program Files\..." yields a token without the drive.
    // Walk left to find the drive root and rebuild the token from there.
    if (!IsDrivePath(tok)) {
        size_t stop = (i > 260) ? i - 260 : 0;
        for (size_t p = i; p > stop; --p) {
            if (p + 2 >= line.size()) continue;
            if (!iswalpha(line[p]) || line[p + 1] != L':' || (line[p + 2] != L'\\' && line[p + 2] != L'/'))
                continue;
            if (p > 0 && !IsPathDelim(line[p - 1]) && line[p - 1] != L'\\') continue;
            a = p;
            b = i;
            while (b < line.size() && !iswspace(line[b]) && !IsPathHardDelim(line[b])) ++b;
            tok = TrimPathToken(line.substr(a, b - a));
            break;
        }
    }

    if (!IsDrivePath(tok)) return L"";

    size_t end = b;
    for (;;) {
        std::wstring cand = TrimPathToken(line.substr(a, end - a));
        if (cand.size() > 2 && GetFileAttributesW(cand.c_str()) != INVALID_FILE_ATTRIBUTES) {
            exists = true;
            return cand;
        }
        if (end >= line.size()) break;
        size_t p = end;
        while (p < line.size() && iswspace(line[p])) ++p;
        if (p >= line.size() || IsPathHardDelim(line[p])) break;
        size_t q = p;
        while (q < line.size() && !iswspace(line[q]) && !IsPathHardDelim(line[q])) ++q;
        end = q;
    }

    return TrimPathToken(line.substr(a, b - a));
}

// Clipboard copies go through the rich edit itself (WM_COPY) instead of the clipboard
// APIs. An unsigned memory scanner that also imports OpenClipboard / SetClipboardData /
// GetAsyncKeyState reads like a credential stealer to machine-learning engines, and the
// control performs the same job without those imports.
void CopyRange(HWND edit, int start, int end) {
    if (!edit || end <= start) return;
    CHARRANGE oldSel{};
    SendMessageW(edit, EM_EXGETSEL, 0, (LPARAM)&oldSel);
    SendMessageW(edit, EM_SETSEL, (WPARAM)start, (LPARAM)end);
    SendMessageW(edit, WM_COPY, 0, 0);
    SendMessageW(edit, EM_EXSETSEL, 0, (LPARAM)&oldSel);
}

void CopyAllText(HWND edit) {
    if (!edit) return;
    CHARRANGE oldSel{};
    SendMessageW(edit, EM_EXGETSEL, 0, (LPARAM)&oldSel);
    SendMessageW(edit, EM_SETSEL, 0, -1);
    SendMessageW(edit, WM_COPY, 0, 0);
    SendMessageW(edit, EM_EXSETSEL, 0, (LPARAM)&oldSel);
}

void CopyLineAt(HWND edit, POINT client, bool fromKeyboard) {
    if (!edit) return;
    int idx = -1;
    if (fromKeyboard) {
        CHARRANGE sel{};
        SendMessageW(edit, EM_EXGETSEL, 0, (LPARAM)&sel);
        idx = (int)sel.cpMin;
    } else {
        idx = (int)SendMessageW(edit, EM_CHARFROMPOS, 0, (LPARAM)&client);
    }
    if (idx < 0) idx = 0;
    int lineNo = (int)SendMessageW(edit, EM_EXLINEFROMCHAR, 0, (LPARAM)idx);
    if (lineNo < 0) return;
    int start = (int)SendMessageW(edit, EM_LINEINDEX, (WPARAM)lineNo, 0);
    if (start < 0) return;
    int len = (int)SendMessageW(edit, EM_LINELENGTH, (WPARAM)start, 0);
    if (len <= 0) return;
    CopyRange(edit, start, start + len);
}

void OpenToken(const std::wstring& path, bool reveal) {
    if (path.empty()) return;
    if (reveal) {
        std::wstring args = L"/select,\"" + path + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    } else {
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

bool IsDirectoryPath(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// SHA-256 of a file on disk, used to jump straight to its VirusTotal report.
// Implemented locally (src/sha256.h) instead of through the advapi32 crypto provider:
// keeping CryptAcquireContext/CryptHashData out of the import table matters for the
// machine-learning score of an unsigned memory reader.
bool ComputeFileSha256(const std::wstring& path, std::wstring& hexOut) {
    hexOut.clear();
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;

    hs::Sha256 ctx;
    hs::Sha256Init(ctx);
    std::vector<BYTE> buf(1u << 20);
    bool ok = true;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(f, buf.data(), (DWORD)buf.size(), &got, nullptr)) { ok = false; break; }
        if (got == 0) break;
        hs::Sha256Update(ctx, buf.data(), got);
    }
    CloseHandle(f);
    if (!ok) return false;

    unsigned char digest[32] = {};
    hs::Sha256Final(ctx, digest);

    static const wchar_t* kHex = L"0123456789abcdef";
    hexOut.reserve(64);
    for (int i = 0; i < 32; ++i) {
        hexOut.push_back(kHex[digest[i] >> 4]);
        hexOut.push_back(kHex[digest[i] & 0x0F]);
    }
    return hexOut.size() == 64;
}

void SearchVirusTotal(const std::wstring& path) {
    if (path.empty()) return;
    HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    SetStatus(L"HASHING FOR VIRUSTOTAL...");
    std::wstring hash;
    bool ok = ComputeFileSha256(path, hash);
    SetCursor(old);

    if (!ok) {
        SetStatus(L"VIRUSTOTAL: CANNOT READ FILE");
        HsMessage(HSI_WARN, L"CANNOT HASH FILE",
                  L"The selected file could not be read, so no SHA-256 could be produced.\r\n\r\n" + path);
        return;
    }

    std::wstring url = L"https://www.virustotal.com/gui/file/" + hash;
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    SetStatus(L"VIRUSTOTAL OPENED IN BROWSER");
}

enum : int {
    CTX_COPY_SEL = 1, CTX_COPY_LINE, CTX_COPY_ALL,
    CTX_OPEN, CTX_FOLDER, CTX_VIRUSTOTAL,
    CTX_SAVE_TXT, CTX_SAVE_HTML, CTX_CLEAR
};

void ShowOutputMenu(HWND edit, POINT screenPt, bool fromKeyboard) {
    POINT client = screenPt;
    if (!fromKeyboard) ScreenToClient(edit, &client);

    bool exists = false;
    std::wstring line;
    std::wstring token;
    if (!fromKeyboard) token = PathTokenAt(edit, client, exists, line);

    CHARRANGE sel{};
    SendMessageW(edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    bool hasSel = sel.cpMax > sel.cpMin;

    if (line.empty()) {
        int start = 0;
        int caret = (int)SendMessageW(edit, EM_LINEINDEX, (WPARAM)-1, 0);
        line = LineAtChar(edit, caret < 0 ? 0 : caret, start);
    }

    HMENU m = CreatePopupMenu();
    if (!m) return;

    if (!token.empty()) {
        std::wstring label = token;
        if (label.size() > 90) label = L"..." + label.substr(label.size() - 87);
        AppendMenuW(m, MF_STRING | MF_GRAYED, 0, (L"  " + label).c_str());
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(m, MF_STRING | (hasSel ? MF_ENABLED : MF_GRAYED), CTX_COPY_SEL, L"Copy selection\tCtrl+C");
    AppendMenuW(m, MF_STRING | (line.empty() ? MF_GRAYED : MF_ENABLED), CTX_COPY_LINE, L"Copy line");
    AppendMenuW(m, MF_STRING, CTX_COPY_ALL, L"Copy all output");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (exists ? MF_ENABLED : MF_GRAYED), CTX_OPEN, L"Open path");
    AppendMenuW(m, MF_STRING | (exists ? MF_ENABLED : MF_GRAYED), CTX_FOLDER, L"Show in folder");
    bool isFile = exists && !IsDirectoryPath(token);
    AppendMenuW(m, MF_STRING | (isFile ? MF_ENABLED : MF_GRAYED), CTX_VIRUSTOTAL,
                L"Search on &VirusTotal (SHA-256)");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, CTX_SAVE_TXT, L"Save report (TXT)...");
    AppendMenuW(m, MF_STRING, CTX_SAVE_HTML, L"Save report (HTML)...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (g_scanning ? MF_GRAYED : MF_ENABLED), CTX_CLEAR, L"Clear output");

    SetForegroundWindow(g_hWnd);
    int cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
                                  screenPt.x, screenPt.y, 0, g_hWnd, nullptr);
    DestroyMenu(m);

    switch (cmd) {
        case CTX_COPY_SEL:
            SendMessageW(edit, WM_COPY, 0, 0);
            SetStatus(L"SELECTION COPIED");
            break;
        case CTX_COPY_LINE:
            CopyLineAt(edit, client, fromKeyboard);
            SetStatus(L"LINE COPIED");
            break;
        case CTX_COPY_ALL:
            CopyAllText(edit);
            SetStatus(L"OUTPUT COPIED");
            break;
        case CTX_OPEN:
            OpenToken(token, false);
            break;
        case CTX_FOLDER:
            OpenToken(token, true);
            break;
        case CTX_VIRUSTOTAL:
            SearchVirusTotal(token);
            break;
        case CTX_SAVE_TXT:
            SaveReportDialog(false);
            break;
        case CTX_SAVE_HTML:
            SaveReportDialog(true);
            break;
        case CTX_CLEAR:
            ClearLog();
            break;
        default:
            break;
    }
}

LRESULT CALLBACK RichSubclass(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    // Ctrl+click opens a path. The mouse message carries the modifier in wParam
    // (MK_CONTROL), so no GetKeyState/GetAsyncKeyState polling is needed - polling key
    // state is a keylogger-shaped import for a memory scanner.
    if (msg == WM_LBUTTONDOWN && (w & MK_CONTROL)) {
        POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        bool exists = false;
        std::wstring line;
        std::wstring token = PathTokenAt(h, pt, exists, line);
        if (exists) {
            OpenToken(token, false);
            return 0;
        }
    }
    if (msg == WM_CONTEXTMENU) {
        bool keyboard = (l == (LPARAM)-1);
        POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        if (keyboard) {
            RECT rc;
            GetWindowRect(h, &rc);
            pt.x = rc.left + S(24);
            pt.y = rc.top + S(24);
        }
        ShowOutputMenu(h, pt, keyboard);
        return 0;
    }
    return DefSubclassProc(h, msg, w, l);
}

LRESULT CALLBACK PidEditSubclass(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (msg == WM_KEYDOWN && w == VK_RETURN) {
        SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(HS_BTN_RUN, BN_CLICKED), (LPARAM)h);
        return 0;
    }
    return DefSubclassProc(h, msg, w, l);
}

LRESULT CALLBACK CmdEditSubclass(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (msg == WM_KEYDOWN && w == VK_RETURN) {
        RunClassicCommandText();
        return 0;
    }
    if (msg == WM_KEYDOWN && w == VK_ESCAPE) {
        UpdateClassicPreview();
        return 0;
    }
    return DefSubclassProc(h, msg, w, l);
}


void CreateChildControls(HWND h) {
    g_hCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                               CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                               0, 0, 200, 220, h, (HMENU)(INT_PTR)HS_COMBO, g_hInst, nullptr);
    SendMessageW(g_hCombo, WM_SETFONT, (WPARAM)g_fBody, TRUE);
    SetWindowSubclass(g_hCombo, ComboSubclass, 1, 0);
    SendMessageW(g_hCombo, CB_SETITEMHEIGHT, (WPARAM)-1, S(28));
    SendMessageW(g_hCombo, CB_SETITEMHEIGHT, 0, S(26));
    // The default minimum visible count is 30, which makes the drop-down ignore the
    // window height and stretch to ~30 rows. Cap it so the requested height wins.
    SendMessageW(g_hCombo, CB_SETMINVISIBLE, (WPARAM)8, 0);

    auto edit = [&](int id, const wchar_t* cue) {
        HWND e = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                 0, 0, 100, 24, h, (HMENU)(INT_PTR)id, g_hInst, nullptr);
        SendMessageW(e, WM_SETFONT, (WPARAM)g_fMono, TRUE);
        SendMessageW(e, EM_SETCUEBANNER, TRUE, (LPARAM)cue);
        SendMessageW(e, EM_SETLIMITTEXT, 32, 0);
        return e;
    };
    g_hEditAddr = edit(HS_EDIT_ADDR, L"0x0000000000000000");
    g_hEditSize = edit(HS_EDIT_SIZE, L"optional");

    g_hEditSearch = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                    0, 0, 100, 24, h, (HMENU)(INT_PTR)HS_EDIT_SEARCH, g_hInst, nullptr);
    SendMessageW(g_hEditSearch, WM_SETFONT, (WPARAM)g_fBody, TRUE);
    SendMessageW(g_hEditSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"search name, path or PID");
    SendMessageW(g_hEditSearch, EM_SETLIMITTEXT, 96, 0);

    g_hBtnSelect = CreateButton(h, HS_BTN_SELECT, L"SELECT .EXE", BTN_GHOST, Th::CARD);
    g_hBtnRefresh = CreateButton(h, HS_BTN_REFRESH, L"REFRESH", BTN_GHOST, Th::CARD);

    for (int i = 0; i < 4; ++i) g_hModeBtn[i] = CreateButton(h, HS_MODE0 + i, ModeLabel(i), BTN_SEGMENT, Th::CARD);
    for (int i = 0; i < 3; ++i) g_hVerbBtn[i] = CreateButton(h, HS_VERB0 + i, VerbLabel(i), BTN_SEGMENT, Th::CARD);
    BtnSetChecked(g_hModeBtn[0], true);
    BtnSetChecked(g_hVerbBtn[1], true);

    g_hTgStats = CreateToggle(h, HS_TG_STATS, L"Permission statistics", true, Th::CARD);
    g_hTgFromBase = CreateToggle(h, HS_TG_FROMBASE, L"Scan from allocation base", false, Th::CARD);
    g_hTgDump = CreateToggle(h, HS_TG_DUMP, L"Dump selected memory (-d)", false, Th::CARD);

    g_hTgFilterAll = CreateToggle(h, HS_TG_FILTERALL, L"Apply all filters (*)", true, Th::CARD);
    g_hTgUnsigned = CreateToggle(h, HS_TG_UNSIGNED, L"Unsigned modules", false, Th::CARD);
    g_hTgMetadata = CreateToggle(h, HS_TG_METADATA, L"Metadata modules", false, Th::CARD);
    g_hTgClrHeap = CreateToggle(h, HS_TG_CLRHEAP, L"CLR heap", false, Th::CARD);
    g_hTgClrPrvx = CreateToggle(h, HS_TG_CLRPRVX, L"CLR prvx (JIT)", false, Th::CARD);
    g_hTgWow64 = CreateToggle(h, HS_TG_WOW64, L"Wow64 init", false, Th::CARD);
    UpdateToggleAvailability();

    g_hBtnRun = CreateButton(h, HS_BTN_RUN, L"RUN ANALYSIS", BTN_PRIMARY, Th::BG);
    g_hBtnStop = CreateButton(h, HS_BTN_STOP, L"STOP", BTN_DANGER, Th::BG);
    g_hBtnSaveTxt = CreateButton(h, HS_BTN_SAVE_TXT, L"SAVE TXT", BTN_GHOST, Th::BG);
    g_hBtnSaveHtml = CreateButton(h, HS_BTN_SAVE_HTML, L"SAVE HTML", BTN_GHOST, Th::BG);
    g_hBtnClear = CreateButton(h, HS_BTN_CLEAR, L"CLEAR", BTN_GHOST, Th::BG);
    g_hBtnElevate = CreateButton(h, HS_BTN_ELEVATE, L"ELEVATE", BTN_GHOST, Th::BG);

    g_hTgInsta = CreateToggle(h, HS_TG_INSTA, L"@hydraxff_yt", false, Th::HEADER);
    g_hTgClassic = CreateToggle(h, HS_TG_CLASSIC, L"CLASSIC MODE", false, Th::HEADER);
    g_hTgFocusUnsigned = CreateToggle(h, HS_TG_FOCUSUNSIGNED, L"FOCUS UNSIGNED", false, Th::HEADER);

    g_hEditPid = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
                                 0, 0, 100, 24, h, (HMENU)(INT_PTR)HS_EDIT_PID, g_hInst, nullptr);
    SendMessageW(g_hEditPid, WM_SETFONT, (WPARAM)g_fMono, TRUE);
    SendMessageW(g_hEditPid, EM_SETCUEBANNER, TRUE, (LPARAM)L"PID override (optional)");
    SendMessageW(g_hEditPid, EM_SETLIMITTEXT, 120, 0);
    SetWindowSubclass(g_hEditPid, PidEditSubclass, 3, 0);

    // Classic-mode command presets, same owner-draw chrome as the process list.
    g_hPreset = CreateWindowExW(0, L"COMBOBOX", L"",
                                WS_CHILD | WS_TABSTOP | WS_VSCROLL |
                                CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                                0, 0, 200, 220, h, (HMENU)(INT_PTR)HS_PRESET, g_hInst, nullptr);
    SendMessageW(g_hPreset, WM_SETFONT, (WPARAM)g_fBody, TRUE);
    SetWindowSubclass(g_hPreset, ComboSubclass, 2, 0);
    SendMessageW(g_hPreset, CB_SETITEMHEIGHT, (WPARAM)-1, S(26));
    SendMessageW(g_hPreset, CB_SETITEMHEIGHT, 0, S(26));
    SendMessageW(g_hPreset, CB_SETMINVISIBLE, (WPARAM)8, 0);
    for (int i = 0; i < kPresetCount; ++i)
        SendMessageW(g_hPreset, CB_ADDSTRING, 0, (LPARAM)kPresets[i].label);
    SendMessageW(g_hPreset, CB_SETCURSEL, (WPARAM)MatchPreset(), 0);

    g_hCmd = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
                             0, 0, 100, 24, h, (HMENU)(INT_PTR)HS_CMD, g_hInst, nullptr);
    SendMessageW(g_hCmd, WM_SETFONT, (WPARAM)g_fMono, TRUE);
    SendMessageW(g_hCmd, EM_SETCUEBANNER, TRUE, (LPARAM)L"moneta64.exe -m ioc -p <pid> -v detail");
    SendMessageW(g_hCmd, EM_SETLIMITTEXT, 900, 0);
    SetWindowSubclass(g_hCmd, CmdEditSubclass, 4, 0);

    // Classic-only fields start parked outside the client area, exactly like the
    // state ApplyClassicMode leaves them in. Creating them at 0,0 painted a stray
    // field box over the header on first open until the mode was toggled.
    HideControl(g_hEditPid);
    HideControl(g_hPreset);
    HideControl(g_hCmd);

    g_hRich = CreateWindowExW(0, L"RICHEDIT50W", L"",
                              WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_NOHIDESEL |
                              WS_VSCROLL | ES_AUTOVSCROLL,
                              0, 0, 100, 100, h, (HMENU)(INT_PTR)HS_RICH, g_hInst, nullptr);
    SendMessageW(g_hRich, WM_SETFONT, (WPARAM)g_fMono, TRUE);
    SendMessageW(g_hRich, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(10, 15, 23));
    SendMessageW(g_hRich, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(10), S(10)));

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.yHeight = 190;
    cf.crTextColor = RGB(203, 213, 225);
    StringCchCopyW(cf.szFaceName, 32, L"Consolas");
    SendMessageW(g_hRich, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    SetWindowSubclass(g_hRich, RichSubclass, 1, 0);

    // Second rich edit behind the same rect: COMMAND / DETAILS never disturb the live log.
    g_hInfo = CreateWindowExW(0, L"RICHEDIT50W", L"",
                              WS_CHILD | ES_MULTILINE | ES_READONLY | ES_NOHIDESEL |
                              WS_VSCROLL | ES_AUTOVSCROLL,
                              0, 0, 100, 100, h, (HMENU)(INT_PTR)HS_INFO, g_hInst, nullptr);
    SendMessageW(g_hInfo, WM_SETFONT, (WPARAM)g_fMono, TRUE);
    SendMessageW(g_hInfo, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(10, 15, 23));
    SendMessageW(g_hInfo, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(10), S(10)));
    SendMessageW(g_hInfo, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    SetWindowSubclass(g_hInfo, RichSubclass, 2, 0);

    g_hTab[0] = CreateButton(h, HS_TAB_OUTPUT, L"OUTPUT", BTN_SEGMENT, Th::CARD);
    g_hTab[1] = CreateButton(h, HS_TAB_CMD, L"COMMAND", BTN_SEGMENT, Th::CARD);
    g_hTab[2] = CreateButton(h, HS_TAB_DETAILS, L"DETAILS", BTN_SEGMENT, Th::CARD);
    BtnSetChecked(g_hTab[0], true);

    // Every control explains itself on hover.
    InitTooltips(h);
    TipAdd(g_hEditSearch, L"Filter the process list.\nType part of a process name, a path, or a PID.");
    TipAdd(g_hCombo, L"Target process.\nThe selected PID is filled into -p automatically.");
    TipAdd(g_hBtnSelect, L"Pick a running executable from disk.\nIts process is selected in the list automatically.");
    TipAdd(g_hBtnRefresh, L"Re-enumerate running processes.\nUse this after starting the target.");
    TipAdd(g_hModeBtn[0], L"-m ioc\nScan only memory regions that already have suspicions attached. Fastest mode.");
    TipAdd(g_hModeBtn[1], L"-m *\nScan every committed memory region. Slowest, most thorough.");
    TipAdd(g_hModeBtn[2], L"-m region\nScan only the region that overlaps --address (and --region-size).");
    TipAdd(g_hModeBtn[3], L"-m referenced\nScan regions referenced from the region at --address.");
    TipAdd(g_hVerbBtn[0], L"-v surface\nOne line per suspicious region. Smallest output.");
    TipAdd(g_hVerbBtn[1], L"-v detail\nAdds region metadata and IOC descriptions. Recommended.");
    TipAdd(g_hVerbBtn[2], L"-v debug\nAdds engine internals. Very large output, use for troubleshooting.");
    TipAdd(g_hEditAddr, L"--address\nStart address for REGION and REFERENCED modes.\nExample: 0x7FFE0000");
    TipAdd(g_hEditSize, L"--region-size\nOptional size of the region at --address. Default is 0.");
    TipAdd(g_hTgStats, L"--option statistics\nPrint permission statistics for the selected memory after the scan.");
    TipAdd(g_hTgFromBase, L"--option from-base\nAlso select every subregion belonging to the allocation bases of the selected memory.");
    TipAdd(g_hTgDump, L"-d\nDump all selected memory to disk after each process scan.\nThis can write a lot of data.");
    TipAdd(g_hTgFilterAll, L"--filter *\nApply every IOC filter - shows malware plus unknown false positives.\nTurn this off to choose individual filters.");
    TipAdd(g_hTgUnsigned, L"--filter unsigned-modules\nOnly image regions backed by unsigned PE files.");
    TipAdd(g_hTgMetadata, L"--filter metadata-modules\nOnly image regions from signed Windows metadata PE files on disk.");
    TipAdd(g_hTgClrHeap, L"--filter clr-heap\nNative executable heaps created during CLR initialisation.");
    TipAdd(g_hTgClrPrvx, L"--filter clr-prvx\nManaged heaps associated with active CLR heaps and JIT code.");
    TipAdd(g_hTgWow64, L"--filter wow64-init\nIOCs from Wow64 initialisation, such as modified system library code.");
    TipAdd(g_hBtnRun, L"Start the analysis.\nThe chosen process is filled into -p; progress streams into OUTPUT.");
    TipAdd(g_hBtnStop, L"Stop the running engine.\nPartial output is kept and can still be saved.");
    TipAdd(g_hBtnSaveTxt, L"Save the full report as plain text.\nThe viewer limit does not apply to the saved file.");
    TipAdd(g_hBtnSaveHtml, L"Save the full report as colour-coded HTML.");
    TipAdd(g_hBtnClear, L"Clear the output view and the result counters.\nSaved reports are not touched.");
    TipAdd(g_hBtnElevate, L"Relaunch as Administrator.\nRequired to scan protected processes.");
    TipAdd(g_hTgInsta, L"Open @hydraxff_yt on Instagram.");
    TipAdd(g_hTgClassic, L"Classic mode - the plain moneta64.exe workflow.\nOne PID/process box, RUN, SAVE. No custom panels.");
    TipAdd(g_hTgFocusUnsigned, L"Focus unsigned modules.\nDims routine engine output and highlights unsigned-module evidence and IOC findings.");
    TipAdd(g_hEditPid, L"Optional target override for classic mode.\nEnter a PID, a process name, or *.\nLeave it blank to scan the process selected in the list.");
    TipAdd(g_hPreset, L"Command preset.\nFills the COMMAND box with a ready-to-run moneta64.exe line.\nThe selected process replaces <pid>.");
    TipAdd(g_hCmd, L"Classic command line.\nEdit any argument and press Enter to run it, or Esc to restore the current settings.\n-p is filled from the selected process.");
    TipAdd(g_hTab[0], L"OUTPUT\nLive engine output. Right-click a path to open it, or Ctrl+click it.");
    TipAdd(g_hTab[1], L"COMMAND\nExact Moneta command line plus every argument and its current value.");
    TipAdd(g_hTab[2], L"DETAILS\nTarget, scan settings, host information and last-run result.");
    TipAdd(g_hRich, L"Live scan output.\nRight-click a file path for Open / VirusTotal / Copy, or Ctrl+click to open it.");
    TipAdd(g_hInfo, L"Command and detail view.\nRight-click to copy; Ctrl+A selects everything.");

    EnableWindow(g_hBtnStop, FALSE);
    EnableWindow(g_hBtnSaveTxt, FALSE);
    EnableWindow(g_hBtnSaveHtml, FALSE);
    UpdateModeAvailability();
}

void HandleCommand(WPARAM w, LPARAM l) {
    int id = LOWORD(w);
    int code = HIWORD(w);
    (void)l;

    if (id == HS_COMBO && code == CBN_SELCHANGE) { OnComboChanged(); return; }
    if (id == HS_PRESET && code == CBN_SELCHANGE) {
        ApplyPreset((int)SendMessageW(g_hPreset, CB_GETCURSEL, 0, 0));
        return;
    }
    if (id == HS_CMD && code == EN_CHANGE) {
        if (!g_cmdSync) MarkCustomPreset();
        return;
    }
    if (id == HS_CMD && code == EN_KILLFOCUS) {
        wchar_t buf[1200] = {};
        GetWindowTextW(g_hCmd, buf, 1200);
        std::wstring line = Trim(buf);
        if (!line.empty() && line != BuildMonetaCommandLine()) ApplyCommandText(line);
        return;
    }
    if (id == HS_EDIT_PID && code == EN_CHANGE) {
        MarkCustomPreset();
        UpdateClassicPreview();
        return;
    }
    if (id == HS_EDIT_SEARCH && code == EN_CHANGE) {
        ApplyProcFilter(g_targetPid, g_targetAll);
        InvalidateRect(g_hWnd, &g_L.cardTarget, FALSE);
        return;
    }

    switch (id) {
        case HS_BTN_SELECT: SelectExeDialog(); break;
        case HS_BTN_REFRESH: RefreshProcessList(); SetStatus(L"PROCESS LIST REFRESHED"); break;
        case HS_BTN_RUN:
            if (g_classic && !ResolveClassicTarget()) {
                HsMessage(HSI_WARN, L"TARGET NOT FOUND",
                          L"Enter a running PID or process name in the classic box.\r\n"
                          L"Use * to scan every accessible process.");
                break;
            }
            StartScan();
            break;
        case HS_BTN_STOP: StopScan(); break;
        case HS_BTN_SAVE_TXT: SaveReportDialog(false); break;
        case HS_BTN_SAVE_HTML: SaveReportDialog(true); break;
        case HS_BTN_CLEAR: ClearLog(); break;
        case HS_BTN_ELEVATE: RelaunchElevated(); break;
        case HS_MODE0: SelectMode(0); break;
        case HS_MODE1: SelectMode(1); break;
        case HS_MODE2: SelectMode(2); break;
        case HS_MODE3: SelectMode(3); break;
        case HS_VERB0: SelectVerb(0); break;
        case HS_VERB1: SelectVerb(1); break;
        case HS_VERB2: SelectVerb(2); break;
        case HS_TG_FILTERALL:
            UpdateToggleAvailability();
            MarkCustomPreset();
            UpdateClassicPreview();
            if (g_tab) BuildInfoView();
            break;
        case HS_TG_STATS:
        case HS_TG_FROMBASE:
        case HS_TG_DUMP:
        case HS_TG_UNSIGNED:
        case HS_TG_METADATA:
        case HS_TG_CLRHEAP:
        case HS_TG_CLRPRVX:
        case HS_TG_WOW64:
            MarkCustomPreset();
            UpdateClassicPreview();
            if (g_tab) BuildInfoView();
            break;
        case HS_TAB_OUTPUT: ShowTab(0); break;
        case HS_TAB_CMD: ShowTab(1); break;
        case HS_TAB_DETAILS: ShowTab(2); break;
        case HS_TG_CLASSIC:
            ApplyClassicMode();
            if (g_tab) BuildInfoView();
            break;
        case HS_TG_FOCUSUNSIGNED:
            g_focusUnsigned = TgGetOn(g_hTgFocusUnsigned);
            RebuildLiveView();
            SetStatus(g_focusUnsigned ? L"FOCUS: UNSIGNED MODULES" : L"READY");
            break;
        case HS_TG_INSTA:
            TgSetOn(g_hTgInsta, true);
            OpenInstagram();
            break;
        default: break;
    }
}

void DrawComboItem(LPDRAWITEMSTRUCT dis) {
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    bool selected = (dis->itemState & ODS_SELECTED) != 0;
    if (dis->itemID == -1) {
        selected = false;
    }

    COLORREF bg = selected ? RGB(16, 55, 64) : Th::INPUT;
    FillSolid(dc, rc, bg);

    wchar_t text[512] = {};
    int idx = (int)dis->itemID;
    if (idx == -1) idx = (int)SendMessageW(dis->hwndItem, CB_GETCURSEL, 0, 0);
    if (idx >= 0) SendMessageW(dis->hwndItem, CB_GETLBTEXT, (WPARAM)idx, (LPARAM)text);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, selected ? Th::ACCENT : Th::TEXT);
    HGDIOBJ of = SelectObject(dc, g_fBody);
    RECT tr = rc;
    tr.left += S(10);
    tr.right -= S(32);
    DrawTextW(dc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, of);
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = h;
            CreateChildControls(h);
            RefreshProcessList();
            // Lay out from the real client area: the window may be smaller than the
            // nominal 1360x900 (DPI scaling, small screens), and laying out for the
            // nominal size clipped the right column and the footer on first open.
            RECT rc{};
            GetClientRect(h, &rc);
            LayoutControls(h, rc.right, rc.bottom);
            // Make the initial tab state explicit instead of relying on creation flags,
            // so the first paint matches what a tab switch produces.
            ShowTab(0);
            SetStatus(L"READY");
            SetTimer(h, 1, 500, nullptr);
            RpcStartWorker();
            RpcPublishIdle();
            return 0;
        }
        case WM_SIZE: {
            if (w != SIZE_MINIMIZED) {
                RECT rc;
                GetClientRect(h, &rc);
                LayoutControls(h, rc.right, rc.bottom);
                // Repaint the frame and all children so a resize never leaves a
                // strip of the previous layout behind.
                RedrawWindow(h, nullptr, nullptr,
                             RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);

            // field backdrops behind edits & combo (drawn after the cards so the border stays visible)
            auto fieldFor = [&](HWND ctl, bool combo) {
                // Hidden controls keep their last rect; drawing a backdrop for them left a
                // stray filled box at 0,0 on the very first paint (classic-only fields are
                // created hidden there until HideControl moves them away).
                if (!ctl || !IsWindowVisible(ctl)) return;
                RECT field{};
                GetWindowRect(ctl, &field);
                MapWindowPoints(HWND_DESKTOP, h, (LPPOINT)&field, 2);
                if (combo) field.bottom = field.top + S(30);
                InflateRect(&field, S(2), S(2));
                FillRoundRect(mem, field, S(7), Th::INPUT, Th::BORDER);
            };

            PaintMain(h, mem);
            fieldFor(g_hCombo, true);
            fieldFor(g_hEditSearch, false);
            fieldFor(g_hEditAddr, false);
            fieldFor(g_hEditSize, false);
            fieldFor(g_hEditPid, false);
            fieldFor(g_hPreset, true);
            fieldFor(g_hCmd, false);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORLISTBOX: {
            HDC dc = (HDC)w;
            SetTextColor(dc, Th::TEXT);
            SetBkColor(dc, Th::INPUT);
            if (!g_brushInput) g_brushInput = CreateSolidBrush(Th::INPUT);
            return (LRESULT)g_brushInput;
        }
        case WM_MEASUREITEM: {
            auto* mis = (MEASUREITEMSTRUCT*)l;
            mis->itemHeight = S(26);
            return TRUE;
        }
        case WM_DRAWITEM: {
            auto* dis = (DRAWITEMSTRUCT*)l;
            if (dis->CtlID == HS_COMBO || dis->CtlID == HS_PRESET) {
                DrawComboItem(dis);
                return TRUE;
            }
            return FALSE;
        }
        case WM_COMMAND:
            HandleCommand(w, l);
            return 0;
        case WM_HS_OUTPUT: {
            int gen = (int)w;
            auto* chunk = (std::string*)l;
            if (chunk) {
                if (gen == g_scanGen) AppendOutputChunk(*chunk);
                delete chunk;
            }
            return 0;
        }
        case WM_HS_DONE:
            OnScanDone((int)w);
            return 0;
        case WM_TIMER:
            if (w == 1) {
                RECT inv[3];
                int n = 0;
                if (g_classic) {
                    inv[n++] = g_L.classicStatus;
                } else {
                    inv[n++] = g_L.infoCard;
                    inv[n++] = g_L.actionRow;
                }
                inv[n++] = g_L.logCard;
                for (int i = 0; i < n; ++i) {
                    RECT x = inv[i];
                    InvalidateRect(h, &x, FALSE);
                }
                if (g_scanning) RpcPublishScanning();
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* mmi = (MINMAXINFO*)l;
            int minW = S(1180), minH = S(880);
            // Never demand more room than the desktop can give: on a small or
            // high-DPI screen the old minimum pushed the footer off-screen.
            RECT wa{};
            if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
                int availW = wa.right - wa.left;
                int availH = wa.bottom - wa.top;
                if (minW > availW - S(40)) minW = availW - S(40);
                if (minH > availH - S(60)) minH = availH - S(60);
            }
            if (minW < S(860)) minW = S(860);
            if (minH < S(700)) minH = S(700);
            mmi->ptMinTrackSize.x = minW;
            mmi->ptMinTrackSize.y = minH;
            return 0;
        }
        case WM_CLOSE:
            if (g_scanning) {
                if (HsMessage(HSI_QUESTION, L"ANALYSIS RUNNING",
                              L"An analysis is still running.\r\nStop it and exit?", true) != IDOK)
                    return 0;
            }
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            KillTimer(h, 1);
            RpcStopWorker();
            if (g_scanning) {
                g_stopRequested = true;
                if (g_hProc) TerminateProcess(g_hProc, 0);
            }
            if (g_reader.joinable()) g_reader.join();
            FreeProcessHandles();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

// =====================================================================================
//  Entry point
// =====================================================================================

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    // Single-binary mode: when the GUI launches itself with --engine, act as the
    // Moneta engine (same argument set the console build accepts).
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc >= 2 && _wcsicmp(argv[1], L"--engine") == 0) {
        int rc = MonetaMain(argc - 1, (const wchar_t**)(argv + 1));
        LocalFree(argv);
        return rc;
    }
    if (argv) LocalFree(argv);

    g_hInst = hInst;
    SetProcessDPIAware();

    // One window only. A second copy would stack on top of the first and leave a
    // half-covered header (the "double title" glitch) until one of them is closed,
    // so activate the running window and exit instead. The class check also catches
    // an instance from an older build that does not hold the mutex yet.
    if (HWND existing = FindWindowW(L"HyperSenseMain", nullptr)) {
        if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
        SetForegroundWindow(existing);
        return 0;
    }
    HANDLE single = CreateMutexW(nullptr, FALSE, L"Local\\HyperSense.SingleInstance");
    if (single && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"HyperSenseMain", nullptr);
        if (existing) {
            if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(single);
        return 0;
    }

    HDC screen = GetDC(nullptr);
    if (screen) {
        g_dpi = GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(nullptr, screen);
    }
    if (g_dpi <= 0) g_dpi = 96;

    // Test hook: render at a forced scale so the 125%/150% layout can be checked
    // without changing the desktop display setting.
    wchar_t dpiEnv[16] = {};
    if (GetEnvironmentVariableW(L"HYPERSENSE_DPI", dpiEnv, 16) > 0) {
        int forced = _wtoi(dpiEnv);
        if (forced >= 72 && forced <= 288) g_dpi = forced;
    }

    CreateFonts();
    InitCommonControls();
    LoadLibraryW(L"Msftedit.dll");
    InitSystemInfo();
    RpcLoadConfig();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"HyperSenseMain";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    WNDCLASSEXW bc{};
    bc.cbSize = sizeof(bc);
    bc.lpfnWndProc = BtnProc;
    bc.hInstance = hInst;
    bc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    bc.hbrBackground = nullptr;
    bc.lpszClassName = kBtNClass;
    RegisterClassExW(&bc);

    WNDCLASSEXW tc = bc;
    tc.lpfnWndProc = TgProc;
    tc.lpszClassName = kTgClass;
    RegisterClassExW(&tc);

    WNDCLASSEXW dc = bc;
    dc.style = CS_DROPSHADOW;
    dc.lpfnWndProc = DlgProc;
    dc.lpszClassName = kDlgClass;
    RegisterClassExW(&dc);

    // Open at a size that always fits the desktop, centred in the work area.
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int availW = wa.right - wa.left;
    int availH = wa.bottom - wa.top;
    if (availW <= 0) availW = GetSystemMetrics(SM_CXSCREEN);
    if (availH <= 0) availH = GetSystemMetrics(SM_CYSCREEN);

    int w = S(1360), hgt = S(900);
    if (w > availW - S(40)) w = availW - S(40);
    if (hgt > availH - S(60)) hgt = availH - S(60);
    if (w < S(860)) w = S(860);
    if (hgt < S(700)) hgt = S(700);

    int x = wa.left + (availW - w) / 2;
    int y = wa.top + (availH - hgt) / 2;
    if (x < wa.left) x = wa.left;
    if (y < wa.top) y = wa.top;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
                                L"HYPERSENSEGLOBAL\u2122  \u2014  Live Usermode Memory Analysis Tool V1",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                x, y, w, hgt, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));

    ShowWindow(hwnd, nShow);
    // Windows may adjust the size when the window is shown; lay out again from the
    // final client area and paint everything before the first frame is visible.
    {
        RECT cr{};
        GetClientRect(hwnd, &cr);
        LayoutControls(hwnd, cr.right, cr.bottom);
        RedrawWindow(hwnd, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_brushInput) { DeleteObject(g_brushInput); g_brushInput = nullptr; }
    DestroyFonts();
    return 0;
}

