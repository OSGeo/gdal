/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_mysql.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

static void* hMutex = NULL;
static int   bInitialized = FALSE;

/************************************************************************/
/*                          ~OGRMySQLDriver()                           */
/************************************************************************/

OGRMySQLDriver::~OGRMySQLDriver()

{
    if( bInitialized )
    {
        mysql_library_end();
        bInitialized = FALSE;
    }
    if( hMutex != NULL )
    {
        CPLDestroyMutex(hMutex);
        hMutex = NULL;
    }
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRMySQLDriver::GetName()

{
    return "MySQL";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRMySQLDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGRMySQLDataSource     *poDS;
 
    if( !EQUALN(pszFilename,"MYSQL:",6) )
        return NULL;
    {
        CPLMutexHolderD(&hMutex);
        if( !bInitialized )
        {
            if ( mysql_library_init( 0, NULL, NULL ) )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Could not initialize MySQL library" );
                return NULL;
            }
            bInitialized = TRUE;
        }
    }

    poDS = new OGRMySQLDataSource();

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

OGRDataSource *OGRMySQLDriver::CreateDataSource( const char * pszName,
                                              char ** /* papszOptions */ )

{
    OGRMySQLDataSource     *poDS;

    poDS = new OGRMySQLDataSource();


    if( !poDS->Open( pszName, TRUE, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
         "MySQL driver doesn't currently support database creation.\n"
                  "Please create database before using." );
        return NULL;
    }

    return poDS;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMySQLDriver::TestCapability( const char * pszCap )

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
/*                          RegisterOGRMySQL()                          */
/************************************************************************/

void RegisterOGRMySQL()

{
    if (! GDAL_CHECK_VERSION("MySQL driver"))
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRMySQLDriver );
}

