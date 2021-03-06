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

#ifndef __UMEM_H
#define __UMEM_H

#include <stdlib.h>
#include "pstypes.h"

extern int32_t bShowMemInfo;

#if DBG
#	define DBG_MALLOC	0
#else
#	define DBG_MALLOC 0
#endif

#if DBG_MALLOC

void _CDECL_ LogMemBlocks (bool bCleanup = false);
void * MemAlloc (size_t size, const char * var, const char * file, int32_t line, int32_t bZeroFill);
void * MemRealloc (void * buffer, size_t size, const char * var, const char * file, int32_t line);
void MemFree (void * buffer);
char * MemStrDup (const char * str, const char * var, const char * file, int32_t line);
void MemInit ();
void LogMemBlocks (bool bCleanup);
bool TrackMemory (bool bTrack);

/* DPH: Changed malloc, etc. to d_malloc. Overloading system calls is very evil and error prone */
#define D2_ALLOC(size)			MemAlloc ((size), "Unknown", __FILE__, __LINE__, 0)
#define D2_CALLOC(n,size)		MemAlloc ((n*size), "Unknown", __FILE__, __LINE__, 1)
#define D2_REALLOC(ptr,size)	MemRealloc ((ptr), (size), "Unknown", __FILE__, __LINE__)
#define D2_FREE(ptr)			 {MemFree (reinterpret_cast<void *> (ptr)); ptr = NULL;} 

#define MALLOC(_var, _type, _count)   ((_var) = reinterpret_cast<_type *> (MemAlloc ((_count) * sizeof (_type), #_var, __FILE__, __LINE__, 0)))

// Checks to see if any blocks are overwritten
void MemValidateHeap ();

#include <iostream>

void RegisterMemBlock (void *p, size_t size, const char *pszFile, int32_t nLine);
int32_t UnregisterMemBlock (void *p);

using namespace std;

void* __CRTDECL operator new (size_t size, const char *pszFile, int32_t nLine);
	
void* __CRTDECL operator new[] (size_t size, const char *pszFile, int32_t nLine);

void __CRTDECL operator delete (void* p/*, const char *pszFile, int32_t nLine*/);

void __CRTDECL operator delete[] (void* p/*, const char *pszFile, int32_t nLine*/);

#define NEW new(__FILE__, __LINE__)

#else

void *MemAlloc (size_t size);
void *MemRealloc (void * buffer, size_t size);
void MemFree (void * buffer);
char *MemStrDup (const char * str);

#define D2_ALLOC(_size)			MemAlloc (_size)
#define D2_CALLOC(n, _size)	MemAlloc (n * _size)
#define D2_REALLOC(_p,_size)	MemRealloc (_p,_size)
#define D2_FREE(_p)			 {MemFree ((void *) (_p)); (_p) = NULL;}

#define MALLOC(_v,_t,_c)		(_v) = NEW _t [_c]
#define FREE(_v)					delete[] (_v)

#define NEW new

#define RegisterMemBlock(_p, _size, _pszFile, _nLine)
#define UnregisterMemBlock(_p)
#define LogMemBlocks(_bCleanUp)
#define TrackMemory(_bTrack)

#endif

extern size_t	nCurAllocd, nMaxAllocd;

#define CREATE(_p,_s,_f)	if ((_p).Create (_s, #_p)) (_p).Clear (_f); else return false
#define RESIZE(_p,_s)			if (!(_p).Resize (_s)) return false
#define DESTROY(_p)				(_p).Destroy ()

void *GetMem (size_t size, char filler);

#endif // __UMEM_H

//eof
