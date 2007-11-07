/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONDataSource class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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
#include <cstdlib>

/************************************************************************/
/*                           OGRGeoJSONDataSource()                     */
/************************************************************************/

OGRGeoJSONDataSource::OGRGeoJSONDataSource()
    : pszName_(NULL), pszGeoData_(NULL), papoLayers_(NULL), nLayers_(0),
        flTransGeom_( OGRGeoJSONDataSource::eGeometryPreserve ),
        flTransAttrs_( OGRGeoJSONDataSource::eAtributesPreserve )
{
    // I've got constructed. Lunch time!
}

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

int OGRGeoJSONDataSource::Open( const char* pszName )
{
    CPLAssert( NULL != pszName );

/* -------------------------------------------------------------------- */
/*      Release resources allocated during previous request.            */
/* -------------------------------------------------------------------- */
    if( NULL != papoLayers_ )
    {
        CPLAssert( nLayers_ > 0 );
        Clear();
    }

/* -------------------------------------------------------------------- */
/*      Determine type of data source: text file (.geojson, .json),     */
/*      Web Service or text passed directly and load data.              */
/* -------------------------------------------------------------------- */
    GeoJSONSourceType nSrcType;
    
    nSrcType = GeoJSONGetSourceType( pszName );
    if( eGeoJSONSourceService == nSrcType )
    {
        if( !ReadFromService( pszName ) )
            return FALSE;
    }
    else if( eGeoJSONSourceText == nSrcType )
    {
        pszGeoData_ = CPLStrdup( pszName );
    }
    else if( eGeoJSONSourceFile == nSrcType )
    {
        if( !ReadFromFile( pszName ) )
            return FALSE;
    }
    else
    {
        CPLDebug( "GeoJSON",
                  "Unknown datasource type. Use .geojson file, text input or URL" );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Construct OGR layer and feature objects from                    */
/*      GeoJSON text tree.                                              */
/* -------------------------------------------------------------------- */
    if( NULL == pszGeoData_ )
    {
        Clear();
        return FALSE;
    }

    OGRGeoJSONLayer* poLayer = LoadLayer();
    if( NULL == poLayer )
    {
        Clear();
        return FALSE;
    }

    poLayer->DetectGeometryType();

/* -------------------------------------------------------------------- */
/*      NOTE: Currently, the driver generates only one layer per        */
/*      single GeoJSON file, input or service request.                  */
/* -------------------------------------------------------------------- */
    const int nLayerIndex = 0;
    nLayers_ = 1;
    
    papoLayers_ =
        (OGRGeoJSONLayer**)CPLMalloc( sizeof(OGRGeoJSONLayer*) * nLayers_ );
    papoLayers_[nLayerIndex] = poLayer; 

    CPLAssert( NULL != papoLayers_ );
    CPLAssert( nLayers_ > 0 );
    return TRUE;
}

/************************************************************************/
/*                           GetName()                                  */
/************************************************************************/

const char* OGRGeoJSONDataSource::GetName()
{
    return pszName_;
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
    if( 0 <= nLayer || nLayer < nLayers_ )
    {
        CPLAssert( NULL != papoLayers_[nLayer] );

        return papoLayers_[nLayer];
    }

    return NULL;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer* OGRGeoJSONDataSource::CreateLayer( const char* pszName_,
                                             OGRSpatialReference* poSRS,
                                             OGRwkbGeometryType eGType,
                                             char** papszOptions )
{
    CPLAssert( OGRGeoJSONLayer::DefaultGeometryType == eGType );

    OGRGeoJSONLayer* poLayer = NULL;
    poLayer = new OGRGeoJSONLayer( pszName_, poSRS, eGType, papszOptions );

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONDataSource::TestCapability( const char* pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                           SetGeometryTranslation()               */
/************************************************************************/

void
OGRGeoJSONDataSource::SetGeometryTranslation( GeometryTranslation type )
{
    flTransGeom_ = type;
}

/************************************************************************/
/*                           TestCapability()                           */
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
}

/************************************************************************/
/*                           ReadFromFile()                             */
/************************************************************************/

int OGRGeoJSONDataSource::ReadFromFile( const char* pszSource )
{
    CPLAssert( NULL == pszGeoData_ );

    if( NULL == pszSource )
    {
        CPLDebug( "GeoJSON", "Input file path is null" );
        return FALSE;
    }

    FILE* fp = NULL;

    fp = VSIFOpen( pszSource, "rb" );
    if( NULL == fp )
        return FALSE;

    std::size_t nDataLen = 0;

    VSIFSeek( fp, 0, SEEK_END );
    nDataLen = VSIFTell( fp );
    VSIFSeek( fp, 0, SEEK_SET );

    pszGeoData_ = (char*)CPLMalloc(nDataLen + 1);
    if( NULL == pszGeoData_ )
        return FALSE;

    pszGeoData_[nDataLen] = '\0';
    if( ( nDataLen != VSIFRead( pszGeoData_, 1, nDataLen, fp ) ) )
    {
        Clear();
        VSIFClose( fp );
        return FALSE;
    }
    VSIFClose( fp );

    pszName_ = CPLStrdup( pszSource );

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

    CPLHTTPResult* pResult = NULL;
    char* papsOptions[] = { "HEADERS=Accept: text/plain", NULL };

    pResult = CPLHTTPFetch( pszSource, papsOptions );

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

    // TODO: Eventually, CPLHTTPResult::pabyData could be assigned
    //       to pszGeoData_, so we will avoid copying of potentially (?) big data.
    pszGeoData_ = (char*)CPLMalloc( sizeof(char) * pResult->nDataLen + 1 );
    CPLAssert( NULL != pszGeoData_ );

    strncpy( pszGeoData_, pszData, pResult->nDataLen );
    pszGeoData_[pResult->nDataLen] = '\0';

    pszName_ = CPLStrdup( pszSource );

/* -------------------------------------------------------------------- */
/*      Cleanup HTTP resources.                                         */
/* -------------------------------------------------------------------- */
    CPLHTTPDestroyResult( pResult );

    CPLAssert( NULL != pszGeoData_ );
    return TRUE;
}

/************************************************************************/
/*                           LoadLayer()                          */
/************************************************************************/

OGRGeoJSONLayer* OGRGeoJSONDataSource::LoadLayer()
{
    if( NULL == pszGeoData_ )
    {
        CPLError( CE_Failure, CPLE_ObjectNull,
                  "GeoJSON", "GeoJSON data buffer empty" );
        return NULL;
    }

    OGRErr err = OGRERR_NONE;
    OGRGeoJSONLayer* poLayer = NULL;    
    
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
    
/* -------------------------------------------------------------------- */
/*      Parse GeoJSON and build valid OGRLayer instance.                */
/* -------------------------------------------------------------------- */
    err = reader.Parse( pszGeoData_ );
    if( OGRERR_NONE == err )
    {
        // TODO: Think about better name selection
        poLayer = reader.ReadLayer( OGRGeoJSONLayer::DefaultName );
    }

    return poLayer;
}