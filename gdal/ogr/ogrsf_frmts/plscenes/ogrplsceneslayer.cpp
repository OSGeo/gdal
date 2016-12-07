/******************************************************************************
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  Implements OGRPLScenesLayer
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Planet Labs
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

#include "ogr_plscenes.h"
#include "ogrgeojsonreader.h"
#include <algorithm>

CPL_CVSID("$Id$");

typedef struct
{
    const char* pszName;
    OGRFieldType eType;
} PLAttribute;

static const PLAttribute apsAttrs[] = {
    { "id",                     OFTString },
    { "acquired",               OFTDateTime },
    { "camera.bit_depth",       OFTInteger },
    { "camera.color_mode",      OFTString },
    { "camera.exposure_time",   OFTInteger },
    { "camera.gain",            OFTInteger },
    { "camera.tdi_pulses",      OFTInteger },
    { "cloud_cover.estimated",  OFTReal },
    { "data.products.analytic.full",      OFTString },
    { "data.products.visual.full",        OFTString },
    { "file_size",                        OFTInteger },
    { "image_statistics.gsd",             OFTReal },
    { "image_statistics.image_quality",   OFTString },
    { "image_statistics.snr",             OFTReal },
    { "links.full",                       OFTString },
    { "links.self",                       OFTString },
    { "links.square_thumbnail",           OFTString },
    { "links.thumbnail",                  OFTString },
    { "sat.alt",                          OFTReal },
    { "sat.id",                           OFTString },
    { "sat.lat",                          OFTReal },
    { "sat.lng",                          OFTReal },
    { "sat.off_nadir",                    OFTReal },
    { "strip_id",                         OFTReal },
    { "sun.altitude",                     OFTReal },
    { "sun.azimuth",                      OFTReal },
    { "sun.local_time_of_day",            OFTReal },
};

static bool OGRPLScenesLayerFieldNameComparator(const CPLString& osFirst,
                                                const CPLString& osSecond)
{
    const char* pszUnderscore1 = strrchr(osFirst.c_str(), '_');
    const char* pszUnderscore2 = strrchr(osSecond.c_str(), '_');
    int val1 = 0, val2 = 0;
    if( pszUnderscore1 && pszUnderscore2 &&
        (CPLString(osFirst).substr(0, pszUnderscore1-osFirst.c_str()) ==
         CPLString(osSecond).substr(0, pszUnderscore2-osSecond.c_str())) &&
        sscanf(pszUnderscore1 + 1, "%d", &val1) == 1 &&
        sscanf(pszUnderscore2 + 1, "%d", &val2) == 1 )
    {
        return val1 < val2;
    }
    return osFirst < osSecond;
}

/************************************************************************/
/*                           OGRPLScenesLayer()                         */
/************************************************************************/

OGRPLScenesLayer::OGRPLScenesLayer( OGRPLScenesDataset* poDSIn,
                                    const char* pszName,
                                    const char* pszBaseURL,
                                    json_object* poObjCount10 ) :
    poDS(poDSIn),
    osBaseURL(pszBaseURL),
    poFeatureDefn(new OGRFeatureDefn(pszName)),
    poSRS(new OGRSpatialReference(SRS_WKT_WGS84)),
    bEOF(false),
    nNextFID(1),
    nFeatureCount(-1),
    poGeoJSONDS(NULL),
    poGeoJSONLayer(NULL),
    poMainFilter(NULL),
    nPageSize(atoi(CPLGetConfigOption("PLSCENES_PAGE_SIZE", "1000"))),
    bStillInFirstPage(false),
    bAcquiredAscending(-1),
    bFilterMustBeClientSideEvaluated(false)
{
    SetDescription(pszName);
    poFeatureDefn->SetGeomType(wkbMultiPolygon);
    for( int i = 0;
         i < static_cast<int>(sizeof(apsAttrs)) /
             static_cast<int>(sizeof(apsAttrs[0]));
         i++ )
    {
        OGRFieldDefn oField(apsAttrs[i].pszName, apsAttrs[i].eType);
        poFeatureDefn->AddFieldDefn(&oField);
    }
    poFeatureDefn->Reference();
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    ResetReading();

    if( poObjCount10 != NULL )
    {
        json_object* poCount = CPL_json_object_object_get(poObjCount10, "count");
        if( poCount != NULL )
          nFeatureCount = std::max(static_cast<int64_t>(0),
                                   json_object_get_int64(poCount));

        OGRGeoJSONDataSource* poTmpDS = new OGRGeoJSONDataSource();
        OGRGeoJSONReader oReader;
        oReader.SetFlattenNestedAttributes(true, '.');
        oReader.ReadLayer( poTmpDS, "layer", poObjCount10 );
        OGRLayer* poTmpLayer = poTmpDS->GetLayer(0);
        if( poTmpLayer )
        {
            OGRFeatureDefn* poTmpFDefn = poTmpLayer->GetLayerDefn();
            std::vector<CPLString> aosNewFields;
            for(int i=0;i<poTmpFDefn->GetFieldCount();i++)
            {
                if( poFeatureDefn->GetFieldIndex(poTmpFDefn->GetFieldDefn(i)->GetNameRef()) < 0 )
                    aosNewFields.push_back(poTmpFDefn->GetFieldDefn(i)->GetNameRef());
            }
            std::sort(aosNewFields.begin(), aosNewFields.end(), OGRPLScenesLayerFieldNameComparator);
            for(int i=0;i<(int)aosNewFields.size();i++)
            {
                OGRFieldDefn oField(poTmpFDefn->GetFieldDefn(poTmpFDefn->GetFieldIndex(aosNewFields[i])));
                poFeatureDefn->AddFieldDefn(&oField);
            }
        }
        delete poTmpDS;
    }
}

/************************************************************************/
/*                           ~OGRPLScenesLayer()                        */
/************************************************************************/

OGRPLScenesLayer::~OGRPLScenesLayer()
{
    poFeatureDefn->Release();
    poSRS->Release();
    delete poGeoJSONDS;
    delete poMainFilter;
}

/************************************************************************/
/*                             BuildFilter()                            */
/************************************************************************/

CPLString OGRPLScenesLayer::BuildFilter(swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_OPERATION )
    {
        if( poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2 )
        {
            // For AND, we can deal with a failure in one of the branch
            // since client-side will do that extra filtering
            CPLString osFilter1 = BuildFilter(poNode->papoSubExpr[0]);
            CPLString osFilter2 = BuildFilter(poNode->papoSubExpr[1]);
            if( !osFilter1.empty() && !osFilter2.empty() )
                return osFilter1 + "&" + osFilter2;
            else if( !osFilter1.empty() )
                return osFilter1;
            else
                return osFilter2;
        }
        else if( (poNode->nOperation == SWQ_EQ ||
                  poNode->nOperation == SWQ_NE ||
                  poNode->nOperation == SWQ_LT ||
                  poNode->nOperation == SWQ_LE ||
                  poNode->nOperation == SWQ_GT ||
                  poNode->nOperation == SWQ_GE) && poNode->nSubExprCount == 2 &&
                  poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
                  poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
                  poNode->papoSubExpr[0]->field_index != poFeatureDefn->GetFieldIndex("id") &&
                  poNode->papoSubExpr[0]->field_index < poFeatureDefn->GetFieldCount() )
        {
            OGRFieldDefn *poFieldDefn =
                poFeatureDefn->GetFieldDefn(poNode->papoSubExpr[0]->field_index);

            int nOperation = poNode->nOperation;

            // image_quality supports only gte filters
            // (https://www.planet.com/docs-v0/v0/scenes/planetscope/#metadata)
            if( poNode->papoSubExpr[0]->field_index ==
                    poFeatureDefn->GetFieldIndex("image_statistics.image_quality") &&
                nOperation != SWQ_GE )
            {
                // == target can be safely turned as >= target
                if( poNode->nOperation == SWQ_EQ &&
                    poNode->papoSubExpr[1]->field_type == SWQ_STRING &&
                    strcmp(poNode->papoSubExpr[1]->string_value, "target") == 0 )
                {
                    nOperation = SWQ_GE;
                }
                else
                {
                    if( !bFilterMustBeClientSideEvaluated )
                    {
                        bFilterMustBeClientSideEvaluated = true;
                        CPLDebug("PLSCENES",
                                 "Part or full filter will have to be "
                                 "evaluated on client side.");
                    }
                    return "";
                }
            }

            CPLString osFilter(poFieldDefn->GetNameRef());

            bool bDateTimeParsed = false;
            int nYear = 0;
            int nMonth = 0;
            int nDay = 0;
            int nHour = 0;
            int nMinute = 0;
            int nSecond = 0;
            if( poNode->papoSubExpr[1]->field_type == SWQ_TIMESTAMP )
            {
                if( sscanf(poNode->papoSubExpr[1]->string_value,"%04d/%02d/%02d %02d:%02d:%02d",
                           &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond) >= 3 ||
                    sscanf(poNode->papoSubExpr[1]->string_value,"%04d-%02d-%02dT%02d:%02d:%02d",
                           &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond) >= 3 )
                    bDateTimeParsed = true;
            }

            osFilter += ".";

            if( nOperation == SWQ_EQ )
            {
                if( bDateTimeParsed )
                    osFilter += "gte";
                else
                    osFilter += "eq";
            }
            else if( nOperation == SWQ_NE )
                osFilter += "neq";
            else if( nOperation == SWQ_LT )
                osFilter += "lt";
            else if( nOperation == SWQ_LE )
                osFilter += "lte";
            else if( nOperation == SWQ_GT )
                osFilter += "gt";
            else if( nOperation == SWQ_GE )
                osFilter += "gte";
            osFilter += "=";

            if (poNode->papoSubExpr[1]->field_type == SWQ_FLOAT)
                osFilter += CPLSPrintf("%.8f", poNode->papoSubExpr[1]->float_value);
            else if (poNode->papoSubExpr[1]->field_type == SWQ_INTEGER)
                osFilter += CPLSPrintf(CPL_FRMT_GIB, poNode->papoSubExpr[1]->int_value);
            else if (poNode->papoSubExpr[1]->field_type == SWQ_STRING)
                osFilter += poNode->papoSubExpr[1]->string_value;
            else if (poNode->papoSubExpr[1]->field_type == SWQ_TIMESTAMP)
            {
                if( bDateTimeParsed )
                {
                    osFilter += CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02d",
                                           nYear, nMonth, nDay, nHour, nMinute, nSecond);
                    if( nOperation == SWQ_EQ )
                    {
                        osFilter += "&";
                        osFilter += poFieldDefn->GetNameRef();
                        osFilter += ".lt=";
                        nSecond ++;
                        if( nSecond == 60 ) { nSecond = 0; nMinute ++; }
                        if( nMinute == 60 ) { nMinute = 0; nHour ++; }
                        if( nHour == 24 ) { nHour = 0; nDay ++; }
                        osFilter += CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02d",
                                               nYear, nMonth, nDay, nHour, nMinute, nSecond);
                    }
                }
                else
                    osFilter += poNode->papoSubExpr[1]->string_value;
            }
            return osFilter;
        }
    }
    if( !bFilterMustBeClientSideEvaluated )
    {
        bFilterMustBeClientSideEvaluated = true;
        CPLDebug("PLSCENES",
                 "Part or full filter will have to be evaluated on client side.");
    }
    return "";
}

/************************************************************************/
/*                             ResetReading()                           */
/************************************************************************/

void OGRPLScenesLayer::ResetReading()
{
    bEOF = false;
    if( poGeoJSONLayer && bStillInFirstPage )
        poGeoJSONLayer->ResetReading();
    else
        poGeoJSONLayer = NULL;
    nNextFID = 1;
    bStillInFirstPage = true;
    osRequestURL = BuildURL(nPageSize);
}

/************************************************************************/
/*                             BuildURL()                               */
/************************************************************************/

CPLString OGRPLScenesLayer::BuildURL(int nFeatures)
{
    CPLString osURL = osBaseURL + CPLSPrintf("?count=%d", nFeatures);

    if( bAcquiredAscending == 1 )
        osURL += "&order_by=acquired%20asc";
    else if( bAcquiredAscending == 0 )
        osURL += "&order_by=acquired%20desc";

    if( m_poFilterGeom != NULL || poMainFilter != NULL )
    {
        OGRGeometry* poIntersection = NULL;
        OGRGeometry* poFilterGeom = m_poFilterGeom;
        if( poFilterGeom )
        {
            OGREnvelope sEnvelope;
            poFilterGeom->getEnvelope(&sEnvelope);
            if( sEnvelope.MinX <= -180 && sEnvelope.MinY <= -90 &&
                sEnvelope.MaxX >= 180 && sEnvelope.MaxY >= 90 )
                poFilterGeom = NULL;
        }

        if( poFilterGeom && poMainFilter )
            poIntersection = poFilterGeom->Intersection(poMainFilter);
        else if( poFilterGeom )
            poIntersection = poFilterGeom;
        else if( poMainFilter )
            poIntersection = poMainFilter;
        if( poIntersection )
        {
            char* pszWKT = NULL;
            OGREnvelope sEnvelope;
            poIntersection->getEnvelope(&sEnvelope);
            if( sEnvelope.MinX == sEnvelope.MaxX && sEnvelope.MinY == sEnvelope.MaxY )
            {
                pszWKT = CPLStrdup(CPLSPrintf("POINT(%.18g %.18g)",
                                            sEnvelope.MinX, sEnvelope.MinY));
            }
            else
                poIntersection->exportToWkt(&pszWKT);

            osURL += "&intersects=";
            char* pszWKTEscaped = CPLEscapeString(pszWKT, -1, CPLES_URL);
            osURL += pszWKTEscaped;
            CPLFree(pszWKTEscaped);
            CPLFree(pszWKT);
        }
        if( poIntersection != m_poFilterGeom && poIntersection != poMainFilter  )
            delete poIntersection;
    }

    if( !osFilterURLPart.empty() )
    {
        if( osFilterURLPart[0] == '&' )
            osURL += osFilterURLPart;
        else
            osURL = osBaseURL + osFilterURLPart;
    }

    return osURL;
}

/************************************************************************/
/*                              GetNextPage()                           */
/************************************************************************/

int OGRPLScenesLayer::GetNextPage()
{
    delete poGeoJSONDS;
    poGeoJSONLayer = NULL;
    poGeoJSONDS = NULL;

    if( osRequestURL.empty() )
    {
        bEOF = true;
        if( !bFilterMustBeClientSideEvaluated && nFeatureCount < 0 )
            nFeatureCount = 0;
        return FALSE;
    }
    // In the case of a "id = 'foo'" filter, a non existing resource
    // will cause a 404 error, which we want to be silent
    int bQuiet404Error = ( osRequestURL.find('?') == std::string::npos );
    json_object* poObj = poDS->RunRequest(osRequestURL, bQuiet404Error);
    if( poObj == NULL )
    {
        bEOF = true;
        if( !bFilterMustBeClientSideEvaluated && nFeatureCount < 0 )
            nFeatureCount = 0;
        return FALSE;
    }

    if( !bFilterMustBeClientSideEvaluated && nFeatureCount < 0 )
    {
        json_object* poType = CPL_json_object_object_get(poObj, "type");
        if( poType && json_object_get_type(poType) == json_type_string &&
            strcmp(json_object_get_string(poType), "Feature") == 0 )
        {
            nFeatureCount = 1;
        }
        else
        {
            json_object* poCount = CPL_json_object_object_get(poObj, "count");
            if( poCount == NULL )
            {
                json_object_put(poObj);
                bEOF = true;
                nFeatureCount = 0;
                return FALSE;
            }
            nFeatureCount = std::max(static_cast<int64_t>(0),
                                     json_object_get_int64(poCount));
        }
    }

    // Parse the Feature/FeatureCollection with the GeoJSON reader
    poGeoJSONDS = new OGRGeoJSONDataSource();
    OGRGeoJSONReader oReader;
    oReader.SetFlattenNestedAttributes(true, '.');
    oReader.ReadLayer( poGeoJSONDS, "layer", poObj);
    poGeoJSONLayer = poGeoJSONDS->GetLayer(0);

    // Get URL of next page
    osNextURL = "";
    if( poGeoJSONLayer )
    {
        json_object* poLinks = CPL_json_object_object_get(poObj, "links");
        if( poLinks && json_object_get_type(poLinks) == json_type_object )
        {
            json_object* poNext = CPL_json_object_object_get(poLinks, "next");
            if( poNext && json_object_get_type(poNext) == json_type_string )
            {
                osNextURL = json_object_get_string(poNext);
            }
        }
    }

    json_object_put(poObj);
    return poGeoJSONLayer != NULL;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRPLScenesLayer::SetSpatialFilter(  OGRGeometry *poGeomIn )
{
    nFeatureCount = -1;
    poGeoJSONLayer = NULL;

    if( poGeomIn )
    {
        OGREnvelope sEnvelope;
        poGeomIn->getEnvelope(&sEnvelope);
        if( sEnvelope.MinX == sEnvelope.MaxX && sEnvelope.MinY == sEnvelope.MaxY )
        {
            OGRPoint p(sEnvelope.MinX, sEnvelope.MinY);
            InstallFilter(&p);
        }
        else
            InstallFilter( poGeomIn );
    }
    else
        InstallFilter( poGeomIn );

    ResetReading();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRPLScenesLayer::SetAttributeFilter( const char *pszQuery )

{
    if( pszQuery == NULL )
        osQuery = "";
    else
        osQuery = pszQuery;

    nFeatureCount = -1;
    poGeoJSONLayer = NULL;

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszQuery);

    osFilterURLPart = "";
    bFilterMustBeClientSideEvaluated = false;
    if( m_poAttrQuery != NULL )
    {
        swq_expr_node* poNode = (swq_expr_node*) m_poAttrQuery->GetSWQExpr();

        poNode->ReplaceBetweenByGEAndLERecurse();

        if( poNode->eNodeType == SNT_OPERATION &&
            poNode->nOperation == SWQ_EQ && poNode->nSubExprCount == 2 &&
            poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
            poNode->papoSubExpr[0]->field_index == poFeatureDefn->GetFieldIndex("id") &&
            poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
            poNode->papoSubExpr[1]->field_type == SWQ_STRING )
        {
            osFilterURLPart = poNode->papoSubExpr[1]->string_value;
        }
        else
        {
            CPLString osFilter = BuildFilter(poNode);
            if( !osFilter.empty() )
            {
                osFilterURLPart = "&";
                osFilterURLPart += osFilter;
            }
        }
    }

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPLScenesLayer::GetNextFeature()
{
    if( !bFilterMustBeClientSideEvaluated )
        return GetNextRawFeature();

    while( true )
    {
        OGRFeature *poFeature = NULL;
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
/*                            GetNextRawFeature()                       */
/************************************************************************/

OGRFeature* OGRPLScenesLayer::GetNextRawFeature()
{
    if( bEOF ||
        (!bFilterMustBeClientSideEvaluated &&
         nFeatureCount >= 0 &&
         nNextFID > nFeatureCount) )
        return NULL;

    if( poGeoJSONLayer == NULL )
    {
        if( !GetNextPage() )
            return NULL;
    }

#ifdef notdef
    if( CPLTestBool(CPLGetConfigOption("OGR_LIMIT_TOO_MANY_FEATURES", "FALSE")) &&
        nFeatureCount > nPageSize )
    {
        bEOF = true;
        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
        const char* pszWKT = "MULTIPOLYGON(((-180 90,180 90,180 -90,-180 -90,-180 90)))";
        OGRGeometry* poGeom = NULL;
        OGRGeometryFactory::createFromWkt((char**)&pszWKT, poSRS, &poGeom);
        poFeature->SetGeometryDirectly(poGeom);
        return poFeature;
    }
#endif

    OGRFeature* poGeoJSONFeature = poGeoJSONLayer->GetNextFeature();
    if( poGeoJSONFeature == NULL )
    {
        osRequestURL = osNextURL;
        bStillInFirstPage = false;
        if( !GetNextPage() )
            return NULL;
        poGeoJSONFeature = poGeoJSONLayer->GetNextFeature();
        if( poGeoJSONFeature == NULL )
        {
            bEOF = true;
            return NULL;
        }
    }
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetFID(nNextFID++);

    OGRGeometry* poGeom = poGeoJSONFeature->StealGeometry();
    if( poGeom != NULL )
    {
        if( poGeom->getGeometryType() == wkbPolygon )
        {
            OGRMultiPolygon* poMP = new OGRMultiPolygon();
            poMP->addGeometryDirectly(poGeom);
            poGeom = poMP;
        }
        poGeom->assignSpatialReference(poSRS);
        poFeature->SetGeometryDirectly(poGeom);
    }

    for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        OGRFieldType eType = poFieldDefn->GetType();
        int iSrcField = poGeoJSONFeature->GetFieldIndex(poFieldDefn->GetNameRef());
        if( iSrcField >= 0 && poGeoJSONFeature->IsFieldSet(iSrcField) )
        {
            if( eType == OFTInteger )
                poFeature->SetField(i,
                    poGeoJSONFeature->GetFieldAsInteger(iSrcField));
            else if( eType == OFTReal )
                poFeature->SetField(i,
                    poGeoJSONFeature->GetFieldAsDouble(iSrcField));
            else
                poFeature->SetField(i,
                    poGeoJSONFeature->GetFieldAsString(iSrcField));
        }
    }

    delete poGeoJSONFeature;

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRPLScenesLayer::GetFeatureCount(int bForce)
{
    if( nFeatureCount < 0 )
    {
        CPLString osURL(BuildURL(1));
        if( bFilterMustBeClientSideEvaluated )
        {
            nFeatureCount = OGRLayer::GetFeatureCount(bForce);
        }
        else if( osURL.find('?') == std::string::npos )
        {
            /* Case of a "id = XXXXX" filter: we get directly a Feature, */
            /* not a FeatureCollection */
            GetNextPage();
        }
        else
        {
            nFeatureCount = 0;
            json_object* poObj = poDS->RunRequest(osURL);
            if( poObj != NULL )
            {
                json_object* poCount = CPL_json_object_object_get(poObj, "count");
                if( poCount != NULL )
                    nFeatureCount = std::max(static_cast<int64_t>(0),
                                             json_object_get_int64(poCount));

                // Small optimization, if the feature count is actually 1
                // then we can fetch it as the full layer
                if( nFeatureCount == 1 )
                {
                    delete poGeoJSONDS;
                    // Parse the Feature/FeatureCollection with the GeoJSON reader
                    poGeoJSONDS = new OGRGeoJSONDataSource();
                    OGRGeoJSONReader oReader;
                    oReader.SetFlattenNestedAttributes(true, '.');
                    oReader.ReadLayer( poGeoJSONDS, "layer", poObj);
                    poGeoJSONLayer = poGeoJSONDS->GetLayer(0);
                    osNextURL = "";
                }
                json_object_put(poObj);
            }
        }
    }

    return nFeatureCount;
}

/************************************************************************/
/*                                GetExtent()                           */
/************************************************************************/

OGRErr OGRPLScenesLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    GetFeatureCount();
    if( nFeatureCount > 0 && nFeatureCount < nPageSize )
        return OGRLayer::GetExtentInternal(0, psExtent, bForce);

    psExtent->MinX = -180;
    psExtent->MinY = -90;
    psExtent->MaxX = 180;
    psExtent->MaxY = 90;
    return OGRERR_NONE;
}

/************************************************************************/
/*                              SetMainFilterRect()                     */
/************************************************************************/

void OGRPLScenesLayer::SetMainFilterRect( double dfMinX, double dfMinY,
                                          double dfMaxX, double dfMaxY )
{
    delete poMainFilter;
    if( dfMinX == dfMaxX && dfMinY == dfMaxY )
        poMainFilter = new OGRPoint(dfMinX, dfMinY);
    else
    {
        OGRPolygon* poPolygon = new OGRPolygon();
        poMainFilter = poPolygon;
        OGRLinearRing* poLR = new OGRLinearRing;
        poPolygon->addRingDirectly(poLR);
        poLR->addPoint(dfMinX, dfMinY);
        poLR->addPoint(dfMinX, dfMaxY);
        poLR->addPoint(dfMaxX, dfMaxY);
        poLR->addPoint(dfMaxX, dfMinY);
        poLR->addPoint(dfMinX, dfMinY);
    }
    ResetReading();
}

/************************************************************************/
/*                              TestCapability()                        */
/************************************************************************/

int OGRPLScenesLayer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return !bFilterMustBeClientSideEvaluated;
    if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    return FALSE;
}
