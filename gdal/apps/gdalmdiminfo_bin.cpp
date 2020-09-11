/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to list info about a multidimensional raster
 * Author:   Even Rouault,<even.rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal_version.h"
#include "gdal.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = nullptr)

{
    printf( "Usage: gdalmdiminfo [--help-general] [-oo NAME=VALUE]* [-arrayoption NAME=VALUE]*\n"
            "                    [-detailed] [-nopretty] [-array {array_name}] [-limit {number}]\n"
            "                    [-stats] datasetname\n" );

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                    GDALMultiDimInfoOptionsForBinary()                */
/************************************************************************/

static GDALMultiDimInfoOptionsForBinary *GDALMultiDimInfoOptionsForBinaryNew(void)
{
    return static_cast<GDALMultiDimInfoOptionsForBinary *>(
        CPLCalloc(1, sizeof(GDALMultiDimInfoOptionsForBinary)));
}

/************************************************************************/
/*                   GDALMultiDimInfoOptionsForBinaryFree()             */
/************************************************************************/

static void GDALMultiDimInfoOptionsForBinaryFree( GDALMultiDimInfoOptionsForBinary* psOptionsForBinary )
{
    if( psOptionsForBinary )
    {
        CPLFree(psOptionsForBinary->pszFilename);
        CSLDestroy(psOptionsForBinary->papszOpenOptions);
        CPLFree(psOptionsForBinary);
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    EarlySetConfigOptions(argc, argv);

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

    for( int i = 0; argv != nullptr && argv[i] != nullptr; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( argv );
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
        {
            Usage();
        }
    }
    argv = CSLAddString(argv, "-stdout");

    GDALMultiDimInfoOptionsForBinary* psOptionsForBinary = GDALMultiDimInfoOptionsForBinaryNew();

    GDALMultiDimInfoOptions *psOptions
        = GDALMultiDimInfoOptionsNew(argv + 1, psOptionsForBinary);
    if( psOptions == nullptr )
        Usage();

    if( psOptionsForBinary->pszFilename == nullptr )
        Usage("No datasource specified.");

    GDALDatasetH hDataset
        = GDALOpenEx( psOptionsForBinary->pszFilename, GDAL_OF_MULTIDIM_RASTER | GDAL_OF_VERBOSE_ERROR, nullptr,
                      psOptionsForBinary->papszOpenOptions, nullptr );
    if( !hDataset )
    {
        fprintf( stderr,
                 "gdalmdiminfo failed - unable to open '%s'.\n",
                 psOptionsForBinary->pszFilename );

        GDALMultiDimInfoOptionsForBinaryFree(psOptionsForBinary);

        GDALMultiDimInfoOptionsFree( psOptions );

        CSLDestroy( argv );

        GDALDumpOpenDatasets( stderr );

        GDALDestroyDriverManager();

        CPLDumpSharedList( nullptr );
        CPLCleanupTLS();
        exit(1);
    }

    char* pszGDALInfoOutput = GDALMultiDimInfo( hDataset, psOptions );

    if( pszGDALInfoOutput )
        printf( "%s", pszGDALInfoOutput );

    CPLFree( pszGDALInfoOutput );

    GDALClose(hDataset);

    GDALMultiDimInfoOptionsForBinaryFree(psOptionsForBinary);

    GDALMultiDimInfoOptionsFree( psOptions );

    CSLDestroy( argv );

    GDALDumpOpenDatasets( stderr );

    GDALDestroyDriverManager();

    CPLDumpSharedList( nullptr );
    CPLCleanupTLS();

    exit( 0 );
}
MAIN_END
