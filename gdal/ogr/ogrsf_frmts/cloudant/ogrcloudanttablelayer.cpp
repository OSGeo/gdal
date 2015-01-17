/******************************************************************************
 * $Id$
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
#include "swq.h"

#include <algorithm>

CPL_CVSID("$Id$");

/************************************************************************/
/*                       OGRCloudantTableLayer()                         */
/************************************************************************/

OGRCloudantTableLayer::OGRCloudantTableLayer(OGRCloudantDataSource* poDS,
                                           const char* pszName) :
                                                        OGRCouchDBTableLayer((OGRCouchDBDataSource*) poDS, pszName)

{
    bHasStandardSpatial = -1;
    pszSpatialView = NULL;
    pszSpatialDDoc = NULL;
}

/************************************************************************/
/*                      ~OGRCouchDBTableLayer()                         */
/************************************************************************/

OGRCloudantTableLayer::~OGRCloudantTableLayer()

{ 
    if( bMustWriteMetadata )
    {
        WriteMetadata();
        bMustWriteMetadata = FALSE;
    }

    if (pszSpatialDDoc)
        free((void*)pszSpatialDDoc);
}

/************************************************************************/
/*                   RunSpatialFilterQueryIfNecessary()                 */
/************************************************************************/

int OGRCloudantTableLayer::RunSpatialFilterQueryIfNecessary()
{
    if (!bMustRunSpatialFilter)
        return TRUE;

    bMustRunSpatialFilter = FALSE;

    CPLAssert(nOffset == 0);

    aosIdsToFetch.resize(0);

    if (pszSpatialView == NULL)
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
    if (poAnswerObj == NULL)
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        return FALSE;
    }

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        CPLError(CE_Failure, CPLE_AppDefined,
                    "FetchNextRowsSpatialFilter() failed");
        json_object_put(poAnswerObj);
        return FALSE;
    }

    /* Catch error for a non cloudant geo database */
    json_object* poError = json_object_object_get(poAnswerObj, "error");
    json_object* poReason = json_object_object_get(poAnswerObj, "reason");

    const char* pszError = json_object_get_string(poError);
    const char* pszReason = json_object_get_string(poReason);

    if (pszError && pszReason && strcmp(pszError, "not_found") == 0 &&
        strcmp(pszReason, "Document is missing attachment") == 0)
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        json_object_put(poAnswerObj);
        return FALSE;
    }

    if (poDS->IsError(poAnswerObj, "FetchNextRowsSpatialFilter() failed"))
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        json_object_put(poAnswerObj);
        return FALSE;
    }

    json_object* poRows = json_object_object_get(poAnswerObj, "rows");
    if (poRows == NULL ||
        !json_object_is_type(poRows, json_type_array))
    {
        CPLDebug("Cloudant",
                    "Cloudant geo not working --> client-side spatial filtering");
        bServerSideSpatialFilteringWorks = FALSE;
        CPLError(CE_Failure, CPLE_AppDefined,
                    "FetchNextRowsSpatialFilter() failed");
        json_object_put(poAnswerObj);
        return FALSE;
    }

    int nRows = json_object_array_length(poRows);
    for(int i=0;i<nRows;i++)
    {
        json_object* poRow = json_object_array_get_idx(poRows, i);
        if ( poRow == NULL ||
            !json_object_is_type(poRow, json_type_object) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "FetchNextRowsSpatialFilter() failed");
            json_object_put(poAnswerObj);
            return FALSE;
        }

        json_object* poId = json_object_object_get(poRow, "id");
        const char* pszId = json_object_get_string(poId);
        if (pszId != NULL)
        {
            aosIdsToFetch.push_back(pszId);
        }
    }

    std::sort(aosIdsToFetch.begin(), aosIdsToFetch.end());

    json_object_put(poAnswerObj);

    return TRUE;
}


/************************************************************************/
/*                          GetSpatialView()                          */
/************************************************************************/

void OGRCloudantTableLayer::GetSpatialView()
{
    if (pszSpatialView == NULL)
    {
        char **papszTokens;

        if (bHasStandardSpatial < 0 || bHasStandardSpatial == FALSE)
        {
            pszSpatialView = CPLGetConfigOption("CLOUDANT_SPATIAL_FILTER" , NULL);
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
            bHasStandardSpatial = (poAnswerObj != NULL &&
                json_object_is_type(poAnswerObj, json_type_object) &&
                json_object_object_get(poAnswerObj, "st_indexes") != NULL);
            json_object_put(poAnswerObj);
        }

        if (bHasStandardSpatial)
            pszSpatialView = "_design/SpatialView/_geo/spatial";

        papszTokens = 
            CSLTokenizeString2( pszSpatialView, "/", 0);

        if ((papszTokens[0] == NULL) || (papszTokens[1] == NULL))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "GetSpatialView() failed, invalid spatial design doc.");
            return;
        }

        pszSpatialDDoc = (char*) calloc(strlen(papszTokens[0]) + strlen(papszTokens[1]) + 2, 1);

        sprintf(pszSpatialDDoc, "%s/%s", papszTokens[0], papszTokens[1]);

        CSLDestroy(papszTokens);  

    }
}

/************************************************************************/
/*                          WriteMetadata()                             */
/************************************************************************/

void OGRCloudantTableLayer::WriteMetadata()
{
    GetLayerDefn();

    if (pszSpatialDDoc == NULL)
        GetSpatialView();

    CPLString osURI;
    osURI = "/";
    osURI += osEscapedName;
    osURI += "/";
    osURI += pszSpatialDDoc;


   json_object* poDDocObj = poDS->GET(osURI);
    if (poDDocObj == NULL)
        return;

    if ( !json_object_is_type(poDDocObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WriteMetadata() failed");
        json_object_put(poDDocObj);
        return;
    }

    json_object* poError = json_object_object_get(poDDocObj, "error");
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
        const char * pszEpsg = NULL;
        const char * pszAuthName = NULL;
        char szSrid[100];

        if (poSRS->IsProjected())
        {
            pszAuthName = poSRS->GetAuthorityName("PROJCS");
            if ((pszAuthName != NULL) && (strncmp(pszAuthName, "EPSG", 4) == 0))
                pszEpsg = poSRS->GetAuthorityCode("PROJCS");
        }
        else
        {
            pszAuthName = poSRS->GetAuthorityName("GEOGCS");
            if ((pszAuthName != NULL) && (strncmp(pszAuthName, "EPSG", 4) == 0))
                pszEpsg = poSRS->GetAuthorityCode("GEOGCS");
        }

        if (pszEpsg != NULL) 
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

    for(int i=FIRST_FIELD;i<poFeatureDefn->GetFieldCount();i++)
    {
        json_object* poField = json_object_new_object();
        json_object_array_add(poFields, poField);

        json_object_object_add(poField, "name",
            json_object_new_string(poFeatureDefn->GetFieldDefn(i)->GetNameRef()));

        const char* pszType = NULL;
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
    if (bHasLoadedMetadata)
        return;

    bHasLoadedMetadata = TRUE;

    if (pszSpatialDDoc == NULL)
        GetSpatialView();

    CPLString osURI("/");
    osURI += osEscapedName;
    osURI += "/";
    osURI += pszSpatialDDoc;

    json_object* poAnswerObj = poDS->GET(osURI);
    if (poAnswerObj == NULL)
        return;

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "LoadMetadata() failed");
        json_object_put(poAnswerObj);
        return;
    }

    json_object* poRev = json_object_object_get(poAnswerObj, "_rev");
    const char* pszRev = json_object_get_string(poRev);
    if (pszRev)
        osMetadataRev = pszRev;

    json_object* poError = json_object_object_get(poAnswerObj, "error");
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

    json_object* poJsonSRS = json_object_object_get(poAnswerObj, "srsid");
    const char* pszSRS = json_object_get_string(poJsonSRS);
    if (pszSRS != NULL)
    {
        poSRS = new OGRSpatialReference();
        if (poSRS->importFromURN(pszSRS) != OGRERR_NONE)
        {
            delete poSRS;
            poSRS = NULL;
        }
    }

    json_object* poGeomType = json_object_object_get(poAnswerObj, "geomtype");
    const char* pszGeomType = json_object_get_string(poGeomType);

     if (pszGeomType)
    {
        if (EQUAL(pszGeomType, "NONE"))
        {
            eGeomType = wkbNone;
            bExtentValid = TRUE;
        }
        else
        {
            eGeomType = OGRFromOGCGeomType(pszGeomType);

            json_object* poIs25D = json_object_object_get(poAnswerObj, "is_25D");
            if (poIs25D && json_object_get_boolean(poIs25D))
                eGeomType = wkbSetZ(eGeomType);

            json_object* poExtent = json_object_object_get(poAnswerObj, "extent");
            if (poExtent && json_object_get_type(poExtent) == json_type_object)
            {
                json_object* poBbox = json_object_object_get(poExtent, "bbox");
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
                    bExtentValid = bExtentSet = TRUE;
                }
            }
        }
    }

    json_object* poGeoJSON = json_object_object_get(poAnswerObj, "geojson_documents");
    if (poGeoJSON && json_object_is_type(poGeoJSON, json_type_boolean))
        bGeoJSONDocument = json_object_get_boolean(poGeoJSON);

    json_object* poFields = json_object_object_get(poAnswerObj, "fields");
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

        int nFields = json_object_array_length(poFields);
        for(int i=0;i<nFields;i++)
        {
            json_object* poField = json_object_array_get_idx(poFields, i);
            if (poField && json_object_is_type(poField, json_type_object))
            {
                json_object* poName = json_object_object_get(poField, "name");
                const char* pszName = json_object_get_string(poName);
                if (pszName)
                {
                    json_object* poType = json_object_object_get(poField, "type");
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
