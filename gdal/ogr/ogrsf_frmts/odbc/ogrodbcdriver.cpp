/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_odbc.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ~OGRODBCDriver()                            */
/************************************************************************/

OGRODBCDriver::~OGRODBCDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRODBCDriver::GetName()

{
    return "ODBC";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRODBCDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGRODBCDataSource     *poDS;

    if( !EQUALN(pszFilename,"ODBC:",5) 
#ifdef WIN32
        && !EQUAL(CPLGetExtension(pszFilename), "MDB")
#endif
        )
        return NULL;

    poDS = new OGRODBCDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE ) )
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

OGRDataSource *OGRODBCDriver::CreateDataSource( const char * pszName,
                                              char ** /* papszOptions */ )

{
    OGRODBCDataSource     *poDS;

    if( !EQUALN(pszName,"ODBC:",5) )
        return NULL;

    poDS = new OGRODBCDataSource();


    if( !poDS->Open( pszName, TRUE, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
         "ODBC driver doesn't currently support database creation.\n"
                  "Please create database with the `createdb' command." );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODBCDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRODBC()                            */
/************************************************************************/

void RegisterOGRODBC()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRODBCDriver );
}

