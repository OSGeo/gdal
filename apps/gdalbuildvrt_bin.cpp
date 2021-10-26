/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to build VRT datasets from raster products or content of SHP tile index
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2007-2016, Even Rouault <even dot rouault at spatialys dot com>
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
#include "cpl_error.h"
#include "commonutils.h"
#include "gdal_version.h"
#include "gdal_utils_priv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = nullptr) CPL_NO_RETURN;

static void Usage(const char* pszErrorMsg)

{
    fprintf(stdout, "%s",
            "Usage: gdalbuildvrt [-tileindex field_name]\n"
            "                    [-resolution {highest|lowest|average|user}]\n"
            "                    [-te xmin ymin xmax ymax] [-tr xres yres] [-tap]\n"
            "                    [-separate] [-b band] [-sd subdataset]\n"
            "                    [-allow_projection_difference] [-q]\n"
            "                    [-addalpha] [-hidenodata]\n"
            "                    [-srcnodata \"value [value...]\"] [-vrtnodata \"value [value...]\"] \n"
            "                    [-ignore_srcmaskband]\n"
            "                    [-a_srs srs_def]\n"
            "                    [-r {nearest,bilinear,cubic,cubicspline,lanczos,average,mode}]\n"
            "                    [-oo NAME=VALUE]*\n"
            "                    [-input_file_list my_list.txt] [-overwrite] output.vrt [gdalfile]*\n"
            "\n"
            "e.g.\n"
            "  % gdalbuildvrt doq_index.vrt doq/*.tif\n"
            "  % gdalbuildvrt -input_file_list my_list.txt doq_index.vrt\n"
            "\n"
            "NOTES:\n"
            "  o With -separate, each files goes into a separate band in the VRT band.\n"
            "    Otherwise, the files are considered as tiles of a larger mosaic.\n"
            "  o -b option selects a band to add into vrt.  Multiple bands can be listed.\n"
            "    By default all bands are queried.\n"
            "  o The default tile index field is 'location' unless otherwise specified by\n"
            "    -tileindex.\n"
            "  o In case the resolution of all input files is not the same, the -resolution\n"
            "    flag enable the user to control the way the output resolution is computed.\n"
            "    Average is the default.\n"
            "  o Input files may be any valid GDAL dataset or a GDAL raster tile index.\n"
            "  o For a GDAL raster tile index, all entries will be added to the VRT.\n"
            "  o If one GDAL dataset is made of several subdatasets and has 0 raster bands,\n"
            "    its datasets will be added to the VRT rather than the dataset itself.\n"
            "    Single subdataset could be selected by its number using the -sd option.\n"
            "  o By default, only datasets of same projection and band characteristics\n"
            "    may be added to the VRT.\n"
            );

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                       GDALBuildVRTOptionsForBinaryNew()              */
/************************************************************************/

static GDALBuildVRTOptionsForBinary *GDALBuildVRTOptionsForBinaryNew(void)
{
    return static_cast<GDALBuildVRTOptionsForBinary *>(
        CPLCalloc(1, sizeof(GDALBuildVRTOptionsForBinary)));
}

/************************************************************************/
/*                       GDALBuildVRTOptionsForBinaryFree()            */
/************************************************************************/

static void GDALBuildVRTOptionsForBinaryFree( GDALBuildVRTOptionsForBinary* psOptionsForBinary )
{
    if( psOptionsForBinary )
    {
        CSLDestroy(psOptionsForBinary->papszSrcFiles);
        CPLFree(psOptionsForBinary->pszDstFilename);
        CPLFree(psOptionsForBinary);
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    EarlySetConfigOptions(argc, argv);

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and process generic GDAL        */
/*      command options.                                                */
/* -------------------------------------------------------------------- */
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
            Usage(nullptr);
        }
    }

    GDALBuildVRTOptionsForBinary* psOptionsForBinary = GDALBuildVRTOptionsForBinaryNew();
    /* coverity[tainted_data] */
    GDALBuildVRTOptions *psOptions = GDALBuildVRTOptionsNew(argv + 1, psOptionsForBinary);
    CSLDestroy( argv );

    if( psOptions == nullptr )
    {
        Usage(nullptr);
    }

    if( psOptionsForBinary->pszDstFilename == nullptr )
    {
        Usage("No target filename specified.");
    }

    if( !(psOptionsForBinary->bQuiet) )
    {
        GDALBuildVRTOptionsSetProgress(psOptions, GDALTermProgress, nullptr);
    }

    /* Avoid overwriting a non VRT dataset if the user did not put the */
    /* filenames in the right order */
    VSIStatBuf sBuf;
    if (!psOptionsForBinary->bOverwrite)
    {
        int bExists = (VSIStat(psOptionsForBinary->pszDstFilename, &sBuf) == 0);
        if (bExists)
        {
            GDALDriverH hDriver = GDALIdentifyDriver( psOptionsForBinary->pszDstFilename, nullptr );
            if (hDriver && !(EQUAL(GDALGetDriverShortName(hDriver), "VRT") ||
                   (EQUAL(GDALGetDriverShortName(hDriver), "API_PROXY") &&
                    EQUAL(CPLGetExtension(psOptionsForBinary->pszDstFilename), "VRT"))) )
            {
                fprintf(stderr,
                        "'%s' is an existing GDAL dataset managed by %s driver.\n"
                        "There is an high chance you did not put filenames in the right order.\n"
                        "If you want to overwrite %s, add -overwrite option to the command line.\n\n",
                        psOptionsForBinary->pszDstFilename, GDALGetDriverShortName(hDriver), psOptionsForBinary->pszDstFilename);
                Usage();
            }
        }
    }

    int bUsageError = FALSE;
    GDALDatasetH hOutDS = GDALBuildVRT(psOptionsForBinary->pszDstFilename,
                                       psOptionsForBinary->nSrcFiles,
                                       nullptr,
                                       psOptionsForBinary->papszSrcFiles,
                                       psOptions, &bUsageError);
    if( bUsageError )
        Usage();
    int nRetCode = (hOutDS) ? 0 : 1;

    GDALBuildVRTOptionsFree(psOptions);
    GDALBuildVRTOptionsForBinaryFree(psOptionsForBinary);

    CPLErrorReset();
    // The flush to disk is only done at that stage, so check if any error has
    // happened
    GDALClose( hOutDS );
    if( CPLGetLastErrorType() != CE_None )
        nRetCode = 1;

    GDALDumpOpenDatasets( stderr );

    GDALDestroyDriverManager();

    OGRCleanupAll();

    return nRetCode;
}
MAIN_END
