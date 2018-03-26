/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Rasterize OGR shapes into a GDAL raster.
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_string.h"
#include "gdal_version.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = nullptr)

{
    printf(
        "Usage: gdal_rasterize [-b band]* [-i] [-at]\n"
        "       {[-burn value]* | [-a attribute_name] | [-3d]} [-add]\n"
        "       [-l layername]* [-where expression] [-sql select_statement]\n"
        "       [-dialect dialect] [-of format] [-a_srs srs_def] [-to \"NAME=VALUE\"]*\n"
        "       [-co \"NAME=VALUE\"]* [-a_nodata value] [-init value]*\n"
        "       [-te xmin ymin xmax ymax] [-tr xres yres] [-tap] [-ts width height]\n"
        "       [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
        "             CInt16/CInt32/CFloat32/CFloat64}] [-optim {[AUTO]/VECTOR/RASTER}] [-q]\n"
        "       <src_datasource> <dst_filename>\n" );

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);
    exit(1);
}

/************************************************************************/
/*                       GDALRasterizeOptionsForBinaryNew()             */
/************************************************************************/

static GDALRasterizeOptionsForBinary *GDALRasterizeOptionsForBinaryNew(void)
{
    return static_cast<GDALRasterizeOptionsForBinary *>(
        CPLCalloc(1, sizeof(GDALRasterizeOptionsForBinary)));
}

/************************************************************************/
/*                       GDALRasterizeOptionsForBinaryFree()            */
/************************************************************************/

static void GDALRasterizeOptionsForBinaryFree(
    GDALRasterizeOptionsForBinary* psOptionsForBinary )
{
    if( psOptionsForBinary == nullptr )
        return;

    CPLFree(psOptionsForBinary->pszSource);
    CPLFree(psOptionsForBinary->pszDest);
    CPLFree(psOptionsForBinary);
}
/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)
{
    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    EarlySetConfigOptions(argc, argv);

/* -------------------------------------------------------------------- */
/*      Generic arg processing.                                         */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if( argc < 1 )
        exit( -argc );

    for( int i = 0; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy(argv);
            return 0;
        }
        else if( EQUAL(argv[i], "--help") )
        {
            Usage();
        }
    }

    GDALRasterizeOptionsForBinary* psOptionsForBinary =
        GDALRasterizeOptionsForBinaryNew();
    // coverity[tainted_data]
    GDALRasterizeOptions *psOptions =
        GDALRasterizeOptionsNew(argv + 1, psOptionsForBinary);
    CSLDestroy(argv);

    if( psOptions == nullptr )
    {
        Usage();
    }

    if( !(psOptionsForBinary->bQuiet) )
    {
        GDALRasterizeOptionsSetProgress(psOptions, GDALTermProgress, nullptr);
    }

    if( psOptionsForBinary->pszSource == nullptr )
        Usage("No input file specified.");

    if( psOptionsForBinary->pszDest == nullptr )
        Usage("No output file specified.");

/* -------------------------------------------------------------------- */
/*      Open input file.                                                */
/* -------------------------------------------------------------------- */
    GDALDatasetH hInDS = GDALOpenEx(
        psOptionsForBinary->pszSource, GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
        nullptr, nullptr, nullptr);

    if( hInDS == nullptr )
        exit(1);

/* -------------------------------------------------------------------- */
/*      Open output file if it exists.                                  */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS = nullptr;
    if( !(psOptionsForBinary->bCreateOutput) )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hDstDS = GDALOpenEx(
            psOptionsForBinary->pszDest,
            GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR | GDAL_OF_UPDATE,
            nullptr, nullptr, nullptr );
        CPLPopErrorHandler();
    }

    if( psOptionsForBinary->pszFormat != nullptr &&
        (psOptionsForBinary->bCreateOutput || hDstDS == nullptr) )
    {
        GDALDriverManager *poDM = GetGDALDriverManager();
        GDALDriver *poDriver =
            poDM->GetDriverByName(psOptionsForBinary->pszFormat);
        char** papszDriverMD = (poDriver) ? poDriver->GetMetadata(): nullptr;
        if( poDriver == nullptr ||
            !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_RASTER,
                                              "FALSE")) ||
            !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE,
                                              "FALSE")) )
        {
            fprintf(stderr,
                    "Output driver `%s' not recognised or does not support "
                    "direct output file creation.\n",
                    psOptionsForBinary->pszFormat);
            fprintf(stderr,
                    "The following format drivers are configured and "
                    "support direct output:\n" );

            for( int iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++ )
            {
                GDALDriver* poIter = poDM->GetDriver(iDriver);
                papszDriverMD = poIter->GetMetadata();
                if( CPLTestBool(
                        CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_RASTER,
                                             "FALSE")) &&
                    CPLTestBool(
                        CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE,
                                             "FALSE")) )
                {
                    fprintf(stderr,  "  -> `%s'\n", poIter->GetDescription());
                }
            }
            exit(1);
        }
    }

    int bUsageError = FALSE;
    GDALDatasetH hRetDS = GDALRasterize(psOptionsForBinary->pszDest,
                                        hDstDS,
                                        hInDS,
                                        psOptions, &bUsageError);
    if(bUsageError == TRUE)
        Usage();
    const int nRetCode = hRetDS ? 0 : 1;

    GDALClose(hInDS);
    GDALClose(hRetDS);
    GDALRasterizeOptionsFree(psOptions);
    GDALRasterizeOptionsForBinaryFree(psOptionsForBinary);

    GDALDestroyDriverManager();

    return nRetCode;
}
MAIN_END
