/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRVirtualArray/CSFCommand (OLE DB records reader) implementation.
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
 * Revision 1.36  2002/04/17 19:53:17  warmerda
 * added SELECT COUNT(*) support
 *
 * Revision 1.35  2002/04/16 21:02:18  warmerda
 * copy columninfo to CSFCommand from rowset after executing a command
 *
 * Revision 1.34  2002/02/05 20:43:23  warmerda
 * moved SFIStream and VirtualArray classes into their own files
 *
 * Revision 1.33  2002/01/31 16:48:15  warmerda
 * removed need for getting feature count for a rowset
 *
 * Revision 1.32  2002/01/11 20:36:31  warmerda
 * set ISLONG flag on geometry column to indicate use of streams
 *
 * Revision 1.31  2001/11/27 21:05:04  warmerda
 * ensure pLayer is initialized
 *
 * Revision 1.30  2001/11/19 21:03:38  warmerda
 * fix a few minor memory leaks
 *
 * Revision 1.29  2001/11/02 19:24:42  warmerda
 * avoid warnings
 *
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
#include "cpl_error.h"
#include "stdafx.h"
#include "SF.h"
#include "SFRS.h"
#include "SFSess.h"
#include "ogr_geometry.h"
#include "sfutil.h"
#include "cpl_conv.h"
#include "cpl_string.h"

// I use a length of 1024, because anything larger will trigger treatment
// as a BLOB by the code in CDynamicAccessor::BindColumns() in ATLDBCLI.H.
// Treatment as a BLOB (with an sequential stream object created) results
// in the failure of a later CanConvert() test in 
// IAccessorImpl::ValidateBindsFromMetaData().
#define		STRING_BUFFER_SIZE	1024

void OGRComDebug( const char * pszDebugClass, const char * pszFormat, ... );

// These global variables are a hack to transmit spatial query info from
// the CSFCommand::Execute() method to the CSFRowset::Execute() method.

static OGRGeometry      *poGeometry = NULL;
static DBPROPOGISENUM   eFilterOp = DBPROP_OGIS_ENVELOPE_INTERSECTS;

int g_nNextSFAccessorHandle = 1;

/************************************************************************/
/*                           CopyColumnInfo()                           */
/*                                                                      */
/*      Copy column info from one CSimpleArray<ATLCOLUMNINFO> to        */
/*      another.  If the source is NULL, just cleanup the destination.  */
/************************************************************************/

static void CopyColumnInfo( CSimpleArray<ATLCOLUMNINFO> *paSource, 
                            CSimpleArray<ATLCOLUMNINFO> *paDest )

{
    int   i;

/* -------------------------------------------------------------------- */
/*      Clear the destination array.                                    */
/* -------------------------------------------------------------------- */
    for( i = 0; i < paDest->GetSize(); i++)
        SysFreeString( (*paDest)[i].pwszName );

    paDest->RemoveAll();

    if( paSource == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Copy the source array.                                          */
/* -------------------------------------------------------------------- */
    for( i = 0; i < paSource->GetSize(); i++)
    {
        paDest->Add( (*paSource)[i] );
        (*paDest)[i].pwszName = ::SysAllocString( (*paDest)[i].pwszName );
    }
}

/************************************************************************/
/*                      CSFCommand::FinalRelease()                      */
/************************************************************************/
void CSFCommand::FinalRelease()
{
    SFAccessorImpl<CSFCommand>::FinalRelease();
    
    // clear destination.
    CopyColumnInfo( NULL, &m_paColInfo );
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

    // copy the column information from the rowset to the command.
    CopyColumnInfo( &(pRowset->m_paColInfo), &m_paColInfo );

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

    for( iBinding = 0; iBinding < (int) cBindings; iBinding++ )
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
            CoTaskMemFree( pRawData );
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
    // clear destination.
    CopyColumnInfo( NULL, &m_paColInfo );

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
            else if( EQUAL(papszTokens[iToken],"count(*)") )
            {
                iOGRIndex = -3;
                m_panOGRIndex.Add( iOGRIndex );
                m_nResultCount = 0;
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
    OGRDataSource *poDS;
    char	*pszCommand;
    char        *pszLayerName;
    char        *pszWHERE = NULL;
    IUnknown    *pIUnknown;

    m_nResultCount = -1;

    QueryInterface(IID_IUnknown,(void **) &pIUnknown);
    poDS = SFGetOGRDataSource(pIUnknown);
    assert(poDS);
	
    // Get the appropriate layer, spatial filters and name filtering here!
    OGRLayer	*pLayer = NULL;
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
    {
        pLayer->SetAttributeFilter( pszWHERE );
        CPLFree( pszWHERE );
    }
    else
        pLayer->SetAttributeFilter( NULL );
	
    if (pcRowsAffected || m_nResultCount != -1 )
    {
        int      nTotalRows;

        nTotalRows = pLayer->GetFeatureCount( m_nResultCount != -1 );
        if( nTotalRows != -1 )
        {
            if( pcRowsAffected != NULL )
                *pcRowsAffected = nTotalRows;
            if( m_nResultCount != -1 )
            {
                m_nResultCount = nTotalRows;
                if( pcRowsAffected != NULL )
                    *pcRowsAffected = nTotalRows;
            }
        }
        else
            CPLDebug( "OGR_OLEDB", 
                      "Couldn't get feature count cheaply for %s,\n"
                      "not setting *pcRowsAffected.  Should be OK.", 
                      pszCommand );
    }

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
            colInfo.dwFlags		= DBCOLUMNFLAGS_ISFIXEDLENGTH|DBCOLUMNFLAGS_MAYBENULL|DBCOLUMNFLAGS_ISNULLABLE|DBCOLUMNFLAGS_ISLONG;
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
        
        // Add the COUNT column.
        else if( nOGRIndex == -3 )
        {
            colInfo.pwszName = ::SysAllocString(A2OLE("COUNT"));
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

    m_rgRowData.Initialize(pLayer,nOffset,this);

    return S_OK;
}
