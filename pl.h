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

#ifndef PL_H
#define PL_H

/*  pl.h
 * 
 * Main header file for the PL library.
 * 
 */

#ifdef __cplusplus
extern "C" {
#endif

/* maximum possible horizontal or vertical resolution */
#define PL_MAX_SCREENSIZE 2048

/*****************************************************************************/
/********************************* CLIPPING **********************************/
/*****************************************************************************/

#define PL_Z_NEAR_PLANE    16    /* where near z plane is */

#define PL_Z_OUTC_IN_VIEW  0x0   /* in front of z plane */
#define PL_Z_OUTC_PART_NZ  0x1   /* partially in front of z plane */
#define PL_Z_OUTC_OUTSIDE  0x2   /* completely behind z plane */

extern int PL_vp_min_x;
extern int PL_vp_max_x;
extern int PL_vp_min_y;
extern int PL_vp_max_y;
extern int PL_vp_cen_x;
extern int PL_vp_cen_y;

/* define viewport
 * 
 * update_center - updates what the engine considers to be the perspective
 *                 focal point of the image
 * 
 */
extern void PL_set_viewport(
        int minx, int miny,
        int maxx, int maxy,
        int update_center);

/* clip lines and polygons to 2D viewport */
extern int PL_clip_line_x(int **v0, int **v1, int len, int min, int max);
extern int PL_clip_line_y(int **v0, int **v1, int len, int min, int max);
extern int PL_clip_poly_x(int *dst, int *src, int len, int num);
extern int PL_clip_poly_y(int *dst, int *src, int len, int num);

/* test point to determine if it's in front of near plane */
extern int PL_point_frustum_test(int *v);
/* test z bounds to determine its position relative to near plane */
extern int PL_frustum_test(int minz, int maxz);
/* clip polygon to near plane */
extern int PL_clip_poly_nz(int *dst, int *src, int len, int num);

/*****************************************************************************/
/********************************** ENGINE ***********************************/
/*****************************************************************************/

/* maximum number of vertices in object */
#define PL_MAX_OBJ_V  4096

#define PL_FLAT       1
#define PL_TEXTURED   0

#define PL_CULL_NONE  0
#define PL_CULL_FRONT 1
#define PL_CULL_BACK  2

/* for storage size definition */
#define PL_VDIM       5  /* X Y Z U V */
#define PL_POLY_VLEN  3  /* Idx U V */

extern int PL_fov; /* min valid value = 8 */
extern struct PL_TEX *PL_cur_tex;
extern int PL_raster_mode; /* PL_FLAT or PL_TEXTURED */
extern int PL_cull_mode;

struct PL_POLY {
    struct PL_TEX *tex;
    
    /* a user defined polygon may only have 3 or 4 vertices.  */
    
    /* [index, U, V] array of indices into obj verts array */
    int verts[6 * PL_POLY_VLEN]; 
    int color;
    int n_verts;
};

struct PL_OBJ {
    struct PL_POLY *polys; /* list of polygons in the object */
    int *verts; /* array of [x, y, z, 0] values */
    int  n_polys;
    int  n_verts;
};

/* take an XYZ coord in world space and convert to screen space */
extern int  PL_xfproj_vert(int *in, int *out);

extern void PL_render_object(struct PL_OBJ *obj);
extern void PL_delete_object(struct PL_OBJ *obj);
extern void PL_copy_object(struct PL_OBJ *dst, struct PL_OBJ *src);

/*****************************************************************************/
/*********************************** IMODE ***********************************/
/*****************************************************************************/

#define PL_TRIANGLES 0x00
#define PL_QUADS     0x01

extern void PL_ibeg(void); /* begin primitive */
/* type is PL_TRIANGLES or PL_QUADS */
extern void PL_type(int type);

/* applies to the next polygon made. */
extern void PL_texture (struct PL_TEX *tex);
/* last color defined before the poly is finished is used as the poly's color */
extern void PL_color   (int r, int g, int b);
extern void PL_texcoord(int u, int v);
extern void PL_vertex  (int x, int y, int z);

extern int  PL_cur_vertex_count (void);
extern int  PL_cur_polygon_count(void);

/* doesn't delete the previous object once called */
extern void PL_iend   (void); /* end primitive */

extern void PL_iinit  (void); /* initialize (only needed if not exporting) */
extern void PL_irender(void); /* render (only needed if not exporting) */

/* save current model that has been defined in immediate mode */
extern void PL_export(struct PL_OBJ *dest);

/* get pointer to object currently being defined in immediate mode */
extern struct PL_OBJ *PL_get_working_copy(void);

/*****************************************************************************/
/********************************* GRAPHICS **********************************/
/*****************************************************************************/

/* textures must be square with a dimension of (1 << PL_REQ_TEX_LOG_DIM) */
#define PL_REQ_TEX_LOG_DIM   7
#define PL_REQ_TEX_DIM       (1 << PL_REQ_TEX_LOG_DIM)

#define PL_TP                12 /* texture interpolation precision */

#define PL_MAX_POLY_VERTS    8  /* max verts in a polygon (post-clip) */

#define PL_STREAM_FLAT       3  /* X Y Z */
#define PL_STREAM_TEX        5  /* X Y Z U V */

extern int  PL_polygon_count; /* number of polygons rendered */

extern int  PL_hres;       /* horizontal resolution */
extern int  PL_vres;       /* vertical resolution */
extern int  PL_hres_h;     /* half resolutions */
extern int  PL_vres_h;

extern int *PL_video_buffer;
extern int *PL_depth_buffer;

/* only square textures with dimensions of PL_REQ_TEX_DIM */
struct PL_TEX {
    int *data; /* 4 byte-per-pixel true color X8R8G8B8 color data */
};

/* Call this to initialize PL
 * 
 * video - pointer to target image (4 byte-per-pixel true color X8R8G8B8)
 * hres - horizontal resolution of image
 * vres - vertical resolution of image
 * 
 */
extern void PL_init(int *video, int hres, int vres);

/* clear viewport color and depth */
extern void PL_clear_vp      (int r, int g, int b);
extern void PL_clear_color_vp(int r, int g, int b); /* clear viewport color */
extern void PL_clear_depth_vp(void); /* clear viewport depth */

/* Solid color polygon fill.
 * Expecting input stream of 3 values [X,Y,Z] */
extern void PL_flat_poly(int *stream, int len, int rgb);

/* Affine (linear) texture mapped polygon fill.
 * Expecting input stream of 5 values [X,Y,Z,U,V] */
extern void PL_lintx_poly(int *stream, int len, int *texel);

/*****************************************************************************/
/*********************************** MATH ************************************/
/*****************************************************************************/

/* maximum matrix stack depth */
#define PL_MAX_MST_DEPTH   64

/* number of elements in PL_sin and PL_cos */
#define PL_TRIGMAX         256
#define PL_TRIGMSK         (PL_TRIGMAX - 1)

/* number of elements in a vector */
#define PL_VLEN            4

/* precision for fixed point math */
#define PL_P               15
#define PL_P_ONE           (1 << PL_P)

/* identity matrix */
#define PL_IDT_MAT {       \
        PL_P_ONE, 0, 0, 0, \
        0, PL_P_ONE, 0, 0, \
        0, 0, PL_P_ONE, 0, \
        0, 0, 0, PL_P_ONE  \
}

#define PL_VEC2_ELEMS(x) x[0], x[1]
#define PL_VEC3_ELEMS(x) x[0], x[1], x[2]
#define PL_VEC4_ELEMS(x) x[0], x[1], x[2], x[3]

extern int PL_sin[PL_TRIGMAX];
extern int PL_cos[PL_TRIGMAX];

/* vectors are assumed to be integer arrays of length PL_VLEN */
/* matrices are assumed to be integer arrays of length 16 */

extern int  PL_winding_order  (int *a, int *b, int *c);
extern void PL_vec_shorten    (int *v); /* shorten vector to fit in 15 bits */
extern void PL_psp_project    (int *src, int *dst, int len, int num, int fov);

/* matrix stack (mst) */
extern void PL_mst_get        (int *m); /* get current top of mst */
extern void PL_mst_push       (void); /* push matrix onto mst */
extern void PL_mst_pop        (void); /* pop matrix from mst */
extern void PL_mst_load_idt   (void); /* load identity matrix */
extern void PL_mst_load       (int *m); /* load specified matrix to mst */
extern void PL_mst_mul        (int *m); /* multiply given matrix to mst */
extern void PL_mst_scale      (int x, int y, int z);
extern void PL_mst_translate  (int x, int y, int z);
extern void PL_mst_rotatex    (int rx);
extern void PL_mst_rotatey    (int ry);
extern void PL_mst_rotatez    (int rz);
extern void PL_set_camera(int x, int y, int z, int rx, int ry);

/* transform a stream of vertices by the current model+view */
extern void PL_mst_xf_modelview_vec(int *v, int *out, int len);

/* result is stored in 'a' */
extern void PL_mat_mul(int *a, int *b);
extern void PL_mat_cpy(int *dst, int *src);

/*****************************************************************************/
/************************************ GEN ************************************/
/*****************************************************************************/

/* flags to specify the faces of the box to generate */
#define PL_TOP       001
#define PL_BOTTOM    002
#define PL_BACK      004
#define PL_FRONT     010
#define PL_LEFT      020
#define PL_RIGHT     040
#define PL_ALL       077

/* generate immediate mode commands for a box */
extern void PL_gen_box_list(
        int x, int y, int z,
        int w, int h, int d,
        int side_flags);

/* generate a box */
extern struct PL_OBJ *PL_gen_box(
        int w, int h, int d,
        int side_flags,
        int r, int g, int b);

/*****************************************************************************/
/********************************* IMPORTER **********************************/
/*****************************************************************************/

extern int import_dmdl(char *name, struct PL_OBJ **o); /* import DMDL object */

/*****************************************************************************/
/******************************* USER DEFINED ********************************/
/*****************************************************************************/

#define PL_ERR_NO_MEM   0
#define PL_ERR_MISC     1
/* error function (PL expects program to halt after calling this) */
extern void  EXT_error(int err_id, char *modname, char *msg);

/* memory allocation function, ideally a calloc or something similar */
extern void *EXT_calloc(unsigned, unsigned);
/* memory freeing function */
extern void  EXT_free(void *);

#ifdef __cplusplus
}
#endif

#endif

