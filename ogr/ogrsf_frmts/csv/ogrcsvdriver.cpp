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
 ****************************************************************************/

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

    if( !poDS->Open( pszFilename, bUpdate, FALSE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRCSVDriver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if (strcmp(pszName, "/dev/stdout") == 0)
        pszName = "/vsistdout/";

    if( VSIStatL( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If the target is not a simple .csv then create it as a          */
/*      directory.                                                      */
/* -------------------------------------------------------------------- */
    CPLString osDirName;

    if( EQUAL(CPLGetExtension(pszName),"csv") )
    {
        osDirName = CPLGetPath(pszName);
        if( osDirName == "" )
            osDirName = ".";

        /* HACK: CPLGetPath("/vsimem/foo.csv") = "/vsimem", but this is not */
        /* recognized afterwards as a valid directory name */
        if( osDirName == "/vsimem" )
            osDirName = "/vsimem/";
    }
    else
    {
        if( strncmp(pszName, "/vsizip/", 8) == 0)
        {
            /* do nothing */
        }
        else if( !EQUAL(pszName, "/vsistdout/") &&
            VSIMkdir( pszName, 0755 ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to create directory %s:\n%s", 
                      pszName, VSIStrerror( errno ) );
            return NULL;
        }
        osDirName = pszName;
    }

/* -------------------------------------------------------------------- */
/*      Force it to open as a datasource.                               */
/* -------------------------------------------------------------------- */
    OGRCSVDataSource   *poDS = new OGRCSVDataSource();

    if( !poDS->Open( osDirName, TRUE, TRUE ) )
    {
        delete poDS;
        return NULL;
    }

    if( osDirName != pszName )
        poDS->SetDefaultCSVName( CPLGetFilename(pszName) );

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCSVDriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

OGRErr OGRCSVDriver::DeleteDataSource( const char *pszFilename )

{
    if( CPLUnlinkTree( pszFilename ) == 0 )
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           RegisterOGRCSV()                           */
/************************************************************************/

void RegisterOGRCSV()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRCSVDriver );
}

