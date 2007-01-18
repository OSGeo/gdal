/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRVirtualArray/CSFCommand (OLE DB records reader) implementation.
 * Author:   Ken Shih, kshih@home.com
 *           Frank Warmerdam, warmerrdam@pobox.com
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
 ****************************************************************************/

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

CPL_C_START
#include "swq.h"
CPL_C_END

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

#ifdef notdef
    // clean up spatial filter geometry if still hanging around.
    if( m_poGeometry != NULL )
    {
        OGRGeometryFactory::destroyGeometry( m_poGeometry );
        m_poGeometry = NULL;
    }
#endif
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

#ifdef notdef
    // clean up spatial filter geometry from previous execute
    if( m_poGeometry != NULL )
    {
        OGRGeometryFactory::destroyGeometry( m_poGeometry );
        m_poGeometry = NULL;
    }
#endif

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
    if( pRowset )
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
                             "%s", 
                            "Too many parameters to command, only 1 allowed.");
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

    if( rgBindings[0].wType == DBTYPE_VARIANT 
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
/*                             CSFRowset()                              */
/************************************************************************/

CSFRowset::CSFRowset()

{
    m_iLayer = -1;
    m_poDS = NULL;
    m_poLayer = NULL;
}

/************************************************************************/
/*                             ~CSFRowset()                             */
/************************************************************************/

CSFRowset::~CSFRowset()

{
    // clear destination.
    CopyColumnInfo( NULL, &m_paColInfo );

    if( m_poLayer != NULL && m_iLayer == -1 )
        delete m_poLayer;

#ifdef notdef
    // clean up spatial filter geometry if still hanging around.
    if( m_poGeometry != NULL )
    {
        OGRGeometryFactory::destroyGeometry( m_poGeometry );
        m_poGeometry = NULL;
    }
#endif

    CPLDebug( "OGR_OLEDB", "~CSFRowset()" );
}

/************************************************************************/
/*                        ProcessSpecialFields()                        */
/*                                                                      */
/*      The FID and OGIS_GEOMETRY fields aren't real fields as far      */
/*      as the underlying OGR code is concerned so we extract them      */
/*      from the list of requested fields (if present) and return an    */
/*      indication to the provider of whether it should add these       */
/*      special fields.                                                 */
/*                                                                      */
/*      Note that FID and OGIS_GEOMETRY will not work properly in       */
/*      the WHERE or ORDER BY clauses because of their special          */
/*      outside-ogr handling.                                           */
/************************************************************************/

char *CSFRowset::ProcessSpecialFields( const char *pszRawCommand, 
                                       int *pbAddGeometry,
                                       int* pnGeometryIndex )

{
    swq_select *select_info = NULL;
    const char *error;
    int        i;

/* -------------------------------------------------------------------- */
/*      Preparse the statement.                                         */
/* -------------------------------------------------------------------- */
    error = swq_select_preparse( pszRawCommand, &select_info );
    if( error != NULL )
    {
        CPLDebug( "OLEDB", "swq_select_preparse() failed, leaving command.");
        return CPLStrdup( pszRawCommand );
    }

/* -------------------------------------------------------------------- */
/*      Expand "SELECT *" to have a list of fields.  We ensure that     */
/*      FID and OGIS_GEOMETRY will be included.  We do this because     */
/*      the default OGRGenSQLResultLayer support won't include FID      */
/*      unless explicitly requested.                                    */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = NULL; 

    for( i = 0; i < m_poDS->GetLayerCount(); i++ )
    {
        if( EQUAL(m_poDS->GetLayer(i)->GetLayerDefn()->GetName(),
                  select_info->table_defs[0].table_name) )
        {
            poLayer = m_poDS->GetLayer(i);
            break;
        }
    }

    if( poLayer != NULL )
    {
        swq_field_list sFieldList;
        char **papszFieldNames;
        int  nFieldCount = poLayer->GetLayerDefn()->GetFieldCount() + 2;

        memset( &sFieldList, 0, sizeof(sFieldList) );

        sFieldList.count = nFieldCount;
        sFieldList.names = (char **) CPLMalloc(sizeof(char *)*(nFieldCount+1));

        sFieldList.names[0] = "FID";
        for( i = 0; i < nFieldCount-2; i++ )
            sFieldList.names[i+1] = (char *) 
                poLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef();

        sFieldList.names[nFieldCount-1] = "OGIS_GEOMETRY";

        swq_select_expand_wildcard( select_info, &sFieldList );
        
        CPLFree( sFieldList.names );
    }
    
/* -------------------------------------------------------------------- */
/*      Now go back and strip out any OGIS_GEOMETRY occurances,         */
/*      since we have to handle that ourselves.                         */
/* -------------------------------------------------------------------- */
    *pbAddGeometry = FALSE;

    for( i = 0; i < select_info->result_columns; i++ )
    {
        swq_col_def *def = select_info->column_defs + i;

        if( def->col_func_name == NULL
                 && (stricmp(def->field_name,"OGIS_GEOMETRY") == 0 ) )
        {
            *pbAddGeometry = TRUE;
            *pnGeometryIndex = i;

            /* Strip one item out of the list of columns */
            swq_free( def->field_name );
            memmove( def, def + 1, 
                     sizeof(swq_col_def) * (select_info->result_columns-i-1));
            select_info->result_columns--;
            i--;
        }
    }

    char *new_command;

    swq_reform_command( select_info );
    new_command = CPLStrdup( select_info->raw_select );
    swq_select_free( select_info );

    CPLDebug( "OGR_OLEDB", "Reformed statement as:%s\n", new_command );
    
    return new_command;
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
    IUnknown    *pIUnknown;
    int         bAddGeometry = TRUE;
    int		nGeometryIndex = -1;
    int         bAddFID = FALSE;

    {
	IRowsetInfo *pRInfo = NULL;
        HRESULT hr;

	hr = QueryInterface(IID_IRowsetInfo, (void **) &pRInfo);
        CPLDebug( "OGR_OLEDB", "CSFRowset::Execute() IRowsetInfo=%p/%d",
                  pRInfo, hr );
        if( pRInfo != NULL )
            pRInfo->Release();
    }

    QueryInterface(IID_IUnknown,(void **) &pIUnknown);
    poDS = SFGetOGRDataSource(pIUnknown);
    
    assert(poDS);
    if( poDS == NULL )
    {
        CPLDebug( "OGR_OLEDB", "Yikes! %d", *(((unsigned char *) poDS)) );
        return E_FAIL;
    }
	
    pszCommand = OLE2A(m_strCommandText);
    CPLDebug( "OGR_OLEDB", "CSFRowset::Execute(%s)", pszCommand );

    if( m_poLayer != NULL && m_iLayer == -1 )
        delete m_poLayer;

    m_iLayer = -1;
    m_poLayer = NULL;
    m_poDS = poDS;
    
/* -------------------------------------------------------------------- */
/*      Does the command start with select?  If so, generate a          */
/*      synthetic layer.                                                */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszCommand,"SELECT",6) )
    {
        char *pszCleanCommand;

        pszCleanCommand = 
            ProcessSpecialFields( pszCommand, &bAddGeometry, &nGeometryIndex );

        m_poLayer = m_poDS->ExecuteSQL( pszCleanCommand, poGeometry, NULL );
        CPLFree( pszCleanCommand );

        if( m_poLayer == NULL )
        {
            return SFReportError(DB_E_ERRORSINCOMMAND,IID_IUnknown,0,
                                 "%s", CPLGetLastErrorMsg() );
        }
    }
/* -------------------------------------------------------------------- */
/*      Otherwise we assume it is a simple table name, and we grab it.  */
/* -------------------------------------------------------------------- */
    else
    {
        for ( int i=0; i < poDS->GetLayerCount(); i++)
        {
            m_poLayer = poDS->GetLayer(i);
            
            if( stricmp(pszCommand,
                         m_poLayer->GetLayerDefn()->GetName()) == 0 )
            {
                m_iLayer = i;
                break;
            }
        }

        if( m_iLayer == -1 )
        {
            m_poLayer = NULL;
            return SFReportError(DB_E_ERRORSINCOMMAND,IID_IUnknown,0,
                                 "Invalid Layer Name: %s", pszCommand );
        }

        m_poLayer->SetSpatialFilter(poGeometry);
        bAddFID = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Setup field map.  We use all fields plus FID and OGIS_GEOMETRY. */
/* -------------------------------------------------------------------- */
    int      iOGRIndex;
    OGRFeatureDefn *poDefn = m_poLayer->GetLayerDefn();

    // Clear index
    m_panOGRIndex.RemoveAll();
    int nIndex = 0;

    /* FID */
    if( bAddFID )
    {
        iOGRIndex = -1;
        m_panOGRIndex.Add(iOGRIndex);
        nIndex++;
    }

    /* All the regular attributes */
    for( iOGRIndex = 0; iOGRIndex < poDefn->GetFieldCount(); iOGRIndex++ ) 
    {
        // Check if the geometry column is supposed to go here
        if(nGeometryIndex == nIndex)
        {
            // The geometry column needs to go here
            int nGeomIndex = -2;
            m_panOGRIndex.Add(nGeomIndex);
            
            bAddGeometry = FALSE;
        }
        
        // Add the column
        m_panOGRIndex.Add(iOGRIndex);
        nIndex++;
    }
        
    /* OGIS_GEOMETRY */
    if( bAddGeometry )
    {
        iOGRIndex = -2;
        m_panOGRIndex.Add(iOGRIndex);
    }

/* -------------------------------------------------------------------- */
/*      Try and count the records.                                      */
/* -------------------------------------------------------------------- */
    if (pcRowsAffected)
    {
        int      nTotalRows;

        nTotalRows = m_poLayer->GetFeatureCount( FALSE );
        if( nTotalRows != -1 )
        {
            if( pcRowsAffected != NULL )
                *pcRowsAffected = nTotalRows;
        }
        else
            CPLDebug( "OGR_OLEDB", 
                      "Couldn't get feature count cheaply for %s,\n"
                      "not setting *pcRowsAffected.  Should be OK.", 
                      pszCommand );
    }

/* -------------------------------------------------------------------- */
/*      Define column information for each field.                       */
/* -------------------------------------------------------------------- */
    int  iField;
    int nOffset = 0;
    ATLCOLUMNINFO colInfo;

    poDefn = m_poLayer->GetLayerDefn();

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
            colInfo.columnid.eKind = DBKIND_NAME;
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
            colInfo.columnid.eKind = DBKIND_NAME;
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
            colInfo.columnid.eKind = DBKIND_NAME;
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
            colInfo.columnid.eKind = DBKIND_NAME;
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

    m_rgRowData.Initialize(m_poLayer,nOffset,this);

    return S_OK;
}
