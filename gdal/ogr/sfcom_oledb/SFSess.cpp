/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implement methods of some of the classes defined in SFSess.h.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.4  2002/09/04 19:08:26  warmerda
 * removed unused variable
 *
 * Revision 1.3  2002/09/04 14:13:34  warmerda
 * convert to unix text mode
 *
 * Revision 1.2  2002/08/30 15:27:54  warmerda
 * Ensure stdafx.h include first
 *
 * Revision 1.1  2002/08/29 18:56:15  warmerda
 * New
 *
 */

#include "stdafx.h"
#include "SF.h"
#include "SFDS.h"

/************************************************************************/
/* ==================================================================== */
/*                    CSFSessionSchemaOGISGeoColumns                    */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           GetRCDBStatus()                            */
/*                                                                      */
/*      Determine whether a given field is NULLed or not.  We have      */
/*      to ensure the ADSK_GEOM_EXTENT is NULL if not known.            */
/************************************************************************/

DBSTATUS CSFSessionSchemaOGISGeoColumns::GetRCDBStatus(CSimpleRow* poRC,
                                                       ATLCOLUMNINFO*poColInfo,
                                                       void *pSrcData)
{
#ifdef SUPPORT_ADSK_GEOM_EXTENT            
    OGISGeometry_Row      *poRow = (OGISGeometry_Row *) pSrcData;

    if( lstrcmpW(poColInfo->pwszName,L"ADSK_GEOM_EXTENT") == 0 )
    {
        if( poRow == NULL )
        {
            CPLDebug( "OGR_OLEDB", "CSFSessionSchemaOGISGeoColumns::GetRCDBStatus() - poRow == NULL" );
            return DBSTATUS_S_OK;
        }
                
        if( poRow->m_pADSK_GEOM_EXTENT == NULL )
            return DBSTATUS_S_ISNULL;
    }
#endif            
    return DBSTATUS_S_OK;
}

/************************************************************************/
/*                              Execute()                               */
/*                                                                      */
/*      Generate rowset.                                                */
/************************************************************************/

HRESULT CSFSessionSchemaOGISGeoColumns::Execute(LONG* pcRowsAffected, 
                                                ULONG cRestrictions, 
                                                const VARIANT* rgRestrictions)
{
    USES_CONVERSION;

    int                iLayer;
    IUnknown        *pIU;
    OGRDataSource    *poDS;
    OGRLayer        *pLayer;
    OGRFeatureDefn    *poDefn;
    CComBSTR bstrTableName;
    CComBSTR bstrColumnName = L"OGIS_GEOMETRY";	// Default geometry column name
    int bTableNameRestriction = FALSE, bColumnNameRestriction = FALSE;
    CSFSource *poCSFSource = NULL;

    CPLDebug( "OGR_OLEDB",
              "CSFSessionSchemaOGISGeoColumns::Execute()." );
                
    QueryInterface(IID_IUnknown,(void **) &pIU);
    poDS = SFGetOGRDataSource(pIU);

    QueryInterface(IID_IUnknown,(void **) &pIU);
    poCSFSource = SFGetCSFSource(pIU);

    if (!poDS || !poCSFSource )
    {
        // Prep errors as well
        return S_FALSE;
    }

    // Get restriction information if supplied
    if(cRestrictions > 0)
    {
	// There are restrictions
        if (cRestrictions >= RESTRICTION_OGISGC_TABLE_CATALOG)
        {
            // Extract TABLE_CATALOG restriction
        }

        if (cRestrictions >= RESTRICTION_OGISGC_TABLE_SCHEMA)
        {
            // Extract TABLE_SCHEMA restriction
        }

        if (cRestrictions >= RESTRICTION_OGISGC_TABLE_NAME)
        {
            // Extract TABLE_NAME restriction
            if(rgRestrictions[RESTRICTION_OGISGC_TABLE_NAME-1].vt != VT_EMPTY)
            {
                BOOL bCheck = FALSE;
                VARIANT vt;
                VariantInit(&vt);
                vt = rgRestrictions[RESTRICTION_OGISGC_TABLE_NAME-1];

                if (vt.vt == VT_BSTR)
                {
                    bCheck = TRUE;
                }
                else
                {
                    // Try and convert it
                    HRESULT hResult = VariantChangeType(&vt,&vt,0,VT_BSTR);
                    if(SUCCEEDED(hResult))
                    {
                        bCheck = TRUE;
                    }
                }

                if(bCheck)
                {
                    bstrTableName = vt.bstrVal;
                    bTableNameRestriction = TRUE;
                }
            }
        }

        if (cRestrictions >= RESTRICTION_OGISGC_COLUMN_NAME)
        {
            // Extract COLUMN_NAME restriction
            if(rgRestrictions[RESTRICTION_OGISGC_COLUMN_NAME-1].vt != VT_EMPTY)
            {
                BOOL bCheck = FALSE;
                VARIANT vt;
                VariantInit(&vt);
                vt = rgRestrictions[RESTRICTION_OGISGC_COLUMN_NAME-1];

                if (vt.vt == VT_BSTR)
                {
                    bCheck = TRUE;
                }
                else
                {
                    // Try and convert it
                    HRESULT hResult = VariantChangeType(&vt,&vt,0,VT_BSTR);
                    if(SUCCEEDED(hResult))
                    {
                        bCheck = TRUE;
                    }
                }

                if(bCheck)
                {
                    bstrColumnName = vt.bstrVal;
                    bColumnNameRestriction = TRUE;
                }
            }
        }

        if (cRestrictions >= RESTRICTION_OGISGC_GEOM_TYPE)
        {
            // Extract GEOM_TYPE restriction
        }

        if (cRestrictions == RESTRICTION_OGISGC_SPATIAL_REF_SYSTEM_ID)
        {
            // Extract SPATIAL_REF_SYSTEM_ID restriction
        }

        if (cRestrictions == RESTRICTION_OGISGC_SPATIAL_EXTENT)
        {
            // Extract SPATIAL_EXTENT restriction
        }
    }

    for (iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++)
    {
        OGISGeometry_Row trData;
        char             *pszWKT = NULL;
        
        pLayer = poDS->GetLayer(iLayer);
        poDefn = pLayer->GetLayerDefn();
        
        lstrcpyW(trData.m_szTableName,A2OLE(poDefn->GetName()));
        lstrcpyW(trData.m_szColumnName,A2OLE("OGIS_GEOMETRY"));

        if( bTableNameRestriction 
            && wcsicmp(bstrTableName, trData.m_szTableName) != 0)
            continue;

        if( bColumnNameRestriction 
            && wcsicmp(bstrColumnName, trData.m_szColumnName) != 0)
            continue;

        trData.m_nGeomType = SFWkbGeomTypeToDBGEOM(poDefn->GetGeomType());
        QueryInterface(IID_IUnknown,(void **) &pIU);
        pszWKT = SFGetLayerWKT( pLayer, pIU );
                
        if( pszWKT != NULL )
        {
            trData.m_nSpatialRefId = poCSFSource->GetSRSID(pszWKT);
            OGRFree( pszWKT );
        }
        else
            trData.m_nSpatialRefId = poCSFSource->GetSRSID("");

#ifdef SUPPORT_ADSK_GEOM_EXTENT    
        OGREnvelope sExtent;
                
        if( pLayer->GetExtent( &sExtent, FALSE ) == OGRERR_NONE )
        {
            OGRPolygon oExtentPoly;
            OGRLinearRing oExtentRing;

            oExtentRing.addPoint( sExtent.MinX, sExtent.MinY );
            oExtentRing.addPoint( sExtent.MinX, sExtent.MaxY );
            oExtentRing.addPoint( sExtent.MaxX, sExtent.MaxY );
            oExtentRing.addPoint( sExtent.MaxX, sExtent.MinY );
            oExtentRing.addPoint( sExtent.MinX, sExtent.MinY );

            oExtentPoly.addRing( &oExtentRing );

            CPLDebug( "FME_OLEDB",
                      "ADSK_GEOM_EXTENT(%f,%f,%f,%f) -> %d bytes",
                      sExtent.MinX, sExtent.MaxX,
                      sExtent.MinY, sExtent.MaxY,
                      oExtentPoly.WkbSize() );
                    
            BYTE abyGeometry[93];
            memset( abyGeometry, 0, sizeof(abyGeometry) );
                    
            oExtentPoly.exportToWkb( wkbNDR, abyGeometry + 0 );

            //20020412 - map to istream - ryan
            IStream    *pIStream;
            HGLOBAL     hMem;
            hMem=GlobalAlloc(GMEM_MOVEABLE, sizeof(abyGeometry));
            CreateStreamOnHGlobal(hMem, TRUE, &pIStream);
            pIStream->Write(abyGeometry, sizeof(abyGeometry), NULL );

            LARGE_INTEGER nLargeZero = { 0,0 };
            pIStream->Seek( nLargeZero, STREAM_SEEK_SET, NULL);

            //20020412 - ryan
            trData.m_pADSK_GEOM_EXTENT = pIStream;
        }
#endif

        m_rgRowData.Add(trData);
    }

    *pcRowsAffected = m_rgRowData.GET_SIZE_MACRO();

    CPLDebug( "OGR_OLEDB",
              "CSFSessionSchemaOGISGeoColumns::Execute() - complete" );

    return S_OK;
}

/************************************************************************/
/* ==================================================================== */
/*                       CSFSessionSchemaSpatRef                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           GetRCDBStatus()                            */
/************************************************************************/

DBSTATUS CSFSessionSchemaSpatRef::GetRCDBStatus(CSimpleRow* poRC,
                                                ATLCOLUMNINFO*poColInfo,
                                                void *pSrcData)
{
    OGISSpat_Row      *poRow = (OGISSpat_Row *) pSrcData;

    if( lstrcmpW(poColInfo->pwszName,L"AUTHORITY_NAME") == 0
        ||lstrcmpW(poColInfo->pwszName,L"AUTHORITY_ID") == 0 )
    {
        if( lstrcmpW(poRow->m_szAuthorityName,L"") == 0 )
            return DBSTATUS_S_ISNULL;
    }
    if( lstrcmpW(poColInfo->pwszName,L"SPATIAL_REF_SYSTEM_WKT") == 0 )
    {
        if( poRow == NULL )
        {
            CPLDebug( "OGR_OLEDB", "CSFSessionSchemaSpatRef::GetRCDBStatus() - poRow == NULL" );
            return DBSTATUS_S_OK;
        }
                
        if( lstrcmpW(poRow->m_pszSpatialRefSystem,L"") == 0 )
            return DBSTATUS_S_ISNULL;
    }
            
    return DBSTATUS_S_OK;
}

/************************************************************************/
/*                              Execute()                               */
/************************************************************************/

HRESULT CSFSessionSchemaSpatRef::Execute(LONG* pcRowsAffected, 
                                         ULONG cRestrictions, 
                                         const VARIANT* rgRestrictions)
{
    USES_CONVERSION;
    bool    bAddDefault = false;

    // See if we can get the Spatial reference system for each layer.
    // It is unclear at the current time what the valid authority id and spatial
    // ref ids are.  
    IUnknown        *pIU;
    CSFSource       *poCSFSource;
    LONG lSRSIDRestriction = -1;

    CPLDebug( "OGR_OLEDB",
              "CSFSessionSchemaSpatRef::Execute()." );
                
    QueryInterface(IID_IUnknown,(void **) &pIU);
    poCSFSource = SFGetCSFSource(pIU);

    if (!poCSFSource)
    {
        // Prep errors as well
        return S_FALSE;
    }

    // Get restriction information if supplied
    if(cRestrictions > 0)
    {
				// There are restrictions
        if (cRestrictions >= RESTRICTION_OGISSR_SRS_ID)
        {
            // Extract SPATIAL_REF_SYSTEM_ID restriction
            if(rgRestrictions[0].vt != VT_EMPTY)
            {
                if (rgRestrictions[0].vt == VT_I4)
                {
                    lSRSIDRestriction = rgRestrictions[0].ulVal;
                }
            }
        }

        if (cRestrictions >= RESTRICTION_OGISSR_AUTHORITY_NAME)
        {
            // Extract AUTHORITY_NAME restriction
        }

        if (cRestrictions >= RESTRICTION_OGISSR_AUTHORITY_ID)
        {
            // Extract AUTHORITY_ID restriction
        }

        if (cRestrictions >= RESTRICTION_OGISSR_SRS_WKT)
        {
            // Extract SPATIAL_REF_SYSTEM_WKT restriction
        }
    }

    for( int iSRS = 0; iSRS < poCSFSource->GetSRSCount(); iSRS++ )
    {
        OGISSpat_Row trData;
        const char *pszWKT = poCSFSource->GetSRSWKT( iSRS );
                
        if( lSRSIDRestriction != -1
            && lSRSIDRestriction != iSRS )
            continue;
                
        lstrcpyW(trData.m_szAuthorityName,A2OLE(""));
        trData.m_nAuthorityId = 0;
        trData.m_nSpatialRefId = iSRS;
        lstrcpyW(trData.m_pszSpatialRefSystem,A2OLE(pszWKT));
                
        m_rgRowData.Add(trData);
    }

    *pcRowsAffected = m_rgRowData.GET_SIZE_MACRO();

    return S_OK;
}
