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
        return -1;
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

/************************************************************************/
/*                           Delete()                                   */
/************************************************************************/

static CPLErr OGRFlatGoBufDriverDelete( const char *pszDataSource )

{
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszDataSource, &sStatBuf ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s does not appear to be a file or directory.",
                  pszDataSource );

        return CE_Failure;
    }

    if( VSI_ISREG(sStatBuf.st_mode) )
    {
        VSIUnlink( pszDataSource );
        return CE_None;
    }

    if( VSI_ISDIR(sStatBuf.st_mode) )
    {
        char **papszDirEntries = VSIReadDir( pszDataSource );

        for( int iFile = 0;
             papszDirEntries != nullptr && papszDirEntries[iFile] != nullptr;
             iFile++ )
        {
            if( EQUAL(CPLGetExtension(papszDirEntries[iFile]), "fgb") )
            {
                VSIUnlink( CPLFormFilename( pszDataSource,
                                            papszDirEntries[iFile],
                                            nullptr ) );
            }
        }

        CSLDestroy( papszDirEntries );

        VSIRmdir( pszDataSource );
    }

    return CE_None;
}

/************************************************************************/
/*                       RegisterOGRFlatGeobuf()                        */
/************************************************************************/

void RegisterOGRFlatGeobuf()
{
    if( GDALGetDriverByName("FlatGeobuf") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("FlatGeobuf");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "FlatGeobuf");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "fgb");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/flatgeobuf.html");
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
    poDriver->pfnDelete = OGRFlatGoBufDriverDelete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}


/************************************************************************/
/*                         OGRFlatGeobufDataset()                       */
/************************************************************************/


OGRFlatGeobufDataset::OGRFlatGeobufDataset(const char *pszName, bool bIsDir,
                                           bool bCreate):
    m_bCreate(bCreate),
    m_bIsDir(bIsDir)
{
    SetDescription(pszName);
}

/************************************************************************/
/*                         ~OGRFlatGeobufDataset()                      */
/************************************************************************/

OGRFlatGeobufDataset::~OGRFlatGeobufDataset()
{
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *OGRFlatGeobufDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( OGRFlatGeobufDriverIdentify(poOpenInfo) == FALSE ||
        poOpenInfo->eAccess == GA_Update )
    {
        return nullptr;
    }

    auto bVerifyBuffers = CPLFetchBool( poOpenInfo->papszOpenOptions, "VERIFY_BUFFERS", true );

    auto poDS = std::unique_ptr<OGRFlatGeobufDataset>(
        new OGRFlatGeobufDataset(poOpenInfo->pszFilename,
                                 CPL_TO_BOOL(poOpenInfo->bIsDirectory),
                                 false));

    if( poOpenInfo->bIsDirectory )
    {
        CPLStringList aosFiles(VSIReadDir(poOpenInfo->pszFilename));
        int nCountFGB = 0;
        int nCountNonFGB = 0;
        for( int i = 0; i < aosFiles.size(); i++ )
        {
            if( strcmp(aosFiles[i], ".") == 0 || strcmp(aosFiles[i], "..") == 0 )
                continue;
            if( EQUAL(CPLGetExtension(aosFiles[i]), "fgb") )
                nCountFGB ++;
            else
                nCountNonFGB ++;
        }
        // Consider that a directory is a FlatGeobuf dataset if there is a
        // majority of .fgb files in it
        if( nCountFGB == 0 || nCountFGB < nCountNonFGB )
        {
            return nullptr;
        }
        for( int i = 0; i < aosFiles.size(); i++ )
        {
            if( EQUAL(CPLGetExtension(aosFiles[i]), "fgb") )
            {
                CPLString osFilename(
                    CPLFormFilename(poOpenInfo->pszFilename, aosFiles[i], nullptr) );
                VSILFILE* fp = VSIFOpenL(osFilename, "rb");
                if( fp )
                {
                    poDS->OpenFile(osFilename, fp, bVerifyBuffers);
                    VSIFCloseL(fp);
                }
            }
        }
    }
    else
    {
        if( poOpenInfo->fpL == nullptr ||
            !poDS->OpenFile(poOpenInfo->pszFilename, poOpenInfo->fpL, bVerifyBuffers) )
        {
            return nullptr;
        }
    }
    return poDS.release();
}

/************************************************************************/
/*                           OpenFile()                                 */
/************************************************************************/

bool OGRFlatGeobufDataset::OpenFile(const char* pszFilename, VSILFILE* fp, bool bVerifyBuffers)
{
    uint64_t offset = sizeof(magicbytes);
    CPLDebug("FlatGeobuf", "Start at offset (%lu)", static_cast<long unsigned int>(offset));
    if (VSIFSeekL(fp, offset, SEEK_SET) == -1) {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to get seek in file");
        return false;
    }
    uint32_t headerSize;
    if (VSIFReadL(&headerSize, 4, 1, fp) != 1) {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to read header size");
        return false;
    }
    CPL_LSBPTR32(&headerSize);
    CPLDebug("FlatGeobuf", "headerSize (%d)", headerSize);
    if (headerSize > header_max_buffer_size) {
        CPLError(CE_Failure, CPLE_AppDefined, "Header size too large (> 1MB)");
        return false;
    }
    std::unique_ptr<GByte, CPLFreeReleaser> buf(static_cast<GByte*>(VSIMalloc(headerSize)));
    if (buf == nullptr) {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to allocate memory for header");
        return false;
    }
    if (VSIFReadL(buf.get(), 1, headerSize, fp) != headerSize) {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to read header");
        return false;
    }
    if (bVerifyBuffers) {
        flatbuffers::Verifier v(buf.get(), headerSize);
        auto ok = VerifyHeaderBuffer(v);
        if (!ok) {
            CPLError(CE_Failure, CPLE_AppDefined, "Header failed consistency verification");
            return false;
        }
    }
    auto header = GetHeader(buf.get());
    offset += 4 + headerSize;
    CPLDebug("FlatGeobuf", "Add headerSize to offset (%d)", 4 + headerSize);

    auto featuresCount = header->features_count();

    if (featuresCount > std::numeric_limits<size_t>::max() / 8) {
        CPLError(CE_Failure, CPLE_AppDefined, "Too many features for this architecture");
        return false;
    }

    auto index_node_size = header->index_node_size();
    if (index_node_size > 0) {
        try {
            auto treeSize = PackedRTree::size(featuresCount);
            offset += treeSize;
            CPLDebug("FlatGeobuf", "Add treeSize to offset (%lu)", static_cast<long unsigned int>(treeSize));
        } catch (const std::exception& e) {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to calculate tree size: %s", e.what());
            return false;
        }
    }
    offset += featuresCount * 8;
    CPLDebug("FlatGeobuf", "Add featuresCount * 8 to offset (%lu)", static_cast<long unsigned int>(featuresCount * 8));

    CPLDebug("FlatGeobuf", "Features start at offset (%lu)", static_cast<long unsigned int>(offset));

    auto poLayer = std::unique_ptr<OGRFlatGeobufLayer>(
        new OGRFlatGeobufLayer(header, buf.release(), pszFilename, offset));
    poLayer->VerifyBuffers(bVerifyBuffers);

    m_apoLayers.push_back(std::move(poLayer));

    return true;
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

    bool bIsDir = false;
    if( !EQUAL(CPLGetExtension(pszName), "fgb") )
    {
        if( VSIMkdir(pszName, 0755) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create directory %s:\n%s",
                     pszName, VSIStrerror(errno));
            return nullptr;
        }
        bIsDir = true;
    }

    return new OGRFlatGeobufDataset(pszName, bIsDir, true);
}

OGRLayer* OGRFlatGeobufDataset::GetLayer( int iLayer ) {
    if( iLayer < 0 || iLayer >= GetLayerCount() )
        return nullptr;
    return m_apoLayers[iLayer].get();
}

int OGRFlatGeobufDataset::TestCapability( const char * pszCap )
{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return m_bCreate && (m_bIsDir || m_apoLayers.empty());
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return m_bCreate;
    else if (EQUAL(pszCap, OLCCreateGeomField))
        return m_bCreate;
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

/************************************************************************/
/*                        LaunderLayerName()                            */
/************************************************************************/

static CPLString LaunderLayerName(const char* pszLayerName)
{
    std::string osRet(CPLLaunderForFilename(pszLayerName, nullptr));
    if( osRet != pszLayerName )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Invalid layer name for a file name: %s. Laundered to %s.",
                 pszLayerName, osRet.c_str());
    }
    return osRet;
}

OGRLayer* OGRFlatGeobufDataset::ICreateLayer( const char *pszLayerName,
                                OGRSpatialReference *poSpatialRef,
                                OGRwkbGeometryType eGType,
                                char **papszOptions )
{
    // Verify we are in update mode.
    if( !m_bCreate )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.\n"
                 "New layer %s cannot be created.",
                 GetDescription(), pszLayerName);

        return nullptr;
    }
    if( !m_bIsDir && !m_apoLayers.empty() )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Can create only one single layer in a .fgb file. "
                 "Use a directory output for multiple layers");

        return nullptr;
    }

    // Verify that the datasource is a directory.
    VSIStatBufL sStatBuf;

    // What filename would we use?
    CPLString osFilename;

    if( m_bIsDir )
        osFilename = CPLFormFilename(GetDescription(),
                                LaunderLayerName(pszLayerName).c_str(), "fgb");
    else
        osFilename = GetDescription();

    // Does this directory/file already exist?
    if( VSIStatL(osFilename, &sStatBuf) == 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Attempt to create layer %s, but %s already exists.",
                 pszLayerName, osFilename.c_str());
        return nullptr;
    }

    // Create a layer.
    auto poLayer = std::unique_ptr<OGRFlatGeobufLayer>(
        new OGRFlatGeobufLayer(pszLayerName, osFilename, poSpatialRef, eGType));

    poLayer->CreateSpatialIndexAtClose(
        CPLFetchBool( papszOptions, "SPATIAL_INDEX", true ) );

    m_apoLayers.push_back(std::move(poLayer));

    return m_apoLayers.back().get();
}

/************************************************************************/
//                            GetFileList()                             */
/************************************************************************/

char** OGRFlatGeobufDataset::GetFileList()
{
    CPLStringList oFileList;
    for( const auto& poLayer: m_apoLayers )
    {
        oFileList.AddString( poLayer->GetFilename().c_str() );
    }
    return oFileList.StealList();
}
