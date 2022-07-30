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

/*  imode.c
 * 
 * Simple immediate mode geometry interface.
 * 
 */

#include <string.h>

static struct PL_OBJ product;
static struct PL_OBJ working_copy;

static int vertices[PL_MAX_OBJ_V];             /* temp storage for vertices */
static struct PL_POLY polys[PL_MAX_OBJ_V / 4]; /* temp storage for polygons */

static int polytype   = PL_TRIANGLES;
static int n_vertices = 0;     /* entered so far */
static int n_polys    = 0;     /* entered so far */

static struct PL_TEX *curtex = NULL;

static int cur_verts[4];
static int vert_num = 0;
static int cur_texc[2 * 4];
static int texc_num = 0;
static int cur_r    = 0xff;
static int cur_g    = 0xff;
static int cur_b    = 0xff;
static int cur_u    = 0;
static int cur_v    = 0;

static void
clear_product(void)
{
	int i;
	
	if (product.verts) {
		EXT_free(product.verts);
	}
	product.verts   = NULL;
	product.n_verts = 0;

	if (product.polys) {
		for (i = 0; i < product.n_polys; i++) {
			product.polys[i].color   = 0;
			product.polys[i].n_verts = 0;
			product.polys[i].tex     = NULL;
		}
		EXT_free(product.polys);
	}
	product.polys   = NULL;
	product.n_polys = 0;
}

static int
add_vertex(int x, int y, int z)
{
	int i;

	for (i = 0; i < n_vertices; i++) {
		if ((vertices[i * PL_VLEN    ] == x) &&
			(vertices[i * PL_VLEN + 1] == y) &&
			(vertices[i * PL_VLEN + 2] == z)) {
			return i;
		}
	}
	vertices[n_vertices * PL_VLEN    ] = x;
	vertices[n_vertices * PL_VLEN + 1] = y;
	vertices[n_vertices * PL_VLEN + 2] = z;
	return n_vertices++;
}

static void
add_polygon(void)
{
	struct PL_POLY *tmp;
    int i;
    int base;
    int edges;

	tmp = &polys[n_polys];
	memset(tmp, 0, sizeof(struct PL_POLY));
	tmp->tex = curtex;
	edges    = 3;
	
	switch (polytype) {
		case PL_TRIANGLES:
			edges = 3;
			break;
		case PL_QUADS:
			edges = 4;
			break;
	}
	if (edges == 4) {
	    /* check for a quad with two identical vertices,
	     * if it has any, turn it into a triangle
	     */
		if (cur_verts[0] == cur_verts[1]) {
			edges               = 3;
			cur_verts[1]        = cur_verts[2];
			cur_texc[2 * 1    ] = cur_texc[2 * 2];
			cur_texc[2 * 1 + 1] = cur_texc[2 * 2 + 1];

			cur_verts[2]        = cur_verts[3];
			cur_texc[2 * 2    ] = cur_texc[2 * 3];
			cur_texc[2 * 2 + 1] = cur_texc[2 * 3 + 1];
		}
		if (cur_verts[2] == cur_verts[3]) {
			edges = 3;
		}
	}
	
    tmp->color = (cur_r << 16 | cur_g << 8 | cur_b);
    
	for (i = 0; i < edges; i++) {
		base = i * PL_POLY_VLEN;
		tmp->verts[base    ] = cur_verts[i];
		tmp->verts[base + 1] = cur_texc[2 * i];
		tmp->verts[base + 2] = cur_texc[2 * i + 1];
	}
	base = i * PL_POLY_VLEN;
	tmp->verts[base    ] = cur_verts[0];
	tmp->verts[base + 1] = cur_texc[0];
	tmp->verts[base + 2] = cur_texc[1];
	tmp->n_verts = edges;
	
	n_polys++;
}

extern void
PL_ibeg(void)
{
	clear_product();
	
	n_vertices = 0;
	n_polys    = 0;
	vert_num   = 0;
	texc_num   = 0;
}

extern void
PL_type(int type)
{
	/* reset when primitive type is changed */
	if (type != polytype) {
		vert_num = 0;
		texc_num = 0;
	}
	polytype = type;
}

extern void
PL_texture(struct PL_TEX *tex)
{
	curtex = tex;
}

extern void
PL_color(int r, int g, int b)
{
	cur_r = r;
	cur_g = g;
	cur_b = b;
}

extern void
PL_texcoord(int u, int v)
{
	cur_u = u;
	cur_v = v;
}

extern void
PL_vertex(int x, int y, int z)
{
	cur_verts[vert_num++] = add_vertex(x, y, z);
	cur_texc[texc_num++]  = cur_u;
	cur_texc[texc_num++]  = cur_v;
	switch (polytype) {
		case PL_TRIANGLES:
			if (vert_num == 3) {
				add_polygon();
				vert_num = 0;
				texc_num = 0;
			}
			break;
		case PL_QUADS:
			if (vert_num == 4) {
				add_polygon();
				vert_num = 0;
				texc_num = 0;
			}
			break;
		default:
			vert_num = 0;
			texc_num = 0;
			break;
	}
}

extern void
PL_iend(void)
{
    int i;

	if (n_vertices && n_polys) {
		if (product.verts) {
			EXT_error(PL_ERR_MISC, "imode", "end without beg v");
		}
		product.verts = EXT_calloc(n_vertices * PL_VLEN, sizeof(int));
		if (product.verts == NULL) {
			EXT_error(PL_ERR_NO_MEM, "imode", "no memory");
		}
		for (i = 0; i < n_vertices * PL_VLEN; i++) {
			product.verts[i] = vertices[i];
		}
		product.n_verts = n_vertices;
		if (product.polys) {
			EXT_error(PL_ERR_MISC, "imode", "end without beg p");
		}
		product.polys = EXT_calloc(n_polys + 1, sizeof(struct PL_POLY));
		if (product.polys == NULL) {
			EXT_error(PL_ERR_NO_MEM, "imode", "no memory");
		}
		for (i = 0; i < n_polys; i++) {
			memcpy(&product.polys[i], &polys[i], sizeof(struct PL_POLY));
		}
		product.n_polys = n_polys;
	}
}

extern void
PL_iinit(void)
{
	if (n_vertices && n_polys) {
		PL_copy_object(&working_copy, &product);
	}
}

extern void
PL_irender(void)
{
	if (n_vertices && n_polys) {
		PL_render_object(&working_copy);
	}
}

extern int
PL_cur_vertex_count(void)
{
	return n_vertices;
}

extern int
PL_cur_polygon_count(void)
{
	return n_polys;
}

extern void
PL_export(struct PL_OBJ *dest)
{
	int i;
	
	dest->verts = EXT_calloc(product.n_verts * PL_VLEN, sizeof(int));
	if (dest->verts == NULL) {
		EXT_error(PL_ERR_NO_MEM, "imode", "no memory");
	}
	for (i = 0; i < product.n_verts * PL_VLEN; i++) {
		dest->verts[i] = product.verts[i];
	}
	dest->n_verts = product.n_verts;
	dest->polys = EXT_calloc(product.n_polys, sizeof(struct PL_POLY));
	if (dest->polys == NULL) {
		EXT_error(PL_ERR_NO_MEM, "imode", "no memory");
	}
	for (i = 0; i < n_polys; i++) {
		/* tex is shallow copied since we're not  */
		/* going to be deep copying texture data, */
		/* just need to copy over vertices        */
		memcpy(&dest->polys[i], &product.polys[i], sizeof(struct PL_POLY));
	}
	dest->n_polys = product.n_polys;
}

extern struct PL_OBJ *
PL_get_working_copy(void)
{
    return &working_copy;
}
