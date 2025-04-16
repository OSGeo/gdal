/******************************************************************************
 *
 * Project:  Viewshed Generator
 * Purpose:  Viewshed Generator mainline.
 * Author:   Tamas Szekeres <szekerest@gmail.com>
 *
 ******************************************************************************
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <limits>

#include "commonutils.h"
#include "gdal.h"
#include "gdalargumentparser.h"

#include "viewshed/cumulative.h"
#include "viewshed/viewshed.h"

namespace gdal
{

namespace
{

struct Options
{
    viewshed::Options opts;
    std::string osSrcFilename;
    int nBandIn{1};
    bool bQuiet;
};

/// Parse arguments into options structure.
///
/// \param argParser  Argument parser
/// \param aosArgv  Command line options as a string list
/// \return  Command line parsed as options
Options parseArgs(GDALArgumentParser &argParser, const CPLStringList &aosArgv)
{
    Options localOpts;

    viewshed::Options &opts = localOpts.opts;

    argParser.add_output_format_argument(opts.outputFormat);
    argParser.add_argument("-ox")
        .store_into(opts.observer.x)
        .metavar("<value>")
        .help(_("The X position of the observer (in SRS units)."));

    argParser.add_argument("-oy")
        .store_into(opts.observer.y)
        .metavar("<value>")
        .help(_("The Y position of the observer (in SRS units)."));

    argParser.add_argument("-oz")
        .default_value(2)
        .store_into(opts.observer.z)
        .metavar("<value>")
        .nargs(1)
        .help(_("The height of the observer above the DEM surface in the "
                "height unit of the DEM."));

    argParser.add_argument("-vv")
        .default_value(255)
        .store_into(opts.visibleVal)
        .metavar("<value>")
        .nargs(1)
        .help(_("Pixel value to set for visible areas."));

    argParser.add_argument("-iv")
        .default_value(0)
        .store_into(opts.invisibleVal)
        .metavar("<value>")
        .nargs(1)
        .help(_("Pixel value to set for invisible areas."));

    argParser.add_argument("-ov")
        .default_value(0)
        .store_into(opts.outOfRangeVal)
        .metavar("<value>")
        .nargs(1)
        .help(
            _("Pixel value to set for the cells that fall outside of the range "
              "specified by the observer location and the maximum distance."));

    argParser.add_creation_options_argument(opts.creationOpts);

    argParser.add_argument("-a_nodata")
        .default_value(-1.0)
        .store_into(opts.nodataVal)
        .metavar("<value>")
        .nargs(1)
        .help(_("The value to be set for the cells in the output raster that "
                "have no data."));

    argParser.add_argument("-tz")
        .default_value(0.0)
        .store_into(opts.targetHeight)
        .metavar("<value>")
        .nargs(1)
        .help(_("The height of the target above the DEM surface in the height "
                "unit of the DEM."));

    argParser.add_argument("-md")
        .default_value(0)
        .store_into(opts.maxDistance)
        .metavar("<value>")
        .nargs(1)
        .help(_("Maximum distance from observer to compute visibility."));

    argParser.add_argument("-j")
        .default_value(3)
        .store_into(opts.numJobs)
        .metavar("<value>")
        .nargs(1)
        .help(_(
            "Number of relative simultaneous jobs to run in cumulative mode"));

    // Value for standard atmospheric refraction. See
    // doc/source/programs/gdal_viewshed.rst
    argParser.add_argument("-cc")
        .default_value(0.85714)
        .store_into(opts.curveCoeff)
        .metavar("<value>")
        .nargs(1)
        .help(_("Coefficient to consider the effect of the curvature and "
                "refraction."));

    argParser.add_argument("-b")
        .default_value(localOpts.nBandIn)
        .store_into(localOpts.nBandIn)
        .metavar("<value>")
        .nargs(1)
        .help(_("Select an input band band containing the DEM data."));

    argParser.add_argument("-om")
        .choices("NORMAL", "DEM", "GROUND", "ACCUM")
        .metavar("NORMAL|DEM|GROUND|ACCUM")
        .action(
            [&into = opts.outputMode](const std::string &value)
            {
                if (EQUAL(value.c_str(), "DEM"))
                    into = viewshed::OutputMode::DEM;
                else if (EQUAL(value.c_str(), "GROUND"))
                    into = viewshed::OutputMode::Ground;
                else if (EQUAL(value.c_str(), "ACCUM"))
                    into = viewshed::OutputMode::Cumulative;
                else
                    into = viewshed::OutputMode::Normal;
            })
        .nargs(1)
        .help(_("Sets what information the output contains."));

    argParser.add_argument("-os")
        .default_value(10)
        .store_into(opts.observerSpacing)
        .metavar("<value>")
        .nargs(1)
        .help(_("Spacing between observer cells when using cumulative mode."));

    argParser.add_quiet_argument(&localOpts.bQuiet);

    argParser.add_argument("src_filename")
        .store_into(localOpts.osSrcFilename)
        .metavar("<src_filename>");

    argParser.add_argument("dst_filename")
        .store_into(opts.outputFilename)
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
    return localOpts;
}

/// Validate specified options.
///
/// \param localOpts  Options to validate
/// \param argParser  Argument parser
void validateArgs(Options &localOpts, const GDALArgumentParser &argParser)
{
    viewshed::Options &opts = localOpts.opts;

    if (opts.maxDistance < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Max distance must be non-negative.");
        exit(2);
    }

    if (opts.outputFormat.empty())
    {
        opts.outputFormat =
            GetOutputDriverForRaster(opts.outputFilename.c_str());
        if (opts.outputFormat.empty())
        {
            exit(2);
        }
    }

    if (opts.outputMode != viewshed::OutputMode::Cumulative)
    {
        for (const char *opt : {"-os", "-j"})
            if (argParser.is_used(opt))
            {
                std::string err = "Option " + std::string(opt) +
                                  " can only be used in cumulative mode.";
                CPLError(CE_Failure, CPLE_AppDefined, "%s", err.c_str());
                exit(2);
            }
    }

    if (opts.outputMode == viewshed::OutputMode::Cumulative)
    {
        for (const char *opt : {"-ox", "-oy", "-vv", "-iv", "-md"})
            if (argParser.is_used(opt))
            {
                std::string err = "Option " + std::string(opt) +
                                  " can't be used in cumulative mode.";
                CPLError(CE_Failure, CPLE_AppDefined, "%s", err.c_str());
                exit(2);
            }
    }
    else
    {
        for (const char *opt : {"-ox", "-oy"})
            if (!argParser.is_used(opt))
            {
                std::string err =
                    "Option " + std::string(opt) + " is required.";
                CPLError(CE_Failure, CPLE_AppDefined, "%s", err.c_str());
                exit(2);
            }
    }

    // For double values that are out of range for byte raster output,
    // set to zero.  Values less than zero are sentinel as NULL nodata.
    if (opts.outputMode == viewshed::OutputMode::Normal &&
        opts.nodataVal > std::numeric_limits<uint8_t>::max())
        opts.nodataVal = 0;
}

}  // unnamed namespace
}  // namespace gdal

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)
{
    using namespace gdal;

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

    Options localOpts = parseArgs(argParser, aosArgv);
    viewshed::Options &opts = localOpts.opts;

    validateArgs(localOpts, argParser);

    /* -------------------------------------------------------------------- */
    /*      Open source raster file.                                        */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS =
        GDALOpen(localOpts.osSrcFilename.c_str(), GA_ReadOnly);
    if (hSrcDS == nullptr)
        exit(2);

    GDALRasterBandH hBand = GDALGetRasterBand(hSrcDS, localOpts.nBandIn);
    if (hBand == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Band %d does not exist on dataset.", localOpts.nBandIn);
        exit(2);
    }

    if (!argParser.is_used("-cc"))
        opts.curveCoeff =
            gdal::viewshed::adjustCurveCoeff(opts.curveCoeff, hSrcDS);

    /* -------------------------------------------------------------------- */
    /*      Invoke.                                                         */
    /* -------------------------------------------------------------------- */

    GDALDatasetH hDstDS;

    bool bSuccess;
    if (opts.outputMode == viewshed::OutputMode::Cumulative)
    {
        viewshed::Cumulative oViewshed(opts);
        bSuccess = oViewshed.run(localOpts.osSrcFilename,
                                 localOpts.bQuiet ? GDALDummyProgress
                                                  : GDALTermProgress);
    }
    else
    {
        viewshed::Viewshed oViewshed(opts);
        bSuccess = oViewshed.run(hBand, localOpts.bQuiet ? GDALDummyProgress
                                                         : GDALTermProgress);
        hDstDS = GDALDataset::FromHandle(oViewshed.output().release());
        GDALClose(hSrcDS);
        if (GDALClose(hDstDS) != CE_None)
            bSuccess = false;
    }

    GDALDestroyDriverManager();
    OGRCleanupAll();

    return bSuccess ? 0 : 1;
}

MAIN_END
