/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIngresDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_ingres.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          ~OGRIngresDriver()                           */
/************************************************************************/

OGRIngresDriver::~OGRIngresDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRIngresDriver::GetName()

{
    return "Ingres";
}

/************************************************************************/
/*                          ParseWrappedName()                          */
/************************************************************************/

char **OGRIngresDriver::ParseWrappedName( const char *pszEncodedName )

{
    if( pszEncodedName[0] != '@' )
        return NULL;

    return CSLTokenizeStringComplex( pszEncodedName+1, ",", TRUE, FALSE );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRIngresDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGRIngresDataSource *poDS = NULL;
    char **papszOptions = ParseWrappedName( pszFilename );
    const char *pszDriver = CSLFetchNameValue( papszOptions, "driver" );
    if( pszDriver != NULL && EQUAL(pszDriver,"ingres") )
    {
        poDS = new OGRIngresDataSource();

        if( !poDS->Open( pszFilename, papszOptions, TRUE ) )
        {
            delete poDS;
            poDS = NULL;
        }
    }

    CSLDestroy( papszOptions );

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRIngresDriver::CreateDataSource( const char * pszName,
                                                  char ** /* papszOptions */ )

{
    OGRIngresDataSource *poDS = NULL;

    char **papszOpenOptions = ParseWrappedName( pszName );

    const char *pszDriver = CSLFetchNameValue( papszOpenOptions, "driver" );

    if( pszDriver != NULL && EQUAL(pszDriver,"ingres") )
    {
        poDS = new OGRIngresDataSource();
        if( !poDS->Open( pszName, papszOpenOptions, TRUE ) )
        {
            delete poDS;
            poDS = NULL;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Ingres driver doesn't currently support database creation.\n"
                      "Please create database before using." );
        }
    }

    CSLDestroy( papszOpenOptions );

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIngresDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    if( EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                          RegisterOGRIngres()                          */
/************************************************************************/

void RegisterOGRIngres()

{
    if( !GDAL_CHECK_VERSION("Ingres") )
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(new OGRIngresDriver);
}
