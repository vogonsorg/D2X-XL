#ifndef _CARRAY_H
#define _CARRAY_H

#include "pstypes.h"

//-----------------------------------------------------------------------------

template < class _T > class CArray {

	template < class _T > class CArrayData {
		public:
			_T		*buffer;
			_T		null;
			uint	length;
			uint	pos;
			bool	bExternal;
			bool	bChild;
			};

	protected:
		CArrayData<_T>	m_data;

	public:
		template < class _T >
		class Iterator {
			private:
				_T*			m_start;
				_T*			m_end;
				_T*			m_p;
				CArray<_T>&	m_a;
			public:
				Iterator (CArray<_T>& a) { m_a = a, m_p = NULL; }
				operator bool() const { return m_p != NULL; }
				_T* operator*() const { return m_p; }
				Iterator& operator++() { 
					if (m_p) {
						if (m_p < m_end)
							m_p++;
						else
							m_p = NULL;
						}
					return *this;
					}
				Iterator& operator--() { 
					if (m_p) {
						if (m_p > m_end)
							m_p--;
						else
							m_p = NULL;
						}
					return *this;
					}
				_T* Start (void) { m_p = m_start = m_a.Start (); m_end = m_a.End (); }
				_T* End (void) { m_p = m_start = m_a.End (); m_end = m_a.Start (); }
			};

		CArray () { Init (); }
		
		~CArray() { Destroy (); }
		
		inline void Init (void) { 
			m_data.buffer = reinterpret_cast<_T *> (NULL); 
			m_data.length = 0;
			m_data.bExternal = m_data.bChild = false;
			memset (&m_data.null, 0, sizeof (_T));
			}

		inline void Clear (ubyte filler = 0, uint count = 0xffffffff) { 
			if (m_data.buffer) 
				memset (m_data.buffer, filler, (count < m_data.length) ? count : m_data.length); 
			}
		
#ifdef _DEBUG
		inline int Index (_T* elem) { 
			if (!m_data.buffer || (elem < m_data.buffer) || (elem >= m_data.buffer + m_data.length))
				return -1;	// no buffer or element out of buffer
			uint i = static_cast<uint> (reinterpret_cast<ubyte*> (elem) - reinterpret_cast<ubyte*> (m_data.buffer));
			if (i % sizeof (_T))	
				return -1;	// elem in the buffer, but not properly aligned
			return static_cast<int> (elem - m_data.buffer); 
			}
#else
		inline uint Index (_T* elem) { return elem - m_data.buffer; }
#endif

#ifdef _DEBUG
		inline _T* Pointer (uint i) { 
			if (!m_data.buffer || (i >= m_data.length))
				return NULL;
			return m_data.buffer + i; 
			}
#else
		inline _T* Pointer (uint i) { return m_data.buffer + i; }
#endif

		inline void Destroy (void) { 
			if (m_data.buffer) {
#if 0
				if (m_data.bExternal)
					m_data.buffer = NULL;
				else 
#endif
				if (!m_data.bChild)
					delete[] m_data.buffer;
				Init ();
				}
			}
			
		inline _T *Create (uint length) {
			if (m_data.length != length) {
				Destroy ();
				m_data.length = (m_data.buffer = new _T [length]) ? length : 0;
				}
			return m_data.buffer;
			}
			
		inline _T* Buffer (uint i = 0) { return m_data.buffer + i; }
		
		inline void SetBuffer (_T *buffer, bool bChild = false, uint length = 0xffffffff) {
			if (m_data.buffer != buffer) {
				m_data.buffer = buffer;
				m_data.length = length;
				m_data.bChild = bChild;
				if (buffer)
					m_data.bExternal = true;
				else
					m_data.bExternal = false;
				}
			}
			
		inline _T* Resize (unsigned int length) {
			if (!m_data.buffer)
				return Create (length);
			_T* p = new _T [length];
			if (!p)
				return m_data.buffer;
			memcpy (p, m_data.buffer, ((length < m_data.length) ? m_data.length : length) * sizeof (_T)); 
			delete[] m_data.buffer;
			return m_data.buffer = p;
			}

		inline uint Length (void) { return m_data.length; }

		inline size_t Size (void) { return m_data.length * sizeof (_T); }
#if DBG
		inline _T& operator[] (uint i) { 
			if (i < m_data.length) 
				return m_data.buffer [i];
			return m_data.null; 
			}
#else
		inline _T& operator[] (uint i) { return m_data.buffer [i]; }
#endif

		inline _T& operator= (CArray<_T>& source) { return Copy (source); }

		inline _T& operator= (_T* source) { 
			if (m_data.buffer)
				memcpy (m_data.buffer, source, m_data.length * sizeof (_T)); 
			return m_data.buffer [0];
			}

		inline _T& Copy (CArray<_T>& source, uint offset = 0) { 
			if (source.m_data.buffer && (static_cast<int> (source.m_data.length) > 0)) {
				if (!m_data.buffer)
					Create (source.m_data.length);
				memcpy (m_data.buffer + offset, source.m_data.buffer, ((m_data.length < source.m_data.length) ? m_data.length : source.m_data.length) * sizeof (_T)); 
				}
			return m_data.buffer [0];
			}

		inline _T& operator+ (CArray<_T>& source) { 
			uint offset = m_data.length;
			if (m_data.buffer) 
				Resize (m_data.length + source.m_data.length);
			return Copy (source, offset);
			}

		inline bool operator== (CArray<_T>& other) { 
			return (m_data.length == other.m_data.length) && !(m_data.length && memcmp (m_data.buffer, other.m_data.buffer)); 
			}

		//inline operator bool() const { return m_data.buffer != 0; }

		inline bool operator!= (CArray<_T>& other) { 
			return (m_data.length != other.m_data.length) || (m_data.length && memcmp (m_data.buffer, other.m_data.buffer)); 
			}

		inline _T* Start (void) { return m_data.buffer; }

		inline _T* End (void) { return (m_data.buffer && m_data.length) ? m_data.buffer + m_data.length - 1 : NULL; }

		inline _T* operator++ (void) { return (m_buffer && (m_pos < m_size - 1)) ? m_buffer + ++m_pos : NULL; }

		inline _T* operator-- (void) { return (m_buffer && (m_pos > 0)) ? m_buffer + --m_pos : NULL; }

		inline _T* operator+ (uint i) { return m_data.buffer ? m_data.buffer + i : NULL; }

		inline _T* operator- (uint i) { return m_data.buffer ? m_data.buffer - i : NULL; }

		CArray<_T>& ShareBuffer (CArray<_T>& child) {
			child.m_data = m_data;
			child.m_data.bChild = true;
			return child;
			}

		inline bool operator! () { return m_data.buffer == NULL; }
	};

inline int operator- (char* v, CArray<char>& a) { return a.Index (v); }
inline int operator- (ubyte* v, CArray<ubyte>& a) { return a.Index (v); }
inline int operator- (short* v, CArray<short>& a) { return a.Index (v); }
inline int operator- (ushort* v, CArray<ushort>& a) { return a.Index (v); }
inline int operator- (int* v, CArray<int>& a) { return a.Index (v); }
inline int operator- (uint* v, CArray<uint>& a) { return a.Index (v); }

class CCharArray : public CArray<char> {};
class CByteArray : public CArray<ubyte> {};
class CShortArray : public CArray<short> {};
class CUShortArray : public CArray<ushort> {};
class CIntArray : public CArray<int> {};
class CUIntArray : public CArray<uint> {};
class CFloatArray : public CArray<float> {};

//-----------------------------------------------------------------------------

#endif //_CARRAY_H
