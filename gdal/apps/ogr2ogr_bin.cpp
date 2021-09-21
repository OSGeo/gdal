/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for translating between formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2015, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <memory>
#include <vector>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"
#include "gdal_version.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static bool StringCISortFunction( const CPLString& a, const CPLString& b )
{
    return STRCASECMP(a.c_str(), b.c_str()) < 0;
}

static void Usage( const char* pszAdditionalMsg = nullptr, bool bShort = true )
{
    printf(
        "Usage: ogr2ogr [--help-general] [-skipfailures] [-append] [-update]\n"
        "               [-select field_list] [-where restricted_where|@filename]\n"
        "               [-progress] [-sql <sql statement>|@filename] [-dialect dialect]\n"
        "               [-preserve_fid] [-fid FID] [-limit nb_features]\n"
        "               [-spat xmin ymin xmax ymax] [-spat_srs srs_def] [-geomfield field]\n"
        "               [-a_srs srs_def] [-t_srs srs_def] [-s_srs srs_def] [-ct string]\n"
        "               [-f format_name] [-overwrite] [[-dsco NAME=VALUE] ...]\n"
        "               dst_datasource_name src_datasource_name\n"
        "               [-lco NAME=VALUE] [-nln name] \n"
        "               [-nlt type|PROMOTE_TO_MULTI|CONVERT_TO_LINEAR|CONVERT_TO_CURVE]\n"
        "               [-dim XY|XYZ|XYM|XYZM|layer_dim] [layer [layer ...]]\n"
        "\n"
        "Advanced options :\n"
        "               [-gt n] [-ds_transaction]\n"
        "               [[-oo NAME=VALUE] ...] [[-doo NAME=VALUE] ...]\n"
        "               [-clipsrc [xmin ymin xmax ymax]|WKT|datasource|spat_extent]\n"
        "               [-clipsrcsql sql_statement] [-clipsrclayer layer]\n"
        "               [-clipsrcwhere expression]\n"
        "               [-clipdst [xmin ymin xmax ymax]|WKT|datasource]\n"
        "               [-clipdstsql sql_statement] [-clipdstlayer layer]\n"
        "               [-clipdstwhere expression]\n"
        "               [-wrapdateline][-datelineoffset val]\n"
        "               [[-simplify tolerance] | [-segmentize max_dist]]\n"
        "               [-makevalid]\n"
        "               [-addfields] [-unsetFid] [-emptyStrAsNull]\n"
        "               [-relaxedFieldNameMatch] [-forceNullable] [-unsetDefault]\n"
        "               [-fieldTypeToString All|(type1[,type2]*)] [-unsetFieldWidth]\n"
        "               [-mapFieldType srctype|All=dsttype[,srctype2=dsttype2]*]\n"
        "               [-fieldmap identity | index1[,index2]*]\n"
        "               [-splitlistfields] [-maxsubfields val]\n"
        "               [-resolveDomains]\n"
        "               [-explodecollections] [-zfield field_name]\n"
        "               [-gcp ungeoref_x ungeoref_y georef_x georef_y [elevation]]* [-order n | -tps]\n"
        "               [[-s_coord_epoch epoch] | [-t_coord_epoch epoch] | [-a_coord_epoch epoch]]\n"
        "               [-nomd] [-mo \"META-TAG=VALUE\"]* [-noNativeData]\n");

    if( bShort )
    {
        printf("\nNote: ogr2ogr --long-usage for full help.\n");
        if( pszAdditionalMsg )
            fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);
        exit(1);
    }

    printf(
        "\n -f format_name: output file format name, possible values are:\n");

    std::vector<CPLString> aoSetDrivers;
    OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();
    for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
    {
        GDALDriver *poDriver = poR->GetDriver(iDriver);

        if( CPLTestBool( CSLFetchNameValueDef(poDriver->GetMetadata(),
                                              GDAL_DCAP_CREATE, "FALSE")) )
            aoSetDrivers.push_back(poDriver->GetDescription());
    }
    std::sort(aoSetDrivers.begin(), aoSetDrivers.end(), StringCISortFunction);
    for( const auto &oDriver : aoSetDrivers )
    {
        printf("     -f \"%s\"\n", oDriver.c_str());
    }

    printf(
        " -append: Append to existing layer instead of creating new if it exists\n"
        " -overwrite: delete the output layer and recreate it empty\n"
        " -update: Open existing output datasource in update mode\n"
        " -progress: Display progress on terminal. Only works if input layers have the \n"
        "                                          \"fast feature count\" capability\n"
        " -select field_list: Comma-delimited list of fields from input layer to\n"
        "                     copy to the new layer (defaults to all)\n"
        " -where restricted_where: Attribute query (like SQL WHERE)\n"
        " -wrapdateline: split geometries crossing the dateline meridian\n"
        "                (long. = +/- 180deg)\n"
        " -datelineoffset: offset from dateline in degrees\n"
        "                (default long. = +/- 10deg,\n"
        "                geometries within 170deg to -170deg will be split)\n"
        " -sql statement: Execute given SQL statement and save result.\n"
        " -dialect value: select a dialect, usually OGRSQL to avoid native sql.\n"
        " -skipfailures: skip features or layers that fail to convert\n"
        " -gt n: group n features per transaction (default 20000). n can be set to unlimited\n"
        " -spat xmin ymin xmax ymax: spatial query extents\n"
        " -simplify tolerance: distance tolerance for simplification.\n"
        " -segmentize max_dist: maximum distance between 2 nodes.\n"
        "                       Used to create intermediate points\n"
        " -dsco NAME=VALUE: Dataset creation option (format specific)\n"
        " -lco  NAME=VALUE: Layer creation option (format specific)\n"
        " -oo   NAME=VALUE: Input dataset open option (format specific)\n"
        " -doo  NAME=VALUE: Destination dataset open option (format specific)\n"
        " -nln name: Assign an alternate name to the new layer\n"
        " -nlt type: Force a geometry type for new layer.  One of NONE, GEOMETRY,\n"
        "      POINT, LINESTRING, POLYGON, GEOMETRYCOLLECTION, MULTIPOINT,\n"
        "      MULTIPOLYGON, or MULTILINESTRING, or PROMOTE_TO_MULTI or CONVERT_TO_LINEAR.  Add \"25D\" for 3D layers.\n"
        "      Default is type of source layer.\n"
        " -dim dimension: Force the coordinate dimension to the specified value.\n"
        " -fieldTypeToString type1,...: Converts fields of specified types to\n"
        "      fields of type string in the new layer. Valid types are : Integer,\n"
        "      Integer64, Real, String, Date, Time, DateTime, Binary, IntegerList, Integer64List, RealList,\n"
        "      StringList. Special value All will convert all fields to strings.\n"
        " -fieldmap index1,index2,...: Specifies the list of field indexes to be\n"
        "      copied from the source to the destination. The (n)th value specified\n"
        "      in the list is the index of the field in the target layer definition\n"
        "      in which the n(th) field of the source layer must be copied. Index count\n"
        "      starts at zero. There must be exactly as many values in the list as\n"
        "      the count of the fields in the source layer. We can use the 'identity'\n"
        "      setting to specify that the fields should be transferred by using the\n"
        "      same order. This setting should be used along with the append setting.\n");

    printf(" -a_srs srs_def: Assign an output SRS\n"
           " -t_srs srs_def: Reproject/transform to this SRS on output\n"
           " -s_srs srs_def: Override source SRS\n"
           "\n"
           " Srs_def can be a full WKT definition (hard to escape properly),\n"
           " or a well known definition (i.e. EPSG:4326) or a file with a WKT\n"
           " definition.\n" );

    if( pszAdditionalMsg )
        fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);
}

/************************************************************************/
/*                 GDALVectorTranslateOptionsForBinaryNew()             */
/************************************************************************/

static GDALVectorTranslateOptionsForBinary *
GDALVectorTranslateOptionsForBinaryNew()
{
    return static_cast<GDALVectorTranslateOptionsForBinary *>(
        CPLCalloc(1, sizeof(GDALVectorTranslateOptionsForBinary)));
}

/************************************************************************/
/*                  GDALVectorTranslateOptionsForBinaryFree()           */
/************************************************************************/

static void GDALVectorTranslateOptionsForBinaryFree(
    GDALVectorTranslateOptionsForBinary* psOptionsForBinary )
{
    if( psOptionsForBinary )
    {
        CPLFree(psOptionsForBinary->pszDataSource);
        CPLFree(psOptionsForBinary->pszDestDataSource);
        CSLDestroy(psOptionsForBinary->papszOpenOptions);
        CPLFree(psOptionsForBinary->pszFormat);
        CPLFree(psOptionsForBinary);
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START( nArgc, papszArgv )
{
    // Check strict compilation and runtime library version as we use C++ API.
    if( !GDAL_CHECK_VERSION(papszArgv[0]) )
        exit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = nullptr;
    GDALDatasetH hODS = nullptr;
    bool bCloseODS = true;
    GDALDatasetH hDstDS = nullptr;
    int nRetCode = 1;
    GDALVectorTranslateOptionsForBinary* psOptionsForBinary = nullptr;
    GDALVectorTranslateOptions *psOptions = nullptr;

    nArgc = OGRGeneralCmdLineProcessor( nArgc, &papszArgv, 0 );

    if( nArgc < 1 )
    {
        papszArgv = nullptr;
        nRetCode = -nArgc;
        goto exit;
    }

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME,
                   GDALVersionInfo("RELEASE_NAME"));
            nRetCode = 0;
            goto exit;
        }
        else if( EQUAL(papszArgv[iArg], "--help") )
        {
            Usage();
            goto exit;
        }
        else if ( EQUAL(papszArgv[iArg], "--long-usage") )
        {
            Usage(nullptr, false);
            goto exit;
        }
    }

    psOptionsForBinary = GDALVectorTranslateOptionsForBinaryNew();
    psOptions =
        GDALVectorTranslateOptionsNew(papszArgv + 1, psOptionsForBinary);

    if( psOptions == nullptr )
    {
        Usage();
        GDALVectorTranslateOptionsForBinaryFree(psOptionsForBinary);
        goto exit;
    }

    if( psOptionsForBinary->pszDataSource == nullptr ||
        psOptionsForBinary->pszDestDataSource == nullptr )
    {
        if( psOptionsForBinary->pszDestDataSource == nullptr )
            Usage("no target datasource provided");
        else
            Usage("no source datasource provided");
        GDALVectorTranslateOptionsFree(psOptions);
        GDALVectorTranslateOptionsForBinaryFree(psOptionsForBinary);
        goto exit;
    }

    if( strcmp(psOptionsForBinary->pszDestDataSource, "/vsistdout/") == 0 )
        psOptionsForBinary->bQuiet = TRUE;

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */

    // Avoid opening twice the same datasource if it is both the input and
    // output Known to cause problems with at least FGdb, SQlite and GPKG
    // drivers. See #4270
    if (psOptionsForBinary->eAccessMode != ACCESS_CREATION &&
        strcmp(psOptionsForBinary->pszDestDataSource,
               psOptionsForBinary->pszDataSource) == 0)
    {
        hODS = GDALOpenEx(
            psOptionsForBinary->pszDataSource,
            GDAL_OF_UPDATE | GDAL_OF_VECTOR, nullptr,
            psOptionsForBinary->papszOpenOptions, nullptr);

        GDALDriverH hDriver =
            hODS != nullptr ? GDALGetDatasetDriver(hODS) : nullptr;

        // Restrict to those 3 drivers. For example it is known to break with
        // the PG driver due to the way it manages transactions.
        if( hDriver && !(EQUAL(GDALGetDescription(hDriver), "FileGDB") ||
                         EQUAL(GDALGetDescription(hDriver), "SQLite") ||
                         EQUAL(GDALGetDescription(hDriver), "GPKG")) )
        {
            hDS = GDALOpenEx(psOptionsForBinary->pszDataSource,
                             GDAL_OF_VECTOR, nullptr,
                             psOptionsForBinary->papszOpenOptions, nullptr);
        }
        else
        {
            hDS = hODS;
            bCloseODS = false;
        }
    }
    else
    {
        hDS = GDALOpenEx(psOptionsForBinary->pszDataSource,
                         GDAL_OF_VECTOR, nullptr,
                         psOptionsForBinary->papszOpenOptions, nullptr);
    }

/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
    if( hDS == nullptr )
    {
        GDALDriverManager *poDM = GetGDALDriverManager();

        fprintf(stderr, "FAILURE:\n"
                "Unable to open datasource `%s' with the following drivers.\n",
                psOptionsForBinary->pszDataSource );

        for( int iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++ )
        {
            GDALDriver* poIter = poDM->GetDriver(iDriver);
            char** papszDriverMD = poIter->GetMetadata();
            if( CPLTestBool(CSLFetchNameValueDef(papszDriverMD,
                                                 GDAL_DCAP_VECTOR, "FALSE")) )
            {
                fprintf(stderr,  "  -> `%s'\n", poIter->GetDescription());
            }
        }

        GDALVectorTranslateOptionsFree(psOptions);
        GDALVectorTranslateOptionsForBinaryFree(psOptionsForBinary);
        goto exit;
    }

    if( hODS != nullptr && psOptionsForBinary->pszFormat != nullptr )
    {
        GDALDriverManager *poDM = GetGDALDriverManager();

        GDALDriver* poDriver =
            poDM->GetDriverByName(psOptionsForBinary->pszFormat);
        if( poDriver == nullptr )
        {
            fprintf(stderr,  "Unable to find driver `%s'.\n",
                    psOptionsForBinary->pszFormat);
            fprintf(stderr,  "The following drivers are available:\n");

            for( int iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++ )
            {
                GDALDriver* poIter = poDM->GetDriver(iDriver);
                char** papszDriverMD = poIter->GetMetadata();
                if( CPLTestBool(CSLFetchNameValueDef(
                        papszDriverMD, GDAL_DCAP_VECTOR, "FALSE")) &&
                    (CPLTestBool(CSLFetchNameValueDef(
                         papszDriverMD, GDAL_DCAP_CREATE, "FALSE")) ||
                     CPLTestBool(CSLFetchNameValueDef(
                         papszDriverMD, GDAL_DCAP_CREATECOPY, "FALSE"))) )
                {
                    fprintf( stderr,  "  -> `%s'\n", poIter->GetDescription() );
                }
            }
            GDALVectorTranslateOptionsFree(psOptions);
            GDALVectorTranslateOptionsForBinaryFree(psOptionsForBinary);
            goto exit;
        }
    }

    if( !(psOptionsForBinary->bQuiet) )
    {
        GDALVectorTranslateOptionsSetProgress(psOptions, GDALTermProgress,
                                              nullptr);
    }

    {
        // TODO(schwehr): Remove scope after removing gotos
        int bUsageError = FALSE;
        hDstDS = GDALVectorTranslate(psOptionsForBinary->pszDestDataSource,
                                     hODS, 1, &hDS, psOptions, &bUsageError);
        if( bUsageError )
            Usage();
        else
            nRetCode = hDstDS ? 0 : 1;
    }

    GDALVectorTranslateOptionsFree(psOptions);
    GDALVectorTranslateOptionsForBinaryFree(psOptionsForBinary);

    if( hDS )
        GDALClose(hDS);
    if( bCloseODS )
        GDALClose(hDstDS);

exit:
    CSLDestroy(papszArgv);
    GDALDestroy();

    return nRetCode;
}
MAIN_END
