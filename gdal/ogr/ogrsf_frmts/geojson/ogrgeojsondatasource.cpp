/******************************************************************************
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

#include "cpl_port.h"
#include "ogr_geojson.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_http.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_error.h"
#include "json.h"
// #include "json_object.h"
#include "gdal_utils.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
#include "ogrgeojsonwriter.h"
#include "ogrsf_frmts.h"
// #include "symbol_renames.h"


CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRGeoJSONDataSource()                     */
/************************************************************************/

OGRGeoJSONDataSource::OGRGeoJSONDataSource() :
    pszName_(NULL),
    pszGeoData_(NULL),
    nGeoDataLen_(0),
    papoLayers_(NULL),
    papoLayersWriter_(NULL),
    nLayers_(0),
    fpOut_(NULL),
    flTransGeom_(OGRGeoJSONDataSource::eGeometryPreserve),
    flTransAttrs_(OGRGeoJSONDataSource::eAttributesPreserve),
    bOtherPages_(false),
    bFpOutputIsSeekable_(false),
    nBBOXInsertLocation_(0),
    bUpdatable_(false)
{}

/************************************************************************/
/*                           ~OGRGeoJSONDataSource()                    */
/************************************************************************/

OGRGeoJSONDataSource::~OGRGeoJSONDataSource()
{
    FlushCache();
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
        if( poOpenInfo->eAccess == GA_Update )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Update from remote service not supported");
            return FALSE;
        }
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

    SetDescription( poOpenInfo->pszFilename );
    LoadLayers(poOpenInfo->papszOpenOptions);
    if( nLayers_ == 0 )
    {
        bool bEmitError = true;
        if( eGeoJSONSourceService == nSrcType )
        {
            const CPLString osTmpFilename =
                CPLSPrintf("/vsimem/%p/%s", this,
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

    if( eGeoJSONSourceText == nSrcType && poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Update from inline definition not supported");
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
        if( papoLayers_ )
            return papoLayers_[nLayer];
        else
            return papoLayersWriter_[nLayer];
    }

    return NULL;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer* OGRGeoJSONDataSource::ICreateLayer( const char* pszNameIn,
                                              OGRSpatialReference* poSRS,
                                              OGRwkbGeometryType eGType,
                                              char** papszOptions )
{
    if( NULL == fpOut_ )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSON driver doesn't support creating a layer "
                 "on a read-only datasource");
        return NULL;
    }

    if( nLayers_ != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSON driver doesn't support creating more than one layer");
        return NULL;
    }

    VSIFPrintfL( fpOut_, "{\n\"type\": \"FeatureCollection\",\n" );

    bool bWriteFC_BBOX =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "WRITE_BBOX", "FALSE"));

    const bool bRFC7946 = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "RFC7946", "FALSE"));

    const char* pszNativeData = CSLFetchNameValue(papszOptions, "NATIVE_DATA");
    const char* pszNativeMediaType =
        CSLFetchNameValue(papszOptions, "NATIVE_MEDIA_TYPE");
    bool bWriteCRSIfWGS84 = true;
    bool bFoundNameInNativeData = false;
    if( pszNativeData && pszNativeMediaType &&
        EQUAL(pszNativeMediaType, "application/vnd.geo+json") )
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
            bWriteCRSIfWGS84 = false;
            json_object_object_foreachC(poObj, it)
            {
                if( strcmp(it.key, "type") == 0 ||
                    strcmp(it.key, "features") == 0  )
                {
                    continue;
                }
                if( strcmp(it.key, "bbox") == 0 )
                {
                    if( CSLFetchNameValue(papszOptions, "WRITE_BBOX") == NULL )
                        bWriteFC_BBOX = true;
                    continue;
                }
                if( strcmp(it.key, "crs") == 0 )
                {
                    if( !bRFC7946 )
                        bWriteCRSIfWGS84 = true;
                    continue;
                }
                // See https://tools.ietf.org/html/rfc7946#section-7.1
                if( bRFC7946 &&
                    (strcmp(it.key, "coordinates") == 0 ||
                     strcmp(it.key, "geometries") == 0 ||
                     strcmp(it.key, "geometry") == 0 ||
                     strcmp(it.key, "properties") == 0 ) )
                {
                    continue;
                }

                if( strcmp(it.key, "name") == 0 )
                    bFoundNameInNativeData = true;

                // If a native description exists, ignore it if an explicit
                // DESCRIPTION option has been provided.
                if( strcmp(it.key, "description") == 0 &&
                    CSLFetchNameValue(papszOptions, "DESCRIPTION") )
                {
                    continue;
                }

                json_object* poKey = json_object_new_string(it.key);
                VSIFPrintfL( fpOut_, "%s: ",
                             json_object_to_json_string(poKey) );
                json_object_put(poKey);
                VSIFPrintfL( fpOut_, "%s,\n",
                             json_object_to_json_string(it.val) );
            }
            json_object_put(poObj);
        }
    }

    if( !bFoundNameInNativeData &&
        CPLFetchBool(papszOptions, "WRITE_NAME", true) &&
        !EQUAL(pszNameIn, OGRGeoJSONLayer::DefaultName) &&
        !EQUAL(pszNameIn, "") )
    {
        json_object* poName = json_object_new_string(pszNameIn);
        VSIFPrintfL( fpOut_, "\"name\": %s,\n",
                     json_object_to_json_string(poName) );
        json_object_put(poName);
    }

    const char* pszDescription = CSLFetchNameValue(papszOptions, "DESCRIPTION");
    if( pszDescription )
    {
        json_object* poDesc = json_object_new_string(pszDescription);
        VSIFPrintfL( fpOut_, "\"description\": %s,\n",
                     json_object_to_json_string(poDesc) );
        json_object_put(poDesc);
    }

    OGRCoordinateTransformation* poCT = NULL;
    if( bRFC7946 )
    {
        if( poSRS == NULL )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "No SRS set on layer. Assuming it is long/lat on WGS84 ellipsoid");
        }
        else
        {
            OGRSpatialReference oSRSWGS84;
            oSRSWGS84.SetWellKnownGeogCS( "WGS84" );
            if( !poSRS->IsSame(&oSRSWGS84) )
            {
                poCT = OGRCreateCoordinateTransformation( poSRS, &oSRSWGS84 );
                if( poCT == NULL )
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Failed to create coordinate transformation between the "
                        "input coordinate system and WGS84.  This may be because "
                        "they are not transformable, or because projection "
                        "services (PROJ.4 DLL/.so) could not be loaded." );

                    return NULL;
                }
            }
        }
    }
    else if( poSRS )
    {
        const char* pszAuthority = poSRS->GetAuthorityName(NULL);
        const char* pszAuthorityCode = poSRS->GetAuthorityCode(NULL);
        if( pszAuthority != NULL && pszAuthorityCode != NULL &&
            EQUAL(pszAuthority, "EPSG") &&
            (bWriteCRSIfWGS84 || !EQUAL(pszAuthorityCode, "4326")) )
        {
            json_object* poObjCRS = json_object_new_object();
            json_object_object_add(poObjCRS, "type",
                                   json_object_new_string("name"));
            json_object* poObjProperties = json_object_new_object();
            json_object_object_add(poObjCRS, "properties", poObjProperties);

            if( strcmp(pszAuthorityCode, "4326") == 0 )
            {
                json_object_object_add(
                    poObjProperties, "name",
                    json_object_new_string("urn:ogc:def:crs:OGC:1.3:CRS84"));
            }
            else
            {
                json_object_object_add(
                    poObjProperties, "name",
                    json_object_new_string(
                        CPLSPrintf("urn:ogc:def:crs:EPSG::%s",
                                   pszAuthorityCode)));
            }

            const char* pszCRS = json_object_to_json_string( poObjCRS );
            VSIFPrintfL( fpOut_, "\"crs\": %s,\n", pszCRS );

            json_object_put(poObjCRS);
        }
    }

    if( bFpOutputIsSeekable_ && bWriteFC_BBOX )
    {
        nBBOXInsertLocation_ = static_cast<int>(VSIFTellL( fpOut_ ));

        const std::string osSpaceForBBOX(SPACE_FOR_BBOX + 1, ' ');
        VSIFPrintfL( fpOut_, "%s\n", osSpaceForBBOX.c_str());
    }

    VSIFPrintfL( fpOut_, "\"features\": [\n" );

    OGRGeoJSONWriteLayer* poLayer =
        new OGRGeoJSONWriteLayer( pszNameIn, eGType, papszOptions,
                                  bWriteFC_BBOX, poCT, this );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    CPLAssert(papoLayers_ == NULL);
    papoLayersWriter_ = static_cast<OGRGeoJSONWriteLayer **>(
        CPLRealloc(papoLayers_,
                   sizeof(OGRGeoJSONWriteLayer *) * (nLayers_ + 1)));

    papoLayersWriter_[nLayers_++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONDataSource::TestCapability( const char* pszCap )
{
    if( EQUAL( pszCap, ODsCCreateLayer ) )
        return fpOut_ != NULL && nLayers_ == 0;

    return FALSE;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

int OGRGeoJSONDataSource::Create( const char* pszName,
                                  char** /* papszOptions */ )
{
    CPLAssert( NULL == fpOut_ );

    if( strcmp(pszName, "/dev/stdout") == 0 )
        pszName = "/vsistdout/";

    bFpOutputIsSeekable_ =
        !(strcmp(pszName,"/vsistdout/") == 0 ||
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
    fpOut_ = VSIFOpenExL( pszName, "w", true );
    if( NULL == fpOut_)
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create GeoJSON datasource: %s: %s",
                  pszName, VSIGetLastErrorMsg() );
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

void
OGRGeoJSONDataSource::SetAttributesTranslation( AttributesTranslation type )
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
        if( papoLayers_ != NULL )
            delete papoLayers_[i];
        else
            delete papoLayersWriter_[i];
    }

    CPLFree( papoLayers_ );
    papoLayers_ = NULL;
    CPLFree( papoLayersWriter_ );
    papoLayersWriter_ = NULL;
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
        !VSIIngestFile(poOpenInfo->fpL, poOpenInfo->pszFilename,
                       &pabyOut, NULL, -1) )
    {
        return FALSE;
    }

    VSIFCloseL(poOpenInfo->fpL);
    poOpenInfo->fpL = NULL;
    pszGeoData_ = reinterpret_cast<char *>(pabyOut);

    pszName_ = CPLStrdup( poOpenInfo->pszFilename );

    CPLAssert( NULL != pszGeoData_ );

    bUpdatable_ = ( poOpenInfo->eAccess == GA_Update );

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

    char* papsOptions[] = {
        const_cast<char *>("HEADERS=Accept: text/plain, application/json"),
        NULL
    };

    CPLHTTPResult* pResult = CPLHTTPFetch( pszSource, papsOptions );

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
    char* pszData = reinterpret_cast<char *>(pResult->pabyData);

    if( eGeoJSONProtocolUnknown != GeoJSONGetProtocolType( pszData ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "The data that was downloaded also starts with "
            "protocol prefix (http://, https:// or ftp://) "
            "and cannot be processed as GeoJSON data.");
        CPLHTTPDestroyResult( pResult );
        return FALSE;
    }

    // Directly assign CPLHTTPResult::pabyData to pszGeoData_.
    pszGeoData_ = pszData;
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

void OGRGeoJSONDataSource::LoadLayers(char** papszOpenOptionsIn)
{
    if( NULL == pszGeoData_ )
    {
        CPLError( CE_Failure, CPLE_ObjectNull,
                  "GeoJSON data buffer empty" );
        return;
    }

    const char* const apszPrefix[] = { "loadGeoJSON(", "jsonp(" };
    for( size_t iP = 0; iP < sizeof(apszPrefix) / sizeof(apszPrefix[0]); iP++ )
    {
        if( strncmp(pszGeoData_, apszPrefix[iP], strlen(apszPrefix[iP])) == 0 )
        {
            const size_t nDataLen = strlen(pszGeoData_);
            memmove( pszGeoData_, pszGeoData_ + strlen(apszPrefix[iP]),
                    nDataLen - strlen(apszPrefix[iP]) );
            size_t i = nDataLen - strlen(apszPrefix[iP]);
            pszGeoData_[i] = '\0';
            while( i > 0 && pszGeoData_[i] != ')' )
            {
                i--;
            }
            pszGeoData_[i] = '\0';
        }
    }

    if( !GeoJSONIsObject( pszGeoData_) )
    {
        CPLDebug( "GeoJSON",
                  "No valid GeoJSON data found in source '%s'", pszName_ );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Is it ESRI Feature Service data ?                               */
/* -------------------------------------------------------------------- */
    if( strstr(pszGeoData_, "esriGeometry") ||
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
                    CPL_json_object_object_get(poObj, "exceededTransferLimit");
                if( poExceededTransferLimit &&
                    json_object_get_type(poExceededTransferLimit) ==
                        json_type_boolean )
                {
                    bOtherPages_ = CPL_TO_BOOL(
                        json_object_get_boolean(poExceededTransferLimit) );
                }
            }
            reader.ReadLayers( this );
        }
        return;
    }

/* -------------------------------------------------------------------- */
/*      Is it TopoJSON data ?                                           */
/* -------------------------------------------------------------------- */
    if( strstr(pszGeoData_, "\"type\"") &&
        strstr(pszGeoData_, "\"Topology\"") )
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

    if( eAttributesSkip == flTransAttrs_ )
    {
        reader.SetSkipAttributes( true );
        CPLDebug( "GeoJSON", "Skip all attributes." );
    }

    reader.SetFlattenNestedAttributes(
        CPLFetchBool(papszOpenOptionsIn, "FLATTEN_NESTED_ATTRIBUTES", false),
        CSLFetchNameValueDef(papszOpenOptionsIn,
                             "NESTED_ATTRIBUTE_SEPARATOR", "_")[0]);

    const bool bDefaultNativeData = bUpdatable_;
    reader.SetStoreNativeData(
        CPLFetchBool(papszOpenOptionsIn, "NATIVE_DATA", bDefaultNativeData));

    reader.SetArrayAsString(
        CPLTestBool(CSLFetchNameValueDef(papszOpenOptionsIn, "ARRAY_AS_STRING",
                CPLGetConfigOption("OGR_GEOJSON_ARRAY_AS_STRING", "NO"))));

/* -------------------------------------------------------------------- */
/*      Parse GeoJSON and build valid OGRLayer instance.                */
/* -------------------------------------------------------------------- */
    const OGRErr err = reader.Parse( pszGeoData_ );
    if( OGRERR_NONE == err )
    {
        json_object* poObj = reader.GetJSonObject();
        if( poObj && json_object_get_type(poObj) == json_type_object )
        {
            json_object* poProperties =
                CPL_json_object_object_get(poObj, "properties");
            if( poProperties &&
                json_object_get_type(poProperties) == json_type_object )
            {
                json_object* poExceededTransferLimit =
                    CPL_json_object_object_get(poProperties,
                                               "exceededTransferLimit");
                if( poExceededTransferLimit &&
                    json_object_get_type(poExceededTransferLimit) ==
                        json_type_boolean )
                {
                    bOtherPages_ = CPL_TO_BOOL(
                        json_object_get_boolean(poExceededTransferLimit) );
                }
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
    CPLAssert(papoLayersWriter_ == NULL);

    poLayer->DetectGeometryType();

    // Return layer in readable state.
    poLayer->ResetReading();

    papoLayers_ = static_cast<OGRGeoJSONLayer**>(
        CPLRealloc( papoLayers_, sizeof(OGRGeoJSONLayer*) * (nLayers_ + 1)));
    papoLayers_[nLayers_] = poLayer;
    nLayers_++;
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

void OGRGeoJSONDataSource::FlushCache()
{
    if( papoLayersWriter_ != NULL )
        return;

    for( int i = 0; i < nLayers_; i++ )
    {
        if( papoLayers_[i]->HasBeenUpdated() )
        {
            papoLayers_[i]->SetUpdated(false);

            bool bOK = false;

            // Disable all filters.
            OGRFeatureQuery *poAttrQueryBak = papoLayers_[i]->m_poAttrQuery;
            papoLayers_[i]->m_poAttrQuery = NULL;
            OGRGeometry* poFilterGeomBak = papoLayers_[i]->m_poFilterGeom;
            papoLayers_[i]->m_poFilterGeom = NULL;

            // If the source data only contained one single feature and
            // that's still the case, then do not use a FeatureCollection
            // on writing.
            bool bAlreadyDone = false;
            if( papoLayers_[i]->GetFeatureCount(TRUE) == 1 &&
                papoLayers_[i]->GetMetadata("NATIVE_DATA") == NULL )
            {
                papoLayers_[i]->ResetReading();
                OGRFeature* poFeature = papoLayers_[i]->GetNextFeature();
                if( poFeature != NULL )
                {
                    if( poFeature->GetNativeData() != NULL )
                    {
                        bAlreadyDone = true;
                        OGRGeoJSONWriteOptions oOptions;
                        json_object* poObj =
                            OGRGeoJSONWriteFeature(poFeature, oOptions);
                        VSILFILE* fp = VSIFOpenL(pszName_, "wb");
                        if( fp != NULL )
                        {
                            bOK =
                                VSIFPrintfL(
                                    fp, "%s",
                                    json_object_to_json_string(poObj) ) > 0;
                            VSIFCloseL( fp );
                        }
                        json_object_put( poObj );
                    }
                    delete poFeature;
                }
            }

            // Otherwise do layer translation.
            if( !bAlreadyDone )
            {
                char** papszOptions = CSLAddString(NULL, "-f");
                papszOptions = CSLAddString(papszOptions, "GeoJSON");
                GDALVectorTranslateOptions* psOptions =
                    GDALVectorTranslateOptionsNew(papszOptions, NULL);
                CSLDestroy(papszOptions);
                GDALDatasetH hSrcDS = this;
                CPLString osNewFilename(pszName_);
                osNewFilename += ".tmp";
                GDALDatasetH hOutDS =
                    GDALVectorTranslate(osNewFilename, NULL, 1, &hSrcDS,
                                        psOptions, NULL);
                GDALVectorTranslateOptionsFree(psOptions);

                if( hOutDS != NULL )
                {
                    CPLErrorReset();
                    GDALClose(hOutDS);
                    bOK = (CPLGetLastErrorType() == CE_None);
                }
                if( bOK )
                {
                    CPLString osBackup(pszName_);
                    osBackup += ".bak";
                    if( VSIRename(pszName_, osBackup) < 0 )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "Cannot create backup copy");
                    }
                    else if( VSIRename(osNewFilename, pszName_) < 0 )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "Cannot rename %s to %s",
                                osNewFilename.c_str(), pszName_);
                    }
                    else
                    {
                        VSIUnlink(osBackup);
                    }
                }
            }

            // Restore filters.
            papoLayers_[i]->m_poAttrQuery = poAttrQueryBak;
            papoLayers_[i]->m_poFilterGeom = poFilterGeomBak;
        }
    }
}
