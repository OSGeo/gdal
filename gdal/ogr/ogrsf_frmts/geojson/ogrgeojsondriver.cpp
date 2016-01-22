/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONDriver class (OGR GeoJSON Driver).
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
#include <cpl_conv.h>

/************************************************************************/
/*                           OGRGeoJSONDriver()                         */
/************************************************************************/

OGRGeoJSONDriver::OGRGeoJSONDriver()
{
}

/************************************************************************/
/*                          ~OGRGeoJSONDriver()                         */
/************************************************************************/

OGRGeoJSONDriver::~OGRGeoJSONDriver()
{
}

/************************************************************************/
/*                           GetName()                                  */
/************************************************************************/

const char* OGRGeoJSONDriver::GetName()
{
    return "GeoJSON";
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

OGRDataSource* OGRGeoJSONDriver::Open( const char* pszName, int bUpdate )
{
    return Open( pszName, bUpdate, NULL );
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

OGRDataSource* OGRGeoJSONDriver::Open( const char* pszName, int bUpdate,
                                       char** papszOptions )
{
    UNREFERENCED_PARAM(papszOptions);

    OGRGeoJSONDataSource* poDS = NULL;
    poDS = new OGRGeoJSONDataSource();

/* -------------------------------------------------------------------- */
/*      Processing configuration options.                               */
/* -------------------------------------------------------------------- */

    // TODO: Currently, options are based on environment variables.
    //       This is workaround for not yet implemented Andrey's concept
    //       described in document 'RFC 10: OGR Open Parameters'.

    poDS->SetGeometryTranslation( OGRGeoJSONDataSource::eGeometryPreserve );
    const char* pszOpt = CPLGetConfigOption("GEOMETRY_AS_COLLECTION", NULL);
    if( NULL != pszOpt && EQUALN(pszOpt, "YES", 3) )
    {
            poDS->SetGeometryTranslation(
                OGRGeoJSONDataSource::eGeometryAsCollection );
    }

    poDS->SetAttributesTranslation( OGRGeoJSONDataSource::eAtributesPreserve );
    pszOpt = CPLGetConfigOption("ATTRIBUTES_SKIP", NULL);
    if( NULL != pszOpt && EQUALN(pszOpt, "YES", 3) )
    {
        poDS->SetAttributesTranslation( 
            OGRGeoJSONDataSource::eAtributesSkip );
    }

/* -------------------------------------------------------------------- */
/*      Open and start processing GeoJSON datasoruce to OGR objects.    */
/* -------------------------------------------------------------------- */
    if( !poDS->Open( pszName ) )
    {
        delete poDS;
        poDS= NULL;
    }

    if( NULL != poDS && bUpdate )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "GeoJSON Driver doesn't support update." );
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           CreateDataSource()                         */
/************************************************************************/

OGRDataSource* OGRGeoJSONDriver::CreateDataSource( const char* pszName,
                                                   char** papszOptions )
{
    OGRGeoJSONDataSource* poDS = new OGRGeoJSONDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           DeleteDataSource()                         */
/************************************************************************/

OGRErr OGRGeoJSONDriver::DeleteDataSource( const char* pszName )
{
    if( VSIUnlink( pszName ) == 0 )
    {
        return OGRERR_NONE;
    }
    
    CPLDebug( "GeoJSON", "Failed to delete \'%s\'", pszName);

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONDriver::TestCapability( const char* pszCap )
{
    if( EQUAL( pszCap, ODrCCreateDataSource ) )
        return TRUE;
    else if( EQUAL(pszCap, ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRGeoJSON()                       */
/************************************************************************/

void RegisterOGRGeoJSON()
{
    if( GDAL_CHECK_VERSION("OGR/GeoJSON driver") )
    {
        OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( 
            new OGRGeoJSONDriver );
    }
}

