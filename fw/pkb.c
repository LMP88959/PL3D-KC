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

#include "fw.h"

/*  pkb.c
 * 
 * Polled keyboard input handling.
 * Lets you easily test if a key is being held or was just pressed.
 * 
 */

#include <string.h>

#define KCT 	1024

#define KREL    3
#define KPRS    2
#define KONC    1

static char kstate[KCT];
static char kpress[KCT];

static void
kset(int k, char v)
{
    if ((k >= 0) && (k < KCT)) {
        kpress[k] = v;
    }
}

extern void
pkb_reset(void)
{
    memset(kstate, KREL, sizeof(kstate));
    memset(kpress, 0, sizeof(kpress));
}

extern void
pkb_poll(void)
{
    int i;
	
	for (i = 0; i < KCT; i++) {
		if (kpress[i]) {
            kstate[i] = (kstate[i] == KREL) ? KONC : KPRS;
		} else {
			kstate[i] = KREL;
		}
	}

}

extern void
pkb_keyboard(int key)
{
    kset(key, 1);
}

extern void
pkb_keyboardup(int key)
{
    kset(key, 0);
}

extern int
pkb_key_pressed(int key)
{
	return (kstate[key] == KONC);
}

extern int
pkb_key_held(int key)
{
	return (pkb_key_pressed(key) || (kstate[key] == KPRS));
}
