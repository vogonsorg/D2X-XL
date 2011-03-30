#include "descent.h"
#include "cfile.h"
#include "text.h"
#include "menu.h"
#include "IpToCountry.h"

CStack<CIpToCountry> ipToCountry;

//------------------------------------------------------------------------------

static inline bool ParseIP (char* buffer, uint& ip)
{
#if DBG
char* token = strtok (buffer, ",") + 1;
for (ip = 0; isdigit (*token); token++)
	ip = 10 * ip + uint (*token - '0');
if (ip == 0x7FFFFFFF)
	ip = ip;
#else
ip = strtoul (strtok (buffer, "#") + 1, NULL, 10);
#endif
return (ip > 0);
}

//------------------------------------------------------------------------------

static inline bool Skip (int nFields)
{
while (nFields--)
	if (!strtok (NULL, ","))
		return false;
return true;
}

//------------------------------------------------------------------------------

static bool LoadBinary (void)
{
	CFile	cf;

if (!cf.Exist ("IpToCountry.bin", gameFolders.szCacheDir, 0))
	return false;
if (cf.Date ("IpToCountry.csv", gameFolders.szDataDir [1], 0) > cf.Date ("IpToCountry.bin", gameFolders.szCacheDir, 0))
	return false;
if (!cf.Open ("IpToCountry.bin", gameFolders.szCacheDir, "rb", 0))
	return false;
int h = cf.Size () - sizeof (int);
if ((h < 0) || (h / sizeof (CIpToCountry) < 1) || (h % sizeof (CIpToCountry) != 0)) {
	cf.Close ();
	return false;
	}

uint nRecords = (uint) cf.ReadInt ();

if (!ipToCountry.Create ((uint) nRecords))
	return false;

bool bSuccess = (ipToCountry.Read (cf, nRecords) == nRecords);
if (bSuccess)
	ipToCountry.Grow ((uint) nRecords);
else
	ipToCountry.Destroy ();

cf.Close ();
return bSuccess;
}

//------------------------------------------------------------------------------

static bool SaveBinary (void)
{
	CFile	cf;

if (!cf.Open ("IpToCountry.bin", gameFolders.szCacheDir, "wb", 0))
	return false;
cf.WriteInt ((int) ipToCountry.ToS ());
bool bSuccess = (ipToCountry.Write (cf) == ipToCountry.Length ());
if (cf.Close ())
	bSuccess = false;
return bSuccess;
}

//------------------------------------------------------------------------------

static CFile cf;

int ReadIpToCountryRecord (void)
{
char lineBuf [1024];
char* token;
uint minIP, maxIP;
char country [4];

country [3] = '\0';
if (!cf.GetS (lineBuf, sizeof (lineBuf)))
	return 0;
if ((lineBuf [0] == '\0') || (lineBuf [0] == '#'))
	return 1;
if (!(ParseIP (lineBuf, minIP) && ParseIP (NULL, maxIP)))
	return 1;
// skip 3 fields
if (!Skip (3))
	return 1;
if (!(token = strtok (NULL, ",")))
	return 1;
strncpy (country, token + 1, sizeof (country) - 1);
if (!strncmp (country, "ZZ", 2))
	return 1;
//if (!strcmp (country, "ZZZ"))
//	return 1;
if ((token = strtok (NULL, ",")) && !strcmp (token, "\"Reserved\""))
	return 1;
if (minIP > maxIP)
	Swap (minIP, maxIP);
ipToCountry.Push (CIpToCountry (minIP, maxIP, country));
return 1;
}

//------------------------------------------------------------------------------

static int LoadIpToCountryPoll (CMenu& menu, int& key, int nCurItem, int nState)
{
if (!ReadIpToCountryRecord ())
	key = -1;
else {
	menu [0].m_value++;
	menu [0].m_bRebuild = 1;
	key = 0;
	}
return nCurItem;
}

//------------------------------------------------------------------------------

int LoadIpToCountry (void)
{
if (LoadBinary ())
	return ipToCountry.Length ();

	int nRecords = cf.LineCount ("IpToCountry.csv", gameFolders.szDataDir [1], "#");

if (nRecords <= 0)
	return nRecords;

if (!ipToCountry.Create ((uint) nRecords))
	return -1;

if (!cf.Open ("IpToCountry.csv", gameFolders.szDataDir [1], "rt", 0))
	return -1;

ProgressBar (TXT_LOADING_IPTOCOUNTRY, 0, nRecords, LoadIpToCountryPoll); 

cf.Close ();
if (ipToCountry.ToS ())
	ipToCountry.SortAscending ();

SaveBinary ();

return (int) ipToCountry.ToS ();
}

//------------------------------------------------------------------------------

char* CountryFromIP (uint ip)
{
uint l = 0, r = ipToCountry.ToS () - 1, i;
do {
	i = (l + r) / 2;
	if (ipToCountry [i] < ip)
		l = i + 1;
	else if (ipToCountry [i] > ip)
		r = i - 1;
	else
		break;
	} while (l <= r);

if (l > r)
	return "n/a";

// find first record with equal key
for (; i > 0; i--)
	if (ipToCountry [i - 1] != ip)
		break;

int h = i;
int dMin = ipToCountry [i].Range ();
// Find smallest IP range containing key
while (ipToCountry [++i] == ip) {
	int d = ipToCountry [i].Range ();
	if (dMin > d) {
		dMin = d;
		h = i;
		}
	}
return ipToCountry [h].m_country;
}

//------------------------------------------------------------------------------

