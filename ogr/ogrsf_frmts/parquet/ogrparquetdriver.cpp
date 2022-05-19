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

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRParquetDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( !OGRParquetDriverIdentify(poOpenInfo) ||
        poOpenInfo->eAccess == GA_Update )
    {
        return nullptr;
    }

    try
    {
        std::shared_ptr<arrow::io::RandomAccessFile> infile;
        if( STARTS_WITH(poOpenInfo->pszFilename, "/vsi") ||
            CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_VSI", "NO")) )
        {
            VSILFILE* fp = poOpenInfo->fpL;
            poOpenInfo->fpL = nullptr;
            infile = std::make_shared<OGRArrowRandomAccessFile>(fp);
        }
        else
        {
            PARQUET_ASSIGN_OR_THROW(
              infile,
              arrow::io::ReadableFile::Open(poOpenInfo->pszFilename));
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
            CPLGetBasename(poOpenInfo->pszFilename),
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

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
