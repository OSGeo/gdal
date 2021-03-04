/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRWalkDriver class.
 * Author:   Xian Chen, chenxian at walkinfo.com.cn
 *
 ******************************************************************************
 * Copyright (c) 2013,  ZJU Walkinfo Technology Corp., Ltd.
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogrwalk.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          ~OGRWalkDriver()                            */
/************************************************************************/

OGRWalkDriver::~OGRWalkDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRWalkDriver::GetName()

{
    return "Walk";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRWalkDriver::Open( const char * pszFilename, int bUpdate )
{

    if( STARTS_WITH_CI(pszFilename, "PGEO:") )
        return nullptr;

    if( STARTS_WITH_CI(pszFilename, "GEOMEDIA:") )
        return nullptr;

    if( !STARTS_WITH_CI(pszFilename, "WALK:")
        && !EQUAL(CPLGetExtension(pszFilename), "MDB") )
        return nullptr;

#ifndef WIN32
    // Try to register MDB Tools driver
    //
    // ODBCINST.INI NOTE:
    // This operation requires write access to odbcinst.ini file
    // located in directory pointed by ODBCINISYS variable.
    // Usually, it points to /etc, so non-root users can overwrite this
    // setting ODBCINISYS with location they have write access to, e.g.:
    // $ export ODBCINISYS=$HOME/etc
    // $ touch $ODBCINISYS/odbcinst.ini
    //
    // See: http://www.unixodbc.org/internals.html
    //
    if ( !InstallMdbDriver() )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unable to install MDB driver for ODBC, MDB access may not supported.\n" );
    }
    else
        CPLDebug( "Walk", "MDB Tools driver installed successfully!");

#endif /* ndef WIN32 */

    OGRWalkDataSource  *poDS = new OGRWalkDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
    {
        delete poDS;
        return nullptr;
    }

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("WALK") )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRWalkDriver::CreateDataSource( const char * pszName,
                                                CPL_UNUSED char **papszOptions )
{
    //if( !EQUAL(CPLGetExtension(pszName), "MDB") )
    //    return NULL;

    OGRWalkDataSource  *poDS = new OGRWalkDataSource();

    if( !poDS->Open( pszName, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
         "Walk driver doesn't currently support database creation.\n"
                  "Please create database with the `createdb' command." );
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWalkDriver::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                          RegisterOGRWalk()                           */
/************************************************************************/

void RegisterOGRWalk()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRWalkDriver );
}
