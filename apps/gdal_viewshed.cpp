/******************************************************************************
 *
 * Project:  Viewshed Generator
 * Purpose:  Viewshed Generator mainline.
 * Author:   Tamas Szekeres <szekerest@gmail.com>
 *
 ******************************************************************************
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"
#include "commonutils.h"
#include "gdalargumentparser.h"

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    EarlySetConfigOptions(argc, argv);

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    CPLStringList aosArgv;
    aosArgv.Assign(argv, /* bTakeOwnership= */ true);
    if (argc < 1)
        std::exit(-argc);

    GDALArgumentParser argParser(aosArgv[0], /* bForBinary=*/true);

    argParser.add_description(
        _("Calculates a viewshed raster from an input raster DEM."));

    argParser.add_epilog(_("For more details, consult "
                           "https://gdal.org/programs/gdal_viewshed.html"));

    std::string osFormat;
    argParser.add_output_format_argument(osFormat);

    double dfObserverX = 0;
    argParser.add_argument("-ox")
        .store_into(dfObserverX)
        .required()
        .metavar("<value>")
        .help(_("The X position of the observer (in SRS units)."));

    double dfObserverY = 0;
    argParser.add_argument("-oy")
        .store_into(dfObserverY)
        .required()
        .metavar("<value>")
        .help(_("The Y position of the observer (in SRS units)."));

    double dfObserverHeight = 2;
    argParser.add_argument("-oz")
        .store_into(dfObserverHeight)
        .default_value(dfObserverHeight)
        .metavar("<value>")
        .nargs(1)
        .help(_("The height of the observer above the DEM surface in the "
                "height unit of the DEM."));

    double dfVisibleVal = 255;
    argParser.add_argument("-vv")
        .store_into(dfVisibleVal)
        .default_value(dfVisibleVal)
        .metavar("<value>")
        .nargs(1)
        .help(_("Pixel value to set for visible areas."));

    double dfInvisibleVal = 0.0;
    argParser.add_argument("-iv")
        .store_into(dfInvisibleVal)
        .default_value(dfInvisibleVal)
        .metavar("<value>")
        .nargs(1)
        .help(_("Pixel value to set for invisible areas."));

    double dfOutOfRangeVal = 0.0;
    argParser.add_argument("-ov")
        .store_into(dfOutOfRangeVal)
        .default_value(dfOutOfRangeVal)
        .metavar("<value>")
        .nargs(1)
        .help(
            _("Pixel value to set for the cells that fall outside of the range "
              "specified by the observer location and the maximum distance."));

    CPLStringList aosCreationOptions;
    argParser.add_creation_options_argument(aosCreationOptions);

    double dfNoDataVal = -1.0;
    argParser.add_argument("-a_nodata")
        .store_into(dfNoDataVal)
        .default_value(dfNoDataVal)
        .metavar("<value>")
        .nargs(1)
        .help(_("The value to be set for the cells in the output raster that "
                "have no data."));

    double dfTargetHeight = 0.0;
    argParser.add_argument("-tz")
        .store_into(dfTargetHeight)
        .default_value(dfTargetHeight)
        .metavar("<value>")
        .nargs(1)
        .help(_("The height of the target above the DEM surface in the height "
                "unit of the DEM."));

    double dfMaxDistance = 0.0;
    argParser.add_argument("-md")
        .store_into(dfMaxDistance)
        .default_value(dfMaxDistance)
        .metavar("<value>")
        .nargs(1)
        .help(_("Maximum distance from observer to compute visibility."));

    // Value for standard atmospheric refraction. See
    // doc/source/programs/gdal_viewshed.rst
    double dfCurvCoeff = 0.85714;
    argParser.add_argument("-cc")
        .store_into(dfCurvCoeff)
        .default_value(dfCurvCoeff)
        .metavar("<value>")
        .nargs(1)
        .help(_("Coefficient to consider the effect of the curvature and "
                "refraction."));

    int nBandIn = 1;
    argParser.add_argument("-b")
        .store_into(nBandIn)
        .default_value(nBandIn)
        .metavar("<value>")
        .nargs(1)
        .help(_("Select an input band band containing the DEM data."));

    std::string osOutputMode;
    argParser.add_argument("-om")
        .store_into(osOutputMode)
        .choices("NORMAL", "DEM", "GROUND")
        .metavar("NORMAL|DEM|GROUND")
        .default_value("NORMAL")
        .nargs(1)
        .help(_("Sets what information the output contains."));

    bool bQuiet = false;
    argParser.add_quiet_argument(&bQuiet);

    std::string osSrcFilename;
    argParser.add_argument("src_filename")
        .store_into(osSrcFilename)
        .metavar("<src_filename>");

    std::string osDstFilename;
    argParser.add_argument("dst_filename")
        .store_into(osDstFilename)
        .metavar("<dst_filename>");

    try
    {
        argParser.parse_args(aosArgv);
    }
    catch (const std::exception &err)
    {
        argParser.display_error_and_usage(err);
        std::exit(1);
    }

    GDALProgressFunc pfnProgress = nullptr;
    if (!bQuiet)
        pfnProgress = GDALTermProgress;

    if (osFormat.empty())
    {
        osFormat = GetOutputDriverForRaster(osDstFilename.c_str());
        if (osFormat.empty())
        {
            exit(2);
        }
    }

    GDALViewshedOutputType outputMode = GVOT_NORMAL;
    if (EQUAL(osOutputMode.c_str(), "DEM"))
    {
        outputMode = GVOT_MIN_TARGET_HEIGHT_FROM_DEM;
    }
    else if (EQUAL(osOutputMode.c_str(), "GROUND"))
    {
        outputMode = GVOT_MIN_TARGET_HEIGHT_FROM_GROUND;
    }

    /* -------------------------------------------------------------------- */
    /*      Open source raster file.                                        */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS = GDALOpen(osSrcFilename.c_str(), GA_ReadOnly);
    if (hSrcDS == nullptr)
        exit(2);

    GDALRasterBandH hBand = GDALGetRasterBand(hSrcDS, nBandIn);
    if (hBand == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Band %d does not exist on dataset.", nBandIn);
        exit(2);
    }

    const bool bCurvCoeffSpecified = argParser.is_used("-cc");
    if (!bCurvCoeffSpecified)
    {
        const OGRSpatialReference *poSRS =
            GDALDataset::FromHandle(hSrcDS)->GetSpatialRef();
        if (poSRS)
        {
            OGRErr eSRSerr;
            const double dfSemiMajor = poSRS->GetSemiMajor(&eSRSerr);
            if (eSRSerr != OGRERR_FAILURE &&
                fabs(dfSemiMajor - SRS_WGS84_SEMIMAJOR) >
                    0.05 * SRS_WGS84_SEMIMAJOR)
            {
                dfCurvCoeff = 1.0;
                CPLDebug("gdal_viewshed",
                         "Using -cc=1.0 as a non-Earth CRS has been detected");
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Invoke.                                                         */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS = GDALViewshedGenerate(
        hBand, osFormat.c_str(), osDstFilename.c_str(),
        aosCreationOptions.List(), dfObserverX, dfObserverY, dfObserverHeight,
        dfTargetHeight, dfVisibleVal, dfInvisibleVal, dfOutOfRangeVal,
        dfNoDataVal, dfCurvCoeff, GVM_Edge, dfMaxDistance, pfnProgress, nullptr,
        outputMode, nullptr);
    bool bSuccess = hDstDS != nullptr;
    GDALClose(hSrcDS);
    if (GDALClose(hDstDS) != CE_None)
        bSuccess = false;

    GDALDestroyDriverManager();
    OGRCleanupAll();

    return bSuccess ? 0 : 1;
}

MAIN_END
