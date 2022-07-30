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

#ifndef FW_MAIN_H
#define FW_MAIN_H

/*  fw.h
 * 
 * Main file to include to use the FW framework.
 * 
 * This lite edition of FW has the following capabilities:
 *   - simple program loop
 *   - polled keyboard input
 *   - ability to create software video context on Win32/macOS(XQuartz)/Linux
 *      > GDI under Windows
 *      > X11 (with optional MIT-SHM image extension) under macOS/Linux
 *   - low and high resolution clock sampling
 *   
 */

/***********************************************************************/
#if defined(WIN32) || defined(_WIN32) || defined(_WIN32_)
/* windows */
#define FW_OS_TYPE_WINDOWS  1
#else
/* unix + X11 */
#define FW_OS_TYPE_X11      1

/* define as 1 if you want to compile with MIT-SHM support */
#ifndef FW_X11_HAS_SHM_EXT
#define FW_X11_HAS_SHM_EXT  1
#endif

/* define as 1 if running X11 under macOS with XQuartz */
#ifndef FW_X11_IS_MACOS
#ifdef __APPLE__
#define FW_X11_IS_MACOS     1
#else
#define FW_X11_IS_MACOS     0
#endif
#endif

#endif
/***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifdef VOLATILE
#undef VOLATILE
#endif
#define VOLATILE volatile

/***********************************************************************/

typedef struct {
    int *video;  /* pointer to video memory */
    int  width;  /* horizontal resolution */
    int  height; /* vertical resolution */
    int  pitch;  /* bytes per scanline */
    int  bytespp;/* bytes per pixel */
    int  flags;  /* any flags used when creating video context */
} VIDINFO;

/* system functions */
extern void sys_init (void); /* setup FW system (call this before anything) */
extern void sys_sethz(int hz); /* set loop rate */
extern void sys_updatefunc (void (*update)(void)); /* update callback */
extern void sys_displayfunc(void (*draw)(void));   /* display callback */
extern void sys_keybfunc   (void (*keyboard)(int key)); /* key down callback */
extern void sys_keybupfunc (void (*keyboard)(int key)); /* key up callback */
extern void sys_start   (void); /* start main loop */
extern void sys_shutdown(void); /* waits for loop to finish */
extern void sys_kill    (void); /* instantaneous shutdown */
extern int  sys_poll    (void); /* poll the operating system for events */
extern int  sys_getfps  (void); /* get current fps of system */
extern void sys_capfps  (int cap); /* limit fps to hz specified by sys_sethz */
/***********************************************************************/

#define FW_VFLAG_NONE      000
#define FW_VFLAG_VIDFAST   002 /* request the fastest graphics context */

#define FW_VERR_OK         0
#define FW_VERR_NOMEM      1
#define FW_VERR_WINDOW     2

/* video device functions */

/* open video context with given title, resolution, and scale */
extern int  vid_open(char *title, int width, int height, int scale, int flags);
extern void vid_blit       (void); /* draw image onto window */
extern void vid_sync       (void); /* sync (behavior is OS-dependent) */
extern VIDINFO *vid_getinfo(void); /* get current video info */

/***********************************************************************/

typedef unsigned int utime;

#define FW_CLK_MODE_LORES   0 /* low resolution clock (default) */
#define FW_CLK_MODE_HIRES   1 /* high resolution clock */

/* clock and timing functions */
extern void  clk_mode  (int mode); /* set clock mode */
extern utime clk_sample(void); /* sample the clock (milliseconds) */

/***********************************************************************/

/* keyboard input functions */
#define FW_KEY_ARROW_LEFT	0x25
#define FW_KEY_ARROW_UP		0x26
#define FW_KEY_ARROW_RIGHT	0x27
#define FW_KEY_ARROW_DOWN	0x28
#define FW_KEY_PLUS			'+'
#define FW_KEY_MINUS		'-'
#define FW_KEY_EQUALS		'='
#define FW_KEY_ENTER		0x0d
#define FW_KEY_SPACE		0x20
#define FW_KEY_TAB			0x09
#define FW_KEY_ESCAPE		0x1b
#define FW_KEY_SHIFT		0x10
#define FW_KEY_CONTROL		0x11
#define FW_KEY_BACKSPACE	0x08

extern int  kbd_vk2ascii    (int vk); /* virtual keycode to ascii */
extern void kbd_ignorerepeat(int ignore); /* ignore OS key repeat when held */
/***********************************************************************/

/* polled keyboard implementation */
extern void pkb_reset      (void); /* reset all keys */
extern void pkb_keyboard   (int key); /* key down callback for polled kb */
extern void pkb_keyboardup (int key); /* key up callback for polled kb */
extern int  pkb_key_pressed(int key); /* this tests if key was pressed */
extern int  pkb_key_held   (int key); /* this tests if key is being held */
/***********************************************************************/

extern void FW_info        (char *s, ...); /* info message */
extern void FW_error       (char *s, ...); /* error message (halts program) */

#ifdef __cplusplus
}
#endif

#endif
