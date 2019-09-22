/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Implements OGRFlatGeobufDataset class
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2019, Björn Harrtell <bjorn at wololo dot org>
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

#include "ogr_flatgeobuf.h"

#include <memory>

#include "header_generated.h"

static int OGRFlatGeobufDriverIdentify(GDALOpenInfo* poOpenInfo){
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "FGB:") )
        return TRUE;

    if( poOpenInfo->bIsDirectory )
    {
        if( CPLGetValueType(CPLGetFilename(poOpenInfo->pszFilename)) ==
                                                        CPL_VALUE_INTEGER )
        {
            // TODO: what is this?
            return FALSE;
        }
        return FALSE;
    }

    const auto nHeaderBytes = poOpenInfo->nHeaderBytes;
    const auto pabyHeader = poOpenInfo->pabyHeader;

    if(nHeaderBytes < 4)
        return FALSE;

    if( pabyHeader[0] == 0x66 &&
        pabyHeader[1] == 0x67 &&
        pabyHeader[2] == 0x62 ) {
        if (pabyHeader[3] == 0x00) {
            CPLDebug("FlatGeobuf", "Verified magicbytes");
            return TRUE;
        } else {
            CPLError(CE_Failure, CPLE_OpenFailed,
                "Unsupported FlatGeobuf version %d.\n",
                poOpenInfo->pabyHeader[3]);
        }

    }

    return FALSE;
}

void RegisterOGRFlatGeobuf()
{
    if( GDALGetDriverByName("FlatGeobuf") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("FlatGeobuf");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "FlatGeobuf");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "fgb");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drv_flatgeobuf.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime Binary");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean Int16 Float32");
    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index' default='YES'/>"
"</LayerCreationOptionList>");

    poDriver->pfnOpen = OGRFlatGeobufDataset::Open;
    poDriver->pfnCreate = OGRFlatGeobufDataset::Create;
    poDriver->pfnIdentify = OGRFlatGeobufDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}


/************************************************************************/
/*                          OGRFlatGeobufDataset()                          */
/************************************************************************/

OGRFlatGeobufDataset::OGRFlatGeobufDataset()
{

}

OGRFlatGeobufDataset::OGRFlatGeobufDataset(const char *osDirName)
{
    CPLDebug("FlatGeobuf", "Request to create dataset %s", osDirName);
    m_create = true;
    if (osDirName)
        m_osName = osDirName;
}

/************************************************************************/
/*                         ~OGRFlatGeobufDataset()                          */
/************************************************************************/

OGRFlatGeobufDataset::~OGRFlatGeobufDataset()
{
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *OGRFlatGeobufDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( !OGRFlatGeobufDriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update )
        return nullptr;

    VSILFILE *fp = poOpenInfo->fpL;

    auto bVerifyBuffers = CPLFetchBool( poOpenInfo->papszOpenOptions, "VERIFY_BUFFERS", true );

    if (fp == nullptr) {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to get handle to open file");
        return nullptr;
    }

    CPLString osFilename(poOpenInfo->pszFilename);

    uint64_t offset = sizeof(magicbytes);
    CPLDebug("FlatGeobuf", "Start at offset (%lu)", static_cast<long unsigned int>(offset));
    if (VSIFSeekL(fp, offset, SEEK_SET) == -1) {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to get seek in file");
        return nullptr;        
    }
    uint32_t headerSize;
    if (VSIFReadL(&headerSize, 4, 1, fp) != 1) {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to read header size");
        return nullptr;
    }
    CPL_LSBPTR32(&headerSize);
    CPLDebug("FlatGeobuf", "headerSize (%d)", headerSize);
    if (headerSize > header_max_buffer_size) {
        CPLError(CE_Failure, CPLE_AppDefined, "Header size too large (> 1MB)");
        return nullptr;
    }
    std::unique_ptr<GByte, CPLFreeReleaser> buf(static_cast<GByte*>(VSIMalloc(headerSize)));
    if (buf == nullptr) {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to allocate memory for header");
        return nullptr;
    }
    if (VSIFReadL(buf.get(), 1, headerSize, fp) != headerSize) {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to read header");
        return nullptr;
    }
    if (bVerifyBuffers) {
        flatbuffers::Verifier v(buf.get(), headerSize);
        auto ok = VerifyHeaderBuffer(v);
        if (!ok) {
            CPLError(CE_Failure, CPLE_AppDefined, "Header failed consistency verification");
            return nullptr;
        }
    }
    auto header = GetHeader(buf.get());
    offset += 4 + headerSize;
    CPLDebug("FlatGeobuf", "Add headerSize to offset (%d)", 4 + headerSize);

    auto featuresCount = header->features_count();

    if (featuresCount > std::numeric_limits<size_t>::max() / 8) {
        CPLError(CE_Failure, CPLE_AppDefined, "Too many features for this architecture");
        return nullptr;
    }

    auto index_node_size = header->index_node_size();
    if (index_node_size > 0) {
        try {
            auto treeSize = PackedRTree::size(featuresCount);
            offset += treeSize;
            CPLDebug("FlatGeobuf", "Add treeSize to offset (%lu)", static_cast<long unsigned int>(treeSize));
        } catch (const std::exception& e) {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to calculate tree size: %s", e.what());
            return nullptr;
        }
    }
    offset += featuresCount * 8;
    CPLDebug("FlatGeobuf", "Add featuresCount * 8 to offset (%lu)", static_cast<long unsigned int>(featuresCount * 8));

    CPLDebug("FlatGeobuf", "Features start at offset (%lu)", static_cast<long unsigned int>(offset));

    auto poDS = new OGRFlatGeobufDataset();
    poDS->SetDescription(osFilename);

    auto poLayer = new OGRFlatGeobufLayer(header, buf.release(), osFilename, offset);
    poLayer->VerifyBuffers(bVerifyBuffers);

    poDS->m_apoLayers.push_back(std::unique_ptr<OGRLayer>(poLayer));

    return poDS;
}

GDALDataset *OGRFlatGeobufDataset::Create( const char *pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char ** /* papszOptions */)
{
    // First, ensure there isn't any such file yet.
    VSIStatBufL sStatBuf;

    if( VSIStatL(pszName, &sStatBuf) == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "It seems a file system object called '%s' already exists.",
                 pszName);

        return nullptr;
    }

    // If the target is not a simple .fgb then create it as a directory.
    CPLString osDirName;

    if( EQUAL(CPLGetExtension(pszName), "fgb") )
    {
        osDirName = CPLGetPath(pszName);
        if( osDirName == "" )
            osDirName = ".";

        // HACK: CPLGetPath("/vsimem/foo.fgb") = "/vsimem", but this is not
        // recognized afterwards as a valid directory name.
        if( osDirName == "/vsimem" )
            osDirName = "/vsimem/";
    }
    else
    {
        if( STARTS_WITH(pszName, "/vsizip/"))
        {
            // Do nothing.
        }
        else if( !EQUAL(pszName, "/vsistdout/") &&
                 VSIMkdir(pszName, 0755) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create directory %s:\n%s",
                     pszName, VSIStrerror(errno));
            return nullptr;
        }
        osDirName = pszName;
    }

    if( EQUAL(CPLGetExtension(pszName), "fgb") )
    {
        return new OGRFlatGeobufDataset(osDirName);
    }

    CPLError(CE_Failure, CPLE_AppDefined,
                     "Creating empty dataset not yet implemented");

    return nullptr;
}

OGRLayer* OGRFlatGeobufDataset::GetLayer( int iLayer ) {
    if( iLayer < 0 || iLayer >= GetLayerCount() )
        return nullptr;
    return m_apoLayers[iLayer].get();
}

int OGRFlatGeobufDataset::TestCapability( const char * pszCap )
{
    if (EQUAL(pszCap, ODrCCreateDataSource))
        return m_create;
    else if (EQUAL(pszCap, ODsCCreateLayer))
        return m_create;
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return m_create;
    else if (EQUAL(pszCap, OLCCreateGeomField))
        return m_create;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return true;
    else if (EQUAL(pszCap, OLCFastFeatureCount))
        return true;
    else if (EQUAL(pszCap, OLCFastGetExtent))
        return true;
    else if (EQUAL(pszCap, OLCFastSpatialFilter))
        return true;
    else
        return false;
}

OGRLayer* OGRFlatGeobufDataset::ICreateLayer( const char *pszLayerName,
                                OGRSpatialReference *poSpatialRef,
                                OGRwkbGeometryType eGType,
                                char **papszOptions )
{
    // Verify we are in update mode.
    if( !m_create )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.\n"
                 "New layer %s cannot be created.",
                 m_osName.c_str(), pszLayerName);

        return nullptr;
    }

    // Verify that the datasource is a directory.
    VSIStatBufL sStatBuf;

    // What filename would we use?
    CPLString osFilename;

    osFilename = CPLFormFilename(m_osName.c_str(), pszLayerName, "fgb");

    // Does this directory/file already exist?
    if( VSIStatL(osFilename, &sStatBuf) == 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Attempt to create layer %s, but %s already exists.",
                 pszLayerName, osFilename.c_str());
        return nullptr;
    }

    // Create a layer.
    OGRFlatGeobufLayer *poLayer = new OGRFlatGeobufLayer(pszLayerName, osFilename, poSpatialRef, eGType);

    poLayer->CreateSpatialIndexAtClose(
        CPLFetchBool( papszOptions, "SPATIAL_INDEX", true ) );

    m_apoLayers.push_back(
        std::unique_ptr<OGRLayer>(poLayer)
    );

    return poLayer;
}
