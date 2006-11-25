/* $Id: 2dsline.c,v 1.10 2003/04/29 08:05:41 btb Exp $ */
/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include <string.h>

#include "u_mem.h"

#include "gr.h"
#include "grdef.h"
#include "error.h"

#ifdef __MSDOS__
#include "modex.h"
#include "vesa.h"
#endif

#if defined(POLY_ACC)
#include "poly_acc.h"
#endif

#include "inferno.h"
#include "ogl_init.h"

#if !defined(NO_ASM) && defined(__WATCOMC__)

void gr_linear_darken( ubyte * dest, int darkeningLevel, int count, ubyte * fade_table );
#pragma aux gr_linear_darken parm [edi] [eax] [ecx] [edx] modify exact [eax ebx ecx edx edi] = \
"               xor ebx, ebx                "   \
"               mov bh, al                  "   \
"gld_loop:      mov bl, [edi]               "   \
"               mov al, [ebx+edx]           "   \
"               mov [edi], al               "   \
"               inc edi                     "   \
"               dec ecx                     "   \
"               jnz gld_loop                "

#elif !defined(NO_ASM) && defined(__GNUC__)

static inline void gr_linear_darken( ubyte * dest, int darkeningLevel, int count, ubyte * fade_table ) {
   int dummy[4];
   __asm__ __volatile__ (
"               xorl %%ebx, %%ebx;"
"               movb %%al, %%bh;"
"0:             movb (%%edi), %%bl;"
"               movb (%%ebx, %%edx), %%al;"
"               movb %%al, (%%edi);"
"               incl %%edi;"
"               decl %%ecx;"
"               jnz 0b"
   : "=D" (dummy[0]), "=a" (dummy[1]), "=c" (dummy[2]), "=d" (dummy[3])
   : "0" (dest), "1" (darkeningLevel), "2" (count), "3" (fade_table)
   : "%ebx");
}

#elif !defined(NO_ASM) && defined(_MSC_VER)

__inline void gr_linear_darken( ubyte * dest, int darkeningLevel, int count, ubyte * fade_table )
{
	__asm {
    mov edi,[dest]
    mov eax,[darkeningLevel]
    mov ecx,[count]
    mov edx,[fade_table]
    xor ebx, ebx
    mov bh, al
gld_loop:
    mov bl,[edi]
    mov al,[ebx+edx]
    mov [edi],al
    inc edi
    dec ecx
    jnz gld_loop
	}
}

#else // Unknown compiler, or no assembler. So we use C.

void gr_linear_darken(ubyte * dest, int darkeningLevel, int count, ubyte * fade_table) {
	register int i;

	for (i=0;i<count;i++)
	{
		*dest = fade_table[(*dest)+(darkeningLevel*256)];
		dest++;
	}
}

#endif

#ifdef NO_ASM // No Assembler. So we use C.
#if 0
void gr_linear_stosd( ubyte * dest, ubyte color, unsigned short count )
{
	int i, x;

	if (count > 3) {
		while ((int)(dest) & 0x3) { *dest++ = color; count--; };
		if (count >= 4) {
			x = (color << 24) | (color << 16) | (color << 8) | color;
			while (count > 4) { *(int *)dest = x; dest += 4; count -= 4; };
		}
		while (count > 0) { *dest++ = color; count--; };
	} else {
		for (i=0; i<count; i++ )
			*dest++ = color;
	}
}
#else
void gr_linear_stosd( ubyte * dest, grs_color *color, unsigned int nbytes) 
{
memset(dest, color->index, nbytes);
}
#endif
#endif

#if defined(POLY_ACC)
//$$ Note that this code WAS a virtual clone of the mac code and any changes to mac should be reflected here.
void gr_linear15_darken( short * dest, int darkeningLevel, int count, ubyte * fade_table )
{
    //$$ this routine is a prime candidate for using the alpha blender.
    int i;
    unsigned short rt[32], gt[32], bt[32];
    unsigned long level, intLevel, dlevel;

    dlevel = (darkeningLevel << 16) / GR_FADE_LEVELS;
    level = intLevel = 0;
    for(i = 0; i != 32; ++i)
    {
        rt[i] = intLevel << 10;
        gt[i] = intLevel << 5;
        bt[i] = intLevel;

        level += dlevel;
        intLevel = level >> 16;
    }

    pa_flush();
    for (i=0; i<count; i++ )    {
        if(*dest & 0x8000)
	        *dest =
   	         rt[((*dest >> 10) & 0x1f)] |
      	      gt[((*dest >> 5) & 0x1f)] |
         	   bt[((*dest >> 0) & 0x1f)] |
            	0x8000;
	        dest++;
	}
}

void gr_linear15_stosd( short * dest, ubyte color, unsigned short count )
{
    //$$ this routine is a prime candidate for using the alpha blender.
    short c = pa_clut[color];
    pa_flush();
    while(count--)
        *dest++ = c;
}
#endif

void gr_uscanline( int x1, int x2, int y )
{
	if (gameStates.render.grAlpha >= GR_ACTUAL_FADE_LEVELS ) {
		switch(TYPE)
		{
		case BM_LINEAR:
#ifdef OGL
		case BM_OGL:
			OglULineC(x1, y, x2, y, &COLOR);
#else
			gr_linear_stosd( DATA + ROWSIZE*y + x1, &COLOR, x2-x1+1);
#endif
			break;
#ifdef __MSDOS__
		case BM_MODEX:
			gr_modex_uscanline( x1+XOFFSET, x2+XOFFSET, y+YOFFSET, &COLOR );
			break;
		case BM_SVGA:
			gr_vesa_scanline( x1+XOFFSET, x2+XOFFSET, y+YOFFSET, &COLOR );
			break;
#endif
#if defined(POLY_ACC)
		case BM_LINEAR15:
			gr_linear15_stosd( (short *)(DATA + ROWSIZE*y + x1 * PA_BPP), &COLOR, x2-x1+1);
			break;
#endif
		}
	} else {
		switch(TYPE)
		{
		case BM_LINEAR:
#ifdef OGL
		case BM_OGL:
#endif
			gr_linear_darken( DATA + ROWSIZE*y + x1, (int) gameStates.render.grAlpha, x2-x1+1, grFadeTable);
			break;
#ifdef __MSDOS__
		case BM_MODEX:
			gr_modex_uscanline( x1+XOFFSET, x2+XOFFSET, y+YOFFSET, COLOR );
			break;
		case BM_SVGA:
#if 1
			{
				ubyte * vram = (ubyte *)0xA0000;
				int VideoLocation,page,offset1, offset2;

				VideoLocation = (ROWSIZE * y) + x1;
				page    = VideoLocation >> 16;
				offset1  = VideoLocation & 0xFFFF;
				offset2   = offset1 + (x2-x1+1);

				gr_vesa_setpage( page );
				if ( offset2 <= 0xFFFF ) {
					gr_linear_darken( &vram[offset1], gameStates.render.grAlpha, x2-x1+1, grFadeTable);
				} else {
					gr_linear_darken( &vram[offset1], gameStates.render.grAlpha, 0xFFFF-offset1+1, grFadeTable);
					page++;
					gr_vesa_setpage(page);
					gr_linear_darken( vram, gameStates.render.grAlpha, offset2 - 0xFFFF, grFadeTable);
				}
			}
#else
			gr_vesa_scanline( x1+XOFFSET, x2+XOFFSET, y+YOFFSET, COLOR );
#endif
			break;
#endif
#if defined(POLY_ACC)
		case BM_LINEAR15:
			gr_linear15_darken( (short *)(DATA + ROWSIZE*y + x1 * PA_BPP), gameStates.render.grAlpha, x2-x1+1, grFadeTable);
			break;
#endif
		}
	}
}

void gr_scanline( int x1, int x2, int y )
{
	if ((y<0)||(y>MAXY)) return;

	if (x2 < x1 ) x2 ^= x1 ^= x2;

	if (x1 > MAXX) return;
	if (x2 < MINX) return;

	if (x1 < MINX) x1 = MINX;
	if (x2 > MAXX) x2 = MAXX;

	if (gameStates.render.grAlpha >= GR_ACTUAL_FADE_LEVELS ) {
		switch(TYPE)
		{
		case BM_LINEAR:
#ifdef OGL
		case BM_OGL:
#endif
			gr_linear_stosd( DATA + ROWSIZE*y + x1, &COLOR, x2-x1+1);
			break;
#ifdef __MSDOS__
		case BM_MODEX:
			gr_modex_uscanline( x1+XOFFSET, x2+XOFFSET, y+YOFFSET, &COLOR );
			break;
		case BM_SVGA:
			gr_vesa_scanline( x1+XOFFSET, x2+XOFFSET, y+YOFFSET, &COLOR );
			break;
#endif
#if defined(POLY_ACC)
		case BM_LINEAR15:
			gr_linear15_stosd( (short *)(DATA + ROWSIZE*y + x1 * PA_BPP), &COLOR, x2-x1+1);
			break;
#endif
		}
	} else {
		switch(TYPE)
		{
		case BM_LINEAR:
#ifdef OGL
		case BM_OGL:
#endif
			gr_linear_darken( DATA + ROWSIZE*y + x1, (int) gameStates.render.grAlpha, x2-x1+1, grFadeTable);
			break;
#ifdef __MSDOS__
		case BM_MODEX:
			gr_modex_uscanline( x1+XOFFSET, x2+XOFFSET, y+YOFFSET, COLOR );
			break;
		case BM_SVGA:
#if 1
			{
				ubyte * vram = (ubyte *)0xA0000;
				int VideoLocation,page,offset1, offset2;

				VideoLocation = (ROWSIZE * y) + x1;
				page    = VideoLocation >> 16;
				offset1  = VideoLocation & 0xFFFF;
				offset2   = offset1 + (x2-x1+1);

				gr_vesa_setpage( page );
				if ( offset2 <= 0xFFFF )	{
					gr_linear_darken( &vram[offset1], gameStates.render.grAlpha, x2-x1+1, grFadeTable);
				} else {
					gr_linear_darken( &vram[offset1], gameStates.render.grAlpha, 0xFFFF-offset1+1, grFadeTable);
					page++;
					gr_vesa_setpage(page);
					gr_linear_darken( vram, gameStates.render.grAlpha, offset2 - 0xFFFF, grFadeTable);
				}
			}
#else
			gr_vesa_scanline( x1+XOFFSET, x2+XOFFSET, y+YOFFSET, COLOR );
#endif
			break;
#endif
#if defined(POLY_ACC)
		case BM_LINEAR15:
			gr_linear15_darken( (short *)(DATA + ROWSIZE*y + x1 * PA_BPP), gameStates.render.grAlpha, x2-x1+1, grFadeTable);
			break;
#endif
		}
	}
}
