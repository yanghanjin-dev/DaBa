/*
 * DaBa - 가볍고 빠른 동영상 플레이어
 *
 * 순수 C (C99) + Win32 API + libmpv(동적 로드)
 *  - 커스텀 다크 타이틀바 / 컨트롤바 (GDI + GDI+ 안티앨리어싱)
 *  - 오너드로우 다크 메뉴 (열기/재생/자막/영상/소리/화면 비율/화면 크기 ...)
 *  - 팟플레이어 스타일 단축키, 재생목록, 폴더 자동 추가, 최근 파일, 재생 위치 기억
 *
 * 빌드: build.bat 참고 (MSVC 또는 MinGW-w64)
 */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define COBJMACROS
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <wchar.h>
#include <math.h>
#include <ctype.h>
#include <wctype.h>
#include "mpv/client.h"

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#define APP_NAME L"DaBa"
#define APP_VER  L"0.0.1"
#define WM_MPV_WAKEUP (WM_APP + 1)
#define TIMER_UI 1

/* ---------- 크기 (96 DPI 기준) ---------- */
#define TITLE_H 34
#define SEEK_H  24
#define BTN_H   42
#define CTRL_H  (SEEK_H + BTN_H)

/* ---------- 색상 ---------- */
#define C_TITLE     RGB(31, 31, 31)
#define C_TITLE_SEP RGB(55, 55, 55)
#define C_SEEKBG    RGB(24, 24, 24)
#define C_BTNBG     RGB(33, 33, 33)
#define C_SEP       RGB(19, 19, 19)
#define C_HOVER     RGB(54, 54, 54)
#define C_PRESS     RGB(66, 66, 66)
#define C_TEXT      RGB(236, 236, 236)
#define C_TEXT2     RGB(150, 150, 150)
#define C_ICON      RGB(214, 214, 214)
#define C_ICON_HOT  RGB(255, 255, 255)
#define C_TRACK     RGB(72, 72, 72)
#define C_PLAYED    RGB(205, 205, 205)
#define C_ACCENT    RGB(255, 214, 0)
#define C_ABLOOP    RGB(150, 125, 0)
#define C_BADGE     RGB(58, 58, 58)
#define C_BADGE_HOT RGB(80, 80, 80)
#define C_BADGE_T   RGB(188, 188, 188)
#define C_CLOSE     RGB(232, 17, 35)
#define C_MBG       RGB(64, 64, 64)
#define C_MSEL      RGB(22, 22, 22)
#define C_MTEXT     RGB(232, 232, 232)
#define C_MACC      RGB(205, 205, 205)
#define C_MDIS      RGB(128, 128, 128)
#define C_MSEP      RGB(44, 44, 44)
#define C_RADIO     RGB(40, 125, 235)
#define C_CHECKBG   RGB(44, 44, 52)
#define C_LISTBG    RGB(28, 28, 28)
#define C_LISTSEL   RGB(58, 58, 58)

/* =====================================================================
 * GDI+ flat API (C 에서 직접 선언)
 * ===================================================================== */
typedef struct { UINT32 GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } GpStartupInput;
typedef struct { float X, Y; } GpPointF;
typedef void GpGraphics;
typedef void GpBrush;
typedef void GpPath;
int  WINAPI GdiplusStartup(ULONG_PTR *, const GpStartupInput *, void *);
void WINAPI GdiplusShutdown(ULONG_PTR);
int  WINAPI GdipCreateFromHDC(HDC, GpGraphics **);
int  WINAPI GdipDeleteGraphics(GpGraphics *);
int  WINAPI GdipSetSmoothingMode(GpGraphics *, int);
int  WINAPI GdipSetPixelOffsetMode(GpGraphics *, int);
int  WINAPI GdipCreateSolidFill(DWORD, GpBrush **);
int  WINAPI GdipSetSolidFillColor(GpBrush *, DWORD);
int  WINAPI GdipDeleteBrush(GpBrush *);
int  WINAPI GdipFillEllipse(GpGraphics *, GpBrush *, float, float, float, float);
int  WINAPI GdipFillRectangle(GpGraphics *, GpBrush *, float, float, float, float);
int  WINAPI GdipFillPolygon(GpGraphics *, GpBrush *, const GpPointF *, int, int);
int  WINAPI GdipCreatePath(int, GpPath **);
int  WINAPI GdipDeletePath(GpPath *);
int  WINAPI GdipAddPathArc(GpPath *, float, float, float, float, float, float);
int  WINAPI GdipClosePathFigure(GpPath *);
int  WINAPI GdipFillPath(GpGraphics *, GpBrush *, GpPath *);

/* =====================================================================
 * libmpv 동적 로드
 * ===================================================================== */
#define MPV_FN(X) \
    X(mpv_handle *, mpv_create, (void)) \
    X(int, mpv_initialize, (mpv_handle *)) \
    X(void, mpv_terminate_destroy, (mpv_handle *)) \
    X(int, mpv_set_option_string, (mpv_handle *, const char *, const char *)) \
    X(int, mpv_command, (mpv_handle *, const char **)) \
    X(int, mpv_command_string, (mpv_handle *, const char *)) \
    X(int, mpv_set_property, (mpv_handle *, const char *, mpv_format, void *)) \
    X(int, mpv_set_property_string, (mpv_handle *, const char *, const char *)) \
    X(int, mpv_get_property, (mpv_handle *, const char *, mpv_format, void *)) \
    X(char *, mpv_get_property_string, (mpv_handle *, const char *)) \
    X(void, mpv_free, (void *)) \
    X(void, mpv_free_node_contents, (mpv_node *)) \
    X(int, mpv_observe_property, (mpv_handle *, uint64_t, const char *, mpv_format)) \
    X(mpv_event *, mpv_wait_event, (mpv_handle *, double)) \
    X(void, mpv_set_wakeup_callback, (mpv_handle *, void (*)(void *), void *))

#define X(r, n, a) static r (*f_##n) a;
MPV_FN(X)
#undef X

/* =====================================================================
 * 상태
 * ===================================================================== */
enum {
    E_NONE, E_LOGO, E_PIN, E_MIN, E_MAX, E_FS, E_CLOSE,
    E_SEEK, E_VOLICON, E_VOL,
    E_PLAY, E_STOP, E_PREV, E_NEXT, E_OPEN,
    E_TIME, E_B_HW, E_B_V, E_B_A, E_B_CH, E_B_SPEED,
    E_LIST, E_SET, E_MENU,
    E_COUNT
};

enum {
    P_TIME = 1, P_DUR, P_PAUSE, P_VOL, P_MUTE, P_SPEED, P_HWDEC, P_VFMT, P_ACODEC, P_ACH,
    P_IDLE, P_EOF, P_CHAPTERS, P_ABA, P_ABB, P_TITLE
};

typedef struct { double t; wchar_t title[80]; } Chapter;

static struct {
    HINSTANCE inst;
    HWND hwnd, video, fsbar, tip, plwnd, pllist;
    mpv_handle *mpv;
    ULONG_PTR gdipToken;
    int dpi;
    HFONT fUI, fTitle, fLogo, fTime, fBadge, fIcon, fIconBig, fIconXs, fMenu, fBig, fHint, fHint2, fCheck;
    HBRUSH brMenu, brList;
    HDC mdc;                     /* 텍스트 측정용 */
    HDC bbdc; HBITMAP bbbmp; int bbw, bbh;  /* 백버퍼 */
    wchar_t exeDir[MAX_PATH], baseDir[MAX_PATH], ini[MAX_PATH];

    /* 재생 상태 */
    double pos, dur, volume, speed;
    int paused, muted, idle, eof, hw, err;
    int hasVideo, vw, vh;
    char vcodec[48], acodec[48];
    int achannels;
    double abA, abB;
    Chapter ch[256]; int nch;
    wchar_t name[MAX_PATH], fmt[16];

    /* 재생목록 */
    wchar_t **pl; int plCount, plCap, plCur, plHint;

    /* 옵션 */
    int optTopmost, optResume, optHideCursor, optNoSaver, optCloseEnd, optSeekTime, optSeekCh,
        optAutoFolder, optAutoFit, optHwdec, repeat, shuffle, subBox, subVis, normalize, pitch,
        deint, rotate, hflip, vflip, aspect, fill;

    /* UI 상태 */
    int fs, hot, pressed, drag, vdrag, showRemain, cursorHidden, fitPending;
    POINT vdragPt, lastMouse;
    double dragPos; DWORD lastSeekTick, lastMove;
    int lastSec, lastPx;
    WINDOWPLACEMENT fsPrev;
    wchar_t tipText[64];
    wchar_t recent[10][MAX_PATH]; int nRecent;
} g;

static int S(int v) { return MulDiv(v, g.dpi ? g.dpi : 96, 96); }
static float SF(float v) { return v * (g.dpi ? g.dpi : 96) / 96.0f; }

/* =====================================================================
 * 유틸
 * ===================================================================== */
static void wsf(wchar_t *buf, size_t n, const wchar_t *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    _vsnwprintf(buf, n - 1, fmt, ap);
    va_end(ap);
    buf[n - 1] = 0;
}

static char *u8(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = (char *)malloc(n > 0 ? n : 1);
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL); else s[0] = 0;
    return s;
}

static void w16(const char *s, wchar_t *out, int cap)
{
    if (!s) { out[0] = 0; return; }
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, out, cap)) out[0] = 0;
    out[cap - 1] = 0;
}

static wchar_t *wdup(const wchar_t *s)
{
    size_t n = wcslen(s) + 1;
    wchar_t *d = (wchar_t *)malloc(n * sizeof(wchar_t));
    memcpy(d, s, n * sizeof(wchar_t));
    return d;
}

static const wchar_t *base_name(const wchar_t *p)
{
    const wchar_t *b = p;
    if (wcsstr(p, L"://")) return p;
    for (const wchar_t *c = p; *c; c++) if (*c == L'\\' || *c == L'/') b = c + 1;
    return b;
}

static const wchar_t *ext_of(const wchar_t *p)
{
    const wchar_t *b = base_name(p), *d = wcsrchr(b, L'.');
    return d ? d + 1 : L"";
}

static int is_url(const wchar_t *p) { return wcsstr(p, L"://") != NULL; }

static const wchar_t *VEXT[] = { L"mp4", L"mkv", L"avi", L"mov", L"wmv", L"flv", L"webm", L"m4v", L"ts", L"m2ts",
    L"mts", L"mpg", L"mpeg", L"vob", L"3gp", L"3g2", L"ogv", L"rm", L"rmvb", L"asf", L"divx", L"f4v", L"tp", L"trp",
    L"m2t", L"hevc", L"h264", L"h265", L"y4m", L"mxf", L"dav", NULL };
static const wchar_t *AEXT[] = { L"mp3", L"flac", L"wav", L"m4a", L"aac", L"ogg", L"oga", L"opus", L"wma", L"ape",
    L"alac", L"aiff", L"aif", L"wv", L"dsf", L"dff", L"mka", L"ac3", L"dts", L"tta", L"mpc", NULL };
static const wchar_t *SEXT[] = { L"srt", L"ass", L"ssa", L"smi", L"sami", L"sub", L"vtt", L"sup", L"idx", L"lrc", NULL };

static int ext_in(const wchar_t *p, const wchar_t **list)
{
    const wchar_t *e = ext_of(p);
    for (int i = 0; list[i]; i++) if (!_wcsicmp(e, list[i])) return 1;
    return 0;
}
static int is_media(const wchar_t *p) { return ext_in(p, VEXT) || ext_in(p, AEXT); }
static int is_sub(const wchar_t *p) { return ext_in(p, SEXT); }

static void fmt_time(double t, wchar_t *b, size_t n)
{
    if (t < 0 || t != t) t = 0;
    int s = (int)t;
    wsf(b, n, L"%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
}

static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }

/* =====================================================================
 * mpv 헬퍼
 * ===================================================================== */
static void mcmd(const char *a, const char *b, const char *c, const char *d)
{
    if (!g.mpv) return;
    const char *v[5] = { a, b, c, d, NULL };
    f_mpv_command(g.mpv, v);
}

static void mcmdf(const char *fmt, ...)
{
    if (!g.mpv) return;
    char buf[1024]; va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    f_mpv_command_string(g.mpv, buf);
}

static void setp_s(const char *n, const char *v) { if (g.mpv) f_mpv_set_property_string(g.mpv, n, v); }
static void setp_d(const char *n, double v) { if (g.mpv) f_mpv_set_property(g.mpv, n, MPV_FORMAT_DOUBLE, &v); }
static void setp_i(const char *n, int64_t v) { if (g.mpv) f_mpv_set_property(g.mpv, n, MPV_FORMAT_INT64, &v); }
static void setp_f(const char *n, int v) { if (g.mpv) f_mpv_set_property(g.mpv, n, MPV_FORMAT_FLAG, &v); }
static double getp_d(const char *n, double def) { double v = def; if (g.mpv && f_mpv_get_property(g.mpv, n, MPV_FORMAT_DOUBLE, &v) < 0) v = def; return v; }
static int64_t getp_i(const char *n, int64_t def) { int64_t v = def; if (g.mpv && f_mpv_get_property(g.mpv, n, MPV_FORMAT_INT64, &v) < 0) v = def; return v; }

static void osd(const wchar_t *fmt, ...)
{
    if (!g.mpv) return;
    wchar_t w[512]; va_list ap; va_start(ap, fmt);
    _vsnwprintf(w, 511, fmt, ap); va_end(ap); w[511] = 0;
    char *s = u8(w);
    mcmd("show-text", s, "1500", NULL);
    free(s);
}

static void on_mpv_wakeup(void *ctx) { (void)ctx; PostMessageW(g.hwnd, WM_MPV_WAKEUP, 0, 0); }

/* =====================================================================
 * 설정 파일 (UTF-16 INI)
 * ===================================================================== */
static int ini_int(const wchar_t *k, int def) { return (int)GetPrivateProfileIntW(L"DaBa", k, def, g.ini); }
static void ini_set_int(const wchar_t *k, int v) { wchar_t b[32]; wsf(b, 32, L"%d", v); WritePrivateProfileStringW(L"DaBa", k, b, g.ini); }

static void ini_init_paths(void)
{
    GetModuleFileNameW(NULL, g.exeDir, MAX_PATH);
    wchar_t *s = wcsrchr(g.exeDir, L'\\'); if (s) *s = 0;
    wcscpy(g.baseDir, g.exeDir);
    wchar_t p[MAX_PATH]; wsf(p, MAX_PATH, L"%ls\\DaBa.ini", g.baseDir);
    HANDLE h = CreateFileW(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {   /* 설치 폴더가 쓰기 불가 → %APPDATA%\DaBa */
        wchar_t ad[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, ad))) {
            wsf(g.baseDir, MAX_PATH, L"%ls\\DaBa", ad);
            SHCreateDirectoryExW(NULL, g.baseDir, NULL);
            wsf(p, MAX_PATH, L"%ls\\DaBa.ini", g.baseDir);
            h = CreateFileW(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
        }
    }
    if (h != INVALID_HANDLE_VALUE) {
        if (GetFileSize(h, NULL) == 0) {  /* 유니코드 INI 가 되도록 BOM 기록 */
            DWORD wr; unsigned char bom[2] = { 0xFF, 0xFE };
            WriteFile(h, bom, 2, &wr, NULL);
        }
        CloseHandle(h);
    }
    wcscpy(g.ini, p);
}

static void load_settings(void)
{
    g.optTopmost    = ini_int(L"Topmost", 0);
    g.optResume     = ini_int(L"Resume", 1);
    g.optHideCursor = ini_int(L"HideCursor", 1);
    g.optNoSaver    = ini_int(L"NoScreenSaver", 1);
    g.optCloseEnd   = ini_int(L"CloseOnEnd", 0);
    g.optSeekTime   = ini_int(L"SeekTime", 1);
    g.optSeekCh     = ini_int(L"SeekChapters", 1);
    g.optAutoFolder = ini_int(L"AutoFolder", 1);
    g.optAutoFit    = ini_int(L"AutoFit", 1);
    g.optHwdec      = ini_int(L"Hwdec", 1);
    g.repeat        = ini_int(L"Repeat", 0);
    g.shuffle       = ini_int(L"Shuffle", 0);
    g.subBox        = ini_int(L"SubBox", 0);
    g.volume        = ini_int(L"Volume", 70);
    g.pitch         = 1;
    g.subVis        = 1;
    g.speed         = 1.0;
    g.nRecent = 0;
    for (int i = 0; i < 10; i++) {
        wchar_t k[8]; wsf(k, 8, L"%d", i);
        GetPrivateProfileStringW(L"Recent", k, L"", g.recent[g.nRecent], MAX_PATH, g.ini);
        if (g.recent[g.nRecent][0]) g.nRecent++;
    }
}

static void save_recent(void)
{
    WritePrivateProfileStringW(L"Recent", NULL, NULL, g.ini);
    for (int i = 0; i < g.nRecent; i++) {
        wchar_t k[8]; wsf(k, 8, L"%d", i);
        WritePrivateProfileStringW(L"Recent", k, g.recent[i], g.ini);
    }
}

static void save_settings(void)
{
    ini_set_int(L"Topmost", g.optTopmost);     ini_set_int(L"Resume", g.optResume);
    ini_set_int(L"HideCursor", g.optHideCursor); ini_set_int(L"NoScreenSaver", g.optNoSaver);
    ini_set_int(L"CloseOnEnd", g.optCloseEnd); ini_set_int(L"SeekTime", g.optSeekTime);
    ini_set_int(L"SeekChapters", g.optSeekCh); ini_set_int(L"AutoFolder", g.optAutoFolder);
    ini_set_int(L"AutoFit", g.optAutoFit);     ini_set_int(L"Hwdec", g.optHwdec);
    ini_set_int(L"Repeat", g.repeat);          ini_set_int(L"Shuffle", g.shuffle);
    ini_set_int(L"SubBox", g.subBox);          ini_set_int(L"Volume", (int)(g.volume + 0.5));
}

static void recent_add(const wchar_t *p)
{
    int i, j;
    for (i = 0; i < g.nRecent; i++) if (!_wcsicmp(g.recent[i], p)) break;
    if (i == g.nRecent) { if (g.nRecent < 10) g.nRecent++; i = g.nRecent - 1; }
    for (j = i; j > 0; j--) wcscpy(g.recent[j], g.recent[j - 1]);
    wcsncpy(g.recent[0], p, MAX_PATH - 1); g.recent[0][MAX_PATH - 1] = 0;
    save_recent();
}

/* =====================================================================
 * 그리기 헬퍼
 * ===================================================================== */
static void fillr(HDC dc, const RECT *r, COLORREF c) { SetDCBrushColor(dc, c); FillRect(dc, r, (HBRUSH)GetStockObject(DC_BRUSH)); }
static void vline(HDC dc, int x, int y1, int y2, COLORREF c) { RECT r = { x, y1, x + 1, y2 }; fillr(dc, &r, c); }
static void hline(HDC dc, int x1, int x2, int y, COLORREF c) { RECT r = { x1, y, x2, y + 1 }; fillr(dc, &r, c); }

static void dtext(HDC dc, HFONT f, const wchar_t *s, RECT *r, COLORREF c, UINT fmt)
{
    SelectObject(dc, f); SetTextColor(dc, c); SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s, -1, r, fmt | DT_NOPREFIX);
}

static int textw(HFONT f, const wchar_t *s)
{
    SIZE z = { 0, 0 };
    SelectObject(g.mdc, f);
    GetTextExtentPoint32W(g.mdc, s, (int)wcslen(s), &z);
    return z.cx;
}

static void glyph(HDC dc, HFONT f, wchar_t ch, const RECT *r, COLORREF c)
{
    wchar_t s[2] = { ch, 0 }; RECT rr = *r;
    dtext(dc, f, s, &rr, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static DWORD argb(COLORREF c) { return 0xFF000000u | ((DWORD)GetRValue(c) << 16) | ((DWORD)GetGValue(c) << 8) | GetBValue(c); }

static GpGraphics *gp_begin(HDC dc)
{
    GpGraphics *gr = NULL;
    GdipCreateFromHDC(dc, &gr);
    if (gr) { GdipSetSmoothingMode(gr, 4); GdipSetPixelOffsetMode(gr, 4); }
    return gr;
}
static GpBrush *gp_brush(COLORREF c) { GpBrush *b = NULL; GdipCreateSolidFill(argb(c), &b); return b; }

static void gp_rect(GpGraphics *gr, float x, float y, float w, float h, COLORREF c)
{
    if (w <= 0 || h <= 0) return;
    GpBrush *b = gp_brush(c); GdipFillRectangle(gr, b, x, y, w, h); GdipDeleteBrush(b);
}
static void gp_circle(GpGraphics *gr, float cx, float cy, float r, COLORREF c)
{
    GpBrush *b = gp_brush(c); GdipFillEllipse(gr, b, cx - r, cy - r, r * 2, r * 2); GdipDeleteBrush(b);
}
static void gp_rrect(GpGraphics *gr, float x, float y, float w, float h, float r, COLORREF c)
{
    GpPath *p = NULL; GdipCreatePath(0, &p); if (!p) return;
    float d = r * 2;
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
    GpBrush *b = gp_brush(c); GdipFillPath(gr, b, p); GdipDeleteBrush(b); GdipDeletePath(p);
}
static void gp_poly(GpGraphics *gr, const GpPointF *pts, int n, COLORREF c)
{
    GpBrush *b = gp_brush(c); GdipFillPolygon(gr, b, pts, n, 0); GdipDeleteBrush(b);
}

static HDC get_bb(int w, int h)
{
    if (!g.bbdc) g.bbdc = CreateCompatibleDC(NULL);
    if (w > g.bbw || h > g.bbh) {
        HDC sdc = GetDC(NULL);
        if (g.bbbmp) DeleteObject(g.bbbmp);
        g.bbw = w > g.bbw ? w : g.bbw; g.bbh = h > g.bbh ? h : g.bbh;
        g.bbbmp = CreateCompatibleBitmap(sdc, g.bbw, g.bbh);
        ReleaseDC(NULL, sdc);
        SelectObject(g.bbdc, g.bbbmp);
    }
    return g.bbdc;
}

static HFONT mkfont(const wchar_t *face, int px, int weight)
{
    return CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

static void make_fonts(void)
{
    HFONT *all[] = { &g.fUI, &g.fTitle, &g.fLogo, &g.fTime, &g.fBadge, &g.fIcon, &g.fIconBig, &g.fIconXs,
                     &g.fMenu, &g.fBig, &g.fHint, &g.fHint2, &g.fCheck };
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) if (*all[i]) { DeleteObject(*all[i]); *all[i] = NULL; }
    const wchar_t *ui = L"Malgun Gothic", *ic = L"Segoe MDL2 Assets";
    g.fUI     = mkfont(ui, S(13), FW_NORMAL);
    g.fTitle  = mkfont(ui, S(14), FW_NORMAL);
    g.fLogo   = mkfont(ui, S(15), FW_BOLD);
    g.fTime   = mkfont(ui, S(15), FW_NORMAL);
    g.fBadge  = mkfont(L"Segoe UI", S(12), FW_SEMIBOLD);
    g.fIcon   = mkfont(ic, S(12), FW_NORMAL);
    g.fIconBig= mkfont(ic, S(16), FW_NORMAL);
    g.fIconXs = mkfont(ic, S(9), FW_NORMAL);
    g.fMenu   = mkfont(ui, S(14), FW_NORMAL);
    g.fBig    = mkfont(L"Segoe UI", S(46), FW_BOLD);
    g.fHint   = mkfont(ui, S(14), FW_NORMAL);
    g.fHint2  = mkfont(ui, S(12), FW_NORMAL);
    g.fCheck  = mkfont(ic, S(11), FW_NORMAL);
}

/* =====================================================================
 * 레이아웃
 * ===================================================================== */
static void get_layout(RECT *t, RECT *v, RECT *c)
{
    RECT cr; GetClientRect(g.hwnd, &cr);
    if (g.fs) { SetRectEmpty(t); SetRectEmpty(c); *v = cr; return; }
    int th = S(TITLE_H), chh = S(CTRL_H);
    SetRect(t, cr.left, cr.top, cr.right, cr.top + th);
    SetRect(c, cr.left, cr.bottom - chh, cr.right, cr.bottom);
    SetRect(v, cr.left, t->bottom, cr.right, c->top);
    if (v->bottom < v->top) v->bottom = v->top;
}

static void layout_title(const RECT *b, RECT *er)
{
    int bw = S(44), x = b->right;
    SetRect(&er[E_LOGO], b->left, b->top, b->left + S(84), b->bottom);
    int ids[] = { E_CLOSE, E_FS, E_MAX, E_MIN, E_PIN };
    for (int i = 0; i < 5; i++) { SetRect(&er[ids[i]], x - bw, b->top, x, b->bottom); x -= bw; }
}

static void badge_text(int e, wchar_t *b, size_t n)
{
    b[0] = 0;
    if (g.idle) return;
    switch (e) {
    case E_B_HW: wsf(b, n, L"%ls", g.hw ? L"H/W" : L"S/W"); break;
    case E_B_V: if (g.hasVideo && g.vcodec[0]) w16(g.vcodec, b, (int)n); break;
    case E_B_A: if (g.acodec[0]) w16(g.acodec, b, (int)n); break;
    case E_B_CH:
        if (g.achannels > 0) {
            switch (g.achannels) {
            case 1: wsf(b, n, L"1.0"); break; case 2: wsf(b, n, L"2.0"); break; case 3: wsf(b, n, L"2.1"); break;
            case 6: wsf(b, n, L"5.1"); break; case 8: wsf(b, n, L"7.1"); break;
            default: wsf(b, n, L"%dch", g.achannels);
            }
        }
        break;
    case E_B_SPEED: if (fabs(g.speed - 1.0) > 0.001) wsf(b, n, L"%.2gx", g.speed); break;
    }
}

static void time_text(wchar_t *cur, wchar_t *tot)
{
    double p = g.drag == E_SEEK ? g.dragPos : g.pos;
    if (g.showRemain && g.dur > 0) { wchar_t t[16]; fmt_time(g.dur - p, t, 16); wsf(cur, 20, L"-%ls", t); }
    else fmt_time(p, cur, 20);
    fmt_time(g.dur, tot, 20);
}

static void layout_ctrl(const RECT *b, RECT *er)
{
    int top = b->top, sr = S(SEEK_H);
    int volw = S(118);
    SetRect(&er[E_VOL], b->right - S(14) - volw, top, b->right - S(14), top + sr);
    SetRect(&er[E_VOLICON], er[E_VOL].left - S(30), top, er[E_VOL].left - S(2), top + sr);
    SetRect(&er[E_SEEK], b->left + S(8), top, er[E_VOLICON].left - S(12), top + sr);

    int y = top + sr, y2 = b->bottom, bw = S(46), x = b->left;
    int lb[] = { E_PLAY, E_STOP, E_PREV, E_NEXT, E_OPEN };
    for (int i = 0; i < 5; i++) { SetRect(&er[lb[i]], x, y, x + bw, y2); x += bw; }
    int rx = b->right;
    int rb[] = { E_MENU, E_SET, E_LIST };
    for (int i = 0; i < 3; i++) { SetRect(&er[rb[i]], rx - bw, y, rx, y2); rx -= bw; }

    x = er[E_OPEN].right + S(16);
    wchar_t cur[20], tot[20], all[48];
    time_text(cur, tot); wsf(all, 48, L"%ls / %ls", cur, tot);
    int tw = textw(g.fTime, all) + S(4);
    if (x + tw < rx - S(8)) { SetRect(&er[E_TIME], x, y, x + tw, y2); x += tw + S(10); }
    int bh = S(20), by = y + (y2 - y - bh) / 2;
    int bd[] = { E_B_HW, E_B_V, E_B_A, E_B_CH, E_B_SPEED };
    for (int i = 0; i < 5; i++) {
        wchar_t t[24]; badge_text(bd[i], t, 24);
        if (!t[0]) continue;
        int w = textw(g.fBadge, t) + S(12);
        if (x + w > rx - S(8)) break;
        SetRect(&er[bd[i]], x, by, x + w, by + bh);
        x += w + S(5);
    }
}

/* 창(메인 또는 전체화면 컨트롤바)의 요소 영역 */
static void get_elems(HWND w, RECT *er)
{
    memset(er, 0, sizeof(RECT) * E_COUNT);
    if (w == g.hwnd) {
        if (g.fs) return;
        RECT t, v, c; get_layout(&t, &v, &c);
        layout_title(&t, er); layout_ctrl(&c, er);
    } else if (w == g.fsbar) {
        RECT c; GetClientRect(w, &c); layout_ctrl(&c, er);
    }
}

static int hit_er(const RECT *er, int x, int y)
{
    POINT p = { x, y };
    for (int e = 1; e < E_COUNT; e++) if (er[e].right > er[e].left && PtInRect(&er[e], p)) return e;
    return 0;
}

static void inval_ctrl(void)
{
    if (g.fs) { if (g.fsbar && IsWindowVisible(g.fsbar)) InvalidateRect(g.fsbar, NULL, FALSE); return; }
    RECT t, v, c; get_layout(&t, &v, &c); InvalidateRect(g.hwnd, &c, FALSE);
}
static void inval_title(void) { if (g.fs) return; RECT t, v, c; get_layout(&t, &v, &c); InvalidateRect(g.hwnd, &t, FALSE); }
static void inval_all(void) { InvalidateRect(g.hwnd, NULL, FALSE); if (g.fsbar) InvalidateRect(g.fsbar, NULL, FALSE); }

/* =====================================================================
 * 페인트: 타이틀바
 * ===================================================================== */
static void btn_bg(HDC dc, int e, const RECT *r)
{
    if (g.pressed == e) fillr(dc, r, C_PRESS);
    else if (g.hot == e) fillr(dc, r, e == E_CLOSE ? C_CLOSE : C_HOVER);
}

static void paint_title(HDC dc, const RECT *b, RECT *er)
{
    fillr(dc, b, C_TITLE);
    /* 로고 + 메뉴 화살표 */
    RECT lr = er[E_LOGO];
    btn_bg(dc, E_LOGO, &lr);
    RECT a = lr; a.left += S(13);
    dtext(dc, g.fLogo, APP_NAME, &a, g.hot == E_LOGO ? C_ICON_HOT : C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    int lw = textw(g.fLogo, APP_NAME);
    RECT cv = { a.left + lw + S(4), lr.top + S(2), a.left + lw + S(20), lr.bottom };
    glyph(dc, g.fIconXs, 0xE70D, &cv, C_TEXT2);
    vline(dc, lr.right, b->top + S(8), b->bottom - S(8), C_TITLE_SEP);

    int x = lr.right + S(12);
    if (g.fmt[0]) {
        RECT f = { x, b->top, x + S(80), b->bottom };
        dtext(dc, g.fTitle, g.fmt, &f, C_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x += textw(g.fTitle, g.fmt) + S(10);
        vline(dc, x, b->top + S(11), b->bottom - S(11), C_TITLE_SEP);
        x += S(11);
    }
    RECT nr = { x, b->top, er[E_PIN].left - S(10), b->bottom };
    if (g.name[0]) dtext(dc, g.fTitle, g.name, &nr, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    else dtext(dc, g.fTitle, L"가볍고 빠른 동영상 플레이어", &nr, RGB(95, 95, 95), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    vline(dc, er[E_PIN].left, b->top + S(8), b->bottom - S(8), C_TITLE_SEP);
    struct { int e; wchar_t gl; } bt[] = {
        { E_PIN, g.optTopmost ? 0xE840 : 0xE718 }, { E_MIN, 0xE921 },
        { E_MAX, IsZoomed(g.hwnd) ? 0xE923 : 0xE922 }, { E_FS, 0xE740 }, { E_CLOSE, 0xE8BB } };
    for (int i = 0; i < 5; i++) {
        RECT r = er[bt[i].e];
        btn_bg(dc, bt[i].e, &r);
        COLORREF c = g.hot == bt[i].e ? C_ICON_HOT : C_ICON;
        if (bt[i].e == E_PIN && g.optTopmost) c = C_ACCENT;
        glyph(dc, g.fIcon, bt[i].gl, &r, c);
    }
}

/* =====================================================================
 * 페인트: 컨트롤바
 * ===================================================================== */
enum { IC_PLAY, IC_PAUSE, IC_STOP, IC_PREV, IC_NEXT, IC_EJECT };

static void draw_icon(HDC dc, int id, const RECT *r, COLORREF c)
{
    GpGraphics *gr = gp_begin(dc); if (!gr) return;
    float cx = (r->left + r->right) / 2.0f, cy = (r->top + r->bottom) / 2.0f, u = SF(1);
    switch (id) {
    case IC_PLAY: { GpPointF p[3] = { { cx - 5 * u, cy - 7.5f * u }, { cx - 5 * u, cy + 7.5f * u }, { cx + 7.5f * u, cy } }; gp_poly(gr, p, 3, c); } break;
    case IC_PAUSE: gp_rrect(gr, cx - 5.5f * u, cy - 7 * u, 4 * u, 14 * u, 0.8f * u, c); gp_rrect(gr, cx + 1.5f * u, cy - 7 * u, 4 * u, 14 * u, 0.8f * u, c); break;
    case IC_STOP: gp_rrect(gr, cx - 6 * u, cy - 6 * u, 12 * u, 12 * u, 1.5f * u, c); break;
    case IC_PREV: { gp_rect(gr, cx - 6.5f * u, cy - 6 * u, 2.4f * u, 12 * u, c);
        GpPointF p[3] = { { cx + 6 * u, cy - 6.5f * u }, { cx + 6 * u, cy + 6.5f * u }, { cx - 4 * u, cy } }; gp_poly(gr, p, 3, c); } break;
    case IC_NEXT: { gp_rect(gr, cx + 4.1f * u, cy - 6 * u, 2.4f * u, 12 * u, c);
        GpPointF p[3] = { { cx - 6 * u, cy - 6.5f * u }, { cx - 6 * u, cy + 6.5f * u }, { cx + 4 * u, cy } }; gp_poly(gr, p, 3, c); } break;
    case IC_EJECT: { GpPointF p[3] = { { cx - 7 * u, cy + 1.5f * u }, { cx + 7 * u, cy + 1.5f * u }, { cx, cy - 6.5f * u } }; gp_poly(gr, p, 3, c);
        gp_rect(gr, cx - 7 * u, cy + 3.5f * u, 14 * u, 2.5f * u, c); } break;
    }
    GdipDeleteGraphics(gr);
}

static void seek_span(const RECT *s, float *x0, float *x1) { *x0 = s->left + SF(8); *x1 = s->right - SF(8); }

static void paint_ctrl(HDC dc, const RECT *b, RECT *er)
{
    RECT seekrow = { b->left, b->top, b->right, b->top + S(SEEK_H) };
    RECT btnrow = { b->left, seekrow.bottom, b->right, b->bottom };
    fillr(dc, &seekrow, C_SEEKBG);
    fillr(dc, &btnrow, C_BTNBG);
    hline(dc, b->left, b->right, btnrow.top, C_SEP);

    /* 버튼 배경 (아이콘보다 먼저) */
    int lb[] = { E_PLAY, E_STOP, E_PREV, E_NEXT, E_OPEN };
    int rb[] = { E_LIST, E_SET, E_MENU };
    for (int i = 0; i < 5; i++) { RECT r = er[lb[i]]; r.top++; btn_bg(dc, lb[i], &r); }
    for (int i = 0; i < 3; i++) { RECT r = er[rb[i]]; r.top++; btn_bg(dc, rb[i], &r); }

    GpGraphics *gr = gp_begin(dc);
    if (gr) {
        /* 탐색바 */
        RECT s = er[E_SEEK]; float x0, x1; seek_span(&s, &x0, &x1);
        float cy = (s.top + s.bottom) / 2.0f, u = SF(1), th = (g.hot == E_SEEK || g.drag == E_SEEK) ? 4 * u : 3 * u;
        double pos = g.drag == E_SEEK ? g.dragPos : g.pos;
        double fr = g.dur > 0 ? clampd(pos / g.dur, 0, 1) : 0;
        gp_rrect(gr, x0, cy - th / 2, x1 - x0, th, th / 2, C_TRACK);
        if (g.dur > 0 && g.abA >= 0) {
            float xa = x0 + (float)((x1 - x0) * clampd(g.abA / g.dur, 0, 1));
            float xb = g.abB >= 0 ? x0 + (float)((x1 - x0) * clampd(g.abB / g.dur, 0, 1)) : xa + 2 * u;
            gp_rect(gr, xa, cy - th / 2 - u, xb - xa, th + 2 * u, C_ABLOOP);
        }
        if (fr > 0) gp_rrect(gr, x0, cy - th / 2, (float)((x1 - x0) * fr), th, th / 2, C_PLAYED);
        if (g.optSeekCh && g.dur > 0)
            for (int i = 0; i < g.nch; i++) {
                if (g.ch[i].t <= 0.5) continue;
                float xc = x0 + (float)((x1 - x0) * clampd(g.ch[i].t / g.dur, 0, 1));
                gp_rect(gr, xc - u, cy - 4 * u, 2 * u, 8 * u, C_ACCENT);
            }
        if (!g.idle)
            gp_circle(gr, x0 + (float)((x1 - x0) * fr), cy, (g.hot == E_SEEK || g.drag == E_SEEK) ? 7 * u : 6 * u, RGB(250, 250, 250));

        /* 볼륨 */
        RECT v = er[E_VOL]; float vx0 = v.left + SF(7), vx1 = v.right - SF(7);
        float vf = (float)clampd(g.volume / 100.0, 0, 1);
        gp_rrect(gr, vx0, cy - 1.5f * u, vx1 - vx0, 3 * u, 1.5f * u, C_TRACK);
        gp_rrect(gr, vx0, cy - 1.5f * u, (vx1 - vx0) * vf, 3 * u, 1.5f * u, g.muted ? RGB(120, 120, 120) : C_ACCENT);
        gp_circle(gr, vx0 + (vx1 - vx0) * vf, cy, (g.hot == E_VOL || g.drag == E_VOL) ? 6.5f * u : 5.5f * u, RGB(250, 250, 250));

        /* 배지 */
        int bd[] = { E_B_HW, E_B_V, E_B_A, E_B_CH, E_B_SPEED };
        for (int i = 0; i < 5; i++) {
            RECT r = er[bd[i]]; if (r.right <= r.left) continue;
            gp_rrect(gr, (float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), 3 * u,
                     g.hot == bd[i] ? C_BADGE_HOT : C_BADGE);
        }
        GdipDeleteGraphics(gr);
    }
    glyph(dc, g.fIconBig, g.muted ? 0xE74F : (g.volume < 1 ? 0xE992 : g.volume < 34 ? 0xE993 : g.volume < 67 ? 0xE994 : 0xE995),
          &er[E_VOLICON], g.hot == E_VOLICON ? C_ICON_HOT : C_ICON);

    /* 왼쪽 버튼들 */
    int playing = !g.idle && !g.paused && !g.eof;
    int ics[] = { playing ? IC_PAUSE : IC_PLAY, IC_STOP, IC_PREV, IC_NEXT, IC_EJECT };
    for (int i = 0; i < 5; i++) {
        draw_icon(dc, ics[i], &er[lb[i]], g.hot == lb[i] ? C_ICON_HOT : C_ICON);
        vline(dc, er[lb[i]].right - 1, btnrow.top + 1, btnrow.bottom, C_SEP);
    }
    /* 오른쪽 버튼들 */
    wchar_t rg[] = { 0xE8FD, 0xE713, 0xE700 };
    for (int i = 0; i < 3; i++) {
        COLORREF c = g.hot == rb[i] ? C_ICON_HOT : C_ICON;
        if (rb[i] == E_LIST && g.plwnd && IsWindowVisible(g.plwnd)) c = C_ACCENT;
        glyph(dc, g.fIconBig, rg[i], &er[rb[i]], c);
        vline(dc, er[rb[i]].left, btnrow.top + 1, btnrow.bottom, C_SEP);
    }
    /* 시간 */
    if (er[E_TIME].right > er[E_TIME].left) {
        wchar_t cur[20], tot[20], rest[32];
        time_text(cur, tot); wsf(rest, 32, L" / %ls", tot);
        RECT r = er[E_TIME];
        dtext(dc, g.fTime, cur, &r, g.idle ? C_TEXT2 : C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        r.left += textw(g.fTime, cur);
        dtext(dc, g.fTime, rest, &r, C_TEXT2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    int bd[] = { E_B_HW, E_B_V, E_B_A, E_B_CH, E_B_SPEED };
    for (int i = 0; i < 5; i++) {
        RECT r = er[bd[i]]; if (r.right <= r.left) continue;
        wchar_t t[24]; badge_text(bd[i], t, 24);
        dtext(dc, g.fBadge, t, &r, bd[i] == E_B_SPEED ? C_ACCENT : C_BADGE_T, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void paint_main(HWND h)
{
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
    if (!g.fs) {
        RECT cr; GetClientRect(h, &cr);
        HDC bb = get_bb(cr.right, cr.bottom);
        RECT t, v, c, er[E_COUNT]; get_layout(&t, &v, &c); get_elems(h, er);
        paint_title(bb, &t, er);
        paint_ctrl(bb, &c, er);
        BitBlt(dc, t.left, t.top, t.right - t.left, t.bottom - t.top, bb, t.left, t.top, SRCCOPY);
        BitBlt(dc, c.left, c.top, c.right - c.left, c.bottom - c.top, bb, c.left, c.top, SRCCOPY);
    }
    EndPaint(h, &ps);
}

static void paint_video(HWND h)
{
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
    RECT r; GetClientRect(h, &r);
    if (r.right > 0 && r.bottom > 0) {
        HDC bb = get_bb(r.right, r.bottom);
        fillr(bb, &r, RGB(0, 0, 0));
        if (!g.hasVideo) {
            int cx = r.right / 2, cy = r.bottom / 2;
            if (!g.idle) {           /* 오디오 전용 */
                RECT a = { 0, cy - S(70), r.right, cy };
                HFONT f = mkfont(L"Segoe MDL2 Assets", S(48), FW_NORMAL);
                glyph(bb, f, 0xE8D6, &a, RGB(90, 90, 90)); DeleteObject(f);
                RECT nrc = { S(20), cy + S(4), r.right - S(20), cy + S(30) };
                dtext(bb, g.fHint, g.name, &nrc, RGB(170, 170, 170), DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
            } else if (r.bottom > S(120)) {
                GpGraphics *gr = gp_begin(bb);
                float u = SF(1), ly = cy - S(46);
                if (gr) {
                    gp_rrect(gr, cx - 26 * u, ly - 26 * u, 52 * u, 52 * u, 14 * u, C_ACCENT);
                    GpPointF p[3] = { { cx - 8 * u, ly - 13 * u }, { cx - 8 * u, ly + 13 * u }, { cx + 14 * u, ly } };
                    gp_poly(gr, p, 3, RGB(28, 28, 28));
                    GdipDeleteGraphics(gr);
                }
                RECT a = { 0, cy - S(16), r.right, cy + S(44) };
                dtext(bb, g.fBig, APP_NAME, &a, RGB(70, 70, 70), DT_CENTER | DT_TOP | DT_SINGLELINE);
                RECT b2 = { 0, cy + S(46), r.right, cy + S(70) };
                dtext(bb, g.fHint, g.err ? L"파일을 재생할 수 없습니다" : L"파일을 끌어다 놓거나 여기를 클릭해서 여세요", &b2,
                      g.err ? RGB(210, 100, 100) : RGB(120, 120, 120), DT_CENTER | DT_TOP | DT_SINGLELINE);
                RECT b3 = { 0, cy + S(72), r.right, cy + S(94) };
                dtext(bb, g.fHint2, L"Ctrl+O 파일 열기   ·   F2 폴더 열기   ·   Ctrl+U 주소 열기   ·   우클릭 메뉴", &b3,
                      RGB(78, 78, 78), DT_CENTER | DT_TOP | DT_SINGLELINE);
            }
        }
        BitBlt(dc, 0, 0, r.right, r.bottom, bb, 0, 0, SRCCOPY);
    }
    EndPaint(h, &ps);
}

/* =====================================================================
 * 툴팁 (탐색바 시간 표시)
 * ===================================================================== */
static void tip_show(int sx, int sy, const wchar_t *s)
{
    if (!g.tip) return;
    wcsncpy(g.tipText, s, 63); g.tipText[63] = 0;
    int w = textw(g.fUI, s) + S(16), h = S(24);
    SetWindowPos(g.tip, HWND_TOPMOST, sx - w / 2, sy - h - S(4), w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g.tip, NULL, FALSE);
}
static void tip_hide(void) { if (g.tip && IsWindowVisible(g.tip)) ShowWindow(g.tip, SW_HIDE); }

static LRESULT CALLBACK TipProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r);
        fillr(dc, &r, RGB(80, 80, 80));
        RECT in = { 1, 1, r.right - 1, r.bottom - 1 }; fillr(dc, &in, RGB(16, 16, 16));
        dtext(dc, g.fUI, g.tipText, &r, C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(h, &ps); return 0;
    }
    if (m == WM_NCHITTEST) return HTTRANSPARENT;
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, wp, lp);
}

/* =====================================================================
 * 재생목록
 * ===================================================================== */
static void play_index(int i);
static void pl_update(void);
static void update_title(void);

static void pl_add(const wchar_t *p)
{
    if (g.plCount == g.plCap) { g.plCap = g.plCap ? g.plCap * 2 : 64; g.pl = (wchar_t **)realloc(g.pl, g.plCap * sizeof(wchar_t *)); }
    g.pl[g.plCount++] = wdup(p);
}

static void pl_clear(void)
{
    for (int i = 0; i < g.plCount; i++) free(g.pl[i]);
    g.plCount = 0; g.plCur = -1; g.plHint = 0;
}

static void pl_remove(int i)
{
    if (i < 0 || i >= g.plCount) return;
    free(g.pl[i]);
    memmove(&g.pl[i], &g.pl[i + 1], (g.plCount - i - 1) * sizeof(wchar_t *));
    g.plCount--;
    if (i < g.plCur) g.plCur--;
    else if (i == g.plCur) { g.plCur = -1; g.plHint = i; }
    pl_update();
}

static int cmp_logical(const void *a, const void *b) { return StrCmpLogicalW(*(wchar_t *const *)a, *(wchar_t *const *)b); }

static void pl_sort(void)
{
    wchar_t *cur = g.plCur >= 0 ? g.pl[g.plCur] : NULL;
    qsort(g.pl, g.plCount, sizeof(wchar_t *), cmp_logical);
    if (cur) for (int i = 0; i < g.plCount; i++) if (g.pl[i] == cur) g.plCur = i;
    pl_update();
}

static void add_dir(const wchar_t *dir, int depth)
{
    wchar_t pat[MAX_PATH * 2];
    size_t dl = wcslen(dir);
    int slash = dl && (dir[dl - 1] == L'\\' || dir[dl - 1] == L'/');
    wsf(pat, MAX_PATH * 2, slash ? L"%ls*" : L"%ls\\*", dir);
    WIN32_FIND_DATAW fd;
    HANDLE fh = FindFirstFileExW(pat, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0);
    if (fh == INVALID_HANDLE_VALUE) return;
    wchar_t **files = NULL, **dirs = NULL; int nf = 0, nd = 0, cf = 0, cd = 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
        wchar_t full[MAX_PATH * 2];
        wsf(full, MAX_PATH * 2, slash ? L"%ls%ls" : L"%ls\\%ls", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth <= 0) continue;
            if (nd == cd) { cd = cd ? cd * 2 : 16; dirs = (wchar_t **)realloc(dirs, cd * sizeof(wchar_t *)); }
            dirs[nd++] = wdup(full);
        } else if (is_media(fd.cFileName)) {
            if (nf == cf) { cf = cf ? cf * 2 : 64; files = (wchar_t **)realloc(files, cf * sizeof(wchar_t *)); }
            files[nf++] = wdup(full);
        }
    } while (FindNextFileW(fh, &fd));
    FindClose(fh);
    if (nf) qsort(files, nf, sizeof(wchar_t *), cmp_logical);
    if (nd) qsort(dirs, nd, sizeof(wchar_t *), cmp_logical);
    for (int i = 0; i < nf; i++) { pl_add(files[i]); free(files[i]); }
    for (int i = 0; i < nd; i++) { add_dir(dirs[i], depth - 1); free(dirs[i]); }
    free(files); free(dirs);
}

static void open_paths(wchar_t **paths, int n, int append)
{
    if (n <= 0) return;
    int start, target = -1;
    if (!append) pl_clear();
    start = g.plCount;
    if (n == 1 && !is_url(paths[0])) {
        DWORD at = GetFileAttributesW(paths[0]);
        if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)) {
            add_dir(paths[0], 3);
        } else if (!append && g.optAutoFolder && is_media(paths[0])) {
            wchar_t dir[MAX_PATH * 2]; wcsncpy(dir, paths[0], MAX_PATH * 2 - 1); dir[MAX_PATH * 2 - 1] = 0;
            wchar_t *s = wcsrchr(dir, L'\\'); if (!s) s = wcsrchr(dir, L'/');
            if (s) { if (s == dir + 2 && dir[1] == L':') s[1] = 0; else *s = 0; add_dir(dir, 0); }
            for (int i = start; i < g.plCount; i++) if (!_wcsicmp(g.pl[i], paths[0])) { target = i; break; }
            if (target < 0) { pl_add(paths[0]); target = g.plCount - 1; }
        } else {
            pl_add(paths[0]);
        }
    } else {
        for (int i = 0; i < n; i++) {
            DWORD at = is_url(paths[i]) ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(paths[i]);
            if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)) add_dir(paths[i], 3);
            else pl_add(paths[i]);
        }
    }
    pl_update();
    if (g.plCount == start) return;
    if (target < 0) target = start;
    if (!append || g.idle) play_index(target);
}

static void pl_draw_item(DRAWITEMSTRUCT *d)
{
    RECT r = d->rcItem; HDC dc = d->hDC;
    if ((int)d->itemID < 0 || (int)d->itemID >= g.plCount) { fillr(dc, &r, C_LISTBG); return; }
    int i = (int)d->itemID, sel = (d->itemState & ODS_SELECTED) != 0, cur = i == g.plCur;
    fillr(dc, &r, sel ? C_LISTSEL : C_LISTBG);
    wchar_t num[16]; wsf(num, 16, L"%d", i + 1);
    RECT a = r; a.left += S(6); a.right = a.left + S(32);
    dtext(dc, g.fUI, num, &a, cur ? C_ACCENT : C_TEXT2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    RECT b = r; b.left = a.right + S(10); b.right -= S(8);
    dtext(dc, g.fUI, base_name(g.pl[i]), &b, cur ? C_ACCENT : C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void pl_update(void)
{
    if (!g.pllist) return;
    int sel = (int)SendMessageW(g.pllist, LB_GETCURSEL, 0, 0);
    int top = (int)SendMessageW(g.pllist, LB_GETTOPINDEX, 0, 0);
    SendMessageW(g.pllist, WM_SETREDRAW, FALSE, 0);
    SendMessageW(g.pllist, LB_SETCOUNT, g.plCount, 0);
    if (sel >= g.plCount) sel = g.plCount - 1;
    if (sel < 0) sel = g.plCur;
    SendMessageW(g.pllist, LB_SETTOPINDEX, top < g.plCount ? top : 0, 0);
    SendMessageW(g.pllist, LB_SETCURSEL, sel, 0);
    SendMessageW(g.pllist, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g.pllist, NULL, FALSE);
    InvalidateRect(g.plwnd, NULL, FALSE);
}

/* =====================================================================
 * 재생 제어
 * ===================================================================== */
static int next_index(int dir)
{
    if (g.plCount == 0) return -1;
    if (g.shuffle && g.plCount > 1) {
        int r; do { r = rand() % g.plCount; } while (r == g.plCur);
        return r;
    }
    int cur = g.plCur;
    if (cur < 0) { cur = dir > 0 ? g.plHint - 1 : g.plHint; }
    int n = cur + dir;
    if (n >= g.plCount) return g.repeat == 2 ? 0 : -1;
    if (n < 0) return g.repeat == 2 ? g.plCount - 1 : -1;
    return n;
}

static void set_name_from(const wchar_t *p)
{
    wcsncpy(g.name, base_name(p), MAX_PATH - 1); g.name[MAX_PATH - 1] = 0;
    if (is_url(p)) wcscpy(g.fmt, L"URL");
    else {
        const wchar_t *e = ext_of(p); int i;
        for (i = 0; e[i] && i < 8; i++) g.fmt[i] = towupper(e[i]);
        g.fmt[i] = 0;
    }
}

static void update_title(void)
{
    wchar_t t[MAX_PATH + 32];
    if (g.name[0]) wsf(t, MAX_PATH + 32, L"%ls - %ls", g.name, APP_NAME); else wcscpy(t, APP_NAME);
    SetWindowTextW(g.hwnd, t);
    inval_title();
    if (g.video) InvalidateRect(g.video, NULL, FALSE);
}

static void play_index(int i)
{
    if (i < 0 || i >= g.plCount || !g.mpv) return;
    if (g.optResume && !g.idle && !g.eof) mcmd("write-watch-later-config", NULL, NULL, NULL);
    g.plCur = i; g.err = 0; g.fitPending = g.optAutoFit; g.nch = 0; g.abA = g.abB = -1;
    char *p = u8(g.pl[i]);
    mcmd("loadfile", p, "replace", NULL);
    free(p);
    setp_f("pause", 0);
    set_name_from(g.pl[i]);
    update_title();
    if (!is_url(g.pl[i])) recent_add(g.pl[i]);
    pl_update();
}

static void close_media(int keepName)
{
    if (g.optResume && !g.idle && !g.eof) mcmd("write-watch-later-config", NULL, NULL, NULL);
    mcmd("stop", NULL, NULL, NULL);
    g.pos = 0; g.dur = 0; g.hasVideo = 0; g.nch = 0; g.abA = g.abB = -1;
    if (!keepName) { g.name[0] = 0; g.fmt[0] = 0; }
    update_title(); inval_all();
}

static void seek_abs(double t, int exact)
{
    char b[64]; snprintf(b, sizeof b, "%.3f", t);
    mcmd("seek", b, exact ? "absolute+exact" : "absolute+keyframes", NULL);
}

static void toggle_play(void);
static void open_file_dlg(int append);

static void toggle_play(void)
{
    if (g.idle) {
        if (g.plCount > 0) play_index(g.plCur >= 0 ? g.plCur : (g.plHint < g.plCount ? g.plHint : 0));
        else open_file_dlg(0);
        return;
    }
    if (g.eof) { seek_abs(0, 1); setp_f("pause", 0); return; }
    mcmd("cycle", "pause", NULL, NULL);
}

static void on_eof(void)
{
    if (g.repeat == 1) { seek_abs(0, 1); setp_f("pause", 0); return; }
    if (g.optResume) mcmd("delete-watch-later-config", NULL, NULL, NULL);
    int n = next_index(1);
    if (n >= 0) play_index(n);
    else if (g.optCloseEnd) close_media(0);
}

static void apply_topmost(void)
{
    SetWindowPos(g.hwnd, g.optTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    inval_title();
}

static void relayout(void)
{
    RECT t, v, c; get_layout(&t, &v, &c);
    if (g.video) MoveWindow(g.video, v.left, v.top, v.right - v.left, v.bottom - v.top, TRUE);
    InvalidateRect(g.hwnd, NULL, FALSE);
}

static void fsbar_show(int show)
{
    if (!g.fsbar) return;
    if (show && g.fs) {
        if (IsWindowVisible(g.fsbar)) return;
        MONITORINFO mi = { sizeof mi };
        GetMonitorInfoW(MonitorFromWindow(g.hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        int ch = S(CTRL_H);
        SetWindowPos(g.fsbar, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.bottom - ch,
                     mi.rcMonitor.right - mi.rcMonitor.left, ch, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(g.fsbar, NULL, FALSE);
    } else if (IsWindowVisible(g.fsbar)) {
        ShowWindow(g.fsbar, SW_HIDE);
        tip_hide();
    }
}

static void set_fullscreen(int on)
{
    if (on == g.fs) return;
    if (on) {
        g.fsPrev.length = sizeof g.fsPrev;
        GetWindowPlacement(g.hwnd, &g.fsPrev);
        MONITORINFO mi = { sizeof mi };
        GetMonitorInfoW(MonitorFromWindow(g.hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        g.fs = 1;
        SetWindowLongPtrW(g.hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN);
        SetWindowPos(g.hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    } else {
        g.fs = 0;
        fsbar_show(0);
        if (g.fill) { g.fill = 0; setp_d("panscan", 0); }
        SetWindowLongPtrW(g.hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE);
        SetWindowPlacement(g.hwnd, &g.fsPrev);
        SetWindowPos(g.hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    }
    relayout();
    if (g.optTopmost) apply_topmost();
}

static void fit_window(double factor)
{
    if (g.fs || g.vw <= 0 || g.vh <= 0) return;
    if (IsZoomed(g.hwnd)) { if (factor <= 0) return; ShowWindow(g.hwnd, SW_RESTORE); }
    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(MonitorFromWindow(g.hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    RECT wa = mi.rcWork;
    int bars = S(TITLE_H) + S(CTRL_H);
    double w = g.vw, h = g.vh, maxW = wa.right - wa.left, maxH = (wa.bottom - wa.top) - bars;
    if (factor > 0) { w *= factor; h *= factor; }
    else {
        double s = 1, lw = maxW * 0.85, lh = maxH * 0.85;
        if (w > lw) s = lw / w;
        if (h * s > lh) s = lh / h;
        w *= s; h *= s;
    }
    if (w > maxW) { h = h * maxW / w; w = maxW; }
    if (h > maxH) { w = w * maxH / h; h = maxH; }
    if (w < S(480)) w = S(480);
    int W = (int)(w + 0.5), H = (int)(h + 0.5) + bars;
    RECT cur; GetWindowRect(g.hwnd, &cur);
    int x = (cur.left + cur.right) / 2 - W / 2, y = (cur.top + cur.bottom) / 2 - H / 2;
    if (x + W > wa.right) x = wa.right - W;
    if (y + H > wa.bottom) y = wa.bottom - H;
    if (x < wa.left) x = wa.left;
    if (y < wa.top) y = wa.top;
    SetWindowPos(g.hwnd, NULL, x, y, W, H, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void disable_mpv_child(void)
{
    for (HWND c = GetWindow(g.video, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        if (IsWindowEnabled(c)) EnableWindow(c, FALSE);
}

/* =====================================================================
 * mpv 이벤트
 * ===================================================================== */
static void map_codec(const char *in, char *out, size_t n, int video)
{
    static const char *m[][2] = {
        { "h264", "AVC" }, { "hevc", "HEVC" }, { "av1", "AV1" }, { "vp9", "VP9" }, { "vp8", "VP8" },
        { "mpeg2video", "MPEG2" }, { "mpeg1video", "MPEG1" }, { "mpeg4", "MPEG4" }, { "vc1", "VC1" },
        { "wmv3", "WMV3" }, { "mjpeg", "MJPEG" }, { "prores", "ProRes" },
        { "mp3float", "MP3" }, { "mp3", "MP3" }, { "aac", "AAC" }, { "ac3", "AC3" }, { "eac3", "E-AC3" },
        { "truehd", "TrueHD" }, { "dts", "DTS" }, { "flac", "FLAC" }, { "opus", "Opus" }, { "vorbis", "Vorbis" },
        { NULL, NULL } };
    (void)video;
    out[0] = 0;
    if (!in || !*in) return;
    for (int i = 0; m[i][0]; i++) if (!_stricmp(in, m[i][0])) { snprintf(out, n, "%s", m[i][1]); return; }
    size_t i; for (i = 0; in[i] && i < n - 1 && i < 10; i++) out[i] = (char)toupper((unsigned char)in[i]);
    out[i] = 0;
}

static mpv_node *node_get(mpv_node *map, const char *key)
{
    if (!map || map->format != MPV_FORMAT_NODE_MAP) return NULL;
    mpv_node_list *l = map->u.list;
    for (int i = 0; i < l->num; i++) if (!strcmp(l->keys[i], key)) return &l->values[i];
    return NULL;
}

static void on_time_changed(void)
{
    int sec = (int)g.pos, px = 0;
    if (g.dur > 0) { RECT cr; GetClientRect(g.fs && g.fsbar ? g.fsbar : g.hwnd, &cr); px = (int)(cr.right * g.pos / g.dur); }
    if (sec != g.lastSec || px != g.lastPx) { g.lastSec = sec; g.lastPx = px; inval_ctrl(); }
}

static void on_prop(uint64_t id, mpv_event_property *p)
{
    int has = p->format != MPV_FORMAT_NONE && p->data;
    switch (id) {
    case P_TIME: g.pos = has ? *(double *)p->data : 0; on_time_changed(); return;
    case P_DUR: g.dur = has ? *(double *)p->data : 0; break;
    case P_PAUSE: g.paused = has ? *(int *)p->data : 0; break;
    case P_VOL: if (has) g.volume = *(double *)p->data; break;
    case P_MUTE: g.muted = has ? *(int *)p->data : 0; break;
    case P_SPEED: g.speed = has ? *(double *)p->data : 1.0; break;
    case P_HWDEC: { const char *s = has ? *(char **)p->data : NULL; g.hw = s && *s && strcmp(s, "no") != 0; } break;
    case P_VFMT: map_codec(has ? *(char **)p->data : NULL, g.vcodec, sizeof g.vcodec, 1); break;
    case P_ACODEC: map_codec(has ? *(char **)p->data : NULL, g.acodec, sizeof g.acodec, 0); break;
    case P_ACH: g.achannels = has ? (int)*(int64_t *)p->data : 0; break;
    case P_IDLE: g.idle = has ? *(int *)p->data : 1;
        if (g.idle) { g.hasVideo = 0; g.pos = 0; g.dur = 0; g.vcodec[0] = g.acodec[0] = 0; g.achannels = 0; }
        InvalidateRect(g.video, NULL, FALSE); break;
    case P_EOF: { int e = has ? *(int *)p->data : 0; int was = g.eof; g.eof = e; if (e && !was && !g.idle) on_eof(); } break;
    case P_CHAPTERS:
        g.nch = 0;
        if (has && p->format == MPV_FORMAT_NODE) {
            mpv_node *n = (mpv_node *)p->data;
            if (n->format == MPV_FORMAT_NODE_ARRAY)
                for (int i = 0; i < n->u.list->num && g.nch < 256; i++) {
                    mpv_node *it = &n->u.list->values[i], *t = node_get(it, "time"), *ti = node_get(it, "title");
                    g.ch[g.nch].t = t && t->format == MPV_FORMAT_DOUBLE ? t->u.double_ : 0;
                    if (ti && ti->format == MPV_FORMAT_STRING) w16(ti->u.string, g.ch[g.nch].title, 80);
                    else wsf(g.ch[g.nch].title, 80, L"챕터 %d", i + 1);
                    g.nch++;
                }
        }
        break;
    case P_ABA: g.abA = (has && p->format == MPV_FORMAT_DOUBLE) ? *(double *)p->data : -1; break;
    case P_ABB: g.abB = (has && p->format == MPV_FORMAT_DOUBLE) ? *(double *)p->data : -1; break;
    case P_TITLE:
        if (has && g.plCur >= 0 && g.plCur < g.plCount && is_url(g.pl[g.plCur])) {
            w16(*(char **)p->data, g.name, MAX_PATH); update_title();
        }
        return;
    }
    inval_ctrl();
}

static void on_video_reconfig(void)
{
    int64_t w = getp_i("dwidth", 0), h = getp_i("dheight", 0);
    int had = g.hasVideo;
    g.vw = (int)w; g.vh = (int)h; g.hasVideo = w > 0 && h > 0;
    if (had != g.hasVideo) InvalidateRect(g.video, NULL, FALSE);
    disable_mpv_child();
    if (g.fitPending && g.hasVideo) { g.fitPending = 0; fit_window(0); }
    inval_ctrl();
}

static void mpv_events(void)
{
    if (!g.mpv) return;
    for (;;) {
        mpv_event *e = f_mpv_wait_event(g.mpv, 0);
        if (!e || e->event_id == MPV_EVENT_NONE) break;
        switch (e->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: on_prop(e->reply_userdata, (mpv_event_property *)e->data); break;
        case MPV_EVENT_FILE_LOADED: g.err = 0; disable_mpv_child(); inval_all(); break;
        case MPV_EVENT_VIDEO_RECONFIG: on_video_reconfig(); break;
        case MPV_EVENT_END_FILE: {
            mpv_event_end_file *ef = (mpv_event_end_file *)e->data;
            if (ef && ef->reason == MPV_END_FILE_REASON_ERROR) {
                g.err = 1; InvalidateRect(g.video, NULL, FALSE);
                wchar_t msg[MAX_PATH + 40]; wsf(msg, MAX_PATH + 40, L"재생할 수 없음: %ls", g.name);
                wcsncpy(g.name, msg, MAX_PATH - 1); g.name[MAX_PATH - 1] = 0; update_title();
            }
        } break;
        case MPV_EVENT_SHUTDOWN: break;
        default: break;
        }
    }
}

static int mpv_load_dll(void)
{
    static const wchar_t *names[] = { L"libmpv-2.dll", L"mpv-2.dll", L"mpv-1.dll" };
    HMODULE m = NULL;
    for (int i = 0; i < 3 && !m; i++) {
        wchar_t p[MAX_PATH]; wsf(p, MAX_PATH, L"%ls\\%ls", g.exeDir, names[i]);
        m = LoadLibraryExW(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    for (int i = 0; i < 3 && !m; i++) m = LoadLibraryW(names[i]);
    if (!m) return 0;
#define X(r, n, a) f_##n = (r (*) a)(void *)GetProcAddress(m, #n); if (!f_##n) return 0;
    MPV_FN(X)
#undef X
    return 1;
}

static int mpv_start(void)
{
    g.mpv = f_mpv_create();
    if (!g.mpv) return 0;
    char b[MAX_PATH * 3];
    snprintf(b, sizeof b, "%lld", (long long)(intptr_t)g.video);
    f_mpv_set_option_string(g.mpv, "wid", b);
    const char *opts[][2] = {
        { "idle", "yes" }, { "keep-open", "yes" }, { "osc", "no" }, { "input-default-bindings", "no" },
        { "input-vo-keyboard", "no" }, { "input-cursor", "no" }, { "cursor-autohide", "no" },
        { "force-window", "no" }, { "osd-font", "Malgun Gothic" }, { "osd-font-size", "30" },
        { "osd-border-size", "2" }, { "osd-duration", "1200" }, { "osd-bar", "no" },
        { "sub-font", "Malgun Gothic" }, { "sub-auto", "fuzzy" }, { "sub-codepage", "auto" },
        { "volume-max", "100" }, { "screenshot-template", "DaBa_%F_%wH%wM%wS" }, { "screenshot-format", "png" },
        { "ytdl", "yes" }, { "audio-display", "embedded-first" }, { "hr-seek", "default" },
        { "watch-later-options", "start" }, { "demuxer-max-bytes", "150MiB" },
        { NULL, NULL } };
    for (int i = 0; opts[i][0]; i++) f_mpv_set_option_string(g.mpv, opts[i][0], opts[i][1]);
    f_mpv_set_option_string(g.mpv, "hwdec", g.optHwdec ? "auto-safe" : "no");
    f_mpv_set_option_string(g.mpv, "resume-playback", g.optResume ? "yes" : "no");
    f_mpv_set_option_string(g.mpv, "stop-screensaver", g.optNoSaver ? "yes" : "no");
    snprintf(b, sizeof b, "%d", (int)g.volume); f_mpv_set_option_string(g.mpv, "volume", b);
    {
        wchar_t wl[MAX_PATH]; wsf(wl, MAX_PATH, L"%ls\\watch_later", g.baseDir);
        char *s = u8(wl); f_mpv_set_option_string(g.mpv, "watch-later-dir", s); f_mpv_set_option_string(g.mpv, "watch-later-directory", s); free(s);
        wchar_t pic[MAX_PATH], sd[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYPICTURES, NULL, 0, pic))) {
            wsf(sd, MAX_PATH, L"%ls\\DaBa", pic);
            s = u8(sd); f_mpv_set_option_string(g.mpv, "screenshot-dir", s); f_mpv_set_option_string(g.mpv, "screenshot-directory", s); free(s);
        }
    }
    if (f_mpv_initialize(g.mpv) < 0) return 0;
    if (g.subBox) setp_s("sub-border-style", "opaque-box");

    struct { int id; const char *n; mpv_format f; } ob[] = {
        { P_TIME, "time-pos", MPV_FORMAT_DOUBLE }, { P_DUR, "duration", MPV_FORMAT_DOUBLE },
        { P_PAUSE, "pause", MPV_FORMAT_FLAG }, { P_VOL, "volume", MPV_FORMAT_DOUBLE },
        { P_MUTE, "mute", MPV_FORMAT_FLAG }, { P_SPEED, "speed", MPV_FORMAT_DOUBLE },
        { P_HWDEC, "hwdec-current", MPV_FORMAT_STRING }, { P_VFMT, "video-format", MPV_FORMAT_STRING },
        { P_ACODEC, "audio-codec-name", MPV_FORMAT_STRING }, { P_ACH, "audio-params/channel-count", MPV_FORMAT_INT64 },
        { P_IDLE, "idle-active", MPV_FORMAT_FLAG }, { P_EOF, "eof-reached", MPV_FORMAT_FLAG },
        { P_CHAPTERS, "chapter-list", MPV_FORMAT_NODE }, { P_ABA, "ab-loop-a", MPV_FORMAT_DOUBLE },
        { P_ABB, "ab-loop-b", MPV_FORMAT_DOUBLE }, { P_TITLE, "media-title", MPV_FORMAT_STRING } };
    for (size_t i = 0; i < sizeof ob / sizeof ob[0]; i++) f_mpv_observe_property(g.mpv, ob[i].id, ob[i].n, ob[i].f);
    f_mpv_set_wakeup_callback(g.mpv, on_mpv_wakeup, NULL);
    return 1;
}

/* =====================================================================
 * 파일 대화상자
 * ===================================================================== */
static int file_dialog(int folder, int multi, int kind, wchar_t ***out)
{
    *out = NULL;
    IFileOpenDialog *d = NULL;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&d))) return 0;
    DWORD o = 0; IFileOpenDialog_GetOptions(d, &o);
    o |= FOS_FORCEFILESYSTEM;
    if (folder) o |= FOS_PICKFOLDERS;
    if (multi) o |= FOS_ALLOWMULTISELECT;
    IFileOpenDialog_SetOptions(d, o);
    static wchar_t media[2048], vid[1024], aud[512], sub[256];
    if (!media[0]) {
        for (int i = 0; VEXT[i]; i++) { wcscat(vid, L"*."); wcscat(vid, VEXT[i]); wcscat(vid, L";"); }
        for (int i = 0; AEXT[i]; i++) { wcscat(aud, L"*."); wcscat(aud, AEXT[i]); wcscat(aud, L";"); }
        for (int i = 0; SEXT[i]; i++) { wcscat(sub, L"*."); wcscat(sub, SEXT[i]); wcscat(sub, L";"); }
        wcscpy(media, vid); wcscat(media, aud);
    }
    if (!folder) {
        if (kind == 1) {
            COMDLG_FILTERSPEC fs[] = { { L"자막 파일", sub }, { L"모든 파일", L"*.*" } };
            IFileOpenDialog_SetFileTypes(d, 2, fs);
            IFileOpenDialog_SetTitle(d, L"자막 열기");
        } else if (kind == 2) {
            COMDLG_FILTERSPEC fs[] = { { L"오디오 파일", aud }, { L"모든 파일", L"*.*" } };
            IFileOpenDialog_SetFileTypes(d, 2, fs);
            IFileOpenDialog_SetTitle(d, L"외부 오디오 불러오기");
        } else {
            COMDLG_FILTERSPEC fs[] = { { L"미디어 파일", media }, { L"동영상 파일", vid }, { L"오디오 파일", aud }, { L"모든 파일", L"*.*" } };
            IFileOpenDialog_SetFileTypes(d, 4, fs);
            IFileOpenDialog_SetTitle(d, L"파일 열기");
        }
    } else IFileOpenDialog_SetTitle(d, L"폴더 열기");

    int cnt = 0;
    if (SUCCEEDED(IFileOpenDialog_Show(d, g.hwnd))) {
        IShellItemArray *arr = NULL;
        if (SUCCEEDED(IFileOpenDialog_GetResults(d, &arr)) && arr) {
            DWORD n = 0; IShellItemArray_GetCount(arr, &n);
            *out = (wchar_t **)calloc(n ? n : 1, sizeof(wchar_t *));
            for (DWORD i = 0; i < n; i++) {
                IShellItem *it = NULL;
                if (SUCCEEDED(IShellItemArray_GetItemAt(arr, i, &it)) && it) {
                    LPWSTR nm = NULL;
                    if (SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &nm)) && nm) { (*out)[cnt++] = wdup(nm); CoTaskMemFree(nm); }
                    IShellItem_Release(it);
                }
            }
            IShellItemArray_Release(arr);
        }
    }
    IFileOpenDialog_Release(d);
    return cnt;
}

static void free_list(wchar_t **l, int n) { for (int i = 0; i < n; i++) free(l[i]); free(l); }

static void open_file_dlg(int append)
{
    wchar_t **l; int n = file_dialog(0, 1, 0, &l);
    if (n) open_paths(l, n, append);
    free_list(l, n);
}
static void open_folder_dlg(int append)
{
    wchar_t **l; int n = file_dialog(1, 0, 0, &l);
    if (n) open_paths(l, n, append);
    free_list(l, n);
}
static void open_sub_dlg(void)
{
    if (g.idle) return;
    wchar_t **l; int n = file_dialog(0, 1, 1, &l);
    for (int i = 0; i < n; i++) { char *s = u8(l[i]); mcmd("sub-add", s, "select", NULL); free(s); }
    if (n) osd(L"자막: %ls", base_name(l[0]));
    free_list(l, n);
}
static void open_audio_dlg(void)
{
    if (g.idle) return;
    wchar_t **l; int n = file_dialog(0, 0, 2, &l);
    if (n) { char *s = u8(l[0]); mcmd("audio-add", s, "select", NULL); free(s); osd(L"외부 오디오: %ls", base_name(l[0])); }
    free_list(l, n);
}

/* =====================================================================
 * 입력 상자 (주소 열기)
 * ===================================================================== */
static struct { HWND dlg, edit; wchar_t *buf; int cap, done, ok; } ib;

static LRESULT CALLBACK InputProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) { GetWindowTextW(ib.edit, ib.buf, ib.cap); ib.ok = 1; ib.done = 1; return 0; }
        if (LOWORD(wp) == IDCANCEL) { ib.done = 1; return 0; }
        break;
    case DM_GETDEFID: return MAKELRESULT(IDOK, DC_HASDEFID);
    case WM_CLOSE: ib.done = 1; return 0;
    case WM_CTLCOLORSTATIC: SetBkMode((HDC)wp, TRANSPARENT); return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    return DefWindowProcW(h, m, wp, lp);
}

static int input_box(const wchar_t *title, const wchar_t *prompt, wchar_t *buf, int cap)
{
    static int reg;
    if (!reg) {
        WNDCLASSW wc = { 0 }; wc.lpfnWndProc = InputProc; wc.hInstance = g.inst; wc.lpszClassName = L"DaBaInput";
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc); reg = 1;
    }
    RECT wr; GetWindowRect(g.hwnd, &wr);
    int W = S(540), H = S(160);
    RECT rr = { 0, 0, W, H }; AdjustWindowRectEx(&rr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int ww = rr.right - rr.left, hh = rr.bottom - rr.top;
    ib.buf = buf; ib.cap = cap; ib.done = 0; ib.ok = 0;
    ib.dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"DaBaInput", title, WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             (wr.left + wr.right - ww) / 2, (wr.top + wr.bottom - hh) / 2, ww, hh, g.hwnd, NULL, g.inst, NULL);
    HWND st = CreateWindowW(L"STATIC", prompt, WS_CHILD | WS_VISIBLE, S(16), S(16), W - S(32), S(22), ib.dlg, NULL, g.inst, NULL);
    ib.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", buf, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                              S(16), S(44), W - S(32), S(28), ib.dlg, (HMENU)100, g.inst, NULL);
    HWND ok = CreateWindowW(L"BUTTON", L"열기", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                            W - S(196), H - S(52), S(86), S(32), ib.dlg, (HMENU)IDOK, g.inst, NULL);
    HWND cc = CreateWindowW(L"BUTTON", L"취소", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            W - S(102), H - S(52), S(86), S(32), ib.dlg, (HMENU)IDCANCEL, g.inst, NULL);
    HWND ctl[] = { st, ib.edit, ok, cc };
    for (int i = 0; i < 4; i++) SendMessageW(ctl[i], WM_SETFONT, (WPARAM)g.fMenu, TRUE);
    EnableWindow(g.hwnd, FALSE);
    ShowWindow(ib.dlg, SW_SHOW);
    SetFocus(ib.edit); SendMessageW(ib.edit, EM_SETSEL, 0, -1);
    MSG msg;
    while (!ib.done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(ib.dlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (!ib.done) PostQuitMessage(0);
    EnableWindow(g.hwnd, TRUE);
    DestroyWindow(ib.dlg);
    SetForegroundWindow(g.hwnd);
    return ib.ok && buf[0];
}

static void trim(wchar_t *s)
{
    wchar_t *a = s; while (*a == L' ' || *a == L'"' || *a == L'\t' || *a == L'\r' || *a == L'\n') a++;
    size_t n = wcslen(a);
    while (n && (a[n - 1] == L' ' || a[n - 1] == L'"' || a[n - 1] == L'\r' || a[n - 1] == L'\n' || a[n - 1] == L'\t')) a[--n] = 0;
    memmove(s, a, (n + 1) * sizeof(wchar_t));
}

static void open_url_dlg(void)
{
    wchar_t buf[2048] = L"";
    if (OpenClipboard(g.hwnd)) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (h) { wchar_t *t = (wchar_t *)GlobalLock(h); if (t && wcsstr(t, L"://") && wcslen(t) < 2000) { wcscpy(buf, t); trim(buf); } GlobalUnlock(h); }
        CloseClipboard();
    }
    if (input_box(L"주소 열기", L"열 주소(URL)를 입력하세요 (http, https, rtsp, rtmp ...):", buf, 2048)) {
        trim(buf);
        wchar_t *l[1] = { buf };
        open_paths(l, 1, 0);
    }
}

static void open_clipboard(void)
{
    wchar_t **list = NULL; int n = 0;
    if (!OpenClipboard(g.hwnd)) return;
    HANDLE h = GetClipboardData(CF_HDROP);
    if (h) {
        HDROP hd = (HDROP)h; UINT c = DragQueryFileW(hd, 0xFFFFFFFF, NULL, 0);
        list = (wchar_t **)calloc(c ? c : 1, sizeof(wchar_t *));
        for (UINT i = 0; i < c; i++) {
            UINT len = DragQueryFileW(hd, i, NULL, 0) + 1;
            list[n] = (wchar_t *)malloc(len * sizeof(wchar_t));
            DragQueryFileW(hd, i, list[n], len); n++;
        }
    } else if ((h = GetClipboardData(CF_UNICODETEXT)) != NULL) {
        wchar_t *t = (wchar_t *)GlobalLock(h);
        if (t && wcslen(t) < 4000) {
            list = (wchar_t **)calloc(1, sizeof(wchar_t *));
            list[0] = wdup(t); trim(list[0]); n = list[0][0] ? 1 : 0;
            if (!n) { free(list[0]); }
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
    if (n) open_paths(list, n, 0);
    if (list) free_list(list, n);
}

static void copy_text(const wchar_t *s)
{
    if (!OpenClipboard(g.hwnd)) return;
    EmptyClipboard();
    size_t n = (wcslen(s) + 1) * sizeof(wchar_t);
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, n);
    if (hg) { memcpy(GlobalLock(hg), s, n); GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT, hg); }
    CloseClipboard();
}

/* =====================================================================
 * 오너드로우 다크 메뉴
 * ===================================================================== */
#define MRADIO 0x40000000u
typedef struct { wchar_t text[96], acc[32]; int sub, radio, sep; } MItem;
static MItem g_mi[1200]; static int g_mn;

static MItem *mi_new(void) { if (g_mn >= (int)(sizeof g_mi / sizeof g_mi[0])) g_mn = (int)(sizeof g_mi / sizeof g_mi[0]) - 1; MItem *it = &g_mi[g_mn++]; memset(it, 0, sizeof *it); return it; }

static void mi_set(MItem *it, const wchar_t *t)
{
    const wchar_t *tab = wcschr(t, L'\t');
    if (tab) {
        size_t n = (size_t)(tab - t); if (n > 95) n = 95;
        wcsncpy(it->text, t, n); it->text[n] = 0;
        wcsncpy(it->acc, tab + 1, 31); it->acc[31] = 0;
    } else { wcsncpy(it->text, t, 95); it->text[95] = 0; }
}

static void madd(HMENU m, UINT id, const wchar_t *t, UINT fl)
{
    MItem *it = mi_new(); mi_set(it, t); it->radio = (fl & MRADIO) != 0;
    AppendMenuW(m, MF_OWNERDRAW | (fl & ~MRADIO), id, (LPCWSTR)it);
}
static HMENU msub(HMENU m, const wchar_t *t, UINT fl)
{
    HMENU s = CreatePopupMenu();
    MItem *it = mi_new(); mi_set(it, t); it->sub = 1;
    AppendMenuW(m, MF_OWNERDRAW | MF_POPUP | fl, (UINT_PTR)s, (LPCWSTR)it);
    return s;
}
static void msep(HMENU m) { MItem *it = mi_new(); it->sep = 1; AppendMenuW(m, MF_OWNERDRAW | MF_SEPARATOR, 0, (LPCWSTR)it); }
#define CHK(x) ((x) ? MF_CHECKED : 0)
#define RAD(x) (MRADIO | ((x) ? MF_CHECKED : 0))
#define DIS(x) ((x) ? MF_GRAYED : 0)

static BOOL menu_measure(MEASUREITEMSTRUCT *mi)
{
    if (mi->CtlType != ODT_MENU || !mi->itemData) return FALSE;
    MItem *it = (MItem *)mi->itemData;
    if (it->sep) { mi->itemHeight = S(9); mi->itemWidth = S(40); return TRUE; }
    int tw = textw(g.fMenu, it->text), aw = it->acc[0] ? textw(g.fMenu, it->acc) + S(36) : 0;
    int w = S(46) + tw + aw + S(34) - GetSystemMetrics(SM_CXMENUCHECK);
    if (w < S(160)) w = S(160);
    mi->itemWidth = w; mi->itemHeight = S(30);
    return TRUE;
}

static BOOL menu_draw(DRAWITEMSTRUCT *d)
{
    if (d->CtlType != ODT_MENU || !d->itemData) return FALSE;
    MItem *it = (MItem *)d->itemData; HDC dc = d->hDC; RECT r = d->rcItem;
    if (it->sep) {
        fillr(dc, &r, C_MBG);
        hline(dc, r.left, r.right, (r.top + r.bottom) / 2, C_MSEP);
        hline(dc, r.left, r.right, (r.top + r.bottom) / 2 + 1, RGB(78, 78, 78));
        ExcludeClipRect(dc, r.left, r.top, r.right, r.bottom);
        return TRUE;
    }
    int sel = (d->itemState & ODS_SELECTED) != 0, dis = (d->itemState & (ODS_GRAYED | ODS_DISABLED)) != 0,
        chk = (d->itemState & ODS_CHECKED) != 0;
    fillr(dc, &r, sel && !dis ? C_MSEL : C_MBG);
    COLORREF tc = dis ? C_MDIS : sel ? C_ACCENT : C_MTEXT;
    if (chk) {
        GpGraphics *gr = gp_begin(dc);
        float cx = r.left + SF(20), cy = (r.top + r.bottom) / 2.0f;
        if (gr) {
            if (it->radio) { gp_circle(gr, cx, cy, SF(7), RGB(235, 235, 235)); gp_circle(gr, cx, cy, SF(4.5f), C_RADIO); }
            else gp_rrect(gr, cx - SF(9), cy - SF(9), SF(18), SF(18), SF(4), C_CHECKBG);
            GdipDeleteGraphics(gr);
        }
        if (!it->radio) { RECT c = { r.left + S(11), r.top, r.left + S(29), r.bottom }; glyph(dc, g.fCheck, 0xE73E, &c, RGB(240, 240, 240)); }
    }
    RECT t = r; t.left += S(40); t.right -= S(28);
    dtext(dc, g.fMenu, it->text, &t, tc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (it->acc[0]) dtext(dc, g.fMenu, it->acc, &t, dis ? C_MDIS : sel ? C_ACCENT : C_MACC, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    if (it->sub) { RECT a = r; a.right -= S(12); dtext(dc, g.fMenu, L">", &a, tc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE); }
    ExcludeClipRect(dc, r.left, r.top, r.right, r.bottom);   /* 시스템 화살표 그리기 방지 */
    return TRUE;
}

/* ---------- 명령 ID ---------- */
enum {
    ID_OPEN_FILE = 1000, ID_OPEN_URL, ID_OPEN_FOLDER, ID_OPEN_CLIP, ID_OPEN_SUB, ID_ADD_SUB, ID_RELOAD_SUB, ID_ADD_AUDIO,
    ID_CLOSE, ID_REOPEN, ID_CLEAR_RECENT, ID_ADD_FILES,
    ID_PLAYPAUSE, ID_STOP, ID_PREV, ID_NEXT, ID_CH_PREV, ID_CH_NEXT,
    ID_SEEK_B5, ID_SEEK_F5, ID_SEEK_B30, ID_SEEK_F30, ID_SEEK_B60, ID_SEEK_F60, ID_SEEK_HOME, ID_FRAME_NEXT, ID_FRAME_PREV,
    ID_SPEED_UP, ID_SPEED_DOWN, ID_SPEED_RESET,
    ID_AB_TOGGLE, ID_AB_A, ID_AB_B, ID_AB_CLEAR,
    ID_REPEAT_NONE, ID_REPEAT_ONE, ID_REPEAT_ALL, ID_SHUFFLE,
    ID_OPT_RESUME, ID_OPT_HIDECURSOR, ID_OPT_NOSAVER, ID_OPT_CLOSEEND, ID_OPT_SEEKTIME, ID_OPT_SEEKCH,
    ID_OPT_TOPMOST, ID_OPT_AUTOFIT, ID_OPT_AUTOFOLDER, ID_OPT_HWDEC, ID_OPEN_CONFIG_DIR,
    ID_SUB_CYCLE, ID_SUB_VIS, ID_SUB_DELAY_M, ID_SUB_DELAY_P, ID_SUB_DELAY_0, ID_SUB_BIG, ID_SUB_SMALL, ID_SUB_SIZE0,
    ID_SUB_UP, ID_SUB_DOWN, ID_SUB_POS0, ID_SUB_COPY, ID_SUB_BOX,
    ID_DEINT, ID_ROT0, ID_ROT90, ID_ROT180, ID_ROT270, ID_HFLIP, ID_VFLIP, ID_SHOT, ID_SHOT_VIDEO, ID_SHOT_DIR,
    ID_VID_RESET, ID_BRI_M, ID_BRI_P, ID_CON_M, ID_CON_P, ID_SAT_M, ID_SAT_P, ID_HUE_M, ID_HUE_P,
    ID_AUD_CYCLE, ID_VOL_UP, ID_VOL_DOWN, ID_MUTE, ID_AD_P, ID_AD_M, ID_AD_0, ID_ACH_AUTO, ID_ACH_STEREO, ID_ACH_51,
    ID_NORMALIZE, ID_PITCH,
    ID_SIZE_50, ID_SIZE_100, ID_SIZE_150, ID_SIZE_200, ID_SIZE_FIT,
    ID_FULLSCREEN, ID_FILLSCREEN, ID_PLAYLIST, ID_INFO, ID_STATS, ID_ABOUT, ID_SETTINGS, ID_EXIT,
    ID_PL_PLAY, ID_PL_REMOVE, ID_PL_ADDFILE, ID_PL_ADDFOLDER, ID_PL_SORT, ID_PL_CLEAR,
    ID_AR = 4800, ID_ATRACK = 5000, ID_STRACK = 5200, ID_RECENT = 5400, ID_CHAPTER = 5500, ID_SPEED = 5800
};

static const double SPEEDS[] = { 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 3.0, 4.0 };
static const char *ASPECTS[] = { "-1", "4:3", "16:9", "1.85:1", "2.35:1", NULL };
static const wchar_t *ASPECT_NAMES[] = { L"원본 비율", L"4:3", L"16:9", L"1.85:1", L"2.35:1", L"창에 맞춰 늘이기" };

static void build_tracks(HMENU m, const char *type, UINT base)
{
    int isSub = !strcmp(type, "sub");
    mpv_node n; int any = 0, anySel = 0;
    if (g.idle || f_mpv_get_property(g.mpv, "track-list", MPV_FORMAT_NODE, &n) < 0) {
        madd(m, 0, L"(없음)", MF_GRAYED); return;
    }
    if (n.format == MPV_FORMAT_NODE_ARRAY)
        for (int i = 0; i < n.u.list->num; i++) {
            mpv_node *it = &n.u.list->values[i], *t = node_get(it, "type"), *s = node_get(it, "selected");
            if (t && t->format == MPV_FORMAT_STRING && !strcmp(t->u.string, type) && s && s->u.flag) anySel = 1;
        }
    if (isSub) madd(m, base, L"자막 끄기", RAD(!anySel));
    if (n.format == MPV_FORMAT_NODE_ARRAY)
        for (int i = 0; i < n.u.list->num; i++) {
            mpv_node *it = &n.u.list->values[i];
            mpv_node *t = node_get(it, "type"), *id = node_get(it, "id"), *sel = node_get(it, "selected"),
                     *ti = node_get(it, "title"), *la = node_get(it, "lang"), *co = node_get(it, "codec"), *ex = node_get(it, "external");
            if (!t || t->format != MPV_FORMAT_STRING || strcmp(t->u.string, type) || !id) continue;
            wchar_t wt[64] = L"", wl[24] = L"", wc[24] = L"", lab[160];
            if (ti && ti->format == MPV_FORMAT_STRING) w16(ti->u.string, wt, 64);
            if (la && la->format == MPV_FORMAT_STRING) w16(la->u.string, wl, 24);
            if (co && co->format == MPV_FORMAT_STRING) w16(co->u.string, wc, 24);
            int extl = ex && ex->format == MPV_FORMAT_FLAG && ex->u.flag;
            wsf(lab, 160, L"%d. %ls%ls%ls%ls%ls%ls", (int)id->u.int64, wt, (wt[0] && wl[0]) ? L" " : L"", wl[0] ? L"[" : L"", wl,
                wl[0] ? L"]" : L"", extl ? L" (외부)" : L"");
            if (!wt[0] && !wl[0]) { size_t l = wcslen(lab); wsf(lab + l, 160 - l, L"%ls%ls", wc[0] ? wc : (isSub ? L"자막" : L"소리"), extl ? L" (외부)" : L""); }
            madd(m, base + (UINT)id->u.int64, lab, RAD(sel && sel->u.flag));
            any = 1;
        }
    f_mpv_free_node_contents(&n);
    if (!any && !isSub) madd(m, 0, L"(없음)", MF_GRAYED);
}

static void build_settings(HMENU m)
{
    madd(m, ID_OPT_HWDEC, L"하드웨어 가속 디코딩 (H/W)", CHK(g.optHwdec));
    madd(m, ID_OPT_TOPMOST, L"항상 위", CHK(g.optTopmost));
    madd(m, ID_OPT_AUTOFIT, L"파일을 열면 창 크기를 영상에 맞춤", CHK(g.optAutoFit));
    madd(m, ID_OPT_AUTOFOLDER, L"파일을 열면 같은 폴더의 파일을 재생목록에 추가", CHK(g.optAutoFolder));
    msep(m);
    madd(m, ID_OPT_RESUME, L"재생 위치 기억", CHK(g.optResume));
    madd(m, ID_OPT_HIDECURSOR, L"재생 중 마우스 커서 숨기기", CHK(g.optHideCursor));
    madd(m, ID_OPT_NOSAVER, L"재생 중 화면 보호기 실행 금지", CHK(g.optNoSaver));
    madd(m, ID_OPT_CLOSEEND, L"재생 완료 시 미디어 닫기", CHK(g.optCloseEnd));
    msep(m);
    madd(m, ID_OPT_SEEKTIME, L"탐색바에 마우스 위치 시 시간 표시", CHK(g.optSeekTime));
    madd(m, ID_OPT_SEEKCH, L"탐색바에 챕터 표시", CHK(g.optSeekCh));
    msep(m);
    madd(m, ID_OPEN_CONFIG_DIR, L"설정 폴더 열기", 0);
}

static HMENU build_main_menu(void)
{
    g_mn = 0;
    int nf = g.idle;
    HMENU root = CreatePopupMenu(), s, s2;
    madd(root, ID_OPEN_FILE, L"파일 열기...\tF3", 0);
    s = msub(root, L"열기", 0);
    madd(s, ID_OPEN_FILE, L"파일 열기...\tCtrl+O", 0);
    madd(s, ID_OPEN_URL, L"주소 열기...\tCtrl+U", 0);
    madd(s, ID_OPEN_FOLDER, L"폴더 열기...\tF2", 0);
    madd(s, ID_OPEN_CLIP, L"클립 보드 열기\tCtrl+V", 0);
    madd(s, ID_ADD_FILES, L"재생목록에 파일 추가...", 0);
    msep(s);
    madd(s, ID_SHOT, L"화면 캡처\tCtrl+S", DIS(nf));
    msep(s);
    madd(s, ID_OPEN_SUB, L"자막 열기...\tAlt+O", DIS(nf));
    madd(s, ID_ADD_SUB, L"자막 추가...", DIS(nf));
    madd(s, ID_RELOAD_SUB, L"자막 다시 읽기\tCtrl+Alt+Y", DIS(nf));
    msep(s);
    s2 = msub(s, L"최근/다시 열기", 0);
    if (!g.nRecent) madd(s2, 0, L"(없음)", MF_GRAYED);
    for (int i = 0; i < g.nRecent; i++) {
        wchar_t t[MAX_PATH + 8]; wsf(t, MAX_PATH + 8, L"%d. %ls%ls", i + 1, base_name(g.recent[i]), i == 0 ? L"\tCtrl+Y" : L"");
        madd(s2, ID_RECENT + i, t, 0);
    }
    if (g.nRecent) { msep(s2); madd(s2, ID_CLEAR_RECENT, L"목록 지우기", 0); }
    madd(s, ID_ADD_AUDIO, L"외부 오디오 불러오기...", DIS(nf));
    madd(root, ID_CLOSE, L"닫기\tF4", DIS(nf));
    msep(root);

    /* 재생 */
    s = msub(root, L"재생", 0);
    madd(s, ID_PLAYPAUSE, L"재생/일시 중지\tSpace", 0);
    madd(s, ID_STOP, L"정지", DIS(nf));
    msep(s);
    madd(s, ID_PREV, L"이전 파일 재생\tPgUp", DIS(g.plCount < 2));
    madd(s, ID_NEXT, L"다음 파일 재생\tPgDn", DIS(g.plCount < 2));
    msep(s);
    s2 = msub(s, L"챕터", DIS(g.nch == 0));
    madd(s2, ID_CH_PREV, L"이전 챕터\tCtrl+PgUp", 0);
    madd(s2, ID_CH_NEXT, L"다음 챕터\tCtrl+PgDn", 0);
    if (g.nch) {
        msep(s2);
        int cur = (int)getp_i("chapter", -1);
        for (int i = 0; i < g.nch && i < 200; i++) {
            wchar_t t[128], tm[16]; fmt_time(g.ch[i].t, tm, 16);
            wsf(t, 128, L"%ls\t%ls", g.ch[i].title, tm);
            madd(s2, ID_CHAPTER + i, t, RAD(i == cur));
        }
    }
    s2 = msub(s, L"탐색", DIS(nf));
    madd(s2, ID_SEEK_B5, L"5초 뒤로\t←", 0);      madd(s2, ID_SEEK_F5, L"5초 앞으로\t→", 0);
    madd(s2, ID_SEEK_B30, L"30초 뒤로\tCtrl+←", 0); madd(s2, ID_SEEK_F30, L"30초 앞으로\tCtrl+→", 0);
    madd(s2, ID_SEEK_B60, L"1분 뒤로\tAlt+←", 0);   madd(s2, ID_SEEK_F60, L"1분 앞으로\tAlt+→", 0);
    msep(s2);
    madd(s2, ID_FRAME_PREV, L"이전 프레임\tD", 0);   madd(s2, ID_FRAME_NEXT, L"다음 프레임\tF", 0);
    madd(s2, ID_SEEK_HOME, L"처음으로\tHome", 0);
    s2 = msub(s, L"재생 속도", 0);
    madd(s2, ID_SPEED_UP, L"빠르게 (+0.1)\tC", 0);
    madd(s2, ID_SPEED_DOWN, L"느리게 (-0.1)\tX", 0);
    madd(s2, ID_SPEED_RESET, L"기본 속도\tZ", 0);
    msep(s2);
    for (int i = 0; i < (int)(sizeof SPEEDS / sizeof SPEEDS[0]); i++) {
        wchar_t t[16]; wsf(t, 16, L"%.2gx", SPEEDS[i]);
        madd(s2, ID_SPEED + i, t, RAD(fabs(g.speed - SPEEDS[i]) < 0.001));
    }
    s2 = msub(s, L"구간 반복\tB", DIS(nf));
    madd(s2, ID_AB_TOGGLE, L"구간 반복 단계 전환 (시작→끝→해제)\tB", 0);
    madd(s2, ID_AB_A, L"현재 위치를 시작 지점으로", CHK(g.abA >= 0));
    madd(s2, ID_AB_B, L"현재 위치를 끝 지점으로", CHK(g.abB >= 0));
    madd(s2, ID_AB_CLEAR, L"구간 반복 해제", 0);
    s2 = msub(s, L"반복 재생", 0);
    madd(s2, ID_REPEAT_NONE, L"반복 안 함", RAD(g.repeat == 0));
    madd(s2, ID_REPEAT_ONE, L"한 파일 반복", RAD(g.repeat == 1));
    madd(s2, ID_REPEAT_ALL, L"전체 반복", RAD(g.repeat == 2));
    msep(s);
    madd(s, ID_SHUFFLE, L"셔플(랜덤) 재생", CHK(g.shuffle));
    madd(s, ID_OPT_RESUME, L"재생 위치 기억", CHK(g.optResume));
    madd(s, ID_OPT_HIDECURSOR, L"재생 중 마우스 커서 숨기기", CHK(g.optHideCursor));
    madd(s, ID_OPT_NOSAVER, L"재생 중 화면 보호기 실행 금지", CHK(g.optNoSaver));
    madd(s, ID_OPT_CLOSEEND, L"재생 완료 시 미디어 닫기", CHK(g.optCloseEnd));
    msep(s);
    madd(s, ID_OPT_SEEKTIME, L"탐색바에 마우스 위치 시 시간 표시", CHK(g.optSeekTime));
    madd(s, ID_OPT_SEEKCH, L"탐색바에 책갈피/챕터 표시", CHK(g.optSeekCh));

    /* 자막 */
    s = msub(root, L"자막", 0);
    s2 = msub(s, L"자막 선택\tL", DIS(nf)); build_tracks(s2, "sub", ID_STRACK);
    madd(s, ID_SUB_VIS, L"자막 보이기\tAlt+H", CHK(g.subVis));
    msep(s);
    madd(s, ID_OPEN_SUB, L"자막 열기...\tAlt+O", DIS(nf));
    madd(s, ID_RELOAD_SUB, L"자막 다시 읽기\tCtrl+Alt+Y", DIS(nf));
    msep(s);
    s2 = msub(s, L"자막 싱크(동기화)", DIS(nf));
    madd(s2, ID_SUB_DELAY_M, L"0.5초 빠르게\t,", 0);
    madd(s2, ID_SUB_DELAY_P, L"0.5초 느리게\t.", 0);
    madd(s2, ID_SUB_DELAY_0, L"싱크 초기화\t/", 0);
    s2 = msub(s, L"자막 크기", DIS(nf));
    madd(s2, ID_SUB_BIG, L"크게\tAlt+↑", 0);
    madd(s2, ID_SUB_SMALL, L"작게\tAlt+↓", 0);
    madd(s2, ID_SUB_SIZE0, L"기본 크기", 0);
    s2 = msub(s, L"자막 위치", DIS(nf));
    madd(s2, ID_SUB_UP, L"위로\tCtrl+↑", 0);
    madd(s2, ID_SUB_DOWN, L"아래로\tCtrl+↓", 0);
    madd(s2, ID_SUB_POS0, L"기본 위치", 0);
    msep(s);
    madd(s, ID_SUB_COPY, L"현재 자막 클립 보드로 복사", DIS(nf));
    madd(s, ID_SUB_BOX, L"자막 불투명 박스", CHK(g.subBox));

    /* 영상 */
    s = msub(root, L"영상", 0);
    madd(s, ID_OPT_HWDEC, L"하드웨어 가속 디코딩", CHK(g.optHwdec));
    madd(s, ID_DEINT, L"디인터레이스", CHK(g.deint));
    s2 = msub(s, L"영상 회전", 0);
    madd(s2, ID_ROT0, L"회전 안 함", RAD(g.rotate == 0));
    madd(s2, ID_ROT90, L"90도", RAD(g.rotate == 90));
    madd(s2, ID_ROT180, L"180도", RAD(g.rotate == 180));
    madd(s2, ID_ROT270, L"270도", RAD(g.rotate == 270));
    msep(s2);
    madd(s2, ID_HFLIP, L"좌우 반전", CHK(g.hflip));
    madd(s2, ID_VFLIP, L"상하 반전", CHK(g.vflip));
    s2 = msub(s, L"영상 캡처\tK", DIS(nf));
    madd(s2, ID_SHOT, L"현재 화면 캡처 (자막 포함)\tCtrl+S", 0);
    madd(s2, ID_SHOT_VIDEO, L"영상만 캡처 (자막 제외)\tK", 0);
    madd(s2, ID_SHOT_DIR, L"캡처 폴더 열기", 0);
    msep(s);
    madd(s, ID_VID_RESET, L"영상 속성 초기화\tQ", 0);
    madd(s, ID_BRI_M, L"명도 1% 감소\tW", 0); madd(s, ID_BRI_P, L"명도 1% 증가\tE", 0);
    madd(s, ID_CON_M, L"대비 1% 감소\tR", 0); madd(s, ID_CON_P, L"대비 1% 증가\tT", 0);
    madd(s, ID_SAT_M, L"채도 1% 감소\tY", 0); madd(s, ID_SAT_P, L"채도 1% 증가\tU", 0);
    madd(s, ID_HUE_M, L"색상 1% 감소\tI", 0); madd(s, ID_HUE_P, L"색상 1% 증가\tO", 0);

    /* 소리 */
    s = msub(root, L"소리", 0);
    s2 = msub(s, L"소리 선택\tA", DIS(nf)); build_tracks(s2, "audio", ID_ATRACK);
    madd(s, ID_ADD_AUDIO, L"외부 오디오 불러오기...", DIS(nf));
    msep(s);
    madd(s, ID_VOL_UP, L"소리 크게\tUp", 0);
    madd(s, ID_VOL_DOWN, L"소리 작게\tDown", 0);
    madd(s, ID_MUTE, L"음소거\tM", CHK(g.muted));
    msep(s);
    s2 = msub(s, L"소리 싱크(동기화)", DIS(nf));
    madd(s2, ID_AD_M, L"0.05초 빠르게\t-", 0);
    madd(s2, ID_AD_P, L"0.05초 느리게\t=", 0);
    madd(s2, ID_AD_0, L"싱크 초기화", 0);
    s2 = msub(s, L"소리 출력 채널", 0);
    {
        char *ac = f_mpv_get_property_string(g.mpv, "audio-channels");
        int k = ac && !strcmp(ac, "stereo") ? 1 : ac && !strcmp(ac, "5.1") ? 2 : 0;
        if (ac) f_mpv_free(ac);
        madd(s2, ID_ACH_AUTO, L"자동", RAD(k == 0));
        madd(s2, ID_ACH_STEREO, L"스테레오 (2.0)", RAD(k == 1));
        madd(s2, ID_ACH_51, L"5.1 채널", RAD(k == 2));
    }
    msep(s);
    madd(s, ID_NORMALIZE, L"소리 정규화 (작은 소리 키우기)", CHK(g.normalize));
    madd(s, ID_PITCH, L"배속 재생 시 음정 유지", CHK(g.pitch));

    s = msub(root, L"환경/기타", 0);
    build_settings(s);
    msep(root);

    s = msub(root, L"화면 비율", 0);
    for (int i = 0; i < 6; i++) madd(s, ID_AR + i, ASPECT_NAMES[i], RAD(g.aspect == i));
    s = msub(root, L"화면 크기", 0);
    madd(s, ID_SIZE_50, L"0.5배\tAlt+1", DIS(!g.hasVideo));
    madd(s, ID_SIZE_100, L"1배 (원본)\tAlt+2", DIS(!g.hasVideo));
    madd(s, ID_SIZE_150, L"1.5배\tAlt+3", DIS(!g.hasVideo));
    madd(s, ID_SIZE_200, L"2배\tAlt+4", DIS(!g.hasVideo));
    madd(s, ID_SIZE_FIT, L"화면에 알맞게\tAlt+0", DIS(!g.hasVideo));
    madd(root, ID_FULLSCREEN, L"전체 화면\tEnter", CHK(g.fs));
    madd(root, ID_FILLSCREEN, L"꽉찬 화면\tCtrl+Enter", CHK(g.fill));
    msep(root);
    madd(root, ID_SETTINGS, L"환경 설정...\tF5", 0);
    madd(root, ID_PLAYLIST, L"재생목록...\tF6", CHK(g.plwnd && IsWindowVisible(g.plwnd)));
    madd(root, ID_INFO, L"재생 정보...\tCtrl+F1", DIS(nf));
    madd(root, ID_STATS, L"통계 오버레이\tTab", DIS(nf));
    madd(root, ID_ABOUT, L"프로그램 정보...\tF1", 0);
    msep(root);
    madd(root, ID_EXIT, L"종료\tAlt+F4", 0);
    return root;
}

static void do_command(int id);

static void popup(HMENU m, HWND owner, int x, int y, UINT align)
{
    MENUINFO mi = { sizeof mi }; mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS; mi.hbrBack = g.brMenu;
    SetMenuInfo(m, &mi);
    tip_hide();
    int id = (int)TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | align, x, y, owner, NULL);
    DestroyMenu(m);
    if (id) do_command(id);
}

static void show_main_menu(int x, int y, UINT align) { popup(build_main_menu(), g.hwnd, x, y, align); }
static void show_settings_menu(int x, int y, UINT align)
{
    g_mn = 0; HMENU m = CreatePopupMenu(); build_settings(m);
    popup(m, g.hwnd, x, y, align);
}

/* =====================================================================
 * 정보 창
 * ===================================================================== */
static void prop_w(const char *name, wchar_t *out, int cap)
{
    char *s = g.mpv ? f_mpv_get_property_string(g.mpv, name) : NULL;
    if (s) { w16(s, out, cap); f_mpv_free(s); } else wcscpy(out, L"-");
}

static void show_info(void)
{
    if (g.idle) return;
    wchar_t path[MAX_PATH * 2], ff[64], vc[96], w[16], h[16], fps[24], hw[32], ac[96], sr[16], chn[16], dur[16], size[32];
    prop_w("path", path, MAX_PATH * 2); prop_w("file-format", ff, 64); prop_w("video-codec", vc, 96);
    prop_w("width", w, 16); prop_w("height", h, 16); prop_w("container-fps", fps, 24); prop_w("hwdec-current", hw, 32);
    prop_w("audio-codec", ac, 96); prop_w("audio-params/samplerate", sr, 16); prop_w("audio-params/channel-count", chn, 16);
    fmt_time(g.dur, dur, 16);
    int64_t fsz = getp_i("file-size", -1);
    if (fsz >= 0) wsf(size, 32, L"%.1f MB", fsz / 1048576.0); else wcscpy(size, L"-");
    double vbr = getp_d("video-bitrate", 0), abr = getp_d("audio-bitrate", 0);
    wchar_t t[4096];
    wsf(t, 4096,
        L"[파일]\n이름: %ls\n경로: %ls\n형식: %ls\n크기: %ls\n재생 시간: %ls\n\n"
        L"[영상]\n코덱: %ls\n해상도: %ls x %ls\n프레임: %ls fps\n비트레이트: %.0f kbps\n하드웨어 디코딩: %ls\n\n"
        L"[소리]\n코덱: %ls\n샘플레이트: %ls Hz\n채널: %ls\n비트레이트: %.0f kbps",
        g.name, path, ff, size, dur, vc, w, h, fps, vbr / 1000, hw, ac, sr, chn, abr / 1000);
    MessageBoxW(g.hwnd, t, L"재생 정보", MB_OK | MB_ICONINFORMATION);
}

static void show_about(void)
{
    MessageBoxW(g.hwnd,
        L"DaBa " APP_VER L"\n가볍고 빠른 동영상 플레이어\n재생 엔진: mpv (libmpv)\n\n"
        L"주요 단축키\n"
        L"Space\t재생/일시 중지\n"
        L"Enter\t전체 화면     Ctrl+Enter  꽉찬 화면\n"
        L"← / →\t5초 이동     Ctrl: 30초   Alt: 1분\n"
        L"↑ / ↓\t소리 크게/작게     M  음소거\n"
        L"PgUp/PgDn\t이전/다음 파일\n"
        L"C / X / Z\t재생 속도 빠르게/느리게/기본\n"
        L"D / F\t이전/다음 프레임\n"
        L"B\t구간 반복 (시작→끝→해제)\n"
        L", / . /  /\t자막 싱크 빠르게/느리게/초기화\n"
        L"L / A\t자막/소리 트랙 전환\n"
        L"Alt+H\t자막 보이기/숨기기\n"
        L"Q W E R T Y U I O\t영상 속성 조절\n"
        L"Ctrl+S\t화면 캡처\n"
        L"F2 / F3\t폴더 열기 / 파일 열기\n"
        L"Ctrl+U / Ctrl+V\t주소 열기 / 클립보드 열기\n"
        L"F6\t재생목록     Ctrl+F1  재생 정보",
        L"프로그램 정보", MB_OK | MB_ICONINFORMATION);
}

/* =====================================================================
 * 명령 처리
 * ===================================================================== */
static void adj_int(const char *name, int d, const wchar_t *label)
{
    int64_t v = getp_i(name, 0) + d;
    if (v < -100) v = -100;
    if (v > 100) v = 100;
    setp_i(name, v);
    osd(L"%ls: %d%%", label, (int)v);
}

static void set_volume(double v)
{
    v = clampd(v, 0, 100); g.volume = v; setp_d("volume", v);
    if (g.muted) setp_f("mute", 0);
    osd(L"소리: %d%%", (int)(v + 0.5));
    inval_ctrl();
}

static void set_speed(double s)
{
    s = clampd(floor(s * 100 + 0.5) / 100, 0.1, 8.0);
    setp_d("speed", s); g.speed = s;
    osd(L"재생 속도: %.2fx", s);
    inval_ctrl();
}

static void open_dir(const wchar_t *d) { SHCreateDirectoryExW(NULL, d, NULL); ShellExecuteW(g.hwnd, L"open", d, NULL, NULL, SW_SHOWNORMAL); }

static void shot_dir(wchar_t *out)
{
    char *s = f_mpv_get_property_string(g.mpv, "screenshot-dir");
    if (s && *s) w16(s, out, MAX_PATH); else wcscpy(out, g.baseDir);
    if (s) f_mpv_free(s);
}

static void pl_toggle(void);

static void do_command(int id)
{
    if (!g.mpv && id != ID_EXIT && id != ID_ABOUT) return;
    if (id >= ID_ATRACK && id < ID_ATRACK + 200) { setp_i("aid", id - ID_ATRACK); return; }
    if (id >= ID_STRACK && id < ID_STRACK + 200) { if (id == ID_STRACK) setp_s("sid", "no"); else setp_i("sid", id - ID_STRACK); return; }
    if (id >= ID_RECENT && id < ID_RECENT + 10) {
        wchar_t p[MAX_PATH]; wcscpy(p, g.recent[id - ID_RECENT]); wchar_t *l[1] = { p }; open_paths(l, 1, 0); return;
    }
    if (id >= ID_CHAPTER && id < ID_CHAPTER + 256) { setp_i("chapter", id - ID_CHAPTER); return; }
    if (id >= ID_SPEED && id < ID_SPEED + 10) { set_speed(SPEEDS[id - ID_SPEED]); return; }
    if (id >= ID_AR && id < ID_AR + 6) {
        int k = id - ID_AR; g.aspect = k;
        if (k == 5) { setp_s("keepaspect", "no"); setp_s("video-aspect-override", "-1"); }
        else { setp_s("keepaspect", "yes"); setp_s("video-aspect-override", ASPECTS[k]); }
        osd(L"화면 비율: %ls", ASPECT_NAMES[k]);
        return;
    }
    switch (id) {
    case ID_OPEN_FILE: open_file_dlg(0); break;
    case ID_ADD_FILES: case ID_PL_ADDFILE: open_file_dlg(1); break;
    case ID_PL_ADDFOLDER: open_folder_dlg(1); break;
    case ID_OPEN_URL: open_url_dlg(); break;
    case ID_OPEN_FOLDER: open_folder_dlg(0); break;
    case ID_OPEN_CLIP: open_clipboard(); break;
    case ID_OPEN_SUB: case ID_ADD_SUB: open_sub_dlg(); break;
    case ID_RELOAD_SUB: mcmd("sub-reload", NULL, NULL, NULL); osd(L"자막 다시 읽기"); break;
    case ID_ADD_AUDIO: open_audio_dlg(); break;
    case ID_CLOSE: close_media(0); break;
    case ID_REOPEN: if (g.nRecent) { wchar_t p[MAX_PATH]; wcscpy(p, g.recent[0]); wchar_t *l[1] = { p }; open_paths(l, 1, 0); } break;
    case ID_CLEAR_RECENT: g.nRecent = 0; save_recent(); break;

    case ID_PLAYPAUSE: toggle_play(); break;
    case ID_STOP: close_media(1); break;
    case ID_PREV: { int n = next_index(-1); if (n >= 0) play_index(n); } break;
    case ID_NEXT: { int n = next_index(1); if (n >= 0) play_index(n); } break;
    case ID_CH_PREV: mcmdf("osd-msg add chapter -1"); break;
    case ID_CH_NEXT: mcmdf("osd-msg add chapter 1"); break;
    case ID_SEEK_B5: mcmdf("osd-msg seek -5"); break;
    case ID_SEEK_F5: mcmdf("osd-msg seek 5"); break;
    case ID_SEEK_B30: mcmdf("osd-msg seek -30"); break;
    case ID_SEEK_F30: mcmdf("osd-msg seek 30"); break;
    case ID_SEEK_B60: mcmdf("osd-msg seek -60"); break;
    case ID_SEEK_F60: mcmdf("osd-msg seek 60"); break;
    case ID_SEEK_HOME: seek_abs(0, 1); break;
    case ID_FRAME_NEXT: mcmd("frame-step", NULL, NULL, NULL); break;
    case ID_FRAME_PREV: mcmd("frame-back-step", NULL, NULL, NULL); break;
    case ID_SPEED_UP: set_speed(g.speed + 0.1); break;
    case ID_SPEED_DOWN: set_speed(g.speed - 0.1); break;
    case ID_SPEED_RESET: set_speed(1.0); break;
    case ID_AB_TOGGLE:
        if (g.abA < 0) { setp_d("ab-loop-a", g.pos); osd(L"구간 반복: 시작 지점 설정"); }
        else if (g.abB < 0) { setp_d("ab-loop-b", g.pos); osd(L"구간 반복: 끝 지점 설정 (반복 중)"); }
        else { setp_s("ab-loop-a", "no"); setp_s("ab-loop-b", "no"); osd(L"구간 반복 해제"); }
        break;
    case ID_AB_A: setp_d("ab-loop-a", g.pos); osd(L"구간 반복: 시작 지점 설정"); break;
    case ID_AB_B: setp_d("ab-loop-b", g.pos); osd(L"구간 반복: 끝 지점 설정"); break;
    case ID_AB_CLEAR: setp_s("ab-loop-a", "no"); setp_s("ab-loop-b", "no"); osd(L"구간 반복 해제"); break;
    case ID_REPEAT_NONE: g.repeat = 0; osd(L"반복 안 함"); break;
    case ID_REPEAT_ONE: g.repeat = 1; osd(L"한 파일 반복"); break;
    case ID_REPEAT_ALL: g.repeat = 2; osd(L"전체 반복"); break;
    case ID_SHUFFLE: g.shuffle = !g.shuffle; osd(g.shuffle ? L"셔플 재생 켬" : L"셔플 재생 끔"); break;

    case ID_OPT_RESUME: g.optResume = !g.optResume; setp_s("resume-playback", g.optResume ? "yes" : "no"); break;
    case ID_OPT_HIDECURSOR: g.optHideCursor = !g.optHideCursor; break;
    case ID_OPT_NOSAVER: g.optNoSaver = !g.optNoSaver; setp_s("stop-screensaver", g.optNoSaver ? "yes" : "no"); break;
    case ID_OPT_CLOSEEND: g.optCloseEnd = !g.optCloseEnd; break;
    case ID_OPT_SEEKTIME: g.optSeekTime = !g.optSeekTime; break;
    case ID_OPT_SEEKCH: g.optSeekCh = !g.optSeekCh; inval_ctrl(); break;
    case ID_OPT_TOPMOST: g.optTopmost = !g.optTopmost; apply_topmost(); break;
    case ID_OPT_AUTOFIT: g.optAutoFit = !g.optAutoFit; break;
    case ID_OPT_AUTOFOLDER: g.optAutoFolder = !g.optAutoFolder; break;
    case ID_OPT_HWDEC: g.optHwdec = !g.optHwdec; setp_s("hwdec", g.optHwdec ? "auto-safe" : "no");
        osd(g.optHwdec ? L"하드웨어 가속 디코딩 켬" : L"소프트웨어 디코딩"); break;
    case ID_OPEN_CONFIG_DIR: open_dir(g.baseDir); break;
    case ID_SETTINGS: { POINT p; GetCursorPos(&p); show_settings_menu(p.x, p.y, 0); } break;

    case ID_SUB_CYCLE: mcmd("cycle", "sub", NULL, NULL); mcmd("show-text", "자막: ${?sid==no:끔}${!sid==no:${current-tracks/sub/title:${current-tracks/sub/lang:${sid}}}}", "1500", NULL); break;
    case ID_SUB_VIS: g.subVis = !g.subVis; setp_f("sub-visibility", g.subVis); osd(g.subVis ? L"자막 보이기" : L"자막 숨기기"); break;
    case ID_SUB_DELAY_M: mcmd("add", "sub-delay", "-0.5", NULL); osd(L"자막 싱크: %+.1f초", getp_d("sub-delay", 0)); break;
    case ID_SUB_DELAY_P: mcmd("add", "sub-delay", "0.5", NULL); osd(L"자막 싱크: %+.1f초", getp_d("sub-delay", 0)); break;
    case ID_SUB_DELAY_0: setp_d("sub-delay", 0); osd(L"자막 싱크 초기화"); break;
    case ID_SUB_BIG: mcmd("add", "sub-scale", "0.1", NULL); osd(L"자막 크기: %d%%", (int)(getp_d("sub-scale", 1) * 100 + 0.5)); break;
    case ID_SUB_SMALL: mcmd("add", "sub-scale", "-0.1", NULL); osd(L"자막 크기: %d%%", (int)(getp_d("sub-scale", 1) * 100 + 0.5)); break;
    case ID_SUB_SIZE0: setp_d("sub-scale", 1.0); osd(L"자막 크기: 100%%"); break;
    case ID_SUB_UP: mcmd("add", "sub-pos", "-1", NULL); break;
    case ID_SUB_DOWN: mcmd("add", "sub-pos", "1", NULL); break;
    case ID_SUB_POS0: setp_i("sub-pos", 100); break;
    case ID_SUB_COPY: {
        char *s = f_mpv_get_property_string(g.mpv, "sub-text");
        if (s && *s) { int n = (int)strlen(s) + 1; wchar_t *w = (wchar_t *)malloc(n * sizeof(wchar_t)); w16(s, w, n); copy_text(w); free(w); osd(L"자막을 클립 보드로 복사했습니다"); }
        else osd(L"현재 표시 중인 자막이 없습니다");
        if (s) f_mpv_free(s);
    } break;
    case ID_SUB_BOX: g.subBox = !g.subBox; setp_s("sub-border-style", g.subBox ? "opaque-box" : "outline-and-shadow"); break;

    case ID_DEINT: g.deint = !g.deint; setp_s("deinterlace", g.deint ? "yes" : "no"); osd(g.deint ? L"디인터레이스 켬" : L"디인터레이스 끔"); break;
    case ID_ROT0: case ID_ROT90: case ID_ROT180: case ID_ROT270:
        g.rotate = (id - ID_ROT0) * 90; setp_i("video-rotate", g.rotate); osd(L"영상 회전: %d도", g.rotate); break;
    case ID_HFLIP: g.hflip = !g.hflip; mcmd("vf", "toggle", "@dabahf:hflip", NULL); break;
    case ID_VFLIP: g.vflip = !g.vflip; mcmd("vf", "toggle", "@dabavf:vflip", NULL); break;
    case ID_SHOT: case ID_SHOT_VIDEO: {
        wchar_t d[MAX_PATH]; shot_dir(d); SHCreateDirectoryExW(NULL, d, NULL);
        mcmd("osd-msg", "screenshot", id == ID_SHOT ? "subtitles" : "video", NULL);
    } break;
    case ID_SHOT_DIR: { wchar_t d[MAX_PATH]; shot_dir(d); open_dir(d); } break;
    case ID_VID_RESET: setp_i("brightness", 0); setp_i("contrast", 0); setp_i("saturation", 0); setp_i("hue", 0); osd(L"영상 속성 초기화"); break;
    case ID_BRI_M: adj_int("brightness", -1, L"명도"); break;
    case ID_BRI_P: adj_int("brightness", 1, L"명도"); break;
    case ID_CON_M: adj_int("contrast", -1, L"대비"); break;
    case ID_CON_P: adj_int("contrast", 1, L"대비"); break;
    case ID_SAT_M: adj_int("saturation", -1, L"채도"); break;
    case ID_SAT_P: adj_int("saturation", 1, L"채도"); break;
    case ID_HUE_M: adj_int("hue", -1, L"색상"); break;
    case ID_HUE_P: adj_int("hue", 1, L"색상"); break;

    case ID_AUD_CYCLE: mcmd("cycle", "audio", NULL, NULL); mcmd("show-text", "소리: ${?aid==no:끔}${!aid==no:${current-tracks/audio/title:${current-tracks/audio/lang:${aid}}}}", "1500", NULL); break;
    case ID_VOL_UP: set_volume(g.volume + 5); break;
    case ID_VOL_DOWN: set_volume(g.volume - 5); break;
    case ID_MUTE: mcmd("cycle", "mute", NULL, NULL); osd(getp_i("mute", 0) ? L"음소거" : L"음소거 해제"); break;
    case ID_AD_P: mcmd("add", "audio-delay", "0.05", NULL); osd(L"소리 싱크: %+.0fms", getp_d("audio-delay", 0) * 1000); break;
    case ID_AD_M: mcmd("add", "audio-delay", "-0.05", NULL); osd(L"소리 싱크: %+.0fms", getp_d("audio-delay", 0) * 1000); break;
    case ID_AD_0: setp_d("audio-delay", 0); osd(L"소리 싱크 초기화"); break;
    case ID_ACH_AUTO: setp_s("audio-channels", "auto-safe"); break;
    case ID_ACH_STEREO: setp_s("audio-channels", "stereo"); break;
    case ID_ACH_51: setp_s("audio-channels", "5.1"); break;
    case ID_NORMALIZE: g.normalize = !g.normalize; mcmd("af", "toggle", "@dabanorm:lavfi=[dynaudnorm=f=250:g=31]", NULL);
        osd(g.normalize ? L"소리 정규화 켬" : L"소리 정규화 끔"); break;
    case ID_PITCH: g.pitch = !g.pitch; setp_f("audio-pitch-correction", g.pitch); break;

    case ID_SIZE_50: fit_window(0.5); break;
    case ID_SIZE_100: fit_window(1.0); break;
    case ID_SIZE_150: fit_window(1.5); break;
    case ID_SIZE_200: fit_window(2.0); break;
    case ID_SIZE_FIT: fit_window(0); break;
    case ID_FULLSCREEN: set_fullscreen(!g.fs); break;
    case ID_FILLSCREEN:
        g.fill = !g.fill; setp_d("panscan", g.fill ? 1.0 : 0.0);
        if (g.fill && !g.fs) set_fullscreen(1);
        osd(g.fill ? L"꽉찬 화면" : L"꽉찬 화면 해제");
        break;
    case ID_PLAYLIST: pl_toggle(); break;
    case ID_INFO: show_info(); break;
    case ID_STATS: mcmd("script-binding", "stats/display-stats-toggle", NULL, NULL); break;
    case ID_ABOUT: show_about(); break;
    case ID_EXIT: PostMessageW(g.hwnd, WM_CLOSE, 0, 0); break;

    case ID_PL_PLAY: { int s = g.pllist ? (int)SendMessageW(g.pllist, LB_GETCURSEL, 0, 0) : -1; if (s >= 0) play_index(s); } break;
    case ID_PL_REMOVE: { int s = g.pllist ? (int)SendMessageW(g.pllist, LB_GETCURSEL, 0, 0) : -1; if (s >= 0) pl_remove(s); } break;
    case ID_PL_SORT: pl_sort(); break;
    case ID_PL_CLEAR: { int keep = !g.idle; wchar_t *cur = keep && g.plCur >= 0 ? wdup(g.pl[g.plCur]) : NULL;
        pl_clear(); if (cur) { pl_add(cur); g.plCur = 0; free(cur); } pl_update(); } break;
    }
    inval_all();
}

static int handle_key(UINT vk)
{
    int ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0, shift = GetKeyState(VK_SHIFT) < 0;
    int plain = !ctrl && !alt, id = 0;
    (void)shift;
    switch (vk) {
    case VK_SPACE: id = ID_PLAYPAUSE; break;
    case VK_RETURN: id = ctrl ? ID_FILLSCREEN : ID_FULLSCREEN; break;
    case VK_ESCAPE: if (g.fs) id = ID_FULLSCREEN; break;
    case VK_LEFT: id = ctrl ? ID_SEEK_B30 : alt ? ID_SEEK_B60 : ID_SEEK_B5; break;
    case VK_RIGHT: id = ctrl ? ID_SEEK_F30 : alt ? ID_SEEK_F60 : ID_SEEK_F5; break;
    case VK_UP: id = alt ? ID_SUB_BIG : ctrl ? ID_SUB_UP : ID_VOL_UP; break;
    case VK_DOWN: id = alt ? ID_SUB_SMALL : ctrl ? ID_SUB_DOWN : ID_VOL_DOWN; break;
    case VK_PRIOR: id = ctrl ? ID_CH_PREV : ID_PREV; break;
    case VK_NEXT: id = ctrl ? ID_CH_NEXT : ID_NEXT; break;
    case VK_HOME: id = ID_SEEK_HOME; break;
    case VK_F1: id = ctrl ? ID_INFO : ID_ABOUT; break;
    case VK_F2: id = ID_OPEN_FOLDER; break;
    case VK_F3: id = ID_OPEN_FILE; break;
    case VK_F4: if (!alt) id = ID_CLOSE; break;
    case VK_F5: id = ID_SETTINGS; break;
    case VK_F6: id = ID_PLAYLIST; break;
    case VK_TAB: if (plain) id = ID_STATS; break;
    case VK_OEM_COMMA: if (plain) id = ID_SUB_DELAY_M; break;
    case VK_OEM_PERIOD: if (plain) id = ID_SUB_DELAY_P; break;
    case VK_OEM_2: if (plain) id = ID_SUB_DELAY_0; break;
    case VK_OEM_MINUS: case VK_SUBTRACT: id = ID_AD_M; break;
    case VK_OEM_PLUS: case VK_ADD: id = ID_AD_P; break;
    case VK_MEDIA_PLAY_PAUSE: id = ID_PLAYPAUSE; break;
    case VK_MEDIA_NEXT_TRACK: id = ID_NEXT; break;
    case VK_MEDIA_PREV_TRACK: id = ID_PREV; break;
    case VK_MEDIA_STOP: id = ID_STOP; break;
    case '0': if (alt) id = ID_SIZE_FIT; break;
    case '1': if (alt) id = ID_SIZE_50; break;
    case '2': if (alt) id = ID_SIZE_100; break;
    case '3': if (alt) id = ID_SIZE_150; break;
    case '4': if (alt) id = ID_SIZE_200; break;
    case 'O': id = ctrl ? ID_OPEN_FILE : alt ? ID_OPEN_SUB : ID_HUE_P; break;
    case 'U': id = ctrl ? ID_OPEN_URL : plain ? ID_SAT_P : 0; break;
    case 'V': if (ctrl) id = ID_OPEN_CLIP; break;
    case 'Y': id = (ctrl && alt) ? ID_RELOAD_SUB : ctrl ? ID_REOPEN : plain ? ID_SAT_M : 0; break;
    case 'S': if (ctrl) id = ID_SHOT; break;
    case 'K': if (plain) id = ID_SHOT_VIDEO; break;
    case 'H': if (alt) id = ID_SUB_VIS; break;
    case 'L': if (plain) id = ID_SUB_CYCLE; break;
    case 'A': if (plain) id = ID_AUD_CYCLE; break;
    case 'M': if (plain) id = ID_MUTE; break;
    case 'B': if (plain) id = ID_AB_TOGGLE; break;
    case 'C': if (plain) id = ID_SPEED_UP; break;
    case 'X': if (plain) id = ID_SPEED_DOWN; break;
    case 'Z': if (plain) id = ID_SPEED_RESET; break;
    case 'F': if (plain) id = ID_FRAME_NEXT; break;
    case 'D': if (plain) id = ID_FRAME_PREV; break;
    case 'Q': if (plain) id = ID_VID_RESET; break;
    case 'W': if (plain) id = ID_BRI_M; break;
    case 'E': if (plain) id = ID_BRI_P; break;
    case 'R': if (plain) id = ID_CON_M; break;
    case 'T': if (plain) id = ID_CON_P; break;
    case 'I': if (plain) id = ID_HUE_M; break;
    case 'P': if (ctrl) id = ID_PLAYLIST; break;
    }
    if (id) { do_command(id); return 1; }
    return 0;
}

/* =====================================================================
 * 마우스 (메인 창 + 전체화면 컨트롤바 공용)
 * ===================================================================== */
static void seek_tip(HWND w, const RECT *s, int x, double t)
{
    if (!g.optSeekTime || g.dur <= 0) { tip_hide(); return; }
    wchar_t b[20]; fmt_time(t, b, 20);
    POINT p = { x, s->top }; ClientToScreen(w, &p);
    tip_show(p.x, p.y, b);
}

static double frac_of(const RECT *s, int x)
{
    float x0, x1; seek_span(s, &x0, &x1);
    return x1 > x0 ? clampd((x - x0) / (x1 - x0), 0, 1) : 0;
}

static void elem_action(HWND w, int e, RECT *er)
{
    POINT p;
    switch (e) {
    case E_LOGO: p.x = er[E_LOGO].left; p.y = er[E_LOGO].bottom; ClientToScreen(w, &p); show_main_menu(p.x, p.y, 0); break;
    case E_PIN: do_command(ID_OPT_TOPMOST); break;
    case E_MIN: ShowWindow(g.hwnd, SW_MINIMIZE); break;
    case E_MAX: ShowWindow(g.hwnd, IsZoomed(g.hwnd) ? SW_RESTORE : SW_MAXIMIZE); break;
    case E_FS: do_command(ID_FULLSCREEN); break;
    case E_CLOSE: PostMessageW(g.hwnd, WM_CLOSE, 0, 0); break;
    case E_VOLICON: do_command(ID_MUTE); break;
    case E_PLAY: toggle_play(); break;
    case E_STOP: do_command(ID_STOP); break;
    case E_PREV: do_command(ID_PREV); break;
    case E_NEXT: do_command(ID_NEXT); break;
    case E_OPEN: open_file_dlg(0); break;
    case E_TIME: g.showRemain = !g.showRemain; inval_ctrl(); break;
    case E_B_HW: do_command(ID_OPT_HWDEC); break;
    case E_B_SPEED: do_command(ID_SPEED_RESET); break;
    case E_B_V: case E_B_A: case E_B_CH: do_command(ID_INFO); break;
    case E_LIST: do_command(ID_PLAYLIST); break;
    case E_SET: p.x = er[E_SET].left; p.y = er[E_SET].top; ClientToScreen(w, &p); show_settings_menu(p.x, p.y, TPM_BOTTOMALIGN); break;
    case E_MENU: p.x = er[E_MENU].right; p.y = er[E_MENU].top; ClientToScreen(w, &p); show_main_menu(p.x, p.y, TPM_BOTTOMALIGN | TPM_RIGHTALIGN); break;
    }
}

static void inval_w(HWND w) { if (w == g.hwnd) { inval_title(); inval_ctrl(); } else InvalidateRect(w, NULL, FALSE); }

static void ui_mouse(HWND w, UINT msg, int x, int y)
{
    RECT er[E_COUNT]; get_elems(w, er);
    int e = hit_er(er, x, y);
    switch (msg) {
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT tme = { sizeof tme, TME_LEAVE, w, 0 }; TrackMouseEvent(&tme);
        if (g.drag == E_SEEK) {
            g.dragPos = frac_of(&er[E_SEEK], x) * g.dur;
            if (GetTickCount() - g.lastSeekTick > 50) { seek_abs(g.dragPos, 0); g.lastSeekTick = GetTickCount(); }
            seek_tip(w, &er[E_SEEK], x, g.dragPos);
            inval_w(w); return;
        }
        if (g.drag == E_VOL) { g.volume = frac_of(&er[E_VOL], x) * 100; setp_d("volume", g.volume); inval_w(w); return; }
        if (e != g.hot) { g.hot = e; inval_w(w); }
        if (e == E_SEEK && !g.idle) seek_tip(w, &er[E_SEEK], x, frac_of(&er[E_SEEK], x) * g.dur); else tip_hide();
    } break;
    case WM_LBUTTONDOWN:
        if (e == E_SEEK && g.dur > 0) {
            g.drag = E_SEEK; SetCapture(w);
            g.dragPos = frac_of(&er[E_SEEK], x) * g.dur; seek_abs(g.dragPos, 0); g.lastSeekTick = GetTickCount();
            inval_w(w);
        } else if (e == E_VOL) {
            g.drag = E_VOL; SetCapture(w);
            g.volume = frac_of(&er[E_VOL], x) * 100; setp_d("volume", g.volume);
            if (g.muted) setp_f("mute", 0);
            inval_w(w);
        } else if (e) { g.pressed = e; SetCapture(w); inval_w(w); }
        break;
    case WM_LBUTTONUP:
        if (g.drag == E_SEEK) {
            g.dragPos = frac_of(&er[E_SEEK], x) * g.dur; seek_abs(g.dragPos, 1);
            g.pos = g.dragPos; g.drag = 0; ReleaseCapture(); inval_w(w);
        } else if (g.drag == E_VOL) { g.drag = 0; ReleaseCapture(); osd(L"소리: %d%%", (int)(g.volume + 0.5)); inval_w(w); }
        else if (g.pressed) {
            int p = g.pressed; g.pressed = 0; ReleaseCapture(); inval_w(w);
            if (p == e) elem_action(w, e, er);
        }
        break;
    case WM_MOUSELEAVE:
        if (!g.drag && g.hot) { g.hot = 0; inval_w(w); }
        tip_hide();
        break;
    }
}

/* =====================================================================
 * 재생목록 창
 * ===================================================================== */
static void on_drop(HDROP hd, int append);

static void dark_titlebar(HWND h)
{
    BOOL d = TRUE;
    if (FAILED(DwmSetWindowAttribute(h, 20, &d, sizeof d))) DwmSetWindowAttribute(h, 19, &d, sizeof d);
}

static LRESULT CALLBACK PlProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CREATE:
        g.pllist = CreateWindowExW(0, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NODATA | LBS_OWNERDRAWFIXED | LBS_NOTIFY | LBS_WANTKEYBOARDINPUT | LBS_NOINTEGRALHEIGHT,
            0, 0, 0, 0, h, (HMENU)100, g.inst, NULL);
        SetWindowTheme(g.pllist, L"DarkMode_Explorer", NULL);
        DragAcceptFiles(h, TRUE);
        return 0;
    case WM_SIZE: MoveWindow(g.pllist, 0, S(32), LOWORD(lp), HIWORD(lp) - S(32), TRUE); InvalidateRect(h, NULL, FALSE); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r);
        RECT hd = { 0, 0, r.right, S(32) }; fillr(dc, &hd, C_TITLE);
        hline(dc, 0, r.right, S(32) - 1, C_SEP);
        wchar_t t[64]; wsf(t, 64, L"재생목록  %d개", g.plCount);
        RECT a = hd; a.left += S(12); dtext(dc, g.fUI, t, &a, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT b = hd; b.right -= S(12); dtext(dc, g.fHint2, L"Enter 재생 · Del 삭제 · 우클릭 메뉴", &b, C_TEXT2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        EndPaint(h, &ps); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *mi = (MEASUREITEMSTRUCT *)lp;
        if (mi->CtlType == ODT_LISTBOX) { mi->itemHeight = S(26); return TRUE; }
        return menu_measure(mi);
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *d = (DRAWITEMSTRUCT *)lp;
        if (d->CtlType == ODT_LISTBOX) { pl_draw_item(d); return TRUE; }
        return menu_draw(d);
    }
    case WM_CTLCOLORLISTBOX: return (LRESULT)g.brList;
    case WM_COMMAND:
        if (LOWORD(wp) == 100 && HIWORD(wp) == LBN_DBLCLK) do_command(ID_PL_PLAY);
        return 0;
    case WM_VKEYTOITEM:
        switch (LOWORD(wp)) {
        case VK_RETURN: do_command(ID_PL_PLAY); return -2;
        case VK_DELETE: do_command(ID_PL_REMOVE); return -2;
        case VK_SPACE: case VK_F6: case VK_ESCAPE: handle_key(LOWORD(wp)); return -2;
        }
        return -1;
    case WM_CONTEXTMENU: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (p.x == -1 && p.y == -1) GetCursorPos(&p);
        POINT c = p; ScreenToClient(g.pllist, &c);
        LRESULT it = SendMessageW(g.pllist, LB_ITEMFROMPOINT, 0, MAKELPARAM(c.x, c.y));
        if (HIWORD(it) == 0) SendMessageW(g.pllist, LB_SETCURSEL, LOWORD(it), 0);
        int sel = (int)SendMessageW(g.pllist, LB_GETCURSEL, 0, 0);
        g_mn = 0; HMENU mm = CreatePopupMenu();
        madd(mm, ID_PL_PLAY, L"재생\tEnter", DIS(sel < 0));
        madd(mm, ID_PL_REMOVE, L"목록에서 삭제\tDel", DIS(sel < 0));
        msep(mm);
        madd(mm, ID_PL_ADDFILE, L"파일 추가...", 0);
        madd(mm, ID_PL_ADDFOLDER, L"폴더 추가...", 0);
        msep(mm);
        madd(mm, ID_PL_SORT, L"이름순 정렬", DIS(g.plCount < 2));
        madd(mm, ID_SHUFFLE, L"셔플(랜덤) 재생", CHK(g.shuffle));
        madd(mm, ID_REPEAT_ALL, L"전체 반복", CHK(g.repeat == 2));
        msep(mm);
        madd(mm, ID_PL_CLEAR, L"목록 비우기", DIS(g.plCount == 0));
        popup(mm, h, p.x, p.y, 0);
        return 0;
    }
    case WM_DROPFILES: on_drop((HDROP)wp, 1); return 0;
    case WM_CLOSE: ShowWindow(h, SW_HIDE); inval_ctrl(); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static void pl_toggle(void)
{
    if (!g.plwnd) {
        RECT wr; GetWindowRect(g.hwnd, &wr);
        int w = S(360), hgt = wr.bottom - wr.top;
        MONITORINFO mi = { sizeof mi }; GetMonitorInfoW(MonitorFromWindow(g.hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        int x = wr.right + S(4); if (x + w > mi.rcWork.right) x = wr.right - w - S(8);
        g.plwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"DaBaPL", L"재생목록", WS_OVERLAPPEDWINDOW & ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX),
                                  x, wr.top, w, hgt < S(300) ? S(300) : hgt, g.hwnd, NULL, g.inst, NULL);
        dark_titlebar(g.plwnd);
        pl_update();
    }
    if (IsWindowVisible(g.plwnd)) ShowWindow(g.plwnd, SW_HIDE);
    else {
        ShowWindow(g.plwnd, SW_SHOWNA);
        if (g.plCur >= 0) { SendMessageW(g.pllist, LB_SETCURSEL, g.plCur, 0); }
    }
    inval_ctrl();
}

/* =====================================================================
 * 드래그 앤 드롭
 * ===================================================================== */
static void on_drop(HDROP hd, int append)
{
    UINT n = DragQueryFileW(hd, 0xFFFFFFFF, NULL, 0);
    wchar_t **list = (wchar_t **)calloc(n ? n : 1, sizeof(wchar_t *));
    int m = 0, nsub = 0;
    wchar_t firstSub[MAX_PATH] = L"";
    for (UINT i = 0; i < n; i++) {
        UINT len = DragQueryFileW(hd, i, NULL, 0) + 1;
        wchar_t *b = (wchar_t *)malloc(len * sizeof(wchar_t));
        DragQueryFileW(hd, i, b, len);
        if (is_sub(b) && !g.idle) {
            char *u = u8(b); mcmd("sub-add", u, "select", NULL); free(u);
            if (!nsub) { wcsncpy(firstSub, base_name(b), MAX_PATH - 1); firstSub[MAX_PATH - 1] = 0; }
            nsub++; free(b);
        } else list[m++] = b;
    }
    DragFinish(hd);
    if (m) open_paths(list, m, append || GetKeyState(VK_SHIFT) < 0);
    free_list(list, m);
    if (nsub) osd(L"자막: %ls", firstSub);
    SetForegroundWindow(g.hwnd);
}

/* =====================================================================
 * 비디오 영역 창 / 전체화면 컨트롤바
 * ===================================================================== */
static LRESULT CALLBACK VideoProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_PAINT) { paint_video(h); return 0; }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, wp, lp);
}

static LRESULT CALLBACK BarProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r);
        HDC bb = get_bb(r.right, r.bottom); RECT er[E_COUNT]; get_elems(h, er);
        paint_ctrl(bb, &r, er);
        BitBlt(dc, 0, 0, r.right, r.bottom, bb, 0, 0, SRCCOPY);
        EndPaint(h, &ps); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE: case WM_LBUTTONUP: case WM_MOUSELEAVE:
        g.lastMove = GetTickCount();
        ui_mouse(h, m, GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        ui_mouse(h, WM_LBUTTONDOWN, GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_RBUTTONUP: { POINT p; GetCursorPos(&p); show_main_menu(p.x, p.y, TPM_BOTTOMALIGN); } return 0;
    case WM_MOUSEWHEEL: return SendMessageW(g.hwnd, m, wp, lp);
    case WM_CAPTURECHANGED: if ((HWND)lp != h) { g.drag = 0; g.pressed = 0; } return 0;
    case WM_MEASUREITEM: return menu_measure((MEASUREITEMSTRUCT *)lp);
    case WM_DRAWITEM: return menu_draw((DRAWITEMSTRUCT *)lp);
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* =====================================================================
 * 메인 창
 * ===================================================================== */
static UINT get_dpi_for(HWND h)
{
    static UINT (WINAPI *pGetDpiForWindow)(HWND); static int init;
    if (!init) { init = 1; pGetDpiForWindow = (UINT (WINAPI *)(HWND))(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"); }
    if (pGetDpiForWindow && h) return pGetDpiForWindow(h);
    HDC dc = GetDC(NULL); int d = GetDeviceCaps(dc, LOGPIXELSY); ReleaseDC(NULL, dc); return (UINT)d;
}

static int video_rect_hit(int x, int y)
{
    RECT t, v, c; get_layout(&t, &v, &c); POINT p = { x, y };
    return PtInRect(&v, p);
}

static void on_ui_timer(void)
{
    POINT pt; GetCursorPos(&pt);
    if (g.fs && g.fsbar && IsWindowVisible(g.fsbar) && !g.drag && !g.pressed) {
        RECT br; GetWindowRect(g.fsbar, &br); br.top -= S(24);
        if (!PtInRect(&br, pt)) fsbar_show(0);
    }
    int playing = !g.idle && !g.paused && !g.eof && g.hasVideo;
    if (g.optHideCursor && playing && !g.cursorHidden && GetTickCount() - g.lastMove > 2000 && GetForegroundWindow() == g.hwnd) {
        POINT c = pt; ScreenToClient(g.hwnd, &c);
        HWND under = WindowFromPoint(pt);
        if ((under == g.hwnd) && video_rect_hit(c.x, c.y) && !g.drag) { g.cursorHidden = 1; SetCursor(NULL); }
    }
    /* mpv 창이 포커스를 가져가면 되찾기 */
    if (GetForegroundWindow() == g.hwnd) {
        HWND f = GetFocus();
        if (f && f != g.hwnd && IsChild(g.video, f)) SetFocus(g.hwnd);
    }
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_NCCALCSIZE:
        if (wp) {
            if (!g.fs && IsZoomed(h)) {
                NCCALCSIZE_PARAMS *p = (NCCALCSIZE_PARAMS *)lp;
                MONITORINFO mi = { sizeof mi };
                GetMonitorInfoW(MonitorFromRect(&p->rgrc[0], MONITOR_DEFAULTTONEAREST), &mi);
                p->rgrc[0] = mi.rcWork;
            }
            return 0;
        }
        break;
    case WM_NCACTIVATE: return DefWindowProcW(h, m, wp, -1);
    case WM_NCHITTEST: {
        if (g.fs) return HTCLIENT;
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }; ScreenToClient(h, &p);
        RECT cr; GetClientRect(h, &cr);
        if (!IsZoomed(h)) {
            int b = S(6), l = p.x < b, r = p.x >= cr.right - b, t = p.y < b, bo = p.y >= cr.bottom - b;
            if (t && l) return HTTOPLEFT; if (t && r) return HTTOPRIGHT;
            if (bo && l) return HTBOTTOMLEFT; if (bo && r) return HTBOTTOMRIGHT;
            if (l) return HTLEFT; if (r) return HTRIGHT; if (t) return HTTOP; if (bo) return HTBOTTOM;
        }
        RECT er[E_COUNT]; get_elems(h, er);
        int e = hit_er(er, p.x, p.y);
        if (e == E_MAX) return HTMAXBUTTON;
        RECT t, v, c; get_layout(&t, &v, &c);
        if (PtInRect(&t, p) && e == E_NONE) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_NCMOUSEMOVE:
        if (wp == HTMAXBUTTON) {
            if (g.hot != E_MAX) { g.hot = E_MAX; inval_title(); }
            TRACKMOUSEEVENT tme = { sizeof tme, TME_LEAVE | TME_NONCLIENT, h, 0 }; TrackMouseEvent(&tme);
            return 0;
        }
        if (g.hot) { g.hot = 0; inval_title(); }
        break;
    case WM_NCMOUSELEAVE: if (g.hot == E_MAX) { g.hot = 0; inval_title(); } break;
    case WM_NCLBUTTONDOWN: if (wp == HTMAXBUTTON) { g.pressed = E_MAX; inval_title(); return 0; } break;
    case WM_NCLBUTTONUP:
        if (wp == HTMAXBUTTON) { int was = g.pressed == E_MAX; g.pressed = 0; if (was) ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE); inval_title(); return 0; }
        break;
    case WM_NCLBUTTONDBLCLK: if (wp == HTMAXBUTTON) return 0; break;
    case WM_GETMINMAXINFO: { MINMAXINFO *mm = (MINMAXINFO *)lp; mm->ptMinTrackSize.x = S(420); mm->ptMinTrackSize.y = S(240); } return 0;
    case WM_SIZE: relayout(); return 0;
    case WM_PAINT: paint_main(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DPICHANGED: {
        g.dpi = HIWORD(wp); make_fonts();
        RECT *r = (RECT *)lp;
        SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        if (g.pllist) SendMessageW(g.pllist, LB_SETITEMHEIGHT, 0, S(26));
        relayout(); return 0;
    }
    case WM_TIMER: if (wp == TIMER_UI) on_ui_timer(); return 0;
    case WM_MPV_WAKEUP: mpv_events(); return 0;
    case WM_DROPFILES: on_drop((HDROP)wp, 0); return 0;
    case WM_KEYDOWN: if (handle_key((UINT)wp)) return 0; break;
    case WM_SYSKEYDOWN: if (wp != VK_F4 && handle_key((UINT)wp)) return 0; break;
    case WM_SYSCHAR: return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU && lp == 0) return 0;
        if (((wp & 0xFFF0) == SC_SCREENSAVE || (wp & 0xFFF0) == SC_MONITORPOWER) && g.optNoSaver && !g.idle && !g.paused) return 0;
        break;
    case WM_APPCOMMAND:
        switch (GET_APPCOMMAND_LPARAM(lp)) {
        case APPCOMMAND_MEDIA_PLAY_PAUSE: do_command(ID_PLAYPAUSE); return TRUE;
        case APPCOMMAND_MEDIA_NEXTTRACK: do_command(ID_NEXT); return TRUE;
        case APPCOMMAND_MEDIA_PREVIOUSTRACK: do_command(ID_PREV); return TRUE;
        case APPCOMMAND_MEDIA_STOP: do_command(ID_STOP); return TRUE;
        }
        break;
    case WM_COMMAND: do_command(LOWORD(wp)); return 0;
    case WM_MEASUREITEM: if (menu_measure((MEASUREITEMSTRUCT *)lp)) return TRUE; break;
    case WM_DRAWITEM: if (menu_draw((DRAWITEMSTRUCT *)lp)) return TRUE; break;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && g.cursorHidden) { SetCursor(NULL); return TRUE; }
        break;
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        POINT sp; GetCursorPos(&sp);
        if (sp.x != g.lastMouse.x || sp.y != g.lastMouse.y) {
            g.lastMouse = sp; g.lastMove = GetTickCount();
            if (g.cursorHidden) { g.cursorHidden = 0; SetCursor(LoadCursor(NULL, IDC_ARROW)); }
            if (g.fs) { RECT cr; GetClientRect(h, &cr); if (y >= cr.bottom - S(CTRL_H)) fsbar_show(1); }
        }
        if (g.vdrag) {
            if (abs(x - g.vdragPt.x) > S(4) || abs(y - g.vdragPt.y) > S(4)) {
                g.vdrag = 0; ReleaseCapture();
                SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(sp.x, sp.y));
            }
            return 0;
        }
        ui_mouse(h, m, x, y);
        return 0;
    }
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        RECT er[E_COUNT]; get_elems(h, er);
        int e = hit_er(er, x, y);
        if (e) { ui_mouse(h, WM_LBUTTONDOWN, x, y); return 0; }
        if (video_rect_hit(x, y)) {
            if (m == WM_LBUTTONDBLCLK) { g.vdrag = 0; if (!g.idle) do_command(ID_FULLSCREEN); }
            else if (!g.fs) { g.vdrag = 1; g.vdragPt.x = x; g.vdragPt.y = y; SetCapture(h); }
            else if (g.idle) open_file_dlg(0);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (g.vdrag) { g.vdrag = 0; ReleaseCapture(); if (g.idle && g.plCount == 0) open_file_dlg(0); return 0; }
        ui_mouse(h, m, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSELEAVE: ui_mouse(h, m, 0, 0); return 0;
    case WM_CAPTURECHANGED: if ((HWND)lp != h) { g.drag = 0; g.pressed = 0; g.vdrag = 0; } return 0;
    case WM_RBUTTONUP: { POINT p; GetCursorPos(&p); show_main_menu(p.x, p.y, 0); } return 0;
    case WM_MBUTTONUP: do_command(ID_MUTE); return 0;
    case WM_XBUTTONUP: do_command(GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? ID_PREV : ID_NEXT); return TRUE;
    case WM_MOUSEWHEEL: {
        int d = GET_WHEEL_DELTA_WPARAM(wp);
        if (g.hot == E_SEEK && !g.idle) mcmdf("osd-msg seek %d", d > 0 ? 5 : -5);
        else set_volume(g.volume + (d > 0 ? 5 : -5));
        return 0;
    }
    case WM_CLOSE: {
        if (g.optResume && g.mpv && !g.idle && !g.eof) mcmd("write-watch-later-config", NULL, NULL, NULL);
        save_settings();
        WINDOWPLACEMENT wpl = { sizeof wpl };
        if (g.fs) wpl = g.fsPrev; else GetWindowPlacement(h, &wpl);
        ini_set_int(L"WinX", wpl.rcNormalPosition.left); ini_set_int(L"WinY", wpl.rcNormalPosition.top);
        ini_set_int(L"WinW", wpl.rcNormalPosition.right - wpl.rcNormalPosition.left);
        ini_set_int(L"WinH", wpl.rcNormalPosition.bottom - wpl.rcNormalPosition.top);
        ini_set_int(L"WinMax", wpl.showCmd == SW_SHOWMAXIMIZED);
        DestroyWindow(h);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(h, TIMER_UI);
        if (g.mpv) { mpv_handle *mp = g.mpv; g.mpv = NULL; f_mpv_terminate_destroy(mp); }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* =====================================================================
 * 다크 모드 메뉴 (uxtheme 비공개 API, Windows 10 1809+)
 * ===================================================================== */
static void enable_dark_menus(HWND h)
{
    typedef LONG (WINAPI *RtlGetVersion_t)(OSVERSIONINFOW *);
    RtlGetVersion_t rgv = (RtlGetVersion_t)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    OSVERSIONINFOW v = { sizeof v };
    if (!rgv || rgv(&v) != 0 || v.dwMajorVersion < 10 || v.dwBuildNumber < 17763) return;
    HMODULE ux = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!ux) return;
    typedef int (WINAPI *SetPreferredAppMode_t)(int);
    typedef BOOL (WINAPI *AllowDarkModeForWindow_t)(HWND, BOOL);
    typedef void (WINAPI *FlushMenuThemes_t)(void);
    SetPreferredAppMode_t spam = (SetPreferredAppMode_t)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    AllowDarkModeForWindow_t adw = (AllowDarkModeForWindow_t)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(133));
    FlushMenuThemes_t fmt = (FlushMenuThemes_t)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (spam) spam(2);      /* ForceDark */
    if (adw) adw(h, TRUE);
    if (fmt) fmt();
}

/* =====================================================================
 * 진입점
 * ===================================================================== */
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    (void)prev; (void)cmd;
    g.inst = inst; g.plCur = -1; g.idle = 1; g.abA = g.abB = -1; g.lastSec = -1;
    {   /* Per-Monitor V2 DPI (매니페스트가 없을 때 대비) */
        typedef BOOL (WINAPI *SPDAC)(HANDLE);
        SPDAC f = (SPDAC)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
        if (!f || !f((HANDLE)-4)) SetProcessDPIAware();
    }
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    GpStartupInput gsi = { 1, NULL, FALSE, FALSE };
    GdiplusStartup(&g.gdipToken, &gsi, NULL);
    srand(GetTickCount());

    ini_init_paths();
    load_settings();
    g.dpi = (int)get_dpi_for(NULL);
    g.mdc = CreateCompatibleDC(NULL);
    g.brMenu = CreateSolidBrush(C_MBG);
    g.brList = CreateSolidBrush(C_LISTBG);
    make_fonts();

    HICON icon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    WNDCLASSEXW wc = { sizeof wc };
    wc.style = CS_DBLCLKS; wc.lpfnWndProc = MainProc; wc.hInstance = inst; wc.hIcon = icon; wc.hIconSm = icon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); wc.lpszClassName = L"DaBaMain";
    RegisterClassExW(&wc);
    wc.lpfnWndProc = VideoProc; wc.lpszClassName = L"DaBaVideo"; wc.style = 0; RegisterClassExW(&wc);
    wc.lpfnWndProc = BarProc; wc.lpszClassName = L"DaBaBar"; wc.style = CS_DBLCLKS; RegisterClassExW(&wc);
    wc.lpfnWndProc = TipProc; wc.lpszClassName = L"DaBaTip"; wc.style = 0; RegisterClassExW(&wc);
    wc.lpfnWndProc = PlProc; wc.lpszClassName = L"DaBaPL"; wc.hbrBackground = g.brList; RegisterClassExW(&wc);

    /* 창 위치 복원 */
    int W = ini_int(L"WinW", 0), H = ini_int(L"WinH", 0), X = ini_int(L"WinX", 0), Y = ini_int(L"WinY", 0);
    RECT want = { X, Y, X + W, Y + H };
    if (W < 300 || H < 200 || !MonitorFromRect(&want, MONITOR_DEFAULTTONULL)) {
        MONITORINFO mi = { sizeof mi }; POINT o = { 0, 0 };
        GetMonitorInfoW(MonitorFromPoint(o, MONITOR_DEFAULTTOPRIMARY), &mi);
        W = S(960); H = S(600);
        X = (mi.rcWork.left + mi.rcWork.right - W) / 2; Y = (mi.rcWork.top + mi.rcWork.bottom - H) / 2;
    }
    g.hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, L"DaBaMain", APP_NAME, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             X, Y, W, H, NULL, NULL, inst, NULL);
    if (!g.hwnd) return 1;
    g.dpi = (int)get_dpi_for(g.hwnd); make_fonts();
    dark_titlebar(g.hwnd);
    { MARGINS mg = { 0, 0, 0, 1 }; DwmExtendFrameIntoClientArea(g.hwnd, &mg); }
    enable_dark_menus(g.hwnd);
    g.video = CreateWindowExW(0, L"DaBaVideo", NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                              0, 0, 10, 10, g.hwnd, NULL, inst, NULL);
    g.fsbar = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"DaBaBar", NULL, WS_POPUP, 0, 0, 10, 10, g.hwnd, NULL, inst, NULL);
    g.tip = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, L"DaBaTip", NULL, WS_POPUP,
                            0, 0, 10, 10, g.hwnd, NULL, inst, NULL);
    SetWindowPos(g.hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    relayout();

    if (!mpv_load_dll()) {
        MessageBoxW(g.hwnd, L"재생 엔진(libmpv-2.dll)을 찾을 수 없습니다.\n\n"
                    L"DaBa.exe 와 같은 폴더에 libmpv-2.dll 을 넣어 주세요.\n"
                    L"(https://github.com/shinchiro/mpv-winbuild-cmake/releases 의 mpv-dev-x86_64 파일)",
                    APP_NAME, MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!mpv_start()) { MessageBoxW(g.hwnd, L"재생 엔진을 초기화하지 못했습니다.", APP_NAME, MB_OK | MB_ICONERROR); return 1; }

    ShowWindow(g.hwnd, ini_int(L"WinMax", 0) ? SW_SHOWMAXIMIZED : (show == SW_SHOWMINIMIZED ? SW_SHOWNORMAL : show));
    UpdateWindow(g.hwnd);
    if (g.optTopmost) apply_topmost();
    SetTimer(g.hwnd, TIMER_UI, 250, NULL);

    int argc = 0; LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) open_paths(argv + 1, argc - 1, 0);
    if (argv) LocalFree(argv);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    GdiplusShutdown(g.gdipToken);
    CoUninitialize();
    return 0;
}
