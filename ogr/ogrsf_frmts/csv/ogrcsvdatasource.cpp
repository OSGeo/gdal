/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVDataSource class
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
#include "cpl_string.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::OGRCSVDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
}

/************************************************************************/
/*                         ~OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::~OGRCSVDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCSVDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRCSVDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCSVDataSource::Open( const char * pszFilename )

{
    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Determine what sort of object this is.                          */
/* -------------------------------------------------------------------- */
    VSIStatBuf sStatBuf;

    if( VSIStat( pszFilename, &sStatBuf ) != 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Is this a single CSV file?                                      */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(sStatBuf.st_mode)
        && EQUAL(pszFilename+strlen(pszFilename)-4,".csv") )
        return OpenTable( pszFilename );

/* -------------------------------------------------------------------- */
/*      Otherwise it has to be a directory.                             */
/* -------------------------------------------------------------------- */
    if( !VSI_ISDIR(sStatBuf.st_mode) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Scan through for entries ending in .csv.                        */
/* -------------------------------------------------------------------- */
    int nNotCSVCount = 0, i;
    char **papszNames = CPLReadDir( pszFilename );

    for( i = 0; papszNames != NULL && papszNames[i] != NULL; i++ )
    {
        const char *pszSubFilename = 
            CPLFormFilename( pszFilename, papszNames[i], NULL );

        if( EQUAL(papszNames[i],".") || EQUAL(papszNames[i],"..") )
            continue;

        if( VSIStat( pszSubFilename, &sStatBuf ) != 0 
            || !VSI_ISREG(sStatBuf.st_mode) 
            || !EQUAL(pszSubFilename+strlen(pszSubFilename)-4,".csv") )
        {
            nNotCSVCount++;
            continue;
        }

        if( !OpenTable( pszSubFilename ) )
        {
            nNotCSVCount++;
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      We presume that this is indeed intended to be a CSV             */
/*      datasource if over half the files were .csv files.              */
/* -------------------------------------------------------------------- */
    return nNotCSVCount < nLayers;
}

/************************************************************************/
/*                              OpenTable()                             */
/************************************************************************/

int OGRCSVDataSource::OpenTable( const char * pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    FILE       * fp;

    fp = VSIFOpen( pszFilename, "r" );
    if( fp == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Read and parse a line.  Did we get multiple fields?             */
/* -------------------------------------------------------------------- */
    char **papszFields = CSVReadParseLine( fp );
						
    if( CSLCount(papszFields) < 2 )
    {
        VSIFClose( fp );
        CSLDestroy( papszFields );
        return FALSE;
    }

    VSIRewind( fp );
    CSLDestroy( papszFields );

/* -------------------------------------------------------------------- */
/*      Create a layer.                                                 */
/* -------------------------------------------------------------------- */
    nLayers++;
    papoLayers = (OGRCSVLayer **) CPLRealloc(papoLayers, 
                                             sizeof(void*) * nLayers);
    
    papoLayers[nLayers-1] = new OGRCSVLayer( CPLGetBasename(pszFilename), fp );

    return TRUE;
}
