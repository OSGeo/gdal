/******************************************************************************
 *
 * Project:  Cloudant Translator
 * Purpose:  Definition of classes for OGR Cloudant driver.
 * Author:   Norman Barker, norman at cloudant com
 *           Based on the CouchDB driver
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRCloudantDataSource()                       */
/************************************************************************/

OGRCloudantDataSource::OGRCloudantDataSource() {}

/************************************************************************/
/*                       ~OGRCloudantDataSource()                       */
/************************************************************************/

OGRCloudantDataSource::~OGRCloudantDataSource() {}

/************************************************************************/
/*                             OpenDatabase()                           */
/************************************************************************/

OGRLayer* OGRCloudantDataSource::OpenDatabase(const char* pszLayerName)
{
    CPLString osTableName;
    CPLString osEscapedName;

    if (pszLayerName)
    {
        osTableName = pszLayerName;
        char* pszEscapedName = CPLEscapeString(pszLayerName, -1, CPLES_URL);
        osEscapedName = pszEscapedName;
        CPLFree(pszEscapedName);
    }
    else
    {
        char* pszURL = CPLStrdup(osURL);
        char* pszLastSlash = strrchr(pszURL, '/');
        if (pszLastSlash)
        {
            osEscapedName = pszLastSlash + 1;
            char* l_pszName = CPLUnescapeString(osEscapedName, NULL, CPLES_URL);
            osTableName = l_pszName;
            CPLFree(l_pszName);
            *pszLastSlash = 0;
        }
        osURL = pszURL;
        CPLFree(pszURL);
        pszURL = NULL;

        if (pszLastSlash == NULL)
            return NULL;
    }

    CPLString osURI("/");
    osURI += osEscapedName;

    json_object* poAnswerObj = GET(osURI);
    if (poAnswerObj == NULL)
        return NULL;

    if ( !json_object_is_type(poAnswerObj, json_type_object) ||
            CPL_json_object_object_get(poAnswerObj, "db_name") == NULL )
    {
        IsError(poAnswerObj, "Database opening failed");

        json_object_put(poAnswerObj);
        return NULL;
    }

    OGRCloudantTableLayer* poLayer = new OGRCloudantTableLayer(this, osTableName);

    if ( CPL_json_object_object_get(poAnswerObj, "update_seq") != NULL )
    {
        int nUpdateSeq = json_object_get_int(CPL_json_object_object_get(poAnswerObj, "update_seq"));
        poLayer->SetUpdateSeq(nUpdateSeq);
    }

    json_object_put(poAnswerObj);

    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers ++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCloudantDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    const bool bHTTP =
        STARTS_WITH(pszFilename, "http://") ||
        STARTS_WITH(pszFilename, "https://");
    if( !bHTTP && !STARTS_WITH_CI(pszFilename, "cloudant:") )
        return FALSE;

    bReadWrite = CPL_TO_BOOL(bUpdateIn);

    pszName = CPLStrdup( pszFilename );

    if( bHTTP )
        osURL = pszFilename;
    else
        osURL = pszFilename + 9;
    if (!osURL.empty() && osURL.back() == '/')
        osURL.resize(osURL.size() - 1);

    const char* pszUserPwd = CPLGetConfigOption("CLOUDANT_USERPWD", NULL);
    const char* pszSlash = "/";

    if (pszUserPwd)
        osUserPwd = pszUserPwd;

    if ((strstr(osURL, "/_design/") && strstr(osURL, "/_view/")) ||
        strstr(osURL, "/_all_docs"))
    {
        return OpenView() != NULL;
    }

    /* If passed with https://useraccount.cloudant.com[:port]/database, do not */
    /* try to issue /all_dbs, but directly open the database */
    const char* pszKnowProvider = strstr(osURL, ".cloudant.com/");
    if (pszKnowProvider != NULL &&
        strchr(pszKnowProvider + strlen(".cloudant.com/"), '/' ) == NULL)
    {
        return OpenDatabase() != NULL;
    }

    pszKnowProvider = strstr(osURL, "localhost");
    if (pszKnowProvider != NULL &&
        strstr(pszKnowProvider + strlen("localhost"), pszSlash ) != NULL)
    {
        return OpenDatabase() != NULL;
    }

    /* Get list of tables */
    json_object* poAnswerObj = GET("/_all_dbs");

    if ( !json_object_is_type(poAnswerObj, json_type_array) )
    {
        if ( json_object_is_type(poAnswerObj, json_type_object) )
        {
            json_object* poError = CPL_json_object_object_get(poAnswerObj, "error");
            json_object* poReason = CPL_json_object_object_get(poAnswerObj, "reason");

            const char* pszError = json_object_get_string(poError);
            const char* pszReason = json_object_get_string(poReason);

            if (pszError && pszReason && strcmp(pszError, "not_found") == 0 &&
                strcmp(pszReason, "missing") == 0)
            {
                json_object_put(poAnswerObj);
                poAnswerObj = NULL;

                CPLErrorReset();

                return OpenDatabase() != NULL;
            }
        }
        if (poAnswerObj == NULL)
        {
            IsError(poAnswerObj, "Database listing failed");

            json_object_put(poAnswerObj);
            return FALSE;
        }
    }

    int nTables = json_object_array_length(poAnswerObj);

    for(int i=0;i<nTables;i++)
    {
        json_object* poAnswerObjDBName = json_object_array_get_idx(poAnswerObj, i);
        if ( json_object_is_type(poAnswerObjDBName, json_type_string) )
        {
            const char* pszDBName = json_object_get_string(poAnswerObjDBName);
            if ( strcmp(pszDBName, "_users") != 0 &&
                 strcmp(pszDBName, "_replicator") != 0 )
            {
                papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
                papoLayers[nLayers ++] = new OGRCouchDBTableLayer(this, pszDBName);
            }
        }
    }

    json_object_put(poAnswerObj);

    return TRUE;
}

/************************************************************************/
/*                          ICreateLayer()                              */
/************************************************************************/

OGRLayer   *OGRCloudantDataSource::ICreateLayer( const char *l_pszName,
                                           OGRSpatialReference *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           char ** papszOptions )
{
    if (!IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return NULL;
    }

    char *pszLayerName = CPLStrlwr(CPLStrdup(l_pszName));
    CPLString osLayerName = pszLayerName;
    CPLFree(pszLayerName);

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < GetLayerCount(); iLayer++ )
    {
        if( EQUAL(osLayerName, papoLayers[iLayer]->GetName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                DeleteLayer( osLayerName );
                break;
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          osLayerName.c_str());
                return NULL;
            }
        }
    }

    char* pszEscapedName = CPLEscapeString(osLayerName, -1, CPLES_URL);
    CPLString osEscapedName = pszEscapedName;
    CPLFree(pszEscapedName);

/* -------------------------------------------------------------------- */
/*      Create "database"                                               */
/* -------------------------------------------------------------------- */
    CPLString osURI;
    osURI = "/";
    osURI += osEscapedName;
    json_object* poAnswerObj = PUT(osURI, NULL);

    if (poAnswerObj == NULL)
        return NULL;

    if( !IsOK(poAnswerObj, "Layer creation failed") )
    {
        json_object_put(poAnswerObj);
        return NULL;
    }

    json_object_put(poAnswerObj);

/* -------------------------------------------------------------------- */
/*      Create "spatial index"                                          */
/* -------------------------------------------------------------------- */
    int nUpdateSeq = 0;
    if (eGType != wkbNone)
    {
        char szSrid[100];
        bool bSrid = false;
        const char* designDoc = "_design/SpatialView";
        osURI = "/";
        osURI += osEscapedName;
        osURI += "/";
        osURI += designDoc;

        if (poSpatialRef != NULL)
        {
            // epsg codes are supported in Cloudant
            const char * pszEpsg = NULL;
            const char * pszAuthName = NULL;
            if (poSpatialRef->IsProjected())
            {
                pszAuthName = poSpatialRef->GetAuthorityName("PROJCS");
                if ((pszAuthName != NULL) && (STARTS_WITH(pszAuthName, "EPSG")))
                    pszEpsg = poSpatialRef->GetAuthorityCode("PROJCS");
            }
            else
            {
                pszAuthName = poSpatialRef->GetAuthorityName("GEOGCS");
                if ((pszAuthName != NULL) && (STARTS_WITH(pszAuthName, "EPSG")))
                    pszEpsg = poSpatialRef->GetAuthorityCode("GEOGCS");
            }

            if (pszEpsg != NULL)
            {
                if( snprintf(szSrid, sizeof(szSrid), "urn:ogc:def:crs:epsg::%s",
                             pszEpsg) >= (int)sizeof(szSrid) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Unable to parse SRID");
                    return NULL;
                }
                else
                    bSrid = true;
            }
        }

        // create a spatial design document and serialize it
        json_object* poDoc = json_object_new_object();
        json_object* poStIndexes = json_object_new_object();
        json_object* poSpatial = json_object_new_object();

        json_object_object_add(poDoc, "_id",
                               json_object_new_string(designDoc));
        json_object_object_add(poStIndexes, "spatial", poSpatial);
        json_object_object_add(poSpatial, "index",
            json_object_new_string("function(doc) {if (doc.geometry && doc.geometry.coordinates && doc.geometry.coordinates.length != 0){st_index(doc.geometry);}}"));

        if (bSrid)
            json_object_object_add(poStIndexes, "srsid", json_object_new_string(szSrid));

        json_object_object_add(poDoc, "st_indexes", poStIndexes);

        poAnswerObj = PUT(osURI, json_object_to_json_string(poDoc));

        if( IsOK(poAnswerObj, "Cloudant spatial index creation failed") )
            nUpdateSeq++;

        json_object_put(poDoc);
        json_object_put(poAnswerObj);
    }

    const bool bGeoJSONDocument =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "GEOJSON", "TRUE"));
    int nCoordPrecision = atoi(CSLFetchNameValueDef(papszOptions, "COORDINATE_PRECISION", "-1"));

    OGRCloudantTableLayer* poLayer = new OGRCloudantTableLayer(this, osLayerName);
    if (nCoordPrecision != -1)
        poLayer->SetCoordinatePrecision(nCoordPrecision);
    poLayer->SetInfoAfterCreation(eGType, poSpatialRef,
                                  nUpdateSeq, bGeoJSONDocument);
    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers ++] = poLayer;
    return poLayer;
}
