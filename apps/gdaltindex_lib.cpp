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
#include "cpl_md5.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi_virtual.h"
#include "gdal_utils.h"
#include "gdal_priv.h"
#include "gdal_utils_priv.h"
#include "ogr_api.h"
#include "ograrrowarrayhelper.h"
#include "ogrsf_frmts.h"
#include "ogr_recordbatch.h"
#include "ogr_spatialref.h"
#include "commonutils.h"
#include "gdalargumentparser.h"

#include <ctype.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

constexpr const char ARROW_FORMAT_INT32[] = "i";
constexpr const char ARROW_FORMAT_FLOAT32[] = "f";
constexpr const char ARROW_FORMAT_FLOAT64[] = "g";
constexpr const char ARROW_FORMAT_STRING[] = "u";
constexpr const char ARROW_FORMAT_BINARY[] = "z";
constexpr const char ARROW_FORMAT_LIST[] = "+l";
constexpr const char ARROW_FORMAT_STRUCT[] = "+s";
constexpr const char GEOPARQUET_GEOM_COL_NAME[] = "geometry";

constexpr int NUM_ITEMS_PROJ_BBOX = 4;
constexpr int NUM_ITEMS_PROJ_SHAPE = 2;
constexpr int NUM_ITEMS_PROJ_TRANSFORM = 9;

constexpr int ARROW_BUF_VALIDITY = 0;
constexpr int ARROW_BUF_DATA = 1;
constexpr int ARROW_BUF_BYTES = 2;

constexpr int COUNT_STAC_EXTENSIONS = 2;

typedef enum
{
    FORMAT_AUTO,
    FORMAT_WKT,
    FORMAT_EPSG,
    FORMAT_PROJ
} SrcSRSFormat;

/************************************************************************/
/*                     GDALTileIndexRasterMetadata                      */
/************************************************************************/

struct GDALTileIndexRasterMetadata
{
    OGRFieldType eType = OFTString;
    std::string osFieldName{};
    std::string osRasterItemName{};
};

/************************************************************************/
/*                         GDALTileIndexOptions                         */
/************************************************************************/

struct GDALTileIndexOptions
{
    bool bInvokedFromGdalRasterIndex = false;
    bool bOverwrite = false;
    bool bSkipErrors = false;
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
    std::string osProfile{};         // Only "STAC-GeoParquet" handled
    std::string osBaseURL{};         // Used for "STAC-GeoParquet"
    std::string osIdMethod{};        // Used for "STAC-GeoParquet"
    std::string osIdMetadataItem{};  // Used for "STAC-GeoParquet"
};

/************************************************************************/
/*                            ReleaseArray()                            */
/************************************************************************/

static void ReleaseArray(struct ArrowArray *array)
{
    CPLAssert(array->release != nullptr);
    if (array->buffers)
    {
        for (int i = 0; i < static_cast<int>(array->n_buffers); ++i)
            VSIFree(const_cast<void *>(array->buffers[i]));
        CPLFree(array->buffers);
    }
    if (array->children)
    {
        for (int i = 0; i < static_cast<int>(array->n_children); ++i)
        {
            if (array->children[i] && array->children[i]->release)
            {
                array->children[i]->release(array->children[i]);
                CPLFree(array->children[i]);
            }
        }
        CPLFree(array->children);
    }
    array->release = nullptr;
}

/************************************************************************/
/*                  GDALTileIndexAppOptionsGetParser()                  */
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

    // Hidden as used by gdal raster index
    argParser->add_argument("--invoked-from-gdal-raster-index")
        .store_into(psOptions->bInvokedFromGdalRasterIndex)
        .hidden();

    // Hidden as used by gdal raster index
    argParser->add_argument("-skip_errors")
        .store_into(psOptions->bSkipErrors)
        .hidden();

    // Hidden as used by gdal raster index
    argParser->add_argument("-profile")
        .store_into(psOptions->osProfile)
        .hidden();

    // Hidden as used by gdal raster index
    argParser->add_argument("--base-url")
        .store_into(psOptions->osBaseURL)
        .hidden();

    // Hidden as used by gdal raster index
    argParser->add_argument("--id-method")
        .store_into(psOptions->osIdMethod)
        .hidden();

    // Hidden as used by gdal raster index
    argParser->add_argument("--id-metadata-item")
        .store_into(psOptions->osIdMetadataItem)
        .hidden();

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
/*                   GDALTileIndexAppGetParserUsage()                   */
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
/*                      GDALTileIndexTileIterator                       */
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
                const char *pszCurName = papszSrcDSNames[iCurSrc++];
                if (VSIStatL(pszCurName, &sStatBuf) == 0 &&
                    VSI_ISDIR(sStatBuf.st_mode))
                {
                    auto poSrcDS = std::unique_ptr<GDALDataset>(
                        GDALDataset::Open(pszCurName, GDAL_OF_RASTER, nullptr,
                                          nullptr, nullptr));
                    if (poSrcDS)
                        return pszCurName;

                    osCurDir = pszCurName;
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
                    return pszCurName;
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
                    if (GDALPatternMatch(osFilenameOnly.c_str(),
                                         osFilter.c_str()))
                    {
                        bMatchFound = true;
                        break;
                    }
                }
                if (!bMatchFound)
                    continue;
            }

            std::string osFilename = CPLFormFilenameSafe(
                osCurDir.c_str(), psEntry->pszName, nullptr);
            if (VSI_ISDIR(psEntry->nMode))
            {
                auto poSrcDS = std::unique_ptr<GDALDataset>(
                    GDALDataset::Open(osFilename.c_str(), GDAL_OF_RASTER,
                                      nullptr, nullptr, nullptr));
                if (poSrcDS)
                {
                    return osFilename;
                }
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

    const bool bIsSTACGeoParquet =
        EQUAL(psOptions->osProfile.c_str(), "STAC-GeoParquet");

    auto poOutDrv = poTileIndexDS->GetDriver();
    if (osFormat.empty() && poOutDrv)
        osFormat = poOutDrv->GetDescription();

    const char *pszVal =
        poOutDrv ? poOutDrv->GetMetadataItem(GDAL_DMD_MAX_STRING_LENGTH)
                 : nullptr;
    const int nMaxFieldSize = pszVal ? atoi(pszVal) : 0;

    const bool bFailOnErrors =
        psOptions->bInvokedFromGdalRasterIndex && !psOptions->bSkipErrors;
    bool bSkipFirstTile = false;

    // Configurable mostly/only for autotest purposes.
    const int nMaxBatchSize = std::max(
        1, atoi(CPLGetConfigOption("GDAL_RASTER_INDEX_BATCH_SIZE", "65536")));

    std::vector<ArrowSchema> topSchemas;
    std::vector<ArrowSchema *> topSchemasPointers;
    std::vector<std::unique_ptr<ArrowSchema>> auxSchemas;
    std::vector<ArrowSchema *> stacExtensionsSchemaChildren,
        linksSchemaChildren, linksItemSchemaChildren, assetsSchemaChildren,
        imageAssetSchemaChildren, imageAssetRolesSchemaChildren,
        bandsSchemaChildren, bandsItemSchemaChildren, projBboxSchemaChildren,
        projShapeSchemaChildren, projTransformSchemaChildren;
    ArrowSchema topSchema{};
    const auto noop_release = [](struct ArrowSchema *) {};
    const auto AddTopSchema = [&topSchemas, &noop_release]() -> ArrowSchema &
    {
        topSchemas.push_back(ArrowSchema{});
        topSchemas.back().release = noop_release;
        return topSchemas.back();
    };

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
                        if (poTileIndexDS->DeleteLayer(i) != OGRERR_NONE)
                            return nullptr;
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
            std::unique_ptr<CPLTurnFailureIntoWarningBackuper>
                poFailureIntoWarning;
            if (!bFailOnErrors)
                poFailureIntoWarning =
                    std::make_unique<CPLTurnFailureIntoWarningBackuper>();
            CPL_IGNORE_RET_VAL(poFailureIntoWarning);
            auto poSrcDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                osFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                nullptr, nullptr, nullptr));
            if (!poSrcDS)
            {
                CPLError(bFailOnErrors ? CE_Failure : CE_Warning,
                         CPLE_AppDefined, "Unable to open %s%s.",
                         osFilename.c_str(), bFailOnErrors ? "" : ", skipping");
                if (bFailOnErrors)
                    return nullptr;
                bSkipFirstTile = true;
            }
            else
            {
                auto poSrcSRS = poSrcDS->GetSpatialRef();
                if (poSrcSRS)
                    oSRS = *poSrcSRS;
            }
        }

        if (bIsSTACGeoParquet)
        {
            psOptions->aosLCO.SetNameValue("ROW_GROUP_SIZE",
                                           CPLSPrintf("%d", nMaxBatchSize));
            psOptions->aosLCO.SetNameValue("GEOMETRY_ENCODING", "WKB");
            psOptions->aosLCO.SetNameValue("GEOMETRY_NAME",
                                           GEOPARQUET_GEOM_COL_NAME);
            psOptions->aosLCO.SetNameValue("FID", "");
            psOptions->aosLCO.SetNameValue("WRITE_COVERING_BBOX", "YES");
            psOptions->aosLCO.SetNameValue("COVERING_BBOX_NAME", "bbox");
            if (CPLTestBool(
                    psOptions->aosLCO.FetchNameValueDef("SORT_BY_BBOX", "NO")))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "SORT_BY_BBOX=YES is not compatible with "
                         "STAC-GeoParquet profile");
                return nullptr;
            }
        }

        poLayer = poTileIndexDS->CreateLayer(
            osLayerName.c_str(), oSRS.IsEmpty() ? nullptr : &oSRS, wkbPolygon,
            psOptions->aosLCO.List());
        if (!poLayer)
            return nullptr;

        if (bIsSTACGeoParquet)
        {
            const auto AddAuxSchema = [&auxSchemas, &noop_release]()
            {
                auto newSchema = std::make_unique<ArrowSchema>(ArrowSchema{});
                newSchema->release = noop_release;
                auxSchemas.push_back(std::move(newSchema));
                return auxSchemas.back().get();
            };

            // "id" field
            {
                auto &schema = AddTopSchema();
                schema.format = ARROW_FORMAT_STRING;
                schema.name = "id";
                if (!poLayer->CreateFieldFromArrowSchema(&schema))
                    return nullptr;
            }
            // "stac_extensions" field
            {
                auto &schema = AddTopSchema();

                auto &sub_schema = *AddAuxSchema();
                stacExtensionsSchemaChildren.push_back(&sub_schema);

                schema.format = ARROW_FORMAT_LIST;
                schema.name = "stac_extensions";
                schema.n_children = 1;
                schema.children = stacExtensionsSchemaChildren.data();
                sub_schema.format = ARROW_FORMAT_STRING;
                sub_schema.name = "item";
                if (!poLayer->CreateFieldFromArrowSchema(&schema))
                    return nullptr;
            }
            // "links" field
            {
                auto &links = AddTopSchema();

                auto &item = *AddAuxSchema();
                linksSchemaChildren.push_back(&item);

                auto &href = *AddAuxSchema();
                linksItemSchemaChildren.push_back(&href);

                auto &rel = *AddAuxSchema();
                linksItemSchemaChildren.push_back(&rel);

                auto &type = *AddAuxSchema();
                linksItemSchemaChildren.push_back(&type);

                auto &title = *AddAuxSchema();
                linksItemSchemaChildren.push_back(&title);

                links.format = ARROW_FORMAT_LIST;
                links.name = "links";
                links.n_children = linksSchemaChildren.size();
                links.children = linksSchemaChildren.data();

                item.format = ARROW_FORMAT_STRUCT;
                item.name = "item";
                item.n_children = linksItemSchemaChildren.size();
                item.children = linksItemSchemaChildren.data();

                href.format = ARROW_FORMAT_STRING;
                href.name = "href";

                rel.format = ARROW_FORMAT_STRING;
                rel.name = "rel";

                type.format = ARROW_FORMAT_STRING;
                type.name = "type";
                type.flags = ARROW_FLAG_NULLABLE;

                title.format = ARROW_FORMAT_STRING;
                title.name = "title";
                title.flags = ARROW_FLAG_NULLABLE;

                if (!poLayer->CreateFieldFromArrowSchema(&links))
                    return nullptr;
            }

            // "assets" field
            {
                auto &assets = AddTopSchema();

                auto &image = *AddAuxSchema();
                assetsSchemaChildren.push_back(&image);

                auto &href = *AddAuxSchema();
                imageAssetSchemaChildren.push_back(&href);

                auto &roles = *AddAuxSchema();
                imageAssetSchemaChildren.push_back(&roles);

                auto &title = *AddAuxSchema();
                imageAssetSchemaChildren.push_back(&title);

                auto &type = *AddAuxSchema();
                imageAssetSchemaChildren.push_back(&type);

                auto &roles_item = *AddAuxSchema();
                imageAssetRolesSchemaChildren.push_back(&roles_item);

                assets.format = ARROW_FORMAT_STRUCT;
                assets.name = "assets";
                assets.n_children = assetsSchemaChildren.size();
                assets.children = assetsSchemaChildren.data();

                image.format = ARROW_FORMAT_STRUCT;
                image.name = "image";
                image.n_children = imageAssetSchemaChildren.size();
                image.children = imageAssetSchemaChildren.data();

                href.format = ARROW_FORMAT_STRING;
                href.name = "href";

                roles.format = ARROW_FORMAT_LIST;
                roles.name = "roles";
                roles.flags = ARROW_FLAG_NULLABLE;
                roles.n_children = imageAssetRolesSchemaChildren.size();
                roles.children = imageAssetRolesSchemaChildren.data();

                roles_item.format = ARROW_FORMAT_STRING;
                roles_item.name = "item";
                roles_item.flags = ARROW_FLAG_NULLABLE;

                title.format = ARROW_FORMAT_STRING;
                title.name = "title";
                title.flags = ARROW_FLAG_NULLABLE;

                type.format = ARROW_FORMAT_STRING;
                type.name = "type";
                type.flags = ARROW_FLAG_NULLABLE;

                if (!poLayer->CreateFieldFromArrowSchema(&assets))
                    return nullptr;
            }

            // "bands" field
            {
                auto &bands = AddTopSchema();

                auto &bandsItem = *AddAuxSchema();
                bandsSchemaChildren.push_back(&bandsItem);

                bands.format = ARROW_FORMAT_LIST;
                bands.name = "bands";
                bands.n_children = bandsSchemaChildren.size();
                bands.children = bandsSchemaChildren.data();

                auto &name = *AddAuxSchema();
                bandsItemSchemaChildren.push_back(&name);

                auto &commonName = *AddAuxSchema();
                bandsItemSchemaChildren.push_back(&commonName);

                auto &centerWavelength = *AddAuxSchema();
                bandsItemSchemaChildren.push_back(&centerWavelength);

                auto &fullWidthHalfMax = *AddAuxSchema();
                bandsItemSchemaChildren.push_back(&fullWidthHalfMax);

                auto &nodata = *AddAuxSchema();
                bandsItemSchemaChildren.push_back(&nodata);

                auto &data_type = *AddAuxSchema();
                bandsItemSchemaChildren.push_back(&data_type);

                auto &unit = *AddAuxSchema();
                bandsItemSchemaChildren.push_back(&unit);

                bandsItem.format = ARROW_FORMAT_STRUCT;
                bandsItem.name = "item";
                bandsItem.n_children = bandsItemSchemaChildren.size();
                bandsItem.children = bandsItemSchemaChildren.data();

                name.format = ARROW_FORMAT_STRING;
                name.name = "name";

                commonName.format = ARROW_FORMAT_STRING;
                commonName.name = "eo:common_name";
                commonName.flags = ARROW_FLAG_NULLABLE;

                centerWavelength.format = ARROW_FORMAT_FLOAT32;
                centerWavelength.name = "eo:center_wavelength";
                centerWavelength.flags = ARROW_FLAG_NULLABLE;

                fullWidthHalfMax.format = ARROW_FORMAT_FLOAT32;
                fullWidthHalfMax.name = "eo:full_width_half_max";
                fullWidthHalfMax.flags = ARROW_FLAG_NULLABLE;

                nodata.format = ARROW_FORMAT_STRING;
                nodata.name = "nodata";
                nodata.flags = ARROW_FLAG_NULLABLE;

                data_type.format = ARROW_FORMAT_STRING;
                data_type.name = "data_type";

                unit.format = ARROW_FORMAT_STRING;
                unit.name = "unit";
                unit.flags = ARROW_FLAG_NULLABLE;

                if (!poLayer->CreateFieldFromArrowSchema(&bands))
                    return nullptr;
            }

            // "proj:code" field
            {
                auto &schema = AddTopSchema();
                schema.format = ARROW_FORMAT_STRING;
                schema.name = "proj:code";
                schema.flags = ARROW_FLAG_NULLABLE;
                if (!poLayer->CreateFieldFromArrowSchema(&schema))
                    return nullptr;
            }
            // "proj:wkt2" field
            {
                auto &schema = AddTopSchema();
                schema.format = ARROW_FORMAT_STRING;
                schema.name = "proj:wkt2";
                schema.flags = ARROW_FLAG_NULLABLE;
                if (!poLayer->CreateFieldFromArrowSchema(&schema))
                    return nullptr;
            }
            // "proj:projjson" field
            {
                auto &schema = AddTopSchema();
                schema.format = ARROW_FORMAT_STRING;
                schema.name = "proj:projjson";
                // clang-format off
                static const char jsonMetadata[] = {
                    // Number of key/value pairs (uint32)
                    1, 0, 0, 0,
                    // Length of key (uint32)
                    20, 0, 0, 0,
                    'A', 'R', 'R', 'O', 'W', ':',
                    'e', 'x', 't', 'e', 'n', 's', 'i', 'o', 'n', ':',
                    'n', 'a', 'm', 'e',
                    // Length of value (uint32)
                    10, 0, 0, 0,
                    'a', 'r', 'r', 'o', 'w', '.', 'j', 's', 'o', 'n',
                };
                // clang-format on
                schema.metadata = jsonMetadata;
                schema.flags = ARROW_FLAG_NULLABLE;
                if (!poLayer->CreateFieldFromArrowSchema(&schema))
                    return nullptr;
            }
            // "proj:bbox" field
            {
                auto &schema = AddTopSchema();
                static const char FORMAT_PROJ_BBOX[] = {
                    '+', 'w', ':', '0' + NUM_ITEMS_PROJ_BBOX, 0};
                schema.format = FORMAT_PROJ_BBOX;
                schema.name = "proj:bbox";

                auto &sub_schema = *AddAuxSchema();
                projBboxSchemaChildren.push_back(&sub_schema);

                schema.n_children = projBboxSchemaChildren.size();
                schema.children = projBboxSchemaChildren.data();
                sub_schema.format = ARROW_FORMAT_FLOAT64;
                sub_schema.name = "item";

                if (!poLayer->CreateFieldFromArrowSchema(&schema))
                    return nullptr;
            }
            // "proj:shape" field
            {
                auto &schema = AddTopSchema();
                static const char FORMAT_PROJ_SHAPE[] = {
                    '+', 'w', ':', '0' + NUM_ITEMS_PROJ_SHAPE, 0};
                schema.format = FORMAT_PROJ_SHAPE;
                schema.name = "proj:shape";

                auto &sub_schema = *AddAuxSchema();
                projShapeSchemaChildren.push_back(&sub_schema);

                schema.n_children = projShapeSchemaChildren.size();
                schema.children = projShapeSchemaChildren.data();
                sub_schema.format = ARROW_FORMAT_INT32;
                sub_schema.name = "item";

                if (!poLayer->CreateFieldFromArrowSchema(&schema))
                    return nullptr;
            }
            // "proj:transform" field
            {
                auto &schema = AddTopSchema();
                static const char FORMAT_PROJ_TRANSFORM[] = {
                    '+', 'w', ':', '0' + NUM_ITEMS_PROJ_TRANSFORM, 0};
                schema.format = FORMAT_PROJ_TRANSFORM;
                schema.name = "proj:transform";

                auto &sub_schema = *AddAuxSchema();
                projTransformSchemaChildren.push_back(&sub_schema);

                schema.n_children = projTransformSchemaChildren.size();
                schema.children = projTransformSchemaChildren.data();
                sub_schema.format = ARROW_FORMAT_FLOAT64;
                sub_schema.name = "item";

                if (!poLayer->CreateFieldFromArrowSchema(&schema))
                    return nullptr;
            }
        }
        else
        {
            OGRFieldDefn oLocationField(psOptions->osLocationField.c_str(),
                                        OFTString);
            oLocationField.SetWidth(nMaxFieldSize);
            if (poLayer->CreateField(&oLocationField) != OGRERR_NONE)
                return nullptr;
        }

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

    if (!bIsSTACGeoParquet)
    {
        for (const auto &oFetchMD : psOptions->aoFetchMD)
        {
            if (poLayerDefn->GetFieldIndex(oFetchMD.osFieldName.c_str()) < 0)
            {
                OGRFieldDefn oField(oFetchMD.osFieldName.c_str(),
                                    oFetchMD.eType);
                if (poLayer->CreateField(&oField) != OGRERR_NONE)
                    return nullptr;
            }
        }
    }

    if (bIsSTACGeoParquet)
    {
        {
            auto &geometry = AddTopSchema();
            geometry.format = ARROW_FORMAT_BINARY;
            geometry.name = GEOPARQUET_GEOM_COL_NAME;
        }

        for (auto &schema : topSchemas)
            topSchemasPointers.push_back(&schema);

        topSchema.format = ARROW_FORMAT_STRUCT;
        topSchema.name = "main";
        topSchema.release = noop_release;
        topSchema.n_children = topSchemasPointers.size();
        topSchema.children = topSchemasPointers.data();
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
        if (!bIsSTACGeoParquet)
        {
            poLayer->SetMetadataItem("LOCATION_FIELD",
                                     psOptions->osLocationField.c_str());
        }
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
    if (!bIsSTACGeoParquet && ti_field < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find field `%s' in file `%s'.",
                 psOptions->osLocationField.c_str(), pszDest);
        return nullptr;
    }

    int i_SrcSRSName = -1;
    if (!bIsSTACGeoParquet && !psOptions->osSrcSRSFieldName.empty())
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
    ArrowArray topArray{};

    struct TopArrayReleaser
    {
        ArrowArray *m_array = nullptr;

        explicit TopArrayReleaser(ArrowArray *array) : m_array(array)
        {
        }

        ~TopArrayReleaser()
        {
            if (m_array && m_array->release)
                m_array->release(m_array);
        }

        TopArrayReleaser(const TopArrayReleaser &) = delete;
        TopArrayReleaser &operator=(const TopArrayReleaser &) = delete;
    };

    TopArrayReleaser arrayReleaser(&topArray);

    ArrowArray **topArrays = nullptr;

    int iArray = 0;
    const int iIdArray = iArray++;

    const int iStacExtensionsArray = iArray++;
    ArrowArray *stacExtensionSubArray = nullptr;
    uint32_t nStacExtensionSubArrayMaxAlloc = 0;

    const int iLinksArray = iArray++;
    ArrowArray *linksItemArray = nullptr;

    const int iAssetsArray = iArray++;
    ArrowArray *imageArray = nullptr;
    uint32_t nImageHrefArrayMaxAlloc = 0;
    ArrowArray *imageHrefArray = nullptr;
    ArrowArray *imageRoleArray = nullptr;
    ArrowArray *imageRoleItemArray = nullptr;
    uint32_t nImageRoleItemArrayMaxAlloc = 0;
    ArrowArray *imageTitleArray = nullptr;
    uint32_t nImageTitleArrayMaxAlloc = 0;
    ArrowArray *imageTypeArray = nullptr;
    uint32_t nImageTypeArrayMaxAlloc = 0;

    const int iBandsArray = iArray++;
    uint32_t nBandsItemCount = 0;
    uint32_t nBandsItemAlloc = 0;
    ArrowArray *bandsItemArray = nullptr;
    ArrowArray *bandsNameArray = nullptr;
    uint32_t nBandsNameArrayMaxAlloc = 0;
    ArrowArray *bandsCommonNameArray = nullptr;
    uint32_t nBandsCommonNameArrayMaxAlloc = 0;
    ArrowArray *bandsCenterWavelengthArray = nullptr;
    uint32_t nBandsCenterWavelengthArrayMaxAlloc = 0;
    ArrowArray *bandsFWHMArray = nullptr;
    uint32_t nBandsFWHMArrayMaxAlloc = 0;
    ArrowArray *bandsNodataArray = nullptr;
    uint32_t nBandsNodataArrayMaxAlloc = 0;
    ArrowArray *bandsDataTypeArray = nullptr;
    uint32_t nBandsDataTypeArrayMaxAlloc = 0;
    ArrowArray *bandsUnitArray = nullptr;
    uint32_t nBandsUnitArrayMaxAlloc = 0;

    const int iProjCode = iArray++;
    uint32_t nProjCodeArrayMaxAlloc = 0;
    const int iProjWKT2 = iArray++;
    uint32_t nProjWKT2ArrayMaxAlloc = 0;
    const int iProjPROJJSON = iArray++;
    uint32_t nProjPROJJSONArrayMaxAlloc = 0;
    const int iProjBBOX = iArray++;
    ArrowArray *projBBOXItems = nullptr;
    const int iProjShape = iArray++;
    ArrowArray *projShapeItems = nullptr;
    const int iProjTransform = iArray++;
    ArrowArray *projTransformItems = nullptr;

    const int iWkbArray = iArray++;

    std::unique_ptr<OGRArrowArrayHelper> arrayHelper;

    const auto InitTopArray =
        [iIdArray, iStacExtensionsArray, iLinksArray, iAssetsArray, iBandsArray,
         iProjCode, iProjWKT2, iProjPROJJSON, iProjBBOX, iProjShape,
         iProjTransform, iWkbArray, nMaxBatchSize, &arrayHelper, &topArray,
         &topArrays, &topSchema, &nStacExtensionSubArrayMaxAlloc,
         &stacExtensionSubArray, &linksItemArray, &imageArray,
         &nImageHrefArrayMaxAlloc, &imageHrefArray, &imageRoleArray,
         &imageRoleItemArray, &nImageRoleItemArrayMaxAlloc, &imageTitleArray,
         &imageTypeArray, &nImageTitleArrayMaxAlloc, &nImageTypeArrayMaxAlloc,
         &nBandsItemCount, &nBandsItemAlloc, &bandsItemArray, &bandsNameArray,
         &nBandsNameArrayMaxAlloc, &bandsCommonNameArray,
         &nBandsCommonNameArrayMaxAlloc, &nBandsCenterWavelengthArrayMaxAlloc,
         &bandsCenterWavelengthArray, &nBandsFWHMArrayMaxAlloc, &bandsFWHMArray,
         &bandsNodataArray, &nBandsNodataArrayMaxAlloc, &bandsDataTypeArray,
         &nBandsDataTypeArrayMaxAlloc, &bandsUnitArray,
         &nBandsUnitArrayMaxAlloc, &nProjCodeArrayMaxAlloc,
         &nProjWKT2ArrayMaxAlloc, &nProjPROJJSONArrayMaxAlloc, &projBBOXItems,
         &projShapeItems, &projTransformItems]()
    {
        const auto AllocArray = []()
        {
            auto array =
                static_cast<ArrowArray *>(CPLCalloc(1, sizeof(ArrowArray)));
            array->release = ReleaseArray;
            return array;
        };

        const auto AllocNBuffers = [](ArrowArray &array, int n_buffers)
        {
            array.n_buffers = n_buffers;
            array.buffers = static_cast<const void **>(
                CPLCalloc(n_buffers, sizeof(const void *)));
        };

        const auto AllocNArrays = [](ArrowArray &array, int n_children)
        {
            array.n_children = n_children;
            array.children = static_cast<ArrowArray **>(
                CPLCalloc(n_children, sizeof(ArrowArray *)));
        };

        const auto InitializePrimitiveArray =
            [&AllocNBuffers](ArrowArray &sArray, size_t nEltSize,
                             size_t nLength)
        {
            AllocNBuffers(sArray, 2);
            sArray.buffers[ARROW_BUF_DATA] =
                static_cast<const void *>(CPLCalloc(nLength, nEltSize));
        };

        const auto InitializeStringOrBinaryArray =
            [&AllocNBuffers](ArrowArray &sArray, size_t nLength)
        {
            AllocNBuffers(sArray, 3);
            // +1 since the length of string of idx i is given by
            // offset[i+1] - offset[i]
            sArray.buffers[ARROW_BUF_DATA] = static_cast<const void *>(
                CPLCalloc(nLength + 1, sizeof(uint32_t)));
            // Allocate a minimum amount to not get a null pointer
            sArray.buffers[ARROW_BUF_BYTES] =
                static_cast<const void *>(CPLCalloc(1, 1));
        };

        const auto InitializeListArray =
            [&AllocNBuffers, &AllocNArrays](
                ArrowArray &sArray, ArrowArray *subArray, size_t nLength)
        {
            AllocNBuffers(sArray, 2);
            sArray.buffers[ARROW_BUF_DATA] = static_cast<const void *>(
                CPLCalloc(nLength + 1, sizeof(uint32_t)));
            AllocNArrays(sArray, 1);
            sArray.children[0] = subArray;
        };

        const auto InitializeFixedSizeListArray =
            [&AllocNBuffers,
             &AllocNArrays](ArrowArray &sArray, ArrowArray *subArray,
                            size_t nItemSize, size_t nItemCount, size_t nLength)
        {
            AllocNArrays(sArray, 1);
            AllocNBuffers(sArray, 1);
            sArray.children[0] = subArray;
            AllocNBuffers(*subArray, 2);
            subArray->buffers[ARROW_BUF_DATA] = static_cast<const void *>(
                CPLCalloc(nItemCount * nItemSize, nLength));
        };

        const auto InitializeStructArray = [&AllocNBuffers](ArrowArray &sArray)
        { AllocNBuffers(sArray, 1); };

        topArrays = static_cast<ArrowArray **>(CPLCalloc(
            static_cast<int>(topSchema.n_children), sizeof(ArrowArray *)));
        for (int i = 0; i < static_cast<int>(topSchema.n_children); ++i)
            topArrays[i] = AllocArray();

        topArray = ArrowArray{};
        topArray.release = ReleaseArray;
        topArray.n_children = topSchema.n_children;
        topArray.children = topArrays;
        InitializeStructArray(topArray);

        InitializeStringOrBinaryArray(*topArrays[iIdArray], nMaxBatchSize);

        stacExtensionSubArray = AllocArray();
        nStacExtensionSubArrayMaxAlloc = 0;
        {
            auto *array = topArrays[iStacExtensionsArray];
            InitializeListArray(*array, stacExtensionSubArray, nMaxBatchSize);
            InitializeStringOrBinaryArray(
                *stacExtensionSubArray, COUNT_STAC_EXTENSIONS * nMaxBatchSize);
        }

        linksItemArray = AllocArray();
        {
            auto *array = topArrays[iLinksArray];
            InitializeListArray(*array, linksItemArray, nMaxBatchSize);
            InitializeStructArray(*linksItemArray);

            AllocNArrays(*linksItemArray, 4);
            ArrowArray *linksHrefArray = AllocArray();
            ArrowArray *linksRelArray = AllocArray();
            ArrowArray *linksTypeArray = AllocArray();
            ArrowArray *linksTitleArray = AllocArray();
            linksItemArray->children[0] = linksHrefArray;
            linksItemArray->children[1] = linksRelArray;
            linksItemArray->children[2] = linksTypeArray;
            linksItemArray->children[3] = linksTitleArray;
            InitializeStringOrBinaryArray(*linksHrefArray, nMaxBatchSize);
            InitializeStringOrBinaryArray(*linksRelArray, nMaxBatchSize);
            InitializeStringOrBinaryArray(*linksTypeArray, nMaxBatchSize);
            InitializeStringOrBinaryArray(*linksTitleArray, nMaxBatchSize);
        }

        imageArray = AllocArray();
        nImageHrefArrayMaxAlloc = 0;
        imageHrefArray = AllocArray();
        imageRoleArray = AllocArray();
        imageRoleItemArray = AllocArray();
        nImageHrefArrayMaxAlloc = 0;
        nImageRoleItemArrayMaxAlloc = 0;
        imageTitleArray = AllocArray();
        nImageTitleArrayMaxAlloc = 0;
        imageTypeArray = AllocArray();
        nImageTypeArrayMaxAlloc = 0;
        {
            auto *assets = topArrays[iAssetsArray];
            InitializeStructArray(*assets);
            AllocNArrays(*assets, 1);
            assets->children[0] = imageArray;

            InitializeStructArray(*imageArray);
            AllocNArrays(*imageArray, 4);
            imageArray->children[0] = imageHrefArray;
            imageArray->children[1] = imageRoleArray;
            imageArray->children[2] = imageTitleArray;
            imageArray->children[3] = imageTypeArray;

            InitializeStringOrBinaryArray(*imageHrefArray, nMaxBatchSize);
            InitializeStringOrBinaryArray(*imageTitleArray, nMaxBatchSize);
            InitializeStringOrBinaryArray(*imageTypeArray, nMaxBatchSize);
            InitializeListArray(*imageRoleArray, imageRoleItemArray,
                                nMaxBatchSize);
            InitializeStringOrBinaryArray(*imageRoleItemArray, nMaxBatchSize);
        }

        // "bands" related initialization
        {
            nBandsItemCount = 0;
            nBandsItemAlloc = 0;
            bandsItemArray = AllocArray();
            InitializeListArray(*(topArrays[iBandsArray]), bandsItemArray,
                                nMaxBatchSize);
            InitializeStructArray(*bandsItemArray);

            bandsNameArray = AllocArray();
            InitializeStringOrBinaryArray(*bandsNameArray, 0);
            nBandsNameArrayMaxAlloc = 0;

            bandsCommonNameArray = AllocArray();
            InitializeStringOrBinaryArray(*bandsCommonNameArray, 0);
            nBandsCommonNameArrayMaxAlloc = 0;

            bandsCenterWavelengthArray = AllocArray();
            InitializePrimitiveArray(*bandsCenterWavelengthArray, sizeof(float),
                                     1);
            nBandsCenterWavelengthArrayMaxAlloc = 0;

            bandsFWHMArray = AllocArray();
            InitializePrimitiveArray(*bandsFWHMArray, sizeof(float), 1);
            nBandsFWHMArrayMaxAlloc = 0;

            bandsNodataArray = AllocArray();
            InitializeStringOrBinaryArray(*bandsNodataArray, 0);
            nBandsNodataArrayMaxAlloc = 0;

            bandsDataTypeArray = AllocArray();
            InitializeStringOrBinaryArray(*bandsDataTypeArray, 0);
            nBandsDataTypeArrayMaxAlloc = 0;

            bandsUnitArray = AllocArray();
            InitializeStringOrBinaryArray(*bandsUnitArray, 0);
            nBandsUnitArrayMaxAlloc = 0;

            AllocNArrays(*bandsItemArray, 7);
            bandsItemArray->children[0] = bandsNameArray;
            bandsItemArray->children[1] = bandsCommonNameArray;
            bandsItemArray->children[2] = bandsCenterWavelengthArray;
            bandsItemArray->children[3] = bandsFWHMArray;
            bandsItemArray->children[4] = bandsNodataArray;
            bandsItemArray->children[5] = bandsDataTypeArray;
            bandsItemArray->children[6] = bandsUnitArray;
        }

        // proj:xxxx related initializations
        {
            InitializeStringOrBinaryArray(*topArrays[iProjCode], nMaxBatchSize);
            nProjCodeArrayMaxAlloc = 0;
            InitializeStringOrBinaryArray(*topArrays[iProjWKT2], nMaxBatchSize);
            nProjWKT2ArrayMaxAlloc = 0;
            InitializeStringOrBinaryArray(*topArrays[iProjPROJJSON],
                                          nMaxBatchSize);
            nProjPROJJSONArrayMaxAlloc = 0;

            projBBOXItems = AllocArray();
            InitializeFixedSizeListArray(*(topArrays[iProjBBOX]), projBBOXItems,
                                         sizeof(double), NUM_ITEMS_PROJ_BBOX,
                                         nMaxBatchSize);

            projShapeItems = AllocArray();
            InitializeFixedSizeListArray(*(topArrays[iProjShape]),
                                         projShapeItems, sizeof(int32_t),
                                         NUM_ITEMS_PROJ_SHAPE, nMaxBatchSize);

            projTransformItems = AllocArray();
            InitializeFixedSizeListArray(
                *(topArrays[iProjTransform]), projTransformItems,
                sizeof(double), NUM_ITEMS_PROJ_TRANSFORM, nMaxBatchSize);
        }

        InitializeStringOrBinaryArray(*topArrays[iWkbArray], nMaxBatchSize);

        arrayHelper =
            std::make_unique<OGRArrowArrayHelper>(&topArray, nMaxBatchSize);
    };

    int nBatchSize = 0;

    const auto FlushArrays = [poLayer, &topArray, &linksItemArray, &imageArray,
                              &topSchema, &nBatchSize, &arrayHelper]()
    {
        topArray.length = nBatchSize;
        linksItemArray->length = nBatchSize;
        imageArray->length = nBatchSize;
        for (int i = 0; i < static_cast<int>(topArray.n_children); ++i)
            topArray.children[i]->length = nBatchSize;
        const bool ret = poLayer->WriteArrowBatch(&topSchema, &topArray);
        if (topArray.release)
        {
            topArray.release(&topArray);
        }
        memset(&topArray, 0, sizeof(topArray));
        nBatchSize = 0;
        arrayHelper.reset();
        return ret;
    };

    int iCur = 0;
    int nTotal = nSrcCount + 1;
    while (true)
    {
        const std::string osSrcFilename = oGDALTileIndexTileIterator.next();
        if (osSrcFilename.empty())
            break;
        if (bSkipFirstTile)
        {
            bSkipFirstTile = false;
            continue;
        }

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

        std::unique_ptr<GDALDataset> poSrcDS;
        {
            std::unique_ptr<CPLTurnFailureIntoWarningBackuper>
                poFailureIntoWarning;
            if (!bFailOnErrors)
                poFailureIntoWarning =
                    std::make_unique<CPLTurnFailureIntoWarningBackuper>();
            CPL_IGNORE_RET_VAL(poFailureIntoWarning);

            poSrcDS.reset(GDALDataset::Open(
                osSrcFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                nullptr, nullptr, nullptr));
            if (poSrcDS == nullptr)
            {
                CPLError(bFailOnErrors ? CE_Failure : CE_Warning,
                         CPLE_AppDefined, "Unable to open %s%s.",
                         osSrcFilename.c_str(),
                         bFailOnErrors ? "" : ", skipping");
                if (bFailOnErrors)
                    return nullptr;
                continue;
            }
        }

        GDALGeoTransform gt;
        if (poSrcDS->GetGeoTransform(gt) != CE_None)
        {
            CPLError(bFailOnErrors ? CE_Failure : CE_Warning, CPLE_AppDefined,
                     "It appears no georeferencing is available for\n"
                     "`%s'%s.",
                     osSrcFilename.c_str(), bFailOnErrors ? "" : ", skipping");
            if (bFailOnErrors)
                return nullptr;
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
            CPLError(bFailOnErrors ? CE_Failure : CE_Warning, CPLE_AppDefined,
                     "%s has 0 width or height%s", osSrcFilename.c_str(),
                     bFailOnErrors ? "" : ", skipping");
            if (bFailOnErrors)
                return nullptr;
            continue;
        }

        double adfX[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
        double adfY[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
        adfX[0] = gt.xorig + 0 * gt.xscale + 0 * gt.xrot;
        adfY[0] = gt.yorig + 0 * gt.yrot + 0 * gt.yscale;

        adfX[1] = gt.xorig + nXSize * gt.xscale + 0 * gt.xrot;
        adfY[1] = gt.yorig + nXSize * gt.yrot + 0 * gt.yscale;

        adfX[2] = gt.xorig + nXSize * gt.xscale + nYSize * gt.xrot;
        adfY[2] = gt.yorig + nXSize * gt.yrot + nYSize * gt.yscale;

        adfX[3] = gt.xorig + 0 * gt.xscale + nYSize * gt.xrot;
        adfY[3] = gt.yorig + 0 * gt.yrot + nYSize * gt.yscale;

        adfX[4] = gt.xorig + 0 * gt.xscale + 0 * gt.xrot;
        adfY[4] = gt.yorig + 0 * gt.yrot + 0 * gt.yscale;

        const double dfMinXBeforeReproj =
            std::min(std::min(adfX[0], adfX[1]), std::min(adfX[2], adfX[3]));
        const double dfMinYBeforeReproj =
            std::min(std::min(adfY[0], adfY[1]), std::min(adfY[2], adfY[3]));
        const double dfMaxXBeforeReproj =
            std::max(std::max(adfX[0], adfX[1]), std::max(adfX[2], adfX[3]));
        const double dfMaxYBeforeReproj =
            std::max(std::max(adfY[0], adfY[1]), std::max(adfY[2], adfY[3]));

        // If set target srs, do the forward transformation of all points.
        if (!oTargetSRS.IsEmpty() && poSrcSRS)
        {
            if (!poSrcSRS->IsSame(&oTargetSRS))
            {
                auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                    OGRCreateCoordinateTransformation(poSrcSRS, &oTargetSRS));
                if (!poCT || !poCT->Transform(5, adfX, adfY, nullptr))
                {
                    CPLError(bFailOnErrors ? CE_Failure : CE_Warning,
                             CPLE_AppDefined,
                             "unable to transform points from source "
                             "SRS `%s' to target SRS `%s' for file `%s'%s",
                             poSrcDS->GetProjectionRef(),
                             psOptions->osTargetSRS.c_str(),
                             osFileNameToWrite.c_str(),
                             bFailOnErrors ? "" : ", skipping");
                    if (bFailOnErrors)
                        return nullptr;
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

        auto poPoly = std::make_unique<OGRPolygon>();
        auto poRing = std::make_unique<OGRLinearRing>();
        for (int k = 0; k < 5; k++)
            poRing->addPoint(adfX[k], adfY[k]);
        poPoly->addRing(std::move(poRing));

        if (bIsSTACGeoParquet)
        {
            const char *pszDriverName = poSrcDS->GetDriverName();
            if (pszDriverName && EQUAL(pszDriverName, "MEM"))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Memory datasets cannot be referenced in a "
                         "STAC-GeoParquet catalog");
                return nullptr;
            }

            if (!arrayHelper)
            {
                InitTopArray();
            }

            // Write "id"
            {
                std::string osId(CPLGetFilename(osFileNameToWrite.c_str()));

                if (psOptions->osIdMethod == "md5")
                {
                    const std::string osFilename =
                        VSIFileManager::GetHandler(osFileNameToWrite.c_str())
                            ->GetStreamingFilename(osFileNameToWrite);
                    auto fp = VSIFilesystemHandler::OpenStatic(
                        osFilename.c_str(), "rb");
                    if (!fp)
                    {
                        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                                 osFileNameToWrite.c_str());
                        return nullptr;
                    }
                    CPLMD5Context md5Context;
                    CPLMD5Init(&md5Context);
                    constexpr size_t CHUNK_SIZE = 1024 * 1024;
                    std::vector<GByte> buffer(CHUNK_SIZE, 0);
                    while (true)
                    {
                        const size_t nRead =
                            fp->Read(buffer.data(), 1, buffer.size());
                        CPLMD5Update(&md5Context, buffer.data(), nRead);
                        if (nRead < buffer.size())
                        {
                            if (fp->Error())
                            {
                                CPLError(CE_Failure, CPLE_FileIO,
                                         "Error while reading %s",
                                         osFileNameToWrite.c_str());
                                return nullptr;
                            }
                            break;
                        }
                    }
                    unsigned char digest[16] = {0};
                    CPLMD5Final(digest, &md5Context);
                    char *pszMD5 = CPLBinaryToHex(16, digest);
                    osId = pszMD5;
                    CPLFree(pszMD5);
                    osId += '-';
                    osId += CPLGetFilename(osFileNameToWrite.c_str());
                }
                else if (psOptions->osIdMethod == "metadata-item")
                {
                    const char *pszId = poSrcDS->GetMetadataItem(
                        psOptions->osIdMetadataItem.c_str());
                    if (!pszId)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "No metadata item '%s' in dataset %s",
                                 psOptions->osIdMetadataItem.c_str(),
                                 osFileNameToWrite.c_str());
                        return nullptr;
                    }
                    osId = pszId;
                }
                else if (psOptions->osIdMethod != "filename")
                {
                    // shouldn't happen
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Unhandled id method '%s'",
                             psOptions->osIdMethod.c_str());
                    return nullptr;
                }

                void *ptr = arrayHelper->GetPtrForStringOrBinary(
                    iIdArray, nBatchSize, osId.size(), false);
                if (!ptr)
                    return nullptr;
                memcpy(ptr, osId.data(), osId.size());
            }

            // Write "stac_extensions"
            {
                uint32_t *panOffsets = static_cast<uint32_t *>(
                    const_cast<void *>(topArrays[iStacExtensionsArray]
                                           ->buffers[ARROW_BUF_DATA]));
                panOffsets[nBatchSize + 1] =
                    panOffsets[nBatchSize] + COUNT_STAC_EXTENSIONS;

                {
                    constexpr const char extension[] =
                        "https://stac-extensions.github.io/projection/v2.0.0/"
                        "schema.json";
                    constexpr size_t nStrLen = sizeof(extension) - 1;
                    void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                        stacExtensionSubArray,
                        COUNT_STAC_EXTENSIONS * nBatchSize + 0, nStrLen,
                        nStacExtensionSubArrayMaxAlloc, false);
                    if (!ptr)
                        return nullptr;
                    memcpy(ptr, extension, nStrLen);
                    stacExtensionSubArray->length++;
                }

                {
                    constexpr const char extension[] =
                        "https://stac-extensions.github.io/eo/v2.0.0/"
                        "schema.json";
                    constexpr size_t nStrLen = sizeof(extension) - 1;
                    void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                        stacExtensionSubArray,
                        COUNT_STAC_EXTENSIONS * nBatchSize + 1, nStrLen,
                        nStacExtensionSubArrayMaxAlloc, false);
                    if (!ptr)
                        return nullptr;
                    memcpy(ptr, extension, nStrLen);
                    stacExtensionSubArray->length++;
                }
            }

            // Write "assets.image.href"
            {
                std::string osHref = osFileNameToWrite;
                CPL_IGNORE_RET_VAL(osFileNameToWrite);
                if (!psOptions->osBaseURL.empty())
                {
                    osHref = CPLFormFilenameSafe(psOptions->osBaseURL.c_str(),
                                                 CPLGetFilename(osHref.c_str()),
                                                 nullptr);
                }
                else if (VSIIsLocal(osHref.c_str()))
                {
                    if (!CPLIsFilenameRelative(osHref.c_str()))
                    {
                        osHref = "file://" + osHref;
                    }
                }
                else if (STARTS_WITH(osHref.c_str(), "/vsicurl/"))
                {
                    osHref = osHref.substr(strlen("/vsicurl/"));
                }
                else if (STARTS_WITH(osHref.c_str(), "/vsis3/"))
                {
                    osHref = "s3://" + osHref.substr(strlen("/vsis3/"));
                }
                else if (STARTS_WITH(osHref.c_str(), "/vsigs/"))
                {
                    osHref = "gcs://" + osHref.substr(strlen("/vsigs/"));
                }
                else if (STARTS_WITH(osHref.c_str(), "/vsiaz/"))
                {
                    osHref = "azure://" + osHref.substr(strlen("/vsiaz/"));
                }
                const size_t nHrefLen = osHref.size();
                void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                    imageHrefArray, nBatchSize, nHrefLen,
                    nImageHrefArrayMaxAlloc, false);
                if (!ptr)
                    return nullptr;
                memcpy(ptr, osHref.data(), nHrefLen);
                imageHrefArray->length++;
            }

            // Write "assets.image.roles"
            {
                uint32_t *panRolesOffsets =
                    static_cast<uint32_t *>(const_cast<void *>(
                        imageRoleArray->buffers[ARROW_BUF_DATA]));
                panRolesOffsets[nBatchSize + 1] =
                    panRolesOffsets[nBatchSize] + 1;

                constexpr const char ROLE_DATA[] = "data";
                constexpr size_t nStrLen = sizeof(ROLE_DATA) - 1;
                void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                    imageRoleItemArray, nBatchSize, nStrLen,
                    nImageRoleItemArrayMaxAlloc, false);
                if (!ptr)
                    return nullptr;
                memcpy(ptr, ROLE_DATA, nStrLen);
                imageRoleItemArray->length++;
            }

            // Write "assets.image.type"
            if (pszDriverName && EQUAL(pszDriverName, "GTiff"))
            {
                const char *pszLayout =
                    poSrcDS->GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE");
                if (pszLayout && EQUAL(pszLayout, "COG"))
                {
                    constexpr const char TYPE[] =
                        "image/tiff; application=geotiff; "
                        "profile=cloud-optimized";
                    constexpr size_t TYPE_SIZE = sizeof(TYPE) - 1;
                    void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                        imageTypeArray, nBatchSize, TYPE_SIZE,
                        nImageTypeArrayMaxAlloc, false);
                    if (!ptr)
                        return nullptr;
                    memcpy(ptr, TYPE, TYPE_SIZE);
                }
                else
                {
                    constexpr const char TYPE[] =
                        "image/tiff; application=geotiff";
                    constexpr size_t TYPE_SIZE = sizeof(TYPE) - 1;
                    void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                        imageTypeArray, nBatchSize, TYPE_SIZE,
                        nImageTypeArrayMaxAlloc, false);
                    if (!ptr)
                        return nullptr;
                    memcpy(ptr, TYPE, TYPE_SIZE);
                }
            }
            else if (pszDriverName && EQUAL(pszDriverName, "PNG"))
            {
                constexpr const char TYPE[] = "image/png";
                constexpr size_t TYPE_SIZE = sizeof(TYPE) - 1;
                void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                    imageTypeArray, nBatchSize, TYPE_SIZE,
                    nImageTypeArrayMaxAlloc, false);
                if (!ptr)
                    return nullptr;
                memcpy(ptr, TYPE, TYPE_SIZE);
            }
            else if (pszDriverName && EQUAL(pszDriverName, "JPEG"))
            {
                constexpr const char TYPE[] = "image/jpeg";
                constexpr size_t TYPE_SIZE = sizeof(TYPE) - 1;
                void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                    imageTypeArray, nBatchSize, TYPE_SIZE,
                    nImageTypeArrayMaxAlloc, false);
                if (!ptr)
                    return nullptr;
                memcpy(ptr, TYPE, TYPE_SIZE);
            }
            else if (pszDriverName && (EQUAL(pszDriverName, "JP2KAK") ||
                                       EQUAL(pszDriverName, "JP2OpenJPEG") ||
                                       EQUAL(pszDriverName, "JP2ECW") ||
                                       EQUAL(pszDriverName, "JP2MrSID")))
            {
                constexpr const char TYPE[] = "image/jp2";
                constexpr size_t TYPE_SIZE = sizeof(TYPE) - 1;
                void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                    imageTypeArray, nBatchSize, TYPE_SIZE,
                    nImageTypeArrayMaxAlloc, false);
                if (!ptr)
                    return nullptr;
                memcpy(ptr, TYPE, TYPE_SIZE);
            }
            else
            {
                OGRArrowArrayHelper::SetNull(imageTypeArray, nBatchSize,
                                             nMaxBatchSize, false);
                OGRArrowArrayHelper::SetEmptyStringOrBinary(imageTypeArray,
                                                            nBatchSize);
            }
            imageTypeArray->length++;

            // Write "assets.image.title"
            {
                OGRArrowArrayHelper::SetNull(imageTitleArray, nBatchSize,
                                             nMaxBatchSize, false);
                OGRArrowArrayHelper::SetEmptyStringOrBinary(imageTitleArray,
                                                            nBatchSize);
                imageTitleArray->length++;
            }

            // Write "bands"
            {
                const int nThisBands = poSrcDS->GetRasterCount();
                if (nThisBands + nBandsItemCount > nBandsItemAlloc)
                {
                    const auto nOldAlloc = nBandsItemAlloc;
                    if (nBandsItemAlloc >
                        std::numeric_limits<uint32_t>::max() / 2)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Too many bands");
                        return nullptr;
                    }
                    nBandsItemAlloc = std::max(2 * nBandsItemAlloc,
                                               nThisBands + nBandsItemCount);

                    auto ReallocArray = [nOldAlloc, nBandsItemAlloc](
                                            ArrowArray *array, size_t nItemSize)
                    {
                        if (array->buffers[ARROW_BUF_VALIDITY])
                        {
                            // Bitmap
                            const uint32_t nNewSizeBytes =
                                (nBandsItemAlloc + 7) / 8;
                            char *newPtr =
                                static_cast<char *>(VSI_REALLOC_VERBOSE(
                                    const_cast<void *>(
                                        array->buffers[ARROW_BUF_VALIDITY]),
                                    nNewSizeBytes));
                            if (!newPtr)
                                return false;
                            array->buffers[ARROW_BUF_VALIDITY] =
                                static_cast<const void *>(
                                    const_cast<const char *>(newPtr));
                            const uint32_t nOldSizeBytes = (nOldAlloc + 7) / 8;
                            if (nNewSizeBytes > nOldSizeBytes)
                            {
                                // Initialize new allocated bytes as valid
                                // They are set invalid explicitly with SetNull()
                                memset(newPtr + nOldSizeBytes, 0xFF,
                                       nNewSizeBytes - nOldSizeBytes);
                            }
                        }
                        char *newPtr = static_cast<char *>(VSI_REALLOC_VERBOSE(
                            const_cast<void *>(array->buffers[ARROW_BUF_DATA]),
                            (nBandsItemAlloc + 1) * nItemSize));
                        if (!newPtr)
                            return false;
                        array->buffers[ARROW_BUF_DATA] =
                            static_cast<const void *>(
                                const_cast<const char *>(newPtr));
                        memset(newPtr + (nOldAlloc + 1) * nItemSize, 0,
                               (nBandsItemAlloc - nOldAlloc) * nItemSize);
                        return true;
                    };

                    if (!ReallocArray(bandsNameArray, sizeof(uint32_t)) ||
                        !ReallocArray(bandsCommonNameArray, sizeof(uint32_t)) ||
                        !ReallocArray(bandsCenterWavelengthArray,
                                      sizeof(float)) ||
                        !ReallocArray(bandsFWHMArray, sizeof(float)) ||
                        !ReallocArray(bandsNodataArray, sizeof(uint32_t)) ||
                        !ReallocArray(bandsDataTypeArray, sizeof(uint32_t)) ||
                        !ReallocArray(bandsUnitArray, sizeof(uint32_t)))
                    {
                        return nullptr;
                    }
                }

                uint32_t *panBandsOffsets =
                    static_cast<uint32_t *>(const_cast<void *>(
                        topArrays[iBandsArray]->buffers[ARROW_BUF_DATA]));
                panBandsOffsets[nBatchSize + 1] =
                    panBandsOffsets[nBatchSize] + nThisBands;

                for (int i = 0; i < nThisBands; ++i, ++nBandsItemCount)
                {
                    bandsItemArray->length++;

                    const auto poBand = poSrcDS->GetRasterBand(i + 1);
                    {
                        std::string osBandName = poBand->GetDescription();
                        if (osBandName.empty())
                            osBandName = "Band " + std::to_string(i + 1);
                        void *ptr =
                            OGRArrowArrayHelper::GetPtrForStringOrBinary(
                                bandsNameArray, nBandsItemCount,
                                osBandName.size(), nBandsNameArrayMaxAlloc,
                                false);
                        if (!ptr)
                            return nullptr;
                        memcpy(ptr, osBandName.data(), osBandName.size());
                        bandsNameArray->length++;
                    }

                    const char *pszCommonName =
                        GDALGetSTACCommonNameFromColorInterp(
                            poBand->GetColorInterpretation());
                    if (pszCommonName)
                    {
                        const size_t nLen = strlen(pszCommonName);
                        void *ptr =
                            OGRArrowArrayHelper::GetPtrForStringOrBinary(
                                bandsCommonNameArray, nBandsItemCount, nLen,
                                nBandsCommonNameArrayMaxAlloc, false);
                        if (!ptr)
                            return nullptr;
                        memcpy(ptr, pszCommonName, nLen);
                    }
                    else
                    {
                        OGRArrowArrayHelper::SetNull(bandsCommonNameArray,
                                                     nBandsItemCount,
                                                     nBandsItemAlloc, false);
                        OGRArrowArrayHelper::SetEmptyStringOrBinary(
                            bandsCommonNameArray, nBandsItemCount);
                    }
                    bandsCommonNameArray->length++;

                    if (const char *pszCenterWavelength =
                            poBand->GetMetadataItem("CENTRAL_WAVELENGTH_UM",
                                                    "IMAGERY"))
                    {
                        float *values = static_cast<float *>(
                            const_cast<void *>(bandsCenterWavelengthArray
                                                   ->buffers[ARROW_BUF_DATA]));
                        values[nBandsItemCount] =
                            static_cast<float>(CPLAtof(pszCenterWavelength));
                    }
                    else
                    {
                        OGRArrowArrayHelper::SetNull(bandsCenterWavelengthArray,
                                                     nBandsItemCount,
                                                     nBandsItemAlloc, false);
                    }
                    bandsCenterWavelengthArray->length++;

                    if (const char *pszFWHM =
                            poBand->GetMetadataItem("FWHM_UM", "IMAGERY"))
                    {
                        float *values = static_cast<float *>(const_cast<void *>(
                            bandsFWHMArray->buffers[ARROW_BUF_DATA]));
                        values[nBandsItemCount] =
                            static_cast<float>(CPLAtof(pszFWHM));
                    }
                    else
                    {
                        OGRArrowArrayHelper::SetNull(bandsFWHMArray,
                                                     nBandsItemCount,
                                                     nBandsItemAlloc, false);
                    }
                    bandsFWHMArray->length++;

                    int bHasNoData = false;
                    const double dfNoDataValue =
                        poBand->GetNoDataValue(&bHasNoData);
                    if (bHasNoData)
                    {
                        const std::string osNodata =
                            std::isnan(dfNoDataValue) ? "nan"
                            : std::isinf(dfNoDataValue)
                                ? (dfNoDataValue > 0 ? "inf" : "-inf")
                                : CPLSPrintf("%.17g", dfNoDataValue);
                        void *ptr =
                            OGRArrowArrayHelper::GetPtrForStringOrBinary(
                                bandsNodataArray, nBandsItemCount,
                                osNodata.size(), nBandsNodataArrayMaxAlloc,
                                false);
                        if (!ptr)
                            return nullptr;
                        memcpy(ptr, osNodata.data(), osNodata.size());
                    }
                    else
                    {
                        OGRArrowArrayHelper::SetNull(bandsNodataArray,
                                                     nBandsItemCount,
                                                     nBandsItemAlloc, false);
                    }
                    bandsNodataArray->length++;

                    {
                        const char *pszDT = "other";
                        // clang-format off
                        switch (poBand->GetRasterDataType())
                        {
                            case GDT_Int8:     pszDT = "int8";     break;
                            case GDT_UInt8:    pszDT = "uint8";    break;
                            case GDT_Int16:    pszDT = "int16";    break;
                            case GDT_UInt16:   pszDT = "uint16";   break;
                            case GDT_Int32:    pszDT = "int32";    break;
                            case GDT_UInt32:   pszDT = "uint32";   break;
                            case GDT_Int64:    pszDT = "int64";    break;
                            case GDT_UInt64:   pszDT = "uint64";   break;
                            case GDT_Float16:  pszDT = "float16";  break;
                            case GDT_Float32:  pszDT = "float32";  break;
                            case GDT_Float64:  pszDT = "float64";  break;
                            case GDT_CInt16:   pszDT = "cint16";   break;
                            case GDT_CInt32:   pszDT = "cint32";   break;
                            case GDT_CFloat16: pszDT = "cfloat16"; break;
                            case GDT_CFloat32: pszDT = "cfloat32"; break;
                            case GDT_CFloat64: pszDT = "cfloat64"; break;
                            case GDT_Unknown:                      break;
                            case GDT_TypeCount:                    break;
                        }
                        // clang-format on
                        const size_t nLen = strlen(pszDT);
                        void *ptr =
                            OGRArrowArrayHelper::GetPtrForStringOrBinary(
                                bandsDataTypeArray, nBandsItemCount, nLen,
                                nBandsDataTypeArrayMaxAlloc, false);
                        if (!ptr)
                            return nullptr;
                        memcpy(ptr, pszDT, nLen);

                        bandsDataTypeArray->length++;
                    }

                    const char *pszUnits = poBand->GetUnitType();
                    if (pszUnits && pszUnits[0])
                    {
                        const size_t nLen = strlen(pszUnits);
                        void *ptr =
                            OGRArrowArrayHelper::GetPtrForStringOrBinary(
                                bandsUnitArray, nBandsItemCount, nLen,
                                nBandsUnitArrayMaxAlloc, false);
                        if (!ptr)
                            return nullptr;
                        memcpy(ptr, pszUnits, nLen);
                    }
                    else
                    {
                        OGRArrowArrayHelper::SetNull(bandsUnitArray,
                                                     nBandsItemCount,
                                                     nBandsItemAlloc, false);
                    }
                    bandsUnitArray->length++;
                }
            }

            // Write "proj:code"
            bool bHasProjCode = false;
            {
                auto psArray = topArrays[iProjCode];
                const char *pszSRSAuthName =
                    poSrcSRS ? poSrcSRS->GetAuthorityName(nullptr) : nullptr;
                const char *pszSRSAuthCode =
                    poSrcSRS ? poSrcSRS->GetAuthorityCode(nullptr) : nullptr;
                if (pszSRSAuthName && pszSRSAuthCode)
                {
                    std::string osCode(pszSRSAuthName);
                    osCode += ':';
                    osCode += pszSRSAuthCode;
                    void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                        psArray, nBatchSize, osCode.size(),
                        nProjCodeArrayMaxAlloc, false);
                    if (!ptr)
                        return nullptr;
                    memcpy(ptr, osCode.data(), osCode.size());
                    bHasProjCode = true;
                }
                else
                {
                    OGRArrowArrayHelper::SetNull(psArray, nBatchSize,
                                                 nMaxBatchSize, false);
                    OGRArrowArrayHelper::SetEmptyStringOrBinary(psArray,
                                                                nBatchSize);
                }
            }

            // Write "proj:wkt2"
            {
                auto psArray = topArrays[iProjWKT2];
                std::string osWKT2;
                if (poSrcSRS && !bHasProjCode)
                {
                    const char *const apszOptions[] = {"FORMAT=WKT2_2019",
                                                       nullptr};
                    osWKT2 = poSrcSRS->exportToWkt(apszOptions);
                }
                if (!osWKT2.empty())
                {
                    void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                        psArray, nBatchSize, osWKT2.size(),
                        nProjWKT2ArrayMaxAlloc, false);
                    if (!ptr)
                        return nullptr;
                    memcpy(ptr, osWKT2.data(), osWKT2.size());
                }
                else
                {
                    OGRArrowArrayHelper::SetNull(psArray, nBatchSize,
                                                 nMaxBatchSize, false);
                    OGRArrowArrayHelper::SetEmptyStringOrBinary(psArray,
                                                                nBatchSize);
                }
            }

            // Write "proj:projjson"
            {
                auto psArray = topArrays[iProjPROJJSON];
                std::string osPROJJSON;
                if (poSrcSRS && !bHasProjCode)
                {
                    char *pszPROJJSON = nullptr;
                    poSrcSRS->exportToPROJJSON(&pszPROJJSON, nullptr);
                    if (pszPROJJSON)
                        osPROJJSON = pszPROJJSON;
                    CPLFree(pszPROJJSON);
                }
                if (!osPROJJSON.empty())
                {
                    void *ptr = OGRArrowArrayHelper::GetPtrForStringOrBinary(
                        psArray, nBatchSize, osPROJJSON.size(),
                        nProjPROJJSONArrayMaxAlloc, false);
                    if (!ptr)
                        return nullptr;
                    memcpy(ptr, osPROJJSON.data(), osPROJJSON.size());
                }
                else
                {
                    OGRArrowArrayHelper::SetNull(psArray, nBatchSize,
                                                 nMaxBatchSize, false);
                    OGRArrowArrayHelper::SetEmptyStringOrBinary(psArray,
                                                                nBatchSize);
                }
            }

            // Write proj:bbox
            {
                double *values = static_cast<double *>(
                    const_cast<void *>(projBBOXItems->buffers[ARROW_BUF_DATA]));
                auto ptr = values + nBatchSize * NUM_ITEMS_PROJ_BBOX;
                ptr[0] = dfMinXBeforeReproj;
                ptr[1] = dfMinYBeforeReproj;
                ptr[2] = dfMaxXBeforeReproj;
                ptr[3] = dfMaxYBeforeReproj;
            }

            // Write proj:shape
            {
                int32_t *values = static_cast<int32_t *>(const_cast<void *>(
                    projShapeItems->buffers[ARROW_BUF_DATA]));
                auto ptr = values + nBatchSize * NUM_ITEMS_PROJ_SHAPE;
                ptr[0] = poSrcDS->GetRasterYSize();
                ptr[1] = poSrcDS->GetRasterXSize();
            }

            // Write proj:transform
            {
                double *values = static_cast<double *>(const_cast<void *>(
                    projTransformItems->buffers[ARROW_BUF_DATA]));
                auto ptr = values + nBatchSize * NUM_ITEMS_PROJ_TRANSFORM;
                ptr[0] = gt.xscale;
                ptr[1] = gt.xrot;
                ptr[2] = gt.xorig;
                ptr[3] = gt.yrot;
                ptr[4] = gt.yscale;
                ptr[5] = gt.yorig;
                ptr[6] = 0;
                ptr[7] = 0;
                ptr[8] = 1;
            }

            // Write geometry
            {
                const size_t nWKBSize = poPoly->WkbSize();
                void *ptr = arrayHelper->GetPtrForStringOrBinary(
                    iWkbArray, nBatchSize, nWKBSize, false);
                if (!ptr)
                    return nullptr;
                OGRwkbExportOptions sExportOptions;
                sExportOptions.eWkbVariant = wkbVariantIso;
                if (poPoly->exportToWkb(static_cast<unsigned char *>(ptr),
                                        &sExportOptions) != OGRERR_NONE)
                    return nullptr;
            }

            nBatchSize++;
            if (nBatchSize == nMaxBatchSize && !FlushArrays())
            {
                return nullptr;
            }
        }
        else
        {
            auto poFeature = std::make_unique<OGRFeature>(poLayerDefn);
            poFeature->SetField(ti_field, osFileNameToWrite.c_str());

            if (i_SrcSRSName >= 0 && poSrcSRS)
            {
                const char *pszAuthorityCode =
                    poSrcSRS->GetAuthorityCode(nullptr);
                const char *pszAuthorityName =
                    poSrcSRS->GetAuthorityName(nullptr);
                if (psOptions->eSrcSRSFormat == FORMAT_AUTO)
                {
                    if (pszAuthorityName != nullptr &&
                        pszAuthorityCode != nullptr)
                    {
                        poFeature->SetField(
                            i_SrcSRSName, CPLSPrintf("%s:%s", pszAuthorityName,
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
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
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
                    if (pszAuthorityName != nullptr &&
                        pszAuthorityCode != nullptr)
                        poFeature->SetField(
                            i_SrcSRSName, CPLSPrintf("%s:%s", pszAuthorityName,
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
                        if (sscanf(pszMD, "%04d:%02d:%02d %02d:%02d:%02d",
                                   &nYear, &nMonth, &nDay, &nHour, &nMin,
                                   &nSec) == 6)
                        {
                            poFeature->SetField(
                                oFetchMD.osFieldName.c_str(),
                                CPLSPrintf("%04d/%02d/%02d %02d:%02d:%02d",
                                           nYear, nMonth, nDay, nHour, nMin,
                                           nSec));
                            continue;
                        }
                    }
                    poFeature->SetField(oFetchMD.osFieldName.c_str(), pszMD);
                }
            }

            poFeature->SetGeometryDirectly(poPoly.release());

            if (poLayer->CreateFeature(poFeature.get()) != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to create feature in tile index.");
                return nullptr;
            }
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

    if (bIsSTACGeoParquet && nBatchSize != 0 && !FlushArrays())
    {
        return nullptr;
    }

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
/*                      GDALTileIndexOptionsNew()                       */
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
                psOptions->aoFetchMD.push_back(std::move(oMD));
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
/*                      GDALTileIndexOptionsFree()                      */
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
/*                  GDALTileIndexOptionsSetProgress()                   */
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
