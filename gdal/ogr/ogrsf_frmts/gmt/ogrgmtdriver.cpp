/******************************************************************************
 * $Id: ogrmemdriver.cpp 10645 2007-01-18 02:22:39Z warmerdam $
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGmtDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gmt.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id: ogrmemdriver.cpp 10645 2007-01-18 02:22:39Z warmerdam $");

/************************************************************************/
/*                          ~OGRGmtDriver()                           */
/************************************************************************/

OGRGmtDriver::~OGRGmtDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRGmtDriver::GetName()

{
    return "OGR_GMT";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRGmtDriver::Open( const char * pszFilename, int bUpdate )

{
    if( !EQUAL(CPLGetExtension(pszFilename),"gmt") )
        return NULL;

    OGRGmtDataSource *poDS = new OGRGmtDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRGmtDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
    OGRGmtDataSource *poDS = new OGRGmtDataSource();

    if( poDS->Create( pszName, papszOptions ) )
        return poDS;
    else
    {
        delete poDS;
        return NULL;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGmtDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRGMT()                           */
/************************************************************************/

void RegisterOGRGMT()

{
    OGRSFDriver* poDriver = new OGRGmtDriver;
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                "GMT ASCII Vectors (.gmt)" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gmt" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                "drv_gmt.html" );
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(poDriver);
}

