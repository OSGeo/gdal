/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Implements OGRFlatGeobufDataset class
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2020, Björn Harrtell <bjorn at wololo dot org>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_flatgeobuf.h"

#include <memory>

#include "header_generated.h"

// For users not using CMake...
#ifndef flatbuffers
#error                                                                         \
    "Make sure to build with -Dflatbuffers=gdal_flatbuffers (for example) to avoid potential conflict of flatbuffers"
#endif

using namespace flatbuffers;
using namespace FlatGeobuf;

static int OGRFlatGeobufDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "FGB:"))
        return TRUE;

    if (poOpenInfo->bIsDirectory)
    {
        return -1;
    }

    const auto nHeaderBytes = poOpenInfo->nHeaderBytes;
    const auto pabyHeader = poOpenInfo->pabyHeader;

    if (nHeaderBytes < 4)
        return FALSE;

    if (pabyHeader[0] == 0x66 && pabyHeader[1] == 0x67 && pabyHeader[2] == 0x62)
    {
        if (pabyHeader[3] == 0x03)
        {
            CPLDebug("FlatGeobuf", "Verified magicbytes");
            return TRUE;
        }
        else
        {
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

static CPLErr OGRFlatGoBufDriverDelete(const char *pszDataSource)

{
    VSIStatBufL sStatBuf;

    if (VSIStatL(pszDataSource, &sStatBuf) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s does not appear to be a file or directory.",
                 pszDataSource);

        return CE_Failure;
    }

    if (VSI_ISREG(sStatBuf.st_mode))
    {
        VSIUnlink(pszDataSource);
        return CE_None;
    }

    if (VSI_ISDIR(sStatBuf.st_mode))
    {
        char **papszDirEntries = VSIReadDir(pszDataSource);

        for (int iFile = 0;
             papszDirEntries != nullptr && papszDirEntries[iFile] != nullptr;
             iFile++)
        {
            if (EQUAL(CPLGetExtensionSafe(papszDirEntries[iFile]).c_str(),
                      "fgb"))
            {
                VSIUnlink(CPLFormFilenameSafe(pszDataSource,
                                              papszDirEntries[iFile], nullptr)
                              .c_str());
            }
        }

        CSLDestroy(papszDirEntries);

        VSIRmdir(pszDataSource);
    }

    return CE_None;
}

/************************************************************************/
/*                       RegisterOGRFlatGeobuf()                        */
/************************************************************************/

void RegisterOGRFlatGeobuf()
{
    if (GDALGetDriverByName("FlatGeobuf") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("FlatGeobuf");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_REORDER_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "FlatGeobuf");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "fgb");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/flatgeobuf.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date DateTime Binary");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Boolean Int16 Float32");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "WidthPrecision Comment AlternativeName");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to "
        "create a spatial index' default='YES'/>"
        "  <Option name='TEMPORARY_DIR' type='string' description='Directory "
        "where temporary file should be created'/>"
        "  <Option name='TITLE' type='string' description='Layer title'/>"
        "  <Option name='DESCRIPTION' type='string' "
        "description='Layer description'/>"
        "</LayerCreationOptionList>");
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='VERIFY_BUFFERS' type='boolean' description='Verify "
        "flatbuffers integrity' default='YES'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_COORDINATE_EPOCH, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS,
                              "Name WidthPrecision AlternativeName Comment");

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
                                           bool bCreate, bool bUpdate)
    : m_bCreate(bCreate), m_bUpdate(bUpdate), m_bIsDir(bIsDir)
{
    SetDescription(pszName);
}

/************************************************************************/
/*                         ~OGRFlatGeobufDataset()                      */
/************************************************************************/

OGRFlatGeobufDataset::~OGRFlatGeobufDataset()
{
    OGRFlatGeobufDataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr OGRFlatGeobufDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (OGRFlatGeobufDataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        for (auto &poLayer : m_apoLayers)
        {
            if (poLayer->Close() != CE_None)
                eErr = CE_Failure;
        }

        if (GDALDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *OGRFlatGeobufDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (OGRFlatGeobufDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

    const auto bVerifyBuffers =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "VERIFY_BUFFERS", true);

    auto isDir = CPL_TO_BOOL(poOpenInfo->bIsDirectory);
    auto bUpdate = poOpenInfo->eAccess == GA_Update;

    if (isDir && bUpdate)
    {
        return nullptr;
    }

    auto poDS = std::unique_ptr<OGRFlatGeobufDataset>(new OGRFlatGeobufDataset(
        poOpenInfo->pszFilename, isDir, false, bUpdate));

    if (poOpenInfo->bIsDirectory)
    {
        CPLStringList aosFiles(VSIReadDir(poOpenInfo->pszFilename));
        int nCountFGB = 0;
        int nCountNonFGB = 0;
        for (int i = 0; i < aosFiles.size(); i++)
        {
            if (strcmp(aosFiles[i], ".") == 0 || strcmp(aosFiles[i], "..") == 0)
                continue;
            if (EQUAL(CPLGetExtensionSafe(aosFiles[i]).c_str(), "fgb"))
                nCountFGB++;
            else
                nCountNonFGB++;
        }
        // Consider that a directory is a FlatGeobuf dataset if there is a
        // majority of .fgb files in it
        if (nCountFGB == 0 || nCountFGB < nCountNonFGB)
        {
            return nullptr;
        }
        for (int i = 0; i < aosFiles.size(); i++)
        {
            if (EQUAL(CPLGetExtensionSafe(aosFiles[i]).c_str(), "fgb"))
            {
                const CPLString osFilename(CPLFormFilenameSafe(
                    poOpenInfo->pszFilename, aosFiles[i], nullptr));
                VSILFILE *fp = VSIFOpenL(osFilename, "rb");
                if (fp)
                {
                    if (!poDS->OpenFile(osFilename, fp, bVerifyBuffers))
                        VSIFCloseL(fp);
                }
            }
        }
    }
    else
    {
        if (poOpenInfo->fpL != nullptr)
        {
            if (poDS->OpenFile(poOpenInfo->pszFilename, poOpenInfo->fpL,
                               bVerifyBuffers))
                poOpenInfo->fpL = nullptr;
        }
        else
        {
            return nullptr;
        }
    }
    return poDS.release();
}

/************************************************************************/
/*                           OpenFile()                                 */
/************************************************************************/

bool OGRFlatGeobufDataset::OpenFile(const char *pszFilename, VSILFILE *fp,
                                    bool bVerifyBuffers)
{
    CPLDebugOnly("FlatGeobuf", "Opening OGRFlatGeobufLayer");
    auto poLayer = std::unique_ptr<OGRFlatGeobufLayer>(
        OGRFlatGeobufLayer::Open(pszFilename, fp, bVerifyBuffers));
    if (!poLayer)
        return false;

    if (m_bUpdate)
    {
        CPLDebugOnly("FlatGeobuf", "Creating OGRFlatGeobufEditableLayer");
        auto poEditableLayer = std::unique_ptr<OGRFlatGeobufEditableLayer>(
            new OGRFlatGeobufEditableLayer(poLayer.release(),
                                           papszOpenOptions));
        m_apoLayers.push_back(std::move(poEditableLayer));
    }
    else
    {
        m_apoLayers.push_back(std::move(poLayer));
    }

    return true;
}

GDALDataset *OGRFlatGeobufDataset::Create(const char *pszName, int /* nBands */,
                                          CPL_UNUSED int nXSize,
                                          CPL_UNUSED int nYSize,
                                          CPL_UNUSED GDALDataType eDT,
                                          char ** /* papszOptions */)
{
    // First, ensure there isn't any such file yet.
    VSIStatBufL sStatBuf;

    if (VSIStatL(pszName, &sStatBuf) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "It seems a file system object called '%s' already exists.",
                 pszName);

        return nullptr;
    }

    bool bIsDir = false;
    if (!EQUAL(CPLGetExtensionSafe(pszName).c_str(), "fgb"))
    {
        if (VSIMkdir(pszName, 0755) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create directory %s:\n%s", pszName,
                     VSIStrerror(errno));
            return nullptr;
        }
        bIsDir = true;
    }

    return new OGRFlatGeobufDataset(pszName, bIsDir, true, false);
}

OGRLayer *OGRFlatGeobufDataset::GetLayer(int iLayer)
{
    if (iLayer < 0 || iLayer >= GetLayerCount())
        return nullptr;
    return m_apoLayers[iLayer]->GetLayer();
}

int OGRFlatGeobufDataset::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return m_bCreate && (m_bIsDir || m_apoLayers.empty());
    else if (EQUAL(pszCap, ODsCCurveGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCRandomLayerWrite))
        return m_bUpdate;
    else
        return false;
}

/************************************************************************/
/*                        LaunderLayerName()                            */
/************************************************************************/

static CPLString LaunderLayerName(const char *pszLayerName)
{
    std::string osRet(CPLLaunderForFilenameSafe(pszLayerName, nullptr));
    if (osRet != pszLayerName)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Invalid layer name for a file name: %s. Laundered to %s.",
                 pszLayerName, osRet.c_str());
    }
    return osRet;
}

OGRLayer *
OGRFlatGeobufDataset::ICreateLayer(const char *pszLayerName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions)
{
    // Verify we are in update mode.
    if (!m_bCreate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.\n"
                 "New layer %s cannot be created.",
                 GetDescription(), pszLayerName);

        return nullptr;
    }
    if (!m_bIsDir && !m_apoLayers.empty())
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Can create only one single layer in a .fgb file. "
                 "Use a directory output for multiple layers");

        return nullptr;
    }

    const auto eGType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSpatialRef =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    // Verify that the datasource is a directory.
    VSIStatBufL sStatBuf;

    // What filename would we use?
    CPLString osFilename;

    if (m_bIsDir)
        osFilename = CPLFormFilenameSafe(
            GetDescription(), LaunderLayerName(pszLayerName).c_str(), "fgb");
    else
        osFilename = GetDescription();

    // Does this directory/file already exist?
    if (VSIStatL(osFilename, &sStatBuf) == 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Attempt to create layer %s, but %s already exists.",
                 pszLayerName, osFilename.c_str());
        return nullptr;
    }

    bool bCreateSpatialIndexAtClose =
        CPLFetchBool(papszOptions, "SPATIAL_INDEX", true);

    auto poLayer =
        std::unique_ptr<OGRFlatGeobufLayer>(OGRFlatGeobufLayer::Create(
            this, pszLayerName, osFilename, poSpatialRef, eGType,
            bCreateSpatialIndexAtClose, papszOptions));
    if (poLayer == nullptr)
        return nullptr;

    m_apoLayers.push_back(std::move(poLayer));

    return m_apoLayers.back()->GetLayer();
}

/************************************************************************/
//                            GetFileList()                             */
/************************************************************************/

char **OGRFlatGeobufDataset::GetFileList()
{
    CPLStringList oFileList;
    for (const auto &poLayer : m_apoLayers)
    {
        oFileList.AddString(poLayer->GetFilename().c_str());
    }
    return oFileList.StealList();
}
