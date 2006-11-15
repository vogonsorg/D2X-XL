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
/*
 *
 * Protypes for rle functions.
 *
 * Old Log:
 * Revision 1.5  1995/01/14  11:32:20  john
 * Added RLECacheFlush function.
 *
 * Revision 1.4  1994/11/10  13:16:01  matt
 * Added includes
 *
 * Revision 1.3  1994/11/09  19:53:51  john
 * Added texture rle caching.
 *
 * Revision 1.2  1994/11/09  16:35:18  john
 * First version with working RLE bitmaps.
 *
 * Revision 1.1  1994/11/09  12:40:17  john
 * Initial revision
 *
 *
 */

#ifndef _RLE_H
#define _RLE_H

#include "pstypes.h"
#include "gr.h"

void gr_rle_decode( ubyte * src, ubyte * dest );
int gr_rle_encode( int org_size, ubyte *src, ubyte *dest );
int gr_rle_getsize( int org_size, ubyte *src );
ubyte * r_rle_find_xth_pixel( ubyte *src, int x,int * count, ubyte color );
int gr_bitmap_rle_compress( grsBitmap * bmp );
void gr_rle_expand_scanline_masked( ubyte *dest, ubyte *src, int x1, int x2  );
void gr_rle_expand_scanline( ubyte *dest, ubyte *src, int x1, int x2  );

grsBitmap * rle_expand_texture( grsBitmap * bmP );

void RLECacheFlush();

void rle_swap_0_255(grsBitmap *bmP);

int rle_remap (grsBitmap *bmP, ubyte *colorMap, int maxLen);

int rle_expand (grsBitmap *bmP, ubyte *colorMap, int bSwap0255);

#endif
