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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2006/10/12 16:27:23  hobu
 * Report more detailed driver capabilities
 *
 * Revision 1.1  2004/10/07 20:56:15  fwarmerdam
 * New
 *
 */

#include "ogr_mysql.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          ~OGRMySQLDriver()                           */
/************************************************************************/

OGRMySQLDriver::~OGRMySQLDriver()

{
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

    poDS = new OGRMySQLDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

#ifdef notdef
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
         "PostgreSQL driver doesn't currently support database creation.\n"
                  "Please create database with the `createdb' command." );
        return NULL;
    }

    return poDS;
}
#endif

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMySQLDriver::TestCapability( const char * pszCap )

{

    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    if( EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;
    if( EQUAL(pszCap,OLCRandomWrite) )
        return TRUE;
    if( EQUAL(pszCap,OLCCreateField) )
        return TRUE;        
    if( EQUAL(pszCap,ODsCDeleteLayer) )
        return TRUE;   
    if( EQUAL(pszCap,OLCDeleteFeature) )
        return TRUE;  

    return FALSE;
}

/************************************************************************/
/*                          RegisterOGRMySQL()                          */
/************************************************************************/

void RegisterOGRMySQL()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRMySQLDriver );
}

