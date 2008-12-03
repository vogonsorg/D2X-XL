#ifndef _BITMAP_H
#define _BITMAP_H

#include "ogl_defs.h"
#include "ogl_texture.h"
#include "fbuffer.h"
#include "pbuffer.h"
#include "carray.h"

//-----------------------------------------------------------------------------

#define BM_LINEAR   0
#define BM_MODEX    1
#define BM_SVGA     2
#define BM_RGB15    3   //5 bits each r, g, b stored at 16 bits
#define BM_SVGA15   4
#define BM_OGL      5

//-----------------------------------------------------------------------------

typedef struct grsPoint {
	fix x, y;
} grsPoint;

//-----------------------------------------------------------------------------

class CBitmap;
class CTexture;

#define BM_FLAG_TRANSPARENT         1
#define BM_FLAG_SUPER_TRANSPARENT   2
#define BM_FLAG_NO_LIGHTING         4
#define BM_FLAG_RLE                 8   // A run-length encoded bitmap.
#define BM_FLAG_PAGED_OUT           16  // This bitmap's data is paged out.
#define BM_FLAG_RLE_BIG             32  // for bitmaps that RLE to > 255 per row (i.e. cockpits)
#define BM_FLAG_SEE_THRU				64  // door or other texture containing see-through areas
#define BM_FLAG_TGA						128

//-----------------------------------------------------------------------------

typedef struct tBmProps {
	short   x, y;		// Offset from parent's origin
	short   w, h;		// width, height
	short   rowSize;	// ubyte offset to next row
	sbyte	  nMode;		// 0=Linear, 1=ModeX, 2=SVGA
	ubyte	  flags;		
} tBmProps;

typedef struct tStdBmInfo {
	CBitmap	*override;
	CBitmap	*mask;	//intended for supertransparency masks 
	CBitmap	*parent;
} tStdBmInfo;

typedef struct tAltBmInfo {
	ubyte			nFrameCount;
	CBitmap		*frames;
	CBitmap		*curFrame;
} tAltBmInfo;

#define BM_TYPE_STD		0
#define BM_TYPE_ALT		1
#define BM_TYPE_FRAME	2
#define BM_TYPE_MASK		4

//-----------------------------------------------------------------------------

class CByteArray : public CArray< ubyte > {};

//-----------------------------------------------------------------------------

typedef struct tBitmap {
	char				szName [20];
	tBmProps			props;
	ubyte				flags;		
	CPalette			*palette;
	CByteArray		buffer;
	//ubyte				*buffer;			// ptr to texture data...
											//   Linear = *parent+(rowSize*y+x)
											//   ModeX = *parent+(rowSize*y+x/4)
											//   SVGA = *parent+(rowSize*y+x)
	ushort			nId;				//for application.  initialized to 0
	tRgbColorb		avgColor;		//Average color of all pixels in texture map.
	ubyte				avgColorIndex;	//palette index of palettized color closest to average RGB color
	ubyte				nBPP;
	ubyte				nType;
	ubyte				bWallAnim;
	ubyte				bFromPog;
	ubyte				bChild;
	ubyte				bFlat;		//no texture, just a colored area
	ubyte				nTeam;
#if TEXTURE_COMPRESSION
	ubyte				bCompressed;
	int				nFormat;
	int				nBufSize;
#endif
	int				transparentFrames [4];
	int				supertranspFrames [4];

	CTexture			*texture;
	struct {
		tStdBmInfo	std;
		tAltBmInfo	alt;
		} info;
} tBitmap;


#define BM_FRAMECOUNT(_bmP)	((_bmP)->m_bm.info.alt.nFrameCount)
#define BM_FRAMES(_bmP)			((_bmP)->m_bm.info.alt.frames)
#define BM_CURFRAME(_bmP)		((_bmP)->m_bm.info.alt.curFrame)
#define BM_OVERRIDE(_bmP)		((_bmP)->m_bm.info.std.override)
#define BM_MASK(_bmP)			((_bmP)->m_bm.info.std.mask)
#define BM_PARENT(_bmP)			((_bmP)->m_bm.info.std.parent)

#define MAX_BMP_SIZE(width, height) (4 + ((width) + 2) * (height))

//-----------------------------------------------------------------------------

class CBitmap {
	private:
		tBitmap	m_bm;
	public:
		CBitmap () { Init (); };
		~CBitmap () { Destroy (); };
		inline ubyte& operator[] (const unsigned int i) { return m_bm.buffer [i]; }
		static CBitmap* Create (ubyte mode, int w, int h, int bpp);
		ubyte* CreateBuffer (void);
		bool Setup (ubyte mode, int w, int h, int bpp = 1, ubyte* buffer = NULL);
		void Destroy (void);
		void DestroyMask (void);
		void DestroyFrames (void);
		void DestroyBuffer (void);
		void Init (void);
		void Init (int mode, int x, int y, int w, int h, int bpp = 1, ubyte *buffer = NULL);
		void InitChild (CBitmap *parent, int x, int y, int w, int h);
		CBitmap* CreateChild (int x, int y, int w, int h);
		CBitmap *FreeTexture (CBitmap *bmP);
		void FreeTexture (void);

		inline CBitmap *NextFrame (void) {
			m_bm.info.alt.curFrame++;
			if (++m_bm.info.alt.curFrame >= m_bm.info.alt.frames + m_bm.info.alt.nFrameCount)
				m_bm.info.alt.curFrame = m_bm.info.alt.frames;
			return m_bm.info.alt.curFrame;
			}

		inline CBitmap *CurFrame (int iFrame) {
			if (m_bm.nType != BM_TYPE_ALT)
				return this;
			if (iFrame < 0)
				return m_bm.info.alt.curFrame ? m_bm.info.alt.curFrame : this;
			return m_bm.info.alt.curFrame = (m_bm.info.alt.frames ? m_bm.info.alt.frames + iFrame % m_bm.info.alt.nFrameCount : this);
			}

		inline CBitmap* HasOverride (void)
			{ return (m_bm.nType == BM_TYPE_STD) ? m_bm.info.std.override :  m_bm.info.alt.curFrame; } 

		inline CBitmap *Override (int iFrame) {
			CBitmap *bmP = this;
			if (m_bm.nType == BM_TYPE_STD) {
				if (!m_bm.info.std.override)
					return this;
				bmP = m_bm.info.std.override;
				}
			return bmP->CurFrame (iFrame);
			}

		void SetPalette (CPalette *palette, int transparentColor = -1, int supertranspColor = -1, int *freq = NULL);
		void Remap (CPalette *palette, int transparentColor, int superTranspColor);
		void SetTransparent (int bTransparent);
		void SetSuperTransparent (int bTransparent);
		void CheckTransparency (void);
		int HasTransparency (void);
		int AvgColor (tRgbColorf *colorP = NULL);
		inline tRgbColorb *GetAvgColor (void) { return &m_bm.avgColor; }
		inline tRgbaColorf *GetAvgColor (tRgbaColorf *colorP) { 
			colorP->red = (float) m_bm.avgColor.red / 255.0f;
			colorP->green = (float) m_bm.avgColor.green / 255.0f;
			colorP->blue = (float) m_bm.avgColor.blue / 255.0f;
			colorP->alpha = 1.0f;
			return colorP;
			}
		int AvgColorIndex (void);

		void Swap_0_255 (void);
		void RLESwap_0_255 (void);
		int RLERemap (ubyte *colorMap, int maxLen);
		int RLEExpand (ubyte *colorMap, int bSwap0255);
		int RLECompress (void);
		void ExpandTo (CBitmap *destP);
	
		inline ubyte FrameCount (void) { return ((Type () != BM_TYPE_ALT) && Parent ()) ? Parent ()->FrameCount () : m_bm.info.alt.nFrameCount; }
		inline CBitmap *Frames (void) { return (m_bm.nType == BM_TYPE_ALT) ? m_bm.info.alt.frames : NULL; }
		inline CBitmap *CurFrame (void) { return m_bm.info.alt.curFrame; }
		inline CBitmap *Override (void) { return m_bm.info.std.override; }
		inline CBitmap *Mask (void) { return m_bm.info.std.mask; }
		inline CBitmap *Parent (void) { return m_bm.info.std.parent; }

		inline tBmProps* Props (void) { return &m_bm.props; }
		inline ushort Id (void) { return m_bm.nId; }
		inline void SetFrameCount (ubyte nFrameCount) { m_bm.info.alt.nFrameCount = nFrameCount; }
		inline void SetFrameCount (void) {  m_bm.info.alt.nFrameCount = m_bm.props.h / m_bm.props.w; }
		void SetParent (CBitmap *parent) { m_bm.info.std.parent = parent; }
		void SetMask (CBitmap *mask) { m_bm.info.std.mask = mask; }
		void SetOverride (CBitmap *override) { m_bm.info.std.override = override; }
		void SetCurFrame (CBitmap *frame) { m_bm.info.alt.curFrame = frame; }
		void SetCurFrame (int nFrame) { m_bm.info.alt.curFrame = m_bm.info.alt.frames + nFrame; }

		inline short Width (void) { return m_bm.props.w; }
		inline short Height (void) { return m_bm.props.h; }
		inline short Left (void) { return m_bm.props.x; }
		inline short Top (void) { return m_bm.props.y; }
		inline short Right (void) { return m_bm.props.x + m_bm.props.w; }
		inline short Bottom (void) { return m_bm.props.y + m_bm.props.h; }
		inline short RowSize (void) { return m_bm.props.rowSize; }
		inline ubyte Flags (void) { return m_bm.props.flags; }
		inline sbyte Mode (void) { return m_bm.props.nMode; }
		inline ubyte BPP (void) { return m_bm.nBPP; }
		inline ubyte Type (void) { return m_bm.nType; }
		inline ubyte WallAnim (void) { return m_bm.bWallAnim; }
		inline ubyte FromPog (void) { return m_bm.bFromPog; }
		inline ubyte Flat (void) { return m_bm.bFlat; }
		inline ubyte Team (void) { return m_bm.nTeam; }
		inline CTexture *Texture (void) { return m_bm.texture; }
		inline int *TransparentFrames (int i = 0) { return m_bm.transparentFrames + i; }
		inline int *SuperTranspFrames (int i = 0) { return m_bm.supertranspFrames + i; }
		inline ubyte* Buffer (unsigned int i = 0) { return m_bm.buffer.Buffer () + i; }
		inline char* Name (void) { return m_bm.szName; }
#if TEXTURE_COMPRESSION
		inline int BufSize (void) { return m_bm.buffer ? m_bm.bCompressed ? m_bm.nBufSize : (int) m_bm.props.h * (int) m_bm.props.rowSize : 0; }
#else
		inline int BufSize (void) { return m_bm.buffer.Buffer () ? (int) m_bm.props.h * (int) m_bm.props.rowSize : 0; }
#endif
		inline void SetId (ushort nId) { m_bm.nId = nId; }
		inline void SetName (const char* pszName) { strncpy (m_bm.szName, pszName, sizeof (m_bm.szName)); }
		inline void SetWidth (short w) { m_bm.props.w = w; m_bm.props.rowSize = w * m_bm.nBPP; }
		inline void SetHeight (short h) { m_bm.props.h = h; }
		inline void SetLeft (short x) { m_bm.props.x = x; }
		inline void SetTop (short y) { m_bm.props.y = y; }
		inline void SetRowSize (short rowSize) { m_bm.props.rowSize = rowSize; }
		inline void SetFlags (ubyte flags) { m_bm.props.flags = flags; }
		inline void AddFlags (ubyte flags) { m_bm.props.flags |= flags; }
		inline void DelFlags (ubyte flags) { m_bm.props.flags &= ~flags; }
		inline void SetMode (sbyte nMode) { m_bm.props.nMode = nMode; }
		inline void SetBPP (ubyte nBPP) { m_bm.nBPP = nBPP; m_bm.props.rowSize = m_bm.props.w * m_bm.nBPP; }
		inline void SetType (ubyte nType) { m_bm.nType = nType; }
		inline void SetWallAnim (ubyte bWallAnim) { m_bm.bWallAnim = bWallAnim; }
		inline void SetFromPog (ubyte bFromPog) { m_bm.bFromPog = bFromPog; }
		inline void SetFlat (ubyte bFlat) { m_bm.bFlat = bFlat; }
		inline void SetTeam (ubyte nTeam) { m_bm.nTeam = nTeam; }
		inline void SetAvgColorIndex (ubyte nIndex) { m_bm.avgColorIndex = nIndex; }
		inline void SetAvgColor (tRgbColorb& color) { m_bm.avgColor = color; }
		inline ubyte* SetBuffer (ubyte *buffer) { 
			m_bm.buffer.SetBuffer (buffer); 
			return buffer;
			}
		inline void SetTexture (CTexture *texture) { m_bm.texture = texture; }
		inline CPalette* Palette (void) { return m_bm.palette ? m_bm.palette : paletteManager.Default (); }

		CBitmap *CreateMask (void);
		int CreateMasks (void);
		int SetupFrames (int bDoMipMap, int nTransp, int bLoad);
		CBitmap* SetupTexture (int bDoMipMap, int nTransp, int bLoad);
		int LoadTexture (int dxo, int dyo, int nTransp, int superTransp);
#if RENDER2TEXTURE == 1
		int PrepareTexture (int bMipMap, int nTransp, int bMask, CBO *renderBuffer = NULL);
#elif RENDER2TEXTURE == 2
		int PrepareTexture (int bMipMap, int nTransp, int bMask, CFBO *renderBuffer = NULL);
#else
		int PrepareTexture (int bMipMap, int nTransp, int bMask, tPixelBuffer *renderBuffer = NULL);
#endif
		int Bind (int bMipMaps, int nTransp);
		inline bool Prepared (void) { return Texture () && (Texture ()->Handle () >= 0); }

#if TEXTURE_COMPRESSION
		inline ubyte Compressed (void) { return m_bm.bCompressed; }
		inline int Format (void) { return m_bm.nFormat; }
		inline SetCompressed (ubyte bCompressed) { m_bm.bCompressed = bCompressed; }
		inline void SetFormat (int nFormat) { m_bm.nFormat = nFormat; }
#else
		inline ubyte Compressed (void) { return 0; }
		inline int Format (void) { return 0; }
#endif
		void UnlinkTexture (void);
		void Unlink (int bAddon);
	};

//-----------------------------------------------------------------------------

void LoadBackgroundBitmap (void);
void GrBitmapM (int x, int y, CBitmap *bmP, int bTransp);
void GrBmUBitBltM (int w, int h, int dx, int dy, int sx, int sy, CBitmap * src, CBitmap * dest, int bTransp);

//-----------------------------------------------------------------------------

#endif //_BITMAP_H

//eof
