/*
 * strio.c: string/file manipulation functions by Victor Rachels
 */

#ifdef HAVE_CONFIG_H
#include <conf.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "cfile.h"
#include "strio.h"
//added on 9/16/98 by adb to add memory tracking for this module
#include "u_mem.h"
//end additions - adb

char *fsplitword (CFile& cf, char splitchar)
{
	int32_t	i, mem;
	char	c, *buf;

mem = 256;
buf = NEW char [mem];
c = cf.GetC ();
for (i = 0; (c != splitchar) && !cf.EoF (); i++) {
	if (i == mem) {
		char* newBuf = NEW char [mem + 256];
		memcpy (newBuf, buf, mem);
		mem += 256;
		delete[] buf;
		buf = newBuf;
		}
	buf [i] = c;
	c = cf.GetC ();
	}
if (cf.EoF () && (c != splitchar))
	buf [i++] = c;
buf [i] = 0;
return buf;
}


char *splitword (char *s, char splitchar)
{
	int32_t	l, lw;
	char	*buf, *p;

l = (int32_t) strlen (s);
p = strchr (s, splitchar);
lw = p ? (int32_t) (p - s) : l;
buf = NEW char [lw + 1];
memcpy (buf, s, lw + 1);
buf [lw] = '\0';
if (p)
	memmove (s, p + 1, l - lw);
else
	*s = '\0';
return buf;
}
