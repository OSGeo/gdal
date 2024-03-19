/******************************************************************************
 *
 * Project:  MapServer
 * Purpose:  Commandline App to build tile index for raster files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam, DM Solutions Group Inc
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(bool bIsError, const char *pszErrorMsg)

{
    fprintf(
        bIsError ? stderr : stdout, "%s",
        "Usage: gdaltindex [--help] [--help-general]\n"
        "                  [-overwrite] [-recursive] [-filename_filter "
        "<val>]...\n"
        "                  [-min_pixel_size <val>] [-max_pixel_size <val>]\n"
        "                  [-f <format>] [-tileindex <field_name>] "
        "[-write_absolute_path] \n"
        "                  [-skip_different_projection] [-t_srs <target_srs>]\n"
        "                  [-src_srs_name field_name] [-src_srs_format "
        "{AUTO|WKT|EPSG|PROJ}]\n"
        "                  [-lyr_name <name>] [-lco <KEY>=<VALUE>]...\n"
        "                  [-gti_filename <name>]\n"
        "                  [-tr <xres> <yres>] [-te <xmin> <ymin> <xmax> "
        "<ymax>]\n"
        "                  [-ot <datatype>] [-bandcount <val>] [-nodata "
        "<val>[,<val>...]]\n"
        "                  [-colorinterp <val>[,<val>...]] [-mask]\n"
        "                  [-mo <KEY>=<VALUE>]...\n"
        "                  [-fetch_md <gdal_md_name> <fld_name> "
        "<fld_type>]...\n"
        "                  <index_file> <file_or_dir> [<file_or_dir>]...\n"
        "\n"
        "e.g.\n"
        "  % gdaltindex doq_index.shp doq/*.tif\n"
        "\n"
        "NOTES:\n"
        "  o The index will be created if it doesn't already exist.\n"
        "  o The default tile index field is 'location'.\n"
        "  o Raster filenames will be put in the file exactly as they are "
        "specified\n"
        "    on the commandline unless the option -write_absolute_path is "
        "used.\n"
        "  o If -skip_different_projection is specified, only files with same "
        "projection ref\n"
        "    as files already inserted in the tileindex will be inserted "
        "(unless t_srs is specified).\n"
        "  o If -t_srs is specified, geometries of input files will be "
        "transformed to the desired\n"
        "    target coordinate reference system.\n"
        "    Note that using this option generates files that are NOT "
        "compatible with MapServer < 6.4.\n"
        "  o Simple rectangular polygons are generated in the same coordinate "
        "reference system\n"
        "    as the rasters, or in target reference system if the -t_srs "
        "option is used.\n");

    if (pszErrorMsg != nullptr)
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit(bIsError ? 1 : 0);
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
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    for (int i = 0; argv != nullptr && argv[i] != nullptr; i++)
    {
        if (EQUAL(argv[i], "--utility_version"))
        {
            printf("%s was compiled against GDAL %s and is running against "
                   "GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy(argv);
            return 0;
        }
        else if (EQUAL(argv[i], "--help"))
        {
            Usage(false, nullptr);
        }
    }

    auto psOptionsForBinary = std::make_unique<GDALTileIndexOptionsForBinary>();
    /* coverity[tainted_data] */
    GDALTileIndexOptions *psOptions =
        GDALTileIndexOptionsNew(argv + 1, psOptionsForBinary.get());
    CSLDestroy(argv);

    if (psOptions == nullptr)
    {
        Usage(true, nullptr);
    }

    if (!psOptionsForBinary->bDestSpecified)
        Usage(true, "No index filename specified.");

    int bUsageError = FALSE;
    GDALDatasetH hOutDS = GDALTileIndex(psOptionsForBinary->osDest.c_str(),
                                        psOptionsForBinary->aosSrcFiles.size(),
                                        psOptionsForBinary->aosSrcFiles.List(),
                                        psOptions, &bUsageError);
    if (bUsageError)
        Usage(true, nullptr);
    int nRetCode = (hOutDS) ? 0 : 1;

    GDALTileIndexOptionsFree(psOptions);

    CPLErrorReset();
    // The flush to disk is only done at that stage, so check if any error has
    // happened
    if (GDALClose(hOutDS) != CE_None)
        nRetCode = 1;
    if (CPLGetLastErrorType() != CE_None)
        nRetCode = 1;

    GDALDumpOpenDatasets(stderr);

    GDALDestroyDriverManager();

    OGRCleanupAll();

    return nRetCode;
}

MAIN_END
