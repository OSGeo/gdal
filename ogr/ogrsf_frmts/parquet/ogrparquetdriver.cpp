/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#include "gdal_pam.h"
#include "ogrsf_frmts.h"

#include <algorithm>
#include <map>

#include "ogr_parquet.h"
#include "ogrparquetdrivercore.h"

#include "../arrow_common/ograrrowrandomaccessfile.h"
#include "../arrow_common/ograrrowwritablefile.h"
#include "../arrow_common/ograrrowdataset.hpp"
#include "../arrow_common/ograrrowlayer.hpp"  // for the destructor

#ifdef GDAL_USE_ARROWDATASET

/************************************************************************/
/*                         VSIArrowFileSystem                           */
/************************************************************************/

class VSIArrowFileSystem final : public arrow::fs::FileSystem
{
    const std::string m_osQueryParameters;

  public:
    explicit VSIArrowFileSystem(const std::string &osQueryParameters)
        : m_osQueryParameters(osQueryParameters)
    {
    }

    std::string type_name() const override
    {
        return "vsi";
    }

    using arrow::fs::FileSystem::Equals;

    bool Equals(const arrow::fs::FileSystem &other) const override
    {
        const auto poOther = dynamic_cast<const VSIArrowFileSystem *>(&other);
        return poOther != nullptr &&
               poOther->m_osQueryParameters == m_osQueryParameters;
    }

    using arrow::fs::FileSystem::GetFileInfo;

    arrow::Result<arrow::fs::FileInfo>
    GetFileInfo(const std::string &path) override
    {
        auto fileType = arrow::fs::FileType::Unknown;
        VSIStatBufL sStat;
        if (VSIStatL(path.c_str(), &sStat) == 0)
        {
            if (VSI_ISREG(sStat.st_mode))
                fileType = arrow::fs::FileType::File;
            else if (VSI_ISDIR(sStat.st_mode))
                fileType = arrow::fs::FileType::Directory;
        }
        else
        {
            fileType = arrow::fs::FileType::NotFound;
        }
        arrow::fs::FileInfo info(path, fileType);
        if (fileType == arrow::fs::FileType::File)
            info.set_size(sStat.st_size);
        return info;
    }

    arrow::Result<arrow::fs::FileInfoVector>
    GetFileInfo(const arrow::fs::FileSelector &select) override
    {
        arrow::fs::FileInfoVector res;
        VSIDIR *psDir = VSIOpenDir(select.base_dir.c_str(),
                                   select.recursive ? -1 : 0, nullptr);
        if (psDir == nullptr)
            return res;

        bool bParquetFound = false;
        const int nMaxNonParquetFiles = atoi(
            CPLGetConfigOption("OGR_PARQUET_MAX_NON_PARQUET_FILES", "100"));
        const int nMaxListedFiles =
            atoi(CPLGetConfigOption("OGR_PARQUET_MAX_LISTED_FILES", "1000000"));
        while (const auto psEntry = VSIGetNextDirEntry(psDir))
        {
            if (!bParquetFound)
                bParquetFound =
                    EQUAL(CPLGetExtension(psEntry->pszName), "parquet");

            const std::string osFilename =
                select.base_dir + '/' + psEntry->pszName;
            int nMode = psEntry->nMode;
            if (!psEntry->bModeKnown)
            {
                VSIStatBufL sStat;
                if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                    nMode = sStat.st_mode;
            }

            auto fileType = arrow::fs::FileType::Unknown;
            if (VSI_ISREG(nMode))
                fileType = arrow::fs::FileType::File;
            else if (VSI_ISDIR(nMode))
                fileType = arrow::fs::FileType::Directory;

            arrow::fs::FileInfo info(osFilename, fileType);
            if (fileType == arrow::fs::FileType::File && psEntry->bSizeKnown)
            {
                info.set_size(psEntry->nSize);
            }
            res.push_back(info);

            // Avoid iterating over too many files if there's no likely parquet
            // files.
            if (static_cast<int>(res.size()) == nMaxNonParquetFiles &&
                !bParquetFound)
                break;
            if (static_cast<int>(res.size()) == nMaxListedFiles)
                break;
        }
        VSICloseDir(psDir);
        return res;
    }

    arrow::Status CreateDir(const std::string & /*path*/,
                            bool /*recursive*/ = true) override
    {
        return arrow::Status::IOError("CreateDir() unimplemented");
    }

    arrow::Status DeleteDir(const std::string & /*path*/) override
    {
        return arrow::Status::IOError("DeleteDir() unimplemented");
    }

    arrow::Status DeleteDirContents(const std::string & /*path*/
#if ARROW_VERSION_MAJOR >= 8
                                    ,
                                    bool /*missing_dir_ok*/ = false
#endif
                                    ) override
    {
        return arrow::Status::IOError("DeleteDirContents() unimplemented");
    }

    arrow::Status DeleteRootDirContents() override
    {
        return arrow::Status::IOError("DeleteRootDirContents() unimplemented");
    }

    arrow::Status DeleteFile(const std::string & /*path*/) override
    {
        return arrow::Status::IOError("DeleteFile() unimplemented");
    }

    arrow::Status Move(const std::string & /*src*/,
                       const std::string & /*dest*/) override
    {
        return arrow::Status::IOError("Move() unimplemented");
    }

    arrow::Status CopyFile(const std::string & /*src*/,
                           const std::string & /*dest*/) override
    {
        return arrow::Status::IOError("CopyFile() unimplemented");
    }

    using arrow::fs::FileSystem::OpenInputStream;

    arrow::Result<std::shared_ptr<arrow::io::InputStream>>
    OpenInputStream(const std::string &path) override
    {
        return OpenInputFile(path);
    }

    using arrow::fs::FileSystem::OpenInputFile;

    arrow::Result<std::shared_ptr<arrow::io::RandomAccessFile>>
    OpenInputFile(const std::string &path) override
    {
        std::string osPath(path);
        osPath += m_osQueryParameters;
        CPLDebugOnly("PARQUET", "Opening %s", osPath.c_str());
        auto fp = VSIVirtualHandleUniquePtr(VSIFOpenL(osPath.c_str(), "rb"));
        if (fp == nullptr)
            return arrow::Status::IOError("OpenInputFile() failed for " +
                                          osPath);
        return std::make_shared<OGRArrowRandomAccessFile>(std::move(fp));
    }

    using arrow::fs::FileSystem::OpenOutputStream;

    arrow::Result<std::shared_ptr<arrow::io::OutputStream>>
    OpenOutputStream(const std::string & /*path*/,
                     const std::shared_ptr<const arrow::KeyValueMetadata>
                         & /* metadata */) override
    {
        return arrow::Status::IOError("OpenOutputStream() unimplemented");
    }

    arrow::Result<std::shared_ptr<arrow::io::OutputStream>>
    OpenAppendStream(const std::string & /*path*/,
                     const std::shared_ptr<const arrow::KeyValueMetadata>
                         & /* metadata */) override
    {
        return arrow::Status::IOError("OpenAppendStream() unimplemented");
    }
};

/************************************************************************/
/*                      OpenFromDatasetFactory()                        */
/************************************************************************/

static GDALDataset *OpenFromDatasetFactory(
    const std::string &osBasePath,
    const std::shared_ptr<arrow::dataset::DatasetFactory> &factory,
    CSLConstList papszOpenOptions)
{
    std::shared_ptr<arrow::dataset::Dataset> dataset;
    PARQUET_ASSIGN_OR_THROW(dataset, factory->Finish());

    std::shared_ptr<arrow::dataset::ScannerBuilder> scannerBuilder;
    PARQUET_ASSIGN_OR_THROW(scannerBuilder, dataset->NewScan());

    auto poMemoryPool = std::shared_ptr<arrow::MemoryPool>(
        arrow::MemoryPool::CreateDefault().release());

    // We cannot use the above shared memory pool. Otherwise we get random
    // crashes in multi-threaded arrow code (apparently some cleanup code),
    // that may used the memory pool after it has been destroyed.
    // PARQUET_THROW_NOT_OK(scannerBuilder->Pool(poMemoryPool.get()));

    const bool bIsVSI = STARTS_WITH(osBasePath.c_str(), "/vsi");
    if (bIsVSI)
    {
        const int nFragmentReadAhead =
            atoi(CPLGetConfigOption("OGR_PARQUET_FRAGMENT_READ_AHEAD", "2"));
        PARQUET_THROW_NOT_OK(
            scannerBuilder->FragmentReadahead(nFragmentReadAhead));

        const char *pszBatchSize =
            CPLGetConfigOption("OGR_PARQUET_BATCH_SIZE", nullptr);
        if (pszBatchSize)
        {
            PARQUET_THROW_NOT_OK(
                scannerBuilder->BatchSize(CPLAtoGIntBig(pszBatchSize)));
        }

        const char *pszUseThreads =
            CPLGetConfigOption("OGR_PARQUET_USE_THREADS", nullptr);
        if (pszUseThreads)
        {
            PARQUET_THROW_NOT_OK(
                scannerBuilder->UseThreads(CPLTestBool(pszUseThreads)));
        }

        const char *pszNumThreads =
            CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
        int nNumThreads = 0;
        if (pszNumThreads == nullptr)
            nNumThreads = std::min(4, CPLGetNumCPUs());
        else
            nNumThreads = EQUAL(pszNumThreads, "ALL_CPUS")
                              ? CPLGetNumCPUs()
                              : atoi(pszNumThreads);
        if (nNumThreads > 1)
        {
            CPL_IGNORE_RET_VAL(arrow::SetCpuThreadPoolCapacity(nNumThreads));
        }

#if PARQUET_VERSION_MAJOR >= 10
        const char *pszBatchReadAhead =
            CPLGetConfigOption("OGR_PARQUET_BATCH_READ_AHEAD", nullptr);
        if (pszBatchReadAhead)
        {
            PARQUET_THROW_NOT_OK(
                scannerBuilder->BatchReadahead(atoi(pszBatchReadAhead)));
        }
#endif
    }

    std::shared_ptr<arrow::dataset::Scanner> scanner;
    PARQUET_ASSIGN_OR_THROW(scanner, scannerBuilder->Finish());

    auto poDS = std::make_unique<OGRParquetDataset>(poMemoryPool);
    auto poLayer = std::make_unique<OGRParquetDatasetLayer>(
        poDS.get(), CPLGetBasename(osBasePath.c_str()), scanner,
        scannerBuilder->schema(), papszOpenOptions);
    poDS->SetLayer(std::move(poLayer));
    return poDS.release();
}

/************************************************************************/
/*                         GetFileSystem()                              */
/************************************************************************/

static std::shared_ptr<arrow::fs::FileSystem>
GetFileSystem(std::string &osBasePathInOut,
              const std::string &osQueryParameters)
{
    // Instantiate file system:
    // - VSIArrowFileSystem implementation for /vsi files
    // - base implementation for local files (if OGR_PARQUET_USE_VSI set to NO)
    std::shared_ptr<arrow::fs::FileSystem> fs;
    const bool bIsVSI = STARTS_WITH(osBasePathInOut.c_str(), "/vsi");
    if (bIsVSI || CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_VSI", "YES")))
    {
        fs = std::make_shared<VSIArrowFileSystem>(osQueryParameters);
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
                return nullptr;
            osPath = CPLFormFilename(pszCurDir, osPath.c_str(), nullptr);
            CPLFree(pszCurDir);
        }
        PARQUET_ASSIGN_OR_THROW(fs, arrow::fs::FileSystemFromUriOrPath(osPath));
    }
    return fs;
}

/************************************************************************/
/*                  OpenParquetDatasetWithMetadata()                    */
/************************************************************************/

static GDALDataset *OpenParquetDatasetWithMetadata(
    const std::string &osBasePathIn, const char *pszMetadataFile,
    const std::string &osQueryParameters, CSLConstList papszOpenOptions)
{
    std::string osBasePath(osBasePathIn);
    auto fs = GetFileSystem(osBasePath, osQueryParameters);

    arrow::dataset::ParquetFactoryOptions options;
    auto partitioningFactory = arrow::dataset::HivePartitioning::MakeFactory();
    options.partitioning =
        arrow::dataset::PartitioningOrFactory(std::move(partitioningFactory));

    std::shared_ptr<arrow::dataset::DatasetFactory> factory;
    PARQUET_ASSIGN_OR_THROW(
        factory, arrow::dataset::ParquetDatasetFactory::Make(
                     osBasePath + '/' + pszMetadataFile, std::move(fs),
                     std::make_shared<arrow::dataset::ParquetFileFormat>(),
                     std::move(options)));

    return OpenFromDatasetFactory(osBasePath, factory, papszOpenOptions);
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
    auto fs = GetFileSystem(osBasePath, osQueryParameters);

    arrow::dataset::FileSystemFactoryOptions options;
    std::shared_ptr<arrow::dataset::DatasetFactory> factory;
    VSIStatBufL sStat;
    if (VSIStatL(osBasePath.c_str(), &sStat) == 0 && VSI_ISREG(sStat.st_mode))
    {
        PARQUET_ASSIGN_OR_THROW(
            factory, arrow::dataset::FileSystemDatasetFactory::Make(
                         std::move(fs), {osBasePath},
                         std::make_shared<arrow::dataset::ParquetFileFormat>(),
                         std::move(options)));
    }
    else
    {
        auto partitioningFactory =
            arrow::dataset::HivePartitioning::MakeFactory();
        options.partitioning = arrow::dataset::PartitioningOrFactory(
            std::move(partitioningFactory));

        arrow::fs::FileSelector selector;
        selector.base_dir = osBasePath;
        selector.recursive = true;

        PARQUET_ASSIGN_OR_THROW(
            factory, arrow::dataset::FileSystemDatasetFactory::Make(
                         std::move(fs), std::move(selector),
                         std::make_shared<arrow::dataset::ParquetFileFormat>(),
                         std::move(options)));
    }

    return OpenFromDatasetFactory(osBasePath, factory, papszOpenOptions);
}

#endif

/************************************************************************/
/*                  BuildMemDatasetWithRowGroupExtents()                */
/************************************************************************/

/** Builds a Memory dataset that contains, for each row-group of the input file,
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
        auto poMemDrv = GetGDALDriverManager()->GetDriverByName("Memory");
        if (!poMemDrv)
            return nullptr;
        auto poMemDS = std::unique_ptr<GDALDataset>(
            poMemDrv->Create("", 0, 0, 0, GDT_Unknown, nullptr));
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
            osBasePath.resize(osBasePath.size() - 1);
        std::string osMetadataPath =
            CPLFormFilename(osBasePath.c_str(), "_metadata", nullptr);
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
                    if (EQUAL(CPLGetExtension(pszFilename), "parquet"))
                    {
                        bLikelyParquetDataset = true;
                        break;
                    }
                    else if (strchr(pszFilename, '='))
                    {
                        // HIVE partitioning
                        if (VSIStatL(CPLFormFilename(osBasePath.c_str(),
                                                     pszFilename, nullptr),
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
            infile = std::make_shared<OGRArrowRandomAccessFile>(std::move(fp));
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
        auto st = parquet::arrow::OpenFile(std::move(infile),
                                           poMemoryPool.get(), &arrow_reader);
        if (!st.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "parquet::arrow::OpenFile() failed");
            return nullptr;
        }

        auto poDS = std::make_unique<OGRParquetDataset>(poMemoryPool);
        auto poLayer = std::make_unique<OGRParquetLayer>(
            poDS.get(), CPLGetBasename(osFilename.c_str()),
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
            VSILFILE *fp = VSIFOpenL(pszName, "wb");
            if (fp == nullptr)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszName);
                return nullptr;
            }
            out_file = std::make_shared<OGRArrowWritableFile>(fp);
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
    bool m_bMetadataInitialized = false;
    void InitMetadata();

  public:
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override
    {
        if (EQUAL(pszName, GDAL_DS_LAYER_CREATIONOPTIONLIST))
        {
            InitMetadata();
        }
        return GDALDriver::GetMetadataItem(pszName, pszDomain);
    }

    char **GetMetadata(const char *pszDomain) override
    {
        InitMetadata();
        return GDALDriver::GetMetadata(pszDomain);
    }
};

void OGRParquetDriver::InitMetadata()
{
    if (m_bMetadataInitialized)
        return;
    m_bMetadataInitialized = true;

    CPLXMLTreeCloser oTree(
        CPLCreateXMLNode(nullptr, CXT_Element, "LayerCreationOptionList"));

    std::vector<const char *> apszCompressionMethods;
    bool bHasSnappy = false;
    for (const char *pszMethod :
         {"SNAPPY", "GZIP", "BROTLI", "ZSTD", "LZ4_RAW", "LZO", "LZ4_HADOOP"})
    {
        auto oResult = arrow::util::Codec::GetCompressionType(
            CPLString(pszMethod).tolower());
        if (oResult.ok() && arrow::util::Codec::IsAvailable(*oResult))
        {
            if (EQUAL(pszMethod, "SNAPPY"))
                bHasSnappy = true;
            apszCompressionMethods.emplace_back(pszMethod);
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
        CPLAddXMLAttributeAndValue(psOption, "type", "boolean");
        CPLAddXMLAttributeAndValue(psOption, "default", "YES");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Whether to write xmin/ymin/xmax/ymax "
                                   "columns with the bounding box of "
                                   "geometries");
    }

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

#ifdef GDAL_USE_ARROWDATASET
    poDriver->SetMetadataItem("ARROW_DATASET", "YES");
#endif

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
