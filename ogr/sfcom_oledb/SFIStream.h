/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SFIStream implemention ... simple binary stream COM object.
 * Author:   Ken Shih, kshih@home.com
 *           Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Les Technologies SoftMap Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2002/02/05 20:42:10  warmerda
 * New
 *
 */

// Define the following to get detailed debugging information from the stream
// class.

#undef SFISTREAM_DEBUG

/************************************************************************/
/* ==================================================================== */
/*                              SFIStream                               */
/*                                                                      */
/*      Simple class to hold give IStream semantics on a byte array.    */
/* ==================================================================== */
/************************************************************************/

struct SFIStream : IStream
{

    // CTOR/DTOR
    SFIStream(void *pByte, int nStreamSize)
	{
#ifdef SFISTREAM_DEBUG
            CPLDebug( "OGR_OLEDB", "SFIStream(%p,%d) -> %p", 
                      pByte, nStreamSize, this );
#endif
            m_cRef   = 0;
            m_pStream  = pByte;
            m_nSize    = nStreamSize;
            m_nSeekPos = 0;
	}

    ~SFIStream()
	{
#ifdef SFISTREAM_DEBUG
            CPLDebug( "OGR_OLEDB", "~SFIStream(%p)", this );
#endif
            free(m_pStream);	
	}


    // IUNKOWN
    HRESULT STDMETHODCALLTYPE	QueryInterface (REFIID riid, void **ppv) 
        {
            if (riid == IID_IUnknown||
                riid == IID_IStream || 
                riid == IID_ISequentialStream)
            {
                *ppv = (IStream *) this;		
                AddRef();
            }
            else
            {
                *ppv = 0;
                return E_NOINTERFACE;
            }

            return NOERROR;
	};

    ULONG STDMETHODCALLTYPE		AddRef (void) 
        {
#ifdef SFISTREAM_DEBUG
            CPLDebug( "OGR_OLEDB", "SFIStream::AddRef(%p)", this );
#endif
            return ++m_cRef;
        };
    ULONG STDMETHODCALLTYPE		Release (void )
	{
#ifdef SFISTREAM_DEBUG
            CPLDebug( "OGR_OLEDB", "SFIStream::Release(%p)", this );
#endif
            if (--m_cRef ==0)
            {
                delete this;
                return 0;
            }
            return m_cRef;
	};

	
    // ISEQUENTIAL STREAM

    HRESULT STDMETHODCALLTYPE	Read(void *pDest, ULONG cbToRead, 
                                     ULONG *pcbActuallyRead)
	{
            ULONG pcbDiscardedResult;

#ifdef SFISTREAM_DEBUG
            CPLDebug( "OGR_OLEDB", "SFIStream::Read(%p,%d)", this, cbToRead);
#endif
            if (!pcbActuallyRead)
                pcbActuallyRead = &pcbDiscardedResult;


            *pcbActuallyRead = MIN(cbToRead, m_nSize - m_nSeekPos);
            memcpy(pDest, ((BYTE *)m_pStream)+m_nSeekPos,*pcbActuallyRead);
            m_nSeekPos+= *pcbActuallyRead;

            return S_OK;
	}

    HRESULT STDMETHODCALLTYPE	Write(void const *pv, ULONG, ULONG *) {return S_FALSE;}

    //	ISTREAM

    HRESULT STDMETHODCALLTYPE	Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, 
                                     ULARGE_INTEGER *plibNewPos)
	{
            ULONG nNewPos;

#ifdef SFISTREAM_DEBUG
            CPLDebug( "OGR_OLEDB", "SFIStream::Seek(%p,%d,%d)", 
                      this, (int) dwOrigin, (int) dlibMove.QuadPart);
#endif

            switch(dwOrigin)
            {
		case STREAM_SEEK_SET:
                    nNewPos = (unsigned long) dlibMove.QuadPart;
                    break;
		case STREAM_SEEK_CUR:
                    nNewPos = (unsigned long) (dlibMove.QuadPart + m_nSeekPos);
                    break;
		case STREAM_SEEK_END:
                    nNewPos = (unsigned long) (m_nSize - dlibMove.QuadPart);
                    break;
		default:
                    return STG_E_INVALIDFUNCTION; 
            }

            if (nNewPos < 0 || nNewPos > m_nSize)
                return STG_E_INVALIDPOINTER;

            if( plibNewPos != NULL )
                plibNewPos->QuadPart = nNewPos;

            m_nSeekPos = nNewPos;
#ifdef SFISTREAM_DEBUG
            CPLDebug( "OGR_OLEDB", "SFIStream::Seek(): m_nSeekPos=%d", 
                      m_nSeekPos );
#endif
            return S_OK;
	}


    HRESULT STDMETHODCALLTYPE	SetSize(ULARGE_INTEGER) {return S_FALSE;}
    HRESULT STDMETHODCALLTYPE	CopyTo(IStream *,ULARGE_INTEGER,
                                       ULARGE_INTEGER *,ULARGE_INTEGER*) 
	{
            return S_FALSE;
	}
    HRESULT STDMETHODCALLTYPE	Commit(DWORD) {return S_FALSE;}
    HRESULT STDMETHODCALLTYPE	Revert() {return S_FALSE;}
    HRESULT STDMETHODCALLTYPE	LockRegion(ULARGE_INTEGER,ULARGE_INTEGER,DWORD){return S_FALSE;}
    HRESULT STDMETHODCALLTYPE	UnlockRegion(ULARGE_INTEGER,ULARGE_INTEGER,DWORD) {return S_FALSE;}
    HRESULT STDMETHODCALLTYPE	Stat(STATSTG *poStat,DWORD fStatFlag) 
	{
#ifdef SFISTREAM_DEBUG
            CPLDebug( "OGR_OLEDB", "SFIStream::Stat(%pd)", 
                      this);
#endif

            if (!poStat)
                return STG_E_INVALIDPOINTER;
			
            poStat->cbSize.QuadPart = m_nSize;
            poStat->type =  STGTY_LOCKBYTES; 

            return S_OK;
	}

    HRESULT STDMETHODCALLTYPE	Clone(IStream **) {return S_FALSE;}

    // Data Members

    int		m_cRef;
    void	*m_pStream;
    ULONG	m_nSize;
    ULONG	m_nSeekPos;
};
