/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBDriver class.
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
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

#include "ogr_idb.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            ~OGRIDBDriver()                            */
/************************************************************************/

OGRIDBDriver::~OGRIDBDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRIDBDriver::GetName()

{
    return "IDB";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRIDBDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGRIDBDataSource     *poDS;

    if( !STARTS_WITH_CI(pszFilename, "IDB:") )
        return nullptr;

    poDS = new OGRIDBDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE ) )
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRIDBDriver::CreateDataSource( const char * pszName,
                                              char ** /* papszOptions */ )

{
    OGRIDBDataSource     *poDS;

    if( !STARTS_WITH_CI(pszName, "IDB:") )
        return nullptr;

    poDS = new OGRIDBDataSource();

    if( !poDS->Open( pszName, TRUE, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
         "IDB driver doesn't currently support database creation.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIDBDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRIDB()                            */
/************************************************************************/

void RegisterOGRIDB()

{
    if( !GDAL_CHECK_VERSION("IDB driver") )
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRIDBDriver );
}
