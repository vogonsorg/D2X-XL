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
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#ifdef HAVE_CONFIG_H
#include <conf.height>
#endif

#ifdef _WIN32
#	include <windows.h>
#	include <stddef.h>
#endif
#ifdef __macosx__
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
#else
# include <GL/gl.h>
# include <GL/glu.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#ifndef _WIN32
#	include <unistd.h>
#endif

#include "error.h"
#include "inferno.h"
#include "key.h"
#include "text.h"
#include "findfile.h"
#include "gamefont.h"
#include "pcx.h"
#include "u_mem.h"
#include "strutil.h"
#include "ogl_lib.h"
#include "ogl_bitmap.h"
#include "render.h"
#include "menu.h"
#include "input.h"
#include "menubackground.h"

#if defined (TACTILE)
 #include "tactile.height"
#endif

#define LHX(x)      (gameStates.menus.bHires? 2 * (x) : x)
#define LHY(y)      (gameStates.menus.bHires? (24 * (y)) / 10 : y)

CPalette* menuPalette;

CBackgroundManager backgroundManager;

#define MENU_BACKGROUND_BITMAP_HIRES (CFile::Exist ("scoresb.pcx", gameFolders.szDataDir, 0)? "scoresb.pcx": "scores.pcx")
#define MENU_BACKGROUND_BITMAP_LORES (CFile::Exist ("scores.pcx", gameFolders.szDataDir, 0) ? "scores.pcx": "scoresb.pcx") // Mac datafiles only have scoresb.pcx

#define MENU_BACKGROUND_BITMAP (gameStates.menus.bHires ? MENU_BACKGROUND_BITMAP_HIRES : MENU_BACKGROUND_BITMAP_LORES)

int bHiresBackground;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void CBackground::Init (void)
{
m_canvas = NULL;
m_saved = NULL;
m_background = NULL;
m_name = NULL;
m_bIgnoreCanv = false;
m_bIgnoreBg = false;
}

//------------------------------------------------------------------------------

void CBackground::Destroy (void)
{
if (m_background && (m_background != backgroundManager.Background ()))
	delete m_background;
if (m_saved)
	delete m_saved;
if (m_canvas)
	m_canvas->Destroy ();
Init ();
}

//------------------------------------------------------------------------------

bool CBackground::Load (char* filename, int width, int height)
{
if (!filename)
	m_background = backgroundManager.Background (1)->CreateChild (0, 0, width, height);
else if (backgroundManager.IsDefault (filename) || !(m_background = backgroundManager.LoadBackground (filename)))
	m_background = backgroundManager.Background (0);
}

//------------------------------------------------------------------------------

void CBackground::Setup (int x, int y, int width, int height)
{
if (m_canvas)
	m_canvas->Destroy ();
if (gameOpts->menus.nStyle)
	m_canvas = screen.Canvas ()->CreatePane (0, 0, screen.Width (), screen.Height ());
else
	m_canvas = screen.Canvas ()->CreatePane (x, y, width, height);
CCanvas::SetCurrent (m_canvas);
	
}

//------------------------------------------------------------------------------

void CBackground::Save (int width, int height)
{
if (!gameOpts->menus.nStyle) {
	if (m_saved)
		delete m_saved;
	m_saved = CBitmap::Create (0, width, height, 1);
	m_saved->SetPalette (paletteManager.Default ());
	CCanvas::Current ()->RenderToBitmap (m_saved, 0, 0, width, height, 0, 0);
	}
}

//------------------------------------------------------------------------------

bool CBackground::Create (char* filename, int x, int y, int width, int height)
{
Destroy ();
m_bTopMenu = (backgroundManager.Depth () == 1);
m_bMenuBox = gameOpts->menus.nStyle && (!m_bTopMenu || (gameOpts->menus.altBg.bHave > 0));
if (!Load (filename, width, height))
	return false;
Setup (x, y, width, height);
Draw ();
Save (width, height);
return true;
}

//------------------------------------------------------------------------------

void CBackground::Draw (void)
{
paletteManager.SetEffect (0, 0, 0);
if (!(gameStates.menus.bNoBackground || gameStates.app.bGameRunning)) {
	m_background->Stretch ();
	if (m_bTopMenu)
		PrintVersionInfo ();
	}
if (m_bTopMenu)
	;
else if (m_bMenuBox) {
	if (m_canvas || m_bIgnoreCanv) {
		if (!m_bIgnoreCanv)
			CCanvas::SetCurrent (m_canvas);
		}
	backgroundManager.DrawBox (gameData.menu.nLineWidth, 1.0f, 0);
	}
paletteManager.LoadEffect ();
GrUpdate (0);
}

//------------------------------------------------------------------------------
// Redraw a part of the menu area's background

void CBackground::DrawArea (int left, int top, int right, int bottom, int bPartial)
{
if (gameOpts->menus.nStyle) 
	Draw ();
else {
	if (left < 0) 
		left = 0;
	if (top < 0)
		top = 0;
	int width = right - left + 1;
	int height = bottom - top + 1;
	//if (width > nmBackground.Width ()) width = nmBackground.Width ();
	//if (height > nmBackground.Height ()) height = nmBackground.Height ();
	right = left + width - 1;
	bottom = top + height - 1;
	glDisable (GL_BLEND);
	PrintVersionInfo ();
	if (!m_b3DEffect)
		m_saved->RenderClipped (CCanvas::Current (), left, top, width, height, LHX (10), LHY (10));
	else {
		m_saved->RenderClipped (CCanvas::Current (), left, top, width, height, 0, 0);
		gameStates.render.grAlpha = 2 * 7;
		CCanvas::Current ()->SetColorRGB (0, 0, 0, 255);
		GrURect (right - 5, top + 5, right - 6, bottom - 5);
		GrURect (right - 4, top + 4, right - 5, bottom - 5);
		GrURect (right - 3, top + 3, right - 4, bottom - 5);
		GrURect (right - 2, top + 2, right - 3, bottom - 5);
		GrURect (right - 1, top + 1, right - 2, bottom - 5);
		GrURect (right + 0, top + 0, right - 1, bottom - 5);
		GrURect (left + 5, bottom - 5, right, bottom - 6);
		GrURect (left + 4, bottom - 4, right, bottom - 5);
		GrURect (left + 3, bottom - 3, right, bottom - 4);
		GrURect (left + 2, bottom - 2, right, bottom - 3);
		GrURect (left + 1, bottom - 1, right, bottom - 2);
		GrURect (left + 0, bottom, right, bottom - 1);
		}
	glEnable (GL_BLEND);
	GrUpdate (0);
	}
gameStates.render.grAlpha = FADE_LEVELS;
}

//------------------------------------------------------------------------------

void CBackground::Restore (void)
{
if (!gameOpts->menus.nStyle) {
	if (m_saved)
		m_saved->Blit ();
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

CBackgroundManager::CBackgroundManager () 
{ 
m_save.Create (3); 
m_background [0] = NULL;
m_background [1] = NULL;
m_nDepth = 0;
m_filename [0] = MENU_PCX_NAME ();
filename [1] = MENU_BACKGROUND_BITMAP;
}

//------------------------------------------------------------------------------

void CBackgroundManager::Remove (void) 
{
m_bg.Restore ();
m_bg.Destroy ();
if (gameStates.app.bGameRunning) 
	paletteManager.LoadEffect ();
}

//------------------------------------------------------------------------------

void CBackgroundManager::Init (void)
{
m_background = NULL;
m_nDepth = 0; 
m_b3DEffect = false;
m_filename [0] = MENU_PCX_NAME;
m_filename [1] = MENU_BACKGROUND_BITMAP;
}

//------------------------------------------------------------------------------

void CBackgroundManager::Destroy (void)
{
if (m_background)
	m_background.Destroy ();
Init ();
}

//------------------------------------------------------------------------------

void CBackgroundManager::DrawBox (int nLineWidth, float fAlpha, int bForce)
{
gameStates.render.nFlashScale = 0;
if (bForce || (gameOpts->menus.nStyle == 1)) {
	CCanvas::Current ()->SetColorRGB (PAL2RGBA (22), PAL2RGBA (22), PAL2RGBA (38), (ubyte) (gameData.menu.alpha * fAlpha));
	gameStates.render.grAlpha = (float) gameData.menu.alpha * fAlpha / 255.0f;
	glDisable (GL_TEXTURE_2D);
	GrURect (CCanvas::Current ()->Left (), CCanvas::Current ()->Top (),
			   CCanvas::Current ()->Right (), CCanvas::Current ()->Bottom ());
	gameStates.render.grAlpha = FADE_LEVELS;
	CCanvas::Current ()->SetColorRGB (PAL2RGBA (22), PAL2RGBA (22), PAL2RGBA (38), (ubyte) (255 * fAlpha));
	glLineWidth ((GLfloat) nLineWidth);
	GrUBox (CCanvas::Current ()->Left (), CCanvas::Current ()->Top (),
			  CCanvas::Current ()->Right (), CCanvas::Current ()->Bottom ());
	glLineWidth (1);
	}
}

//------------------------------------------------------------------------------

bool CBackgroundManager::IsDefault (char* filename)
{
return !strcmp (filename, m_filename [0]);
}

//------------------------------------------------------------------------------

CBitmap* CBackgroundManager::LoadCustomBackground (void)
{
if (m_background || (gameStates.app.bNostalgia || !gameOpts->menus.nStyle))
	return m_background;

CBitmap* bmP;

gameOpts->menus.altBg.bHave = 0;
if (!(bmP = CBitmap::Create (0, 0, 0, 1)))
	return NULL;
if (!ReadTGA (gameOpts->menus.altBg.szName, 
#ifdef __linux__
				  gameFolders.szCacheDir,
#else
				  NULL, 
#endif
				  m_customBg, 
				 (gameOpts->menus.altBg.alpha < 0) ? -1 : (int) (gameOpts->menus.altBg.alpha * 255), 
				 gameOpts->menus.altBg.brightness, gameOpts->menus.altBg.grayscale, 0)) {
	delete bmP;
	gameOpts->menus.altBg.bHave = -1;
	return NULL;
	}
gameOpts->menus.altBg.bHave = 1;
return bmP;
}

//------------------------------------------------------------------------------

CBitmap* CBackgroundManager::LoadBackground (char* filename)
{
	int width, height;

if (PCXGetDimensions (filename, &width, &height) != PCX_ERROR_NONE) {
	Error ("Could not open menu background file <%s>\n", filename);
	return NULL;
	}
CBitmap* bmP;
if (!(bmP = CBitmap::Create (0, width, height, 1))) {
	Error ("Not enough memory for menu backgroun\n", filename);
	return NULL;
	}
if (PCXReadBitmap (filename, bmP, bmP->Mode (), 0) != PCX_ERROR_NONE) {
	Error ("Could not read menu background file <%s>\n", filename);
	return NULL;
	}
bmP->SetName (filename);
return bmP;
}

//------------------------------------------------------------------------------

void CBackgroundManager::Create (void)
{
Destroy ();
if (!LoadCustomBackground ())
	m_background [0] = LoadBackground (m_filename [0]);
m_background [1] = LoadBackground (m_filename [1]);
}

//------------------------------------------------------------------------------

void CBackgroundManager::Setup (char *filename, int x, int y, int width, int height, int bPartial)
{
if (m_nDepth && !m_save.Push (m_bg))
	return;
m_nDepth++;
if (m_bg.Create (filename, x, y, width, height, bPartial)) {
	if (!m_filename)
		m_filename = filename;
	}
}

//------------------------------------------------------------------------------

void CBackgroundManager::Restore (void)
{
m_bg.Restore ();
}

//------------------------------------------------------------------------------
