/******************************************************************************
 *
 * Project:  WAsP Translator
 * Purpose:  Implements OGRWAsPDriver.
 * Author:   Vincent Mora, vincent dot mora at oslandia dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Oslandia <info at oslandia dot com>
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

#include "ogrwasp.h"
#include "cpl_conv.h"
#include <cassert>

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRWAsPDriver::Open( const char * pszFilename, int bUpdate )

{
    if (bUpdate)
    {
        return nullptr;
    }

    if (!EQUAL(CPLGetExtension(pszFilename), "map"))
    {
        return nullptr;
    }

    VSILFILE * fh = VSIFOpenL( pszFilename, "r" );
    if ( !fh )
    {
        /*CPLError( CE_Failure, CPLE_FileIO, "cannot open file %s", pszFilename );*/
        return nullptr;
    }
    auto pDataSource = cpl::make_unique<OGRWAsPDataSource>( pszFilename, fh );

    if ( pDataSource->Load(true) != OGRERR_NONE )
    {
        return nullptr;
    }
    return pDataSource.release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWAsPDriver::TestCapability( const char * pszCap )

{
    return EQUAL(pszCap,ODrCCreateDataSource)
        || EQUAL(pszCap,ODrCDeleteDataSource);
}

/************************************************************************/
/*                           CreateDataSource()                           */
/************************************************************************/

OGRDataSource * OGRWAsPDriver::CreateDataSource( const char *pszName, char ** )

{
    VSILFILE * fh = VSIFOpenL( pszName, "w" );
    if ( !fh )
    {
        CPLError( CE_Failure, CPLE_FileIO, "cannot open file %s", pszName );
        return nullptr;
    }
    return new OGRWAsPDataSource( pszName, fh );
}

/************************************************************************/
/*                           DeleteDataSource()                         */
/************************************************************************/

OGRErr OGRWAsPDriver::DeleteDataSource (const char *pszName)

{
    return VSIUnlink( pszName ) == 0 ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                           RegisterOGRWAsP()                           */
/************************************************************************/

void RegisterOGRWAsP()

{
    OGRSFDriver* poDriver = new OGRWAsPDriver;

    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "WAsP .map format" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "map" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/wasp.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(poDriver);
}
