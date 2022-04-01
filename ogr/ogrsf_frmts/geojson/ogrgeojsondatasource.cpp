/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONDataSource class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
    pszName_(nullptr),
    pszGeoData_(nullptr),
    nGeoDataLen_(0),
    papoLayers_(nullptr),
    papoLayersWriter_(nullptr),
    nLayers_(0),
    fpOut_(nullptr),
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
    OGRGeoJSONDataSource::FlushCache(true);
    OGRGeoJSONDataSource::Clear();
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

int OGRGeoJSONDataSource::Open( GDALOpenInfo* poOpenInfo,
                                GeoJSONSourceType nSrcType,
                                const char* pszJSonFlavor )
{
    osJSonFlavor_ = pszJSonFlavor;

    const char* pszUnprefixed = poOpenInfo->pszFilename;
    if( STARTS_WITH_CI(pszUnprefixed, pszJSonFlavor) &&
        pszUnprefixed[strlen(pszJSonFlavor)] == ':' )
    {
        pszUnprefixed += strlen(pszJSonFlavor) + 1;
    }

    if( eGeoJSONSourceService == nSrcType )
    {
        if( !ReadFromService( poOpenInfo, pszUnprefixed ) )
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
        if( poOpenInfo->eAccess == GA_Update )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                        "Update from inline definition not supported");
            return FALSE;
        }
        pszGeoData_ = CPLStrdup( pszUnprefixed );
    }
    else if( eGeoJSONSourceFile == nSrcType )
    {
        if( poOpenInfo->eAccess == GA_Update &&
            !EQUAL(pszJSonFlavor, "GeoJSON") )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                        "Update of %s not supported", pszJSonFlavor);
            return FALSE;
        }
        pszName_ = CPLStrdup( pszUnprefixed );
        bUpdatable_ = ( poOpenInfo->eAccess == GA_Update );

        if( !EQUAL(pszUnprefixed, poOpenInfo->pszFilename) )
        {
            GDALOpenInfo oOpenInfo(pszUnprefixed, GA_ReadOnly);
            if( oOpenInfo.fpL == nullptr || oOpenInfo.pabyHeader == nullptr )
                return FALSE;
            pszGeoData_ = CPLStrdup(
                reinterpret_cast<const char*>(oOpenInfo.pabyHeader));
        }
        else if( poOpenInfo->fpL == nullptr )
            return FALSE;
        else
        {
            pszGeoData_ = CPLStrdup(
                reinterpret_cast<const char*>(poOpenInfo->pabyHeader));
        }
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
    if( nullptr == pszGeoData_ ||
        STARTS_WITH(pszGeoData_, "{\"couchdb\":\"Welcome\"") ||
        STARTS_WITH(pszGeoData_, "{\"db_name\":\"") ||
        STARTS_WITH(pszGeoData_, "{\"total_rows\":") ||
        STARTS_WITH(pszGeoData_, "{\"rows\":["))
    {
        Clear();
        return FALSE;
    }

    SetDescription( poOpenInfo->pszFilename );
    LoadLayers(poOpenInfo, nSrcType, pszUnprefixed, pszJSonFlavor);
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
            pszGeoData_ = nullptr;
            if( GDALIdentifyDriver(osTmpFilename, nullptr) )
                bEmitError = false;
            VSIUnlink(osTmpFilename);
        }
        Clear();

        if( bEmitError )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to read %s data", pszJSonFlavor );
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
        if( papoLayers_ )
            return papoLayers_[nLayer];
        else
            return papoLayersWriter_[nLayer];
    }

    return nullptr;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer* OGRGeoJSONDataSource::ICreateLayer( const char* pszNameIn,
                                              OGRSpatialReference* poSRS,
                                              OGRwkbGeometryType eGType,
                                              char** papszOptions )
{
    if( nullptr == fpOut_ )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSON driver doesn't support creating a layer "
                 "on a read-only datasource");
        return nullptr;
    }

    if( nLayers_ != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSON driver doesn't support creating more than one layer");
        return nullptr;
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
        json_object *poObj = nullptr;
        if( OGRJSonParse(pszNativeData, &poObj) &&
            json_object_get_type(poObj) == json_type_object )
        {
            json_object_iter it;
            it.key = nullptr;
            it.val = nullptr;
            it.entry = nullptr;
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
                    if( CSLFetchNameValue(papszOptions, "WRITE_BBOX") == nullptr )
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
                {
                    bFoundNameInNativeData = true;
                    if( !CPLFetchBool(papszOptions, "WRITE_NAME", true) )
                    {
                        continue;
                    }
                }

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

    OGRCoordinateTransformation* poCT = nullptr;
    if( bRFC7946 )
    {
        if( poSRS == nullptr )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "No SRS set on layer. Assuming it is long/lat on WGS84 ellipsoid");
        }
        else if( poSRS->GetAxesCount() == 3 )
        {
            OGRSpatialReference oSRS_EPSG_4979;
            oSRS_EPSG_4979.importFromEPSG(4979);
            oSRS_EPSG_4979.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if( !poSRS->IsSame(&oSRS_EPSG_4979) )
            {
                poCT = OGRCreateCoordinateTransformation( poSRS, &oSRS_EPSG_4979 );
                if( poCT == nullptr )
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Failed to create coordinate transformation between the "
                        "input coordinate system and WGS84." );

                    return nullptr;
                }
            }
        }
        else
        {
            OGRSpatialReference oSRSWGS84;
            oSRSWGS84.SetWellKnownGeogCS( "WGS84" );
            oSRSWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if( !poSRS->IsSame(&oSRSWGS84) )
            {
                poCT = OGRCreateCoordinateTransformation( poSRS, &oSRSWGS84 );
                if( poCT == nullptr )
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Failed to create coordinate transformation between the "
                        "input coordinate system and WGS84." );

                    return nullptr;
                }
            }
        }
    }
    else if( poSRS )
    {
        char* pszOGCURN = poSRS->GetOGCURN();
        if( pszOGCURN != nullptr &&
            (bWriteCRSIfWGS84 || !EQUAL(pszOGCURN, "urn:ogc:def:crs:EPSG::4326")) )
        {
            json_object* poObjCRS = json_object_new_object();
            json_object_object_add(poObjCRS, "type",
                                   json_object_new_string("name"));
            json_object* poObjProperties = json_object_new_object();
            json_object_object_add(poObjCRS, "properties", poObjProperties);

            if( EQUAL(pszOGCURN, "urn:ogc:def:crs:EPSG::4326") )
            {
                json_object_object_add(
                    poObjProperties, "name",
                    json_object_new_string("urn:ogc:def:crs:OGC:1.3:CRS84"));
            }
            else
            {
                json_object_object_add(
                    poObjProperties, "name",
                    json_object_new_string(pszOGCURN));
            }

            const char* pszCRS = json_object_to_json_string( poObjCRS );
            VSIFPrintfL( fpOut_, "\"crs\": %s,\n", pszCRS );

            json_object_put(poObjCRS);
        }
        CPLFree(pszOGCURN);
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
    CPLAssert(papoLayers_ == nullptr);
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
        return fpOut_ != nullptr && nLayers_ == 0;

    return FALSE;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

int OGRGeoJSONDataSource::Create( const char* pszName,
                                  char** /* papszOptions */ )
{
    CPLAssert( nullptr == fpOut_ );

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
    if( nullptr == fpOut_)
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
        if( papoLayers_ != nullptr )
            delete papoLayers_[i];
        else
            delete papoLayersWriter_[i];
    }

    CPLFree( papoLayers_ );
    papoLayers_ = nullptr;
    CPLFree( papoLayersWriter_ );
    papoLayersWriter_ = nullptr;
    nLayers_ = 0;

    CPLFree( pszName_ );
    pszName_ = nullptr;

    CPLFree( pszGeoData_ );
    pszGeoData_ = nullptr;
    nGeoDataLen_ = 0;

    if( fpOut_ )
    {
        VSIFCloseL( fpOut_ );
        fpOut_ = nullptr;
    }
}

/************************************************************************/
/*                           ReadFromFile()                             */
/************************************************************************/

int OGRGeoJSONDataSource::ReadFromFile( GDALOpenInfo* poOpenInfo,
                                        const char* pszUnprefixed )
{
    GByte* pabyOut = nullptr;
    if( !EQUAL(poOpenInfo->pszFilename, pszUnprefixed) )
    {
        GDALOpenInfo oOpenInfo(pszUnprefixed, GA_ReadOnly);
        if( oOpenInfo.fpL == nullptr || oOpenInfo.pabyHeader == nullptr )
            return FALSE;
        VSIFSeekL(oOpenInfo.fpL, 0, SEEK_SET );
        if( !VSIIngestFile(oOpenInfo.fpL, pszUnprefixed,
                        &pabyOut, nullptr, -1) )
        {
            return FALSE;
        }
    }
    else
    {
        if( poOpenInfo->fpL == nullptr )
            return FALSE;
        VSIFSeekL( poOpenInfo->fpL, 0, SEEK_SET );
        if( !VSIIngestFile(poOpenInfo->fpL, poOpenInfo->pszFilename,
                        &pabyOut, nullptr, -1) )
        {
            return FALSE;
        }

        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
    }

    CPLFree(pszGeoData_);
    pszGeoData_ = reinterpret_cast<char *>(pabyOut);

    CPLAssert( nullptr != pszGeoData_ );

    return TRUE;
}

/************************************************************************/
/*                           ReadFromService()                          */
/************************************************************************/

int OGRGeoJSONDataSource::ReadFromService( GDALOpenInfo* poOpenInfo,
                                           const char* pszSource )
{
    CPLAssert( nullptr == pszGeoData_ );
    CPLAssert( nullptr != pszSource );

    CPLErrorReset();

/* -------------------------------------------------------------------- */
/*      Look if we already cached the content.                          */
/* -------------------------------------------------------------------- */
    char* pszStoredContent = OGRGeoJSONDriverStealStoredContent(pszSource);
    if( pszStoredContent != nullptr )
    {
        if( (osJSonFlavor_ == "ESRIJSON" && ESRIJSONIsObject(pszStoredContent)) ||
            (osJSonFlavor_ == "TopoJSON" && TopoJSONIsObject(pszStoredContent)) )
        {
            pszGeoData_ = pszStoredContent;
            nGeoDataLen_ = strlen(pszGeoData_);

            pszName_ = CPLStrdup( pszSource );
            return true;
        }

        OGRGeoJSONDriverStoreContent( pszSource, pszStoredContent );
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the GeoJSON result.                                        */
/* -------------------------------------------------------------------- */
    char* papsOptions[] = {
        const_cast<char *>("HEADERS=Accept: text/plain, application/json"),
        nullptr
    };

    CPLHTTPResult* pResult = CPLHTTPFetch( pszSource, papsOptions );

/* -------------------------------------------------------------------- */
/*      Try to handle CURL/HTTP errors.                                 */
/* -------------------------------------------------------------------- */
    if( nullptr == pResult
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

    // Directly assign CPLHTTPResult::pabyData to pszGeoData_.
    pszGeoData_ = pszData;
    nGeoDataLen_ = pResult->nDataLen;
    pResult->pabyData = nullptr;
    pResult->nDataLen = 0;

    pszName_ = CPLStrdup( pszSource );

/* -------------------------------------------------------------------- */
/*      Cleanup HTTP resources.                                         */
/* -------------------------------------------------------------------- */
    CPLHTTPDestroyResult( pResult );

    CPLAssert( nullptr != pszGeoData_ );

/* -------------------------------------------------------------------- */
/*      Cache the content if it is not handled by this driver, but      */
/*      another related one.                                            */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszSource, poOpenInfo->pszFilename) &&
        osJSonFlavor_ == "GeoJSON" )
    {
        if( !GeoJSONIsObject(pszGeoData_) )
        {
            if( ESRIJSONIsObject(pszGeoData_) ||
                TopoJSONIsObject(pszGeoData_) ||
                GeoJSONSeqIsObject(pszGeoData_) )
            {
                OGRGeoJSONDriverStoreContent( pszSource, pszGeoData_ );
                pszGeoData_ = nullptr;
                nGeoDataLen_ = 0;
            }
            return false;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                       RemoveJSonPStuff()                             */
/************************************************************************/

void OGRGeoJSONDataSource::RemoveJSonPStuff()
{
    const char* const apszPrefix[] = { "loadGeoJSON(", "jsonp(" };
    for( size_t iP = 0; iP < CPL_ARRAYSIZE(apszPrefix); iP++ )
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
}

/************************************************************************/
/*                           LoadLayers()                               */
/************************************************************************/

void OGRGeoJSONDataSource::LoadLayers(GDALOpenInfo* poOpenInfo,
                                      GeoJSONSourceType nSrcType,
                                      const char* pszUnprefixed,
                                      const char* pszJSonFlavor)
{
    if( nullptr == pszGeoData_ )
    {
        CPLError( CE_Failure, CPLE_ObjectNull,
                  "%s data buffer empty", pszJSonFlavor );
        return;
    }

    if( nSrcType != eGeoJSONSourceFile )
    {
        RemoveJSonPStuff();
    }

/* -------------------------------------------------------------------- */
/*      Is it ESRI Feature Service data ?                               */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszJSonFlavor, "ESRIJSON") )
    {
        OGRESRIJSONReader reader;
        if( nSrcType == eGeoJSONSourceFile )
        {
            if( !ReadFromFile( poOpenInfo, pszUnprefixed ) )
                return;
        }
        OGRErr err = reader.Parse( pszGeoData_ );
        if( OGRERR_NONE == err )
        {
            json_object* poObj = reader.GetJSonObject();
            CheckExceededTransferLimit(poObj);
            reader.ReadLayers( this, nSrcType );
        }
        return;
    }

/* -------------------------------------------------------------------- */
/*      Is it TopoJSON data ?                                           */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszJSonFlavor, "TOPOJSON") )
    {
        OGRTopoJSONReader reader;
        if( nSrcType == eGeoJSONSourceFile )
        {
            if( !ReadFromFile( poOpenInfo, pszUnprefixed ) )
                return;
        }
        OGRErr err = reader.Parse( pszGeoData_,
            nSrcType == eGeoJSONSourceService &&
            !STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:") );
        if( OGRERR_NONE == err )
        {
            reader.ReadLayers( this );
        }
        return;
    }

    VSILFILE* fp = nullptr;
    if( nSrcType == eGeoJSONSourceFile &&
        !EQUAL(poOpenInfo->pszFilename, pszUnprefixed) )
    {
        GDALOpenInfo oOpenInfo(pszUnprefixed, GA_ReadOnly);
        if( oOpenInfo.fpL == nullptr || oOpenInfo.pabyHeader == nullptr )
            return;
        CPL_IGNORE_RET_VAL(oOpenInfo.TryToIngest(6000));
        CPLFree(pszGeoData_);
        pszGeoData_ = CPLStrdup(
                        reinterpret_cast<const char*>(oOpenInfo.pabyHeader));
        fp = oOpenInfo.fpL;
        oOpenInfo.fpL = nullptr;
    }

    if( !GeoJSONIsObject( pszGeoData_) )
    {
        CPLDebug( pszJSonFlavor,
                  "No valid %s data found in source '%s'",
                  pszJSonFlavor, pszName_ );
        if( fp )
            VSIFCloseL(fp);
        return;
    }

/* -------------------------------------------------------------------- */
/*      Configure GeoJSON format translator.                            */
/* -------------------------------------------------------------------- */
    OGRGeoJSONReader* poReader = new OGRGeoJSONReader();
    SetOptionsOnReader(poOpenInfo, poReader);

/* -------------------------------------------------------------------- */
/*      Parse GeoJSON and build valid OGRLayer instance.                */
/* -------------------------------------------------------------------- */
    bool bUseStreamingInterface = false;
    const GIntBig nMaxBytesFirstPass = CPLAtoGIntBig(
        CPLGetConfigOption("OGR_GEOJSON_MAX_BYTES_FIRST_PASS", "0"));
    if( (fp != nullptr || poOpenInfo->fpL != nullptr) &&
        (!STARTS_WITH(pszUnprefixed, "/vsistdin/") ||
         (nMaxBytesFirstPass > 0 && nMaxBytesFirstPass <= 1000000)) )
    {
        const char* pszStr = strstr( pszGeoData_, "\"features\"");
        if( pszStr )
        {
            pszStr += strlen("\"features\"");
            while( *pszStr && isspace(*pszStr) )
                pszStr ++;
            if( *pszStr == ':' )
            {
                pszStr ++;
                while( *pszStr && isspace(*pszStr) )
                    pszStr ++;
                if( *pszStr == '[' )
                {
                    bUseStreamingInterface = true;
                }
            }
        }
    }

    if( bUseStreamingInterface )
    {
        bool bTryStandardReading = false;
        if( poReader->FirstPassReadLayer( this, fp ? fp : poOpenInfo->fpL,
                                          bTryStandardReading ) )
        {
            if( fp )
                fp = nullptr;
            else
                poOpenInfo->fpL = nullptr;
            CheckExceededTransferLimit(poReader->GetJSonObject());
        }
        else
        {
            delete poReader;
        }
        if( !bTryStandardReading )
        {
            if( fp )
                VSIFCloseL(fp);
            return;
        }

        poReader = new OGRGeoJSONReader();
        SetOptionsOnReader(poOpenInfo, poReader);
    }

    if( fp )
        VSIFCloseL(fp);
    if( nSrcType == eGeoJSONSourceFile )
    {
        if( !ReadFromFile( poOpenInfo, pszUnprefixed ) )
        {
            delete poReader;
            return;
        }
        RemoveJSonPStuff();
    }
    const OGRErr err = poReader->Parse( pszGeoData_ );
    if( OGRERR_NONE == err )
    {
        CheckExceededTransferLimit(poReader->GetJSonObject());
    }

    poReader->ReadLayers( this );
    delete poReader;
}

/************************************************************************/
/*                          SetOptionsOnReader()                        */
/************************************************************************/

void OGRGeoJSONDataSource::SetOptionsOnReader(GDALOpenInfo* poOpenInfo,
                                              OGRGeoJSONReader* poReader)
{
    if( eGeometryAsCollection == flTransGeom_ )
    {
        poReader->SetPreserveGeometryType( false );
        CPLDebug( "GeoJSON", "Geometry as OGRGeometryCollection type." );
    }

    if( eAttributesSkip == flTransAttrs_ )
    {
        poReader->SetSkipAttributes( true );
        CPLDebug( "GeoJSON", "Skip all attributes." );
    }

    poReader->SetFlattenNestedAttributes(
        CPLFetchBool(poOpenInfo->papszOpenOptions, "FLATTEN_NESTED_ATTRIBUTES", false),
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                             "NESTED_ATTRIBUTE_SEPARATOR", "_")[0]);

    const bool bDefaultNativeData = bUpdatable_;
    poReader->SetStoreNativeData(
        CPLFetchBool(poOpenInfo->papszOpenOptions, "NATIVE_DATA", bDefaultNativeData));

    poReader->SetArrayAsString(
        CPLTestBool(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "ARRAY_AS_STRING",
                CPLGetConfigOption("OGR_GEOJSON_ARRAY_AS_STRING", "NO"))));

    poReader->SetDateAsString(
        CPLTestBool(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "DATE_AS_STRING",
                CPLGetConfigOption("OGR_GEOJSON_DATE_AS_STRING", "NO"))));
}

/************************************************************************/
/*                     CheckExceededTransferLimit()                     */
/************************************************************************/

void OGRGeoJSONDataSource::CheckExceededTransferLimit(json_object* poObj)
{
    for( int i = 0; i < 2; i ++ )
    {
        if( i == 1 )
        {
            if( poObj && json_object_get_type(poObj) == json_type_object )
            {
                poObj = CPL_json_object_object_get(poObj, "properties");
            }
        }
        if( poObj &&
            json_object_get_type(poObj) == json_type_object )
        {
            json_object* poExceededTransferLimit =
                CPL_json_object_object_get(poObj,
                                        "exceededTransferLimit");
            if( poExceededTransferLimit &&
                json_object_get_type(poExceededTransferLimit) ==
                    json_type_boolean )
            {
                bOtherPages_ = CPL_TO_BOOL(
                    json_object_get_boolean(poExceededTransferLimit) );
                return;
            }
        }
    }
}

/************************************************************************/
/*                            AddLayer()                                */
/************************************************************************/

void OGRGeoJSONDataSource::AddLayer( OGRGeoJSONLayer* poLayer )
{
    CPLAssert(papoLayersWriter_ == nullptr);

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

void OGRGeoJSONDataSource::FlushCache(bool /*bAtClosing*/)
{
    if( papoLayersWriter_ != nullptr )
        return;

    for( int i = 0; i < nLayers_; i++ )
    {
        if( papoLayers_[i]->HasBeenUpdated() )
        {
            papoLayers_[i]->SetUpdated(false);

            bool bOK = false;

            // Disable all filters.
            OGRFeatureQuery *poAttrQueryBak = papoLayers_[i]->m_poAttrQuery;
            papoLayers_[i]->m_poAttrQuery = nullptr;
            OGRGeometry* poFilterGeomBak = papoLayers_[i]->m_poFilterGeom;
            papoLayers_[i]->m_poFilterGeom = nullptr;

            // If the source data only contained one single feature and
            // that's still the case, then do not use a FeatureCollection
            // on writing.
            bool bAlreadyDone = false;
            if( papoLayers_[i]->GetFeatureCount(TRUE) == 1 &&
                papoLayers_[i]->GetMetadata("NATIVE_DATA") == nullptr )
            {
                papoLayers_[i]->ResetReading();
                OGRFeature* poFeature = papoLayers_[i]->GetNextFeature();
                if( poFeature != nullptr )
                {
                    if( poFeature->GetNativeData() != nullptr )
                    {
                        bAlreadyDone = true;
                        OGRGeoJSONWriteOptions oOptions;
                        json_object* poObj =
                            OGRGeoJSONWriteFeature(poFeature, oOptions);
                        VSILFILE* fp = VSIFOpenL(pszName_, "wb");
                        if( fp != nullptr )
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
                char** papszOptions = CSLAddString(nullptr, "-f");
                papszOptions = CSLAddString(papszOptions, "GeoJSON");
                GDALVectorTranslateOptions* psOptions =
                    GDALVectorTranslateOptionsNew(papszOptions, nullptr);
                CSLDestroy(papszOptions);
                GDALDatasetH hSrcDS = this;
                CPLString osNewFilename(pszName_);
                osNewFilename += ".tmp";
                GDALDatasetH hOutDS =
                    GDALVectorTranslate(osNewFilename, nullptr, 1, &hSrcDS,
                                        psOptions, nullptr);
                GDALVectorTranslateOptionsFree(psOptions);

                if( hOutDS != nullptr )
                {
                    CPLErrorReset();
                    GDALClose(hOutDS);
                    bOK = (CPLGetLastErrorType() == CE_None);
                }
                if( bOK )
                {
                    const bool bOverwrite =
                        CPLTestBool(CPLGetConfigOption("OGR_GEOJSON_REWRITE_IN_PLACE",
#ifdef WIN32
                                                       "YES"
#else
                                                       "NO"
#endif
                                                        ));
                    if( bOverwrite )
                    {
                        VSILFILE* fpTarget = nullptr;
                        for( int attempt = 0; attempt < 10; attempt++ )
                        {
                            fpTarget = VSIFOpenL(pszName_, "rb+");
                            if( fpTarget )
                                break;
                            CPLSleep(0.1);
                        }
                        if( !fpTarget )
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Cannot rewrite %s", pszName_);
                        }
                        else
                        {
                            const bool bCopyOK = CPL_TO_BOOL(
                                VSIOverwriteFile(fpTarget, osNewFilename));
                            VSIFCloseL(fpTarget);
                            if( bCopyOK )
                            {
                                VSIUnlink(osNewFilename);
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Cannot rewrite %s with content of %s",
                                         pszName_, osNewFilename.c_str());
                            }
                        }
                    }
                    else
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
            }

            // Restore filters.
            papoLayers_[i]->m_poAttrQuery = poAttrQueryBak;
            papoLayers_[i]->m_poFilterGeom = poFilterGeomBak;
        }
    }
}
