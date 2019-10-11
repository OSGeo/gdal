/******************************************************************************
 *
 * Project:  Cloudant Translator
 * Purpose:  Definition of classes for OGR Cloudant driver.
 * Author:   Norman Barker, norman at cloudant com
 *           Based on CouchDB driver
 *
 ******************************************************************************
 * Copyright (c) 2014, Norman Barker <norman at cloudant com>
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

#include "ogr_cloudant.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonwriter.h"
#include "ogr_swq.h"

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/*                       OGRCloudantTableLayer()                         */
/************************************************************************/

OGRCloudantTableLayer::OGRCloudantTableLayer( OGRCloudantDataSource* poDSIn,
                                              const char* pszName) :
    OGRCouchDBTableLayer((OGRCouchDBDataSource*) poDSIn, pszName),
    bHasStandardSpatial(-1),
    pszSpatialView(nullptr),
    pszSpatialDDoc(nullptr)
{}

/************************************************************************/
/*                      ~OGRCouchDBTableLayer()                         */
/************************************************************************/

OGRCloudantTableLayer::~OGRCloudantTableLayer()

{
    if( bMustWriteMetadata )
    {
        OGRCloudantTableLayer::GetLayerDefn();
        OGRCloudantTableLayer::WriteMetadata();
        bMustWriteMetadata = false;
    }

    if (pszSpatialDDoc)
        CPLFree(pszSpatialDDoc);
}

/************************************************************************/
/*                   RunSpatialFilterQueryIfNecessary()                 */
/************************************************************************/

bool OGRCloudantTableLayer::RunSpatialFilterQueryIfNecessary()
{
    if( !bMustRunSpatialFilter )
        return true;

    bMustRunSpatialFilter = false;

    CPLAssert(nOffset == 0);

    aosIdsToFetch.resize(0);

    if (pszSpatialView == nullptr)
        GetSpatialView();

    OGREnvelope sEnvelope;
    m_poFilterGeom->getEnvelope( &sEnvelope );

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/";
    osURI += pszSpatialView;
    osURI += "?bbox=";
    osURI += CPLSPrintf("%.9f,%.9f,%.9f,%.9f",
                        sEnvelope.MinX, sEnvelope.MinY,
                        sEnvelope.MaxX, sEnvelope.MaxY);

    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj == nullptr)
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = false;
        return false;
    }

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = false;
        CPLError(CE_Failure, CPLE_AppDefined,
                    "FetchNextRowsSpatialFilter() failed");
        json_object_put(poAnswerObj);
        return false;
    }

    /* Catch error for a non cloudant geo database */
    json_object* poError = CPL_json_object_object_get(poAnswerObj, "error");
    json_object* poReason = CPL_json_object_object_get(poAnswerObj, "reason");

    const char* pszError = json_object_get_string(poError);
    const char* pszReason = json_object_get_string(poReason);

    if (pszError && pszReason && strcmp(pszError, "not_found") == 0 &&
        strcmp(pszReason, "Document is missing attachment") == 0)
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = false;
        json_object_put(poAnswerObj);
        return false;
    }

    if (poDS->IsError(poAnswerObj, "FetchNextRowsSpatialFilter() failed"))
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = false;
        json_object_put(poAnswerObj);
        return false;
    }

    json_object* poRows = CPL_json_object_object_get(poAnswerObj, "rows");
    if (poRows == nullptr ||
        !json_object_is_type(poRows, json_type_array))
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = false;
        CPLError(CE_Failure, CPLE_AppDefined,
                    "FetchNextRowsSpatialFilter() failed");
        json_object_put(poAnswerObj);
        return false;
    }

    auto nRows = json_object_array_length(poRows);
    for(auto i=decltype(nRows){0};i<nRows;i++)
    {
        json_object* poRow = json_object_array_get_idx(poRows, i);
        if ( poRow == nullptr ||
            !json_object_is_type(poRow, json_type_object) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "FetchNextRowsSpatialFilter() failed");
            json_object_put(poAnswerObj);
            return false;
        }

        json_object* poId = CPL_json_object_object_get(poRow, "id");
        const char* pszId = json_object_get_string(poId);
        if (pszId != nullptr)
        {
            aosIdsToFetch.push_back(pszId);
        }
    }

    std::sort(aosIdsToFetch.begin(), aosIdsToFetch.end());

    json_object_put(poAnswerObj);

    return true;
}

/************************************************************************/
/*                          GetSpatialView()                          */
/************************************************************************/

void OGRCloudantTableLayer::GetSpatialView()
{
    if (pszSpatialView == nullptr)
    {
        if (bHasStandardSpatial < 0 || bHasStandardSpatial == FALSE)
        {
            pszSpatialView = CPLGetConfigOption("CLOUDANT_SPATIAL_FILTER" , nullptr);
            if (pszSpatialView)
                bHasStandardSpatial = FALSE;
        }

        if (bHasStandardSpatial < 0)
        {
            // get standard cloudant geo spatial view
            CPLString osURI("/");
            osURI += osEscapedName;
            osURI += "/_design/SpatialView";

            json_object* poAnswerObj = poDS->GET(osURI);
            bHasStandardSpatial = (poAnswerObj != nullptr &&
                json_object_is_type(poAnswerObj, json_type_object) &&
                CPL_json_object_object_get(poAnswerObj, "st_indexes") != nullptr);
            json_object_put(poAnswerObj);
        }

        if (bHasStandardSpatial)
            pszSpatialView = "_design/SpatialView/_geo/spatial";

        char **papszTokens =
            CSLTokenizeString2( pszSpatialView, "/", 0);

        if ((papszTokens[0] == nullptr) || (papszTokens[1] == nullptr))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "GetSpatialView() failed, invalid spatial design doc.");
            CSLDestroy(papszTokens);
            return;
        }

        const size_t nLen = strlen(papszTokens[0]) + strlen(papszTokens[1]) + 2;
        pszSpatialDDoc = static_cast<char *>(CPLCalloc(nLen, 1));

        snprintf(pszSpatialDDoc, nLen, "%s/%s", papszTokens[0], papszTokens[1]);

        CSLDestroy(papszTokens);
    }
}

/************************************************************************/
/*                          WriteMetadata()                             */
/************************************************************************/

void OGRCloudantTableLayer::WriteMetadata()
{
    if (pszSpatialDDoc == nullptr)
        OGRCloudantTableLayer::GetSpatialView();
    if( pszSpatialDDoc == nullptr )
        return;

    CPLString osURI;
    osURI = "/";
    osURI += osEscapedName;
    osURI += "/";
    osURI += pszSpatialDDoc;

   json_object* poDDocObj = poDS->GET(osURI);
    if (poDDocObj == nullptr)
        return;

    if ( !json_object_is_type(poDDocObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WriteMetadata() failed");
        json_object_put(poDDocObj);
        return;
    }

    json_object* poError = CPL_json_object_object_get(poDDocObj, "error");
    const char* pszError = json_object_get_string(poError);
    if (pszError && strcmp(pszError, "not_found") == 0)
    {
        json_object_put(poDDocObj);
        return;
    }

    if (poDS->IsError(poDDocObj, "WriteMetadata() failed"))
    {
        json_object_put(poDDocObj);
        return;
    }

    if (poSRS)
    {
        // epsg codes are supported in Cloudant
        const char * pszEpsg = nullptr;
        const char * pszAuthName = nullptr;
        char szSrid[100];

        if (poSRS->IsProjected())
        {
            pszAuthName = poSRS->GetAuthorityName("PROJCS");
            if ((pszAuthName != nullptr) && (STARTS_WITH(pszAuthName, "EPSG")))
                pszEpsg = poSRS->GetAuthorityCode("PROJCS");
        }
        else
        {
            pszAuthName = poSRS->GetAuthorityName("GEOGCS");
            if ((pszAuthName != nullptr) && (STARTS_WITH(pszAuthName, "EPSG")))
                pszEpsg = poSRS->GetAuthorityCode("GEOGCS");
        }

        if (pszEpsg != nullptr)
        {
            const char * pszUrn = "urn:ogc:def:crs:epsg::";
            CPLStrlcpy(szSrid, pszUrn, sizeof(szSrid));
            if (CPLStrlcpy(szSrid + sizeof(pszUrn), pszEpsg, sizeof(szSrid)) <= sizeof(szSrid))
            {
                json_object_object_add(poDDocObj, "srsid",
                                   json_object_new_string(pszUrn));
            }
        }
    }

    if (eGeomType != wkbNone)
    {
        json_object_object_add(poDDocObj, "geomtype",
                    json_object_new_string(OGRToOGCGeomType(eGeomType)));
        if (wkbHasZ(poFeatureDefn->GetGeomType()))
        {
            json_object_object_add(poDDocObj, "is_25D",
                               json_object_new_boolean(TRUE));
        }
    }
    else
    {
        json_object_object_add(poDDocObj, "geomtype",
                               json_object_new_string("NONE"));
    }

    json_object_object_add(poDDocObj, "geojson_documents",
                           json_object_new_boolean(bGeoJSONDocument));

    json_object* poFields = json_object_new_array();
    json_object_object_add(poDDocObj, "fields", poFields);

    for(int i=COUCHDB_FIRST_FIELD;i<poFeatureDefn->GetFieldCount();i++)
    {
        json_object* poField = json_object_new_object();
        json_object_array_add(poFields, poField);

        json_object_object_add(poField, "name",
            json_object_new_string(poFeatureDefn->GetFieldDefn(i)->GetNameRef()));

        const char* pszType = nullptr;
        switch (poFeatureDefn->GetFieldDefn(i)->GetType())
        {
            case OFTInteger: pszType = "integer"; break;
            case OFTReal: pszType = "real"; break;
            case OFTString: pszType = "string"; break;
            case OFTIntegerList: pszType = "integerlist"; break;
            case OFTRealList: pszType = "reallist"; break;
            case OFTStringList: pszType = "stringlist"; break;
            default: pszType = "string"; break;
        }

        json_object_object_add(poField, "type",
                               json_object_new_string(pszType));
    }

    json_object* poAnswerObj = poDS->PUT(osURI,
                                         json_object_to_json_string(poDDocObj));

    json_object_put(poDDocObj);
    json_object_put(poAnswerObj);
}

/************************************************************************/
/*                     OGRCloudantIsNumericObject()                      */
/************************************************************************/

static int OGRCloudantIsNumericObject(json_object* poObj)
{
    int iType = json_object_get_type(poObj);
    return iType == json_type_int || iType == json_type_double;
}

/************************************************************************/
/*                          LoadMetadata()                              */
/************************************************************************/

void OGRCloudantTableLayer::LoadMetadata()
{
    if( bHasLoadedMetadata )
        return;

    bHasLoadedMetadata = true;

    if (pszSpatialDDoc == nullptr)
        GetSpatialView();
    if( pszSpatialDDoc == nullptr )
        return;

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/";
    osURI += pszSpatialDDoc;

    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj == nullptr)
        return;

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "LoadMetadata() failed");
        json_object_put(poAnswerObj);
        return;
    }

    json_object* poRev = CPL_json_object_object_get(poAnswerObj, "_rev");
    const char* pszRev = json_object_get_string(poRev);
    if (pszRev)
        osMetadataRev = pszRev;

    json_object* poError = CPL_json_object_object_get(poAnswerObj, "error");
    const char* pszError = json_object_get_string(poError);
    if (pszError && strcmp(pszError, "not_found") == 0)
    {
        json_object_put(poAnswerObj);
        return;
    }

    if (poDS->IsError(poAnswerObj, "LoadMetadata() failed"))
    {
        json_object_put(poAnswerObj);
        return;
    }

    json_object* poJsonSRS = CPL_json_object_object_get(poAnswerObj, "srsid");
    const char* pszSRS = json_object_get_string(poJsonSRS);
    if (pszSRS != nullptr)
    {
        poSRS = new OGRSpatialReference();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSRS->importFromURN(pszSRS) != OGRERR_NONE)
        {
            delete poSRS;
            poSRS = nullptr;
        }
    }

    json_object* poGeomType = CPL_json_object_object_get(poAnswerObj, "geomtype");
    const char* pszGeomType = json_object_get_string(poGeomType);

     if (pszGeomType)
    {
        if (EQUAL(pszGeomType, "NONE"))
        {
            eGeomType = wkbNone;
            bExtentValid = true;
        }
        else
        {
            eGeomType = OGRFromOGCGeomType(pszGeomType);

            json_object* poIs25D = CPL_json_object_object_get(poAnswerObj, "is_25D");
            if (poIs25D && json_object_get_boolean(poIs25D))
                eGeomType = wkbSetZ(eGeomType);

            json_object* poExtent = CPL_json_object_object_get(poAnswerObj, "extent");
            if (poExtent && json_object_get_type(poExtent) == json_type_object)
            {
                json_object* poBbox = CPL_json_object_object_get(poExtent, "bbox");
                if (poBbox &&
                    json_object_get_type(poBbox) == json_type_array &&
                    json_object_array_length(poBbox) == 4 &&
                    OGRCloudantIsNumericObject(json_object_array_get_idx(poBbox, 0)) &&
                    OGRCloudantIsNumericObject(json_object_array_get_idx(poBbox, 1)) &&
                    OGRCloudantIsNumericObject(json_object_array_get_idx(poBbox, 2)) &&
                    OGRCloudantIsNumericObject(json_object_array_get_idx(poBbox, 3)))
                {
                    dfMinX = json_object_get_double(json_object_array_get_idx(poBbox, 0));
                    dfMinY = json_object_get_double(json_object_array_get_idx(poBbox, 1));
                    dfMaxX = json_object_get_double(json_object_array_get_idx(poBbox, 2));
                    dfMaxY = json_object_get_double(json_object_array_get_idx(poBbox, 3));
                    bExtentValid = true;
                    bExtentSet = true;
                }
            }
        }
    }

    json_object* poGeoJSON = CPL_json_object_object_get(poAnswerObj, "geojson_documents");
    if (poGeoJSON && json_object_is_type(poGeoJSON, json_type_boolean))
        bGeoJSONDocument = CPL_TO_BOOL(json_object_get_boolean(poGeoJSON));

    json_object* poFields = CPL_json_object_object_get(poAnswerObj, "fields");
    if (poFields && json_object_is_type(poFields, json_type_array))
    {
        poFeatureDefn = new OGRFeatureDefn( osName );
        poFeatureDefn->Reference();

        poFeatureDefn->SetGeomType(eGeomType);
        if( poFeatureDefn->GetGeomFieldCount() != 0 )
            poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

        OGRFieldDefn oFieldId("_id", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldId);

        OGRFieldDefn oFieldRev("_rev", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldRev);

        auto nFields = json_object_array_length(poFields);
        for(auto i=decltype(nFields){0};i<nFields;i++)
        {
            json_object* poField = json_object_array_get_idx(poFields, i);
            if (poField && json_object_is_type(poField, json_type_object))
            {
                json_object* poName = CPL_json_object_object_get(poField, "name");
                const char* pszName = json_object_get_string(poName);
                if (pszName)
                {
                    json_object* poType = CPL_json_object_object_get(poField, "type");
                    const char* pszType = json_object_get_string(poType);
                    OGRFieldType eType = OFTString;
                    if (pszType)
                    {
                        if (strcmp(pszType, "integer") == 0)
                            eType = OFTInteger;
                        else if (strcmp(pszType, "integerlist") == 0)
                            eType = OFTIntegerList;
                        else if (strcmp(pszType, "real") == 0)
                            eType = OFTReal;
                        else if (strcmp(pszType, "reallist") == 0)
                            eType = OFTRealList;
                        else if (strcmp(pszType, "string") == 0)
                            eType = OFTString;
                        else if (strcmp(pszType, "stringlist") == 0)
                            eType = OFTStringList;
                    }

                    OGRFieldDefn oField(pszName, eType);
                    poFeatureDefn->AddFieldDefn(&oField);
                }
            }
        }
    }

    std::sort(aosIdsToFetch.begin(), aosIdsToFetch.end());

    json_object_put(poAnswerObj);

    return;
}
