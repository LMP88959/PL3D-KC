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

/*  math.c
 * 
 * Integer-only math using fixed point numbers.
 * Implements a basic matrix stack for transformations, among other things.
 * 
 */

#include <string.h>

int PL_sin[PL_TRIGMAX] = {
	0x0000,0x0324,0x0647,0x096a,0x0c8b,0x0fab,0x12c8,0x15e2,
	0x18f8,0x1c0b,0x1f19,0x2223,0x2528,0x2826,0x2b1f,0x2e11,
	0x30fb,0x33de,0x36ba,0x398c,0x3c56,0x3f17,0x41ce,0x447a,
	0x471c,0x49b4,0x4c3f,0x4ebf,0x5133,0x539b,0x55f5,0x5842,
	0x5a82,0x5cb4,0x5ed7,0x60ec,0x62f2,0x64e8,0x66cf,0x68a6,
	0x6a6d,0x6c24,0x6dca,0x6f5f,0x70e2,0x7255,0x73b5,0x7504,
	0x7641,0x776c,0x7884,0x798a,0x7a7d,0x7b5d,0x7c29,0x7ce3,
	0x7d8a,0x7e1d,0x7e9d,0x7f09,0x7f62,0x7fa7,0x7fd8,0x7ff6,
	0x8000,0x7ff6,0x7fd8,0x7fa7,0x7f62,0x7f09,0x7e9d,0x7e1d,
	0x7d8a,0x7ce3,0x7c29,0x7b5d,0x7a7d,0x798a,0x7884,0x776c,
	0x7641,0x7504,0x73b5,0x7255,0x70e2,0x6f5f,0x6dca,0x6c24,
	0x6a6d,0x68a6,0x66cf,0x64e8,0x62f2,0x60ec,0x5ed7,0x5cb4,
	0x5a82,0x5842,0x55f5,0x539b,0x5133,0x4ebf,0x4c3f,0x49b4,
	0x471c,0x447a,0x41ce,0x3f17,0x3c56,0x398c,0x36ba,0x33de,
	0x30fb,0x2e11,0x2b1f,0x2826,0x2528,0x2223,0x1f19,0x1c0b,
	0x18f8,0x15e2,0x12c8,0x0fab,0x0c8b,0x096a,0x0647,0x0324
};

int PL_cos[PL_TRIGMAX];

static struct {
	int tx, ty, tz;
	unsigned int rx, ry;
} xf_vw; /* view transform */

static int mat_idt[16] = PL_IDT_MAT;
static int mat_model[16] = PL_IDT_MAT;
static int mst_stack[PL_MAX_MST_DEPTH * 16];
static int mst_top = 0;

extern void
PL_set_camera(int x, int y, int z, int rx, int ry)
{
	xf_vw.tx = -x;
	xf_vw.ty = -y;
	xf_vw.tz = -z;
	
    xf_vw.rx = (unsigned int) (PL_TRIGMAX - rx & PL_TRIGMSK);
    xf_vw.ry = (unsigned int) (PL_TRIGMAX - ry & PL_TRIGMSK);
}

extern void
PL_mst_get(int *out)
{
	PL_mat_cpy(out, mat_model);
}

extern void
PL_mst_push(void)
{
	if ((mst_top + 1) >= PL_MAX_MST_DEPTH) {
	    EXT_error(PL_ERR_MISC, "math", "stack overflow");
	}
	PL_mat_cpy(&mst_stack[(mst_top + 1) * 16], mat_model);
	mst_top++;
}

extern void
PL_mst_pop(void)
{
	if ((mst_top - 1) < 0) {
	    EXT_error(PL_ERR_MISC, "math", "stack underflow");
	}
	PL_mat_cpy(mat_model, &mst_stack[(mst_top--) * 16]);
}

extern void
PL_mst_load_idt(void)
{	
	PL_mat_cpy(mat_model, mat_idt);
}

extern void
PL_mst_load(int *m)
{
	PL_mat_cpy(mat_model, m);
}

extern void
PL_mst_mul(int *m)
{
	PL_mat_mul(mat_model, m);
}

extern void
PL_mst_scale(int x, int y, int z)
{
	int mat[16];
	
	memcpy(mat, mat_idt, sizeof(mat));
	
	mat[0]  = x;
	mat[5]  = y;
	mat[10] = z;
	PL_mst_mul(mat);
}

extern void
PL_mst_translate(int x, int y, int z)
{
    int mat[16];
    
    memcpy(mat, mat_idt, sizeof(mat));
    
	mat[12] = x;
	mat[13] = y;
	mat[14] = z;
	PL_mst_mul(mat);
}

#define _M_(x, y) (((x) * (y)) >> PL_P)
#define _MI_(i, j) (((i) << 2) + (j))

extern void
PL_mst_rotatex(int rx)
{
    int cx, sx;
    int mat[16];
      
    memcpy(mat, mat_idt, sizeof(mat));
    
    cx = PL_cos[rx & PL_TRIGMSK];
    sx = PL_sin[rx & PL_TRIGMSK];

    mat[_MI_(1, 1)] = cx;
    mat[_MI_(2, 1)] = -sx;
    mat[_MI_(1, 2)] = sx;
    mat[_MI_(2, 2)] = cx;
    
    PL_mst_mul(mat);
}

extern void
PL_mst_rotatey(int ry)
{
    int cy, sy;
    int mat[16];
      
    memcpy(mat, mat_idt, sizeof(mat));

    cy = PL_cos[ry & PL_TRIGMSK];
    sy = PL_sin[ry & PL_TRIGMSK];
    
    mat[_MI_(0, 0)] = cy;
    mat[_MI_(2, 0)] = sy;
    mat[_MI_(0, 2)] = -sy;
    mat[_MI_(2, 2)] = cy;
    
    PL_mst_mul(mat);
}

extern void
PL_mst_rotatez(int rz)
{
    int cz, sz;
    int mat[16];
    
    memcpy(mat, mat_idt, sizeof(mat));
      
    cz = PL_cos[rz & PL_TRIGMSK];
    sz = PL_sin[rz & PL_TRIGMSK];
    
    mat[_MI_(0, 0)] = cz;
    mat[_MI_(1, 0)] = sz;
    mat[_MI_(0, 1)] = -sz;
    mat[_MI_(1, 1)] = cz;
    
    PL_mst_mul(mat);
}

#undef _MI_

extern void
PL_mst_xf_modelview_vec(int *v, int *out, int len)
{
    register int sx, sy, cx, cy;
    register int x, y, z, w;
    int xx, yy, zz;
    int tx, ty, tz;
    int *m;

    cx = PL_cos[xf_vw.rx];
    sx = PL_sin[xf_vw.rx];
    cy = PL_cos[xf_vw.ry];
    sy = PL_sin[xf_vw.ry];

    m = mat_model;
    
	tx = xf_vw.tx + m[12];
	ty = xf_vw.ty + m[13];
	tz = xf_vw.tz + m[14];

    while ((len--) > 0) {
        x = v[0];
        y = v[1];
        z = v[2];

        xx = ((x * m[0] + y * m[4] + z * m[8])  >> PL_P) + tx;
        yy = ((x * m[1] + y * m[5] + z * m[9])  >> PL_P) + ty;
        zz = ((x * m[2] + y * m[6] + z * m[10]) >> PL_P) + tz;

        /* yaw */
        w  = (zz * sy + xx * cy) >> PL_P;
        zz = (zz * cy - xx * sy) >> PL_P;
        xx = w;

        /* pitch */
        w  = (yy * cx - zz * sx) >> PL_P;
        zz = (yy * sx + zz * cx) >> PL_P;
        yy = w;

        out[0] = xx;
        out[1] = yy;
        out[2] = zz;
        v   += PL_VLEN;
        out += PL_VLEN;
    }

}

extern void
PL_mat_mul(int *a, int *b)
{
	int m[16];
	
	memcpy(m, a, sizeof(m));
	
	a[0] =  _M_(b[0], m[0])  + _M_(b[1], m[4]) +
	        _M_(b[2], m[8])  + _M_(b[3], m[12]);
	a[1] =  _M_(b[0], m[1])  + _M_(b[1], m[5]) +
	        _M_(b[2], m[9])  + _M_(b[3], m[13]);
	a[2] =  _M_(b[0], m[2])  + _M_(b[1], m[6]) +
	        _M_(b[2], m[10]) + _M_(b[3], m[14]);

	a[4] =  _M_(b[4], m[0])  + _M_(b[5], m[4]) +
	        _M_(b[6], m[8])  + _M_(b[7], m[12]);
	a[5] =  _M_(b[4], m[1])  + _M_(b[5], m[5]) +
	        _M_(b[6], m[9])  + _M_(b[7], m[13]);
	a[6] =  _M_(b[4], m[2])  + _M_(b[5], m[6]) +
	        _M_(b[6], m[10]) + _M_(b[7], m[14]);

	a[8] =  _M_(b[8], m[0])  + _M_(b[9], m[4]) +
	        _M_(b[10],m[8])  + _M_(b[11],m[12]);
	a[9] =  _M_(b[8], m[1])  + _M_(b[9], m[5]) +
	        _M_(b[10], m[9]) + _M_(b[11],m[13]);
	a[10] = _M_(b[8], m[2])  + _M_(b[9], m[6]) +
	        _M_(b[10],m[10]) + _M_(b[11],m[14]);

	a[12] = _M_(b[12], m[0]) + _M_(b[13], m[4]) +
	        _M_(b[14], m[8]) + _M_(b[15], m[12]);
	a[13] = _M_(b[12], m[1]) + _M_(b[13], m[5]) +
	        _M_(b[14], m[9]) + _M_(b[15], m[13]);
	a[14] = _M_(b[12], m[2]) + _M_(b[13], m[6]) +
	        _M_(b[14], m[10])+ _M_(b[15], m[14]);
}

#undef _M_

extern void
PL_mat_cpy(int *dst, int *src)
{
	memcpy(dst, src, sizeof(int) * 16);
}

extern int
PL_winding_order(int *a, int *b, int *c)
{
    int nc[3];
    
    nc[0] = (a[2] * b[1]) - (a[1] * b[2]);
    nc[1] = (a[0] * b[2]) - (a[2] * b[0]);
    nc[2] = (a[1] * b[0]) - (a[0] * b[1]);
    PL_vec_shorten(nc);
    return ((c[0] * nc[0]) + (c[1] * nc[1]) + (c[2] * nc[2])) < 0;
}

extern void
PL_vec_shorten(int *v)
{
    while (v[0] > 32767 || v[0] < -32768 ||
           v[1] > 32767 || v[1] < -32768 ||
           v[2] > 32767 || v[2] < -32768) {
        v[0] >>= 1;
        v[1] >>= 1;
        v[2] >>= 1;
    }
}

extern void
PL_psp_project(int *src, int *dst, int len, int num, int fov)
{
    int z, ffac;
    int nbytes;
    int shift = 0;
    
    ffac = (1 << (fov + 12));
    shift = fov - 8;
    
    len -= 3;
    nbytes = len * sizeof(int);
    while (num--) {
        z = src[2];
        fov = ffac / z;
        /* rounding is necessary */
        *dst++ = ((src[0] * fov + (1 << 11)) >> 12) + PL_vp_cen_x;
        *dst++ = PL_vp_cen_y - ((src[1] * fov + (1 << 11)) >> 12);
        *dst++ = fov >> shift;   /* 1/Z in 12.20 */
        memcpy(dst, src += 3, nbytes);
        src += len;
        dst += len;
    }
}
