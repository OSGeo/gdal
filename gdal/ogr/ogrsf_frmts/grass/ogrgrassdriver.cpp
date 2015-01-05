/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGRASSDriver class.
 * Author:   Radim Blazek, radim.blazek@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Radim Blazek <radim.blazek@gmail.com>
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

#include "ogrgrass.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          ~OGRGRASSDriver()                           */
/************************************************************************/
OGRGRASSDriver::~OGRGRASSDriver()
{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/
const char *OGRGRASSDriver::GetName()
{
    return "OGR_GRASS";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
OGRDataSource *OGRGRASSDriver::Open( const char * pszFilename,
                                     int bUpdate )
{
    OGRGRASSDataSource  *poDS;

    poDS = new OGRGRASSDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE ) )
    {
        delete poDS;
        return NULL;
    }
    else
    {
        return poDS;
    }
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/
OGRDataSource *OGRGRASSDriver::CreateDataSource( const char * pszName,
                                                 char **papszOptions )
{
    CPLError( CE_Failure, CPLE_AppDefined, 
	      "CreateDataSource is not supported by GRASS driver.\n" );
            
    return NULL;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/
OGRErr OGRGRASSDriver::DeleteDataSource( const char *pszDataSource )
{
    CPLError( CE_Failure, CPLE_AppDefined,
	      "DeleteDataSource is not supported by GRASS driver" );

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/
int OGRGRASSDriver::TestCapability( const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                          RegisterOGRGRASS()                          */
/************************************************************************/
void RegisterOGRGRASS()
{
    OGRGRASSDriver	*poDriver;

    if (! GDAL_CHECK_VERSION("OGR/GRASS driver"))
        return;

    if( GDALGetDriverByName( "OGR_GRASS" ) == NULL )
    {
        poDriver = new OGRGRASSDriver();
        
        poDriver->SetDescription( "GRASS" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "GRASS Vectors (5.7+)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "drv_grass.html" );

        OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( poDriver );
    }
}

