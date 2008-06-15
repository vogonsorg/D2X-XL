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
#	include <conf.h>
#endif
#ifdef RCS
static char rcsid [] = "$Id: lighting.c,v 1.4 2003/10/04 03:14:47 btb Exp $";
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>	// for memset ()

#include "inferno.h"
#include "error.h"
#include "u_mem.h"
#include "fix.h"
#include "vecmat.h"
#include "network.h"
#include "ogl_defs.h"
#include "ogl_lib.h"
#include "ogl_tmu.h"
#include "ogl_color.h"
#include "ogl_shader.h"
#include "ogl_render.h"
#include "gameseg.h"
#include "endlevel.h"
#include "renderthreads.h"
#include "light.h"
#include "lightmap.h"
#include "headlight.h"
#include "dynlight.h"

#define ONLY_LIGHTMAPS 0
#define CONST_LIGHT_COUNT 1

#define PPL_AMBIENT_LIGHT	0.3f
#define PPL_DIFFUSE_LIGHT	0.7f

#define GEO_LIN_ATT	0.05f
#define GEO_QUAD_ATT	0.005f
#define OBJ_LIN_ATT	0.05f
#define OBJ_QUAD_ATT	0.005f

//------------------------------------------------------------------------------

GLhandleARB gsShaderProg [2][3] = {{0,0,0},{0,0,0}};
GLhandleARB gsf [2][3] = {{0,0,0},{0,0,0}};
GLhandleARB gsv [2][3] = {{0,0,0},{0,0,0}};

const char *grayScaleFS [2][3] = {{
	"uniform sampler2D baseTex;\r\n" \
	"uniform vec4 faceColor;\r\n" \
	"void main(void){" \
	"float l = (faceColor.r + faceColor.g + faceColor.b) / 4.0;\r\n" \
	"gl_FragColor = vec4 (l, l, l, faceColor.a);}"
	,
	"uniform sampler2D baseTex;\r\n" \
	"void main(void){" \
	"vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"float l = (texColor.r * 0.3 + texColor.g * 0.59 + texColor.b * 0.11) / 4.0;\r\n" \
	"gl_FragColor = vec4 (l, l, l, texColor.a);}"
	,
	"uniform sampler2D baseTex, decalTex;\r\n" \
	"void main(void){" \
	"vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"vec4 decalColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"float l = (texColor.r * 0.3 + texColor.g * 0.59 + texColor.b * 0.11) / 4.0;\r\n" \
	"gl_FragColor = vec4 (l, l, l, texColor.a);}"
	},
	{
	"uniform sampler2D baseTex;\r\n" \
	"uniform vec4 faceColor;\r\n" \
	"void main(void){" \
	"float l = (faceColor.r + faceColor.g + faceColor.b) / 4.0;\r\n" \
	"gl_FragColor = vec4 (l, l, l, faceColor.a);}"
	,
	"uniform sampler2D baseTex;\r\n" \
	"void main(void){" \
	"vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"float l = (texColor.r * 0.3 + texColor.g * 0.59 + texColor.b * 0.11) / 4.0;\r\n" \
	"gl_FragColor = vec4 (l, l, l, texColor.a);}"
	,
	"uniform sampler2D baseTex, decalTex;\r\n" \
	"void main(void){" \
	"vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"vec4 decalColor = texture2D (baseTex, gl_TexCoord [2].xy);\r\n" \
	"texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"float l = (texColor.r * 0.3 + texColor.g * 0.59 + texColor.b * 0.11) / 4.0;\r\n" \
	"gl_FragColor = vec4 (l, l, l, texColor.a);}"
	}};

const char *grayScaleVS [2][3] = {{
	"void main(void){" \
	"gl_Position=ftransform();"\
	"gl_FrontColor=gl_Color;}"
	,
	"void main(void){" \
	"gl_TexCoord [0]=gl_MultiTexCoord0;"\
	"gl_Position=ftransform();"\
	"gl_FrontColor=gl_Color;}"
	,
	"void main(void){" \
	"gl_TexCoord [0]=gl_MultiTexCoord0;"\
	"gl_TexCoord [1]=gl_MultiTexCoord1;"\
	"gl_Position=ftransform();"\
	"gl_FrontColor=gl_Color;}"
	},
	{
	"void main(void){" \
	"gl_Position=ftransform();"\
	"gl_FrontColor=gl_Color;}"
	,
	"void main(void){" \
	"gl_TexCoord [1]=gl_MultiTexCoord1;"\
	"gl_Position=ftransform();"\
	"gl_FrontColor=gl_Color;}"
	,
	"void main(void){" \
	"gl_TexCoord [1]=gl_MultiTexCoord1;"\
	"gl_TexCoord [2]=gl_MultiTexCoord2;"\
	"gl_Position=ftransform();"\
	"gl_FrontColor=gl_Color;}"
	}};

//-------------------------------------------------------------------------

void DeleteGrayScaleShader (void)
{
for (int i = 0; i < 2; i++) {
	for (int j = 0; j < 3; j++) {
		if (gsShaderProg [i][j]) {
			DeleteShaderProg (&gsShaderProg [i][j]);
			gsShaderProg [i][j] = 0;
			}
		}
	}
}

//-------------------------------------------------------------------------

void InitGrayScaleShader (void)
{
if (!gameStates.ogl.bShadersOk)
	gameOpts->ogl.bGlTexMerge = 0;
else {
	PrintLog ("building grayscale shader programs\n");
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 3; j++) {
			if (gsShaderProg [i][j])
				DeleteShaderProg (&gsShaderProg [i][j]);
			gameStates.render.textures.bHaveGrayScaleShader = 
				CreateShaderProg (&gsShaderProg [i][j]) &&
				CreateShaderFunc (&gsShaderProg [i][j], &gsf [i][j], &gsv [i][j], grayScaleFS [i][j], grayScaleVS [i][j], 1) &&
				LinkShaderProg (&gsShaderProg [i][j]);
			if (!gameStates.render.textures.bHaveGrayScaleShader) {
				DeleteGrayScaleShader ();
				return;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------
// per pixel lighting, no lightmaps
// 2 - 8 light sources

const char *pszPPXLightingFS [] = {
	"#define LIGHTS 8\r\n" \
	"uniform vec4 matColor;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 colorSum = vec4 (gl_Color.rgb * fLightScale, gl_Color.a);\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	int i;\r\n" \
	"	for (i = 0; i < LIGHTS; i++) if (i < nLights) {\r\n" \
	"		vec4 color;\r\n" \
	"		vec3 lightVec = vec3 (gl_LightSource [i].position) - vertPos;\r\n" \
	"		float lightDist = length (lightVec);\r\n" \
	"		float lightRad = gl_LightSource [i].specular.a;\r\n" \
	"	   float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"     float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"     float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	   if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"			color = gl_LightSource [i].diffuse + gl_LightSource [i].ambient;\r\n" \
	"		else {\r\n" \
	"			/*NdotL = 1.0 - ((1.0 - NdotL) * 0.95);*/\r\n" \
	"			att += gl_LightSource [i].linearAttenuation * dist + gl_LightSource [i].quadraticAttenuation * dist * dist;\r\n" \
	"			NdotL = max (NdotL, 0.0);\r\n" \
	"			if (lightRad > 0.0)\r\n" \
	"				NdotL += (1.0 - NdotL) / att;\r\n" \
	"			color = (gl_LightSource [i].diffuse * NdotL + gl_LightSource [i].ambient) / att;\r\n" \
	"			}\r\n" \
	"		/*specular highlight >>>>>\r\n" \
	"		if (NdotL >= 0.0) {\r\n" \
	"			vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"			float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"			color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"			}\r\n" \
	"		*/\r\n" \
	"		colorSum += color * gl_LightSource [i].constantAttenuation;\r\n" \
	"		}\r\n" \
	"	gl_FragColor = vec4 (min (matColor.rgb, matColor.rgb * colorSum.rgb), matColor.a * fLightScale);\r\n" \
	"	}"
	,
	"#define LIGHTS 8\r\n" \
	"uniform sampler2D baseTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 colorSum = vec4 (gl_Color.rgb * fLightScale, gl_Color.a);\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	int i;\r\n" \
	"	for (i = 0; i < LIGHTS; i++) if (i < nLights) {\r\n" \
	"		vec4 color;\r\n" \
	"		vec3 lightVec = vec3 (gl_LightSource [i].position) - vertPos;\r\n" \
	"		float lightDist = length (lightVec);\r\n" \
	"		float lightRad = gl_LightSource [i].specular.a;\r\n" \
	"	   float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"     float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"     float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	   if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"			color = gl_LightSource [i].diffuse + gl_LightSource [i].ambient;\r\n" \
	"		else {\r\n" \
	"			/*NdotL = 1.0 - ((1.0 - NdotL) * 0.95);*/\r\n" \
	"			att += gl_LightSource [i].linearAttenuation * dist + gl_LightSource [i].quadraticAttenuation * dist * dist;\r\n" \
	"			NdotL = max (NdotL, 0.0);\r\n" \
	"			if (lightRad > 0.0)\r\n" \
	"				NdotL += (1.0 - NdotL) / att;\r\n" \
	"			color = (gl_LightSource [i].diffuse * NdotL + gl_LightSource [i].ambient) / att;\r\n" \
	"			}\r\n" \
	"		/*specular highlight >>>>>\r\n" \
	"		if (NdotL >= 0.0) {\r\n" \
	"			vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"			float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"			color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"			}\r\n" \
	"		*/\r\n" \
	"		colorSum += color * gl_LightSource [i].constantAttenuation;\r\n" \
	"		}\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * colorSum.rgb, texColor.a * fLightScale);\r\n" \
	"	}"
	,
	"#define LIGHTS 8\r\n" \
	"uniform sampler2D baseTex, decalTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 colorSum = vec4 (gl_Color.rgb * fLightScale, gl_Color.a);\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [1].xy) ;\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	int i;\r\n" \
	"	for (i = 0; i < LIGHTS; i++) if (i < nLights) {\r\n" \
	"		vec4 color;\r\n" \
	"		vec3 lightVec = vec3 (gl_LightSource [i].position) - vertPos;\r\n" \
	"		float lightDist = length (lightVec);\r\n" \
	"		float lightRad = gl_LightSource [i].specular.a;\r\n" \
	"	   float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"     float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"     float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	   if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"			color = gl_LightSource [i].diffuse + gl_LightSource [i].ambient;\r\n" \
	"		else {\r\n" \
	"			/*NdotL = 1.0 - ((1.0 - NdotL) * 0.95);*/\r\n" \
	"			att += gl_LightSource [i].linearAttenuation * dist + gl_LightSource [i].quadraticAttenuation * dist * dist;\r\n" \
	"			NdotL = max (NdotL, 0.0);\r\n" \
	"			if (lightRad > 0.0)\r\n" \
	"				NdotL += (1.0 - NdotL) / att;\r\n" \
	"			color = (gl_LightSource [i].diffuse * NdotL + gl_LightSource [i].ambient) / att;\r\n" \
	"			}\r\n" \
	"		/*specular highlight >>>>>\r\n" \
	"		if (NdotL >= 0.0) {\r\n" \
	"			vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"			float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"			color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"			}\r\n" \
	"		*/\r\n" \
	"		colorSum += color * gl_LightSource [i].constantAttenuation;\r\n" \
	"		}\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * colorSum.rgb, texColor.a * fLightScale);\r\n" \
	"	}"
	,
	"#define LIGHTS 8\r\n" \
	"uniform sampler2D baseTex, decalTex, maskTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"float bMask = texture2D (maskTex, gl_TexCoord [2].xy).r;\r\n" \
	"if (bMask < 0.5)\r\n" \
	"  discard;\r\n" \
	"else {\r\n" \
	"	vec4 colorSum = vec4 (gl_Color.rgb * fLightScale, gl_Color.a);\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [1].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	int i;\r\n" \
	"	for (i = 0; i < LIGHTS; i++) if (i < nLights) {\r\n" \
	"		vec4 color;\r\n" \
	"		vec3 lightVec = vec3 (gl_LightSource [i].position) - vertPos;\r\n" \
	"		float lightDist = length (lightVec);\r\n" \
	"		float lightRad = gl_LightSource [i].specular.a;\r\n" \
	"	   float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"     float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"     float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	   if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"			color = gl_LightSource [i].diffuse + gl_LightSource [i].ambient;\r\n" \
	"		else {\r\n" \
	"			/*NdotL = 1.0 - ((1.0 - NdotL) * 0.95);*/\r\n" \
	"			att += gl_LightSource [i].linearAttenuation * dist + gl_LightSource [i].quadraticAttenuation * dist * dist;\r\n" \
	"			NdotL = max (NdotL, 0.0);\r\n" \
	"			if (lightRad > 0.0)\r\n" \
	"				NdotL += (1.0 - NdotL) / att;\r\n" \
	"			color = (gl_LightSource [i].diffuse * NdotL + gl_LightSource [i].ambient) / att;\r\n" \
	"			}\r\n" \
	"		/*specular highlight >>>>>\r\n" \
	"		if (NdotL >= 0.0) {\r\n" \
	"			vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"			float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"			color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"			}\r\n" \
	"		*/\r\n" \
	"		colorSum += color * gl_LightSource [i].constantAttenuation;\r\n" \
	"		}\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * colorSum.rgb, texColor.a * fLightScale);\r\n" \
	"	}\r\n" \
	"}"
	};

// ----------------------------------------------------------------------------------------------
// one light source

const char *pszPP1LightingFS [] = {
	"uniform vec4 matColor;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = vec4 (gl_Color.rgb * fLightScale, gl_Color.a);\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	vec3 lightVec = vec3 (gl_LightSource [0].position) - vertPos;\r\n" \
	"	float lightDist = length (lightVec);\r\n" \
	"	float lightRad = gl_LightSource [0].specular.a;\r\n" \
	"	float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"  float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"  float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"		color = gl_LightSource [0].diffuse + gl_LightSource [0].ambient;\r\n" \
	"	else {\r\n" \
	"		att += gl_LightSource [0].linearAttenuation * dist + gl_LightSource [0].quadraticAttenuation * dist * dist;\r\n" \
	"		NdotL = max (NdotL, 0.0);\r\n" \
	"		if (lightRad > 0.0)\r\n" \
	"			NdotL += (1.0 - NdotL) / att;\r\n" \
	"		color = (gl_LightSource [0].diffuse * NdotL + gl_LightSource [0].ambient) / att;\r\n" \
	"		}\r\n" \
	"	/*specular highlight >>>>>\r\n" \
	"	if (NdotL >= 0.0) {\r\n" \
	"		vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"		float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"		color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"		}\r\n" \
	"	<<<<< specular highlight*/\r\n" \
	"	gl_FragColor = vec4 (min (matColor.rgb, matColor.rgb * color.rgb * gl_LightSource [0].constantAttenuation), color.a * fLightScale);\r\n" \
	"	}"
	,
	"uniform sampler2D baseTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = vec4 (gl_Color.rgb * fLightScale, gl_Color.a);\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	vec3 lightVec = vec3 (gl_LightSource [0].position) - vertPos;\r\n" \
	"	float lightDist = length (lightVec);\r\n" \
	"	float lightRad = gl_LightSource [0].specular.a;\r\n" \
	"	float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"  float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"  float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"		color = gl_LightSource [0].diffuse + gl_LightSource [0].ambient;\r\n" \
	"	else {\r\n" \
	"		att += gl_LightSource [0].linearAttenuation * dist + gl_LightSource [0].quadraticAttenuation * dist * dist;\r\n" \
	"		NdotL = max (NdotL, 0.0);\r\n" \
	"		if (lightRad > 0.0)\r\n" \
	"			NdotL += (1.0 - NdotL) / att;\r\n" \
	"		color = (gl_LightSource [0].diffuse * NdotL + gl_LightSource [0].ambient) / att;\r\n" \
	"		}\r\n" \
	"	/*specular highlight >>>>>\r\n" \
	"	if (NdotL >= 0.0) {\r\n" \
	"		vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"		float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"		color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"		}\r\n" \
	"	<<<<< specular highlight*/\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb * gl_LightSource [0].constantAttenuation, texColor.a * fLightScale);\r\n" \
	"	}"
	,
	"uniform sampler2D baseTex, decalTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = vec4 (gl_Color.rgb * fLightScale, gl_Color.a);\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [1].xy) ;\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	vec3 lightVec = vec3 (gl_LightSource [0].position) - vertPos;\r\n" \
	"	float lightDist = length (lightVec);\r\n" \
	"	float lightRad = gl_LightSource [0].specular.a;\r\n" \
	"	float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"  float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"  float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"		color = gl_LightSource [0].diffuse + gl_LightSource [0].ambient;\r\n" \
	"	else {\r\n" \
	"		att += gl_LightSource [0].linearAttenuation * dist + gl_LightSource [0].quadraticAttenuation * dist * dist;\r\n" \
	"		NdotL = max (NdotL, 0.0);\r\n" \
	"		if (lightRad > 0.0)\r\n" \
	"			NdotL += (1.0 - NdotL) / att;\r\n" \
	"		color = (gl_LightSource [0].diffuse * NdotL + gl_LightSource [0].ambient) / att;\r\n" \
	"		}\r\n" \
	"	/*specular highlight >>>>>\r\n" \
	"	if (NdotL >= 0.0) {\r\n" \
	"		vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"		float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"		color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"		}\r\n" \
	"	<<<<< specular highlight*/\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb * gl_LightSource [0].constantAttenuation, texColor.a * fLightScale);\r\n" \
	"	}"
	,
	"uniform sampler2D baseTex, decalTex, maskTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"float bMask = texture2D (maskTex, gl_TexCoord [2].xy).r;\r\n" \
	"if (bMask < 0.5)\r\n" \
	"  discard;\r\n" \
	"else {\r\n" \
	"	vec4 color = vec4 (gl_Color.rgb * fLightScale, gl_Color.a);\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [1].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	vec3 lightVec = vec3 (gl_LightSource [0].position) - vertPos;\r\n" \
	"	float lightDist = length (lightVec);\r\n" \
	"	float lightRad = gl_LightSource [0].specular.a;\r\n" \
	"	float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"  float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"  float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"		color = gl_LightSource [0].diffuse + gl_LightSource [0].ambient;\r\n" \
	"	else {\r\n" \
	"		att += gl_LightSource [0].linearAttenuation * dist + gl_LightSource [0].quadraticAttenuation * dist * dist;\r\n" \
	"		NdotL = max (NdotL, 0.0);\r\n" \
	"		if (lightRad > 0.0)\r\n" \
	"			NdotL += (1.0 - NdotL) / att;\r\n" \
	"		color = (gl_LightSource [0].diffuse * NdotL + gl_LightSource [0].ambient) / att;\r\n" \
	"		}\r\n" \
	"	/*specular highlight >>>>>\r\n" \
	"	if (NdotL >= 0.0) {\r\n" \
	"		vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"		float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"		color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"		}\r\n" \
	"	<<<<< specular highlight*/\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb * gl_LightSource [0].constantAttenuation, texColor.a * fLightScale);}\r\n" \
	"	}"
	};

// ----------------------------------------------------------------------------------------------

const char *pszPPLightingVS [] = {
	"#define LIGHTS 8\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	normal = normalize (gl_NormalMatrix * gl_Normal);\r\n" \
	"	vertPos = vec3 (gl_ModelViewMatrix * gl_Vertex);\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"#define LIGHTS 8\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	normal = normalize (gl_NormalMatrix * gl_Normal);\r\n" \
	"	vertPos = vec3 (gl_ModelViewMatrix * gl_Vertex);\r\n" \
	"	gl_Position = ftransform();\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"#define LIGHTS 8\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	normal = normalize (gl_NormalMatrix * gl_Normal);\r\n" \
	"	vertPos = vec3 (gl_ModelViewMatrix * gl_Vertex);\r\n" \
	"	gl_Position = ftransform();\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"#define LIGHTS 8\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	normal = normalize (gl_NormalMatrix * gl_Normal);\r\n" \
	"	vertPos = vec3 (gl_ModelViewMatrix * gl_Vertex);\r\n" \
	"	gl_Position = ftransform();\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
	"	gl_TexCoord [2] = gl_MultiTexCoord2;\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	};
	
//-------------------------------------------------------------------------

const char *pszLightingFS [] = {
	"void main() {\r\n" \
	"gl_FragColor = gl_Color;\r\n" \
	"	}"
	,
	"uniform sampler2D baseTex;\r\n" \
	"void main() {\r\n" \
	"	gl_FragColor = texture2D (baseTex, gl_TexCoord [0].xy) * gl_Color;\r\n" \
	"	}"
	,
	"uniform sampler2D baseTex, decalTex;\r\n" \
	"void main() {\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [1].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	gl_FragColor = texColor * gl_Color;\r\n" \
	"	}"
	,
	"uniform sampler2D baseTex, decalTex, maskTex;\r\n" \
	"void main() {\r\n" \
	"float bMask = texture2D (maskTex, gl_TexCoord [2].xy).r;\r\n" \
	"if (bMask < 0.5)\r\n" \
	"  discard;\r\n" \
	"else {\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [0].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [1].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	gl_FragColor = texColor * gl_Color;}\r\n" \
	"	}"
	};


const char *pszLightingVS [] = {
	"void main() {\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"void main() {\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"void main() {\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"void main() {\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
	"	gl_TexCoord [2] = gl_MultiTexCoord2;\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	};
	
// ----------------------------------------------------------------------------------------------

const char *pszPPXLMLightingFS [] = {
	"#define LIGHTS 8\r\n" \
	"uniform sampler2D lMapTex;\r\n" \
	"uniform vec4 matColor;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 colorSum = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	int i;\r\n" \
	"	for (i = 0; i < LIGHTS; i++) if (i < nLights) {\r\n" \
	"		vec4 color;\r\n" \
	"		vec3 lightVec = vec3 (gl_LightSource [i].position) - vertPos;\r\n" \
	"		float lightDist = length (lightVec);\r\n" \
	"		float lightRad = gl_LightSource [i].specular.a;\r\n" \
	"	   float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"     float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"     float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	   if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"			color = gl_LightSource [i].diffuse + gl_LightSource [i].ambient;\r\n" \
	"		else {\r\n" \
	"			att += gl_LightSource [i].linearAttenuation * dist + gl_LightSource [i].quadraticAttenuation * dist * dist;\r\n" \
	"			NdotL = max (NdotL, 0.0);\r\n" \
	"			if (lightRad > 0.0)\r\n" \
	"				NdotL += (1.0 - NdotL) / att;\r\n" \
	"			color = (gl_LightSource [i].diffuse * NdotL + gl_LightSource [i].ambient) / att;\r\n" \
	"			}\r\n" \
	"		/*specular highlight >>>>>\r\n" \
	"		if (NdotL >= 0.0) {\r\n" \
	"			vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"			float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"			color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"			}\r\n" \
	"		<<<<< specular highlight*/\r\n" \
	"		colorSum += color * gl_LightSource [i].constantAttenuation;\r\n" \
	"		}\r\n" \
	"	gl_FragColor = vec4 (min (matColor.rgb, matColor.rgb * colorSum.rgb), matColor.a * gl_Color.a * fLightScale);\r\n" \
	"	}"
	,
	"#define LIGHTS 8\r\n" \
	"uniform sampler2D lMapTex, baseTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 colorSum = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	int i;\r\n" \
	"	for (i = 0; i < LIGHTS; i++) if (i < nLights) {\r\n" \
	"		vec4 color;\r\n" \
	"		vec3 lightVec = vec3 (gl_LightSource [i].position) - vertPos;\r\n" \
	"		float lightDist = length (lightVec);\r\n" \
	"		float lightRad = gl_LightSource [i].specular.a;\r\n" \
	"	   float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"     float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"     float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	   if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"			color = gl_LightSource [i].diffuse + gl_LightSource [i].ambient;\r\n" \
	"		else {\r\n" \
	"			att += gl_LightSource [i].linearAttenuation * dist + gl_LightSource [i].quadraticAttenuation * dist * dist;\r\n" \
	"			NdotL = max (NdotL, 0.0);\r\n" \
	"			if (lightRad > 0.0)\r\n" \
	"				NdotL += (1.0 - NdotL) / att;\r\n" \
	"			color = (gl_LightSource [i].diffuse * NdotL + gl_LightSource [i].ambient) / att;\r\n" \
	"			}\r\n" \
	"		/*specular highlight >>>>>\r\n" \
	"		if (NdotL >= 0.0) {\r\n" \
	"			vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"			float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"			color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"			}\r\n" \
	"		<<<<< specular highlight*/\r\n" \
	"		colorSum += color * gl_LightSource [i].constantAttenuation;\r\n" \
	"		}\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * colorSum.rgb, texColor.a * gl_Color.a * fLightScale);\r\n" \
	"	}"
	,
	"#define LIGHTS 8\r\n" \
	"uniform sampler2D lMapTex, baseTex, decalTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 colorSum = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [2].xy) ;\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	int i;\r\n" \
	"	for (i = 0; i < LIGHTS; i++) if (i < nLights) {\r\n" \
	"     vec4 color;\r\n" \
	"		vec3 lightVec = vec3 (gl_LightSource [i].position) - vertPos;\r\n" \
	"		float lightDist = length (lightVec);\r\n" \
	"		float lightRad = gl_LightSource [i].specular.a;\r\n" \
	"	   float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"     float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"     float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	   if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"			color = gl_LightSource [i].diffuse + gl_LightSource [i].ambient;\r\n" \
	"		else {\r\n" \
	"			att += gl_LightSource [i].linearAttenuation * dist + gl_LightSource [i].quadraticAttenuation * dist * dist;\r\n" \
	"			NdotL = max (NdotL, 0.0);\r\n" \
	"			if (lightRad > 0.0)\r\n" \
	"				NdotL += (1.0 - NdotL) / att;\r\n" \
	"			color = (gl_LightSource [i].diffuse * NdotL + gl_LightSource [i].ambient) / att;\r\n" \
	"			}\r\n" \
	"		/*specular highlight >>>>>\r\n" \
	"		if (NdotL >= 0.0) {\r\n" \
	"			vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"			float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"			color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"			}\r\n" \
	"		<<<<< specular highlight*/\r\n" \
	"		colorSum += color * gl_LightSource [i].constantAttenuation;\r\n" \
	"		}\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * colorSum.rgb, texColor.a * gl_Color.a * fLightScale);\r\n" \
	"	}"
	,
	"#define LIGHTS 8\r\n" \
	"uniform sampler2D lMapTex, baseTex, decalTex, maskTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"float bMask = texture2D (maskTex, gl_TexCoord [3].xy).r;\r\n" \
	"if (bMask < 0.5)\r\n" \
	"  discard;\r\n" \
	"else {\r\n" \
	"	vec4 colorSum = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [2].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	int i;\r\n" \
	"	for (i = 0; i < LIGHTS; i++) if (i < nLights) {\r\n" \
	"		vec4 color;\r\n" \
	"		vec3 lightVec = vec3 (gl_LightSource [i].position) - vertPos;\r\n" \
	"		float lightDist = length (lightVec);\r\n" \
	"		float lightRad = gl_LightSource [i].specular.a;\r\n" \
	"	   float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"     float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"     float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	   if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"			color = gl_LightSource [i].diffuse + gl_LightSource [i].ambient;\r\n" \
	"		else {\r\n" \
	"			att += gl_LightSource [i].linearAttenuation * dist + gl_LightSource [i].quadraticAttenuation * dist * dist;\r\n" \
	"			NdotL = max (NdotL, 0.0);\r\n" \
	"			if (lightRad > 0.0)\r\n" \
	"				NdotL += (1.0 - NdotL) / att;\r\n" \
	"			color = (gl_LightSource [i].diffuse * NdotL + gl_LightSource [i].ambient) / att;\r\n" \
	"			}\r\n" \
	"		/*specular highlight >>>>>\r\n" \
	"		if (NdotL >= 0.0) {\r\n" \
	"			vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"			float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"			color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"			}\r\n" \
	"		<<<<< specular highlight*/\r\n" \
	"		colorSum += color * gl_LightSource [i].constantAttenuation;\r\n" \
	"		}\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * colorSum.rgb, texColor.a * gl_Color.a * fLightScale);}\r\n" \
	"	}"
	};


// ----------------------------------------------------------------------------------------------

const char *pszPP1LMLightingFS [] = {
	"uniform sampler2D lMapTex;\r\n" \
	"uniform vec4 matColor;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	vec3 lightVec = vec3 (gl_LightSource [0].position) - vertPos;\r\n" \
	"	float lightDist = length (lightVec);\r\n" \
	"	float lightRad = gl_LightSource [0].specular.a;\r\n" \
	"	float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"  float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"  float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"		color = gl_LightSource [0].diffuse + gl_LightSource [0].ambient;\r\n" \
	"	else {\r\n" \
	"		att += gl_LightSource [0].linearAttenuation * dist + gl_LightSource [0].quadraticAttenuation * dist * dist;\r\n" \
	"		NdotL = max (NdotL, 0.0);\r\n" \
	"		if (lightRad > 0.0)\r\n" \
	"			NdotL += (1.0 - NdotL) / att;\r\n" \
	"		color += (gl_LightSource [0].diffuse * NdotL + gl_LightSource [0].ambient) / att;\r\n" \
	"		}\r\n" \
	"	/*specular highlight >>>>>\r\n" \
	"	if (NdotL >= 0.0) {\r\n" \
	"		vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"		float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"		color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"		}\r\n" \
	"	<<<<< specular highlight*/\r\n" \
	"	color *= gl_LightSource [0].constantAttenuation;\r\n" \
	"	gl_FragColor = vec4 (min (matColor.rgb, matColor.rgb * color.rgb), matColor.a * gl_Color.a * fLightScale);\r\n" \
	"	}"
	,
	"uniform sampler2D lMapTex, baseTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	vec3 lightVec = vec3 (gl_LightSource [0].position) - vertPos;\r\n" \
	"	float lightDist = length (lightVec);\r\n" \
	"	float lightRad = gl_LightSource [0].specular.a;\r\n" \
	"	float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"  float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"  float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"		color += gl_LightSource [0].diffuse + gl_LightSource [0].ambient;\r\n" \
	"	else {\r\n" \
	"		att += gl_LightSource [0].linearAttenuation * dist + gl_LightSource [0].quadraticAttenuation * dist * dist;\r\n" \
	"		NdotL = max (NdotL, 0.0);\r\n" \
	"		if (lightRad > 0.0)\r\n" \
	"			NdotL += (1.0 - NdotL) / att;\r\n" \
	"		color += (gl_LightSource [0].diffuse * NdotL + gl_LightSource [0].ambient) / att;\r\n" \
	"		}\r\n" \
	"	/*specular highlight >>>>>\r\n" \
	"	if (NdotL >= 0.0) {\r\n" \
	"		vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"		float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"		color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"		}\r\n" \
	"	<<<<< specular highlight*/\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb * gl_LightSource [0].constantAttenuation, texColor.a * gl_Color.a * fLightScale);\r\n" \
	"	}"
	,
	"uniform sampler2D lMapTex, baseTex, decalTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [2].xy) ;\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	vec3 lightVec = vec3 (gl_LightSource [0].position) - vertPos;\r\n" \
	"	float lightDist = length (lightVec);\r\n" \
	"	float lightRad = gl_LightSource [0].specular.a;\r\n" \
	"	float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"  float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"  float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"		color += gl_LightSource [0].diffuse + gl_LightSource [0].ambient;\r\n" \
	"	else {\r\n" \
	"		att += gl_LightSource [0].linearAttenuation * dist + gl_LightSource [0].quadraticAttenuation * dist * dist;\r\n" \
	"		NdotL = max (NdotL, 0.0);\r\n" \
	"		if (lightRad > 0.0)\r\n" \
	"			NdotL += (1.0 - NdotL) / att;\r\n" \
	"		color += (gl_LightSource [0].diffuse * NdotL + gl_LightSource [0].ambient) / att;\r\n" \
	"		}\r\n" \
	"	/*specular highlight >>>>>\r\n" \
	"	if (NdotL >= 0.0) {\r\n" \
	"		vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"		float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"		color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"		}\r\n" \
	"	<<<<< specular highlight*/\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb * gl_LightSource [0].constantAttenuation, texColor.a * gl_Color.a * fLightScale);\r\n" \
	"	}"
	,
	"uniform sampler2D lMapTex, baseTex, decalTex, maskTex;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"float bMask = texture2D (maskTex, gl_TexCoord [3].xy).r;\r\n" \
	"if (bMask < 0.5)\r\n" \
	"  discard;\r\n" \
	"else {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [2].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	vec3 n = normalize (normal);\r\n" \
	"	vec3 lightVec = vec3 (gl_LightSource [0].position) - vertPos;\r\n" \
	"	float lightDist = length (lightVec);\r\n" \
	"	float lightRad = gl_LightSource [0].specular.a;\r\n" \
	"	float dist = max (lightDist - lightRad, 0.0);\r\n" \
	"  float att = 1.0, NdotL = dot (n, lightVec / lightDist);\r\n" \
	"  float nMinDot = ((NdotL <= 0) && (dot (n, lightVec) <= 0)) ? 0.0 : -0.1;\r\n" \
	"	if ((NdotL >= nMinDot) && (dist == 0.0))\r\n" \
	"		color += gl_LightSource [0].diffuse + gl_LightSource [0].ambient;\r\n" \
	"	else {\r\n" \
	"		att += gl_LightSource [0].linearAttenuation * dist + gl_LightSource [0].quadraticAttenuation * dist * dist;\r\n" \
	"		NdotL = max (NdotL, 0.0);\r\n" \
	"		if (lightRad > 0.0)\r\n" \
	"			NdotL += (1.0 - NdotL) / att;\r\n" \
	"		color += (gl_LightSource [0].diffuse * NdotL + gl_LightSource [0].ambient) / att;\r\n" \
	"		}\r\n" \
	"	/*specular highlight >>>>>\r\n" \
	"	if (NdotL >= 0.0) {\r\n" \
	"		vec3 halfV = normalize (gl_LightSource [i].halfVector.xyz);\r\n" \
	"		float NdotHV = max (dot (n, halfV), 0.0);\r\n" \
	"		color += (gl_LightSource [i].specular * pow (NdotHV, 16.0)) / att;\r\n" \
	"		}\r\n" \
	"	<<<<< specular highlight*/\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb * gl_LightSource [0].constantAttenuation, texColor.a * gl_Color.a * fLightScale);}\r\n" \
	"	}"
	};

// ----------------------------------------------------------------------------------------------

const char *pszPPLMLightingVS [] = {
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	normal = normalize (gl_NormalMatrix * gl_Normal);\r\n" \
	"	vertPos = vec3 (gl_ModelViewMatrix * gl_Vertex);\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	normal = normalize (gl_NormalMatrix * gl_Normal);\r\n" \
	"	vertPos = vec3 (gl_ModelViewMatrix * gl_Vertex);\r\n" \
	"	gl_Position = ftransform();\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	normal = normalize (gl_NormalMatrix * gl_Normal);\r\n" \
	"	vertPos = vec3 (gl_ModelViewMatrix * gl_Vertex);\r\n" \
	"	gl_Position = ftransform();\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
	"	gl_TexCoord [2] = gl_MultiTexCoord2;\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"varying vec3 normal, vertPos;\r\n" \
	"void main() {\r\n" \
	"	normal = normalize (gl_NormalMatrix * gl_Normal);\r\n" \
	"	vertPos = vec3 (gl_ModelViewMatrix * gl_Vertex);\r\n" \
	"	gl_Position = ftransform();\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
	"	gl_TexCoord [2] = gl_MultiTexCoord2;\r\n" \
	"	gl_TexCoord [3] = gl_MultiTexCoord3;\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	};
	
//-------------------------------------------------------------------------

const char *pszPP0LMLightingFS [] = {
#if ONLY_LIGHTMAPS
	"uniform sampler2D lMapTex;\r\n" \
	"uniform vec4 matColor;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"void main() {\r\n" \
	"gl_FragColor = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"}"
#else
	"uniform sampler2D lMapTex;\r\n" \
	"uniform vec4 matColor;\r\n" \
	"uniform float fLightScale, nLights;\r\n" \
	"void main() {\r\n" \
	"vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy) * fLightScale;\r\n" \
	"gl_FragColor = /*min (texColor,*/ vec4 (min (matColor.rgb, matColor.rgb * color.rgb), matColor.a * gl_Color.a * fLightScale);\r\n" \
	"}"
#endif
	,
	"uniform sampler2D lMapTex, baseTex;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy);\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb, texColor.a * gl_Color.a);\r\n" \
	"	}"
	,
	"uniform sampler2D lMapTex, baseTex, decalTex;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy);\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [2].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb, texColor.a * gl_Color.a);\r\n" \
	"	}"
	,
	"uniform sampler2D lMapTex, baseTex, decalTex, maskTex;\r\n" \
	"void main() {\r\n" \
	"float bMask = texture2D (maskTex, gl_TexCoord [3].xy).r;\r\n" \
	"if (bMask < 0.5)\r\n" \
	"  discard;\r\n" \
	"else {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy);\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [2].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb, texColor.a * gl_Color.a);\r\n" \
	"	}\r\n" \
	"}"
	};


//-------------------------------------------------------------------------

const char *pszLMLightingFS [] = {
	"uniform sampler2D lMapTex;\r\n" \
	"uniform vec4 matColor;\r\n" \
	"void main() {\r\n" \
	"vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy) + gl_Color;\r\n" \
	"gl_FragColor = vec4 (min (matColor.rgb, matColor.rgb * color.rgb), matColor.a * gl_Color.a);\r\n" \
	"}"
	,
	"uniform sampler2D lMapTex, baseTex;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy) + gl_Color;\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb, texColor.a * gl_Color.a);\r\n" \
	"	}"
	,
	"uniform sampler2D lMapTex, baseTex, decalTex;\r\n" \
	"void main() {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy) + gl_Color;\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [2].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb, texColor.a * gl_Color.a);\r\n" \
	"	}"
	,
	"uniform sampler2D lMapTex, baseTex, decalTex, maskTex;\r\n" \
	"void main() {\r\n" \
	"float bMask = texture2D (maskTex, gl_TexCoord [3].xy).r;\r\n" \
	"if (bMask < 0.5)\r\n" \
	"  discard;\r\n" \
	"else {\r\n" \
	"	vec4 color = texture2D (lMapTex, gl_TexCoord [0].xy) + gl_Color;\r\n" \
	"	vec4 texColor = texture2D (baseTex, gl_TexCoord [1].xy);\r\n" \
	"  vec4 decalColor = texture2D (decalTex, gl_TexCoord [2].xy);\r\n" \
	"	texColor = vec4 (vec3 (mix (texColor, decalColor, decalColor.a)), (texColor.a + decalColor.a));\r\n" \
	"	gl_FragColor = vec4 (texColor.rgb * color.rgb, texColor.a * gl_Color.a);\r\n" \
	"	}\r\n" \
	"}"
	};

//-------------------------------------------------------------------------

const char *pszLMLightingVS [] = {
	"void main() {\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"void main() {\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"void main() {\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
	"	gl_TexCoord [2] = gl_MultiTexCoord2;\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	,
	"void main() {\r\n" \
	"	gl_TexCoord [0] = gl_MultiTexCoord0;\r\n" \
	"	gl_TexCoord [1] = gl_MultiTexCoord1;\r\n" \
	"	gl_TexCoord [2] = gl_MultiTexCoord2;\r\n" \
	"	gl_TexCoord [3] = gl_MultiTexCoord3;\r\n" \
	"	gl_Position = ftransform();\r\n" \
   "	gl_FrontColor = gl_Color;\r\n" \
	"	}"
	};
	
//-------------------------------------------------------------------------

char *BuildLightingShader (const char *pszTemplate, int nLights)
{
	int	l = (int) strlen (pszTemplate) + 1;
	char	*pszFS, szLights [2];

if (!(pszFS = (char *) D2_ALLOC (l)))
	return NULL;
if (nLights > MAX_LIGHTS_PER_PIXEL)
	nLights = MAX_LIGHTS_PER_PIXEL;
memcpy (pszFS, pszTemplate, l);
if (strstr (pszFS, "#define LIGHTS ") == pszFS) {
	sprintf (szLights, "%d", nLights);
	pszFS [15] = *szLights;
	}
#ifdef _DEBUG
PrintLog (" \n\nShader program:\n");
PrintLog (pszFS);
PrintLog (" \n");
#endif
return pszFS;
}

//-------------------------------------------------------------------------

GLhandleARB perPixelLightingShaderProgs [9][4] = 
	{{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}};

GLhandleARB ppLvs [9][4] = 
	{{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}}; 

GLhandleARB ppLfs [9][4] = 
	{{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}}; 

int InitPerPixelLightingShader (int nType, int nLights)
{
	int	h, i, j, bOk;
	char	*pszFS, *pszVS;
	const char	**fsP, **vsP;

if (!(gameStates.ogl.bShadersOk && gameStates.render.bUsePerPixelLighting))
	gameStates.ogl.bPerPixelLightingOk =
	gameStates.render.bPerPixelLighting = 0;
if (!gameStates.render.bPerPixelLighting)
	return -1;
if (perPixelLightingShaderProgs [nLights][nType])
	return nLights;
for (h = 0; h <= 3; h++) {
	for (i = 0; i <= gameStates.render.nMaxLightsPerPass; i++) {
		if (perPixelLightingShaderProgs [i][h])
			continue;
		if (HaveLightmaps ()) {
			if (i) {
				fsP = (i == 1) ? pszPP1LMLightingFS : pszPPXLMLightingFS;
				vsP = pszPPLMLightingVS;
				//nLights = MAX_LIGHTS_PER_PIXEL;
				}
			else {
				fsP = pszPP0LMLightingFS;
				vsP = pszPPLMLightingVS;
				}
			}
		else {
			if (i) {
				fsP = (i == 1) ? pszPP1LightingFS : pszPPXLightingFS;
				vsP = pszPPLightingVS;
				//nLights = MAX_LIGHTS_PER_PIXEL;
				}
			else {
				fsP = pszLightingFS;
				vsP = pszLightingVS;
				}
			}
		PrintLog ("building lighting shader programs\n");
		if ((gameStates.ogl.bPerPixelLightingOk = (gameStates.ogl.bShadersOk && gameOpts->render.nPath))) {
			pszFS = BuildLightingShader (fsP [h], i);
			pszVS = BuildLightingShader (vsP [h], i);
			bOk = (pszFS != NULL) && (pszVS != NULL) &&
					CreateShaderProg (perPixelLightingShaderProgs [i] + h) &&
					CreateShaderFunc (perPixelLightingShaderProgs [i] + h, ppLfs [i] + h, ppLvs [i] + h, pszFS, pszVS, 1) &&
					LinkShaderProg (perPixelLightingShaderProgs [i] + h);
			D2_FREE (pszFS);
			D2_FREE (pszVS);
			if (!bOk) {
				gameStates.ogl.bPerPixelLightingOk =
				gameStates.render.bPerPixelLighting = 0;
				for (i = 0; i <= MAX_LIGHTS_PER_PIXEL; i++)
					for (j = 0; j < 4; j++)
						DeleteShaderProg (perPixelLightingShaderProgs [i] + j);
				nLights = 0;
				return -1;
				}
			}
		}
	}
return gameData.render.ogl.nPerPixelLights [nType] = nLights;
}

// ----------------------------------------------------------------------------------------------

void ResetPerPixelLightingShaders (void)
{
memset (perPixelLightingShaderProgs, 0, sizeof (perPixelLightingShaderProgs));
}

// ----------------------------------------------------------------------------------------------

GLhandleARB lightmapShaderProgs [4] = {0,0,0,0};
GLhandleARB lmLvs [4] = {0,0,0,0}; 
GLhandleARB lmLfs [4] = {0,0,0,0}; 

int InitLightmapShader (int nType)
{
	int	h, j, bOk;

if (!gameStates.ogl.bShadersOk) {
	gameStates.render.bPerPixelLighting = 0;
	return 0;
	}
if (lightmapShaderProgs [nType])
	return 1;
for (h = 0; h <= 3; h++) {
	if (lightmapShaderProgs [h])
		continue;
	PrintLog ("building lightmap shader programs\n");
	bOk = CreateShaderProg (lightmapShaderProgs + h) &&
			CreateShaderFunc (lightmapShaderProgs + h, lmLfs + h, lmLvs + h, pszLMLightingFS [h], pszLMLightingVS [h], 1) &&
			LinkShaderProg (lightmapShaderProgs + h);
	if (!bOk) {
		gameStates.render.bPerPixelLighting = 0;
		for (j = 0; j < 4; j++)
			DeleteShaderProg (lightmapShaderProgs + j);
		return -1;
		}
	}
return 1;
}

// ----------------------------------------------------------------------------------------------

void ResetLightmapShaders (void)
{
memset (lightmapShaderProgs, 0, sizeof (lightmapShaderProgs));
}

//------------------------------------------------------------------------------

#ifdef _DEBUG
int CheckUsedLights2 (void);
#endif

int SetupHardwareLighting (grsFace *faceP, int nType)
{
PROF_START
	int						nLightRange, nLights;
	float						fBrightness;
	tRgbaColorf				ambient, diffuse;
#if 0
	tRgbaColorf				black = {0,0,0,0};
#endif
	tRgbaColorf				specular = {0.5f,0.5f,0.5f,0.5f};
	//fVector					vPos = {{0,0,0,1}};
	GLenum					hLight;
	tActiveShaderLight	*activeLightsP;
	tShaderLight			*psl;
	tShaderLightIndex		*sliP = &gameData.render.lights.dynamic.shader.index [0][0];

	static fVector3 vNoDir = {{0, 0, 0}};

#ifdef _DEBUG
if (faceP && (faceP->nSegment == nDbgSeg) && ((nDbgSide < 0) || (faceP->nSide == nDbgSide)))
	nDbgSeg = nDbgSeg;
if (faceP - FACES == nDbgFace)
	nDbgFace = nDbgFace;
#endif
if (!gameStates.ogl.iLight) {
#ifdef _DEBUG
	if (faceP && (faceP->nSegment == nDbgSeg) && ((nDbgSide < 0) || (faceP->nSide == nDbgSide)))
		nDbgSeg = nDbgSeg;
	if (faceP - FACES == nDbgFace)
		nDbgFace = nDbgFace;
#endif
#if ONLY_LIGHTMAPS == 2
	gameStates.ogl.nLights = 0;
#else
	gameStates.ogl.nLights = SetNearestFaceLights (faceP, nType != 0);
#endif
	if (gameStates.ogl.nLights > gameStates.render.nMaxLightsPerFace)
		gameStates.ogl.nLights = gameStates.render.nMaxLightsPerFace;
	gameStates.ogl.nFirstLight = sliP->nFirst;
	gameData.render.nTotalLights += gameStates.ogl.nLights;
	if (gameData.render.nMaxLights < gameStates.ogl.nLights)
		gameData.render.nMaxLights = gameStates.ogl.nLights;
#ifdef _DEBUG
	if (faceP && (faceP->nSegment == nDbgSeg) && ((nDbgSide < 0) || (faceP->nSide == nDbgSide)))
		HUDMessage (0, "%d lights", gameStates.ogl.nLights);
#endif
	}
activeLightsP = gameData.render.lights.dynamic.shader.activeLights [0] + gameStates.ogl.nFirstLight;
nLightRange = sliP->nLast - gameStates.ogl.nFirstLight + 1;
for (nLights = 0; 
	  (gameStates.ogl.iLight < gameStates.ogl.nLights) & (nLightRange > 0) && (nLights < gameStates.render.nMaxLightsPerPass); 
	  activeLightsP++, nLightRange--) { 
	if (!(psl = GetActiveShaderLight (activeLightsP, 0)))
		continue;
#ifdef _DEBUG
	if (faceP && (faceP->nSegment == nDbgSeg) && ((nDbgSide < 0) || (faceP->nSide == nDbgSide)))
		nDbgSeg = nDbgSeg;
	if (faceP - FACES == nDbgFace)
		nDbgFace = nDbgFace;
	if ((psl->nTarget < 0) && (-psl->nTarget - 1 != faceP->nSegment))
		faceP = faceP;
	else if ((psl->nTarget > 0) && (psl->nTarget != faceP - FACES + 1))
		faceP = faceP;
	if (!psl->nTarget)
		psl = psl;
#endif
	if (psl->bUsed [0] == 2) {//nearest vertex light
		ResetUsedLight (psl, 0);
		sliP->nActive--;
		}
	hLight = GL_LIGHT0 + nLights++;
	//glEnable (hLight);
	specular.alpha = (psl->info.nSegment >= 0) ? psl->info.fRad : psl->info.fRad * psl->info.fBoost; //krasser Missbrauch!
	fBrightness = psl->info.fBrightness;
#ifdef _DEBUG
	if ((psl->info.nObject >= 0) && (OBJECTS [psl->info.nObject].nType == nDbgObjType) &&
		 ((nDbgObjId < 0) || (OBJECTS [psl->info.nObject].id == nDbgObjId)))
		nDbgObjType = nDbgObjType;
#endif
	if (psl->info.nType == 2) {
		glLightf (hLight, GL_CONSTANT_ATTENUATION, 1.0f);
		glLightf (hLight, GL_LINEAR_ATTENUATION, OBJ_LIN_ATT / fBrightness);
		glLightf (hLight, GL_QUADRATIC_ATTENUATION, OBJ_QUAD_ATT / fBrightness);
		glLightfv (hLight, GL_SPOT_DIRECTION, (GLfloat *) &vNoDir);
		ambient.red = psl->info.color.red * PPL_AMBIENT_LIGHT;
		ambient.green = psl->info.color.green * PPL_AMBIENT_LIGHT;
		ambient.blue = psl->info.color.blue * PPL_AMBIENT_LIGHT;
		ambient.alpha = 1.0f;
		diffuse.red = psl->info.color.red * PPL_DIFFUSE_LIGHT;
		diffuse.green = psl->info.color.green * PPL_DIFFUSE_LIGHT;
		diffuse.blue = psl->info.color.blue * PPL_DIFFUSE_LIGHT;
		diffuse.alpha = 1.0f;
		}
	else {
		glLightf (hLight, GL_CONSTANT_ATTENUATION, 1.0f);
		glLightf (hLight, GL_LINEAR_ATTENUATION, GEO_LIN_ATT / fBrightness);
		glLightf (hLight, GL_QUADRATIC_ATTENUATION, GEO_QUAD_ATT / fBrightness);
		glLightfv (hLight, GL_SPOT_DIRECTION, (GLfloat *) &psl->info.vDirf);
		ambient.red = psl->info.color.red * PPL_AMBIENT_LIGHT;
		ambient.green = psl->info.color.green * PPL_AMBIENT_LIGHT;
		ambient.blue = psl->info.color.blue * PPL_AMBIENT_LIGHT;
		ambient.alpha = 1.0f;
		fBrightness = min (fBrightness, 1.0f) * PPL_DIFFUSE_LIGHT;
		diffuse.red = psl->info.color.red * fBrightness;
		diffuse.green = psl->info.color.green * fBrightness;
		diffuse.blue = psl->info.color.blue * fBrightness;
		diffuse.alpha = 1.0f;
		}
	glLightfv (hLight, GL_DIFFUSE, (GLfloat *) &diffuse);
	glLightfv (hLight, GL_SPECULAR, (GLfloat *) &specular);
	glLightfv (hLight, GL_AMBIENT, (GLfloat *) &ambient);
	glLightfv (hLight, GL_POSITION, (GLfloat *) (psl->vPosf));
	gameStates.ogl.iLight++;
	}
if (nLightRange <= 0) {
	gameStates.ogl.iLight = gameStates.ogl.nLights;
	}
gameStates.ogl.nFirstLight = activeLightsP - gameData.render.lights.dynamic.shader.activeLights [0];
#ifdef _DEBUG
if ((gameStates.ogl.iLight < gameStates.ogl.nLights) && !nLightRange)
	nDbgSeg = nDbgSeg;
#endif
if (InitPerPixelLightingShader (nType, nLights) >= 0) {
	PROF_END(ptPerPixelLighting)
	return nLights;
	}
OglDisableLighting ();
PROF_END(ptPerPixelLighting)
return -1;
}

//------------------------------------------------------------------------------

int G3SetupPerPixelShader (grsFace *faceP, int bDepthOnly, int nType, bool bHeadlight)
{
PROF_START
	static grsBitmap	*nullBmP = NULL;

	int	bLightmaps, nLights, nShader;

if (bDepthOnly)
	return 0;
if (0 > (nLights = SetupHardwareLighting (faceP, nType)))
	return 0;
#if ONLY_LIGHTMAPS == 2
nType = 0;
#endif
#if CONST_LIGHT_COUNT
nShader = 20 + 4 * gameStates.render.nMaxLightsPerPass + nType;
#else
nShader = 20 + 4 * nLights + nType;
#endif
#ifdef _DEBUG
if (faceP && (faceP->nSegment == nDbgSeg) && ((nDbgSide < 0) || (faceP->nSide == nDbgSide)))
	nDbgSeg = nDbgSeg;
#endif
if ((bLightmaps = HaveLightmaps ())) {
	int i = faceP->nLightmap / LIGHTMAP_BUFSIZE;
#if 1//def _DEBUG
	if (OglCreateLightmap (i))
#endif
		{INIT_TMU (InitTMU0, GL_TEXTURE0, nullBmP, lightmapData.buffers + i, 1, 1);}
	}
if (nShader != gameStates.render.history.nShader) {
	gameData.render.nShaderChanges++;
#if CONST_LIGHT_COUNT
	glUseProgramObject (activeShaderProg = perPixelLightingShaderProgs [gameStates.render.nMaxLightsPerPass][nType]);
#else
	glUseProgramObject (activeShaderProg = perPixelLightingShaderProgs [nLights][nType]);
#endif
	if (bLightmaps)
		glUniform1i (glGetUniformLocation (activeShaderProg, "lMapTex"), 0);
	if (nType) {
		glUniform1i (glGetUniformLocation (activeShaderProg, "baseTex"), bLightmaps);
		if (nType > 1) {
			glUniform1i (glGetUniformLocation (activeShaderProg, "decalTex"), 1 + bLightmaps);
			if (nType > 2)
				glUniform1i (glGetUniformLocation (activeShaderProg, "maskTex"), 2 + bLightmaps);
			}
		}
	}
if (!nType) 
	glUniform4fv (glGetUniformLocation (activeShaderProg, "matColor"), 1, (GLfloat *) &faceP->color);
#if CONST_LIGHT_COUNT
glUniform1f (glGetUniformLocation (activeShaderProg, "nLights"), (GLfloat) nLights); 
#endif
glUniform1f (glGetUniformLocation (activeShaderProg, "fLightScale"), 
#if 1
				 (nLights ? (float) nLights / (float) gameStates.ogl.nLights : 1.0f));
#else
				 (nLights && gameStates.ogl.iLight) ? 0.0f : 1.0f);
#endif
PROF_END(ptShaderStates)
return gameStates.render.history.nShader = nShader;
}

//------------------------------------------------------------------------------

int G3SetupLightmapShader (grsFace *faceP, int bDepthOnly, int nType, bool bHeadlight)
{
PROF_START
	static grsBitmap	*nullBmP = NULL;

	int	nShader;

if (bDepthOnly)
	return 0;
if (!InitLightmapShader (nType))
	return 0;
nShader = 60 + nType;
#ifdef _DEBUG
if (faceP && (faceP->nSegment == nDbgSeg) && ((nDbgSide < 0) || (faceP->nSide == nDbgSide)))
	nDbgSeg = nDbgSeg;
#endif
int i = faceP->nLightmap / LIGHTMAP_BUFSIZE;
if (OglCreateLightmap (i))
	{INIT_TMU (InitTMU0, GL_TEXTURE0, nullBmP, lightmapData.buffers + i, 1, 1);}
if (nShader != gameStates.render.history.nShader) {
	gameData.render.nShaderChanges++;
#if CONST_LIGHT_COUNT
	glUseProgramObject (activeShaderProg = lightmapShaderProgs [nType]);
#else
	glUseProgramObject (activeShaderProg = perPixelLightingShaderProgs [nLights][nType]);
#endif
	glUniform1i (glGetUniformLocation (activeShaderProg, "lMapTex"), 0);
	if (nType) {
		glUniform1i (glGetUniformLocation (activeShaderProg, "baseTex"), 1);
		if (nType > 1) {
			glUniform1i (glGetUniformLocation (activeShaderProg, "decalTex"), 2);
			if (nType > 2)
				glUniform1i (glGetUniformLocation (activeShaderProg, "maskTex"), 3);
			}
		}
	}
if (!nType) 
	glUniform4fv (glGetUniformLocation (activeShaderProg, "matColor"), 1, (GLfloat *) &faceP->color);
PROF_END(ptShaderStates)
return gameStates.render.history.nShader = nShader;
}

//------------------------------------------------------------------------------

int G3SetupGrayScaleShader (int nType, tRgbaColorf *colorP)
{
if (gameStates.render.textures.bHaveGrayScaleShader) {
	if (nType > 2)
		nType = 2;
	int bLightmaps = HaveLightmaps ();
	int nShader = 90 + 3 * bLightmaps + nType;
	if (gameStates.render.history.nShader != nShader) {
		gameData.render.nShaderChanges++;
		glUseProgramObject (activeShaderProg = gsShaderProg [bLightmaps][nType]);
		if (!nType) 
			glUniform4fv (glGetUniformLocation (activeShaderProg, "faceColor"), 1, (GLfloat *) colorP);
		else {
			glUniform1i (glGetUniformLocation (activeShaderProg, "baseTex"), bLightmaps);
			if (nType > 1)
				glUniform1i (glGetUniformLocation (activeShaderProg, "decalTex"), 1 + bLightmaps);
			}
		gameStates.render.history.nShader = nShader;
		}
	}
return gameStates.render.history.nShader;
}

// ----------------------------------------------------------------------------------------------
//eof
