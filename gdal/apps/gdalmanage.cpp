/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Commandline utility for GDAL identify, delete, rename and copy 
 *           (by file) operations. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 2007, Frank Warmerdam
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

#include "gdal.h"
#include "cpl_string.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: gdalmanage identify [-r] [-u] files*\n"
            "    or gdalmanage copy [-f driver] oldname newname\n"
            "    or gdalmanage rename [-f driver] oldname newname\n"
            "    or gdalmanage delete [-f driver] datasetname\n" );
    exit( 1 );
}

/************************************************************************/
/*                       ProcessIdentifyTarget()                        */
/************************************************************************/

static void ProcessIdentifyTarget( const char *pszTarget, 
                                   char **papszSiblingList, 
                                   int bRecursive, int bReportFailures )

{
    GDALDriverH hDriver;
    VSIStatBufL sStatBuf;
    int i;

    hDriver = GDALIdentifyDriver( pszTarget, papszSiblingList );

    if( hDriver != NULL )
        printf( "%s: %s\n", pszTarget, GDALGetDriverShortName( hDriver ) );
    else if( bReportFailures )
        printf( "%s: unrecognised\n", pszTarget );

    if( !bRecursive || hDriver != NULL )
        return;

    if( VSIStatL( pszTarget, &sStatBuf ) != 0 
        || !VSI_ISDIR( sStatBuf.st_mode ) )
        return;

    papszSiblingList = VSIReadDir( pszTarget );
    for( i = 0; papszSiblingList && papszSiblingList[i]; i++ )
    {
        if( EQUAL(papszSiblingList[i],"..") 
            || EQUAL(papszSiblingList[i],".") )
            continue;

        CPLString osSubTarget = 
            CPLFormFilename( pszTarget, papszSiblingList[i], NULL );

        ProcessIdentifyTarget( osSubTarget, papszSiblingList, 
                               bRecursive, bReportFailures );
    }
    CSLDestroy(papszSiblingList);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static void Identify( int nArgc, char **papszArgv )

{
/* -------------------------------------------------------------------- */
/*      Scan for commandline switches                                   */
/* -------------------------------------------------------------------- */
    int bRecursive = FALSE, bReportFailures = FALSE;

    while( nArgc > 0 && papszArgv[0][0] == '-' )
    {
        if( EQUAL(papszArgv[0],"-r") )
            bRecursive = TRUE;
        else if( EQUAL(papszArgv[0],"-u") )
            bReportFailures = TRUE;
        else
            Usage();

        papszArgv++;
        nArgc--;
    }

/* -------------------------------------------------------------------- */
/*      Process given files.                                            */
/* -------------------------------------------------------------------- */
    while( nArgc > 0 )
    {
        ProcessIdentifyTarget( papszArgv[0], NULL, 
                               bRecursive, bReportFailures );
        nArgc--;
        papszArgv++;
    }
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static void Delete( GDALDriverH hDriver, int nArgc, char **papszArgv )

{
    if( nArgc != 1 )
        Usage();

    GDALDeleteDataset( hDriver, papszArgv[0] );
}

/************************************************************************/
/*                                Copy()                                */
/************************************************************************/

static void Copy( GDALDriverH hDriver, int nArgc, char **papszArgv,
                  const char *pszOperation )

{
    if( nArgc != 2 )
        Usage();

    if( EQUAL(pszOperation,"copy") )
        GDALCopyDatasetFiles( hDriver, papszArgv[1], papszArgv[0] );
    else
        GDALRenameDataset( hDriver, papszArgv[1], papszArgv[0] );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    char *pszDriver = NULL;
    GDALDriverH hDriver = NULL;

    /* Check that we are running against at least GDAL 1.5 */
    /* Note to developers : if we use newer API, please change the requirement */
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1500)
    {
        fprintf(stderr, "At least, GDAL >= 1.5.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n", argv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

    if( argc < 3 )
        Usage();

    if( EQUAL(argv[1], "--utility_version") )
    {
        printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
        return 0;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a driver specifier?                                  */
/* -------------------------------------------------------------------- */
    char **papszRemainingArgv = argv + 2;
    int  nRemainingArgc = argc - 2;

    if( EQUAL(papszRemainingArgv[0],"-f") && nRemainingArgc > 1 )
    {
        pszDriver = papszRemainingArgv[1];
        papszRemainingArgv += 2;
        nRemainingArgc -= 2;
    }

    if( pszDriver != NULL )
    {
        hDriver = GDALGetDriverByName( pszDriver );
        if( hDriver == NULL )
        {
            fprintf( stderr, "Unable to find driver named '%s'.\n",
                     pszDriver );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Split out based on operation.                                   */
/* -------------------------------------------------------------------- */
    if( EQUALN(argv[1],"identify",5) )
        Identify( nRemainingArgc, papszRemainingArgv );

    else if( EQUAL(argv[1],"copy") )
        Copy( hDriver, nRemainingArgc, papszRemainingArgv, "copy" );

    else if( EQUAL(argv[1],"rename") )
        Copy( hDriver, nRemainingArgc, papszRemainingArgv, "rename" );
    
    else if( EQUAL(argv[1],"delete") )
        Delete( hDriver, nRemainingArgc, papszRemainingArgv );

    else
        Usage();

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CSLDestroy( argv );
    GDALDestroyDriverManager();

    exit( 0 );
}

