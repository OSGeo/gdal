/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Commandline application to build overviews. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#include "gdal_priv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: gdaladdo [-r {nearest,average,average_mp,average_magphase,mode}]\n"
            "                [--help-general] filename levels\n"
            "\n"
            "Example:\n"
            " %% gdaladdo -r average abc.tif 2 4 8 16\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    GDALDataset *poDataset;
    const char * pszResampling = "nearest";
    const char * pszFilename = NULL;
    int          anLevels[1024];
    int          nLevelCount = 0;
    int          nResultStatus = 0;

    GDALAllRegister();

    nArgc = GDALGeneralCmdLineProcessor( nArgc, &papszArgv, 0 );
    if( nArgc < 1 )
        exit( -nArgc );

/* -------------------------------------------------------------------- */
/*      Parse commandline.                                              */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-r") && iArg < nArgc-1 )
            pszResampling = papszArgv[++iArg];
        else if( pszFilename == NULL )
            pszFilename = papszArgv[iArg];
        else if( atoi(papszArgv[iArg]) > 0 )
            anLevels[nLevelCount++] = atoi(papszArgv[iArg]);
        else
            Usage();
    }

    if( pszFilename == NULL || nLevelCount == 0 )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open data file.                                                 */
/* -------------------------------------------------------------------- */
    poDataset = (GDALDataset *) GDALOpen( pszFilename, GA_Update );

    if( poDataset == NULL )
        poDataset = (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );

    if( poDataset == NULL )
        exit( 2 );

/* -------------------------------------------------------------------- */
/*      Generate overviews.                                             */
/* -------------------------------------------------------------------- */
    if( poDataset->BuildOverviews( pszResampling, 
                                   nLevelCount, anLevels, 0, NULL,
                                   GDALTermProgress, NULL ) != CE_None )
    {
        printf( "Overview building failed.\n" );
        nResultStatus = 100;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    delete poDataset;

    CSLDestroy( papszArgv );
    GDALDestroyDriverManager();

    return nResultStatus;
}
