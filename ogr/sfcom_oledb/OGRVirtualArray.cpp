/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRVirtualArray implementation, declared in SFRS.h.
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
 * Revision 1.8  2002/09/06 14:28:48  warmerda
 * update debug output a bit
 *
 * Revision 1.7  2002/08/09 21:31:54  warmerda
 * short circuit a case where the cache is already loaded in CheckRows
 *
 * Revision 1.6  2002/05/08 20:34:24  warmerda
 * don't increment lastfeature when GetFeature() fails
 *
 * Revision 1.5  2002/05/02 19:50:52  warmerda
 * added SUPPORT_2D_FLATTEN to ensure 2D geometries
 *
 * Revision 1.4  2002/04/30 19:47:36  warmerda
 * dont lose last character off strings as long as the field width
 *
 * Revision 1.3  2002/04/25 20:15:26  warmerda
 * upgraded to use ExecuteSQL()
 *
 * Revision 1.1  2002/02/05 20:41:50  warmerda
 * New
 *
 */

#include <assert.h>
#include "cpl_error.h"
#include "stdafx.h"
#include "SF.h"
#include "SFRS.h"
#include "SFSess.h"
#include "ogr_geometry.h"
#include "sfutil.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "SFIStream.h"

void OGRComDebug( const char * pszDebugClass, const char * pszFormat, ... );

#undef ROWGET_DEBUG

/************************************************************************/
/* ==================================================================== */
/*                           OGRVirtualArray                            */
/*                                                                      */
/*      This class holds a cache of records from the table.             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           OGRVirtualArray()                          */
/************************************************************************/

OGRVirtualArray::OGRVirtualArray()
{
    mBuffer = NULL;
    m_nBufferSize = 0;
    m_nLastRecordAccessed = -1;
    m_nPackedRecordLength = 0;

    m_nFeatureCacheBase = 0;
    m_nFeatureCacheSize = 0;
    m_papoFeatureCache = NULL;
}

/************************************************************************/
/*                           ~OGRVirtualArray()                         */
/************************************************************************/

OGRVirtualArray::~OGRVirtualArray()
{
    CPLDebug( "OGR_OLEDB", "~OGRVirtualArray()" );

    if( mBuffer != NULL )
	free(mBuffer);

    ResetCache( 0, 0 );
}

/************************************************************************/
/*                             RemoveAll()                              */
/************************************************************************/

void OGRVirtualArray::RemoveAll()
{
    CPLDebug( "OGR_OLEDB", "OGRVirtualArray::RemoveAll()" );
}

/************************************************************************/
/*                             Initialize()                             */
/*                                                                      */
/*      Initialize the record cache.                                    */
/************************************************************************/

void	OGRVirtualArray::Initialize(OGRLayer *pLayer, int nBufferSize, 
                                    CSFRowset *poRowset )
{
    m_nBufferSize  = nBufferSize;
    mBuffer	       = (BYTE *) malloc(nBufferSize);
    m_pOGRLayer    = pLayer;
    m_pFeatureDefn = pLayer->GetLayerDefn();
    m_pRowset      = poRowset;

    CPLDebug( "OGR_OLEDB", "OGRVirtualArray::Initialize()" );
    m_pOGRLayer->ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/*                                                                      */
/*      Fetch the requested feature.  This may come from OGR, or        */
/*      from the local feature cache maintained by the                  */
/*      OGRVirtualArray.                                                */
/************************************************************************/

OGRFeature *OGRVirtualArray::GetFeature( int iIndex )

{
    OGRFeature *poFeature = NULL;

/* ==================================================================== */
/*      Is the feature in our cache?  If so, return it removing it      */
/*      from our cache.                                                 */
/* ==================================================================== */
    if( iIndex >= m_nFeatureCacheBase 
        && iIndex < m_nFeatureCacheBase + m_nFeatureCacheSize 
        && m_papoFeatureCache[iIndex - m_nFeatureCacheBase] != NULL )
    {
        poFeature = m_papoFeatureCache[iIndex - m_nFeatureCacheBase];
        m_papoFeatureCache[iIndex - m_nFeatureCacheBase] = NULL;
        return poFeature;
    }

/* ==================================================================== */
/*      Fetch the feature using conventional "serial" access to an      */
/*      OGRLayer.  It would be nice if this code recognised that        */
/*      some sources support random fetches of features, and took       */
/*      advantage of that.                                              */
/* ==================================================================== */
    // Make sure we are at the correct position to read the next record.
    if (iIndex -1 != m_nLastRecordAccessed)
    {
        int  nSkipped = 0;

        CPLDebug( "OGR_OLEDB", 
                  "%d requested, last was %d, some skipping required.",
                  iIndex, m_nLastRecordAccessed );

        if( m_nLastRecordAccessed >= iIndex )
        {
            m_pOGRLayer->ResetReading();
            m_nLastRecordAccessed = -1;
        }

        while (m_nLastRecordAccessed != iIndex -1)
        {
            poFeature = m_pOGRLayer->GetNextFeature();
            if( poFeature )
            {
                nSkipped++;
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
        CPLDebug( "OGR_OLEDB", "Skipped %d features.", nSkipped );
    }

    if (iIndex -1 != m_nLastRecordAccessed)
    {
        CPLDebug( "OGR_OLEDB", 
                  "Went *PAST* end of dataset requesting feature %d.", 
                  iIndex );
        return NULL;
    }

    poFeature = m_pOGRLayer->GetNextFeature();

    if (!poFeature)
    {
        CPLDebug( "OGR_OLEDB", "Hit end of dataset requesting feature %d.", 
                  iIndex );
        return NULL;
    }

    m_nLastRecordAccessed = iIndex;

    return poFeature;
}

/************************************************************************/
/*                               GetRow()                               */
/*                                                                      */
/*      Fetch request record from the record cache, possibly having     */
/*      to add it to the cache.                                         */
/************************************************************************/

BYTE *OGRVirtualArray::GetRow( int iIndex, HRESULT &hr )
{
    OGRFeature      *poFeature;
#ifdef ROWGET_DEBUG
    CPLDebug( "OGR_OLEDB", "OGRVirtualArray::operator[%d]", iIndex );
#else
    if( iIndex == 0 )
        CPLDebug( "OGR_OLEDB", "OGRVirtualArray::operator[%d] ... getting first row.  Rest will not be reported.", iIndex );
#endif

    hr = S_OK;

    // Pre-initialize output buffer.
    memset( mBuffer, 0, m_nBufferSize );

/* -------------------------------------------------------------------- */
/*      Fetch the feature.  Eventually we should return a real error    */
/*      status depending on the nature of the failure.                  */
/* -------------------------------------------------------------------- */
    poFeature = GetFeature( iIndex );
    if( poFeature == NULL )
    {
        CPLDebug( "OGR_OLEDB", "OGRVirtualArray::operator[%d] ... got NULL from GetFeature(), returning DB_S_ENDOFROWSET.", iIndex );
        hr = DB_S_ENDOFROWSET;
        return NULL;
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

    return mBuffer;
}

/************************************************************************/
/*                            FillGeometry()                            */
/************************************************************************/

int OGRVirtualArray::FillGeometry( OGRGeometry *poGeom, 
                                 unsigned char *pabyBuffer,
                                 ATLCOLUMNINFO *pColInfo )

{
    int      nOffset = pColInfo->cbOffset;
    int      bGeomCopy = FALSE;

    if( poGeom == NULL )
        return TRUE;

#ifdef SUPPORT_2D_FLATTEN
    if( poGeom->getCoordinateDimension() == 3 )
    {
        CPLDebug( "OGR_OLEDB", "Flattening 3D geometry to 2D." );
        poGeom = poGeom->clone();
        poGeom->flattenTo2D();
        bGeomCopy = TRUE;
    }
#endif
    
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

/* -------------------------------------------------------------------- */
/*      Cleanup if a temporary 2D geometry was created.                 */
/* -------------------------------------------------------------------- */
    if( bGeomCopy )
        delete poGeom;

    return TRUE;
}
	
/************************************************************************/
/*                            FillOGRField()                            */
/*                                                                      */
/*      Copy information for one field into the provided buffer from    */
/*      an OGRFeature.                                                  */
/************************************************************************/

int OGRVirtualArray::FillOGRField( OGRFeature *poFeature, int iField, 
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
            int nStringWidth = MIN(strlen(pszStr),pColInfo->ulColumnSize);

            strncpy((char *) pabyBuffer + nOffset,pszStr,nStringWidth);
            if( nStringWidth < (int) pColInfo->ulColumnSize )
                pabyBuffer[nOffset+nStringWidth] = '\0';
        }
        break;
    }

    return TRUE;
}

/************************************************************************/
/*                             ResetCache()                             */
/************************************************************************/

void OGRVirtualArray::ResetCache( int nStart, int nSize )

{
    int     i;

/* -------------------------------------------------------------------- */
/*      Clear any existing cache.                                       */
/* -------------------------------------------------------------------- */
    if( m_nFeatureCacheSize > 0 )
    {
        for( i = 0; i < m_nFeatureCacheSize; i++ )
        {
            if( m_papoFeatureCache[i] != NULL )
                delete m_papoFeatureCache[i];
        }

        CPLFree( m_papoFeatureCache );

        m_nFeatureCacheBase = 0;
        m_nFeatureCacheSize = 0;
        m_papoFeatureCache = NULL;
    }

    if( nSize == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Create a new cache with all null entries.                       */
/* -------------------------------------------------------------------- */
    m_nFeatureCacheBase = nStart;
    m_nFeatureCacheSize = nSize;
    m_papoFeatureCache = (OGRFeature **) CPLCalloc(sizeof(OGRFeature *),nSize);
}

/************************************************************************/
/*                             CheckRows()                              */
/*                                                                      */
/*      This method is called by the IFRowsetImpl::GetNextRows()        */
/*      method to establish how many out of the requested number of     */
/*      rows will actually be available if the end of the rowset        */
/*      will be struck.   Since we don't know the size of our layer     */
/*      result we have to fetch the features (to see if they are        */
/*      there) and rather than have to re-read them later we cache      */
/*      them as OGRFeatures within the OGRVirtualArray under the        */
/*      assumption that our GetRow() method will soon be called for     */
/*      them all.                                                       */
/*                                                                      */
/*      In an ideal world we might actually get the result size when    */
/*      the OGRVirtualArray is initialized for cases where it is        */
/*      "cheap" to do so, and then use that to answer the               */
/*      CheckRows() question without having to read and cache the       */
/*      results.  However, that would require substantial rework.       */
/*                                                                      */
/************************************************************************/

int OGRVirtualArray::CheckRows( int nStart, int nRequestCount )

{
    if( nRequestCount > 1 )
        CPLDebug( "OGR_OLEDB", "OGRVirtualArray::CheckRows( %d, %d )", 
                  nStart, nRequestCount );

    if( nStart >= m_nFeatureCacheBase
        && nStart+nRequestCount <= m_nFeatureCacheBase + m_nFeatureCacheSize )
        return nRequestCount;

    // Reset the m_papoFeatureCache stuff to a clean new cache with
    // nRequestCount entries. 
    ResetCache( nStart, nRequestCount );

    // Request features till it fails. 
    for( int i=0; i < nRequestCount; i++ )
    {
        m_papoFeatureCache[i] = GetFeature( i + nStart );
        if( m_papoFeatureCache[i] == NULL )
        {
            // we reached end-of-rowset.  Let the caller know how many 
            // features are available. 
            return i;
        }
    }

    // all the features were available. 
    return nRequestCount;
}


