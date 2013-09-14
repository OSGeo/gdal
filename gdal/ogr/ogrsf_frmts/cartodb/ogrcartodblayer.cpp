/******************************************************************************
 * $Id$
 *
 * Project:  CartoDB Translator
 * Purpose:  Implements OGRCARTODBLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_cartodb.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRCARTODBLayer()                            */
/************************************************************************/

OGRCARTODBLayer::OGRCARTODBLayer(OGRCARTODBDataSource* poDS)

{
    this->poDS = poDS;

    poSRS = NULL;

    poFeatureDefn = NULL;
    
    poCachedObj = NULL;

    ResetReading();
}

/************************************************************************/
/*                         ~OGRCARTODBLayer()                           */
/************************************************************************/

OGRCARTODBLayer::~OGRCARTODBLayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCARTODBLayer::ResetReading()

{
    if( poCachedObj != NULL )
        json_object_put(poCachedObj);
    poCachedObj = NULL;
    bEOF = FALSE;
    nFetchedObjects = -1;
    iNextInFetchedObjects = 0;
    iNext = 0;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRCARTODBLayer::GetLayerDefn()
{
    CPLAssert(poFeatureDefn);
    return poFeatureDefn;
}

/************************************************************************/
/*                          EWKBToGeometry()                            */
/************************************************************************/

static OGRGeometry *EWKBToGeometry( GByte *pabyWKB, int nLength )

{
    OGRGeometry *poGeometry = NULL;
    unsigned int ewkbFlags = 0;
    
    if (nLength < 5)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid EWKB content : %d bytes", nLength );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Detect XYZM variant of PostGIS EWKB                             */
/*                                                                      */
/*      OGR does not support parsing M coordinate,                      */
/*      so we return NULL geometry.                                     */
/* -------------------------------------------------------------------- */
    memcpy(&ewkbFlags, pabyWKB+1, 4);
    OGRwkbByteOrder eByteOrder = (pabyWKB[0] == 0 ? wkbXDR : wkbNDR);
    if( OGR_SWAP( eByteOrder ) )
        ewkbFlags= CPL_SWAP32(ewkbFlags);

    if (ewkbFlags & 0x40000000)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Reading EWKB with 4-dimensional coordinates (XYZM) is not supported" );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      PostGIS EWKB format includes an  SRID, but this won't be        */
/*      understood by OGR, so if the SRID flag is set, we remove the    */
/*      SRID (bytes at offset 5 to 8).                                  */
/* -------------------------------------------------------------------- */
    if( nLength > 9 &&
        ((pabyWKB[0] == 0 /* big endian */ && (pabyWKB[1] & 0x20) )
        || (pabyWKB[0] != 0 /* little endian */ && (pabyWKB[4] & 0x20))) )
    {
        memmove( pabyWKB+5, pabyWKB+9, nLength-9 );
        nLength -= 4;
        if( pabyWKB[0] == 0 )
            pabyWKB[1] &= (~0x20);
        else
            pabyWKB[4] &= (~0x20);
    }

/* -------------------------------------------------------------------- */
/*      Try to ingest the geometry.                                     */
/* -------------------------------------------------------------------- */
    OGRGeometryFactory::createFromWkb( pabyWKB, NULL, &poGeometry, nLength );

    return poGeometry;
}


/************************************************************************/
/*                           HEXToGeometry()                            */
/************************************************************************/

static OGRGeometry *HEXToGeometry( const char *pszBytea )

{
    GByte   *pabyWKB;
    int     nWKBLength=0;
    OGRGeometry *poGeometry;

    if( pszBytea == NULL )
        return NULL;

    pabyWKB = CPLHexToBinary(pszBytea, &nWKBLength);

    poGeometry = EWKBToGeometry(pabyWKB, nWKBLength);

    CPLFree(pabyWKB);

    return poGeometry;
}


/************************************************************************/
/*                           BuildFeature()                             */
/************************************************************************/

OGRFeature *OGRCARTODBLayer::BuildFeature(json_object* poRowObj)
{
    OGRFeature* poFeature = NULL;
    if( poRowObj != NULL &&
        json_object_get_type(poRowObj) == json_type_object )
    {
        poFeature = new OGRFeature(poFeatureDefn);

        if( osFIDColName.size() )
        {
            json_object* poVal = json_object_object_get(poRowObj, osFIDColName);
            if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_int )
            {
                poFeature->SetFID(json_object_get_int64(poVal));
            }
        }
        else
        {
            poFeature->SetFID(iNext);
        }

        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            json_object* poVal = json_object_object_get(poRowObj,
                            poFeatureDefn->GetFieldDefn(i)->GetNameRef());
            if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_string )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDateTime )
                {
                    int nYear, nMonth, nDay, nHour, nMinute, nTZ;
                    float fSecond;
                    if( OGRParseXMLDateTime( json_object_get_string(poVal),
                                  &nYear, &nMonth, &nDay, &nHour, &nMinute, &fSecond, &nTZ) )
                    {
                        poFeature->SetField(i, nYear, nMonth, nDay, nHour, nMinute, (int)fSecond, nTZ);
                    }
                }
                else
                {
                    poFeature->SetField(i, json_object_get_string(poVal));
                }
            }
            else if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_int )
            {
                poFeature->SetField(i, (int)json_object_get_int64(poVal));
            }
            else if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_double )
            {
                poFeature->SetField(i, json_object_get_double(poVal));
            }
        }

        for(int i=0;i<poFeatureDefn->GetGeomFieldCount();i++)
        {
            json_object* poVal = json_object_object_get(poRowObj,
                            poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
            if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_string )
            {
                OGRGeometry* poGeom = HEXToGeometry(json_object_get_string(poVal));
                poFeature->SetGeomFieldDirectly(i, poGeom);
            }
        }
    }
    return poFeature;
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature *OGRCARTODBLayer::GetNextRawFeature()
{
    if( bEOF ) 
        return NULL;

    if( iNextInFetchedObjects >= nFetchedObjects )
    {
        CPLString osSQL = osBaseSQL;
        osSQL += " LIMIT ";
        osSQL += CPLSPrintf("%d", GetFeaturesToFetch());
        osSQL += " OFFSET ";
        osSQL += CPLSPrintf("%d", iNext);
        json_object* poObj = poDS->RunSQL(osSQL);
        if( poObj == NULL )
        {
            bEOF = TRUE;
            return NULL;
        }
        json_object* poRows = json_object_object_get(poObj, "rows");
        if( poRows == NULL ||
            json_object_get_type(poRows) != json_type_array ||
            json_object_array_length(poRows) == 0 )
        {
            json_object_put(poObj);
            bEOF = TRUE;
            return NULL;
        }

        if( poCachedObj != NULL )
            json_object_put(poCachedObj);
        poCachedObj = poObj;

        nFetchedObjects = json_object_array_length(poRows);
        iNextInFetchedObjects = 0;
    }

    json_object* poRows = json_object_object_get(poCachedObj, "rows");
    json_object* poRowObj = json_object_array_get_idx(poRows, iNextInFetchedObjects);

    iNextInFetchedObjects ++;

    OGRFeature* poFeature = BuildFeature(poRowObj);
    iNext ++;

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRCARTODBLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    GetLayerDefn();

    while(TRUE)
    {
        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCARTODBLayer::TestCapability( const char * pszCap )

{
    if ( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    return FALSE;
}
