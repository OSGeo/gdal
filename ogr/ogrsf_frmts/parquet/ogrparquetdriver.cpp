/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_pam.h"
#include "ogrsf_frmts.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <tuple>

#include "ogr_parquet.h"
#include "ogrparquetdrivercore.h"
#include "memdataset.h"

#include "../arrow_common/ograrrowrandomaccessfile.h"
#include "../arrow_common/vsiarrowfilesystem.hpp"
#include "../arrow_common/ograrrowwritablefile.h"
#include "../arrow_common/ograrrowdataset.hpp"
#include "../arrow_common/ograrrowlayer.hpp"  // for the destructor

#ifdef GDAL_USE_ARROWDATASET

/************************************************************************/
/*                      OpenFromDatasetFactory()                        */
/************************************************************************/

static GDALDataset *OpenFromDatasetFactory(
    const std::string &osBasePath,
    const std::shared_ptr<arrow::dataset::DatasetFactory> &factory,
    CSLConstList papszOpenOptions,
    const std::shared_ptr<arrow::fs::FileSystem> &fs)
{
    std::shared_ptr<arrow::dataset::Dataset> dataset;
    PARQUET_ASSIGN_OR_THROW(dataset, factory->Finish());

    auto poMemoryPool = std::shared_ptr<arrow::MemoryPool>(
        arrow::MemoryPool::CreateDefault().release());

    const bool bIsVSI = STARTS_WITH(osBasePath.c_str(), "/vsi");
    auto poDS = std::make_unique<OGRParquetDataset>(poMemoryPool);
    auto poLayer = std::make_unique<OGRParquetDatasetLayer>(
        poDS.get(), CPLGetBasenameSafe(osBasePath.c_str()).c_str(), bIsVSI,
        dataset, papszOpenOptions);
    poDS->SetLayer(std::move(poLayer));
    poDS->SetFileSystem(fs);
    return poDS.release();
}

/************************************************************************/
/*                         GetFileSystem()                              */
/************************************************************************/

static std::tuple<std::shared_ptr<arrow::fs::FileSystem>, std::string>
GetFileSystem(std::string &osBasePathInOut,
              const std::string &osQueryParameters)
{
    // Instantiate file system:
    // - VSIArrowFileSystem implementation for /vsi files
    // - base implementation for local files (if OGR_PARQUET_USE_VSI set to NO)
    std::shared_ptr<arrow::fs::FileSystem> fs;
    const bool bIsVSI = STARTS_WITH(osBasePathInOut.c_str(), "/vsi");
    VSIStatBufL sStat;
    std::string osFSFilename;
    if ((bIsVSI ||
         CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_VSI", "YES"))) &&
        VSIStatL(osBasePathInOut.c_str(), &sStat) == 0)
    {
        osFSFilename = osBasePathInOut;
        fs = std::make_shared<VSIArrowFileSystem>("PARQUET", osQueryParameters);
    }
    else
    {
        // FileSystemFromUriOrPath() doesn't like relative paths
        // so transform them to absolute.
        std::string osPath(osBasePathInOut);
        if (CPLIsFilenameRelative(osPath.c_str()))
        {
            char *pszCurDir = CPLGetCurrentDir();
            if (pszCurDir == nullptr)
                return {nullptr, osFSFilename};
            osPath = CPLFormFilenameSafe(pszCurDir, osPath.c_str(), nullptr);
            CPLFree(pszCurDir);
        }
        PARQUET_ASSIGN_OR_THROW(
            fs, arrow::fs::FileSystemFromUriOrPath(osPath, &osFSFilename));
    }
    return {fs, osFSFilename};
}

/************************************************************************/
/*                       MakeParquetFileFormat()                        */
/************************************************************************/

static std::shared_ptr<arrow::dataset::ParquetFileFormat>
MakeParquetFileFormat()
{
    auto parquetFileFormat =
        std::make_shared<arrow::dataset::ParquetFileFormat>();
#if ARROW_VERSION_MAJOR >= 21
    auto fragmentScanOptions =
        std::dynamic_pointer_cast<arrow::dataset::ParquetFragmentScanOptions>(
            parquetFileFormat->default_fragment_scan_options);
    CPLAssert(fragmentScanOptions);
    fragmentScanOptions->arrow_reader_properties->set_arrow_extensions_enabled(
        CPLTestBool(
            CPLGetConfigOption("OGR_PARQUET_ENABLE_ARROW_EXTENSIONS", "YES")));
#endif
    return parquetFileFormat;
}

/************************************************************************/
/*                  OpenParquetDatasetWithMetadata()                    */
/************************************************************************/

static GDALDataset *OpenParquetDatasetWithMetadata(
    const std::string &osBasePathIn, const char *pszMetadataFile,
    const std::string &osQueryParameters, CSLConstList papszOpenOptions)
{
    std::string osBasePath(osBasePathIn);
    const auto &[fs, osFSFilename] =
        GetFileSystem(osBasePath, osQueryParameters);

    arrow::dataset::ParquetFactoryOptions options;
    auto partitioningFactory = arrow::dataset::HivePartitioning::MakeFactory();
    options.partitioning =
        arrow::dataset::PartitioningOrFactory(std::move(partitioningFactory));

    std::shared_ptr<arrow::dataset::DatasetFactory> factory;
    PARQUET_ASSIGN_OR_THROW(factory,
                            arrow::dataset::ParquetDatasetFactory::Make(
                                osFSFilename + '/' + pszMetadataFile, fs,
                                MakeParquetFileFormat(), std::move(options)));

    return OpenFromDatasetFactory(osBasePath, factory, papszOpenOptions, fs);
}

/************************************************************************/
/*                 OpenParquetDatasetWithoutMetadata()                  */
/************************************************************************/

static GDALDataset *
OpenParquetDatasetWithoutMetadata(const std::string &osBasePathIn,
                                  const std::string &osQueryParameters,
                                  CSLConstList papszOpenOptions)
{
    std::string osBasePath(osBasePathIn);
    const auto &[fs, osFSFilename] =
        GetFileSystem(osBasePath, osQueryParameters);

    arrow::dataset::FileSystemFactoryOptions options;
    std::shared_ptr<arrow::dataset::DatasetFactory> factory;

    const auto fileInfo = fs->GetFileInfo(osFSFilename);
    if (fileInfo->IsFile())
    {
        PARQUET_ASSIGN_OR_THROW(
            factory, arrow::dataset::FileSystemDatasetFactory::Make(
                         fs, {std::move(osFSFilename)}, MakeParquetFileFormat(),
                         std::move(options)));
    }
    else
    {
        auto partitioningFactory =
            arrow::dataset::HivePartitioning::MakeFactory();
        options.partitioning = arrow::dataset::PartitioningOrFactory(
            std::move(partitioningFactory));

        arrow::fs::FileSelector selector;
        selector.base_dir = std::move(osFSFilename);
        selector.recursive = true;

        PARQUET_ASSIGN_OR_THROW(
            factory, arrow::dataset::FileSystemDatasetFactory::Make(
                         fs, std::move(selector), MakeParquetFileFormat(),
                         std::move(options)));
    }

    return OpenFromDatasetFactory(osBasePath, factory, papszOpenOptions, fs);
}

#endif

/************************************************************************/
/*                  BuildMemDatasetWithRowGroupExtents()                */
/************************************************************************/

/** Builds a MEM dataset that contains, for each row-group of the input file,
 * the feature count and spatial extent of the features of this row group,
 * using Parquet statistics. This assumes that the Parquet file declares
 * a "covering":{"bbox":{ ... }} metadata item.
 *
 * Only for debug purposes.
 */
static GDALDataset *BuildMemDatasetWithRowGroupExtents(OGRParquetLayer *poLayer)
{
    int iParquetXMin = -1;
    int iParquetYMin = -1;
    int iParquetXMax = -1;
    int iParquetYMax = -1;
    if (poLayer->GeomColsBBOXParquet(0, iParquetXMin, iParquetYMin,
                                     iParquetXMax, iParquetYMax))
    {
        auto poMemDS = std::unique_ptr<GDALDataset>(
            MEMDataset::Create("", 0, 0, 0, GDT_Unknown, nullptr));
        if (!poMemDS)
            return nullptr;
        OGRSpatialReference *poTmpSRS = nullptr;
        const auto poSrcSRS = poLayer->GetSpatialRef();
        if (poSrcSRS)
            poTmpSRS = poSrcSRS->Clone();
        auto poMemLayer =
            poMemDS->CreateLayer("footprint", poTmpSRS, wkbPolygon, nullptr);
        if (poTmpSRS)
            poTmpSRS->Release();
        if (!poMemLayer)
            return nullptr;
        poMemLayer->CreateField(
            std::make_unique<OGRFieldDefn>("feature_count", OFTInteger64)
                .get());

        const auto metadata =
            poLayer->GetReader()->parquet_reader()->metadata();
        const int numRowGroups = metadata->num_row_groups();
        for (int iRowGroup = 0; iRowGroup < numRowGroups; ++iRowGroup)
        {
            std::string osMinTmp, osMaxTmp;
            OGRField unusedF;
            bool unusedB;
            OGRFieldSubType unusedSubType;

            OGRField sXMin;
            OGR_RawField_SetNull(&sXMin);
            bool bFoundXMin = false;
            OGRFieldType eXMinType = OFTMaxType;

            OGRField sYMin;
            OGR_RawField_SetNull(&sYMin);
            bool bFoundYMin = false;
            OGRFieldType eYMinType = OFTMaxType;

            OGRField sXMax;
            OGR_RawField_SetNull(&sXMax);
            bool bFoundXMax = false;
            OGRFieldType eXMaxType = OFTMaxType;

            OGRField sYMax;
            OGR_RawField_SetNull(&sYMax);
            bool bFoundYMax = false;
            OGRFieldType eYMaxType = OFTMaxType;

            if (poLayer->GetMinMaxForParquetCol(
                    iRowGroup, iParquetXMin, nullptr,
                    /* bComputeMin = */ true, sXMin, bFoundXMin,
                    /* bComputeMax = */ false, unusedF, unusedB, eXMinType,
                    unusedSubType, osMinTmp, osMaxTmp) &&
                bFoundXMin && eXMinType == OFTReal &&
                poLayer->GetMinMaxForParquetCol(
                    iRowGroup, iParquetYMin, nullptr,
                    /* bComputeMin = */ true, sYMin, bFoundYMin,
                    /* bComputeMax = */ false, unusedF, unusedB, eYMinType,
                    unusedSubType, osMinTmp, osMaxTmp) &&
                bFoundYMin && eYMinType == OFTReal &&
                poLayer->GetMinMaxForParquetCol(
                    iRowGroup, iParquetXMax, nullptr,
                    /* bComputeMin = */ false, unusedF, unusedB,
                    /* bComputeMax = */ true, sXMax, bFoundXMax, eXMaxType,
                    unusedSubType, osMaxTmp, osMaxTmp) &&
                bFoundXMax && eXMaxType == OFTReal &&
                poLayer->GetMinMaxForParquetCol(
                    iRowGroup, iParquetYMax, nullptr,
                    /* bComputeMin = */ false, unusedF, unusedB,
                    /* bComputeMax = */ true, sYMax, bFoundYMax, eYMaxType,
                    unusedSubType, osMaxTmp, osMaxTmp) &&
                bFoundYMax && eYMaxType == OFTReal)
            {
                OGRFeature oFeat(poMemLayer->GetLayerDefn());
                oFeat.SetField(0,
                               static_cast<GIntBig>(
                                   metadata->RowGroup(iRowGroup)->num_rows()));
                auto poPoly = std::make_unique<OGRPolygon>();
                auto poLR = std::make_unique<OGRLinearRing>();
                poLR->addPoint(sXMin.Real, sYMin.Real);
                poLR->addPoint(sXMin.Real, sYMax.Real);
                poLR->addPoint(sXMax.Real, sYMax.Real);
                poLR->addPoint(sXMax.Real, sYMin.Real);
                poLR->addPoint(sXMin.Real, sYMin.Real);
                poPoly->addRingDirectly(poLR.release());
                oFeat.SetGeometryDirectly(poPoly.release());
                CPL_IGNORE_RET_VAL(poMemLayer->CreateFeature(&oFeat));
            }
        }

        return poMemDS.release();
    }
    return nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRParquetDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess == GA_Update)
        return nullptr;

#if ARROW_VERSION_MAJOR >= 21
    // Register geoarrow.wkb extension if not already done
    if (!arrow::GetExtensionType(EXTENSION_NAME_GEOARROW_WKB) &&
        CPLTestBool(CPLGetConfigOption(
            "OGR_PARQUET_REGISTER_GEOARROW_WKB_EXTENSION", "YES")))
    {
        CPL_IGNORE_RET_VAL(arrow::RegisterExtensionType(
            std::make_shared<OGRGeoArrowWkbExtensionType>(
                std::move(arrow::binary()), std::string())));
    }
#endif

#ifdef GDAL_USE_ARROWDATASET
    std::string osBasePath(poOpenInfo->pszFilename);
    std::string osQueryParameters;
    const bool bStartedWithParquetPrefix =
        STARTS_WITH(osBasePath.c_str(), "PARQUET:");

    if (bStartedWithParquetPrefix)
    {
        osBasePath = osBasePath.substr(strlen("PARQUET:"));
    }

    // Little trick to allow using syntax of
    // https://github.com/opengeospatial/geoparquet/discussions/101
    // ogrinfo
    // "/vsicurl/https://ai4edataeuwest.blob.core.windows.net/us-census/2020/cb_2020_us_vtd_500k.parquet?${SAS_TOKEN}"
    if (STARTS_WITH(osBasePath.c_str(), "/vsicurl/"))
    {
        const auto nPos = osBasePath.find(".parquet?st=");
        if (nPos != std::string::npos)
        {
            osQueryParameters = osBasePath.substr(nPos + strlen(".parquet"));
            osBasePath.resize(nPos + strlen(".parquet"));
        }
    }

    if (bStartedWithParquetPrefix || poOpenInfo->bIsDirectory ||
        !osQueryParameters.empty())
    {
        VSIStatBufL sStat;
        if (!osBasePath.empty() && osBasePath.back() == '/')
            osBasePath.pop_back();
        const std::string osMetadataPath =
            CPLFormFilenameSafe(osBasePath.c_str(), "_metadata", nullptr);
        if (CPLTestBool(
                CPLGetConfigOption("OGR_PARQUET_USE_METADATA_FILE", "YES")) &&
            VSIStatL((osMetadataPath + osQueryParameters).c_str(), &sStat) == 0)
        {
            // If there's a _metadata file, then use it to avoid listing files
            try
            {
                return OpenParquetDatasetWithMetadata(
                    osBasePath, "_metadata", osQueryParameters,
                    poOpenInfo->papszOpenOptions);
            }
            catch (const std::exception &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Parquet exception: %s",
                         e.what());
            }
            return nullptr;
        }
        else
        {
            bool bLikelyParquetDataset = false;
            if (poOpenInfo->bIsDirectory)
            {
                // Detect if the directory contains .parquet files, or
                // subdirectories with a name of the form "key=value", typical
                // of HIVE partitioning.
                const CPLStringList aosFiles(VSIReadDir(osBasePath.c_str()));
                for (const char *pszFilename : cpl::Iterate(aosFiles))
                {
                    if (EQUAL(CPLGetExtensionSafe(pszFilename).c_str(),
                              "parquet"))
                    {
                        bLikelyParquetDataset = true;
                        break;
                    }
                    else if (strchr(pszFilename, '='))
                    {
                        // HIVE partitioning
                        if (VSIStatL(CPLFormFilenameSafe(osBasePath.c_str(),
                                                         pszFilename, nullptr)
                                         .c_str(),
                                     &sStat) == 0 &&
                            VSI_ISDIR(sStat.st_mode))
                        {
                            bLikelyParquetDataset = true;
                            break;
                        }
                    }
                }
            }

            if (bStartedWithParquetPrefix || bLikelyParquetDataset)
            {
                try
                {
                    return OpenParquetDatasetWithoutMetadata(
                        osBasePath, osQueryParameters,
                        poOpenInfo->papszOpenOptions);
                }
                catch (const std::exception &e)
                {
                    // If we aren't quite sure that the passed file name is
                    // a directory, then silently continue
                    if (poOpenInfo->bIsDirectory)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Parquet exception: %s", e.what());
                        return nullptr;
                    }
                }
            }
        }
    }
#endif

    if (!OGRParquetDriverIdentify(poOpenInfo))
    {
        return nullptr;
    }

    if (poOpenInfo->bIsDirectory)
        return nullptr;

    std::string osFilename(poOpenInfo->pszFilename);
    if (STARTS_WITH(poOpenInfo->pszFilename, "PARQUET:"))
    {
        osFilename = poOpenInfo->pszFilename + strlen("PARQUET:");
    }

    try
    {
        std::shared_ptr<arrow::io::RandomAccessFile> infile;
        if (STARTS_WITH(osFilename.c_str(), "/vsi") ||
            CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_VSI", "NO")))
        {
            VSIVirtualHandleUniquePtr fp(poOpenInfo->fpL);
            poOpenInfo->fpL = nullptr;
            if (fp == nullptr)
            {
                fp.reset(VSIFOpenL(osFilename.c_str(), "rb"));
                if (fp == nullptr)
                    return nullptr;
            }
            infile = std::make_shared<OGRArrowRandomAccessFile>(osFilename,
                                                                std::move(fp));
        }
        else
        {
            PARQUET_ASSIGN_OR_THROW(infile,
                                    arrow::io::ReadableFile::Open(osFilename));
        }

        // Open Parquet file reader
        std::unique_ptr<parquet::arrow::FileReader> arrow_reader;
        auto poMemoryPool = std::shared_ptr<arrow::MemoryPool>(
            arrow::MemoryPool::CreateDefault().release());

        const int nNumCPUs = OGRParquetLayerBase::GetNumCPUs();
        const char *pszUseThreads =
            CPLGetConfigOption("OGR_PARQUET_USE_THREADS", nullptr);
        if (!pszUseThreads && nNumCPUs > 1)
        {
            pszUseThreads = "YES";
        }
        const bool bUseThreads = pszUseThreads && CPLTestBool(pszUseThreads);

        const char *pszParquetBatchSize =
            CPLGetConfigOption("OGR_PARQUET_BATCH_SIZE", nullptr);

#if ARROW_VERSION_MAJOR >= 21
        parquet::arrow::FileReaderBuilder fileReaderBuilder;
        {
            auto st = fileReaderBuilder.Open(std::move(infile));
            if (!st.ok())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "parquet::arrow::FileReaderBuilder::Open() failed: %s",
                         st.message().c_str());
                return nullptr;
            }
        }
        fileReaderBuilder.memory_pool(poMemoryPool.get());
        parquet::ArrowReaderProperties fileReaderProperties;
        fileReaderProperties.set_arrow_extensions_enabled(CPLTestBool(
            CPLGetConfigOption("OGR_PARQUET_ENABLE_ARROW_EXTENSIONS", "YES")));
        if (pszParquetBatchSize)
        {
            fileReaderProperties.set_batch_size(
                CPLAtoGIntBig(pszParquetBatchSize));
        }
        if (bUseThreads)
        {
            fileReaderProperties.set_use_threads(true);
        }
        fileReaderBuilder.properties(fileReaderProperties);
        {
            auto res = fileReaderBuilder.Build();
            if (!res.ok())
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "parquet::arrow::FileReaderBuilder::Build() failed: %s",
                    res.status().message().c_str());
                return nullptr;
            }
            arrow_reader = std::move(*res);
        }
#elif ARROW_VERSION_MAJOR >= 19
        PARQUET_ASSIGN_OR_THROW(
            arrow_reader,
            parquet::arrow::OpenFile(std::move(infile), poMemoryPool.get()));
#else
        auto st = parquet::arrow::OpenFile(std::move(infile),
                                           poMemoryPool.get(), &arrow_reader);
        if (!st.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "parquet::arrow::OpenFile() failed: %s",
                     st.message().c_str());
            return nullptr;
        }
#endif

#if ARROW_VERSION_MAJOR < 21
        if (pszParquetBatchSize)
        {
            arrow_reader->set_batch_size(CPLAtoGIntBig(pszParquetBatchSize));
        }

        if (bUseThreads)
        {
            arrow_reader->set_use_threads(true);
        }
#endif

        auto poDS = std::make_unique<OGRParquetDataset>(poMemoryPool);
        auto poLayer = std::make_unique<OGRParquetLayer>(
            poDS.get(), CPLGetBasenameSafe(osFilename.c_str()).c_str(),
            std::move(arrow_reader), poOpenInfo->papszOpenOptions);

        // For debug purposes: return a layer with the extent of each row group
        if (CPLTestBool(
                CPLGetConfigOption("OGR_PARQUET_SHOW_ROW_GROUP_EXTENT", "NO")))
        {
            return BuildMemDatasetWithRowGroupExtents(poLayer.get());
        }

        poDS->SetLayer(std::move(poLayer));
        return poDS.release();
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Parquet exception: %s",
                 e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRParquetDriverCreate(const char *pszName, int nXSize,
                                           int nYSize, int nBands,
                                           GDALDataType eType,
                                           char ** /* papszOptions */)
{
    if (!(nXSize == 0 && nYSize == 0 && nBands == 0 && eType == GDT_Unknown))
        return nullptr;

    try
    {
        std::shared_ptr<arrow::io::OutputStream> out_file;
        if (STARTS_WITH(pszName, "/vsi") ||
            CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_VSI", "YES")))
        {
            VSIVirtualHandleUniquePtr fp =
                VSIFilesystemHandler::OpenStatic(pszName, "wb");
            if (fp == nullptr)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszName);
                return nullptr;
            }
            out_file = std::make_shared<OGRArrowWritableFile>(std::move(fp));
        }
        else
        {
            PARQUET_ASSIGN_OR_THROW(out_file,
                                    arrow::io::FileOutputStream::Open(pszName));
        }

        return new OGRParquetWriterDataset(out_file);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Parquet exception: %s",
                 e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                         OGRParquetDriver()                           */
/************************************************************************/

class OGRParquetDriver final : public GDALDriver
{
    std::mutex m_oMutex{};
    bool m_bMetadataInitialized = false;
    void InitMetadata();

  public:
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override;

    char **GetMetadata(const char *pszDomain) override
    {
        std::lock_guard oLock(m_oMutex);
        InitMetadata();
        return GDALDriver::GetMetadata(pszDomain);
    }
};

const char *OGRParquetDriver::GetMetadataItem(const char *pszName,
                                              const char *pszDomain)
{
    std::lock_guard oLock(m_oMutex);
    if (EQUAL(pszName, GDAL_DS_LAYER_CREATIONOPTIONLIST))
    {
        InitMetadata();
    }
    return GDALDriver::GetMetadataItem(pszName, pszDomain);
}

void OGRParquetDriver::InitMetadata()
{
    if (m_bMetadataInitialized)
        return;
    m_bMetadataInitialized = true;

    CPLXMLTreeCloser oTree(
        CPLCreateXMLNode(nullptr, CXT_Element, "LayerCreationOptionList"));

    std::vector<const char *> apszCompressionMethods;
    bool bHasSnappy = false;
    int minComprLevel = INT_MAX;
    int maxComprLevel = INT_MIN;
    std::string osCompressionLevelDesc = "Compression level, codec dependent.";
    for (const char *pszMethod :
         {"SNAPPY", "GZIP", "BROTLI", "ZSTD", "LZ4_RAW", "LZO", "LZ4_HADOOP"})
    {
        auto compressionTypeRes = arrow::util::Codec::GetCompressionType(
            CPLString(pszMethod).tolower());
        if (compressionTypeRes.ok() &&
            arrow::util::Codec::IsAvailable(*compressionTypeRes))
        {
            const auto compressionType = *compressionTypeRes;
            if (EQUAL(pszMethod, "SNAPPY"))
                bHasSnappy = true;
            apszCompressionMethods.emplace_back(pszMethod);

            auto minCompressLevelRes =
                arrow::util::Codec::MinimumCompressionLevel(compressionType);
            auto maxCompressLevelRes =
                arrow::util::Codec::MaximumCompressionLevel(compressionType);
            auto defCompressLevelRes =
                arrow::util::Codec::DefaultCompressionLevel(compressionType);
            if (minCompressLevelRes.ok() && maxCompressLevelRes.ok() &&
                defCompressLevelRes.ok())
            {
                minComprLevel = std::min(minComprLevel, *minCompressLevelRes);
                maxComprLevel = std::max(maxComprLevel, *maxCompressLevelRes);
                osCompressionLevelDesc += ' ';
                osCompressionLevelDesc += pszMethod;
                osCompressionLevelDesc += ": [";
                osCompressionLevelDesc += std::to_string(*minCompressLevelRes);
                osCompressionLevelDesc += ',';
                osCompressionLevelDesc += std::to_string(*maxCompressLevelRes);
                osCompressionLevelDesc += "], default=";
                if (EQUAL(pszMethod, "ZSTD"))
                    osCompressionLevelDesc += std::to_string(
                        OGR_PARQUET_ZSTD_DEFAULT_COMPRESSION_LEVEL);
                else
                    osCompressionLevelDesc +=
                        std::to_string(*defCompressLevelRes);
                osCompressionLevelDesc += '.';
            }
        }
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "COMPRESSION");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Compression method");
        CPLAddXMLAttributeAndValue(psOption, "default",
                                   bHasSnappy ? "SNAPPY" : "NONE");
        {
            auto poValueNode = CPLCreateXMLNode(psOption, CXT_Element, "Value");
            CPLAddXMLAttributeAndValue(poValueNode, "alias", "UNCOMPRESSED");
            CPLCreateXMLNode(poValueNode, CXT_Text, "NONE");
        }
        for (const char *pszMethod : apszCompressionMethods)
        {
            auto poValueNode = CPLCreateXMLNode(psOption, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, pszMethod);
        }
    }

    if (minComprLevel <= maxComprLevel)
    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "COMPRESSION_LEVEL");
        CPLAddXMLAttributeAndValue(psOption, "type", "int");
        CPLAddXMLAttributeAndValue(
            psOption, "min",
            CPLSPrintf("%d",
                       std::min(DEFAULT_COMPRESSION_LEVEL, minComprLevel)));
        CPLAddXMLAttributeAndValue(psOption, "max",
                                   CPLSPrintf("%d", maxComprLevel));
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   osCompressionLevelDesc.c_str());
        CPLAddXMLAttributeAndValue(psOption, "default",
                                   CPLSPrintf("%d", DEFAULT_COMPRESSION_LEVEL));
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "GEOMETRY_ENCODING");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Encoding of geometry columns");
        CPLAddXMLAttributeAndValue(psOption, "default", "WKB");
        for (const char *pszEncoding :
             {"WKB", "WKT", "GEOARROW", "GEOARROW_INTERLEAVED"})
        {
            auto poValueNode = CPLCreateXMLNode(psOption, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, pszEncoding);
            if (EQUAL(pszEncoding, "GEOARROW"))
                CPLAddXMLAttributeAndValue(poValueNode, "alias",
                                           "GEOARROW_STRUCT");
        }
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "ROW_GROUP_SIZE");
        CPLAddXMLAttributeAndValue(psOption, "type", "integer");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Maximum number of rows per group");
        CPLAddXMLAttributeAndValue(psOption, "default", "65536");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "GEOMETRY_NAME");
        CPLAddXMLAttributeAndValue(psOption, "type", "string");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Name of geometry column");
        CPLAddXMLAttributeAndValue(psOption, "default", "geometry");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "COORDINATE_PRECISION");
        CPLAddXMLAttributeAndValue(psOption, "type", "float");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Number of decimals for coordinates (only "
                                   "for GEOMETRY_ENCODING=WKT)");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "FID");
        CPLAddXMLAttributeAndValue(psOption, "type", "string");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Name of the FID column to create");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "POLYGON_ORIENTATION");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(
            psOption, "description",
            "Which ring orientation to use for polygons");
        CPLAddXMLAttributeAndValue(psOption, "default", "COUNTERCLOCKWISE");
        CPLCreateXMLElementAndValue(psOption, "Value", "COUNTERCLOCKWISE");
        CPLCreateXMLElementAndValue(psOption, "Value", "UNMODIFIED");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "EDGES");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(
            psOption, "description",
            "Name of the coordinate system for the edges");
        CPLAddXMLAttributeAndValue(psOption, "default", "PLANAR");
        CPLCreateXMLElementAndValue(psOption, "Value", "PLANAR");
        CPLCreateXMLElementAndValue(psOption, "Value", "SPHERICAL");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "CREATOR");
        CPLAddXMLAttributeAndValue(psOption, "type", "string");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Name of creating application");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "WRITE_COVERING_BBOX");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "default", "AUTO");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Whether to write xmin/ymin/xmax/ymax "
                                   "columns with the bounding box of "
                                   "geometries");
        CPLCreateXMLElementAndValue(psOption, "Value", "AUTO");
        CPLCreateXMLElementAndValue(psOption, "Value", "YES");
        CPLCreateXMLElementAndValue(psOption, "Value", "NO");
    }

#if ARROW_VERSION_MAJOR >= 21
    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "USE_PARQUET_GEO_TYPES");
        CPLAddXMLAttributeAndValue(psOption, "default", "NO");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Whether to use Parquet Geometry/Geography "
                                   "logical types (introduced in libarrow 21), "
                                   "when using GEOMETRY_ENCODING=WKB encoding");
        CPLCreateXMLElementAndValue(psOption, "Value", "YES");
        CPLCreateXMLElementAndValue(psOption, "Value", "NO");
        CPLCreateXMLElementAndValue(psOption, "Value", "ONLY");
    }
#endif

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "SORT_BY_BBOX");
        CPLAddXMLAttributeAndValue(psOption, "type", "boolean");
        CPLAddXMLAttributeAndValue(psOption, "default", "NO");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Whether features should be sorted based on "
                                   "the bounding box of their geometries");
    }

    char *pszXML = CPLSerializeXMLTree(oTree.get());
    GDALDriver::SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST, pszXML);
    CPLFree(pszXML);
}

/************************************************************************/
/*                         RegisterOGRParquet()                         */
/************************************************************************/

void RegisterOGRParquet()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    auto poDriver = std::make_unique<OGRParquetDriver>();
    OGRParquetDriverSetCommonMetadata(poDriver.get());

    poDriver->pfnOpen = OGRParquetDriverOpen;
    poDriver->pfnCreate = OGRParquetDriverCreate;

    poDriver->SetMetadataItem("ARROW_VERSION", ARROW_VERSION_STRING);
#ifdef GDAL_USE_ARROWDATASET
    poDriver->SetMetadataItem("ARROW_DATASET", "YES");
#endif

    GetGDALDriverManager()->RegisterDriver(poDriver.release());

#if ARROW_VERSION_MAJOR >= 16
    // Mostly for tests
    const char *pszPath =
        CPLGetConfigOption("OGR_PARQUET_LOAD_FILE_SYSTEM_FACTORIES", nullptr);
    if (pszPath)
    {
        auto result = arrow::fs::LoadFileSystemFactories(pszPath);
        if (!result.ok())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "arrow::fs::LoadFileSystemFactories() failed with %s",
                     result.message().c_str());
        }
    }
#endif

#if defined(GDAL_USE_ARROWDATASET) && defined(GDAL_USE_ARROWCOMPUTE)
    {
        auto status = arrow::compute::Initialize();
        if (!status.ok())
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "arrow::compute::Initialize() failed with %s",
                     status.message().c_str());
        }
    }
#endif
}
