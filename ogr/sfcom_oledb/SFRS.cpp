/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  CVirtualArray (OLE DB records reader) implementation.
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
 * Revision 1.4  1999/06/04 15:20:00  warmerda
 * Added copyright header.
 *
 */

#include "stdafx.h"
#include "SF.h"
#include "SFRS.h"
#include "ogr_geometry.h"
#include "oledb_sf.h"


OGRGeometry *SHPReadOGRObject( SHPHandle hSHP, int iShape );



#define MIN(a,b) ( a < b ? a : b)




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
            // fill in later		
	
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

    HRESULT STDMETHODCALLTYPE	Read(void *pDest, ULONG cbToRead, ULONG *pcbActuallyRead)
	{
            ULONG pcbDiscardedResult;

            if (!pcbActuallyRead)
                pcbActuallyRead = &pcbDiscardedResult;


            *pcbActuallyRead = MIN(cbToRead, m_nSize - m_nSeekPos);
            memcpy(pDest, m_pStream,*pcbActuallyRead);
            m_nSeekPos+= *pcbActuallyRead;

            return S_OK;
	}

    HRESULT STDMETHODCALLTYPE	Write(void const *pv, ULONG, ULONG *) {return S_FALSE;}

    //	ISTREAM

    HRESULT STDMETHODCALLTYPE	Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPos)
	{
            ULONG nNewPos;

            switch(dwOrigin)
            {
		case STREAM_SEEK_SET:
                    nNewPos = dlibMove.QuadPart;
                    break;
		case STREAM_SEEK_CUR:
                    nNewPos = dlibMove.QuadPart + m_nSeekPos;
                    break;
		case STREAM_SEEK_END:
                    nNewPos = m_nSize - dlibMove.QuadPart;
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
    HRESULT STDMETHODCALLTYPE	CopyTo(IStream *,ULARGE_INTEGER,ULARGE_INTEGER *,ULARGE_INTEGER*) 
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

CVirtualArray::CVirtualArray()
{
    m_ppasArray = NULL;
    m_nArraySize = 0;
    m_hDBFHandle = NULL;
    m_hSHPHandle = NULL;
}


CVirtualArray::~CVirtualArray()
{
	//RemoveAll();
}

void CVirtualArray::RemoveAll()
{

    int i;

    for (i=0; i < m_nArraySize; i++)
    {
        free(m_ppasArray[i]);
    }

    free(m_ppasArray);

    m_ppasArray = NULL;
    m_nArraySize = 0;

    if (m_hDBFHandle)
        DBFClose(m_hDBFHandle);

    if (m_hSHPHandle)
        SHPClose(m_hSHPHandle);

    m_hDBFHandle = NULL;
    m_hSHPHandle = NULL;
}


void	CVirtualArray::Initialize(int nArraySize, DBFHandle hDBF, SHPHandle hSHP)
{
    m_ppasArray   = (BYTE **) calloc(nArraySize, sizeof(BYTE *));
    m_nArraySize  = nArraySize;
    m_hDBFHandle  = hDBF;
    m_hSHPHandle  = hSHP;

    int i;
    int			nOffset = 0;
    SchemaInfo		sInfo;
		
    for (i=0;i < DBFGetFieldCount(hDBF); i++)
    {
        DBFFieldType	eType;
        int				nWidth;

        eType = DBFGetFieldInfo(hDBF,i,NULL,&nWidth,NULL);

        sInfo.eType = eType;
        sInfo.nOffset = nOffset;

        switch (eType)
        {
            case FTInteger:	
                nOffset += 4;
                break;
            case FTString:
                nWidth +=1;
                nWidth *= 2;
                nOffset += nWidth;
                if (nWidth %4)
                    nOffset += (4 - (nWidth %4));
                break;
            case FTDouble:
                nOffset += 8;
                break;

        }

        aSchemaInfo.Add(sInfo);
    }


    if (hSHP)
    {
        memset(&sInfo,0,sizeof(SchemaInfo));
        sInfo.nOffset = nOffset;
        aSchemaInfo.Add(sInfo);
    }

    m_nPackedRecordLength = nOffset  // for records
        + 4  // for pointer to BLOB contining Geometry
        + 4; // for size of array pointed to by above buffer;
}


BYTE &CVirtualArray::operator[](int iIndex) 
{
    ATLASSERT(iIndex >=0 && iIndex < m_nArraySize);
    int i;
    BYTE *pBuffer;
    const char   *pszStr;

    if (m_ppasArray[iIndex])
        return *(m_ppasArray[iIndex]);

    pBuffer = m_ppasArray[iIndex] = (BYTE *) calloc(m_nPackedRecordLength,1);

    for (i=0; i < aSchemaInfo.GetSize()-1; i++)
    {
        switch(aSchemaInfo[i].eType)
        {
            case FTInteger:	
                *((int *) &(pBuffer[aSchemaInfo[i].nOffset])) = DBFReadIntegerAttribute(m_hDBFHandle,iIndex,i);
                break;
            case FTDouble:
                *((double *) &(pBuffer[aSchemaInfo[i].nOffset])) = DBFReadDoubleAttribute(m_hDBFHandle,iIndex,i);
                break;
            case FTString:
                pszStr = DBFReadStringAttribute(m_hDBFHandle,iIndex,i);

                strcpy((char *) &(pBuffer[aSchemaInfo[i].nOffset]),pszStr);
                break;
        }
    }


    // Read in the shape and change it to a WKB format.
    OGRGeometry *poShape = SHPReadOGRObject(m_hSHPHandle,iIndex);
    void		*pByte   = malloc(poShape->WkbSize());
    poShape->exportToWkb((OGRwkbByteOrder) 1, (unsigned char *) pByte);
	
#define USE_ISTREAM
#ifdef  USE_ISTREAM
    IStream	*pStream = new SFIStream(pByte,poShape->WkbSize());
    *((void **) &(pBuffer[aSchemaInfo[aSchemaInfo.GetSize()-1].nOffset])) = pStream;
#else
    *((void **) &(pBuffer[aSchemaInfo[aSchemaInfo.GetSize()-1].nOffset])) = pByte;
    *((int *) &(pBuffer[aSchemaInfo[aSchemaInfo.GetSize()-1].nOffset+4])) = poShape->WkbSize();
#endif
	
    delete poShape;
	
    return *(m_ppasArray[iIndex]);
}







/////////////////////////////////////////////////////////////////////////////
// CSFCommand
HRESULT CSFCommand::Execute(IUnknown * pUnkOuter, REFIID riid, DBPARAMS * pParams, 
								 LONG * pcRowsAffected, IUnknown ** ppRowset)
{
    CSFRowset* pRowset;
    return CreateRowset(pUnkOuter, riid, pParams, pcRowsAffected, ppRowset, pRowset);
}


DBTYPE DBFType2OLEType(DBFFieldType eDBFType,int *pnOffset, int nWidth)
{
    DBTYPE eDBType;


    switch(eDBFType)
    {
        case FTString:
            eDBType = DBTYPE_STR;
            nWidth++;
            nWidth *= 2;
            *pnOffset += nWidth;
            if (nWidth %4)
                *pnOffset += (4- (nWidth %4));
            break;
        case FTInteger:
            eDBType = DBTYPE_I4;
            *pnOffset += 4;
            break;
        case FTDouble:
            eDBType = DBTYPE_R8;
            *pnOffset += 8;
            break;
    }

    return eDBType;
}
HRESULT CSFRowset::Execute(DBPARAMS * pParams, LONG* pcRowsAffected)
{	
    USES_CONVERSION;
    LPSTR		szBaseFile = OLE2A(m_strCommandText);
    DBFHandle	hDBF;
    SHPHandle	hSHP;
    char            szFilename[512];
    int			nFields;
    int			i;
    int			nOffset = 0;
	
	
    strcpy(szFilename,szBaseFile);
    strcat(szFilename,".dbf");
    hDBF = DBFOpen(szFilename,"r");
	
    if (!hDBF)
        return DB_E_ERRORSINCOMMAND;

    strcpy(szFilename,szBaseFile);
    strcat(szFilename,".shp");
    hSHP = SHPOpen(szFilename,"r");
	
    if (!hSHP)
        return DB_E_ERRORSINCOMMAND;

    *pcRowsAffected = DBFGetRecordCount(hDBF);
    nFields = DBFGetFieldCount(hDBF);

    for (i=0; i < nFields; i++)
    {
        char	szFieldName[32];
        int		nWidth = 0;
        int		nDecimals = 0;

        DBFFieldType	eType;
		
        eType = DBFGetFieldInfo(hDBF,i,szFieldName,&nWidth, &nDecimals);


        ATLCOLUMNINFO colInfo;
        memset(&colInfo, 0, sizeof(ATLCOLUMNINFO));
		
        colInfo.pwszName	= ::SysAllocString(A2OLE(szFieldName));
        colInfo.iOrdinal	= i+1;
        colInfo.dwFlags		= DBCOLUMNFLAGS_ISFIXEDLENGTH;
        colInfo.ulColumnSize= (nWidth > 8 ? nWidth : 8);
        colInfo.bPrecision  = nDecimals;
        colInfo.bScale		= 0;
        colInfo.columnid.uName.pwszName = colInfo.pwszName;
        colInfo.cbOffset	= nOffset;
        colInfo.wType		= DBFType2OLEType(eType,&nOffset,nWidth);	

        m_paColInfo.Add(colInfo);
    }

	

    ATLCOLUMNINFO colInfo;
    memset(&colInfo, 0, sizeof(ATLCOLUMNINFO));
	
    colInfo.pwszName	= ::SysAllocString(A2OLE("OGIS_GEOMETRY"));
    colInfo.iOrdinal	= i+1;
    colInfo.dwFlags		= DBCOLUMNFLAGS_ISFIXEDLENGTH;
    colInfo.ulColumnSize= 4;
    colInfo.bPrecision  = 0;
    colInfo.bScale		= 0;
    colInfo.columnid.uName.pwszName = colInfo.pwszName;
    colInfo.cbOffset	= nOffset;
    colInfo.wType		= (DBTYPE_BYTES | DBTYPE_BYREF);

#ifdef USE_ISTREAM
    colInfo.wType		= (DBTYPE_IUNKNOWN);
#endif
    m_paColInfo.Add(colInfo);

    m_rgRowData.Initialize(*pcRowsAffected,hDBF,hSHP);

    return S_OK;
}
