/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONDataSource class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_geojson.h"
#include "ogrgeojsonutils.h"
#include "ogrgeojsonreader.h"
#include <cpl_http.h>
#include <json.h> // JSON-C
#include <cstddef>
#include <cstdlib>
using namespace std;

/************************************************************************/
/*                           OGRGeoJSONDataSource()                     */
/************************************************************************/

OGRGeoJSONDataSource::OGRGeoJSONDataSource()
    : pszName_(NULL), pszGeoData_(NULL), nGeoDataLen_(0),
        papoLayers_(NULL), nLayers_(0), fpOut_(NULL),
        flTransGeom_( OGRGeoJSONDataSource::eGeometryPreserve ),
        flTransAttrs_( OGRGeoJSONDataSource::eAtributesPreserve ),
        bOtherPages_(FALSE),
        bFpOutputIsSeekable_( FALSE ),
        nBBOXInsertLocation_(0)
{ }

/************************************************************************/
/*                           ~OGRGeoJSONDataSource()                    */
/************************************************************************/

OGRGeoJSONDataSource::~OGRGeoJSONDataSource()
{
    Clear();
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

int OGRGeoJSONDataSource::Open( GDALOpenInfo* poOpenInfo,
                                GeoJSONSourceType nSrcType )
{
    if( eGeoJSONSourceService == nSrcType )
    {
        if( !ReadFromService( poOpenInfo->pszFilename ) )
            return FALSE;
    }
    else if( eGeoJSONSourceText == nSrcType )
    {
        pszGeoData_ = CPLStrdup( poOpenInfo->pszFilename );
    }
    else if( eGeoJSONSourceFile == nSrcType )
    {
        if( !ReadFromFile( poOpenInfo ) )
            return FALSE;
    }
    else
    {
        Clear();
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Construct OGR layer and feature objects from                    */
/*      GeoJSON text tree.                                              */
/* -------------------------------------------------------------------- */
    if( NULL == pszGeoData_ ||
        STARTS_WITH(pszGeoData_, "{\"couchdb\":\"Welcome\"") ||
        STARTS_WITH(pszGeoData_, "{\"db_name\":\"") ||
        STARTS_WITH(pszGeoData_, "{\"total_rows\":") ||
        STARTS_WITH(pszGeoData_, "{\"rows\":["))
    {
        Clear();
        return FALSE;
    }

    LoadLayers(poOpenInfo->papszOpenOptions);
    if( nLayers_ == 0 )
    {
        bool bEmitError = true;
        if( eGeoJSONSourceService == nSrcType )
        {
            CPLString osTmpFilename = CPLSPrintf("/vsimem/%p/%s", this,
                                        CPLGetFilename(poOpenInfo->pszFilename));
            VSIFCloseL(VSIFileFromMemBuffer( osTmpFilename,
                                             (GByte*)pszGeoData_,
                                             nGeoDataLen_,
                                             TRUE ));
            pszGeoData_ = NULL;
            if( GDALIdentifyDriver(osTmpFilename, NULL) )
                bEmitError = false;
            VSIUnlink(osTmpFilename);
        }
        Clear();

        if( bEmitError )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "Failed to read GeoJSON data" );
        }
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           GetName()                                  */
/************************************************************************/

const char* OGRGeoJSONDataSource::GetName()
{
    return pszName_ ? pszName_ : "";
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRGeoJSONDataSource::GetLayerCount()
{
    return nLayers_;
}

/************************************************************************/
/*                           GetLayer()                                 */
/************************************************************************/

OGRLayer* OGRGeoJSONDataSource::GetLayer( int nLayer )
{
    if( 0 <= nLayer && nLayer < nLayers_ )
    {
        return papoLayers_[nLayer];
    }

    return NULL;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer* OGRGeoJSONDataSource::ICreateLayer( const char* pszName_,
                                             OGRSpatialReference* poSRS,
                                             OGRwkbGeometryType eGType,
                                             char** papszOptions )
{
    if ( NULL == fpOut_ )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSON driver doesn't support creating a layer on a read-only datasource");
        return NULL;
    }

    if ( nLayers_ != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSON driver doesn't support creating more than one layer");
        return NULL;
    }

    OGRGeoJSONWriteLayer* poLayer
        = new OGRGeoJSONWriteLayer( pszName_, eGType, papszOptions, this );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */

    papoLayers_ = (OGRLayer **)
        CPLRealloc( papoLayers_,  sizeof(OGRLayer*) * (nLayers_ + 1) );

    papoLayers_[nLayers_++] = poLayer;

    VSIFPrintfL( fpOut_, "{\n\"type\": \"FeatureCollection\",\n" );
    
    const char* pszNativeData = CSLFetchNameValue(papszOptions, "NATIVE_DATA");
    const char* pszNativeMediaType = CSLFetchNameValue(papszOptions, "NATIVE_MEDIA_TYPE");
    if( pszNativeMediaType && EQUAL(pszNativeMediaType, "application/vnd.geo+json") )
    {
        json_object *poObj = NULL;
        if( OGRJSonParse(pszNativeData, &poObj) &&
            json_object_get_type(poObj) == json_type_object )
        {
            json_object_iter it;
            it.key = NULL;
            it.val = NULL;
            it.entry = NULL;
            CPLString osNativeData;
            json_object_object_foreachC(poObj, it)
            {
                if( strcmp(it.key, "type") == 0 ||
                    strcmp(it.key, "features") == 0||
                    strcmp(it.key, "crs") == 0 )
                {
                    continue;
                }
                json_object* poKey = json_object_new_string(it.key);
                VSIFPrintfL( fpOut_, "%s: ", json_object_to_json_string(poKey) );
                json_object_put(poKey);
                VSIFPrintfL( fpOut_, "%s,\n", json_object_to_json_string(it.val) );
            }
            json_object_put(poObj);
        }
    }

    if (poSRS)
    {
        const char* pszAuthority = poSRS->GetAuthorityName(NULL);
        const char* pszAuthorityCode = poSRS->GetAuthorityCode(NULL);
        if (pszAuthority != NULL && pszAuthorityCode != NULL &&
            EQUAL(pszAuthority, "EPSG"))
        {
            json_object* poObjCRS = json_object_new_object();
            json_object_object_add(poObjCRS, "type",
                                json_object_new_string("name"));
            json_object* poObjProperties = json_object_new_object();
            json_object_object_add(poObjCRS, "properties", poObjProperties);

            if (strcmp(pszAuthorityCode, "4326") == 0)
            {
                json_object_object_add(poObjProperties, "name",
                                    json_object_new_string("urn:ogc:def:crs:OGC:1.3:CRS84"));
            }
            else
            {
                json_object_object_add(poObjProperties, "name",
                                    json_object_new_string(CPLSPrintf("urn:ogc:def:crs:EPSG::%s", pszAuthorityCode)));
            }

            const char* pszCRS = json_object_to_json_string( poObjCRS );
            VSIFPrintfL( fpOut_, "\"crs\": %s,\n", pszCRS );

            json_object_put(poObjCRS);
        }
    }

    if (bFpOutputIsSeekable_)
    {
        nBBOXInsertLocation_ = (int) VSIFTellL( fpOut_ );

        char szSpaceForBBOX[SPACE_FOR_BBOX+1];
        memset(szSpaceForBBOX, ' ', SPACE_FOR_BBOX);
        szSpaceForBBOX[SPACE_FOR_BBOX] = '\0';
        VSIFPrintfL( fpOut_, "%s\n", szSpaceForBBOX);
    }

    VSIFPrintfL( fpOut_, "\"features\": [\n" );

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONDataSource::TestCapability( const char* pszCap )
{
    if( EQUAL( pszCap, ODsCCreateLayer ) )
        return fpOut_ != NULL && nLayers_ == 0;

    // if( EQUAL( pszCap, ODsCDeleteLayer ) )
    //     return FALSE;

    return FALSE;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

int OGRGeoJSONDataSource::Create( const char* pszName,
                                  char** /* papszOptions */ )
{
    CPLAssert( NULL == fpOut_ );

    if (strcmp(pszName, "/dev/stdout") == 0)
        pszName = "/vsistdout/";

    bFpOutputIsSeekable_ =  !(strcmp(pszName,"/vsistdout/") == 0 ||
                              STARTS_WITH(pszName, "/vsigzip/") ||
                              STARTS_WITH(pszName, "/vsizip/"));

/* -------------------------------------------------------------------- */
/*     File overwrite not supported.                                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;
    if( 0 == VSIStatL( pszName, &sStatBuf ) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The GeoJSON driver does not overwrite existing files." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    fpOut_ = VSIFOpenL( pszName, "w" );
    if( NULL == fpOut_)
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create GeoJSON datasource: %s.", 
                  pszName );
        return FALSE;
    }

    pszName_ = CPLStrdup( pszName );

    return TRUE;
}

/************************************************************************/
/*                           SetGeometryTranslation()                   */
/************************************************************************/

void
OGRGeoJSONDataSource::SetGeometryTranslation( GeometryTranslation type )
{
    flTransGeom_ = type;
}

/************************************************************************/
/*                           SetAttributesTranslation()                 */
/************************************************************************/

void OGRGeoJSONDataSource::SetAttributesTranslation( AttributesTranslation type )
{
    flTransAttrs_ = type;
}

/************************************************************************/
/*                  PRIVATE FUNCTIONS IMPLEMENTATION                    */
/************************************************************************/

void OGRGeoJSONDataSource::Clear()
{
    for( int i = 0; i < nLayers_; i++ )
    {
        CPLAssert( NULL != papoLayers_ );
        delete papoLayers_[i];
    }

    CPLFree( papoLayers_ );
    papoLayers_ = NULL;
    nLayers_ = 0;

    CPLFree( pszName_ );
    pszName_ = NULL;

    CPLFree( pszGeoData_ );
    pszGeoData_ = NULL;
    nGeoDataLen_ = 0;

    if( fpOut_ )
    {
        VSIFCloseL( fpOut_ );
        fpOut_ = NULL;
    }
}

/************************************************************************/
/*                           ReadFromFile()                             */
/************************************************************************/

int OGRGeoJSONDataSource::ReadFromFile( GDALOpenInfo* poOpenInfo )
{
    GByte* pabyOut = NULL;
    if( poOpenInfo->fpL == NULL ||
        !VSIIngestFile( poOpenInfo->fpL, poOpenInfo->pszFilename, &pabyOut, NULL, -1) )
    {
        return FALSE;
    }

    VSIFCloseL(poOpenInfo->fpL);
    poOpenInfo->fpL = NULL;
    pszGeoData_ = (char*) pabyOut;

    pszName_ = CPLStrdup( poOpenInfo->pszFilename );

    CPLAssert( NULL != pszGeoData_ );
    return TRUE;
}

/************************************************************************/
/*                           ReadFromService()                          */
/************************************************************************/

int OGRGeoJSONDataSource::ReadFromService( const char* pszSource )
{
    CPLAssert( NULL == pszGeoData_ );
    CPLAssert( NULL != pszSource );

    if( eGeoJSONProtocolUnknown == GeoJSONGetProtocolType( pszSource ) )
    {
        CPLDebug( "GeoJSON", "Unknown service type (use HTTP, HTTPS, FTP)" );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the GeoJSON result.                                        */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    char* papsOptions[] = { (char*) "HEADERS=Accept: text/plain, application/json", NULL };

    CPLHTTPResult* pResult
        = CPLHTTPFetch( pszSource, papsOptions );

/* -------------------------------------------------------------------- */
/*      Try to handle CURL/HTTP errors.                                 */
/* -------------------------------------------------------------------- */
    if( NULL == pResult
        || 0 == pResult->nDataLen || 0 != CPLGetLastErrorNo() )
    {
        CPLHTTPDestroyResult( pResult );
        return FALSE;
    }

   if( 0 != pResult->nStatus )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Curl reports error: %d: %s",
                  pResult->nStatus, pResult->pszErrBuf );
        CPLHTTPDestroyResult( pResult );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Copy returned GeoJSON data to text buffer.                      */
/* -------------------------------------------------------------------- */
    const char* pszData = reinterpret_cast<char*>(pResult->pabyData);

    if ( eGeoJSONProtocolUnknown != GeoJSONGetProtocolType( pszData ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "The data that was downloaded also starts with "
            "protocol prefix (http://, https:// or ftp://) "
            "and cannot be processed as GeoJSON data.");
        CPLHTTPDestroyResult( pResult );
        return FALSE;
    }

    // Directly assign CPLHTTPResult::pabyData to pszGeoData_
    pszGeoData_ = (char*) pszData;
    nGeoDataLen_ = pResult->nDataLen;
    pResult->pabyData = NULL;
    pResult->nDataLen = 0;

    pszName_ = CPLStrdup( pszSource );

/* -------------------------------------------------------------------- */
/*      Cleanup HTTP resources.                                         */
/* -------------------------------------------------------------------- */
    CPLHTTPDestroyResult( pResult );

    CPLAssert( NULL != pszGeoData_ );
    return TRUE;
}

/************************************************************************/
/*                           LoadLayers()                               */
/************************************************************************/

void OGRGeoJSONDataSource::LoadLayers(char** papszOpenOptions)
{
    if( NULL == pszGeoData_ )
    {
        CPLError( CE_Failure, CPLE_ObjectNull,
                  "GeoJSON data buffer empty" );
        return;
    }

    const char* const apszPrefix[] = { "loadGeoJSON(", "jsonp(" };
    for(size_t iP = 0; iP < sizeof(apszPrefix) / sizeof(apszPrefix[0]); iP++ )
    {
        if( strncmp(pszGeoData_, apszPrefix[iP], strlen(apszPrefix[iP])) == 0 )
        {
            size_t nDataLen = strlen(pszGeoData_);
            memmove( pszGeoData_, pszGeoData_ + strlen(apszPrefix[iP]),
                    nDataLen - strlen(apszPrefix[iP]) );
            size_t i = nDataLen - strlen(apszPrefix[iP]);
            pszGeoData_[i] = '\0';
            while( i > 0 && pszGeoData_[i] != ')' )
            {
                i --;
            }
            pszGeoData_[i] = '\0';
        }
    }


    if ( !GeoJSONIsObject( pszGeoData_) )
    {
        CPLDebug( "GeoJSON", "No valid GeoJSON data found in source '%s'", pszName_ );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Is it ESRI Feature Service data ?                               */
/* -------------------------------------------------------------------- */
    if ( strstr(pszGeoData_, "esriGeometry") ||
         strstr(pszGeoData_, "esriFieldType") )
    {
        OGRESRIJSONReader reader;
        OGRErr err = reader.Parse( pszGeoData_ );
        if( OGRERR_NONE == err )
        {
            json_object* poObj = reader.GetJSonObject();
            if( poObj && json_object_get_type(poObj) == json_type_object )
            {
                json_object* poExceededTransferLimit =
                    json_object_object_get(poObj, "exceededTransferLimit");
                if( poExceededTransferLimit && json_object_get_type(poExceededTransferLimit) == json_type_boolean )
                    bOtherPages_ = json_object_get_boolean(poExceededTransferLimit);
            }
            reader.ReadLayers( this );
        }
        return;
    }

/* -------------------------------------------------------------------- */
/*      Is it TopoJSON data ?                                           */
/* -------------------------------------------------------------------- */
    if ( strstr(pszGeoData_, "\"type\"") &&
         strstr(pszGeoData_, "\"Topology\"")  )
    {
        OGRTopoJSONReader reader;
        OGRErr err = reader.Parse( pszGeoData_ );
        if( OGRERR_NONE == err )
        {
            reader.ReadLayers( this );
        }
        return;
    }

/* -------------------------------------------------------------------- */
/*      Configure GeoJSON format translator.                            */
/* -------------------------------------------------------------------- */
    OGRGeoJSONReader reader;

    if( eGeometryAsCollection == flTransGeom_ )
    {
        reader.SetPreserveGeometryType( false );
        CPLDebug( "GeoJSON", "Geometry as OGRGeometryCollection type." );
    }

    if( eAtributesSkip == flTransAttrs_ )
    {
        reader.SetSkipAttributes( true );
        CPLDebug( "GeoJSON", "Skip all attributes." );
    }

    reader.SetFlattenNestedAttributes(
        (bool)CSLFetchBoolean(papszOpenOptions, "FLATTEN_NESTED_ATTRIBUTES", FALSE),
        CSLFetchNameValueDef(papszOpenOptions, "NESTED_ATTRIBUTE_SEPARATOR", "_")[0]);

    reader.SetStoreNativeData(
        (bool)CSLFetchBoolean(papszOpenOptions, "NATIVE_DATA", FALSE));

/* -------------------------------------------------------------------- */
/*      Parse GeoJSON and build valid OGRLayer instance.                */
/* -------------------------------------------------------------------- */
    OGRErr err = reader.Parse( pszGeoData_ );
    if( OGRERR_NONE == err )
    {
        json_object* poObj = reader.GetJSonObject();
        if( poObj && json_object_get_type(poObj) == json_type_object )
        {
            json_object* poProperties = json_object_object_get(poObj, "properties");
            if( poProperties && json_object_get_type(poProperties) == json_type_object )
            {
                json_object* poExceededTransferLimit =
                    json_object_object_get(poProperties, "exceededTransferLimit");
                if( poExceededTransferLimit && json_object_get_type(poExceededTransferLimit) == json_type_boolean )
                    bOtherPages_ = json_object_get_boolean(poExceededTransferLimit);
            }
        }

        reader.ReadLayers( this );
    }

    return;
}

/************************************************************************/
/*                            AddLayer()                                */
/************************************************************************/

void OGRGeoJSONDataSource::AddLayer( OGRGeoJSONLayer* poLayer )
{
    poLayer->DetectGeometryType();

    /* Return layer in readable state. */
    poLayer->ResetReading();

    papoLayers_ = (OGRLayer**)CPLRealloc( papoLayers_, sizeof(OGRLayer*) * (nLayers_ + 1));
    papoLayers_[nLayers_] = poLayer; 
    nLayers_ ++;
}
