/******************************************************************************
 * $Id$
 *
 * Project:  OGDI Bridge
 * Purpose:  Implements OGROGDIDriver class.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *           (Based on some code contributed by Frank Warmerdam :)
 *
 ******************************************************************************
 * Copyright (c) 2000, Daniel Morissette
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

#include "ogrogdi.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           ~OGROGDIDriver()                           */
/************************************************************************/

OGROGDIDriver::~OGROGDIDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGROGDIDriver::GetName()

{
    return "OGR_OGDI";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGROGDIDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGROGDIDataSource   *poDS;

    if( !EQUALN(pszFilename,"gltp:",5) )
        return FALSE;

    poDS = new OGROGDIDataSource();

    if( !poDS->Open( pszFilename, TRUE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    if ( poDS != NULL && bUpdate )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "OGDI Driver doesn't support update." );
        delete poDS;
        poDS = NULL;
    }
 
    return poDS;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROGDIDriver::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                          RegisterOGROGDI()                           */
/************************************************************************/

void RegisterOGROGDI()

{
    if (! GDAL_CHECK_VERSION("OGR/OGDI driver"))
        return;
    OGRSFDriver* poDriver = new OGROGDIDriver;
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                "OGDI Vectors (VPF, VMAP, DCW)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                "drv_ogdi.html" );
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(poDriver);
}

