/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Multithreading test application.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
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
#include "gdal_alg.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static int nThreadCount = 4, nIterations = 1, bLockOnOpen = TRUE;
static int nOpenIterations = 1;
static volatile int nPendingThreads = 0;
static const char *pszFilename = NULL;
static int nChecksum = 0;

static void *pGlobalMutex = NULL;

static void WorkerFunc( void * );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()
{
    printf( "multireadtest [-nlo] [-t <thread#>]\n"
            "              [-i <iterations>] [-oi <iterations>\n"
            "              filename\n" );
    exit( 1 );
}


/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    int iArg;

/* -------------------------------------------------------------------- */
/*      Process arguments.                                              */
/* -------------------------------------------------------------------- */
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

    for( iArg = 1; iArg < argc; iArg++ )
    {
        if( EQUAL(argv[iArg],"-i") && iArg < argc-1 )
            nIterations = atoi(argv[++iArg]);
        else if( EQUAL(argv[iArg],"-oi") && iArg < argc-1 )
            nOpenIterations = atoi(argv[++iArg]);
        else if( EQUAL(argv[iArg],"-t") && iArg < argc-1 )
            nThreadCount = atoi(argv[++iArg]);
        else if( EQUAL(argv[iArg],"-nlo") )
            bLockOnOpen = FALSE;
        else if( pszFilename == NULL )
            pszFilename = argv[iArg];
        else
        {
            printf( "Unrecognised argument: %s\n", argv[iArg] );
            Usage();
        }
    }

    if( pszFilename == NULL )
    {
        printf( "Need a file to operate on.\n" );
        Usage();
        exit( 1 );
    }

    if( nOpenIterations > 0 )
        bLockOnOpen = FALSE;

/* -------------------------------------------------------------------- */
/*      Get the checksum of band1.                                      */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS;

    GDALAllRegister();
    hDS = GDALOpen( pszFilename, GA_ReadOnly );
    if( hDS == NULL )
        exit( 1 );

    nChecksum = GDALChecksumImage( GDALGetRasterBand( hDS, 1 ), 
                                   0, 0, 
                                   GDALGetRasterXSize( hDS ), 
                                   GDALGetRasterYSize( hDS ) );
    
    GDALClose( hDS );

    printf( "Got checksum %d, launching %d worker threads on %s, %d iterations.\n", 
            nChecksum, nThreadCount, pszFilename, nIterations );

/* -------------------------------------------------------------------- */
/*      Fire off worker threads.                                        */
/* -------------------------------------------------------------------- */
    int iThread;

    pGlobalMutex = CPLCreateMutex();
    CPLReleaseMutex( pGlobalMutex );

    nPendingThreads = nThreadCount;

    for( iThread = 0; iThread < nThreadCount; iThread++ )
    {
        if( CPLCreateThread( WorkerFunc, NULL ) == -1 )
        {
            printf( "CPLCreateThread() failed.\n" );
            exit( 1 );
        }
    }

    while( nPendingThreads > 0 )
        CPLSleep( 0.5 );

    CPLReleaseMutex( pGlobalMutex );

    printf( "All threads complete.\n" );
    
    CSLDestroy( argv );
    
    GDALDestroyDriverManager();
    
    return 0;
}


/************************************************************************/
/*                             WorkerFunc()                             */
/************************************************************************/

static void WorkerFunc( void * )

{
    GDALDatasetH hDS;
    int iIter, iOpenIter;

    for( iOpenIter = 0; iOpenIter < nOpenIterations; iOpenIter++ )
    {
        if( bLockOnOpen )
            CPLAcquireMutex( pGlobalMutex, 100.0 );

        hDS = GDALOpen( pszFilename, GA_ReadOnly );

        if( bLockOnOpen )
            CPLReleaseMutex( pGlobalMutex );

        for( iIter = 0; iIter < nIterations && hDS != NULL; iIter++ )
        {
            int nMyChecksum;
        
            nMyChecksum = GDALChecksumImage( GDALGetRasterBand( hDS, 1 ), 
                                             0, 0, 
                                             GDALGetRasterXSize( hDS ), 
                                             GDALGetRasterYSize( hDS ) );

            if( nMyChecksum != nChecksum )
            {
                printf( "Checksum ERROR in worker thread!\n" );
                break;
            }
        }

        if( hDS )
        {
            if( bLockOnOpen )
                CPLAcquireMutex( pGlobalMutex, 100.0 );
            GDALClose( hDS );
            if( bLockOnOpen )
                CPLReleaseMutex( pGlobalMutex );
        }
    }

    CPLAcquireMutex( pGlobalMutex, 100.0 );
    nPendingThreads--;
    CPLReleaseMutex( pGlobalMutex );
}
