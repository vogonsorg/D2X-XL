//prototypes opengl functions - Added 9/15/99 Matthew Mueller
#ifndef _OGL_LIB_H
#define _OGL_LIB_H

#ifdef _WIN32
#include <windows.h>
#include <stddef.h>
#endif

#include "descent.h"
#include "vecmat.h"

//------------------------------------------------------------------------------

extern GLuint hBigSphere;
extern GLuint hSmallSphere;
extern GLuint circleh5;
extern GLuint circleh10;
extern GLuint mouseIndList;
extern GLuint cross_lh [2];
extern GLuint primary_lh [3];
extern GLuint secondary_lh [5];
extern GLuint g3InitTMU [4][2];
extern GLuint g3ExitTMU [2];

extern int r_polyc, r_tpolyc, r_bitmapc, r_ubitmapc, r_ubitbltc, r_upixelc, r_tvertexc, r_texcount;
extern int gr_renderstats, gr_badtexture;

//------------------------------------------------------------------------------

typedef struct tRenderQuality {
	int	texMinFilter;
	int	texMagFilter;
	int	bNeedMipmap;
	int	bAntiAliasing;
} tRenderQuality;

extern tRenderQuality renderQualities [];

typedef struct tSinCosf {
	float	fSin, fCos;
} tSinCosf;

//------------------------------------------------------------------------------

void OglDeleteLists (GLuint *lp, int n);
void ComputeSinCosTable (int nSides, tSinCosf *sinCosP);
int CircleListInit (int nSides, int nType, int mode);
void G3Normal (g3sPoint** pointList, CFixVector* pvNormal);
void G3CalcNormal (g3sPoint **pointList, CFloatVector *pvNormal);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#define OGLTEXBUFSIZE (4096 * 4096 * 4)

typedef struct tScreenScale {
	float x, y;
} __pack__ tScreenScale;

class COglData {
	public:
		GLubyte			buffer [OGLTEXBUFSIZE];
		CPalette*		palette;
		GLenum			nSrcBlend;
		GLenum			nDestBlend;
		float				zNear;
		float				zFar;
		CFloatVector3	depthScale;
		tScreenScale	screenScale;
		CFBO				drawBuffers [2];
		CFBO*				drawBufferP;
		CFBO				glowBuffer;
		int				nPerPixelLights [8];
		float				lightRads [8];
		CFloatVector	lightPos [8];
		int				bLightmaps;
		int				nHeadlights;
		fix				xStereoSeparation;
	public:
		COglData () { Initialize (); }
		void Initialize (void);
		inline void SelectDrawBuffer (int nSide) { drawBufferP = drawBuffers + (nSide > 0); }
};


#if DBG_OGL

typedef struct tClientBuffer {
	const GLvoid*	buffer;
	const char*		pszFile;
	int				nLine;
} tClientBuffer;

#endif

class COglStates {
	public:
		int	bInitialized;
		int	bRebuilding;
		int	bShadersOk;
		int	bMultiTexturingOk;
		int	bRender2TextureOk;
		int	bPerPixelLightingOk;
		int	bUseRender2Texture;
		int	bFullScreen;
		int	bLastFullScreen;
		int	bUseTransform;
		int	nTransformCalls;
		int	bGlTexMerge;
		int	bBrightness;
		int	bLowMemory;
		int	nColorBits;
		int	nPreloadTextures;
		ubyte	nTransparencyLimit;
		GLint	nDepthBits;
		GLint	nStencilBits;
		int	bEnableTexture2D;
		int	bEnableTexClamp;
		int	bEnableScissor;
		int	bNeedMipMaps;
		int	bFSAA;
		int	bAntiAliasing;
		int	bAntiAliasingOk;
		int	bVoodooHack;
		int	bTextureCompression;
		int	bHaveTexCompression;
		int	bHaveVBOs;
		int	texMinFilter;
		int	texMagFilter;
		int	nTexMagFilterState;
		int	nTexMinFilterState;
		int	nTexEnvModeState;
		int	nContrast;
		int	nLastX;
		int	nLastY;
		int	nLastW;
		int	nLastH;
		int	nCurWidth;
		int	nCurHeight;
		int	nLights;
		int	iLight;
		int	nFirstLight;
		int	bCurFullScreen;
		int	bpp;
		int	bScaleLight;
		int	bDynObjLight;
		int	bVertexLighting;
		int	bHeadlight;
		int	bStandardContrast;
		int	nRGBAFormat;
		int	nRGBFormat;
		int	bIntensity4;
		int	bLuminance4Alpha4;
		int	bOcclusionQuery;
		int	bDepthBlending;
		int	bUseDepthBlending;
		int	bHaveDepthBuffer;
		int	bHaveBlur;
		int	nDrawBuffer;
		int	nStencil;
	#ifdef GL_ARB_multitexture
		int	bArbMultiTexture;
	#endif
	#ifdef GL_SGIS_multitexture
		int	bSgisMultiTexture;
	#endif
		float	fAlpha;
		float	fLightRange;
		GLuint hDepthBuffer;

		int	nTMU [2];	//active driver and client TMUs
		int	clientStates [4][6];	// client states for the first 4 TMUs
#if DBG_OGL
		tClientBuffer	clientBuffers [4][6];
#endif

	public:
		COglStates () { Initialize (); }
		void Initialize (void);
};


class COGL {
	public:
		COglData		m_data;
		COglStates	m_states;

	public:
		void Initialize (void);

		void SetupExtensions (void);
		void SetupMultiTexturing (void);
		void SetupShaders (void);
		void SetupOcclusionQuery (void);
		void SetupPointSprites (void);
		void SetupTextureCompression (void);
		void SetupStencilOps (void);
		void SetupRefreshSync (void);
		void SetupAntiAliasing (void);
		void SetupVBOs (void);

		void InitState (void);
		void InitShaders (void);

		void SetupProjection (void);
		void SetupFrustum (void);
		void SelectTMU (int nTMU, bool bClient = false);
		int EnableClientState (GLuint nState, int nTMU = -1);
		int DisableClientState (GLuint nState, int nTMU = -1);
		int EnableClientStates (int bTexCoord, int bColor, int bNormals, int nTMU = -1);
		void DisableClientStates (int bTexCoord, int bColor, int bNormals, int nTMU = -1);
		void ResetClientStates (void);
		void StartFrame (int bFlat, int bResetColorBuf, fix xStereoSeparation);
		void EndFrame (void);
		void EnableLighting (int bSpecular);
		void DisableLighting (void);
		void SetRenderQuality (int nQuality = -1);
		void Viewport (int x, int y, int w, int h);
		void SwapBuffers (int bForce, int bClear);
		void SetupTransform (int bForce);
		void ResetTransform (int bForce);
		void BlendFunc (GLenum nSrcBlend, GLenum nDestBlend);
		void SetScreenMode (void);
		void GetVerInfo (void);
		GLuint CreateDepthTexture (int nTMU, int bFBO);
		GLuint CreateStencilTexture (int nTMU, int bFBO);
		void CreateDrawBuffer (void);
		void DestroyDrawBuffer (void);
		void DestroyDrawBuffers (void);
		void SetDrawBuffer (int nBuffer, int bFBO);
		void SetReadBuffer (int nBuffer, int bFBO);
		void FlushDrawBuffer (bool bAdditive = false);
		inline void SelectDrawBuffer (int nSide) { 
			m_data.SelectDrawBuffer (nSide); 
			CreateDrawBuffer ();
			}
		inline CFBO* DrawBuffer (void) { return m_data.drawBufferP; }
		void RebuildContext (int bGame);
		void DrawArrays (GLenum mode, GLint first, GLsizei count);
		void ColorMask (GLboolean bRed, GLboolean bGreen, GLboolean bBlue, GLboolean bAlpha, GLboolean bEyeOffset = GL_TRUE);
		inline int Enhance3D (int bForce = 0) { 
			return !(gameOpts->render.bUseShaders && m_states.bShadersOk)
					 ? 0
					 : !(bForce || gameOpts->render.bEnhance3D)
						 ? 0
						 : (gameOpts->render.n3DGlasses == GLASSES_AMBER_BLUE) 
							 ? 1 
							 : (gameOpts->render.n3DGlasses == GLASSES_RED_CYAN) 
								? 2 
								: 0; 
			}

#if DBG_OGL
		void VertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, const char* pszFile, int nLine);
		void ColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, const char* pszFile, int nLine);
		void TexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, const char* pszFile, int nLine);
		void NormalPointer (GLenum type, GLsizei stride, const GLvoid* pointer, const char* pszFile, int nLine);

		void GenTextures (GLsizei n, GLuint *hTextures);
		void DeleteTextures (GLsizei n, GLuint *hTextures);
		void BindTexture (GLuint handle) { glBindTexture (GL_TEXTURE_2D, handle); }

#else
		inline void VertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, const char* pszFile, int nLine)
			{ glVertexPointer (size, type, stride, pointer); }
		inline void ColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, const char* pszFile, int nLine)
			{ glColorPointer (size, type, stride, pointer); }
		inline void TexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, const char* pszFile, int nLine)
			{ glTexCoordPointer (size, type, stride, pointer); }
		inline void NormalPointer (GLenum type, GLsizei stride, const GLvoid* pointer, const char* pszFile, int nLine)
			{ glNormalPointer (type, stride, pointer); }

		inline void GenTextures (GLsizei n, GLuint *hTextures) { glGenTextures (n, hTextures); }
		inline void DeleteTextures (GLsizei n, GLuint *hTextures) { glDeleteTextures (n, hTextures); }
#endif

		inline void ClearError (int bTrapError) {
#if DBG_OGL
			GLenum nError = glGetError ();
			if (nError) {
				const char* pszError = reinterpret_cast<const char*> (gluErrorString (nError));
				PrintLog ("%s\n", pszError);
				if (bTrapError)
					nError = nError;
				}
#else
			glGetError();
#endif
			}


		inline int SetTransform (int bUseTransform) { return m_states.bUseTransform = bUseTransform; }
		inline int UseTransform (void) { return m_states.bUseTransform; }
		inline void SetStereoSeparation (fix xStereoSeparation) { m_data.xStereoSeparation = xStereoSeparation; }
		inline fix StereoSeparation (void) { return m_data.xStereoSeparation; }

		inline int HaveDrawBuffer (void) {
			return m_states.bRender2TextureOk && m_data.drawBufferP->Handle () && m_data.drawBufferP->Active ();
			}

		int StencilOff (void);
		void StencilOn (int bStencil);

		void InitEnhanced3DShader (void);
		void DeleteEnhanced3DShader (void);
};

extern COGL ogl;

//------------------------------------------------------------------------------

static inline CFloatVector3* G3GetNormal (g3sPoint *pPoint, CFloatVector *pvNormal)
{
return pPoint->p3_normal.nFaces ? pPoint->p3_normal.vNormal.XYZ () : pvNormal->XYZ ();
}

//------------------------------------------------------------------------------

static inline void OglCullFace (int bFront)
{
if (gameStates.render.bRearView /*&& (gameStates.render.nWindow != 0)*/)
	glCullFace (bFront ? GL_BACK : GL_FRONT);
else
	glCullFace (bFront ? GL_FRONT : GL_BACK);
}

//------------------------------------------------------------------------------

#endif
