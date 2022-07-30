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

/*  main.c
 * 
 * Basic demo showing how to define a 3D scene, generate geometry,
 * import geometry, implement first person camera controls, and transform
 * the geometry.
 * 
 * Controls:
 *      Arrow keys - looking
 *      W/A/S/D keys - movement
 *      T/G keys - move up and down
 *      C - cycle through culling modes
 *      1 - flat rendering
 *      2 - textured rendering
 *      3 - toggle between two FOVs
 *      SPACE - start/stop dynamic transformation
 * 
 */

#include "fw/fw.h"

#include <stdlib.h>
#include <stdio.h>

extern void *
EXT_calloc(unsigned n, unsigned esz)
{
    return calloc(n, esz);
}

extern void
EXT_free(void *p)
{
    free(p);
}

extern void
EXT_error(int err_id, char *modname, char *msg)
{
    printf("vx error 0x%x in %s: %s\n", err_id, modname, msg);
    sys_kill();
    getchar();
    exit(0);
}

#define VW 896
#define VH 504
/* cube size */
#define CUSZ 128
/* grid size */
#define GRSZ 1
/* movement speed */
#define MOVSPD 4

static struct PL_OBJ *floortile;
static struct PL_OBJ *texcube;
static struct PL_OBJ *imported;
static int camrx = 0, camry = 0;
static int x = 0, y = 200, z = 90;
static int rot = 1;
static int sinvar = 0;
static struct PL_TEX checktex;
static int checker[PL_REQ_TEX_DIM * PL_REQ_TEX_DIM];
static unsigned fpsclock = 0;

static void
maketex(void)
{
    int i, j, c;

    for (i = 0; i < PL_REQ_TEX_DIM; i++) {
        for (j = 0; j < PL_REQ_TEX_DIM; j++) {
            if (i < 0x10 || j < 0x10 ||
               (i > ((PL_REQ_TEX_DIM - 1) - 0x10)) ||
               (j > ((PL_REQ_TEX_DIM - 1) - 0x10))) {
                /* border color */
                c = 0x3f4f5f;
            } else {
                /* checkered pattern */
                if (((((i & 0x10)) ^ ((j & 0x10))))) {
                    c = 0x3f4f5f;
                } else {
                    c = 0xd4ccba;
                }
            }
            if (i == j || abs(i - j) < 3) {
                /* thick red line along diagonal */
                checker[i + j * PL_REQ_TEX_DIM] = 0x902215;
            } else {
                checker[i + j * PL_REQ_TEX_DIM] = c;
            }
        }
    }
    checktex.data = checker;
}

static void
init(void)
{
    maketex();

	PL_texture(&checktex);
	texcube = PL_gen_box(CUSZ, CUSZ, CUSZ, PL_ALL, 255, 255, 255);
	PL_texture(NULL);
	floortile = PL_gen_box(CUSZ, CUSZ, CUSZ, PL_TOP, 77, 101, 94);

	import_dmdl("pots", &imported);
	
	PL_fov = 9;
    
    PL_cur_tex = NULL;
	PL_cull_mode = PL_CULL_BACK;
	PL_raster_mode = PL_TEXTURED;
	
	fpsclock = clk_sample();
}

static void
update(void)
{    
	if (pkb_key_pressed(FW_KEY_ESCAPE)) {
		sys_shutdown();
	}
	if (pkb_key_held(FW_KEY_ARROW_RIGHT)) {
		camry += 1;
	}
	if (pkb_key_held(FW_KEY_ARROW_LEFT)) {
		camry -= 1;
	}
	if (pkb_key_held(FW_KEY_ARROW_UP)) {
		camrx -= 1;
	}
	if (pkb_key_held(FW_KEY_ARROW_DOWN)) {
		camrx += 1;
	}

	if (pkb_key_held('w')) {
		x += (MOVSPD * PL_sin[camry & PL_TRIGMSK]) >> PL_P;
		y -= (MOVSPD * PL_sin[camrx & PL_TRIGMSK]) >> PL_P;
		z += (MOVSPD * PL_cos[camry & PL_TRIGMSK]) >> PL_P;
	}
	if (pkb_key_held('s')) {
		x -= (MOVSPD * PL_sin[camry & PL_TRIGMSK]) >> PL_P;
		y += (MOVSPD * PL_sin[camrx & PL_TRIGMSK]) >> PL_P;
		z -= (MOVSPD * PL_cos[camry & PL_TRIGMSK]) >> PL_P;
	}
	if (pkb_key_held('a')) {
		x -= (MOVSPD * PL_cos[camry & PL_TRIGMSK]) >> PL_P;
		z += (MOVSPD * PL_sin[camry & PL_TRIGMSK]) >> PL_P;
	}
	if (pkb_key_held('d')) {
		x += (MOVSPD * PL_cos[camry & PL_TRIGMSK]) >> PL_P;
		z -= (MOVSPD * PL_sin[camry & PL_TRIGMSK]) >> PL_P;
	}
	if (pkb_key_held('t')) {
		y += MOVSPD;
	}
	if (pkb_key_held('g')) {
		y -= MOVSPD;
	}
	if (pkb_key_pressed('c')) {
		static int cmod = PL_CULL_BACK;
		if (cmod == PL_CULL_BACK) {
			cmod = PL_CULL_NONE;
		} else if (cmod == PL_CULL_FRONT) {
			cmod = PL_CULL_BACK;
		} else {
			cmod = PL_CULL_FRONT;
		}
		PL_cull_mode = cmod;
	}
	if (pkb_key_held('1')) {
	    PL_raster_mode = PL_FLAT;
	}
	if (pkb_key_held('2')) {
	    PL_raster_mode = PL_TEXTURED;
	}

	if (pkb_key_pressed('3')) {
		if (PL_fov == 8) {
		    PL_fov = 9;
		} else {
		    PL_fov = 8;
		}
		printf("fov: %d\n", PL_fov);
	}

	if (pkb_key_pressed(' ')) {
		rot = !rot;
	}
	sinvar++;
}

static void
display(void)
{
    int i = 0, j = 0;
    int p1 = PL_P_ONE;
    int mo;

    /* clear viewport to black */
	PL_clear_vp(0, 0, 0);
	PL_polygon_count = 0;
    
    /* define camera orientation */
    PL_set_camera(x, y, z, camrx, camry);
    
    { /* draw imported model */
        PL_mst_push();
        if (rot) {
            mo = (PL_sin[sinvar & PL_TRIGMSK] * 256) >> PL_P;
            PL_mst_translate(mo, 400, 500);
        } else {
            PL_mst_translate(0, 400, 500);
        }
        PL_render_object(imported);
        PL_mst_pop();
    }
    
    /* draw tile grid */
    for (i = -GRSZ; i < GRSZ; i++) {
        for (j = -GRSZ; j < GRSZ; j++) {
            PL_mst_push();
            PL_mst_translate(
                       0 + i * CUSZ,
                       0,
                     600 + j * CUSZ);
            PL_render_object(floortile);
            PL_mst_pop();
        }
    }

    { /* draw textured cube */
        PL_mst_push();
        PL_mst_translate(-100, 100, 500);
        if (rot) {
            PL_mst_rotatex(sinvar >> 2);
            PL_mst_rotatey(sinvar >> 1);
            PL_mst_scale(p1 * ((sinvar & 0xff) + 128) >> 8, p1, p1);
        }
        PL_render_object(texcube);
        PL_mst_pop();
    }
	
	if (clk_sample() > fpsclock) {
	    fpsclock = clk_sample() + 1000;
	    printf("FPS: %d\n", sys_getfps());
	}

	/* update window and sync */
    vid_blit();
    vid_sync();
}

int
main(int argc, char **argv)
{
    if (argc != 1) {
        printf("note: %s does not take any arguments.\n", argv[0]);
    }
    
    sys_init();
    sys_updatefunc(update);
    sys_displayfunc(display);

    clk_mode(FW_CLK_MODE_HIRES);
    pkb_reset();
    sys_sethz(70);
    sys_capfps(0);

    if (vid_open("PL", VW, VH, 1, FW_VFLAG_VIDFAST) != FW_VERR_OK) {
        FW_error("unable to create window\n");
        return 1;
    }

    /* give the video memory to PL */
    PL_init(vid_getinfo()->video, VW, VH);
    
    init();
    sys_start();

    sys_shutdown();
    return 0;
}
