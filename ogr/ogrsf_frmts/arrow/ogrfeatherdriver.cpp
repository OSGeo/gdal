/******************************************************************************
 *
 * Project:  Feather Translator
 * Purpose:  Implements OGRFeatherDriver.
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

#include "ogr_feather.h"
#include "../arrow_common/ograrrowrandomaccessfile.h"
#include "../arrow_common/ograrrowwritablefile.h"
#include "../arrow_common/ograrrowdataset.hpp"

#include "ogrfeatherdrivercore.h"

/************************************************************************/
/*                        IsArrowIPCStream()                            */
/************************************************************************/

static bool IsArrowIPCStream(GDALOpenInfo *poOpenInfo)
{
    // WARNING: if making changes in that method, reflect them in
    // OGRFeatherDriverIsArrowIPCStreamBasic() in ogrfeatherdrivercore.cpp

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "ARROW_IPC_STREAM:"))
        return true;

    constexpr int CONTINUATION_SIZE = 4;  // 0xFFFFFFFF
    constexpr int METADATA_SIZE_SIZE = 4;

    // See
    // https://arrow.apache.org/docs/format/Columnar.html#encapsulated-message-format
    if (poOpenInfo->fpL != nullptr &&
        poOpenInfo->nHeaderBytes >= CONTINUATION_SIZE + METADATA_SIZE_SIZE &&
        memcmp(poOpenInfo->pabyHeader, "\xFF\xFF\xFF\xFF", CONTINUATION_SIZE) ==
            0)
    {
        const char *pszExt = CPLGetExtension(poOpenInfo->pszFilename);
        if (EQUAL(pszExt, "arrows") || EQUAL(pszExt, "ipc"))
            return true;

        const uint32_t nMetadataSize =
            CPL_LSBUINT32PTR(poOpenInfo->pabyHeader + CONTINUATION_SIZE);
        if (strcmp(poOpenInfo->pszFilename, "/vsistdin/") == 0)
        {
            // Padding after metadata and before body is not necessarily present
            // but the body must be at least 4 bytes
            constexpr int PADDING_MAX_SIZE = 4;

            // /vsistdin/ cannot seek back beyond first MB
            if (nMetadataSize >
                1024 * 1024 -
                    (CONTINUATION_SIZE + METADATA_SIZE_SIZE + PADDING_MAX_SIZE))
            {
                return false;
            }
            const int nSizeToRead = CONTINUATION_SIZE + METADATA_SIZE_SIZE +
                                    nMetadataSize + PADDING_MAX_SIZE;
            if (!poOpenInfo->TryToIngest(nSizeToRead))
            {
                return false;
            }

            const std::string osTmpFilename(
                CPLSPrintf("/vsimem/_arrow/%p", poOpenInfo));
            auto fp = VSIVirtualHandleUniquePtr(VSIFileFromMemBuffer(
                osTmpFilename.c_str(), poOpenInfo->pabyHeader, nSizeToRead,
                false));
            auto infile =
                std::make_shared<OGRArrowRandomAccessFile>(std::move(fp));
            auto options = arrow::ipc::IpcReadOptions::Defaults();
            auto result =
                arrow::ipc::RecordBatchStreamReader::Open(infile, options);
            CPLDebug("ARROW", "RecordBatchStreamReader::Open(): %s",
                     result.status().message().c_str());
            VSIUnlink(osTmpFilename.c_str());
            return result.ok();
        }

        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
        const auto nFileSize = VSIFTellL(poOpenInfo->fpL);
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
        if (nMetadataSize >
            nFileSize - (CONTINUATION_SIZE + METADATA_SIZE_SIZE))
            return false;

        // Do not give ownership of poOpenInfo->fpL to infile
        auto infile =
            std::make_shared<OGRArrowRandomAccessFile>(poOpenInfo->fpL, false);
        auto options = arrow::ipc::IpcReadOptions::Defaults();
        auto result =
            arrow::ipc::RecordBatchStreamReader::Open(infile, options);
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
        return result.ok();
    }
    return false;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRFeatherDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess == GA_Update)
    {
        return nullptr;
    }

    const bool bIsStreamingFormat = IsArrowIPCStream(poOpenInfo);
    if (!bIsStreamingFormat && !OGRFeatherDriverIsArrowFileFormat(poOpenInfo))
    {
        return nullptr;
    }

    std::shared_ptr<arrow::io::RandomAccessFile> infile;
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "ARROW_IPC_STREAM:"))
    {
        const std::string osFilename(poOpenInfo->pszFilename +
                                     strlen("ARROW_IPC_STREAM:"));
        auto fp =
            VSIVirtualHandleUniquePtr(VSIFOpenL(osFilename.c_str(), "rb"));
        if (fp == nullptr)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                     osFilename.c_str());
            return nullptr;
        }
        infile = std::make_shared<OGRArrowRandomAccessFile>(std::move(fp));
    }
    else if (STARTS_WITH(poOpenInfo->pszFilename, "/vsi") ||
             CPLTestBool(CPLGetConfigOption("OGR_ARROW_USE_VSI", "NO")))
    {
        VSIVirtualHandleUniquePtr fp(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
        infile = std::make_shared<OGRArrowRandomAccessFile>(std::move(fp));
    }
    else
    {
        auto result = arrow::io::ReadableFile::Open(poOpenInfo->pszFilename);
        if (!result.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ReadableFile::Open() failed with %s",
                     result.status().message().c_str());
            return nullptr;
        }
        infile = *result;
    }

    auto poMemoryPool = std::shared_ptr<arrow::MemoryPool>(
        arrow::MemoryPool::CreateDefault().release());
    auto options = arrow::ipc::IpcReadOptions::Defaults();
    options.memory_pool = poMemoryPool.get();

    auto poDS = std::make_unique<OGRFeatherDataset>(poMemoryPool);
    if (bIsStreamingFormat)
    {
        auto result =
            arrow::ipc::RecordBatchStreamReader::Open(infile, options);
        if (!result.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RecordBatchStreamReader::Open() failed with %s",
                     result.status().message().c_str());
            return nullptr;
        }
        auto poRecordBatchStreamReader = *result;
        const bool bSeekable =
            !STARTS_WITH_CI(poOpenInfo->pszFilename, "ARROW_IPC_STREAM:") &&
            strcmp(poOpenInfo->pszFilename, "/vsistdin/") != 0;
        std::string osLayername = CPLGetBasename(poOpenInfo->pszFilename);
        if (osLayername.empty())
            osLayername = "layer";
        auto poLayer = std::make_unique<OGRFeatherLayer>(
            poDS.get(), osLayername.c_str(), infile, bSeekable, options,
            poRecordBatchStreamReader);
        poDS->SetLayer(std::move(poLayer));

        // Pre-load field domains, as this depends on the first record batch
        auto poLayerPtr = poDS->GetLayer(0);
        const auto poFeatureDefn = poLayerPtr->GetLayerDefn();
        bool bHasReadBatch = false;
        for (int i = 0; i < poFeatureDefn->GetFieldCount(); ++i)
        {
            const auto poFieldDefn = poFeatureDefn->GetFieldDefn(i);
            const auto &osDomainName = poFieldDefn->GetDomainName();
            if (!osDomainName.empty())
            {
                if (!bHasReadBatch)
                {
                    bHasReadBatch = true;
                    delete poLayerPtr->GetNextFeature();
                    poLayerPtr->ResetReading();
                }
                poDS->GetFieldDomain(osDomainName);
            }
        }
    }
    else
    {
        auto result = arrow::ipc::RecordBatchFileReader::Open(infile, options);
        if (!result.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RecordBatchFileReader::Open() failed with %s",
                     result.status().message().c_str());
            return nullptr;
        }
        auto poRecordBatchReader = *result;
        auto poLayer = std::make_unique<OGRFeatherLayer>(
            poDS.get(), CPLGetBasename(poOpenInfo->pszFilename),
            poRecordBatchReader);
        poDS->SetLayer(std::move(poLayer));
    }
    return poDS.release();
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRFeatherDriverCreate(const char *pszName, int nXSize,
                                           int nYSize, int nBands,
                                           GDALDataType eType,
                                           char ** /* papszOptions */)
{
    if (!(nXSize == 0 && nYSize == 0 && nBands == 0 && eType == GDT_Unknown))
        return nullptr;

    std::shared_ptr<arrow::io::OutputStream> out_file;
    if (STARTS_WITH(pszName, "/vsi") ||
        CPLTestBool(CPLGetConfigOption("OGR_ARROW_USE_VSI", "YES")))
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
        auto result = arrow::io::FileOutputStream::Open(pszName);
        if (!result.ok())
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s: %s", pszName,
                     result.status().message().c_str());
            return nullptr;
        }
        out_file = *result;
    }

    return new OGRFeatherWriterDataset(pszName, out_file);
}

/************************************************************************/
/*                         OGRFeatherDriver()                           */
/************************************************************************/

class OGRFeatherDriver final : public GDALDriver
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

void OGRFeatherDriver::InitMetadata()
{
    if (m_bMetadataInitialized)
        return;
    m_bMetadataInitialized = true;

    CPLXMLTreeCloser oTree(
        CPLCreateXMLNode(nullptr, CXT_Element, "LayerCreationOptionList"));

    std::vector<const char *> apszCompressionMethods;
    bool bHasLZ4 = false;
    for (const char *pszMethod : {"ZSTD", "LZ4"})
    {
        auto oResult = arrow::util::Codec::GetCompressionType(
            CPLString(pszMethod).tolower());
        if (oResult.ok() && arrow::util::Codec::IsAvailable(*oResult))
        {
            if (EQUAL(pszMethod, "LZ4"))
                bHasLZ4 = true;
            apszCompressionMethods.emplace_back(pszMethod);
        }
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "FORMAT");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "File format variant");
        for (const char *pszEncoding : {"FILE", "STREAM"})
        {
            auto poValueNode = CPLCreateXMLNode(psOption, CXT_Element, "Value");
            CPLCreateXMLNode(poValueNode, CXT_Text, pszEncoding);
        }
    }

    {
        auto psOption = CPLCreateXMLNode(oTree.get(), CXT_Element, "Option");
        CPLAddXMLAttributeAndValue(psOption, "name", "COMPRESSION");
        CPLAddXMLAttributeAndValue(psOption, "type", "string-select");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Compression method");
        CPLAddXMLAttributeAndValue(psOption, "default",
                                   bHasLZ4 ? "LZ4" : "NONE");
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
        CPLAddXMLAttributeAndValue(psOption, "default", "GEOARROW");
        for (const char *pszEncoding :
             {"GEOARROW", "GEOARROW_INTERLEAVED", "WKB", "WKT"})
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
        CPLAddXMLAttributeAndValue(psOption, "name", "BATCH_SIZE");
        CPLAddXMLAttributeAndValue(psOption, "type", "integer");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Maximum number of rows per batch");
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
        CPLAddXMLAttributeAndValue(psOption, "name", "FID");
        CPLAddXMLAttributeAndValue(psOption, "type", "string");
        CPLAddXMLAttributeAndValue(psOption, "description",
                                   "Name of the FID column to create");
    }

    char *pszXML = CPLSerializeXMLTree(oTree.get());
    GDALDriver::SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST, pszXML);
    CPLFree(pszXML);
}

/************************************************************************/
/*                         RegisterOGRArrow()                           */
/************************************************************************/

void RegisterOGRArrow()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    auto poDriver = std::make_unique<OGRFeatherDriver>();

    OGRFeatherDriverSetCommonMetadata(poDriver.get());

    poDriver->pfnOpen = OGRFeatherDriverOpen;
    poDriver->pfnCreate = OGRFeatherDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
