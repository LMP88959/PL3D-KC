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

/*  xvid.c
 * 
 * All the X11 specific code for input/graphics/timing.
 * Also deals with things needed for macOS.
 * 
 */
#ifdef FW_OS_TYPE_X11

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <sys/sysctl.h>

#include <sys/time.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#if FW_X11_HAS_SHM_EXT
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

static struct {
    Display *display;
    int      screen;
    
    Window root_wnd;
    Window window;
    
    GC      gc;
    Visual *visual;
    XImage *ximage;
    
    int width;
    int height;
    int depth;
#if FW_X11_HAS_SHM_EXT
    XShmSegmentInfo shminfo;
#endif
    int use_shm;
} FWi_x = {
        NULL, /* display */
        0,    /* screen */
        
        None, /* root_wnd */
        None, /* window */
        
        None, /* gc */
        NULL, /* visual */
        NULL, /* ximage */
        
        320,  /* width */
        200,  /* height */
        32,   /* depth */
                        
#if FW_X11_HAS_SHM_EXT
        { 0, 0, 0, 0 }, /* shminfo */
#endif
        0,    /* use_shm */
};

#define FW_XEVENT_MASK \
        (KeyPressMask        | KeyReleaseMask    | \
         StructureNotifyMask | FocusChangeMask   | \
         ButtonPressMask     | ButtonReleaseMask | PointerMotionMask)

static void xkey_callb(XKeyEvent *event);

static void close_xdisplay(void);

static Atom WM_DELETE_WINDOW;

static VIDINFO FW_curinfo;

static struct {
    int width;
    int height;
    int scale;
} deviceinfo;

static int clk_hires_supported = 0;
static int clock_mode = FW_CLK_MODE_LORES;

static VOLATILE int xerrored = 0;

static int
handle_xerror(Display *display, XErrorEvent *error)
{
    FW_info("[xvid] XError: [%p] %d", display, error->type);
    xerrored = 1;
    return 0;
}

static void
xerror_install(void)
{
    xerrored = 0;
    XSetErrorHandler(handle_xerror);
}

static int
xerror_uninstall(void)
{
    if (FWi_x.display) {
        XSync(FWi_x.display, False);
    }
    XSetErrorHandler(NULL);
    return xerrored;
}

static int
readevent(XEvent *e)
{    
    switch (e->type) {
        case KeyPress:
        case KeyRelease:
            xkey_callb(&e->xkey);
            return 1;
        case FocusOut:
            pkb_reset();
            return 0;
        case MappingNotify:
            XRefreshKeyboardMapping(&e->xmapping);
            return 1;
        default:
            return 1;
    }
}

static int
open_xdisplay(void)
{
    XSetWindowAttributes setattr;
    XWindowAttributes    getattr;
    unsigned long rm, bm;
    XClassHint hint;
    XWMHints   wm_hints;
    int winmask;
    Display *d;

    /* display is already open? don't reopen */
    if (FWi_x.display != NULL) {
        return 0;
    }
    FWi_x.display = XOpenDisplay(NULL);
    FWi_x.screen  = 0;
    if (FWi_x.display == NULL) {
        return 0;
    }
    
    d = FWi_x.display;
    
    FWi_x.screen   = XDefaultScreen    (d);
    FWi_x.root_wnd = XDefaultRootWindow(d);
    
    setattr.event_mask       = FW_XEVENT_MASK;
    setattr.border_pixel     = BlackPixel(d, FWi_x.screen);
    setattr.background_pixel = setattr.border_pixel;
    
    winmask      = CWEventMask;
    FWi_x.window = XCreateWindow(
            d, FWi_x.root_wnd, 0, 0,
            320, 200, 0, CopyFromParent,
            InputOutput, CopyFromParent,
            winmask, &setattr);
    
    XGetWindowAttributes(d, FWi_x.window, &getattr);
    FWi_x.visual = getattr.visual;
    FWi_x.depth  = getattr.depth;
    if (FWi_x.depth != 24 && FWi_x.depth != 32) {
        FW_info("[xvid] true color display required");
        XCloseDisplay(d);
        return 0;
    }
    rm = FWi_x.visual->red_mask  & 0xffffff;
    bm = FWi_x.visual->blue_mask & 0xffffff;
    if ((rm == 0xff && bm == 0xff0000)) {
        FW_info("[xvid] detected bgr! only rgb supported, continuing anyways");
    }
    
    WM_DELETE_WINDOW = XInternAtom(d, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d, FWi_x.window, &WM_DELETE_WINDOW, 1);
    
    hint.res_name  = "jvfwLE";
    hint.res_class = "JvFWLE";
    XSetClassHint(d, FWi_x.window, &hint);
    
    wm_hints.flags         = InputHint | StateHint | WindowGroupHint;
    wm_hints.input         = True;
    wm_hints.initial_state = NormalState;
    wm_hints.window_group  = FWi_x.window;
    XSetWMHints(d, FWi_x.window, &wm_hints);
    FWi_x.gc = XCreateGC(d, FWi_x.window, 0, NULL);
    return 1;
}

static int
test_xlocal(void)
{
    int loc;
    char *name;
    
    loc = 0;
    name = XDisplayName(NULL);
    loc |= (name == NULL);
    if (name != NULL) {
        loc |= (name[0] == ':');
        loc |= (strncmp(name, "unix:", 5) == 0);
        loc |= (strstr (name, "xquartz:") != NULL); /* xquartz on mac */
    }
    return loc;
}

static int
test_xshm(Display *display)
{
#if FW_X11_HAS_SHM_EXT
    int  major;
    int  minor;
    Bool b;
#endif
    int n;
    
    if (XQueryExtension(display, "MIT-SHM", &n, &n, &n) == False) {
        return 0;
    }
#if FW_X11_HAS_SHM_EXT
    if (XShmQueryVersion(display, &major, &minor, &b) == False) {
        return 0;
    }
    return 1;
#else
    return 0;
#endif
}

#if FW_X11_IS_MACOS
static long
FWi_stol(register char *is, int *err)
{
    char *tailptr;
    long  val;
    int   i;
    
    if (is == NULL) {
        return 0;
    }
    while (isspace(*is)) { is++; }
    i = 0;
    while (!isspace(is[i])) { i++; }
    is[i] = '\0';
    errno = 0;
    *err  = 0;
    val   = strtol(is, &tailptr, 10);
    if (errno == ERANGE) {
        FW_info("[xvid] out of integer range");
        *err = 1;
    } else if (errno != 0) {
        FW_info("[xvid] bad string: %s", strerror(errno));
        *err = 1;
    } else if (*tailptr != '\0') {
        FW_info("[xvid] string contained non-numeric characters");
        *err = 1;
    }
    return val;
}
#endif

static long
find_shmmax(int *guess) {
    long  maxseg = (1 << 22); /* 4MB */
#if FW_X11_IS_MACOS
    FILE *f;
    char  buf[256];
    char *sub;
    long  size;
    int   err = 0;
    
    *guess = 1;
    f = popen("sysctl kern.sysv.shmmax", "r");
    while (fgets(buf, sizeof(buf), f) != NULL) {
    }
    pclose(f);
    sub = strchr(buf, ':');
    if (sub != NULL) {
        sub++; /* skip past colon */
        size = FWi_stol(sub, &err);
        if (!err) {
            maxseg = size;
            *guess = 0;
        }
    }
#endif
    return maxseg;
}

static int
create_xvidbuffer(int w, int h, int shm)
{
    XImage  *img = NULL;
    Display *d;
    Visual  *vis;
    int  dpth;
    long shmmx;
    int  shmsz;
    int  guess, success;
    GC   gc;
    
    guess = 0;
    success = 0;
    
    d    = FWi_x.display;
    vis  = FWi_x.visual;
    dpth = FWi_x.depth;
    
    if (d == NULL) {
        return 0;
    }
#if FW_X11_HAS_SHM_EXT
    FWi_x.use_shm = shm && test_xshm(d) && test_xlocal();
#else
    FWi_x.use_shm = test_xshm(d);
#endif
    
#if FW_X11_HAS_SHM_EXT
    if (FWi_x.use_shm) {
        shmmx = find_shmmax(&guess);
        xerror_install();
        FW_info("[xvid] max shared memory segment size [%s]: %ld",
                guess ? "guessed" : "actual", shmmx);
        img = XShmCreateImage(d, vis, dpth, ZPixmap, 0, &FWi_x.shminfo, w, h);
        if (xerror_uninstall() || img == NULL) {
            FW_info("[xvid] Error unable to create SHM image");
            goto shm_err_nullimg;
        }
        shmsz = img->bytes_per_line * img->height;
        if (shmsz > shmmx) {
            FW_info("[xvid] [WARNING] videobuf > than max shared mem segment:");
            FW_info("[xvid] %d > %ld", shmsz, shmmx);
        }
        FWi_x.shminfo.shmid = shmget(IPC_PRIVATE, shmsz, IPC_CREAT | 0777);
        if (FWi_x.shminfo.shmid < 0) {
            FW_info("[xvid] error shmget of size %d failed", shmsz);
            goto shm_err_failshmget;
        }
        
        img->data = shmat(FWi_x.shminfo.shmid, NULL, 0);
        FWi_x.shminfo.shmaddr = img->data;
        if (FWi_x.shminfo.shmaddr == (char*) -1) {
            FW_info("[xvid] error shmat failed");
            goto shm_err_failshmat;
        }
        
        FWi_x.shminfo.readOnly = False;
        xerror_install();
        success = XShmAttach(d, &FWi_x.shminfo);
        if (xerror_uninstall() || !success) {
            FW_info("[xvid] error attaching shared memory info to XDisplay");
            goto shm_err_failattach;
        }
        
        xerror_install();
        /* test SHM put */
        gc = XCreateGC(d, FWi_x.window, 0, NULL);
        XShmPutImage(d, FWi_x.window, gc, img, 0, 0, 0, 0, 1, 1, False);
        XSync(d, False);
        XFreeGC(d, gc);
        if (xerror_uninstall() || !success) {
            FW_info("[xvid] error testing SHM put onto XDisplay");
            goto shm_err_failattach; /* same goto as attach failure */
        }
        
        XSync(d, False);
        goto shm_success;
        
        shm_err_failattach: shmdt (FWi_x.shminfo.shmaddr);
        shm_err_failshmat:  shmctl(FWi_x.shminfo.shmid, IPC_RMID, 0);
        shm_err_failshmget: XDestroyImage(img); img = NULL;
        shm_err_nullimg:    FWi_x.use_shm = 0;
        /* marks shared mem for deletion after X server+client detach */
        shm_success:        shmctl(FWi_x.shminfo.shmid, IPC_RMID, 0);
        FW_info("[xvid] using X-Windows SHM extension: %d", FWi_x.use_shm);
    }
#endif
    
    if (img == NULL) {
        img = XCreateImage(d, vis, dpth, ZPixmap, 0, 0, w, h, XBitmapPad(d), 0);
        if (img != NULL) {
            img->data = malloc(img->bytes_per_line * img->height);
            if (img->data == NULL) {
                XDestroyImage(img);
                img = NULL;
            }
        }
    }
    FWi_x.ximage = img;
    return (img != NULL);
}

extern int
vid_open(char *title, int width, int height, int scale, int flags)
{
    XSizeHints hints = { 0 };
    Display *d;
    XEvent evt;
    int w, h, rw, rh, bpp;
    
    if (title == NULL) {
        title = "vFWLE";
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
    
    FW_curinfo.flags = flags;
    FW_curinfo.bytespp = 4; /* 4 bytes per pixel */
    
    bpp = FW_curinfo.bytespp; 

    FW_curinfo.width  = FW_BYTE_ALIGN(width , bpp);
    FW_curinfo.height = FW_BYTE_ALIGN(height, bpp);
    rw = FW_curinfo.width;
    rh = FW_curinfo.height;
    
    FW_curinfo.pitch = FW_CALC_PITCH(rw, bpp);
    
    deviceinfo.scale  = scale;
    deviceinfo.width  = FW_BYTE_ALIGN(rw * scale, bpp);
    deviceinfo.height = FW_BYTE_ALIGN(rh * scale, bpp);
    
    FWi_x.width  = deviceinfo.width;
    FWi_x.height = deviceinfo.height;
    w = FWi_x.width;
    h = FWi_x.height;
    
    FW_info("[xvid] creating X video context [%dx%d]", w, h);

    close_xdisplay();
    if (!open_xdisplay()) {
        FW_info("[xvid] couldn't create display or window");
        return FW_VERR_WINDOW;
    }
    
    d = FWi_x.display;
    
    XResizeWindow(d, FWi_x.window, w, h);
    
    hints.flags      = PMinSize | PMaxSize | PBaseSize;
    hints.min_width  = hints.max_width  = hints.base_width = w;
    hints.min_height = hints.max_height = hints.base_height = h;
    XSetWMNormalHints(d, FWi_x.window, &hints);
    
    XMapRaised(d, FWi_x.window);
    do {
        XMaskEvent(d, StructureNotifyMask, &evt);
    } while ((evt.type != MapNotify) || (evt.xmap.event != FWi_x.window));
    
    XStoreName  (d, FWi_x.window, title);
    XSetIconName(d, FWi_x.window, title);
    
    XSelectInput(d, FWi_x.window, FW_XEVENT_MASK);
    
    if (!create_xvidbuffer(w, h, flags & FW_VFLAG_VIDFAST)) {
        FW_info("[xvid] couldn't create video buffer");
        return FW_VERR_WINDOW;
    }
    
    if (FW_curinfo.video) {
        free(FW_curinfo.video);
    }
    FW_curinfo.video = calloc(rw * rh, 4);
    if (FW_curinfo.video == NULL) {
        return FW_VERR_NOMEM;
    }
  
    /* refresh newly created display */
    vid_blit();
    vid_sync();
    
    return FW_VERR_OK;
}

static void
resizevideo(unsigned *src, unsigned sw, unsigned sh,
            unsigned *dst, unsigned dw, unsigned dh)
{
    register unsigned *t, *p;
    int ratx, raty, acc;
    unsigned x, y;
        
    ratx = ((sw << 16) / dw) + 1;
    raty = ((sh << 16) / dh) + 1;
    for (y = 0; y < dh; y++) {
        t = dst + y * dw;
        p = src + ((y * raty) >> 16) * sw;
        acc = 0;
        for (x = 0; x < dw; x++) {
            *t++ = p[acc >> 16];
            acc += ratx;
        }
    }
}

extern void
vid_blit(void)
{
    Display *d;
    Window w;
    unsigned *v;
    unsigned sw, sh, dw, dh;
    
    v  = (unsigned *) FW_curinfo.video;
    d  = FWi_x.display;
    if ((v == NULL) || (d == NULL)) {
        return;
    }
    w  = FWi_x.window;
    sw = FW_curinfo.width;
    sh = FW_curinfo.height;
    dw = deviceinfo.width;
    dh = deviceinfo.height;

    if (deviceinfo.scale == 1) {
        memcpy(FWi_x.ximage->data, v, dw * dh * 4);
    } else {
        resizevideo(v, sw, sh, (unsigned *) FWi_x.ximage->data, dw, dh);
    }
    if (FWi_x.use_shm) {
#if FW_X11_HAS_SHM_EXT
        XShmPutImage(d, w, FWi_x.gc, FWi_x.ximage, 0, 0, 0, 0, dw, dh, False);
#endif
    } else {
        XPutImage(d, w, FWi_x.gc, FWi_x.ximage, 0, 0, 0, 0, dw, dh);
    }
}

extern void
vid_sync(void)
{
    if (FWi_x.display == NULL) {
        return;
    }
    XSync(FWi_x.display, False);
}

extern void
kbd_ignorerepeat(int ignore)
{
    if (FWi_x.display == NULL) {
        return;
    }
    if (ignore) {
        XAutoRepeatOff(FWi_x.display);
    } else {
        XAutoRepeatOn(FWi_x.display);
    }
}

extern int
wnd_osm_handle(void)
{
    XEvent e;
    int ret;
    
    if (FWi_x.display == NULL) {
        return 0;
    }
        
    ret = 0;
    while (XPending(FWi_x.display)) {
        memset(&e, 0, sizeof(XEvent));
        XNextEvent(FWi_x.display, &e);
        if (e.type == ClientMessage &&
            (Atom) e.xclient.data.l[0] == WM_DELETE_WINDOW) {
            sys_kill();
            return 0;
        }
        readevent(&e);
        ret = 1;
    }
    return ret;
}

static void
close_xdisplay(void)
{
    if (FWi_x.display == NULL) {
        return;
    }
    if (FWi_x.ximage != NULL) {
#if FW_X11_HAS_SHM_EXT
        if (FWi_x.use_shm) {
            XShmDetach(FWi_x.display, &FWi_x.shminfo);
            shmdt (FWi_x.shminfo.shmaddr);
            shmctl(FWi_x.shminfo.shmid, IPC_RMID, 0);
            XSync(FWi_x.display, False);
        }
#endif
        XDestroyImage(FWi_x.ximage);
        FWi_x.ximage = NULL;
    }

    XUnmapWindow(FWi_x.display, FWi_x.window);

    FWi_x.visual = 0;

    if (FWi_x.gc != None) {
        XFreeGC(FWi_x.display, FWi_x.gc);
        FWi_x.gc = None;
    }

    if (FWi_x.window != None) {
        XUnmapWindow  (FWi_x.display, FWi_x.window);
        XDestroyWindow(FWi_x.display, FWi_x.window);
        FWi_x.window = None;
    }

    if (FWi_x.window != None) {
        XDestroyWindow(FWi_x.display, FWi_x.window);
        FWi_x.window = None;
    }

    XAutoRepeatOn(FWi_x.display);

    XCloseDisplay(FWi_x.display);
    FWi_x.display = NULL;
}

extern void
wnd_term(void)
{
    close_xdisplay();
}

/* hardware specific clock functions */

#if FW_X11_IS_MACOS && (defined(__MACH__) || defined(__APPLE__))
#include <mach/mach_time.h>

static int
hardware_clk_query(void)
{
    mach_timebase_info_data_t info;
    
    return (mach_timebase_info(&info) == 0);
}

static utime
hardware_clk_ms(int *avail) /* return 0 in int* if not avail */
{
    static utime numer = 0;
    static utime denom = 0;

    if ((numer == 0) && (denom == 0)) {
        mach_timebase_info_data_t info;
        if (mach_timebase_info(&info)) {
            /* fallback */
            *avail = 0;
            return 0;
        }
        numer = info.numer;
        denom = info.denom;
    }
    *avail = 1;
    return (mach_absolute_time() * numer) / denom / 1000000;
}
#else
/* linux */

static int
hardware_clk_query(void)
{
    struct timespec ts;
    
    return (clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
}

static utime
hardware_clk_ms(int *avail) /* return 0 in int* if not avail */
{
    struct timespec ts;
    
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
        /* fallback */
        *avail = 0;
        return 0;
    }
    
    *avail = 1;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (utime) (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

extern void
clk_init(void)
{
    static int clk_init = 0;
    
    if (!clk_init) {
        clk_hires_supported = hardware_clk_query();
        clk_init = 1;
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
            FW_error("[xclk] invalid clock mode");
            break;
    }
}

extern utime
clk_sample(void)
{
    static int avail = 1;
    struct timeval tv;
    utime tm;

    if (clock_mode == FW_CLK_MODE_LORES) {
        return (time(NULL) * 1000);
    }
    
    /* first try hardware clock */
    if (avail) {        
        tm = hardware_clk_ms(&avail);
        if (!avail) {
            goto fbclk;
        }
        return tm;
    }

    /* fallback to C standard clock */
fbclk:
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
    
}

static void def_keyboard_func(int key) { (void) key; }
static void (*keyboard_func)(int key)   = def_keyboard_func;
static void (*keyboardup_func)(int key) = def_keyboard_func;

static struct {
   KeySym keysym;
   int key;
} xl8tab[] = {
   {XK_a, 'a'},
   {XK_b, 'b'},
   {XK_c, 'c'},
   {XK_d, 'd'},
   {XK_e, 'e'},
   {XK_f, 'f'},
   {XK_g, 'g'},
   {XK_h, 'h'},
   {XK_i, 'i'},
   {XK_j, 'j'},
   {XK_k, 'k'},
   {XK_l, 'l'},
   {XK_m, 'm'},
   {XK_n, 'n'},
   {XK_o, 'o'},
   {XK_p, 'p'},
   {XK_q, 'q'},
   {XK_r, 'r'},
   {XK_s, 's'},
   {XK_t, 't'},
   {XK_u, 'u'},
   {XK_v, 'v'},
   {XK_w, 'w'},
   {XK_x, 'x'},
   {XK_y, 'y'},
   {XK_z, 'z'},
   
   {XK_A, 'A'},
   {XK_B, 'B'},
   {XK_C, 'C'},
   {XK_D, 'D'},
   {XK_E, 'E'},
   {XK_F, 'F'},
   {XK_G, 'G'},
   {XK_H, 'H'},
   {XK_I, 'I'},
   {XK_J, 'J'},
   {XK_K, 'K'},
   {XK_L, 'L'},
   {XK_M, 'M'},
   {XK_N, 'N'},
   {XK_O, 'O'},
   {XK_P, 'P'},
   {XK_Q, 'Q'},
   {XK_R, 'R'},
   {XK_S, 'S'},
   {XK_T, 'T'},
   {XK_U, 'U'},
   {XK_V, 'V'},
   {XK_W, 'W'},
   {XK_X, 'X'},
   {XK_Y, 'Y'},
   {XK_Z, 'Z'},
   
   {XK_0, '0'},
   {XK_1, '1'},
   {XK_2, '2'},
   {XK_3, '3'},
   {XK_4, '4'},
   {XK_5, '5'},
   {XK_6, '6'},
   {XK_7, '7'},
   {XK_8, '8'},
   {XK_9, '9'},

   {XK_KP_0, '0'},
   {XK_KP_Insert, '0'},
   {XK_KP_1, '1'},
   {XK_KP_End, '1'},
   {XK_KP_2, '2'},
   {XK_KP_Down, '2'},
   {XK_KP_3, '3'},
   {XK_KP_Page_Down, '3'},
   {XK_KP_4, '4'},
   {XK_KP_Left, '4'},
   {XK_KP_5, '5'},
   {XK_KP_Begin, '5'},
   {XK_KP_6, '6'},
   {XK_KP_Right, '6'},
   {XK_KP_7, '7'},
   {XK_KP_Home, '7'},
   {XK_KP_8, '8'},
   {XK_KP_Up, '8'},
   {XK_KP_9, '9'},
   {XK_KP_Page_Up, '9'},
   {XK_KP_Delete, FW_KEY_BACKSPACE},
   {XK_KP_Decimal, FW_KEY_BACKSPACE},

   {XK_Escape, FW_KEY_ESCAPE},
   {XK_grave, '~'},
   {XK_minus, FW_KEY_MINUS},
   {XK_equal, FW_KEY_EQUALS},
   {XK_KP_Subtract, FW_KEY_MINUS},
   {XK_KP_Add, FW_KEY_PLUS},
   {XK_KP_Enter, FW_KEY_ENTER},
   {XK_KP_Equal, FW_KEY_EQUALS},
   {XK_KP_Divide, '/'},
   {XK_KP_Multiply, '*'},
   {XK_BackSpace, FW_KEY_BACKSPACE},
   {XK_Tab, FW_KEY_TAB},
   {XK_bracketleft, '['},
   {XK_bracketright, ']'},
   {XK_Return, FW_KEY_ENTER},
   {XK_semicolon, ':'},
   {XK_apostrophe, '\''},
   {XK_backslash, '\\'},
   {XK_less, '<'},
   {XK_comma, ','},
   {XK_period, '.'},
   {XK_slash, '/'},
   {XK_space, FW_KEY_SPACE},
   {XK_Delete, FW_KEY_BACKSPACE},
   {XK_Left, FW_KEY_ARROW_LEFT},
   {XK_Right, FW_KEY_ARROW_RIGHT},
   {XK_Up, FW_KEY_ARROW_UP},
   {XK_Down, FW_KEY_ARROW_DOWN},

   {XK_Shift_L, FW_KEY_SHIFT},
   {XK_Shift_R, FW_KEY_SHIFT},
   {XK_Control_L, FW_KEY_CONTROL},
   {XK_Control_R, FW_KEY_CONTROL}
};

static void
xkey_callb(XKeyEvent *event)
{
    int send = 0;
    int j, n = (sizeof(xl8tab) / sizeof(*xl8tab));
    KeySym sym;
    
    sym = XLookupKeysym(event, 0);
    for (j = 0; j < n; j++) {
        if (xl8tab[j].keysym == sym) {
            send = xl8tab[j].key;
            break;
        }
    }
    
    if (event->type == KeyPress) {
        keyboard_func(send);
    } else {
        keyboardup_func(send);
    }
}

extern void
sys_keybfunc(void (*keyboard)(int key))
{
    keyboard_func = keyboard;
}

extern void
sys_keybupfunc(void (*keyboard)(int key))
{
    keyboardup_func = keyboard;
}

extern int
kbd_vk2ascii(int vk)
{
    switch (vk) {
        case XK_Left:
            return FW_KEY_ARROW_LEFT;
        case XK_Up:
            return FW_KEY_ARROW_UP;
        case XK_Right:
            return FW_KEY_ARROW_RIGHT;
        case XK_Down:
            return FW_KEY_ARROW_DOWN;
        case XK_plus:
            return FW_KEY_PLUS;
        case XK_minus:
            return FW_KEY_MINUS;
        case XK_equal:
            return FW_KEY_EQUALS;
        case XK_Return:
            return FW_KEY_ENTER;
        case XK_space:
            return FW_KEY_SPACE;
        case XK_Tab:
            return FW_KEY_TAB;
        case XK_Escape:
            return FW_KEY_ESCAPE;
        case XK_Shift_L:
        case XK_Shift_R:
            return FW_KEY_SHIFT;
        case XK_Control_L:
        case XK_Control_R:
            return FW_KEY_CONTROL;
        case XK_BackSpace:
            return FW_KEY_BACKSPACE;
        default:
            break;
    }
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

