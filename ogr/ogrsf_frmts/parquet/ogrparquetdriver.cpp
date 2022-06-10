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

#include <map>

#include "ogr_parquet.h"
#include "../arrow_common/ograrrowrandomaccessfile.h"
#include "../arrow_common/ograrrowwritablefile.h"
#include "../arrow_common/ograrrowdataset.hpp"

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

template<size_t N> constexpr int constexpr_length( const char (&) [N] )
{
  return static_cast<int>(N-1);
}

static int OGRParquetDriverIdentify( GDALOpenInfo* poOpenInfo )
{
#ifdef GDAL_USE_ARROWDATASET
    if( poOpenInfo->bIsDirectory )
        return -1;
#endif
    if( STARTS_WITH(poOpenInfo->pszFilename, "PARQUET:") )
        return TRUE;

    // See https://github.com/apache/parquet-format#file-format
    bool bRet = false;
    constexpr const char SIGNATURE[] = "PAR1";
    constexpr int SIGNATURE_SIZE = constexpr_length(SIGNATURE);
    static_assert(SIGNATURE_SIZE == 4, "SIGNATURE_SIZE == 4");
    constexpr int METADATASIZE_SIZE = 4;
    if( poOpenInfo->fpL != nullptr &&
        poOpenInfo->nHeaderBytes >= SIGNATURE_SIZE + METADATASIZE_SIZE + SIGNATURE_SIZE &&
        memcmp(poOpenInfo->pabyHeader, SIGNATURE, SIGNATURE_SIZE) == 0 )
    {
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
        const auto nFileSize = VSIFTellL(poOpenInfo->fpL);
        VSIFSeekL(poOpenInfo->fpL, nFileSize - (METADATASIZE_SIZE + SIGNATURE_SIZE), SEEK_SET);
        uint32_t nMetadataSize = 0;
        static_assert(sizeof(nMetadataSize) == METADATASIZE_SIZE, "sizeof(nMetadataSize) == METADATASIZE_SIZE");
        VSIFReadL(&nMetadataSize, 1, sizeof(nMetadataSize), poOpenInfo->fpL);
        CPL_LSBPTR32(&nMetadataSize);
        unsigned char abyTrailingBytes[SIGNATURE_SIZE] = {0};
        VSIFReadL(&abyTrailingBytes[0], 1, SIGNATURE_SIZE, poOpenInfo->fpL);
        bRet = memcmp(abyTrailingBytes, SIGNATURE, SIGNATURE_SIZE) == 0 &&
               nMetadataSize < nFileSize;
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
    }
    return bRet;
}

#ifdef GDAL_USE_ARROWDATASET

/************************************************************************/
/*                         VSIArrowFileSystem                           */
/************************************************************************/

class VSIArrowFileSystem final: public arrow::fs::FileSystem
{
    const std::string m_osQueryParameters;

public:

    explicit VSIArrowFileSystem(const std::string& osQueryParameters):
        m_osQueryParameters(osQueryParameters)
    {}

    std::string type_name() const override { return "vsi"; }

    using arrow::fs::FileSystem::Equals;
    bool Equals(const arrow::fs::FileSystem& other) const override
    {
        const auto poOther = dynamic_cast<const VSIArrowFileSystem*>(&other);
        return poOther != nullptr && poOther->m_osQueryParameters == m_osQueryParameters;
    }

    using arrow::fs::FileSystem::GetFileInfo;
    arrow::Result<arrow::fs::FileInfo> GetFileInfo(const std::string& path) override
    {
        auto fileType = arrow::fs::FileType::Unknown;
        VSIStatBufL sStat;
        if( VSIStatL(path.c_str(), &sStat) == 0 )
        {
            if( VSI_ISREG(sStat.st_mode) )
                fileType = arrow::fs::FileType::File;
            else if( VSI_ISDIR(sStat.st_mode) )
                fileType = arrow::fs::FileType::Directory;
        }
        else
        {
            fileType = arrow::fs::FileType::NotFound;
        }
        arrow::fs::FileInfo info(path, fileType);
        if( fileType == arrow::fs::FileType::File )
            info.set_size(sStat.st_size);
        return info;
    }

    arrow::Result<arrow::fs::FileInfoVector> GetFileInfo(
                        const arrow::fs::FileSelector& select) override
    {
        arrow::fs::FileInfoVector res;
        VSIDIR* psDir = VSIOpenDir(select.base_dir.c_str(),
                                   select.recursive ? -1 : 0,
                                   nullptr);
        if( psDir == nullptr )
            return res;

        bool bParquetFound = false;
        const int nMaxNonParquetFiles = atoi(
            CPLGetConfigOption("OGR_PARQUET_MAX_NON_PARQUET_FILES", "100"));
        const int nMaxListedFiles = atoi(
            CPLGetConfigOption("OGR_PARQUET_MAX_LISTED_FILES", "1000000"));
        while( const auto psEntry = VSIGetNextDirEntry(psDir) )
        {
            if( !bParquetFound )
                bParquetFound = EQUAL(CPLGetExtension(psEntry->pszName), "parquet");

            const std::string osFilename =
                select.base_dir + '/' + psEntry->pszName;
            int nMode = psEntry->nMode;
            if( !psEntry->bModeKnown )
            {
                VSIStatBufL sStat;
                if( VSIStatL(osFilename.c_str(), &sStat) == 0 )
                    nMode = sStat.st_mode;
            }

            auto fileType = arrow::fs::FileType::Unknown;
            if( VSI_ISREG(nMode) )
                fileType = arrow::fs::FileType::File;
            else if( VSI_ISDIR(nMode) )
                fileType = arrow::fs::FileType::Directory;

            arrow::fs::FileInfo info(osFilename, fileType);
            if( fileType == arrow::fs::FileType::File &&
                psEntry->bSizeKnown )
            {
                info.set_size(psEntry->nSize);
            }
            res.push_back(info);

            // Avoid iterating over too many files if there's no likely parquet
            // files.
            if( static_cast<int>(res.size()) == nMaxNonParquetFiles && !bParquetFound )
                break;
            if( static_cast<int>(res.size()) == nMaxListedFiles )
                break;
        }
        VSICloseDir(psDir);
        return res;
    }

    arrow::Status CreateDir(const std::string& /*path*/, bool /*recursive*/ = true) override
    {
        return arrow::Status::IOError("CreateDir() unimplemented");
    }

    arrow::Status DeleteDir(const std::string& /*path*/) override
    {
        return arrow::Status::IOError("DeleteDir() unimplemented");
    }

    arrow::Status DeleteDirContents(const std::string& /*path*/
#if ARROW_VERSION_MAJOR >= 8
                                    , bool /*missing_dir_ok*/ = false
#endif
                                   ) override
    {
        return arrow::Status::IOError("DeleteDirContents() unimplemented");
    }

    arrow::Status DeleteRootDirContents() override
    {
        return arrow::Status::IOError("DeleteRootDirContents() unimplemented");
    }

    arrow::Status DeleteFile(const std::string& /*path*/) override
    {
        return arrow::Status::IOError("DeleteFile() unimplemented");
    }

    arrow::Status Move(const std::string& /*src*/, const std::string& /*dest*/) override
    {
        return arrow::Status::IOError("Move() unimplemented");
    }

    arrow::Status CopyFile(const std::string& /*src*/, const std::string& /*dest*/) override
    {
        return arrow::Status::IOError("CopyFile() unimplemented");
    }

    using arrow::fs::FileSystem::OpenInputStream;
    arrow::Result<std::shared_ptr<arrow::io::InputStream>> OpenInputStream(const std::string& path) override
    {
        return OpenInputFile(path);
    }

    using arrow::fs::FileSystem::OpenInputFile;
    arrow::Result<std::shared_ptr<arrow::io::RandomAccessFile>> OpenInputFile(const std::string& path) override
    {
        std::string osPath(path);
        osPath += m_osQueryParameters;
        CPLDebugOnly("PARQUET", "Opening %s", osPath.c_str());
        VSILFILE* fp = VSIFOpenL(osPath.c_str(), "rb");
        if( fp == nullptr )
            return arrow::Status::IOError("OpenInputFile() failed for " + osPath);
        return std::make_shared<OGRArrowRandomAccessFile>(fp);
    }

    using arrow::fs::FileSystem::OpenOutputStream;
    arrow::Result<std::shared_ptr<arrow::io::OutputStream>> OpenOutputStream(
        const std::string& /*path*/,
        const std::shared_ptr<const arrow::KeyValueMetadata>& /* metadata */ ) override
    {
        return arrow::Status::IOError("OpenOutputStream() unimplemented");
    }

    arrow::Result<std::shared_ptr<arrow::io::OutputStream>> OpenAppendStream(
        const std::string& /*path*/,
        const std::shared_ptr<const arrow::KeyValueMetadata>& /* metadata */ ) override
    {
        return arrow::Status::IOError("OpenAppendStream() unimplemented");
    }
};

/************************************************************************/
/*                      OpenFromDatasetFactory()                        */
/************************************************************************/

static GDALDataset* OpenFromDatasetFactory(const std::string& osBasePath,
                                           const std::shared_ptr<arrow::dataset::DatasetFactory>& factory)
{
    std::shared_ptr<arrow::dataset::Dataset> dataset;
    PARQUET_ASSIGN_OR_THROW(dataset, factory->Finish());

    std::shared_ptr<arrow::dataset::ScannerBuilder> scannerBuilder;
    PARQUET_ASSIGN_OR_THROW(scannerBuilder, dataset->NewScan());

    auto poMemoryPool = arrow::MemoryPool::CreateDefault();

    PARQUET_THROW_NOT_OK(scannerBuilder->Pool(poMemoryPool.get()));

    const bool bIsVSI = STARTS_WITH(osBasePath.c_str(), "/vsi");
    if( bIsVSI )
    {
        PARQUET_THROW_NOT_OK(scannerBuilder->FragmentReadahead(2));
        // scannerBuilder->BatchSize(10);
    }

    std::shared_ptr<arrow::dataset::Scanner> scanner;
    PARQUET_ASSIGN_OR_THROW(scanner, scannerBuilder->Finish());

    auto poDS = cpl::make_unique<OGRParquetDataset>(std::move(poMemoryPool));
    auto poLayer = cpl::make_unique<OGRParquetDatasetLayer>(
                        poDS.get(),
                        CPLGetBasename(osBasePath.c_str()),
                        scanner,
                        scannerBuilder->schema());
    poDS->SetLayer(std::move(poLayer));
    return poDS.release();
}

/************************************************************************/
/*                         GetFileSystem()                              */
/************************************************************************/

static  std::shared_ptr<arrow::fs::FileSystem> GetFileSystem(std::string& osBasePathInOut,
                                                             const std::string& osQueryParameters)
{
    // Instanciate file system:
    // - VSIArrowFileSystem implementation for /vsi files
    // - base implementation for local files
    std::shared_ptr<arrow::fs::FileSystem> fs;
    const bool bIsVSI = STARTS_WITH(osBasePathInOut.c_str(), "/vsi");
    if( bIsVSI ||
        CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_VSI", "NO")) )
    {
        fs = std::make_shared<VSIArrowFileSystem>(osQueryParameters);
    }
    else
    {
        // FileSystemFromUriOrPath() doesn't like relative paths
        // so transform them to absolute.
        std::string osPath(osBasePathInOut);
        if( CPLIsFilenameRelative(osPath.c_str()) )
        {
            char* pszCurDir = CPLGetCurrentDir();
            if( pszCurDir == nullptr )
                return nullptr;
            osPath = CPLFormFilename(pszCurDir, osPath.c_str(), nullptr);
            CPLFree(pszCurDir);
        }
        PARQUET_ASSIGN_OR_THROW(
            fs,
            arrow::fs::FileSystemFromUriOrPath(osPath));
    }
    return fs;
}

/************************************************************************/
/*                  OpenParquetDatasetWithMetadata()                    */
/************************************************************************/

static GDALDataset* OpenParquetDatasetWithMetadata(const std::string& osBasePathIn,
                                                   const char* pszMetadataFile,
                                                   const std::string& osQueryParameters)
{
    std::string osBasePath(osBasePathIn);
    auto fs = GetFileSystem(osBasePath, osQueryParameters);

    arrow::dataset::ParquetFactoryOptions options;
    auto partitioningFactory = arrow::dataset::HivePartitioning::MakeFactory();
    options.partitioning = arrow::dataset::PartitioningOrFactory(partitioningFactory);

    std::shared_ptr<arrow::dataset::DatasetFactory> factory;
    PARQUET_ASSIGN_OR_THROW(
        factory,
        arrow::dataset::ParquetDatasetFactory::Make(
            osBasePath + '/' + pszMetadataFile,
            fs,
            std::make_shared<arrow::dataset::ParquetFileFormat>(),
            options));

    return OpenFromDatasetFactory(osBasePath, factory);
}

/************************************************************************/
/*                 OpenParquetDatasetWithoutMetadata()                  */
/************************************************************************/

static GDALDataset* OpenParquetDatasetWithoutMetadata(const std::string& osBasePathIn,
                                                      const std::string& osQueryParameters)
{
    std::string osBasePath(osBasePathIn);
    auto fs = GetFileSystem(osBasePath, osQueryParameters);

    arrow::dataset::FileSystemFactoryOptions options;
    auto partitioningFactory = arrow::dataset::HivePartitioning::MakeFactory();
    options.partitioning = arrow::dataset::PartitioningOrFactory(partitioningFactory);

    arrow::fs::FileSelector selector;
    selector.base_dir = osBasePath;
    selector.recursive = true;

    std::shared_ptr<arrow::dataset::DatasetFactory> factory;
    PARQUET_ASSIGN_OR_THROW(
        factory,
        arrow::dataset::FileSystemDatasetFactory::Make(
            fs,
            selector,
            std::make_shared<arrow::dataset::ParquetFileFormat>(),
            options));

    return OpenFromDatasetFactory(osBasePath, factory);
}

#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRParquetDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( poOpenInfo->eAccess == GA_Update )
        return nullptr;

#ifdef GDAL_USE_ARROWDATASET
    std::string osBasePath(poOpenInfo->pszFilename);
    std::string osQueryParameters;
    const bool bStartedWithParquetPrefix = STARTS_WITH(osBasePath.c_str(), "PARQUET:");

    if( bStartedWithParquetPrefix )
    {
        osBasePath = osBasePath.substr(strlen("PARQUET:"));
    }

    // Little trick to allow using syntax of
    // https://github.com/opengeospatial/geoparquet/discussions/101
    // ogrinfo "/vsicurl/https://ai4edataeuwest.blob.core.windows.net/us-census/2020/cb_2020_us_vtd_500k.parquet?${SAS_TOKEN}"
    if( STARTS_WITH(osBasePath.c_str(), "/vsicurl/") )
    {
        const auto nPos = osBasePath.find(".parquet?st=");
        if( nPos != std::string::npos )
        {
            osQueryParameters = osBasePath.substr(nPos + strlen(".parquet"));
            osBasePath.resize(nPos + strlen(".parquet"));
        }
    }

    if( bStartedWithParquetPrefix || poOpenInfo->bIsDirectory ||
        !osQueryParameters.empty() )
    {
        VSIStatBufL sStat;
        if( !osBasePath.empty() && osBasePath.back() == '/' )
            osBasePath.resize(osBasePath.size() - 1);
        std::string osMetadataPath =
            CPLFormFilename(osBasePath.c_str(), "_metadata", nullptr);
        if( CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_METADATA_FILE", "YES")) &&
            VSIStatL( (osMetadataPath + osQueryParameters).c_str(), &sStat ) == 0 )
        {
            // If there's a _metadata file, then use it to avoid listing files
            try
            {
                return OpenParquetDatasetWithMetadata(osBasePath,
                                                      "_metadata",
                                                      osQueryParameters);
            }
            catch( const std::exception& e)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Parquet exception: %s", e.what());
            }
            return nullptr;
        }
        else
        {
            bool bLikelyParquetDataset = false;
            if( poOpenInfo->bIsDirectory )
            {
                // Detect if the directory contains .parquet files, or
                // subdirectories with a name of the form "key=value", typical
                // of HIVE partitioning.
                char** papszFiles = VSIReadDir(osBasePath.c_str());
                for( char** papszIter = papszFiles; papszIter && *papszIter; ++papszIter )
                {
                    if( EQUAL(CPLGetExtension(*papszIter), "parquet") )
                    {
                        bLikelyParquetDataset = true;
                        break;
                    }
                    else if( strchr(*papszIter, '=') )
                    {
                        // HIVE partitioning
                        if( VSIStatL( CPLFormFilename(osBasePath.c_str(),
                                        *papszIter, nullptr), &sStat ) == 0 &&
                            VSI_ISDIR(sStat.st_mode) )
                        {
                            bLikelyParquetDataset = true;
                            break;
                        }
                    }
                }
                CSLDestroy(papszFiles);
            }

            if( bStartedWithParquetPrefix || bLikelyParquetDataset )
            {
                try
                {
                    return OpenParquetDatasetWithoutMetadata(osBasePath,
                                                             osQueryParameters);
                }
                catch( const std::exception& e)
                {
                    // If we aren't quite sure that the passed file name is
                    // a directory, then silently continue
                    if( poOpenInfo->bIsDirectory )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Parquet exception: %s", e.what());
                        return nullptr;
                    }
                }
            }
        }

        if( poOpenInfo->bIsDirectory )
            return nullptr;
    }
#endif

    if( !OGRParquetDriverIdentify(poOpenInfo) )
    {
        return nullptr;
    }

    std::string osFilename(poOpenInfo->pszFilename);
    if( STARTS_WITH(poOpenInfo->pszFilename, "PARQUET:") )
    {
        osFilename = poOpenInfo->pszFilename + strlen("PARQUET:");
    }

    try
    {
        std::shared_ptr<arrow::io::RandomAccessFile> infile;
        if( STARTS_WITH(osFilename.c_str(), "/vsi") ||
            CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_VSI", "NO")) )
        {
            VSILFILE* fp = poOpenInfo->fpL;
            if( fp == nullptr )
            {
                fp = VSIFOpenL(osFilename.c_str(), "rb");
                if( fp == nullptr )
                    return nullptr;
            }
            poOpenInfo->fpL = nullptr;
            infile = std::make_shared<OGRArrowRandomAccessFile>(fp);
        }
        else
        {
            PARQUET_ASSIGN_OR_THROW(
              infile,
              arrow::io::ReadableFile::Open(osFilename));
        }

        // Open Parquet file reader
        std::unique_ptr<parquet::arrow::FileReader> arrow_reader;
        auto poMemoryPool = arrow::MemoryPool::CreateDefault();
        auto st = parquet::arrow::OpenFile(infile, poMemoryPool.get(), &arrow_reader);
        if( !st.ok() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "parquet::arrow::OpenFile() failed");
            return nullptr;
        }

        auto poDS = cpl::make_unique<OGRParquetDataset>(std::move(poMemoryPool));
        auto poLayer = cpl::make_unique<OGRParquetLayer>(
            poDS.get(),
            CPLGetBasename(osFilename.c_str()),
            std::move(arrow_reader));
        poDS->SetLayer(std::move(poLayer));
        return poDS.release();
    }
    catch( const std::exception& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Parquet exception: %s", e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset* OGRParquetDriverCreate(const char * pszName,
                                           int nXSize, int nYSize, int nBands,
                                           GDALDataType eType,
                                           char ** /* papszOptions */ )
{
    if( !(nXSize == 0 && nYSize == 0 && nBands == 0 && eType == GDT_Unknown) )
        return nullptr;

    try
    {
        std::shared_ptr<arrow::io::OutputStream> out_file;
        if( STARTS_WITH(pszName, "/vsi") ||
            CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_VSI", "YES")) )
        {
            VSILFILE* fp = VSIFOpenL(pszName, "wb");
            if( fp == nullptr )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot create %s", pszName);
                return nullptr;
            }
            out_file = std::make_shared<OGRArrowWritableFile>(fp);
        }
        else
        {
            PARQUET_ASSIGN_OR_THROW(
                out_file, arrow::io::FileOutputStream::Open(pszName));
        }

        return new OGRParquetWriterDataset(out_file);
    }
    catch( const std::exception& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Parquet exception: %s", e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                         OGRParquetDriver()                           */
/************************************************************************/

class OGRParquetDriver final: public GDALDriver
{
    bool m_bMetadataInitialized = false;
    void InitMetadata();

public:
    const char* GetMetadataItem(const char* pszName, const char* pszDomain) override
    {
        if( EQUAL(pszName, GDAL_DS_LAYER_CREATIONOPTIONLIST) )
        {
            InitMetadata();
        }
        return GDALDriver::GetMetadataItem(pszName, pszDomain);
    }

    char** GetMetadata(const char* pszDomain) override
    {
        InitMetadata();
        return GDALDriver::GetMetadata(pszDomain);
    }
};

void OGRParquetDriver::InitMetadata()
{
    if( m_bMetadataInitialized )
        return;
    m_bMetadataInitialized = true;

    CPLXMLTreeCloser oTree(CPLCreateXMLNode(
                        nullptr, CXT_Element, "LayerCreationOptionList"));

    std::vector<const char*> apszCompressionMethods;
    bool bHasSnappy = false;
    for( const char* pszMethod: { "SNAPPY",
                                  "GZIP",
                                  "BROTLI",
                                  "ZSTD",
                                  "LZ4",
                                  "LZ4_FRAME",
                                  "LZO",
                                  "BZ2",
                                  "LZ4_HADOOP" } )
    {
        auto oResult = arrow::util::Codec::GetCompressionType(
                        CPLString(pszMethod).tolower());
        if( oResult.ok() && arrow::util::Codec::IsAvailable(*oResult) )
        {
            if( EQUAL(pszMethod, "SNAPPY") )
                bHasSnappy = true;
            apszCompressionMethods.emplace_back(pszMethod);
        }
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "COMPRESSION");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "description", "Compression method");
        CPLAddXMLAttributeAndValue(psOption, "default", bHasSnappy ? "SNAPPY" : "NONE");
        {
            auto poValueNode = CPLCreateXMLNode(psOption, CXT_Element, "Value");
            CPLAddXMLAttributeAndValue(poValueNode, "alias", "UNCOMPRESSED");
            CPLCreateXMLNode(poValueNode, CXT_Text, "NONE");
        }
        for( const char* pszMethod: apszCompressionMethods )
        {
            auto poValueNode = CPLCreateXMLNode(psOption, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, pszMethod);
        }
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "GEOMETRY_ENCODING");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "description", "Encoding of geometry columns");
        CPLAddXMLAttributeAndValue(psOption, "default", "WKB");
        for( const char* pszEncoding : {"WKB", "WKT", "GEOARROW"} )
        {
            auto poValueNode = CPLCreateXMLNode(psOption, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, pszEncoding);
        }
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "ROW_GROUP_SIZE");
        CPLAddXMLAttributeAndValue(psOption, "type", "integer");
        CPLAddXMLAttributeAndValue(psOption, "description", "Maximum number of rows per group");
        CPLAddXMLAttributeAndValue(psOption, "default", "65536");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "GEOMETRY_NAME");
        CPLAddXMLAttributeAndValue(psOption, "type", "string");
        CPLAddXMLAttributeAndValue(psOption, "description", "Name of geometry column");
        CPLAddXMLAttributeAndValue(psOption, "default", "geometry");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "FID");
        CPLAddXMLAttributeAndValue(psOption, "type", "string");
        CPLAddXMLAttributeAndValue(psOption, "description", "Name of the FID column to create");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "POLYGON_ORIENTATION");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "description", "Which ring orientation to use for polygons");
        CPLAddXMLAttributeAndValue(psOption, "default", "COUNTERCLOCKWISE");
        CPLCreateXMLElementAndValue(psOption, "Value", "COUNTERCLOCKWISE");
        CPLCreateXMLElementAndValue(psOption, "Value", "UNMODIFIED");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "EDGES");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "description", "Name of the coordinate system for the edges");
        CPLAddXMLAttributeAndValue(psOption, "default", "PLANAR");
        CPLCreateXMLElementAndValue(psOption, "Value", "PLANAR");
        CPLCreateXMLElementAndValue(psOption, "Value", "SPHERICAL");
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "CREATOR");
        CPLAddXMLAttributeAndValue(psOption, "type", "string");
        CPLAddXMLAttributeAndValue(psOption, "description", "Name of creating application");
    }

    char* pszXML = CPLSerializeXMLTree(oTree.get());
    GDALDriver::SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST, pszXML);
    CPLFree(pszXML);
}

/************************************************************************/
/*                         RegisterOGRParquet()                         */
/************************************************************************/

void RegisterOGRParquet()
{
    if( GDALGetDriverByName( "Parquet" ) != nullptr )
        return;

    auto poDriver = cpl::make_unique<OGRParquetDriver>();

    poDriver->SetDescription( "Parquet" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "(Geo)Parquet" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "parquet" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/parquet.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date Time DateTime "
                               "Binary IntegerList Integer64List RealList StringList" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                               "Boolean Int16 Float32 JSON UUID" );

    poDriver->pfnOpen = OGRParquetDriverOpen;
    poDriver->pfnIdentify = OGRParquetDriverIdentify;
    poDriver->pfnCreate = OGRParquetDriverCreate;

#ifdef GDAL_USE_ARROWDATASET
    poDriver->SetMetadataItem("ARROW_DATASET", "YES");
#endif

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
