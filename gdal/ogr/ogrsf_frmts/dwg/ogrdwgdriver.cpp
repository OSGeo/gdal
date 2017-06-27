/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Implements OGRDWGDriver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dwg.h"
#include "cpl_conv.h"
#include "ogrteigha.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRDWGDriver()                            */
/************************************************************************/

OGRDWGDriver::OGRDWGDriver() : poServices(NULL)

{
}

/************************************************************************/
/*                          ~OGRDWGDriver()                             */
/************************************************************************/

OGRDWGDriver::~OGRDWGDriver()

{
    OGRTEIGHADeinitialize();
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRDWGDriver::GetName()

{
    return "DWG";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRDWGDriver::Open( const char * pszFilename, int /*bUpdate*/ )

{
    if( !EQUAL(CPLGetExtension(pszFilename),"dwg") )
        return NULL;

    // Check that this is a real file since the driver doesn't support
    // VSI*L API
    VSIStatBuf sStat;
    if( VSIStat(pszFilename, &sStat) != 0 )
        return NULL;

    if( !OGRTEIGHAInitialize() )
        return NULL;

    OGRDWGDataSource   *poDS = new OGRDWGDataSource();

    if( !poDS->Open( OGRDWGGetServices(), pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDWGDriver::TestCapability( const char * /*pszCap*/ )

{
    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRDWG()                           */
/************************************************************************/

void RegisterOGRDWG()

{
    OGRSFDriver* poDriver = new OGRDWGDriver;
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                "AutoCAD DWG" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dwg" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                "drv_dwg.html" );
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( poDriver );
}
