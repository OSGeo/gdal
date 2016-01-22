/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements GeoPackageDriver.
 * Author:   Paul Ramsey <pramsey@boundlessgeo.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
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

#include "ogr_geopackage.h"

// g++ -g -Wall -fPIC -shared -o ogr_geopackage.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gpkg ogr/ogrsf_frmts/gpkg/*.c* -L. -lgdal 



/************************************************************************/
/*                        ~OGRGeoPackageDriver()                         */
/************************************************************************/

OGRGeoPackageDriver::~OGRGeoPackageDriver()
{
} 


/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRGeoPackageDriver::GetName()
{
    return "GPKG";
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRGeoPackageDriver::Open( const char * pszFilename, int bUpdate )
{
    OGRGeoPackageDataSource   *poDS = new OGRGeoPackageDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}


/************************************************************************/
/*                                CreateDataSource()                    */
/************************************************************************/

OGRDataSource *OGRGeoPackageDriver::CreateDataSource( const char * pszFilename, char **papszOptions )
{
	/* First, ensure there isn't any such file yet. */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszFilename, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "A file system object called '%s' already exists.",
                  pszFilename );

        return NULL;
    }
	
    OGRGeoPackageDataSource   *poDS = new OGRGeoPackageDataSource();

    if( !poDS->Create( pszFilename, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                         DeleteDataSource()                           */
/************************************************************************/

OGRErr OGRGeoPackageDriver::DeleteDataSource( const char *pszFilename )
{
    if (VSIUnlink( pszFilename ) == 0)
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRGeoPackageDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}


/************************************************************************/
/*                         RegisterOGRGeoPackage()                       */
/************************************************************************/

void RegisterOGRGeoPackage()
{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRGeoPackageDriver );
}


