/******************************************************************************
 * $Id$
 *
 * Project:  CartoDB Translator
 * Purpose:  Implements OGRCARTODBTableLayer class.
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
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                    OGRCARTODBEscapeIdentifier( )                     */
/************************************************************************/

static
CPLString OGRCARTODBEscapeIdentifier(const char* pszColumnName)
{
    CPLString osStr;

    osStr += "\"";

    char ch;
    for(int i=0; (ch = pszColumnName[i]) != '\0'; i++)
    {
        if (ch == '"')
            osStr.append(1, ch);
        osStr.append(1, ch);
    }

    osStr += "\"";

    return osStr;
}

/************************************************************************/
/*                        OGRCARTODBTableLayer()                        */
/************************************************************************/

OGRCARTODBTableLayer::OGRCARTODBTableLayer(OGRCARTODBDataSource* poDS,
                                           const char* pszName) :
                                           OGRCARTODBLayer(poDS)

{
    osName = pszName;
}

/************************************************************************/
/*                    ~OGRCARTODBTableLayer()                           */
/************************************************************************/

OGRCARTODBTableLayer::~OGRCARTODBTableLayer()

{
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRCARTODBTableLayer::GetLayerDefn()
{
    if( poFeatureDefn != NULL )
        return poFeatureDefn;

    poFeatureDefn = new OGRFeatureDefn(osName);
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    osBaseSQL = CPLSPrintf("SELECT * FROM %s",
                           OGRCARTODBEscapeIdentifier(osName).c_str());

    json_object* poObj = poDS->RunSQL(CPLSPrintf("%s LIMIT 0", osBaseSQL.c_str()));
    if( poObj == NULL )
    {
        return poFeatureDefn;
    }

    json_object* poFields = json_object_object_get(poObj, "fields");
    if( poFields == NULL || json_object_get_type(poFields) != json_type_object)
    {
        json_object_put(poObj);
        return poFeatureDefn;
        return FALSE;
    }

    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object_object_foreachC( poFields, it )
    {
        const char* pszColName = it.key;
        if( it.val != NULL && json_object_get_type(it.val) == json_type_object)
        {
            json_object* poType = json_object_object_get(it.val, "type");
            if( poType != NULL && json_object_get_type(poType) == json_type_string )
            {
                const char* pszType = json_object_get_string(poType);
                CPLDebug("CARTODB", "%s : %s", pszColName, pszType);
                if( EQUAL(pszType, "string") )
                {
                    OGRFieldDefn oFieldDefn(pszColName, OFTString);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
                else if( EQUAL(pszType, "number") )
                {
                    if( !EQUAL(pszColName, "cartodb_id") )
                    {
                        OGRFieldDefn oFieldDefn(pszColName, OFTReal);
                        poFeatureDefn->AddFieldDefn(&oFieldDefn);
                    }
                    else
                        osFIDColName = pszColName;
                }
                else if( EQUAL(pszType, "date") )
                {
                    if( !EQUAL(pszColName, "created_at") &&
                        !EQUAL(pszColName, "updated_at") )
                    {
                        OGRFieldDefn oFieldDefn(pszColName, OFTDateTime);
                        poFeatureDefn->AddFieldDefn(&oFieldDefn);
                    }
                }
                else if( EQUAL(pszType, "geometry") )
                {
                    if( !EQUAL(pszColName, "the_geom_webmercator") )
                    {
                        OGRGeomFieldDefn oFieldDefn(pszColName, wkbUnknown);
                        poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);
                    }
                }
                else
                {
                    CPLDebug("CARTODB", "Unhandled type: %s", pszType);
                }
            }
            else if( poType != NULL && json_object_get_type(poType) == json_type_int )
            {
                /* FIXME? manual creations of geometry columns return integer types */
                OGRGeomFieldDefn oFieldDefn(pszColName, wkbUnknown);
                poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);
            }
        }
    }
    json_object_put(poObj);

    return poFeatureDefn;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::SetAttributeFilter( const char *pszQuery )

{
    GetLayerDefn();

    if( pszQuery == NULL )
        osQuery = "";
    else
    {
        osQuery = pszQuery;
    }

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRCARTODBTableLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return;
    }
    m_iGeomFieldFilter = iGeomField;

    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                               GetSRID()                              */
/************************************************************************/

int OGRCARTODBTableLayer::GetSRID()
{
    /* FIXME: SRID */
    return 4326;
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRCARTODBTableLayer::BuildWhere()

{
    osWHERE = "";

    if( m_poFilterGeom != NULL &&
        m_iGeomFieldFilter >= 0 &&
        m_iGeomFieldFilter < poFeatureDefn->GetGeomFieldCount() )
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );

        CPLString osGeomColumn(poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter)->GetNameRef());

        char szBox3D_1[128];
        char szBox3D_2[128];
        char* pszComma;

        snprintf(szBox3D_1, sizeof(szBox3D_1), "%.12f %.12f", sEnvelope.MinX, sEnvelope.MinY);
        while((pszComma = strchr(szBox3D_1, ',')) != NULL)
            *pszComma = '.';
        snprintf(szBox3D_2, sizeof(szBox3D_2), "%.12f %.12f", sEnvelope.MaxX, sEnvelope.MaxY);
        while((pszComma = strchr(szBox3D_2, ',')) != NULL)
            *pszComma = '.';
        osWHERE.Printf("WHERE %s && 'BOX3D(%s, %s)'::box3d",
                       OGRCARTODBEscapeIdentifier(osGeomColumn).c_str(),
                       szBox3D_1, szBox3D_2 );
    }

    if( strlen(osQuery) > 0 )
    {
        if( strlen(osWHERE) == 0 )
            osWHERE = "WHERE ";
        else
            osWHERE += " AND ";
        osWHERE += osQuery;
    }

    osBaseSQL = CPLSPrintf("SELECT * FROM %s",
                           OGRCARTODBEscapeIdentifier(osName).c_str());
    if( osWHERE.size() )
    {
        osBaseSQL += " ";
        osBaseSQL += osWHERE;
    }
}

/************************************************************************/
/*                              GetFeature()                            */
/************************************************************************/

OGRFeature* OGRCARTODBTableLayer::GetFeature( long nFeatureId )
{
    GetLayerDefn();
    
    if( osFIDColName.size() == 0 )
        return OGRCARTODBLayer::GetFeature(nFeatureId);

    CPLString osSQL(CPLSPrintf("SELECT * FROM %s WHERE %s = %ld",
                               OGRCARTODBEscapeIdentifier(osName).c_str(),
                               OGRCARTODBEscapeIdentifier(osFIDColName).c_str(),
                               nFeatureId));

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRCARTODBGetSingleRow(poObj);
    if( poRowObj == NULL )
    {
        if( poObj != NULL )
            json_object_put(poObj);
        return OGRCARTODBLayer::GetFeature(nFeatureId);
    }

    OGRFeature* poFeature = BuildFeature(poRowObj);
    json_object_put(poObj);

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRCARTODBTableLayer::GetFeatureCount(int bForce)
{
    GetLayerDefn();

    CPLString osSQL(CPLSPrintf("SELECT COUNT(*) FROM %s",
                               OGRCARTODBEscapeIdentifier(osName).c_str()));
    if( osWHERE.size() )
    {
        osSQL += " ";
        osSQL += osWHERE;
    }

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRCARTODBGetSingleRow(poObj);
    if( poRowObj == NULL )
    {
        if( poObj != NULL )
            json_object_put(poObj);
        return OGRCARTODBLayer::GetFeatureCount(bForce);
    }

    json_object* poCount = json_object_object_get(poRowObj, "count");
    if( poCount == NULL || json_object_get_type(poCount) != json_type_int )
    {
        json_object_put(poObj);
        return OGRCARTODBLayer::GetFeatureCount(bForce);
    }

    int nRet = (int)json_object_get_int64(poCount);

    json_object_put(poObj);

    return nRet;
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      For PostGIS use internal Extend(geometry) function              */
/*      in other cases we use standard OGRLayer::GetExtent()            */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce )
{
    CPLString   osSQL;

    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn* poGeomFieldDefn =
        poFeatureDefn->GetGeomFieldDefn(iGeomField);

    /* Do not take the spatial filter into account */
    osSQL.Printf( "SELECT ST_Extent(%s) FROM %s",
                  OGRCARTODBEscapeIdentifier(poGeomFieldDefn->GetNameRef()).c_str(),
                  OGRCARTODBEscapeIdentifier(osName).c_str());

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRCARTODBGetSingleRow(poObj);
    if( poRowObj != NULL )
    {
        json_object* poExtent = json_object_object_get(poRowObj, "st_extent");
        if( poExtent != NULL && json_object_get_type(poExtent) == json_type_string )
        {
            const char* pszBox = json_object_get_string(poExtent);
            const char * ptr, *ptrEndParenthesis;
            char szVals[64*6+6];

            ptr = strchr(pszBox, '(');
            if (ptr)
                ptr ++;
            if (ptr == NULL ||
                (ptrEndParenthesis = strchr(ptr, ')')) == NULL ||
                ptrEndParenthesis - ptr > (int)(sizeof(szVals) - 1))
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                            "Bad extent representation: '%s'", pszBox);

                json_object_put(poObj);
                return OGRERR_FAILURE;
            }

            strncpy(szVals,ptr,ptrEndParenthesis - ptr);
            szVals[ptrEndParenthesis - ptr] = '\0';

            char ** papszTokens = CSLTokenizeString2(szVals," ,",CSLT_HONOURSTRINGS);
            int nTokenCnt = 4;

            if ( CSLCount(papszTokens) != nTokenCnt )
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                            "Bad extent representation: '%s'", pszBox);
                CSLDestroy(papszTokens);

                json_object_put(poObj);
                return OGRERR_FAILURE;
            }

            // Take X,Y coords
            // For PostGis ver >= 1.0.0 -> Tokens: X1 Y1 X2 Y2 (nTokenCnt = 4)
            // For PostGIS ver < 1.0.0 -> Tokens: X1 Y1 Z1 X2 Y2 Z2 (nTokenCnt = 6)
            // =>   X2 index calculated as nTokenCnt/2
            //      Y2 index caluclated as nTokenCnt/2+1
            
            psExtent->MinX = CPLAtof( papszTokens[0] );
            psExtent->MinY = CPLAtof( papszTokens[1] );
            psExtent->MaxX = CPLAtof( papszTokens[nTokenCnt/2] );
            psExtent->MaxY = CPLAtof( papszTokens[nTokenCnt/2+1] );

            CSLDestroy(papszTokens);

            json_object_put(poObj);
            return OGRERR_NONE;
        }
    }

    if( poObj != NULL )
        json_object_put(poObj);

    if( iGeomField == 0 )
        return OGRLayer::GetExtent( psExtent, bForce );
    else
        return OGRLayer::GetExtent( iGeomField, psExtent, bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCARTODBTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return TRUE;
    if( EQUAL(pszCap, OLCFastGetExtent) )
        return TRUE;
    if( EQUAL(pszCap, OLCRandomRead) )
    {
        GetLayerDefn();
        return osFIDColName.size() != 0;
    }
    return OGRCARTODBLayer::TestCapability(pszCap);
}
