/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCIDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.1  2002/12/28 04:07:27  warmerda
 * New
 *
 */

#include "ogr_oci.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           ~OGROCIDriver()                            */
/************************************************************************/

OGROCIDriver::~OGROCIDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGROCIDriver::GetName()

{
    return "OCI";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGROCIDriver::Open( const char * pszFilename,
                                     int bUpdate )

{
    OGROCIDataSource	*poDS;

    poDS = new OGROCIDataSource();

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

OGRDataSource *OGROCIDriver::CreateDataSource( const char * pszName,
                                               char ** /* papszOptions */ )

{
#ifdef notdef 
    OGROCIDataSource	*poDS;

    poDS = new OGROCIDataSource();


    if( !poDS->Open( pszName, TRUE, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
         "PostgreSQL driver doesn't currently support database creation.\n"
                  "Please create database with the `createdb' command." );
        return NULL;
    }

    return poDS;
#else
    CPLError( CE_Failure, CPLE_AppDefined, 
           "ORACLE OCI Driver does not currently support database creation.");
    return NULL;
#endif
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROCIDriver::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                           RegisterOGROCI()                            */
/************************************************************************/

void RegisterOGROCI()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGROCIDriver );
}

