/*****************************************************************************/
/*
 * PiSHi LE (Lite edition) - Fundamentals of the King's Crook graphics engine.
 * 
 *   by EMMIR 2018-2022
 *   
 *   YouTube: https://www.youtube.com/c/LMP88
 *   
 * This software is released into the public domain.
 */
/*****************************************************************************/

#include "pl.h"

/*  clip.c
 * 
 * Code for defining and clipping polygons to the viewport and near plane.
 * 
 */

#include <string.h>

/* special return code specifying the edge was not clipped */
#define PL_NC    0777

#define HI_P     14     /* 2D clipping interpolation precision */
#define HH_P     (HI_P >> 1)

#define CLIP_P   8    /* 3D clipping interpolation precision */

int PL_vp_min_x;
int PL_vp_max_x;
int PL_vp_min_y;
int PL_vp_max_y;
int PL_vp_cen_x;
int PL_vp_cen_y;

extern void
PL_set_viewport(int minx, int miny, int maxx, int maxy, int update_center)
{
	if (minx < 0) { minx = 0; }
	if (miny < 0) { miny = 0; }
	if (maxx >= PL_hres) { maxx = PL_hres - 1; }
	if (maxy >= PL_vres) { maxy = PL_vres - 1; }
	PL_vp_min_x = minx;
	PL_vp_min_y = miny;
	PL_vp_max_x = maxx;
	PL_vp_max_y = maxy;
	if (update_center) {
	    PL_vp_cen_x = ((minx + maxx) >> 1) + 1;
	    PL_vp_cen_y = ((miny + maxy) >> 1) + 1;
	}
}

static void
doclip(int *L, int *R, int *out, int len, int bound, int comp, int ocomp)
{
    int i, f, fh, fhp;
    
    fhp = ((bound - L[comp]) << 15) / (R[comp] - L[comp]);
    fh = fhp >> (15 - HI_P);
    f = fh >> HH_P;
    /* skip x and y */
    for (i = 3; i < len; i++) {
        out[i] = L[i] + (f * ((R[i] - L[i]) >> HH_P));
    }
    /* z needs extra precision */
    out[2] = L[2] + (fhp * (R[2] - L[2]) >> 15);
    /* explicitly set comp and calculate ocomp with a higher precision */
    out[comp] = bound;
    out[ocomp] = L[ocomp] + (fh * (R[ocomp] - L[ocomp]) >> HI_P);
}

/* 2d line clip */
static int
lclip2(int **v0, int **v1, int len, int min, int max, int comp)
{
    static int resv[2 * PL_VDIM]; /* must be static */
    static int *m0 = resv + (0 * PL_VDIM);
    static int *m1 = resv + (1 * PL_VDIM);
    
    int ooo = 1; /* out of order */
    int ret = 0;
    int *L, *R;
    int **Lp, **Rp;

    Lp = v1;
    Rp = v0;

    if ((*Rp)[comp] < (*Lp)[comp]) {
	    ooo = 0;
	    Lp  = v0;
		Rp  = v1;
	}

    L = *Lp;
    R = *Rp;
    
	if ((L[comp] >= max) || (R[comp] <= min)) {
		return PL_NC;
	}

    if (L[comp] <= min) {
        ret = !ooo;
        doclip(L, R, m0, len, min, comp, !comp);
        *Lp = m0;
    }

    if (R[comp] >= max) {
        ret |= ooo;
        doclip(L, R, m1, len, max, comp, !comp);
        *Rp = m1;
    }
	return ret;
}

static int
lclip3(int **v0, int **v1, int len)
{
    /* must be static, the memory is used after function execution */
    static int m[PL_VDIM];
    
    int i, f;
    int ooo = 1; /* out of order */
    int ret = 0;
    int *L, *R;
    int **Lp, **Rp;

    Lp = v1;
    Rp = v0;
    if ((*Rp)[2] < (*Lp)[2]) {
        ooo = 0;
        Lp  = v0;
        Rp  = v1;
    }
    
    L = *Lp;
    R = *Rp;
    
    if (R[2] < PL_Z_NEAR_PLANE) {
        return PL_NC;
    }
    if (L[2] < PL_Z_NEAR_PLANE) {
        ret = !ooo;
        *Lp = m;
        f    = ((PL_Z_NEAR_PLANE - L[2]) << CLIP_P) / (R[2] - L[2]);
        m[0] = L[0] + (f * (R[0] - L[0]) >> CLIP_P);
        m[1] = L[1] + (f * (R[1] - L[1]) >> CLIP_P);
        m[2] = PL_Z_NEAR_PLANE;
        for (i = 3; i < len; i++) {
            m[i] = L[i] + (f * (R[i] - L[i]) >> CLIP_P);
        }
    }
    return ret;
}

/* polygon clip */
static int
pclip(int *dst, int *src, int len, int num,
      int (*clip)(int **v0, int **v1, int len, int min, int max),
      int minv, int maxv)
{
    int nverts;
    int *out;
    int r, nbytes;
    int *v[2];
    
    nverts = 0;
    out    = dst;
    nbytes = len * sizeof(int);
    
    while (num--) {
        v[1] = src;
        v[0] = src += len;
        r = clip(&v[1], &v[0], len, minv, maxv);
        if (r != PL_NC) {
            do {
                memcpy(out, v[r], nbytes);
                out += len;
                nverts++;
            } while (r--);
        }
    }
    memcpy(out, dst, nbytes);
    return nverts;
}

static int
lineclipx(int **v0, int **v1, int len, int min, int max)
{
    return lclip2(v0, v1, len, min, max, 0);
}

static int
lineclipy(int **v0, int **v1, int len, int min, int max)
{
    return lclip2(v0, v1, len, min, max, 1);
}

static int
lineclipnz(int **v0, int **v1, int len, int min, int max)
{
    (void) min;
    (void) max;
    return lclip3(v0, v1, len);
}

extern int
PL_clip_line_x(int **v0, int **v1, int len, int min, int max)
{
    return lclip2(v0, v1, len, min, max, 0) != PL_NC;
}

extern int
PL_clip_line_y(int **v0, int **v1, int len, int min, int max)
{
    return lclip2(v0, v1, len, min, max, 1) != PL_NC;
}

extern int
PL_clip_poly_x(int *dst, int *src, int len, int num)
{
    return pclip(dst, src, len, num, lineclipx, PL_vp_min_x, PL_vp_max_x);
}

extern int
PL_clip_poly_y(int *dst, int *src, int len, int num)
{
    return pclip(dst, src, len, num, lineclipy, PL_vp_min_y, PL_vp_max_y);
}

extern int
PL_clip_poly_nz(int *dst, int *src, int len, int num)
{
    return pclip(dst, src, len, num, lineclipnz, 0, 0);
}

extern int
PL_point_frustum_test(int *v)
{
    if (v[2] <= PL_Z_NEAR_PLANE) {
        return PL_Z_OUTC_OUTSIDE;
    }
    return PL_Z_OUTC_IN_VIEW;
}

extern int
PL_frustum_test(int minz, int maxz)
{
    int outc;
    
    if (maxz <= PL_Z_NEAR_PLANE) {
        return PL_Z_OUTC_OUTSIDE;
    }
    outc = PL_Z_OUTC_IN_VIEW;
    if (minz < PL_Z_NEAR_PLANE) {
        outc |= PL_Z_OUTC_PART_NZ;
    }
    return outc;
}
