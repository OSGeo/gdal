/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVDriver.
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
 * Revision 1.2  2004/07/20 20:53:26  warmerda
 * added support for reading directories of CSV files
 *
 * Revision 1.1  2004/07/20 19:18:23  warmerda
 * New
 *
 */

#include "ogr_csv.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           ~OGRCSVDriver()                            */
/************************************************************************/

OGRCSVDriver::~OGRCSVDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRCSVDriver::GetName()

{
    return "CSV";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRCSVDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRCSVDataSource   *poDS = new OGRCSVDataSource();

    if( !poDS->Open( pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    if( poDS != NULL && bUpdate )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "CSV Driver doesn't support update." );
        delete poDS;
        poDS = NULL;
    }
    
    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRCSVDriver::CreateDataSource( const char * pszName,
                                               char ** /* papszOptions */ )

{
/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBuf sStatBuf;

    if( VSIStat( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return NULL;
    }

#ifdef notdef
/* -------------------------------------------------------------------- */
/*      Create the datasource.                                          */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource     *poDS;

    poDS = new OGRSQLiteDataSource();

    if( !poDS->Open( pszName ) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
#else
    return NULL;
#endif
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCSVDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRCSV()                          */
/************************************************************************/

void RegisterOGRCSV()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRCSVDriver );
}

