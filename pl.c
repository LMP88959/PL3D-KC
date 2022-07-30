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

/*  pl.c
 * 
 * Handles objects, glues the different modules together.
 * 
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

struct PL_TEX *PL_cur_tex = NULL;

int PL_fov          = 9;
int PL_raster_mode  = PL_FLAT;
int PL_cull_mode    = PL_CULL_BACK;

static int tmp_vertices[PL_MAX_OBJ_V];

static void
load_stream(int *dst, int *src, int dim, int len, int *minz, int *maxz)
{
    int index, z;
    int lminz = INT_MAX;
    int lmaxz = INT_MIN;

    while (len--) {
        index = src[0];
        /* index into object vertex array */
        memcpy(dst, &tmp_vertices[index * PL_VLEN], sizeof(int) * 3);
        z = dst[2];
        if (z > lmaxz) lmaxz = z;
        if (z < lminz) lminz = z;

        if (dim == PL_STREAM_TEX) {
            dst[3] = src[1] << PL_TP;
            dst[4] = src[2] << PL_TP;
        } 
        src += 3;
        dst += dim;
    }
    *minz = lminz;
    *maxz = lmaxz;
}

static void
e_render_polygon(struct PL_POLY *poly)
{
    int minz, maxz; /* z extents for frustum testing */
    int res; /* result of frustum test */
    int stype; /* stream type */
    int nedge, rmode;
    struct PL_TEX *tex = PL_cur_tex;
    int *clipped;
    int back_face;
    
    int resv[(PL_MAX_POLY_VERTS * PL_VDIM) * 3];
    int *copy = resv + (0 * (PL_MAX_POLY_VERTS * PL_VDIM));
    int *clip = resv + (1 * (PL_MAX_POLY_VERTS * PL_VDIM));
    int *proj = resv + (2 * (PL_MAX_POLY_VERTS * PL_VDIM));

    nedge = poly->n_verts & 0xf;
    rmode = PL_raster_mode;
    
    switch (rmode) {
        case PL_TEXTURED:
            if (tex == NULL) {
                tex = poly->tex;
            }
            if (tex != NULL && tex->data) {
                stype = PL_STREAM_TEX;
                break;
            }
            /* if no texture, flat color */
            stype = PL_STREAM_FLAT;
            rmode = PL_FLAT;
            break;
        case PL_FLAT:
            stype = PL_STREAM_FLAT;
            break;
        default:
            return; /* bad raster mode */
    }
    
    load_stream(copy, poly->verts, stype, nedge + 1, &minz, &maxz);
    res = PL_frustum_test(minz, maxz);

    if (res == PL_Z_OUTC_OUTSIDE) {
        return;
    }

    /* test winding order in view space rather than screen space */
    back_face = PL_winding_order(copy, copy + stype, copy + stype * 2);
    
    if ((back_face + 1) & PL_cull_mode) {
        return;
    }
    
    if (res == PL_Z_OUTC_PART_NZ) {
        clipped = clip;
        nedge = PL_clip_poly_nz(clipped, copy, stype, nedge);
    } else {
        clipped = copy;
    }
    
    PL_psp_project(clipped, proj, stype, nedge + 1, PL_fov);
    
    if (rmode == PL_TEXTURED) {
        PL_lintx_poly(proj, nedge, tex->data);
    } else {
        PL_flat_poly(proj, nedge, poly->color);
    }
}

extern int
PL_xfproj_vert(int *in, int *out)
{
    int inv[PL_VLEN];
    int xf[PL_VLEN];
    int cnd;
    
    inv[0] = in[0];
    inv[1] = in[1];
    inv[2] = in[2];
    PL_mst_xf_modelview_vec(inv, xf, 1);
    cnd = PL_point_frustum_test(xf);
    if (cnd != PL_Z_OUTC_OUTSIDE) {
        PL_psp_project(xf, out, 3, 1, PL_fov);
    }
    return (cnd == PL_Z_OUTC_IN_VIEW);
}

extern void
PL_render_object(struct PL_OBJ *obj)
{
    int i;

    if (!obj) {
        return;
    }

    if (obj->n_verts >= PL_MAX_OBJ_V) {
        EXT_error(PL_ERR_MISC, "objmgr", "too many object vertices!");
    }

    PL_mst_xf_modelview_vec(obj->verts, tmp_vertices, obj->n_verts);

    for (i = 0; i < obj->n_polys; i++) {
        e_render_polygon(&obj->polys[i]);
    }
}

extern void
PL_delete_object(struct PL_OBJ *obj)
{
    int i;
    
    if (!obj) {
        return;
    }

    if (obj->verts) {
        EXT_free(obj->verts);
    }
    obj->verts   = NULL;
    obj->n_verts = 0;
    
    if (obj->polys) {
        for (i = 0; i < obj->n_polys; i++) {
            obj->polys[i].color   = 0;
            obj->polys[i].n_verts = 0;
            obj->polys[i].tex     = NULL;
        }
        EXT_free(obj->polys);
    }
    obj->polys   = NULL;
    obj->n_polys = 0;
}

extern void
PL_copy_object(struct PL_OBJ *dst, struct PL_OBJ *src)
{
    int i, size;
    
    if (!src) {
        EXT_error(PL_ERR_MISC, "objmgr", "objcpy null src");
        return;
    }
    if (!dst) {
        EXT_error(PL_ERR_MISC, "objmgr", "objcpy null dst");
        return;
    }
    PL_delete_object(dst);
    if (src->n_verts > 0) {
        size = src->n_verts * PL_VLEN * sizeof(int);
        dst->verts = EXT_calloc(1, size);
        if (dst->verts == NULL) {
            EXT_error(PL_ERR_NO_MEM, "objmgr", "no memory");
            return;
        }
        for (i = 0; i < src->n_verts * PL_VLEN; i++) {
            dst->verts[i] = src->verts[i];
        }
        dst->n_verts = src->n_verts;
    } else {
        dst->verts   = NULL;
        dst->n_verts = 0;
    }
    if (src->n_polys > 0) {
        size = src->n_polys * sizeof(struct PL_POLY);
        dst->polys = EXT_calloc(1, size);
        if (dst->polys == NULL) {
            EXT_error(PL_ERR_NO_MEM, "objmgr", "no memory");
        }
        memcpy(dst->polys, src->polys, size);
        dst->n_polys = src->n_polys;
    } else {
        dst->polys   = NULL;
        dst->n_polys = 0;
    }
}

extern void
PL_gen_box_list(int x, int y, int z, int w, int h, int d, int side_flags)
{
    int v0[3], v1[3], v2[3], v3[3], v4[3], v5[3], v6[3], v7[3];
    int tx0[2], tx1[2], tx2[2], tx3[2];
    int tsz;

    h >>= 1;
    w >>= 1;
    d >>= 1;

    v0[0] = x - h; v0[1] = y - w; v0[2] = z + d;
    v1[0] = x + h; v1[1] = y - w; v1[2] = z + d;
    v2[0] = x + h; v2[1] = y + w; v2[2] = z + d;
    v3[0] = x - h; v3[1] = y + w; v3[2] = z + d;
    v4[0] = x - h; v4[1] = y - w; v4[2] = z - d;
    v5[0] = x - h; v5[1] = y + w; v5[2] = z - d;
    v6[0] = x + h; v6[1] = y + w; v6[2] = z - d;
    v7[0] = x + h; v7[1] = y - w; v7[2] = z - d;
    
    tsz = PL_REQ_TEX_DIM - 1;
    tx0[0] = 0;
    tx0[1] = 0;
    tx1[0] = tsz;
    tx1[1] = 0;
    tx2[0] = tsz;
    tx2[1] = tsz;
    tx3[0] = 0;
    tx3[1] = tsz;

    if (side_flags & PL_BACK) {
        PL_texcoord(PL_VEC2_ELEMS(tx0));
        PL_vertex(PL_VEC3_ELEMS(v0));
        PL_texcoord(PL_VEC2_ELEMS(tx1));
        PL_vertex(PL_VEC3_ELEMS(v1));
        PL_texcoord(PL_VEC2_ELEMS(tx2));
        PL_vertex(PL_VEC3_ELEMS(v2));
        PL_texcoord(PL_VEC2_ELEMS(tx3));
        PL_vertex(PL_VEC3_ELEMS(v3));
    }
    if (side_flags & PL_FRONT) {
        PL_texcoord(PL_VEC2_ELEMS(tx0));
        PL_vertex(PL_VEC3_ELEMS(v4));
        PL_texcoord(PL_VEC2_ELEMS(tx1));
        PL_vertex(PL_VEC3_ELEMS(v5));
        PL_texcoord(PL_VEC2_ELEMS(tx2));
        PL_vertex(PL_VEC3_ELEMS(v6));
        PL_texcoord(PL_VEC2_ELEMS(tx3));
        PL_vertex(PL_VEC3_ELEMS(v7));
    }
    if (side_flags & PL_TOP) {
        PL_texcoord(PL_VEC2_ELEMS(tx0));
        PL_vertex(PL_VEC3_ELEMS(v5));
        PL_texcoord(PL_VEC2_ELEMS(tx1));
        PL_vertex(PL_VEC3_ELEMS(v3));
        PL_texcoord(PL_VEC2_ELEMS(tx2));
        PL_vertex(PL_VEC3_ELEMS(v2));
        PL_texcoord(PL_VEC2_ELEMS(tx3));
        PL_vertex(PL_VEC3_ELEMS(v6));
    }
    if (side_flags & PL_BOTTOM) {
        PL_texcoord(PL_VEC2_ELEMS(tx0));
        PL_vertex(PL_VEC3_ELEMS(v4));
        PL_texcoord(PL_VEC2_ELEMS(tx1));
        PL_vertex(PL_VEC3_ELEMS(v7));
        PL_texcoord(PL_VEC2_ELEMS(tx2));
        PL_vertex(PL_VEC3_ELEMS(v1));
        PL_texcoord(PL_VEC2_ELEMS(tx3));
        PL_vertex(PL_VEC3_ELEMS(v0));
    }
    if (side_flags & PL_RIGHT) {
        PL_texcoord(PL_VEC2_ELEMS(tx0));
        PL_vertex(PL_VEC3_ELEMS(v7));
        PL_texcoord(PL_VEC2_ELEMS(tx1));
        PL_vertex(PL_VEC3_ELEMS(v6));
        PL_texcoord(PL_VEC2_ELEMS(tx2));
        PL_vertex(PL_VEC3_ELEMS(v2));
        PL_texcoord(PL_VEC2_ELEMS(tx3));
        PL_vertex(PL_VEC3_ELEMS(v1));
    }
    if (side_flags & PL_LEFT) {
        PL_texcoord(PL_VEC2_ELEMS(tx0));
        PL_vertex(PL_VEC3_ELEMS(v4));
        PL_texcoord(PL_VEC2_ELEMS(tx1));
        PL_vertex(PL_VEC3_ELEMS(v0));
        PL_texcoord(PL_VEC2_ELEMS(tx2));
        PL_vertex(PL_VEC3_ELEMS(v3));
        PL_texcoord(PL_VEC2_ELEMS(tx3));
        PL_vertex(PL_VEC3_ELEMS(v5));
    }
}

extern struct PL_OBJ *
PL_gen_box(int w, int h, int d, int side_flags, int r, int g, int b)
{
    struct PL_OBJ *tmp;
    
    if (!(side_flags & PL_ALL)) {
        return NULL;
    }

    tmp = EXT_calloc(1, sizeof(struct PL_OBJ));
    if (tmp == NULL) {
        EXT_error(PL_ERR_NO_MEM, "objmgr", "no memory");
    }

    PL_ibeg();
    PL_type(PL_QUADS);
    PL_color(r, g, b);
    PL_gen_box_list(0, 0, 0, w, h, d, side_flags);
    PL_iend();
    PL_export(tmp);
    return tmp;
}
