/*****************************************************************************/
/*
 * FW LE (Lite edition) - Fundamentals of the King's Crook graphics engine.
 * 
 *   by EMMIR 2018-2022
 *   
 *   YouTube: https://www.youtube.com/c/LMP88
 *   
 * This software is released into the public domain.
 */
/*****************************************************************************/

#include "fw_priv.h"

/*  wvid.c
 * 
 * All the Windows specific code for input/graphics/timing.
 * 
 */

#ifdef FW_OS_TYPE_WINDOWS
/* at least they let you prune it... */
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
#define NOCRYPT
#define NOMENUS
#define NODRAWTEXT
#define NOMEMMGR
#define NOCOMM
#define NOKANJI
#define NONLS
#define NOKERNEL
#define OEMRESOURCE
#undef UNICODE
#include <windows.h>
#include <mmsystem.h>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <ctype.h>

#define FW_CLASSNAME "FWLE"

static char *
FWi_strdup(char *src)
{
	size_t len;
	char *dst;
	len = strlen(src);
	dst = calloc(len + 1, 1);
	if (!dst) {
	    return NULL;
	}
	if (len > 0) {
		memcpy(dst, src, len);
		dst[len] = '\0';
	}
	return dst;
}

static long long qpc_freq  = 0;
static long long qpc_start = 0;
static DWORD tgt_start     = 0; /* timeGetTime */
static int clk_hires_supported = 0;
static int clock_mode = FW_CLK_MODE_LORES;

static HWND FWi_wnd;
static HANDLE FWi_inst;

static VIDINFO FW_curinfo;

static struct {
	int width;
	int height;
	int scale;
} deviceinfo;

static HBITMAP FWi_hbmp;

static int FWi_ignorekeyrepeat = 0;

static void def_keyboard_func(int key) {}
static void(*FWi_keyboard_func)(int key)   = def_keyboard_func;
static void(*FWi_keyboardup_func)(int key) = def_keyboard_func;

#define REPEAT_BIT (1 << 30)

static int
readevent(MSG *msg)
{
	unsigned int mask = (unsigned int) msg->wParam;
	int type = 0;
	switch (msg->message) {
		case WM_KEYUP:
			FWi_keyboardup_func(kbd_vk2ascii((int) msg->wParam));
			return 1;
		case WM_KEYDOWN:
			if (FWi_ignorekeyrepeat && (msg->lParam & REPEAT_BIT)) {
				return 0;
			}
			FWi_keyboard_func(kbd_vk2ascii((int) msg->wParam));
			return 1;
		default:
			return 0;
	}
}

static LRESULT CALLBACK
os_message_handler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_PAINT:
			break;
		case WM_ERASEBKGND:
			return 1;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_SYSKEYUP:
			break;
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

extern int
vid_open(char *title, int width, int height, int scale, int flags)
{
	static LPCSTR default_title = "vFWLE";
    static int cret = 0;
	WNDCLASSEX w = { 0 };
	HDC hdc;
	int binfo_size;
	BITMAPINFO *binfo;
	LPCSTR sn;
	DWORD style;
    int wnd_w, wnd_h, bpp;
    int rw, rh;
    int dw, dh;
    RECT r;
    
	if (title == NULL) {
		title = default_title;
	}
	if (width < 1) {
		width = 1;
	}
	if (height < 1) {
		height = 1;
	}
	if (scale < 1) {
		scale = 1;
	}
	
	sn = FWi_strdup(title);
	/* Delete previous device if there was one */
	if (FWi_hbmp) {
		DeleteObject(FWi_hbmp);
	}
	FW_curinfo.flags = flags;
	FW_curinfo.bytespp = 4;
	bpp = FW_curinfo.bytespp;
	FW_curinfo.width = FW_BYTE_ALIGN(width, bpp);
	FW_curinfo.height = FW_BYTE_ALIGN(height, bpp);
	rw = FW_curinfo.width;
	rh = FW_curinfo.height;
	FW_curinfo.pitch = FW_CALC_PITCH(rw, bpp);

	deviceinfo.scale = scale;

	deviceinfo.width = FW_BYTE_ALIGN(rw * scale, bpp);
	deviceinfo.height = FW_BYTE_ALIGN(rh * scale, bpp);
	dw = deviceinfo.width;
	dh = deviceinfo.height;
	style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	if (!cret) {
	    SetProcessDPIAware();
		w.cbSize = sizeof(w);
		w.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW |
		        CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW;
		w.lpfnWndProc = os_message_handler;
		FWi_inst = GetModuleHandle(NULL);
		w.hInstance = GetModuleHandle(NULL);
		w.hIcon = LoadIcon(w.hInstance, IDI_APPLICATION);
		w.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
		w.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
		w.lpszMenuName = NULL;
		w.lpszClassName = FW_CLASSNAME;

		if (!RegisterClassEx(&w)) {
			FW_error("[wvid] could not register class");
			return FW_VERR_WINDOW;
		}

		FWi_wnd = CreateWindowEx(0, FW_CLASSNAME, FW_CLASSNAME, style,
		        0, 0, 320, 200, NULL, NULL, FWi_inst, NULL);
		cret = 1;
	}
	
	r.left = 0;
	r.top = 0;
	r.right = dw;
	r.bottom = dh;
	AdjustWindowRectExForDpi(&r, style, 0, 0, GetDpiForWindow(FWi_wnd));
	wnd_w = r.right - r.left;
	wnd_h = r.bottom - r.top;
	SetWindowPos(FWi_wnd, HWND_TOP, 0, 0, wnd_w, wnd_h,
	                SWP_NOMOVE | SWP_NOOWNERZORDER |
	                SWP_NOZORDER | SWP_SHOWWINDOW);

	binfo_size = sizeof(*binfo);

	binfo = calloc(1, binfo_size);
	if (!binfo) {
		FW_error("[wvid] out of memory");
		return FW_VERR_NOMEM;
	}
	memset(&binfo->bmiHeader, 0, sizeof(BITMAPINFOHEADER));
	binfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	binfo->bmiHeader.biWidth = rw;
	binfo->bmiHeader.biHeight = -rh;
	binfo->bmiHeader.biPlanes = 1;
	binfo->bmiHeader.biSizeImage = FW_curinfo.pitch * rh;
	binfo->bmiHeader.biXPelsPerMeter = 0;
	binfo->bmiHeader.biYPelsPerMeter = 0;
	binfo->bmiHeader.biClrUsed = 0;
	binfo->bmiHeader.biClrImportant = 0;
	binfo->bmiHeader.biBitCount = bpp * 8;

	hdc = GetDC(FWi_wnd);
	FWi_hbmp = CreateDIBSection(hdc, binfo, DIB_RGB_COLORS,
	        (void **) (&FW_curinfo.video), NULL, 0);

	ReleaseDC(FWi_wnd, hdc);
	free(binfo);
	binfo = NULL;
	if (FWi_hbmp == NULL) {
		FW_error("[wvid] unable to create display bitmap");
		return FW_VERR_WINDOW;
	}

	SetWindowText(FWi_wnd, sn);
	ShowWindow(FWi_wnd, SW_SHOW);
	UpdateWindow(FWi_wnd);
	SetForegroundWindow(FWi_wnd);
	SetFocus(FWi_wnd);

	SetThreadAffinityMask(GetCurrentThread(), 1);

	/* refresh newly created display */
	vid_blit();
	vid_sync();
	return FW_VERR_OK;
}

extern void
vid_blit(void)
{
    HDC hdc, mdc;
    int sw, sh, dw, dh;
	if (!FW_curinfo.video) {
		return;
	}
	hdc = GetDC(FWi_wnd);
	mdc = CreateCompatibleDC(hdc);
	SelectObject(mdc, FWi_hbmp);
	sw = FW_curinfo.width;
	sh = FW_curinfo.height;
	dw = deviceinfo.width;
	dh = deviceinfo.height;
	switch (deviceinfo.scale) {
		case 1:
			BitBlt(hdc, 0, 0, dw, dh, mdc, 0, 0, SRCCOPY);
			break;
		default:
			StretchBlt(hdc, 0, 0, dw, dh, mdc, 0, 0, sw, sh, SRCCOPY);
			break;
	}
	DeleteDC(mdc);
	ReleaseDC(FWi_wnd, hdc);
}

extern void
vid_sync(void)
{
	GdiFlush();
}

extern void
kbd_ignorerepeat(int ignore)
{
	FWi_ignorekeyrepeat = ignore;
}

extern void
wkeycb(void(*keyboard)(int key))
{
	FWi_keyboard_func = keyboard;
}

extern void
wkeyupcb(void(*keyboard)(int key))
{
	FWi_keyboardup_func = keyboard;
}

extern int
wnd_osm_handle(void)
{
	MSG msg;

	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
		    sys_kill();
			return 0;
		}
		if (readevent(&msg)) {
			return 1;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

extern void
wnd_term(void)
{
    WNDCLASS w;

	DeleteObject(FWi_hbmp);

	DestroyWindow(FWi_wnd);

	if (GetClassInfo(FWi_inst, FW_CLASSNAME, &w)) {
		UnregisterClass(FW_CLASSNAME, FWi_inst);
	}

	PostMessage(FWi_wnd, WM_CLOSE, 0, 0);
}

extern void
clk_init(void)
{
    static int clk_init = 0;
    
    if (!clk_init) {
        LARGE_INTEGER query;
        if (QueryPerformanceFrequency(&query)) {
            qpc_freq = query.QuadPart;
            if (qpc_freq == 0) {
                qpc_freq = 1;
            }
            QueryPerformanceCounter(&query);
            qpc_start = query.QuadPart;

            clk_hires_supported = 1;
        }
    
        timeBeginPeriod(1);
        tgt_start = timeGetTime();
        clk_init  = 1;
    }
}

extern void
clk_mode(int mode)
{
    clk_init();
    if (!clk_hires_supported) {
        clock_mode = FW_CLK_MODE_LORES;
        return;
    }
    switch (mode) {
        case FW_CLK_MODE_LORES:
        case FW_CLK_MODE_HIRES:
            clock_mode = mode;
            break;
        default:
            FW_error("[wclk] invalid clock mode");
            break;
    }
}

extern utime
clk_sample(void)
{
    LARGE_INTEGER li;
    
    if (clock_mode == FW_CLK_MODE_LORES) {
        return timeGetTime() - tgt_start;
    }
    QueryPerformanceCounter(&li);
    return (utime) ((1000 * (li.QuadPart - qpc_start)) / qpc_freq);
}

extern void
sys_keybfunc(void (*keyboard)(int key))
{
    FWi_keyboard_func = keyboard;
}

extern void
sys_keybupfunc(void (*keyboard)(int key))
{
    FWi_keyboardup_func = keyboard;
}

extern int
kbd_vk2ascii(int vk)
{
    if (vk >= 'A' && vk <= 'Z') {
        return vk + 32;
    }
    if (vk >= 0x6a && vk < 0x70) {
        return vk - 64;
    }
    if (vk >= 0x60 && vk < 0x6a) {
        return vk - 48;
    }
    switch (vk) {
        case 0xdb:
            return '[';
        case 0xdd:
            return ']';
        case 0xba:
            return ';';
        case 0xbb:
            return '=';
        case 0xbc:
            return ',';
        case 0xbd:
            return '-';
        case 0xbe:
            return '.';
        case 0xbf:
            return '/';
    }
    return vk;
}

extern VIDINFO *
vid_getinfo(void)
{
    return &FW_curinfo;
}

#endif
