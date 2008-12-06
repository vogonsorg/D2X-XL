#ifndef _CARRAY_H
#define _CARRAY_H

#include "pstypes.h"

//-----------------------------------------------------------------------------

template < class _T > class CArray {
	protected:
		_T		*m_buffer;
		_T		m_null;
		uint	m_size;
		bool	m_bExternal;
		bool	m_bChild;
	public:
		CArray () { Init (); }
		
		~CArray() { Destroy (); }
		
		inline void Init (void) { 
			m_buffer = reinterpret_cast<_T *> (NULL); 
			m_bExternal = m_bChild = false;
			memset (&m_null, 0, sizeof (_T));
			}

		inline void Clear (ubyte filler = 0) { if (m_buffer) memset (m_buffer, filler, m_size); }
		
#ifdef _DEBUG
		inline uint Index (_T* elem) { 
			if (!m_buffer || (elem < m_buffer) || (elem >= m_buffer + m_size))
				return -1;	// no buffer or element out of buffer
			uint i = static_cast<uint> (reinterpret_cast<ubyte*> (elem) - reinterpret_cast<ubyte*> (m_buffer));
			if (i % sizeof (_T))	
				return -1;	// elem in the buffer, but not properly aligned
			return elem - m_buffer; 
			}
#else
		inline uint Index (_T* elem) { return elem - m_buffer; }
#endif

#ifdef _DEBUG
		inline _T* Pointer (uint i) { 
			if (!m_buffer || (i >= m_size))
				return NULL;
			return m_buffer + i; 
			}
#else
		inline _T* Pointer (uint i) { return m_buffer + i; }
#endif

		inline void Destroy (void) { 
			if (m_buffer) {
#if 0
				if (m_bExternal)
					m_buffer = NULL;
				else 
#endif
				if (!m_bChild)
					delete[] m_buffer;
				Init ();
				}
			}
			
		inline _T *Create (uint size) {
			Destroy ();
			m_size = (m_buffer = new _T [size]) ? size : 0;
			return m_buffer;
			}
			
		inline _T* Buffer (void) { return m_buffer; }
		
		inline void SetBuffer (_T *buffer, bool bChild = false, uint size = 0xffffffff) {
			if (m_buffer != buffer) {
				m_buffer = buffer;
				m_size = size;
				m_bChild = bChild;
				if (buffer)
					m_bExternal = true;
				else
					m_bExternal = false;
				}
			}
			
		inline _T* Resize (unsigned int size) {
			if (!m_buffer)
				return Create (size);
			_T* p = new _T [size];
			if (!p)
				return m_buffer;
			memcpy (p, m_buffer, ((size < m_size) ? m_size : size) * sizeof (_T)); 
			delete[] m_buffer;
			return m_buffer = p;
			}
		inline uint Size (void) { return m_size; }
#if DBG
		inline _T& operator[] (uint i) { return (i < m_size) ? m_buffer [i] : m_null; }
#else
		inline _T& operator[] (uint i) { return m_buffer [i]; }
#endif

		inline _T& operator= (CArray<_T>& source) { return Copy (source); }

		inline _T& operator= (_T* source) { 
			if (m_buffer)
				memcpy (m_buffer, source, m_size * sizeof (_T)); 
			}

		inline _T& Copy (CArray<_T>& source, uint offset = 0) { 
			if (source.m_buffer) {
				if (!m_buffer)
					Create (source.m_size);
				memcpy (m_buffer + offset, source.m_buffer, ((m_size < source.m_size) ? m_size : source.m_size) * sizeof (_T)); 
				}
			return m_buffer [0];
			}

		inline _T& operator+ (CArray<_T>& source) { 
			uint offset = m_size;
			if (m_buffer) 
				Resize (m_size + source.m_size);
			return Copy (source, offset);
			}

		inline _T* operator+ (uint i) { return m_buffer ? m_buffer + i : NULL; }
	};

//-----------------------------------------------------------------------------

#endif //_CARRAY_H
