/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  CVirtualArray/CSFCommand (OLE DB records reader) implementation.
 * Author:   Ken Shih, kshih@home.com
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
 * Revision 1.28  2001/11/01 16:47:03  warmerda
 * use factories to destroy features and geometry
 *
 * Revision 1.27  2001/10/24 16:17:25  warmerda
 * improve debugging support
 *
 * Revision 1.26  2001/10/23 21:35:25  warmerda
 * try getting IStream if ISequentialStream is missing.
 *
 * Revision 1.25  2001/10/22 21:29:50  warmerda
 * reworked to allow selecting a subset of fields
 *
 * Revision 1.24  2001/10/02 14:25:16  warmerda
 * ensure attribute query is cleared when not in use
 *
 * Revision 1.23  2001/09/06 03:25:55  warmerda
 * added debug report on spatial envelope, and g_nNextSFAccessorHandle
 *
 * Revision 1.22  2001/08/17 14:25:22  warmerda
 * added spatial and attribute query support
 *
 * Revision 1.21  2001/06/01 18:05:06  warmerda
 * added more debugging, add resetreading on Initialize
 *
 * Revision 1.20  2001/05/31 02:55:49  warmerda
 * formatting
 *
 * Revision 1.19  2001/05/28 19:37:34  warmerda
 * lots of changes
 *
 * Revision 1.18  1999/11/23 15:14:29  warmerda
 * Avoid some casting warnings.
 *
 * Revision 1.17  1999/09/13 02:07:01  warmerda
 * reduced STRING_BUFFER_SIZE
 *
 * Revision 1.16  1999/09/07 14:41:46  warmerda
 * Add transport of integer, real and string lists as simple 80char strings.
 * Eventually we should look into more appropriate means of accomplishing this.
 *
 * Revision 1.15  1999/09/07 12:39:02  warmerda
 * removed oledbsup_sf.h
 *
 * Revision 1.14  1999/07/23 19:20:27  kshih
 * Modifications for errors etc...
 *
 * Revision 1.13  1999/07/20 17:09:57  kshih
 * Use OGR code.
 *
 * Revision 1.12  1999/06/25 18:17:44  kshih
 * Use new routines to get data source.
 *
 * Revision 1.11  1999/06/22 16:59:30  kshih
 * Temporary fix for ADO.  Use static variable to keep datasource around.
 *
 * Revision 1.10  1999/06/21 21:08:46  warmerda
 * added some extra debugging info in GetInitDataSource()
 *
 * Revision 1.9  1999/06/21 20:53:07  warmerda
 * Avoid crashing if InitDataSource() returns NULL.
 *
 * Revision 1.8  1999/06/12 17:15:42  kshih
 * Make use of datasource property
 * Add schema rowsets
 *
 * Revision 1.7  1999/06/08 17:44:10  warmerda
 * Fixed sequential Read() with SFIStream ... seek position wasn't
 * being incremented after reads.
 *
 * Revision 1.6  1999/06/04 15:33:51  warmerda
 * Added copyright headers, and function headers.
 *
 */

#include <assert.h>
#include "stdafx.h"
#include "SF.h"
#include "SFRS.h"
#include "SFSess.h"
#include "ogr_geometry.h"
#include "sfutil.h"
#include "cpl_error.h"
#include "cpl_conv.h"
#include "cpl_string.h"

// Select one of BLOB_NONE, BLOB_IUNKNOWN, BLOB_BYTES, or BLOB_BYTES_BY_REF
// This will determine the type and handling of the geometry column.

#define BLOB_IUNKNOWN

// I use a length of 1024, because anything larger will trigger treatment
// as a BLOB by the code in CDynamicAccessor::BindColumns() in ATLDBCLI.H.
// Treatment as a BLOB (with an sequential stream object created) results
// in the failure of a later CanConvert() test in 
// IAccessorImpl::ValidateBindsFromMetaData().
#define		STRING_BUFFER_SIZE	1024

void OGRComDebug( const char * pszDebugClass, const char * pszFormat, ... );

// Define the following to get detailed debugging information from the stream
// class.

#undef SFISTREAM_DEBUG

// These global variables are a hack to transmit spatial query info from
// the CSFCommand::Execute() method to the CSFRowset::Execute() method.

static OGRGeometry      *poGeometry = NULL;
static DBPROPOGISENUM   eFilterOp = DBPROP_OGIS_ENVELOPE_INTERSECTS;

int g_nNextSFAccessorHandle = 1;

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

/************************************************************************/
/* ==================================================================== */
/*                            SFIDataConvert                            */
/*                                                                      */
/*      A call for doing data conversions.  It just uses the            */
/*      CLSID_DataConvert class for implementation with the             */
/*      exception of added support for IUKNOWN to BYTES                 */
/*      translation.                                                    */
/*                                                                      */
/*      NOTE: This class currently incomplete and unused.               */
/* ==================================================================== */
/************************************************************************/

class SFIDataConvert : public IDataConvert
{
public:
    SFIDataConvert()
        {
            HRESULT hr = ::CoCreateInstance(CLSID_DataConvert, NULL, 
                                            CLSCTX_INPROC_SERVER, 
                                            IID_IDataConvert, 
                                            (void**)&m_spConvert);
            m_cRef = 0;
        }

    ~SFIDataConvert() {}

    // IUNKOWN
    HRESULT STDMETHODCALLTYPE	QueryInterface (REFIID riid, void **ppv) 
        {
            if (riid == IID_IUnknown||
                riid == IID_IDataConvert) 
            {
                *ppv = this;		
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
            return ++m_cRef;
        };

    ULONG STDMETHODCALLTYPE		Release (void )
	{
            if (--m_cRef ==0)
            {
                delete this;
                return 0;
            }
            return m_cRef;
	};

    HRESULT STDMETHODCALLTYPE CanConvert( DBTYPE wSrcType, DBTYPE wDstType )
        {
            if( wSrcType == DBTYPE_IUNKNOWN 
                && wDstType == DBTYPE_BYTES )
            {
                return S_OK;
            }
            else 
                return m_spConvert->CanConvert( wSrcType, wDstType );
        }
	
    HRESULT STDMETHODCALLTYPE GetConversionSize( 
        DBTYPE wSrcType, DBTYPE wDstType,
        ULONG *pcbSrcLength, ULONG *pcbDstLength,
        void *pSrc )
        {
            if( wSrcType == DBTYPE_IUNKNOWN 
                && wDstType == DBTYPE_BYTES )
            {
                return S_OK;
            }
            else 
                return m_spConvert->GetConversionSize( 
                    wSrcType, wDstType, pcbSrcLength, pcbDstLength, pSrc );
        }

    HRESULT STDMETHODCALLTYPE DataConvert( 
        DBTYPE wSrcType, DBTYPE wDstType,
        ULONG cbSrcLength, ULONG * pcbDstLength,
        void *pSrc, void *pDst, 
        ULONG cbDstMaxLength, 
        DBSTATUS dbsSrcStatus, DBSTATUS *pdbsStatus,
        BYTE bPrecision, BYTE bScale, DBDATACONVERT dwFlags )
        {
            if( wSrcType == DBTYPE_IUNKNOWN 
                && wDstType == DBTYPE_BYTES )
            {
                return S_OK;
            }
            else 
                return m_spConvert->DataConvert( 
                    wSrcType, wDstType, cbSrcLength, pcbDstLength, 
                    pSrc, pDst, cbDstMaxLength, 
                    dbsSrcStatus, pdbsStatus, bPrecision, bScale, dwFlags );
        }

    // Data Members

    int		m_cRef;
    CComPtr<IDataConvert> m_spConvert;
};


/************************************************************************/
/* ==================================================================== */
/*                            CVirtualArray                             */
/*                                                                      */
/*      This class holds a cache of records from the table.             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           CVirtualArray()                            */
/************************************************************************/

CVirtualArray::CVirtualArray()
{
    mBuffer = NULL;
    m_nBufferSize = 0;
    m_nLastRecordAccessed = -1;
    m_nPackedRecordLength = 0;
    m_nArraySize = 0;
}

/************************************************************************/
/*                           ~CVirtualArray()                           */
/************************************************************************/

CVirtualArray::~CVirtualArray()
{
    CPLDebug( "OGR_OLEDB", "~CVirtualArray()" );

    if( mBuffer != NULL )
	free(mBuffer);
}

/************************************************************************/
/*                      CVirtualArray::RemoveAll()                      */
/************************************************************************/

void CVirtualArray::RemoveAll()
{
    CPLDebug( "OGR_OLEDB", "CVirtualArray::RemoveAll()" );
}

/************************************************************************/
/*                     CVirtualArray::Initialize()                      */
/*                                                                      */
/*      Initialize the record cache.                                    */
/************************************************************************/

void	CVirtualArray::Initialize(int nArraySize, OGRLayer *pLayer,
                                  int nBufferSize, CSFRowset *poRowset )
{
        m_nBufferSize  = nBufferSize;
	mBuffer	       = (BYTE *) malloc(nBufferSize);
	m_nArraySize   = nArraySize;
	m_pOGRLayer    = pLayer;
	m_pFeatureDefn = pLayer->GetLayerDefn();
        m_pRowset      = poRowset;

        CPLDebug( "OGR_OLEDB", "CVirtualArray::Initialize()" );
        m_pOGRLayer->ResetReading();
}

/************************************************************************/
/*                      CVirtualArray::operator[]                       */
/*                                                                      */
/*      Fetch request record from the record cache, possibly having     */
/*      to add it to the cache.                                         */
/************************************************************************/

BYTE &CVirtualArray::operator[](int iIndex) 
{
    OGRFeature      *poFeature;

    CPLDebug( "OGR_OLEDB", "CVirtualArray::operator[%d]", iIndex );

    // Pre-initialize output buffer.
    memset( mBuffer, 0, m_nBufferSize );

    // Make sure we are at the correct position to read the next record.
    if (iIndex -1 != m_nLastRecordAccessed)
    {
        CPLDebug( "OGR_OLEDB", "ResetReading() and skip:[" );
        m_pOGRLayer->ResetReading();
        m_nLastRecordAccessed = -1;

        while (m_nLastRecordAccessed != iIndex -1)
        {
            poFeature = m_pOGRLayer->GetNextFeature();
            if( poFeature )
            {
                CPLDebug( "OGR_OLEDB", "." );
                OGRFeature::DestroyFeature( poFeature );
            }
            else
            {
                CPLDebug( "OGR_OLEDB", "Didn't get feature at %s:%d\n", 
                          __FILE__, __LINE__ );
                break;
            }

            m_nLastRecordAccessed++;
        }
        CPLDebug( "OGR_OLEDB", "]\n" );
    }

    if (iIndex -1 != m_nLastRecordAccessed)
    {
        CPLDebug( "OGR_OLEDB", "Assertion failed at %s:%d\n", 
                  __FILE__, __LINE__ );
        // Error condition; // Assertion failure!
        return *mBuffer;
    }

    m_nLastRecordAccessed = iIndex;

    poFeature = m_pOGRLayer->GetNextFeature();

    if (!poFeature)
    {
        CPLDebug( "OGR_OLEDB", "Failed to get feature at %s:%d\n", 
                  __FILE__, __LINE__ );
        // Error condition; // Assertion failure!
        return *mBuffer;
    }

/* -------------------------------------------------------------------- */
/*      Fill in fields.                                                 */
/* -------------------------------------------------------------------- */
    int      iDBField;

    for( iDBField = 0; iDBField < m_pRowset->m_paColInfo.GetSize(); iDBField++)
    {
        ATLCOLUMNINFO *pColInfo;
        int           nOGRIndex;
        
        pColInfo = &(m_pRowset->m_paColInfo[iDBField]);
        nOGRIndex = m_pRowset->m_panOGRIndex[iDBField];

        if( nOGRIndex == -1 )
        {
            *((int*) (mBuffer + pColInfo->cbOffset)) = poFeature->GetFID();
        }
        else if( nOGRIndex == -2 )
        {
            FillGeometry( poFeature->GetGeometryRef(), mBuffer, pColInfo );
        }
        else 
        {
            FillOGRField( poFeature, nOGRIndex, mBuffer, pColInfo );
        }
    }

    OGRFeature::DestroyFeature( poFeature );

    return (*mBuffer);
}

/************************************************************************/
/*                            FillGeometry()                            */
/************************************************************************/

int CVirtualArray::FillGeometry( OGRGeometry *poGeom, 
                                 unsigned char *pabyBuffer,
                                 ATLCOLUMNINFO *pColInfo )

{
    int      nOffset = pColInfo->cbOffset;

    if( poGeom == NULL )
        return TRUE;
    
    int                 nSize = poGeom->WkbSize();

/* -------------------------------------------------------------------- */
/*      IUnknown geometry handling.                                     */
/* -------------------------------------------------------------------- */
#ifdef BLOB_IUNKNOWN
    unsigned char	*pByte  = (unsigned char *) malloc(nSize);
    poGeom->exportToWkb((OGRwkbByteOrder) 1, pByte);

#ifdef SFISTREAM_DEBUG
    CPLDebug( "OGR_OLEDB", 
              "Push %d bytes into Stream: %2X%2X%2X%2X%2X%2X%2X%2X\n", 
              poGeom->WkbSize(),
              pByte[0], 
              pByte[1], 
              pByte[2], 
              pByte[3], 
              pByte[4], 
              pByte[5], 
              pByte[6], 
              pByte[7] );
#endif

    IStream	*pStream = new SFIStream(pByte,nSize);
    *((void **) (pabyBuffer + nOffset)) = pStream;
#endif

/* -------------------------------------------------------------------- */
/*      BYTES geometry handling.                                        */
/* -------------------------------------------------------------------- */
#ifdef BLOB_BYTES
    if( nSize >= pColInfo.ulColumnSize )
        poGeom->exportToWkb( (OGRwkbByteOrder) 1, pabyBuffer + nOffset );
    else
        CPLDebug( "OGR_OLEDB", "Geometry to big (%d bytes).", nSize );
#endif

    return TRUE;
}
	
/************************************************************************/
/*                            FillOGRField()                            */
/*                                                                      */
/*      Copy information for one field into the provided buffer from    */
/*      an OGRFeature.                                                  */
/************************************************************************/

int CVirtualArray::FillOGRField( OGRFeature *poFeature, int iField, 
                                 unsigned char *pabyBuffer, 
                                 ATLCOLUMNINFO *pColInfo )

{
    OGRFieldDefn *poDefn;
    int           nOffset = pColInfo->cbOffset;

    poDefn = m_pFeatureDefn->GetFieldDefn(iField);

    switch(poDefn->GetType())
    {
        case OFTInteger:
            CPLAssert( pColInfo->ulColumnSize == 4 );
            *((int *) (pabyBuffer + nOffset)) = 
                poFeature->GetFieldAsInteger(iField);
            break;
            
        case OFTReal:
            CPLAssert( pColInfo->ulColumnSize == 8 );
            *((double *) (pabyBuffer + nOffset)) = 
                poFeature->GetFieldAsDouble(iField);
            break;
            
        case OFTIntegerList:
        case OFTRealList:
        case OFTStringList:
        case OFTString:
        {
            const char *pszStr = poFeature->GetFieldAsString(iField);
            int nStringWidth = MIN(strlen(pszStr),pColInfo->ulColumnSize - 1);

            strncpy((char *) pabyBuffer + nOffset,pszStr,nStringWidth);
            pabyBuffer[nOffset+nStringWidth] = '\0';
        }
        break;
    }

    return TRUE;
}

/************************************************************************/
/*                         CSFComand::Execute()                         */
/************************************************************************/

HRESULT CSFCommand::Execute(IUnknown * pUnkOuter, REFIID riid, 
                            DBPARAMS * pParams,  LONG * pcRowsAffected, 
                            IUnknown ** ppRowset)
{
    CSFRowset* pRowset;
    HRESULT      hr;

    if (pParams != NULL && pParams->pData != NULL)
    {
        hr = ExtractSpatialQuery( pParams );
        if( hr != S_OK )
            return hr;
    }
    hr = CreateRowset(pUnkOuter, riid, pParams, pcRowsAffected, ppRowset, 
                      pRowset);

    // clean up spatial filter geometry if still hanging around.
    if( poGeometry != NULL )
    {
        OGRGeometryFactory::destroyGeometry( poGeometry );
        poGeometry = NULL;
    }
    
    return hr;
}

/************************************************************************/
/*                        ExtractSpatialQuery()                         */
/************************************************************************/

HRESULT CSFCommand::ExtractSpatialQuery( DBPARAMS *pParams )

{
    HRESULT  hr;
    VARIANT   *pVariant = NULL;

/* -------------------------------------------------------------------- */
/*      First we dump all parameter values as best we can to assist     */
/*      in debugging if they are inappropriate.                         */
/* -------------------------------------------------------------------- */
    if( pParams->cParamSets != 1 )
    {
        CPLDebug( "OGR_OLEDB", "DBPARAMS->cParamSets=%d, this is a problem!\n",
                  pParams->cParamSets );
        return SFReportError(DB_E_ERRORSINCOMMAND,IID_ICommand,0,
                             "Improper Parameters.");
    }

    ULONG   cBindings;
    DBACCESSORFLAGS dwAccessorFlags;
    DBBINDING *rgBindings;
    int       iBinding;

    hr = GetBindings( pParams->hAccessor, &dwAccessorFlags, &cBindings, 
                      &rgBindings );
    
    CPLDebug( "OGR_OLEDB", "%d parameter bindings found.", cBindings );

    for( iBinding = 0; iBinding < cBindings; iBinding++ )
    {
        CPLDebug( "OGR_OLEDB", 
                  "iOrdinal=%d,obValue=%d,obLength=%d,cbMaxLen=%d,wType=%d",
                  rgBindings[iBinding].iOrdinal,
                  rgBindings[iBinding].obValue,
                  rgBindings[iBinding].obLength,
                  rgBindings[iBinding].cbMaxLen,
                  rgBindings[iBinding].wType );

        if( rgBindings[iBinding].dwPart & DBPART_LENGTH )
            CPLDebug( "OGR_OLEDB", "Length=%d", 
                      *((int *) (((unsigned char *) pParams->pData) 
                                 + rgBindings[iBinding].obLength)) );
            
        if( rgBindings[iBinding].wType == DBTYPE_WSTR )
        {
            CPLDebug( "OGR_OLEDB", "WSTR=%S", 
                      ((unsigned char *) pParams->pData) 
                      + rgBindings[iBinding].obValue );
        }
        else if( rgBindings[iBinding].wType == DBTYPE_UI4 )
        {
            CPLDebug( "OGR_OLEDB", "UI4=%d", 
                      *((int *) (((unsigned char *) pParams->pData) 
                                 + rgBindings[iBinding].obValue)) );
        }
        else if( rgBindings[iBinding].wType == DBTYPE_VARIANT )
        {
            pVariant = (VARIANT *) (((unsigned char *) pParams->pData) 
                                    + rgBindings[iBinding].obValue);

            CPLDebug( "OGR_OLEDB", "VARIANT.vt=%d", pVariant->vt );
        }
    }

/* -------------------------------------------------------------------- */
/*      Does the passed parameters match with our expectations for      */
/*      spatial query parameters?                                       */
/* -------------------------------------------------------------------- */
    if( cBindings != 3 
        || rgBindings[0].wType != DBTYPE_VARIANT
        || rgBindings[1].wType != DBTYPE_UI4
        || rgBindings[2].wType != DBTYPE_WSTR )
    {
        CPLDebug( "OGR_OLEDB", 
                  "Parameter types inappropriate in ExtractSpatialQuery()\n" );
        return S_OK;
    }

/* -------------------------------------------------------------------- */
/*      Extract the geometry.                                           */
/* -------------------------------------------------------------------- */
    pVariant = (VARIANT *) (((unsigned char *) pParams->pData) 
                            + rgBindings[0].obValue);

    if( rgBindings[0].wType == DBTYPE_BYTES )
    {
        int      nLength;
        OGRErr   eErr;

        if( rgBindings[0].dwPart & DBPART_LENGTH )
            nLength = *((int *) (((unsigned char *) pParams->pData) 
                                 + rgBindings[0].obLength));
        else
            nLength = rgBindings[0].cbMaxLen;

        eErr = OGRGeometryFactory::createFromWkb(                       
            ((unsigned char *) pParams->pData) + rgBindings[0].obValue,
            NULL, &poGeometry, nLength );
        if( eErr != OGRERR_NONE )
            CPLDebug( "OGR_OLEDB", 
                      "Corrupt DBTYPE_BYTES WKB in ExtractSpatialQuery()." );
    }

    else if( rgBindings[0].wType == DBTYPE_VARIANT 
             && pVariant->vt == (VT_UI1|VT_ARRAY) )
    {
        int      nLength;
        SAFEARRAY *pArray;
        unsigned char *pRawData;
        long  UBound, LBound;

        pArray = pVariant->parray;

        if( SafeArrayGetDim(pArray) != 1 )
            return S_OK;
        
        SafeArrayAccessData( pArray, (void **) &pRawData );
        SafeArrayGetUBound( pArray, 1, &UBound );
        SafeArrayGetLBound( pArray, 1, &LBound );
        nLength = UBound - LBound + 1;

        OGRGeometryFactory::createFromWkb( pRawData, NULL, &poGeometry, 
                                           nLength );
        SafeArrayUnaccessData( pArray );
    }

    else if( rgBindings[0].wType == DBTYPE_VARIANT 
             && pVariant->vt == VT_UNKNOWN )
    {
        OGRErr   eErr;
        ISequentialStream *  pIStream = NULL;
        IUnknown *pIUnknown;
        unsigned char *pRawData = NULL;
        int       nSize = 0;

        pIUnknown = pVariant->punkVal;
        if( pIUnknown != NULL )
        {
            hr = pIUnknown->QueryInterface( IID_ISequentialStream,
                                            (void**)&pIStream );
            if( FAILED(hr) )
            {
                CPLDebug( "OGR_OLEDB", 
                          "Failed to get ISequentialStream, try for IStream");
                hr = pIUnknown->QueryInterface( IID_IStream,
                                                (void**)&pIStream );
            }

            if( FAILED(hr) )
                pIStream = NULL;
        }

        CPLDebug( "OGR_OLEDB", "Got pIStream=%p from %p", 
                  pIStream, pIUnknown );

        if( pIStream != NULL )
        {
            BYTE      abyChunk[32];
            ULONG     nBytesRead;
    
            do 
            {
                pIStream->Read( abyChunk, sizeof(abyChunk), &nBytesRead );
                if( nBytesRead > 0 )
                {
                    nSize += nBytesRead;
                    pRawData = (BYTE *) 
                        CoTaskMemRealloc(pRawData, nSize);

                    memcpy( pRawData + nSize - nBytesRead, 
                            abyChunk, nBytesRead );
                }
            }
            while( nBytesRead == sizeof(abyChunk) );
    
            pIStream->Release();

            CPLDebug( "OGR_OLEDB", "Read %d bytes from stream.", nSize );
        }

        if( nSize > 0 )
        {
            eErr = 
                OGRGeometryFactory::createFromWkb( pRawData, NULL, &poGeometry,
                                                   nSize );
            CPLDebug( "OGR_OLEDB", "createFromWkb() = %d/%p\n", 
                      eErr, poGeometry );
        }

        if( nSize == 0 || eErr != OGRERR_NONE )
            CPLDebug("OGR_OLEDB", 
                     "Corrupt IUNKNOWN VARIANT WKB in ExtractSpatialQuery().");
    }

    else
    {
        CPLDebug( "OGR_OLEDB", 
                  "Unsupported geometry column type %d in ExtractSpatialQuery()." );
    }

/* -------------------------------------------------------------------- */
/*      Extract the operation.                                          */
/* -------------------------------------------------------------------- */

    eFilterOp = (DBPROPOGISENUM) 
        *((int *) (((unsigned char *) pParams->pData) 
                   + rgBindings[1].obValue));

    if( poGeometry != NULL )
    {
        OGREnvelope sEnvelope;
        
        poGeometry->getEnvelope( &sEnvelope );
        CPLDebug( "OGR_OLEDB", 
                  "Using %d spatial query with extents:\n"
                  "  xmin=%.4f, ymin=%.4f, xmax=%.4f, ymax=%.4f\n",
                  eFilterOp, 
                  sEnvelope.MinX, sEnvelope.MinY,
                  sEnvelope.MaxX, sEnvelope.MaxY );
    }

    return S_OK;
}

/************************************************************************/
/*                             ~CSFRowset()                             */
/************************************************************************/

CSFRowset::~CSFRowset()

{
    CPLDebug( "OGR_OLEDB", "~CSFRowset()" );
}

/************************************************************************/
/*                            ParseCommand()                            */
/*                                                                      */
/*      For now this method just extracts the list of selected          */
/*      fields, and sets them in m_panOGRIndex.                         */
/************************************************************************/

int CSFRowset::ParseCommand( const char *pszCommand, 
                             OGRLayer *poLayer )

{
    char      **papszTokens;
    OGRFeatureDefn *poDefn = poLayer->GetLayerDefn();

    papszTokens = CSLTokenizeStringComplex( pszCommand, " ,", 
                                            FALSE, FALSE );

    if( CSLCount(papszTokens) > 1 
        && EQUAL(papszTokens[0],"SELECT") 
        && !EQUAL(papszTokens[1],"*") )
    {
        int      iToken, iOGRIndex;

        for( iToken = 1; 
             papszTokens[iToken] != NULL && !EQUAL(papszTokens[iToken],"FROM");
             iToken++ )
        {
            iOGRIndex = poDefn->GetFieldIndex( papszTokens[iToken] );

            if( EQUAL(papszTokens[iToken],"FID") )
            {
                iOGRIndex = -1;
                m_panOGRIndex.Add( iOGRIndex );
            }
            else if( EQUAL(papszTokens[iToken],"OGIS_GEOMETRY") )
            {
                iOGRIndex = -2;
                m_panOGRIndex.Add( iOGRIndex );
            }
            else if( iOGRIndex == -1 )
            {
                CPLDebug( "OGR_OLEDB", "Unrecognised field `%s', skipping.", 
                          papszTokens[iToken] );
            }
            else
                m_panOGRIndex.Add( iOGRIndex );
        }
    }
    else
    {
        int      iOGRIndex;

        iOGRIndex = -1;
        m_panOGRIndex.Add(iOGRIndex);

        for( iOGRIndex = 0; iOGRIndex < poDefn->GetFieldCount(); iOGRIndex++ ) 
            m_panOGRIndex.Add(iOGRIndex);
        
        iOGRIndex = -2;
        m_panOGRIndex.Add(iOGRIndex);
    }

    CSLDestroy(papszTokens);

    return TRUE;
}

/************************************************************************/
/*                         CSFRowset::Execute()                         */
/************************************************************************/

HRESULT CSFRowset::Execute(DBPARAMS * pParams, LONG* pcRowsAffected)
{	
    USES_CONVERSION;
	
    // Get the appropriate Data Source
    int         nOGRIndex;
    OGRDataSource *poDS;
    char	*pszCommand;
    char        *pszLayerName;
    char        *pszWHERE = NULL;
    IUnknown    *pIUnknown;
    QueryInterface(IID_IUnknown,(void **) &pIUnknown);
    poDS = SFGetOGRDataSource(pIUnknown);
    assert(poDS);
	
    // Get the appropriate layer, spatial filters and name filtering here!
    OGRLayer	*pLayer;
    OGRGeometry *pGeomFilter = NULL;
	
    pszCommand = OLE2A(m_strCommandText);
    CPLDebug( "OGR_OLEDB", "CSFRowset::Execute(%s)", pszCommand );

/* -------------------------------------------------------------------- */
/*      Try to extract a layer from the command.  For now our           */
/*      handling for SQL is very primitive.                             */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszCommand,"SELECT ",7) )
    {
        int      iCmdOffset;

        pszLayerName = NULL;
        for( iCmdOffset = 0; pszCommand[iCmdOffset] != NULL; iCmdOffset++ )
        {
            if( EQUALN(pszCommand+iCmdOffset,"FROM ",4) )
            {
                iCmdOffset += 4;
                while( pszCommand[iCmdOffset] == ' ' )
                    iCmdOffset++;

                if( pszCommand[iCmdOffset] == '.' )
                    iCmdOffset++;
                
                pszLayerName = CPLStrdup(pszCommand+iCmdOffset);
                for( int iLN = 0; pszLayerName[iLN] != '\0'; iLN++ )
                {
                    if( pszLayerName[iLN] == ' ' )
                    {
                        pszLayerName[iLN] = '\0';
                        break;
                    }
                }
                CPLDebug( "OGR_OLEDB", 
                          "Parsed layer name %s out of SQL statement.",
                          pszLayerName );
            }

            else if( EQUALN(pszCommand+iCmdOffset,"WHERE ",6) )
            {
                iCmdOffset += 6;
                while( pszCommand[iCmdOffset] == ' ' )
                    iCmdOffset++;

                pszWHERE = CPLStrdup( pszCommand + iCmdOffset );
                CPLDebug( "OGR_OLEDB", 
                          "Parsed WHERE clause `%s' out of SQL Statement.", 
                          pszWHERE );
                break;
            }
        }
    }
    else
    {
        pszLayerName = CPLStrdup( pszCommand );
    }
	
    if (pszLayerName == NULL)
    {
        return SFReportError(DB_E_ERRORSINCOMMAND,IID_IUnknown,0,
                             "Unable to extract layer name from SQL statement.");
    }

    // Now check to see which layer is specified.
    int i;
	
    for (i=0; i < poDS->GetLayerCount(); i++)
    {
        pLayer = poDS->GetLayer(i);
		
        OGRFeatureDefn *poDefn = pLayer->GetLayerDefn();
		
        if (!stricmp(pszLayerName,poDefn->GetName()))
        {
            break;
        }
        pLayer = NULL;
    }

    CPLFree( pszLayerName );
	
    // Make sure a valid layer was found!
    if (pLayer == NULL)
    {
        return SFReportError(DB_E_ERRORSINCOMMAND,IID_IUnknown,0,
                             "Invalid Layer Name");
    }

    m_poDS = poDS;
    m_iLayer = i;
    
    // extract list of fields requested
    
    ParseCommand( pszCommand, pLayer );

    // Now that we have a layer set a filter if necessary.
    if (poGeometry)
        pLayer->SetSpatialFilter(poGeometry);
    else
        pLayer->SetSpatialFilter(NULL);

    if( pszWHERE != NULL )
        pLayer->SetAttributeFilter( pszWHERE );
    else
        pLayer->SetAttributeFilter( NULL );
	
    // Get count
    int      nTotalRows;

    nTotalRows = pLayer->GetFeatureCount(TRUE);
    if (pcRowsAffected)
        *pcRowsAffected = nTotalRows;

    // Setup to define fields. 
    int  iField;
    int nOffset = 0;
    ATLCOLUMNINFO colInfo;
    OGRFeatureDefn *poDefn = pLayer->GetLayerDefn();

    // define all fields.
    for( iField = 0; iField < m_panOGRIndex.GetSize(); iField++ )
    {
        int      nOGRIndex = m_panOGRIndex[iField];

        memset(&colInfo, 0, sizeof(ATLCOLUMNINFO));

        // Add the FID column.
        if( nOGRIndex == -1 )
        {
            colInfo.pwszName = ::SysAllocString(A2OLE("FID"));
            colInfo.iOrdinal = iField+1;
            colInfo.dwFlags  = 0;
            colInfo.columnid.uName.pwszName = colInfo.pwszName;
            colInfo.cbOffset	= nOffset;
            colInfo.bScale	= ~0;
            colInfo.bPrecision  = ~0;
            colInfo.ulColumnSize = 4;
            colInfo.wType = DBTYPE_I4;
            
            nOffset += 8; // keep 8byte aligned.
            m_paColInfo.Add(colInfo);
        }

        // Geometry field.
        else if( nOGRIndex == -2 )
        {
#ifdef BLOB_IUNKNOWN	
            colInfo.pwszName	= ::SysAllocString(A2OLE("OGIS_GEOMETRY"));
            colInfo.iOrdinal	= iField+1;
            colInfo.dwFlags		= DBCOLUMNFLAGS_ISFIXEDLENGTH|DBCOLUMNFLAGS_MAYBENULL|DBCOLUMNFLAGS_ISNULLABLE;
            colInfo.ulColumnSize= 4;
            colInfo.bPrecision  = ~0;
            colInfo.bScale	= ~0;
            colInfo.columnid.uName.pwszName = colInfo.pwszName;
            colInfo.cbOffset	= nOffset;
            colInfo.wType	= DBTYPE_IUNKNOWN;
            nOffset += 4;
            
            m_paColInfo.Add(colInfo);
#endif

#ifdef BLOB_BYTES
            colInfo.pwszName	= ::SysAllocString(A2OLE("OGIS_GEOMETRY"));
            colInfo.iOrdinal	= iField+1;
            colInfo.dwFlags	= DBCOLUMNFLAGS_MAYBENULL|DBCOLUMNFLAGS_ISNULLABLE;
            colInfo.ulColumnSize= 50000;
            colInfo.bPrecision  = ~0;
            colInfo.bScale	= ~0;
            colInfo.columnid.uName.pwszName = colInfo.pwszName;
            colInfo.cbOffset	= nOffset;
            colInfo.wType	= DBTYPE_BYTES;
            nOffset += colInfo.ulColumnSize;
            
            m_paColInfo.Add(colInfo);
#endif
        }
        
        else
        {
            OGRFieldDefn	*poField;
		
            poField = poDefn->GetFieldDefn(nOGRIndex);
		
            memset(&colInfo, 0, sizeof(ATLCOLUMNINFO));
		
            colInfo.pwszName      = ::SysAllocString(A2OLE(poField->GetNameRef()));
            colInfo.iOrdinal	= iField+1;
            colInfo.dwFlags	= DBCOLUMNFLAGS_ISFIXEDLENGTH;
            colInfo.columnid.uName.pwszName = colInfo.pwszName;
            colInfo.cbOffset	= nOffset;
            colInfo.bScale	= ~0;
            colInfo.bPrecision  = ~0;
            
            switch(poField->GetType())
            {
                case OFTInteger:
                    colInfo.ulColumnSize = 4;
                    colInfo.wType = DBTYPE_I4;
                    nOffset += 8; // Make everything 8byte aligned
                    if( poField->GetWidth() != 0 )
                        colInfo.bPrecision = poField->GetWidth();
                    break;

                case OFTReal:
                    colInfo.wType = DBTYPE_R8;
                    colInfo.ulColumnSize = 8;
                    nOffset += 8;
                    break;

                case OFTString:
                    colInfo.wType	     = DBTYPE_STR;
                    colInfo.ulColumnSize = poField->GetWidth() == 0 ? STRING_BUFFER_SIZE-1 : poField->GetWidth();
                    colInfo.dwFlags = 0;
                    nOffset += (((colInfo.ulColumnSize+1) / 8) + 1) * 8;
                    break;

                case OFTIntegerList:
                case OFTRealList:
                case OFTStringList:
                    colInfo.wType = DBTYPE_STR;
                    colInfo.ulColumnSize = 80;
                    nOffset += (((colInfo.ulColumnSize+1) / 8) + 1) * 8;
                    colInfo.dwFlags = 0;
                    break;
                
                default:
                    assert(FALSE);
            }

            m_paColInfo.Add(colInfo);
        }

        CPLDebug( "OGR_OLEDB", "Defined field `%S'", colInfo.pwszName );
    }

    m_rgRowData.Initialize(nTotalRows,pLayer,nOffset,this);

    return S_OK;
}
