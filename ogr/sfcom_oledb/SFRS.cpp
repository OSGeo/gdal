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

// I use a length of 1024, because anything larger will trigger treatment
// as a BLOB by the code in CDynamicAccessor::BindColumns() in ATLDBCLI.H.
// Treatment as a BLOB (with an sequential stream object created) results
// in the failure of a later CanConvert() test in 
// IAccessorImpl::ValidateBindsFromMetaData().
#define		STRING_BUFFER_SIZE	1024

void OGRComDebug( const char * pszDebugClass, const char * pszFormat, ... );


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
            m_cRef   = 1;
            m_pStream  = pByte;
            m_nSize    = nStreamSize;
            m_nSeekPos = 0;
	}

    ~SFIStream()
	{
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
            }
            else
            {
                *ppv = 0;
                return E_NOINTERFACE;
            }

            return NOERROR;
	};

    ULONG STDMETHODCALLTYPE		AddRef (void) {return ++m_cRef;};    
    ULONG STDMETHODCALLTYPE		Release (void )
	{
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

            m_nSeekPos = nNewPos;
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


ATLCOLUMNINFO CShapeFile::colInfo;

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
	m_nLastRecordAccessed = -1;
	m_nPackedRecordLength = 0;
	m_nArraySize = 0;
}

/************************************************************************/
/*                           ~CVirtualArray()                           */
/************************************************************************/

CVirtualArray::~CVirtualArray()
{
	free(mBuffer);
}

/************************************************************************/
/*                      CVirtualArray::RemoveAll()                      */
/************************************************************************/

void CVirtualArray::RemoveAll()
{
	
}

/************************************************************************/
/*                     CVirtualArray::Initialize()                      */
/*                                                                      */
/*      Initialize the record cache.                                    */
/************************************************************************/

void	CVirtualArray::Initialize(int nArraySize, OGRLayer *pLayer,int nBufferSize)
{
	mBuffer			= (BYTE *) malloc(nBufferSize);
	m_nArraySize   = nArraySize;
	m_pOGRLayer    = pLayer;
	m_pFeatureDefn = pLayer->GetLayerDefn();
}

/************************************************************************/
/*                      CVirtualArray::operator[]                       */
/*                                                                      */
/*      Fetch request record from the record cache, possibly having     */
/*      to add it to the cache.                                         */
/************************************************************************/

BYTE &CVirtualArray::operator[](int iIndex) 
{
    // Make sure we are at the correct position to read the next record.
    if (iIndex -1 != m_nLastRecordAccessed)
    {
        m_pOGRLayer->ResetReading();
        m_nLastRecordAccessed = -1;

        while (m_nLastRecordAccessed != iIndex -1)
        {
            delete m_pOGRLayer->GetNextFeature();
        }
    }

    m_nLastRecordAccessed = iIndex;

    OGRFeature *pFeature = m_pOGRLayer->GetNextFeature();

    if (!pFeature)
    {
        // Error condition; // Assertion failure!
        return *mBuffer;
    }

    // Fill the buffer first with field info.
    int i;
    int nOffset = 0;
    for (i=0; i < m_pFeatureDefn->GetFieldCount(); i++)
    {
        OGRFieldDefn *poDefn;

        poDefn = m_pFeatureDefn->GetFieldDefn(i);

        switch(poDefn->GetType())
        {
            case OFTInteger:
                *((int *) &(mBuffer[nOffset])) = pFeature->GetFieldAsInteger(i);
                nOffset += 8;		
                break;
		
            case OFTReal:
                *((double *) &(mBuffer[nOffset])) = pFeature->GetFieldAsDouble(i);
                nOffset += 8;
                break;

            case OFTIntegerList:
            case OFTRealList:
            case OFTStringList:
            {
                int nStringWidth = 80;
                const char *pszStr = pFeature->GetFieldAsString(i);
                strncpy((char *) &(mBuffer[nOffset]),pszStr,nStringWidth);
                mBuffer[nOffset+nStringWidth+1] = 0;
                nOffset += ((((nStringWidth+1)/8)+1)*8);
            }
            break;

            case OFTString:
            {
                int nStringWidth = poDefn->GetWidth() == 0 ? STRING_BUFFER_SIZE -1 : poDefn->GetWidth();

                const char *pszStr = pFeature->GetFieldAsString(i);
                strncpy((char *) &(mBuffer[nOffset]),pszStr,nStringWidth);
                mBuffer[nOffset+nStringWidth+1] = 0;
                nOffset += ((((nStringWidth+1)/8)+1)*8);
            }
            break;
        }
    }

    OGRGeometry *poGeom = pFeature->GetGeometryRef();

    if( poGeom != NULL )
    {
        unsigned char	*pByte  = (unsigned char *) malloc(poGeom->WkbSize());
        poGeom->exportToWkb((OGRwkbByteOrder) 1, pByte);

        IStream	*pStream = new SFIStream(pByte,poGeom->WkbSize());
        *((void **) &(mBuffer[nOffset])) = pStream;
        
        delete pFeature;
    }
    else
    {
        *((void **) &(mBuffer[nOffset])) = NULL;
    }
	
    return (*mBuffer);
}

/************************************************************************/
/*                         CSFComand::Execute()                         */
/************************************************************************/

HRESULT CSFCommand::Execute(IUnknown * pUnkOuter, REFIID riid, 
                            DBPARAMS * pParams,  LONG * pcRowsAffected, 
                            IUnknown ** ppRowset)
{
    CSFRowset* pRowset;
    return CreateRowset(pUnkOuter, riid, pParams, pcRowsAffected, ppRowset, 
                        pRowset);
}




/************************************************************************/
/*                         CSFRowset::Execute()                         */
/************************************************************************/

HRESULT CSFRowset::Execute(DBPARAMS * pParams, LONG* pcRowsAffected)
{	
    USES_CONVERSION;
	
    // Get the appropriate Data Source
    OGRDataSource *poDS;
    char		   *pszCommand;
    IUnknown    *pIUnknown;
    QueryInterface(IID_IUnknown,(void **) &pIUnknown);
    poDS = SFGetOGRDataSource(pIUnknown);
    assert(poDS);
	
    // Get the appropriate layer, spatial filters and name filtering here!
    OGRLayer	*pLayer;
    OGRGeometry *pGeomFilter = NULL;
	
    pszCommand = OLE2A(m_strCommandText);
	
    // If there is a parameter then use it as a spatial filter.
    if (pParams != NULL && pParams->pData != NULL)
    {
        return SFReportError(DB_E_ERRORSINCOMMAND,IID_ICommand,0,
                             "Improper Parameter cannot convert from WKB");

        if (OGRGeometryFactory::createFromWkb(	(unsigned char *) pParams->pData,
                                                NULL,
                                                &pGeomFilter))
        {
            return SFReportError(DB_E_ERRORSINCOMMAND,IID_ICommand,0,"Improper Parameter cannot convert from WKB");
        }
    }
	
	
    // Now check to see which layer is specified.
    int i;
	
    for (i=0; i < poDS->GetLayerCount(); i++)
    {
        pLayer = poDS->GetLayer(i);
		
        OGRFeatureDefn *poDefn = pLayer->GetLayerDefn();
		
        if (!stricmp(pszCommand,poDefn->GetName()))
        {
            break;
        }
        pLayer = NULL;
    }
	
    // Make sure a valid layer was found!
    if (pLayer == NULL)
    {
        return SFReportError(DB_E_ERRORSINCOMMAND,IID_IUnknown,0,
                             "Invalid Layer Name");
    }
	
    // Now that we have a layer set a filter if necessary.
    if (pGeomFilter)
    {
        pLayer->SetSpatialFilter(pGeomFilter);
    }
	
    // Get count
    if (pcRowsAffected)
        *pcRowsAffected = pLayer->GetFeatureCount(TRUE);

    // Now set the column info!
	
    int nOffset = 0;
	
    OGRFeatureDefn *poDefn = pLayer->GetLayerDefn();
	
    for (i=0; i < poDefn->GetFieldCount(); i++)
    {
        OGRFieldDefn	*poField;
		
        poField = poDefn->GetFieldDefn(i);
		
        ATLCOLUMNINFO colInfo;
        memset(&colInfo, 0, sizeof(ATLCOLUMNINFO));
		
        colInfo.pwszName      = ::SysAllocString(A2OLE(poField->GetNameRef()));
        colInfo.iOrdinal	= i+1;
        colInfo.dwFlags		= 0;
        colInfo.columnid.uName.pwszName = colInfo.pwszName;
        colInfo.cbOffset	= nOffset;
        colInfo.bScale		= ~0;
        colInfo.bPrecision  = ~0;
        
		
        switch(poField->GetType())
        {
            case OFTInteger:
                colInfo.ulColumnSize= 4;
                colInfo.wType		= DBTYPE_I4;
                nOffset += 8; // Make everything 8byte aligned
                if( poField->GetWidth() != 0 )
                    colInfo.bPrecision = poField->GetWidth();
                break;

            case OFTReal:
                colInfo.wType		= DBTYPE_R8;
                colInfo.ulColumnSize= 8;
                nOffset += 8;
                break;

            case OFTString:
                colInfo.wType		 = DBTYPE_STR;
                colInfo.ulColumnSize = poField->GetWidth() == 0 ? STRING_BUFFER_SIZE-1 : poField->GetWidth();
                nOffset += (((colInfo.ulColumnSize+1) / 8) + 1) * 8;
                break;

            case OFTIntegerList:
            case OFTRealList:
            case OFTStringList:
                colInfo.wType		 = DBTYPE_STR;
                colInfo.ulColumnSize = 80;
                nOffset += (((colInfo.ulColumnSize+1) / 8) + 1) * 8;
                break;
                
            default:
                assert(FALSE);
        }

        m_paColInfo.Add(colInfo);
    }

    // Set the geometry info

    ATLCOLUMNINFO colInfo;
    memset(&colInfo, 0, sizeof(ATLCOLUMNINFO));
	
    colInfo.pwszName	= ::SysAllocString(A2OLE("OGIS_GEOMETRY"));
    colInfo.iOrdinal	= i+1;
    colInfo.dwFlags		= DBCOLUMNFLAGS_ISFIXEDLENGTH;
    colInfo.ulColumnSize= ~0;
    colInfo.bPrecision  = ~0;
    colInfo.bScale		= ~0;
    colInfo.columnid.uName.pwszName = colInfo.pwszName;
    colInfo.cbOffset	= nOffset;
    colInfo.wType		= (DBTYPE_IUNKNOWN);

    m_paColInfo.Add(colInfo);

    nOffset += 4;
    m_rgRowData.Initialize(*pcRowsAffected,pLayer,nOffset);

    return S_OK;
}
