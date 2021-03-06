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

#ifndef _HASH_H
#define _HASH_H

class CHashTable {
	public:
		int32_t 					m_bitSize;
		int32_t					m_andMask;
		int32_t					m_size;
		int32_t					m_nItems;
		CArray<const char*>	m_key;
		CArray<int32_t>		m_value;

	public:
		CHashTable () { Init (); }
		void Init (void);
		int32_t Create (int32_t size);
		void Destroy ();
		int32_t Search (const char *key);
		void Insert (const char *key, int32_t value);

	private:
		int32_t GetKey (const char *key);

};

#endif
