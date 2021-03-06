/* $Id: mve_main.c,v 1.7 2003/11/27 04:52:18 btb Exp $ */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <string.h>

#ifdef _WIN32
#	pragma pack(push)
#	pragma pack(8)
#	include <windows.h>
#	pragma pack(pop)
#endif

#ifdef _WIN32_WCE // should really be checking for "Pocket PC" somehow
# define LANDSCAPE
#endif

#include <SDL.h>

#include "libmve.h"
#include "u_mem.h"

static SDL_Surface *g_screen;
#ifdef LANDSCAPE
static SDL_Surface *real_screen;
#endif
static ubyte g_palette[768];
static int g_truecolor;

static int doPlay(const char *filename);

static void usage(void)
{
	fprintf(stderr, "usage: mveplay filename\n");
	exit(1);
}

int main(int c, char **v)
{
	if (c != 2)
		usage();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		fprintf(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);

	return doPlay(v[1]);
}


#ifdef LANDSCAPE
/* Create a new rotated surface for drawing */
SDL_Surface *CreateRotatedSurface(SDL_Surface *s)
{
    return(SDL_CreateRGBSurface(s->flags, s->h, s->w,
    s->format->BitsPerPixel,
    s->format->Rmask,
    s->format->Gmask,
    s->format->Bmask,
    s->format->Amask);
}

/* Used to copy the rotated scratch surface to the screen */
void BlitRotatedSurface(SDL_Surface *from, SDL_Surface *to)
{

    int bpp = from->format->BytesPerPixel;
    int w=from->w, h=from->h, pitch=to->pitch;
    int i,j;
    ubyte *pfrom, *pto, *to0;

    SDL_LockSurface(from);
    SDL_LockSurface(to);
    pfrom=reinterpret_cast<ubyte*> (from->pixels);
    to0=reinterpret_cast<ubyte*> (to->pixels) + pitch * (w - 1);
    for (i=0; i<h; i++)
    {
        to0+=bpp;
        pto=to0;
        for (j=0; j<w; j++)
        {
            if (bpp==1) *pto=*pfrom;
            else if (bpp==2) 
            	*reinterpret_cast<ushort*> (pto) = *reinterpret_cast<ushort*> (pfrom);
            else if (bpp==4) 
            	*reinterpret_cast<uint*> (pto) = *reinterpret_cast<uint*> (pfrom);
            else if (bpp==3)
                {
                    pto[0]=pfrom[0];
                    pto[1]=pfrom[1];
                    pto[2]=pfrom[2];
                }
            pfrom+=bpp;
            pto-=pitch;
        }
    }
    SDL_UnlockSurface(from);
    SDL_UnlockSurface(to);
}
#endif


static uint fileRead(void *handle, void *buf, uint count)
{
	unsigned numread;

	numread = fread(buf, 1, count, reinterpret_cast<FILE*> (handle));
	return (numread == count);
}

static void showFrame(ubyte *buf, uint bufw, uint bufh,
					  uint sx, uint sy,
					  uint w, uint h,
					  uint dstx, uint dsty)
{
	int i;
	ubyte *pal;
	SDL_Surface *sprite;
	SDL_Rect srcRect, destRect;

	assert(bufw == w && bufh == h);

	if (g_truecolor)
		sprite = SDL_CreateRGBSurfaceFrom(buf, bufw, bufh, 16, 2 * bufw, 0x7C00, 0x03E0, 0x001F, 0);
	else
	{
		sprite = SDL_CreateRGBSurfaceFrom(buf, bufw, bufh, 8, bufw, 0x7C00, 0x03E0, 0x001F, 0);

		pal = g_palette;
		for(i = 0; i < 256; i++)
		{
			sprite->format->palette->colors[i].r = (*pal++) << 2;
			sprite->format->palette->colors[i].g = (*pal++) << 2;
			sprite->format->palette->colors[i][BA] = (*pal++) << 2;
			sprite->format->palette->colors[i].unused = 0;
		}
	}

	srcRect.x = sx;
	srcRect.y = sy;
	srcRect.w = w;
	srcRect[HA] = h;
	destRect.x = dstx;
	destRect.y = dsty;
	destRect.w = w;
	destRect[HA] = h;

	SDL_BlitSurface(sprite, &srcRect, g_screen, &destRect);
#ifdef LANDSCAPE
	BlitRotatedSurface(g_screen, real_screen);
	if ( (real_screen->flags & SDL_DOUBLEBUF) == SDL_DOUBLEBUF )
		SDL_Flip(real_screen);
	else
		SDL_UpdateRect(real_screen, 0, 0, 0, 0);
#else
	if ( (g_screen->flags & SDL_DOUBLEBUF) == SDL_DOUBLEBUF )
		SDL_Flip(g_screen);
	else
		SDL_UpdateRects(g_screen, 1, &destRect);
#endif
	SDL_FreeSurface(sprite);
}

static void setPalette(ubyte *p, unsigned start, unsigned count)
{
	//Set color 0 to be black
	g_palette[0] = g_palette[1] = g_palette[2] = 0;

	//Set color 255 to be our subtitle color
	g_palette[765] = g_palette[766] = g_palette[767] = 50;

	//movie libs palette into our array
	memcpy(g_palette + start*3, p+start*3, count*3);
}

static int pollEvents()
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		switch(event.nType)
		{
		case SDL_QUIT:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			return 1;
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
			case SDLK_ESCAPE:
			case SDLK_q:
				return 1;
			case SDLK_f:
				SDL_WM_ToggleFullScreen(g_screen);
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

static int doPlay(const char *filename)
{
	int result;
	int done = 0;
	int bpp = 0;
	FILE *mve;
	MVE_videoSpec vSpec;

	mve = fopen(filename, "rb");
	if (mve == NULL) {
		fprintf(stderr, "can't open MVE file\n");
		return 1;
	}

	memset(g_palette, 0, 768);

	MVE_sndInit(1);
	MVE_memCallbacks(MVE_Alloc, MVE_Free);
	MVE_ioCallbacks(fileRead);
	MVE_sfCallbacks(showFrame);
	MVE_palCallbacks(setPalette);

	MVE_rmPrepMovie(mve, -1, -1, 1);

	MVE_getVideoSpec(&vSpec);

#ifndef _WIN32_WCE // doesn't like to change bpp?
	bpp = vSpec.truecolor?16:8;
#endif

#ifdef LANDSCAPE
	real_screen = SDL_SetVideoMode(vSpec.screenHeight, vSpec.screenWidth, bpp, SDL_FULLSCREEN);
	g_screen = CreateRotatedSurface(real_screen);
#else
	g_screen = SDL_SetVideoMode(vSpec.screenWidth, vSpec.screenHeight, bpp, SDL_ANYFORMAT);
#endif

	g_truecolor = vSpec.truecolor;

	while (!done && (result = MVE_rmStepMovie()) == 0)
	{
		done = pollEvents();
	}

	MVE_rmEndMovie();

	fclose(mve);

	return 0;
}
