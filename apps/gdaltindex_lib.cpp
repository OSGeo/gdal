/******************************************************************************
 *
 * Project:  MapServer
 * Purpose:  Commandline App to build tile index for raster files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam, DM Solutions Group Inc
 * Copyright (c) 2007-2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_utils.h"
#include "gdal_priv.h"
#include "gdal_utils_priv.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "ogr_spatialref.h"
#include "commonutils.h"
#include "gdalargumentparser.h"

#include <ctype.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

typedef enum
{
    FORMAT_AUTO,
    FORMAT_WKT,
    FORMAT_EPSG,
    FORMAT_PROJ
} SrcSRSFormat;

/************************************************************************/
/*                        GDALTileIndexRasterMetadata                   */
/************************************************************************/

struct GDALTileIndexRasterMetadata
{
    OGRFieldType eType = OFTString;
    std::string osFieldName{};
    std::string osRasterItemName{};
};

/************************************************************************/
/*                          GDALTileIndexOptions                        */
/************************************************************************/

struct GDALTileIndexOptions
{
    bool bOverwrite = false;
    std::string osFormat{};
    std::string osIndexLayerName{};
    std::string osLocationField = "location";
    CPLStringList aosLCO{};
    std::string osTargetSRS{};
    bool bWriteAbsolutePath = false;
    bool bSkipDifferentProjection = false;
    std::string osSrcSRSFieldName{};
    SrcSRSFormat eSrcSRSFormat = FORMAT_AUTO;
    double xres = std::numeric_limits<double>::quiet_NaN();
    double yres = std::numeric_limits<double>::quiet_NaN();
    double xmin = std::numeric_limits<double>::quiet_NaN();
    double ymin = std::numeric_limits<double>::quiet_NaN();
    double xmax = std::numeric_limits<double>::quiet_NaN();
    double ymax = std::numeric_limits<double>::quiet_NaN();
    std::string osBandCount{};
    std::string osNodata{};
    std::string osColorInterp{};
    std::string osDataType{};
    bool bMaskBand = false;
    std::vector<std::string> aosMetadata{};
    std::string osGTIFilename{};
    bool bRecursive = false;
    double dfMinPixelSize = std::numeric_limits<double>::quiet_NaN();
    double dfMaxPixelSize = std::numeric_limits<double>::quiet_NaN();
    std::vector<GDALTileIndexRasterMetadata> aoFetchMD{};
    std::set<std::string> oSetFilenameFilters{};
    GDALProgressFunc pfnProgress = nullptr;
    void *pProgressData = nullptr;
};

/************************************************************************/
/*                               PatternMatch()                         */
/************************************************************************/

static bool PatternMatch(const char *input, const char *pattern)

{
    while (*input != '\0')
    {
        if (*pattern == '\0')
            return false;

        else if (*pattern == '?')
        {
            pattern++;
            if (static_cast<unsigned int>(*input) > 127)
            {
                // Continuation bytes of such characters are of the form
                // 10xxxxxx (0x80), whereas single-byte are 0xxxxxxx
                // and the start of a multi-byte is 11xxxxxx
                do
                {
                    input++;
                } while (static_cast<unsigned int>(*input) > 127);
            }
            else
            {
                input++;
            }
        }
        else if (*pattern == '*')
        {
            if (pattern[1] == '\0')
                return true;

            // Try eating varying amounts of the input till we get a positive.
            for (int eat = 0; input[eat] != '\0'; eat++)
            {
                if (PatternMatch(input + eat, pattern + 1))
                    return true;
            }

            return false;
        }
        else
        {
            if (CPLTolower(*pattern) != CPLTolower(*input))
            {
                return false;
            }
            else
            {
                input++;
                pattern++;
            }
        }
    }

    if (*pattern != '\0' && strcmp(pattern, "*") != 0)
        return false;
    else
        return true;
}

/************************************************************************/
/*                     GDALTileIndexAppOptionsGetParser()               */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser> GDALTileIndexAppOptionsGetParser(
    GDALTileIndexOptions *psOptions,
    GDALTileIndexOptionsForBinary *psOptionsForBinary)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdaltindex", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(
        _("Build a tile index from a list of datasets."));

    argParser->add_epilog(
        _("For more details, see the full documentation for gdaltindex at\n"
          "https://gdal.org/programs/gdaltindex.html"));

    argParser->add_argument("-overwrite")
        .flag()
        .store_into(psOptions->bOverwrite)
        .help(_("Overwrite the output tile index file if it already exists."));

    argParser->add_argument("-recursive")
        .flag()
        .store_into(psOptions->bRecursive)
        .help(_("Whether directories specified in <file_or_dir> should be "
                "explored recursively."));

    argParser->add_argument("-filename_filter")
        .metavar("<val>")
        .append()
        .store_into(psOptions->oSetFilenameFilters)
        .help(_("Pattern that the filenames contained in directories pointed "
                "by <file_or_dir> should follow."));

    argParser->add_argument("-min_pixel_size")
        .metavar("<val>")
        .store_into(psOptions->dfMinPixelSize)
        .help(_("Minimum pixel size in term of geospatial extent per pixel "
                "(resolution) that a raster should have to be selected."));

    argParser->add_argument("-max_pixel_size")
        .metavar("<val>")
        .store_into(psOptions->dfMaxPixelSize)
        .help(_("Maximum pixel size in term of geospatial extent per pixel "
                "(resolution) that a raster should have to be selected."));

    argParser->add_output_format_argument(psOptions->osFormat);

    argParser->add_argument("-tileindex")
        .metavar("<field_name>")
        .store_into(psOptions->osLocationField)
        .help(_("Name of the layer in the tile index file."));

    argParser->add_argument("-write_absolute_path")
        .flag()
        .store_into(psOptions->bWriteAbsolutePath)
        .help(_("Write the absolute path of the raster files in the tile index "
                "file."));

    argParser->add_argument("-skip_different_projection")
        .flag()
        .store_into(psOptions->bSkipDifferentProjection)
        .help(_(
            "Only files with the same projection as files already inserted in "
            "the tile index will be inserted (unless -t_srs is specified)."));

    argParser->add_argument("-t_srs")
        .metavar("<srs_def>")
        .store_into(psOptions->osTargetSRS)
        .help(_("Geometries of input files will be transformed to the desired "
                "target coordinate reference system."));

    argParser->add_argument("-src_srs_name")
        .metavar("<field_name>")
        .store_into(psOptions->osSrcSRSFieldName)
        .help(_("Name of the field in the tile index file where the source SRS "
                "will be stored."));

    argParser->add_argument("-src_srs_format")
        .metavar("{AUTO|WKT|EPSG|PROJ}")
        .choices("AUTO", "WKT", "EPSG", "PROJ")
        .action(
            [psOptions](const auto &f)
            {
                if (f == "WKT")
                    psOptions->eSrcSRSFormat = FORMAT_WKT;
                else if (f == "EPSG")
                    psOptions->eSrcSRSFormat = FORMAT_EPSG;
                else if (f == "PROJ")
                    psOptions->eSrcSRSFormat = FORMAT_PROJ;
                else
                    psOptions->eSrcSRSFormat = FORMAT_AUTO;
            })
        .help(_("Format of the source SRS to store in the tile index file."));

    argParser->add_argument("-lyr_name")
        .metavar("<name>")
        .store_into(psOptions->osIndexLayerName)
        .help(_("Name of the layer in the tile index file."));

    argParser->add_layer_creation_options_argument(psOptions->aosLCO);

    // GTI driver options

    argParser->add_argument("-gti_filename")
        .metavar("<filename>")
        .store_into(psOptions->osGTIFilename)
        .help(_("Filename of the XML Virtual Tile Index file to generate."));

    // NOTE: no store_into
    argParser->add_argument("-tr")
        .metavar("<xres> <yres>")
        .nargs(2)
        .scan<'g', double>()
        .help(_("Set target resolution."));

    // NOTE: no store_into
    argParser->add_argument("-te")
        .metavar("<xmin> <ymin> <xmax> <ymax>")
        .nargs(4)
        .scan<'g', double>()
        .help(_("Set target extent in SRS unit."));

    argParser->add_argument("-ot")
        .metavar("<datatype>")
        .store_into(psOptions->osDataType)
        .help(_("Output data type."));

    argParser->add_argument("-bandcount")
        .metavar("<val>")
        .store_into(psOptions->osBandCount)
        .help(_("Number of bands of the tiles of the tile index."));

    argParser->add_argument("-nodata")
        .metavar("<val>")
        .append()
        .store_into(psOptions->osNodata)
        .help(_("Nodata value of the tiles of the tile index."));

    // Should we use choices here?
    argParser->add_argument("-colorinterp")
        .metavar("<val>")
        .append()
        .store_into(psOptions->osColorInterp)
        .help(_("Color interpretation of of the tiles of the tile index: red, "
                "green, blue, alpha, gray, undefined."));

    argParser->add_argument("-mask")
        .flag()
        .store_into(psOptions->bMaskBand)
        .help(_("Add a mask band to the tiles of the tile index."));

    argParser->add_argument("-mo")
        .metavar("<name>=<value>")
        .append()
        .store_into(psOptions->aosMetadata)
        .help(_("Write an arbitrary layer metadata item, for formats that "
                "support layer metadata."));

    // NOTE: no store_into
    argParser->add_argument("-fetch_md")
        .nargs(3)
        .metavar("<gdal_md_name> <fld_name> <fld_type>")
        .append()
        .help("Fetch a metadata item from the raster tile and write it as a "
              "field in the tile index.");

    if (psOptionsForBinary)
    {
        argParser->add_quiet_argument(&psOptionsForBinary->bQuiet);

        argParser->add_argument("index_file")
            .metavar("<index_file>")
            .store_into(psOptionsForBinary->osDest)
            .help(_("The name of the output file to create/append to."));

        argParser->add_argument("file_or_dir")
            .metavar("<file_or_dir>")
            .nargs(argparse::nargs_pattern::at_least_one)
            .action([psOptionsForBinary](const std::string &s)
                    { psOptionsForBinary->aosSrcFiles.AddString(s.c_str()); })
            .help(_(
                "The input GDAL raster files or directory, can be multiple "
                "locations separated by spaces. Wildcards may also be used."));
    }

    return argParser;
}

/************************************************************************/
/*                  GDALTileIndexAppGetParserUsage()                    */
/************************************************************************/

std::string GDALTileIndexAppGetParserUsage()
{
    try
    {
        GDALTileIndexOptions sOptions;
        GDALTileIndexOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALTileIndexAppOptionsGetParser(&sOptions, &sOptionsForBinary);
        return argParser->usage();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return std::string();
    }
}

/************************************************************************/
/*                        GDALTileIndexTileIterator                     */
/************************************************************************/

struct GDALTileIndexTileIterator
{
    const GDALTileIndexOptions *psOptions = nullptr;
    int nSrcCount = 0;
    const char *const *papszSrcDSNames = nullptr;
    std::string osCurDir{};
    int iCurSrc = 0;
    VSIDIR *psDir = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALTileIndexTileIterator)

    GDALTileIndexTileIterator(const GDALTileIndexOptions *psOptionsIn,
                              int nSrcCountIn,
                              const char *const *papszSrcDSNamesIn)
        : psOptions(psOptionsIn), nSrcCount(nSrcCountIn),
          papszSrcDSNames(papszSrcDSNamesIn)
    {
    }

    void reset()
    {
        if (psDir)
            VSICloseDir(psDir);
        psDir = nullptr;
        iCurSrc = 0;
    }

    std::string next()
    {
        while (true)
        {
            if (!psDir)
            {
                if (iCurSrc == nSrcCount)
                {
                    break;
                }

                VSIStatBufL sStatBuf;
                const std::string osCurName = papszSrcDSNames[iCurSrc++];
                if (VSIStatL(osCurName.c_str(), &sStatBuf) == 0 &&
                    VSI_ISDIR(sStatBuf.st_mode))
                {
                    auto poSrcDS = std::unique_ptr<GDALDataset>(
                        GDALDataset::Open(osCurName.c_str(), GDAL_OF_RASTER,
                                          nullptr, nullptr, nullptr));
                    if (poSrcDS)
                        return osCurName;

                    osCurDir = osCurName;
                    psDir = VSIOpenDir(
                        osCurDir.c_str(),
                        /*nDepth=*/psOptions->bRecursive ? -1 : 0, nullptr);
                    if (!psDir)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot open directory %s", osCurDir.c_str());
                        return std::string();
                    }
                }
                else
                {
                    return osCurName;
                }
            }

            auto psEntry = VSIGetNextDirEntry(psDir);
            if (!psEntry)
            {
                VSICloseDir(psDir);
                psDir = nullptr;
                continue;
            }

            if (!psOptions->oSetFilenameFilters.empty())
            {
                bool bMatchFound = false;
                const std::string osFilenameOnly =
                    CPLGetFilename(psEntry->pszName);
                for (const auto &osFilter : psOptions->oSetFilenameFilters)
                {
                    if (PatternMatch(osFilenameOnly.c_str(), osFilter.c_str()))
                    {
                        bMatchFound = true;
                        break;
                    }
                }
                if (!bMatchFound)
                    continue;
            }

            const std::string osFilename = CPLFormFilenameSafe(
                osCurDir.c_str(), psEntry->pszName, nullptr);
            if (VSI_ISDIR(psEntry->nMode))
            {
                auto poSrcDS = std::unique_ptr<GDALDataset>(
                    GDALDataset::Open(osFilename.c_str(), GDAL_OF_RASTER,
                                      nullptr, nullptr, nullptr));
                if (poSrcDS)
                    return osFilename;
                continue;
            }

            return osFilename;
        }
        return std::string();
    }
};

/************************************************************************/
/*                           GDALTileIndex()                            */
/************************************************************************/

/* clang-format off */
/**
 * Build a tile index from a list of datasets.
 *
 * This is the equivalent of the
 * <a href="/programs/gdaltindex.html">gdaltindex</a> utility.
 *
 * GDALTileIndexOptions* must be allocated and freed with
 * GDALTileIndexOptionsNew() and GDALTileIndexOptionsFree() respectively.
 *
 * @param pszDest the destination dataset path.
 * @param nSrcCount the number of input datasets.
 * @param papszSrcDSNames the list of input dataset names
 * @param psOptionsIn the options struct returned by GDALTileIndexOptionsNew() or
 * NULL.
 * @param pbUsageError pointer to a integer output variable to store if any
 * usage error has occurred.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose()) or NULL in case of error.
 *
 * @since GDAL3.9
 */
/* clang-format on */

GDALDatasetH GDALTileIndex(const char *pszDest, int nSrcCount,
                           const char *const *papszSrcDSNames,
                           const GDALTileIndexOptions *psOptionsIn,
                           int *pbUsageError)
{
    return GDALTileIndexInternal(pszDest, nullptr, nullptr, nSrcCount,
                                 papszSrcDSNames, psOptionsIn, pbUsageError);
}

GDALDatasetH GDALTileIndexInternal(const char *pszDest,
                                   GDALDatasetH hTileIndexDS, OGRLayerH hLayer,
                                   int nSrcCount,
                                   const char *const *papszSrcDSNames,
                                   const GDALTileIndexOptions *psOptionsIn,
                                   int *pbUsageError)
{
    if (nSrcCount == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No input dataset specified.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    auto psOptions = psOptionsIn
                         ? std::make_unique<GDALTileIndexOptions>(*psOptionsIn)
                         : std::make_unique<GDALTileIndexOptions>();

    GDALTileIndexTileIterator oGDALTileIndexTileIterator(
        psOptions.get(), nSrcCount, papszSrcDSNames);

    /* -------------------------------------------------------------------- */
    /*      Create and validate target SRS if given.                        */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference oTargetSRS;
    if (!psOptions->osTargetSRS.empty())
    {
        if (psOptions->bSkipDifferentProjection)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "-skip_different_projections does not apply "
                     "when -t_srs is requested.");
        }
        oTargetSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        // coverity[tainted_data]
        oTargetSRS.SetFromUserInput(psOptions->osTargetSRS.c_str());
    }

    /* -------------------------------------------------------------------- */
    /*      Open or create the target datasource                            */
    /* -------------------------------------------------------------------- */

    std::unique_ptr<GDALDataset> poTileIndexDSUnique;
    GDALDataset *poTileIndexDS = GDALDataset::FromHandle(hTileIndexDS);
    OGRLayer *poLayer = OGRLayer::FromHandle(hLayer);
    bool bExistingLayer = false;
    std::string osFormat;

    if (!hTileIndexDS)
    {
        if (psOptions->bOverwrite)
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            auto hDriver = GDALIdentifyDriver(pszDest, nullptr);
            if (hDriver)
                GDALDeleteDataset(hDriver, pszDest);
            else
                VSIUnlink(pszDest);
            CPLPopErrorHandler();
        }

        poTileIndexDSUnique.reset(
            GDALDataset::Open(pszDest, GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr,
                              nullptr, nullptr));

        if (poTileIndexDSUnique != nullptr)
        {
            auto poDriver = poTileIndexDSUnique->GetDriver();
            if (poDriver)
                osFormat = poDriver->GetDescription();

            if (poTileIndexDSUnique->GetLayerCount() == 1)
            {
                poLayer = poTileIndexDSUnique->GetLayer(0);
            }
            else
            {
                if (psOptions->osIndexLayerName.empty())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Multiple layers detected: -lyr_name must be "
                             "specified.");
                    if (pbUsageError)
                        *pbUsageError = true;
                    return nullptr;
                }
                CPLPushErrorHandler(CPLQuietErrorHandler);
                poLayer = poTileIndexDSUnique->GetLayerByName(
                    psOptions->osIndexLayerName.c_str());
                CPLPopErrorHandler();
            }
        }
        else
        {
            if (psOptions->osFormat.empty())
            {
                const auto aoDrivers =
                    GetOutputDriversFor(pszDest, GDAL_OF_VECTOR);
                if (aoDrivers.empty())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot guess driver for %s", pszDest);
                    return nullptr;
                }
                else
                {
                    if (aoDrivers.size() > 1)
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Several drivers matching %s extension. Using %s",
                            CPLGetExtensionSafe(pszDest).c_str(),
                            aoDrivers[0].c_str());
                    }
                    osFormat = aoDrivers[0];
                }
            }
            else
            {
                osFormat = psOptions->osFormat;
            }

            auto poDriver =
                GetGDALDriverManager()->GetDriverByName(osFormat.c_str());
            if (poDriver == nullptr)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "%s driver not available.", osFormat.c_str());
                return nullptr;
            }

            poTileIndexDSUnique.reset(
                poDriver->Create(pszDest, 0, 0, 0, GDT_Unknown, nullptr));
            if (!poTileIndexDSUnique)
                return nullptr;
        }

        poTileIndexDS = poTileIndexDSUnique.get();
    }

    if (osFormat.empty())
    {
        if (auto poOutDrv = poTileIndexDS->GetDriver())
            osFormat = poOutDrv->GetDescription();
    }

    const int nMaxFieldSize =
        EQUAL(osFormat.c_str(), "ESRI Shapefile") ? 254 : 0;

    if (poLayer)
    {
        bExistingLayer = true;
    }
    else
    {
        std::string osLayerName;
        if (psOptions->osIndexLayerName.empty())
        {
            VSIStatBuf sStat;
            if (EQUAL(osFormat.c_str(), "ESRI Shapefile") ||
                VSIStat(pszDest, &sStat) == 0)
            {
                osLayerName = CPLGetBasenameSafe(pszDest);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "-lyr_name must be specified.");
                if (pbUsageError)
                    *pbUsageError = true;
                return nullptr;
            }
        }
        else
        {
            if (psOptions->bOverwrite)
            {
                for (int i = 0; i < poTileIndexDS->GetLayerCount(); ++i)
                {
                    auto poExistingLayer = poTileIndexDS->GetLayer(i);
                    if (poExistingLayer && poExistingLayer->GetName() ==
                                               psOptions->osIndexLayerName)
                    {
                        poTileIndexDS->DeleteLayer(i);
                        break;
                    }
                }
            }

            osLayerName = psOptions->osIndexLayerName;
        }

        /* get spatial reference for output file from target SRS (if set) */
        /* or from first input file */
        OGRSpatialReference oSRS;
        if (!oTargetSRS.IsEmpty())
        {
            oSRS = oTargetSRS;
        }
        else
        {
            std::string osFilename = oGDALTileIndexTileIterator.next();
            if (osFilename.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find any tile");
                return nullptr;
            }
            oGDALTileIndexTileIterator.reset();
            auto poSrcDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                osFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                nullptr, nullptr, nullptr));
            if (!poSrcDS)
                return nullptr;

            auto poSrcSRS = poSrcDS->GetSpatialRef();
            if (poSrcSRS)
                oSRS = *poSrcSRS;
        }

        poLayer = poTileIndexDS->CreateLayer(
            osLayerName.c_str(), oSRS.IsEmpty() ? nullptr : &oSRS, wkbPolygon,
            psOptions->aosLCO.List());
        if (!poLayer)
            return nullptr;

        OGRFieldDefn oLocationField(psOptions->osLocationField.c_str(),
                                    OFTString);
        oLocationField.SetWidth(nMaxFieldSize);
        if (poLayer->CreateField(&oLocationField) != OGRERR_NONE)
            return nullptr;

        if (!psOptions->osSrcSRSFieldName.empty())
        {
            OGRFieldDefn oSrcSRSField(psOptions->osSrcSRSFieldName.c_str(),
                                      OFTString);
            oSrcSRSField.SetWidth(nMaxFieldSize);
            if (poLayer->CreateField(&oSrcSRSField) != OGRERR_NONE)
                return nullptr;
        }
    }

    auto poLayerDefn = poLayer->GetLayerDefn();

    for (const auto &oFetchMD : psOptions->aoFetchMD)
    {
        if (poLayerDefn->GetFieldIndex(oFetchMD.osFieldName.c_str()) < 0)
        {
            OGRFieldDefn oField(oFetchMD.osFieldName.c_str(), oFetchMD.eType);
            if (poLayer->CreateField(&oField) != OGRERR_NONE)
                return nullptr;
        }
    }

    if (!psOptions->osGTIFilename.empty())
    {
        if (!psOptions->aosMetadata.empty())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "-mo is not supported when -gti_filename is used");
            return nullptr;
        }
        CPLXMLNode *psRoot =
            CPLCreateXMLNode(nullptr, CXT_Element, "GDALTileIndexDataset");
        CPLCreateXMLElementAndValue(psRoot, "IndexDataset", pszDest);
        CPLCreateXMLElementAndValue(psRoot, "IndexLayer", poLayer->GetName());
        CPLCreateXMLElementAndValue(psRoot, "LocationField",
                                    psOptions->osLocationField.c_str());
        if (!std::isnan(psOptions->xres))
        {
            CPLCreateXMLElementAndValue(psRoot, "ResX",
                                        CPLSPrintf("%.18g", psOptions->xres));
            CPLCreateXMLElementAndValue(psRoot, "ResY",
                                        CPLSPrintf("%.18g", psOptions->yres));
        }
        if (!std::isnan(psOptions->xmin))
        {
            CPLCreateXMLElementAndValue(psRoot, "MinX",
                                        CPLSPrintf("%.18g", psOptions->xmin));
            CPLCreateXMLElementAndValue(psRoot, "MinY",
                                        CPLSPrintf("%.18g", psOptions->ymin));
            CPLCreateXMLElementAndValue(psRoot, "MaxX",
                                        CPLSPrintf("%.18g", psOptions->xmax));
            CPLCreateXMLElementAndValue(psRoot, "MaxY",
                                        CPLSPrintf("%.18g", psOptions->ymax));
        }

        int nBandCount = 0;
        if (!psOptions->osBandCount.empty())
        {
            nBandCount = atoi(psOptions->osBandCount.c_str());
        }
        else
        {
            if (!psOptions->osDataType.empty())
            {
                nBandCount = std::max(
                    nBandCount,
                    CPLStringList(CSLTokenizeString2(
                                      psOptions->osDataType.c_str(), ", ", 0))
                        .size());
            }
            if (!psOptions->osNodata.empty())
            {
                nBandCount = std::max(
                    nBandCount,
                    CPLStringList(CSLTokenizeString2(
                                      psOptions->osNodata.c_str(), ", ", 0))
                        .size());
            }
            if (!psOptions->osColorInterp.empty())
            {
                nBandCount =
                    std::max(nBandCount,
                             CPLStringList(
                                 CSLTokenizeString2(
                                     psOptions->osColorInterp.c_str(), ", ", 0))
                                 .size());
            }
        }

        for (int i = 0; i < nBandCount; ++i)
        {
            auto psBand = CPLCreateXMLNode(psRoot, CXT_Element, "Band");
            CPLAddXMLAttributeAndValue(psBand, "band", CPLSPrintf("%d", i + 1));
            if (!psOptions->osDataType.empty())
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString2(psOptions->osDataType.c_str(), ", ", 0));
                if (aosTokens.size() == 1)
                    CPLAddXMLAttributeAndValue(psBand, "dataType",
                                               aosTokens[0]);
                else if (i < aosTokens.size())
                    CPLAddXMLAttributeAndValue(psBand, "dataType",
                                               aosTokens[i]);
            }
            if (!psOptions->osNodata.empty())
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString2(psOptions->osNodata.c_str(), ", ", 0));
                if (aosTokens.size() == 1)
                    CPLCreateXMLElementAndValue(psBand, "NoDataValue",
                                                aosTokens[0]);
                else if (i < aosTokens.size())
                    CPLCreateXMLElementAndValue(psBand, "NoDataValue",
                                                aosTokens[i]);
            }
            if (!psOptions->osColorInterp.empty())
            {
                const CPLStringList aosTokens(CSLTokenizeString2(
                    psOptions->osColorInterp.c_str(), ", ", 0));
                if (aosTokens.size() == 1)
                    CPLCreateXMLElementAndValue(psBand, "ColorInterp",
                                                aosTokens[0]);
                else if (i < aosTokens.size())
                    CPLCreateXMLElementAndValue(psBand, "ColorInterp",
                                                aosTokens[i]);
            }
        }

        if (psOptions->bMaskBand)
        {
            CPLCreateXMLElementAndValue(psRoot, "MaskBand", "true");
        }
        int res =
            CPLSerializeXMLTreeToFile(psRoot, psOptions->osGTIFilename.c_str());
        CPLDestroyXMLNode(psRoot);
        if (!res)
            return nullptr;
    }
    else
    {
        poLayer->SetMetadataItem("LOCATION_FIELD",
                                 psOptions->osLocationField.c_str());
        if (!std::isnan(psOptions->xres))
        {
            poLayer->SetMetadataItem("RESX",
                                     CPLSPrintf("%.18g", psOptions->xres));
            poLayer->SetMetadataItem("RESY",
                                     CPLSPrintf("%.18g", psOptions->yres));
        }
        if (!std::isnan(psOptions->xmin))
        {
            poLayer->SetMetadataItem("MINX",
                                     CPLSPrintf("%.18g", psOptions->xmin));
            poLayer->SetMetadataItem("MINY",
                                     CPLSPrintf("%.18g", psOptions->ymin));
            poLayer->SetMetadataItem("MAXX",
                                     CPLSPrintf("%.18g", psOptions->xmax));
            poLayer->SetMetadataItem("MAXY",
                                     CPLSPrintf("%.18g", psOptions->ymax));
        }
        if (!psOptions->osBandCount.empty())
        {
            poLayer->SetMetadataItem("BAND_COUNT",
                                     psOptions->osBandCount.c_str());
        }
        if (!psOptions->osDataType.empty())
        {
            poLayer->SetMetadataItem("DATA_TYPE",
                                     psOptions->osDataType.c_str());
        }
        if (!psOptions->osNodata.empty())
        {
            poLayer->SetMetadataItem("NODATA", psOptions->osNodata.c_str());
        }
        if (!psOptions->osColorInterp.empty())
        {
            poLayer->SetMetadataItem("COLOR_INTERPRETATION",
                                     psOptions->osColorInterp.c_str());
        }
        if (psOptions->bMaskBand)
        {
            poLayer->SetMetadataItem("MASK_BAND", "YES");
        }
        const CPLStringList aosMetadata(psOptions->aosMetadata);
        for (const auto &[pszKey, pszValue] :
             cpl::IterateNameValue(aosMetadata))
        {
            poLayer->SetMetadataItem(pszKey, pszValue);
        }
    }

    const int ti_field =
        poLayerDefn->GetFieldIndex(psOptions->osLocationField.c_str());
    if (ti_field < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find field `%s' in file `%s'.",
                 psOptions->osLocationField.c_str(), pszDest);
        return nullptr;
    }

    int i_SrcSRSName = -1;
    if (!psOptions->osSrcSRSFieldName.empty())
    {
        i_SrcSRSName =
            poLayerDefn->GetFieldIndex(psOptions->osSrcSRSFieldName.c_str());
        if (i_SrcSRSName < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to find field `%s' in file `%s'.",
                     psOptions->osSrcSRSFieldName.c_str(), pszDest);
            return nullptr;
        }
    }

    // Load in memory existing file names in tile index.
    std::set<std::string> oSetExistingFiles;
    OGRSpatialReference oAlreadyExistingSRS;
    if (bExistingLayer)
    {
        for (auto &&poFeature : poLayer)
        {
            if (poFeature->IsFieldSetAndNotNull(ti_field))
            {
                if (oSetExistingFiles.empty())
                {
                    auto poSrcDS =
                        std::unique_ptr<GDALDataset>(GDALDataset::Open(
                            poFeature->GetFieldAsString(ti_field),
                            GDAL_OF_RASTER, nullptr, nullptr, nullptr));
                    if (poSrcDS)
                    {
                        auto poSrcSRS = poSrcDS->GetSpatialRef();
                        if (poSrcSRS)
                            oAlreadyExistingSRS = *poSrcSRS;
                    }
                }
                oSetExistingFiles.insert(poFeature->GetFieldAsString(ti_field));
            }
        }
    }

    std::string osCurrentPath;
    if (psOptions->bWriteAbsolutePath)
    {
        char *pszCurrentPath = CPLGetCurrentDir();
        if (pszCurrentPath)
        {
            osCurrentPath = pszCurrentPath;
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "This system does not support the CPLGetCurrentDir call. "
                     "The option -bWriteAbsolutePath will have no effect.");
        }
        CPLFree(pszCurrentPath);
    }

    const bool bIsGTIContext =
        !std::isnan(psOptions->xres) || !std::isnan(psOptions->xmin) ||
        !psOptions->osBandCount.empty() || !psOptions->osNodata.empty() ||
        !psOptions->osColorInterp.empty() || !psOptions->osDataType.empty() ||
        psOptions->bMaskBand || !psOptions->aosMetadata.empty() ||
        !psOptions->osGTIFilename.empty();

    /* -------------------------------------------------------------------- */
    /*      loop over GDAL files, processing.                               */
    /* -------------------------------------------------------------------- */
    int iCur = 0;
    int nTotal = nSrcCount + 1;
    while (true)
    {
        const std::string osSrcFilename = oGDALTileIndexTileIterator.next();
        if (osSrcFilename.empty())
            break;

        std::string osFileNameToWrite;
        VSIStatBuf sStatBuf;

        // Make sure it is a file before building absolute path name.
        if (!osCurrentPath.empty() &&
            CPLIsFilenameRelative(osSrcFilename.c_str()) &&
            VSIStat(osSrcFilename.c_str(), &sStatBuf) == 0)
        {
            osFileNameToWrite = CPLProjectRelativeFilenameSafe(
                osCurrentPath.c_str(), osSrcFilename.c_str());
        }
        else
        {
            osFileNameToWrite = osSrcFilename.c_str();
        }

        // Checks that file is not already in tileindex.
        if (oSetExistingFiles.find(osFileNameToWrite) !=
            oSetExistingFiles.end())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "File %s is already in tileindex. Skipping it.",
                     osFileNameToWrite.c_str());
            continue;
        }

        auto poSrcDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            osSrcFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
            nullptr, nullptr, nullptr));
        if (poSrcDS == nullptr)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unable to open %s, skipping.", osSrcFilename.c_str());
            continue;
        }

        double adfGeoTransform[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        if (poSrcDS->GetGeoTransform(adfGeoTransform) != CE_None)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "It appears no georeferencing is available for\n"
                     "`%s', skipping.",
                     osSrcFilename.c_str());
            continue;
        }

        auto poSrcSRS = poSrcDS->GetSpatialRef();
        // If not set target srs, test that the current file uses same
        // projection as others.
        if (oTargetSRS.IsEmpty())
        {
            if (!oAlreadyExistingSRS.IsEmpty())
            {
                if (poSrcSRS == nullptr ||
                    !poSrcSRS->IsSame(&oAlreadyExistingSRS))
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "%s is not using the same projection system "
                        "as other files in the tileindex.\n"
                        "This may cause problems when using it in MapServer "
                        "for example.\n"
                        "Use -t_srs option to set target projection system. %s",
                        osSrcFilename.c_str(),
                        psOptions->bSkipDifferentProjection
                            ? "Skipping this file."
                            : "");
                    if (psOptions->bSkipDifferentProjection)
                    {
                        continue;
                    }
                }
            }
            else
            {
                if (poSrcSRS)
                    oAlreadyExistingSRS = *poSrcSRS;
            }
        }

        const int nXSize = poSrcDS->GetRasterXSize();
        const int nYSize = poSrcDS->GetRasterYSize();
        if (nXSize == 0 || nYSize == 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s has 0 width or height. Skipping",
                     osSrcFilename.c_str());
            continue;
        }

        double adfX[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
        double adfY[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
        adfX[0] = adfGeoTransform[0] + 0 * adfGeoTransform[1] +
                  0 * adfGeoTransform[2];
        adfY[0] = adfGeoTransform[3] + 0 * adfGeoTransform[4] +
                  0 * adfGeoTransform[5];

        adfX[1] = adfGeoTransform[0] + nXSize * adfGeoTransform[1] +
                  0 * adfGeoTransform[2];
        adfY[1] = adfGeoTransform[3] + nXSize * adfGeoTransform[4] +
                  0 * adfGeoTransform[5];

        adfX[2] = adfGeoTransform[0] + nXSize * adfGeoTransform[1] +
                  nYSize * adfGeoTransform[2];
        adfY[2] = adfGeoTransform[3] + nXSize * adfGeoTransform[4] +
                  nYSize * adfGeoTransform[5];

        adfX[3] = adfGeoTransform[0] + 0 * adfGeoTransform[1] +
                  nYSize * adfGeoTransform[2];
        adfY[3] = adfGeoTransform[3] + 0 * adfGeoTransform[4] +
                  nYSize * adfGeoTransform[5];

        adfX[4] = adfGeoTransform[0] + 0 * adfGeoTransform[1] +
                  0 * adfGeoTransform[2];
        adfY[4] = adfGeoTransform[3] + 0 * adfGeoTransform[4] +
                  0 * adfGeoTransform[5];

        // If set target srs, do the forward transformation of all points.
        if (!oTargetSRS.IsEmpty() && poSrcSRS)
        {
            if (!poSrcSRS->IsSame(&oTargetSRS))
            {
                auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                    OGRCreateCoordinateTransformation(poSrcSRS, &oTargetSRS));
                if (!poCT || !poCT->Transform(5, adfX, adfY, nullptr))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "unable to transform points from source "
                             "SRS `%s' to target SRS `%s' for file `%s' - file "
                             "skipped",
                             poSrcDS->GetProjectionRef(),
                             psOptions->osTargetSRS.c_str(),
                             osFileNameToWrite.c_str());
                    continue;
                }
            }
        }
        else if (bIsGTIContext && !oAlreadyExistingSRS.IsEmpty() &&
                 (poSrcSRS == nullptr ||
                  !poSrcSRS->IsSame(&oAlreadyExistingSRS)))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "%s is not using the same projection system "
                "as other files in the tileindex. This is not compatible of "
                "GTI use. Use -t_srs option to reproject tile extents "
                "to a common SRS.",
                osSrcFilename.c_str());
            return nullptr;
        }

        const double dfMinX =
            std::min(std::min(adfX[0], adfX[1]), std::min(adfX[2], adfX[3]));
        const double dfMinY =
            std::min(std::min(adfY[0], adfY[1]), std::min(adfY[2], adfY[3]));
        const double dfMaxX =
            std::max(std::max(adfX[0], adfX[1]), std::max(adfX[2], adfX[3]));
        const double dfMaxY =
            std::max(std::max(adfY[0], adfY[1]), std::max(adfY[2], adfY[3]));
        const double dfRes =
            sqrt((dfMaxX - dfMinX) * (dfMaxY - dfMinY) / nXSize / nYSize);
        if (!std::isnan(psOptions->dfMinPixelSize) &&
            dfRes < psOptions->dfMinPixelSize)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s has %f as pixel size (< %f). Skipping",
                     osSrcFilename.c_str(), dfRes, psOptions->dfMinPixelSize);
            continue;
        }
        if (!std::isnan(psOptions->dfMaxPixelSize) &&
            dfRes > psOptions->dfMaxPixelSize)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s has %f as pixel size (> %f). Skipping",
                     osSrcFilename.c_str(), dfRes, psOptions->dfMaxPixelSize);
            continue;
        }

        auto poFeature = std::make_unique<OGRFeature>(poLayerDefn);
        poFeature->SetField(ti_field, osFileNameToWrite.c_str());

        if (i_SrcSRSName >= 0 && poSrcSRS)
        {
            const char *pszAuthorityCode = poSrcSRS->GetAuthorityCode(nullptr);
            const char *pszAuthorityName = poSrcSRS->GetAuthorityName(nullptr);
            if (psOptions->eSrcSRSFormat == FORMAT_AUTO)
            {
                if (pszAuthorityName != nullptr && pszAuthorityCode != nullptr)
                {
                    poFeature->SetField(i_SrcSRSName,
                                        CPLSPrintf("%s:%s", pszAuthorityName,
                                                   pszAuthorityCode));
                }
                else if (nMaxFieldSize == 0 ||
                         strlen(poSrcDS->GetProjectionRef()) <=
                             static_cast<size_t>(nMaxFieldSize))
                {
                    poFeature->SetField(i_SrcSRSName,
                                        poSrcDS->GetProjectionRef());
                }
                else
                {
                    char *pszProj4 = nullptr;
                    if (poSrcSRS->exportToProj4(&pszProj4) == OGRERR_NONE)
                    {
                        poFeature->SetField(i_SrcSRSName, pszProj4);
                    }
                    else
                    {
                        poFeature->SetField(i_SrcSRSName,
                                            poSrcDS->GetProjectionRef());
                    }
                    CPLFree(pszProj4);
                }
            }
            else if (psOptions->eSrcSRSFormat == FORMAT_WKT)
            {
                if (nMaxFieldSize == 0 ||
                    strlen(poSrcDS->GetProjectionRef()) <=
                        static_cast<size_t>(nMaxFieldSize))
                {
                    poFeature->SetField(i_SrcSRSName,
                                        poSrcDS->GetProjectionRef());
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot write WKT for file %s as it is too long!",
                             osFileNameToWrite.c_str());
                }
            }
            else if (psOptions->eSrcSRSFormat == FORMAT_PROJ)
            {
                char *pszProj4 = nullptr;
                if (poSrcSRS->exportToProj4(&pszProj4) == OGRERR_NONE)
                {
                    poFeature->SetField(i_SrcSRSName, pszProj4);
                }
                CPLFree(pszProj4);
            }
            else if (psOptions->eSrcSRSFormat == FORMAT_EPSG)
            {
                if (pszAuthorityName != nullptr && pszAuthorityCode != nullptr)
                    poFeature->SetField(i_SrcSRSName,
                                        CPLSPrintf("%s:%s", pszAuthorityName,
                                                   pszAuthorityCode));
            }
        }

        for (const auto &oFetchMD : psOptions->aoFetchMD)
        {
            if (EQUAL(oFetchMD.osRasterItemName.c_str(), "{PIXEL_SIZE}"))
            {
                poFeature->SetField(oFetchMD.osFieldName.c_str(), dfRes);
                continue;
            }

            const char *pszMD =
                poSrcDS->GetMetadataItem(oFetchMD.osRasterItemName.c_str());
            if (pszMD)
            {
                if (EQUAL(oFetchMD.osRasterItemName.c_str(),
                          "TIFFTAG_DATETIME"))
                {
                    int nYear, nMonth, nDay, nHour, nMin, nSec;
                    if (sscanf(pszMD, "%04d:%02d:%02d %02d:%02d:%02d", &nYear,
                               &nMonth, &nDay, &nHour, &nMin, &nSec) == 6)
                    {
                        poFeature->SetField(
                            oFetchMD.osFieldName.c_str(),
                            CPLSPrintf("%04d/%02d/%02d %02d:%02d:%02d", nYear,
                                       nMonth, nDay, nHour, nMin, nSec));
                        continue;
                    }
                }
                poFeature->SetField(oFetchMD.osFieldName.c_str(), pszMD);
            }
        }

        auto poPoly = std::make_unique<OGRPolygon>();
        auto poRing = std::make_unique<OGRLinearRing>();
        for (int k = 0; k < 5; k++)
            poRing->addPoint(adfX[k], adfY[k]);
        poPoly->addRing(std::move(poRing));
        poFeature->SetGeometryDirectly(poPoly.release());

        if (poLayer->CreateFeature(poFeature.get()) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create feature in tile index.");
            return nullptr;
        }

        ++iCur;
        if (psOptions->pfnProgress &&
            !psOptions->pfnProgress(static_cast<double>(iCur) / nTotal, "",
                                    psOptions->pProgressData))
        {
            return nullptr;
        }
        if (iCur >= nSrcCount)
            ++nTotal;
    }
    if (psOptions->pfnProgress)
        psOptions->pfnProgress(1.0, "", psOptions->pProgressData);

    if (poTileIndexDSUnique)
        return GDALDataset::ToHandle(poTileIndexDSUnique.release());
    else
        return GDALDataset::ToHandle(poTileIndexDS);
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

static char *SanitizeSRS(const char *pszUserInput)

{
    OGRSpatialReferenceH hSRS;
    char *pszResult = nullptr;

    CPLErrorReset();

    hSRS = OSRNewSpatialReference(nullptr);
    if (OSRSetFromUserInput(hSRS, pszUserInput) == OGRERR_NONE)
        OSRExportToWkt(hSRS, &pszResult);
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Translating SRS failed:\n%s",
                 pszUserInput);
    }

    OSRDestroySpatialReference(hSRS);

    return pszResult;
}

/************************************************************************/
/*                          GDALTileIndexOptionsNew()                   */
/************************************************************************/

/**
 * Allocates a GDALTileIndexOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdaltindex.html">gdaltindex</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdaltindex_bin.cpp use case) must be allocated with
 * GDALTileIndexOptionsForBinaryNew() prior to this function. Will be filled
 * with potentially present filename, open options,...
 * @return pointer to the allocated GDALTileIndexOptions struct. Must be freed
 * with GDALTileIndexOptionsFree().
 *
 * @since GDAL 3.9
 */

GDALTileIndexOptions *
GDALTileIndexOptionsNew(char **papszArgv,
                        GDALTileIndexOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALTileIndexOptions>();

    /* -------------------------------------------------------------------- */
    /*      Parse arguments.                                                */
    /* -------------------------------------------------------------------- */

    CPLStringList aosArgv;

    if (papszArgv)
    {
        const int nArgc = CSLCount(papszArgv);
        for (int i = 0; i < nArgc; i++)
        {
            aosArgv.AddString(papszArgv[i]);
        }
    }

    try
    {
        auto argParser = GDALTileIndexAppOptionsGetParser(psOptions.get(),
                                                          psOptionsForBinary);
        argParser->parse_args_without_binary_name(aosArgv.List());

        // Check all no store_into args
        if (auto oTr = argParser->present<std::vector<double>>("-tr"))
        {
            psOptions->xres = (*oTr)[0];
            psOptions->yres = (*oTr)[1];
        }

        if (auto oTargetExtent = argParser->present<std::vector<double>>("-te"))
        {
            psOptions->xmin = (*oTargetExtent)[0];
            psOptions->ymin = (*oTargetExtent)[1];
            psOptions->xmax = (*oTargetExtent)[2];
            psOptions->ymax = (*oTargetExtent)[3];
        }

        if (auto fetchMd =
                argParser->present<std::vector<std::string>>("-fetch_md"))
        {

            CPLAssert(fetchMd->size() % 3 == 0);

            // Loop
            for (size_t i = 0; i < fetchMd->size(); i += 3)
            {
                OGRFieldType type;
                const auto &typeName{fetchMd->at(i + 2)};
                if (typeName == "String")
                {
                    type = OFTString;
                }
                else if (typeName == "Integer")
                {
                    type = OFTInteger;
                }
                else if (typeName == "Integer64")
                {
                    type = OFTInteger64;
                }
                else if (typeName == "Real")
                {
                    type = OFTReal;
                }
                else if (typeName == "Date")
                {
                    type = OFTDate;
                }
                else if (typeName == "DateTime")
                {
                    type = OFTDateTime;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "-fetch_md requires a valid type name as third "
                             "argument: %s was given.",
                             fetchMd->at(i).c_str());
                    return nullptr;
                }

                const GDALTileIndexRasterMetadata oMD{type, fetchMd->at(i + 1),
                                                      fetchMd->at(i)};
                psOptions->aoFetchMD.push_back(oMD);
            }
        }

        // Check -t_srs
        if (!psOptions->osTargetSRS.empty())
        {
            auto sanitized{SanitizeSRS(psOptions->osTargetSRS.c_str())};
            if (sanitized)
            {
                psOptions->osTargetSRS = sanitized;
                CPLFree(sanitized);
            }
            else
            {
                // Error was already reported by SanitizeSRS, just return nullptr
                psOptions->osTargetSRS.clear();
                return nullptr;
            }
        }
    }
    catch (const std::exception &error)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", error.what());
        return nullptr;
    }

    return psOptions.release();
}

/************************************************************************/
/*                        GDALTileIndexOptionsFree()                    */
/************************************************************************/

/**
 * Frees the GDALTileIndexOptions struct.
 *
 * @param psOptions the options struct for GDALTileIndex().
 *
 * @since GDAL 3.9
 */

void GDALTileIndexOptionsFree(GDALTileIndexOptions *psOptions)
{
    delete psOptions;
}

/************************************************************************/
/*                 GDALTileIndexOptionsSetProgress()                    */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALTileIndex().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 3.11
 */

void GDALTileIndexOptionsSetProgress(GDALTileIndexOptions *psOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
}

#undef CHECK_HAS_ENOUGH_ADDITIONAL_ARGS
