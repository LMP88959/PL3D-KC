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

/*  sys.c
 * 
 * Contains main loop and sets up signal handlers.
 * 
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static int upd_rate   = 70;
static int upd_period = 14; /* 1000 / 70 */
static int cap_fps = 0;
static int fps = 0;
static int req_shutdown = 0;

static void def_func(void)        {}
static void (*update_func)(void)  = def_func;
static void (*display_func)(void) = def_func;

static void
signal_cb(int s)
{
    switch (s) {
        case SIGABRT:
            fprintf(stderr, "abort\n");
            break;
        case SIGILL:
            fprintf(stderr, "illegal operation\n");
            break;
        case SIGSEGV:
            fprintf(stderr, "segmentation fault\n");
            break;
        case SIGINT:
            fprintf(stderr, "interrupted\n");
            break;
        case SIGFPE:
            fprintf(stderr, "divide by zero most likely\n");
            break;
        case SIGTERM:
            fprintf(stderr, "termination signal\n");
            break;
#ifdef SIGQUIT
        case SIGQUIT:
            fprintf(stderr, "quit signal\n");
            break;
#endif
        default:
            fprintf(stderr, "unexpected signal %d\n", s);
            break;
    }
    wnd_term();
#ifdef FW_OS_TYPE_WINDOWS
	getchar();
#else
    raise(s);
#endif
    exit(1);
}

extern void
sys_sethz(int hz)
{
    if (hz < 1 || hz > 500) {
        hz = 70;
        return;
    }
    
    upd_rate   = hz;
    upd_period = 1000 / upd_rate;
}

extern void
sys_init(void)
{
    signal(SIGINT,  signal_cb);
    signal(SIGSEGV, signal_cb);
    signal(SIGFPE,  signal_cb);
    signal(SIGILL,  signal_cb);
    signal(SIGABRT, signal_cb);
    signal(SIGTERM, signal_cb);
#ifdef SIGQUIT
    signal(SIGQUIT, signal_cb);
#endif
    /* polled keyboard by default */
    sys_keybfunc(pkb_keyboard);
    sys_keybupfunc(pkb_keyboardup);
}

extern void
sys_updatefunc(void (*update)(void))
{
    if (update == NULL) {
        update_func = def_func;
        return;
    }
    update_func = update;
}

extern void
sys_displayfunc(void (*draw)(void))
{
    if (draw == NULL) {
        display_func = def_func;
        return;
    }
    display_func = draw;
}

extern void
sys_start(void)
{
    utime prvclk;
    utime curclk;
    utime nxtsec;
    int tfps, dt;

    FW_info("[sys] FW system starting");
    
    /* simple loop, doesn't play catch-up when fps is low */
    clk_init();
    
    tfps   = 0;
    prvclk = clk_sample();
    nxtsec = clk_sample() + 1000;
    req_shutdown = 0;
    while (!req_shutdown) {
        sys_poll();

        curclk = clk_sample();
        dt = curclk - prvclk;
        if (dt < upd_period) {
            if (!cap_fps) {
                display_func();
                tfps++;
            }
            continue;
        }
        prvclk = curclk;
        pkb_poll();
        update_func();        
        display_func();
        tfps++;
        if (clk_sample() >= nxtsec) {
            fps     = tfps;
            tfps    = 0;
            nxtsec += 1000;
        }
    }
    FW_info("[sys] FW system shutting down...");
    wnd_term();
    FW_info("[sys] FW system shut down successfully");
}

extern void
sys_shutdown(void)
{
    /* waits until end of game loop */
    req_shutdown = 1;
}

extern void
sys_kill(void)
{
    /* instantaneous */
    FW_info("[sys] FW system shutting down...");
    wnd_term();
    FW_info("[sys] FW system shut down successfully");
    exit(0);
}

extern int
sys_poll(void)
{
    return wnd_osm_handle();
}

extern void
sys_capfps(int cap)
{
    cap_fps = cap;
}

extern int
sys_getfps(void)
{
    return fps;
}

extern void
FW_info(char *s, ...)
{
    va_list lst;
    
    printf("[fw] INFO: ");
    va_start(lst, s);
    vprintf(s, lst);
    va_end(lst);
    printf("\n");
}

extern void
FW_error(char *s, ...)
{
    va_list lst;

    fprintf(stderr, "[fw] ERROR: ");
    va_start(lst, s);
    vfprintf(stderr, s, lst);
    va_end(lst);
    fprintf(stderr, "\n");
    
    wnd_term();
    getchar();
    exit(0);
}
