/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogrshape.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_error.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogrlayerpool.h"
#include "ogrsf_frmts.h"
#include "shapefil.h"
#include "shp_vsi.h"

// #define IMMEDIATE_OPENING 1

constexpr int knREFRESH_LOCK_FILE_DELAY_SEC = 10;

/************************************************************************/
/*                          DS_SHPOpen()                                */
/************************************************************************/

SHPHandle OGRShapeDataSource::DS_SHPOpen(const char *pszShapeFile,
                                         const char *pszAccess)
{
    // Do lazy shx loading for /vsicurl/
    if (STARTS_WITH(pszShapeFile, "/vsicurl/") && strcmp(pszAccess, "r") == 0)
        pszAccess = "rl";

    const bool bRestoreSHX =
        CPLTestBool(CPLGetConfigOption("SHAPE_RESTORE_SHX", "FALSE"));
    SHPHandle hSHP = SHPOpenLLEx(
        pszShapeFile, pszAccess,
        const_cast<SAHooks *>(VSI_SHP_GetHook(m_b2GBLimit)), bRestoreSHX);

    if (hSHP != nullptr)
        SHPSetFastModeReadObject(hSHP, TRUE);
    return hSHP;
}

/************************************************************************/
/*                           DS_DBFOpen()                               */
/************************************************************************/

DBFHandle OGRShapeDataSource::DS_DBFOpen(const char *pszDBFFile,
                                         const char *pszAccess)
{
    DBFHandle hDBF =
        DBFOpenLL(pszDBFFile, pszAccess,
                  const_cast<SAHooks *>(VSI_SHP_GetHook(m_b2GBLimit)));
    return hDBF;
}

/************************************************************************/
/*                         OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::OGRShapeDataSource()
    : m_poPool(std::make_unique<OGRLayerPool>()),
      m_b2GBLimit(CPLTestBool(CPLGetConfigOption("SHAPE_2GB_LIMIT", "FALSE")))
{
}

/************************************************************************/
/*                             GetLayerNames()                          */
/************************************************************************/

std::vector<CPLString> OGRShapeDataSource::GetLayerNames() const
{
    std::vector<CPLString> res;
    const_cast<OGRShapeDataSource *>(this)->GetLayerCount();
    for (const auto &poLayer : m_apoLayers)
    {
        res.emplace_back(poLayer->GetName());
    }
    return res;
}

/************************************************************************/
/*                        ~OGRShapeDataSource()                         */
/************************************************************************/

OGRShapeDataSource::~OGRShapeDataSource()

{
    std::vector<CPLString> layerNames;
    if (!m_osTemporaryUnzipDir.empty())
    {
        layerNames = GetLayerNames();
    }
    m_apoLayers.clear();
    m_poPool.reset();

    RecompressIfNeeded(layerNames);
    RemoveLockFile();

    // Free mutex & cond
    if (m_poRefreshLockFileMutex)
    {
        CPLDestroyMutex(m_poRefreshLockFileMutex);
        m_poRefreshLockFileMutex = nullptr;
    }
    if (m_poRefreshLockFileCond)
    {
        CPLDestroyCond(m_poRefreshLockFileCond);
        m_poRefreshLockFileCond = nullptr;
    }
}

/************************************************************************/
/*                              OpenZip()                               */
/************************************************************************/

bool OGRShapeDataSource::OpenZip(GDALOpenInfo *poOpenInfo,
                                 const char *pszOriFilename)
{
    if (!Open(poOpenInfo, true))
        return false;

    SetDescription(pszOriFilename);

    m_bIsZip = true;
    m_bSingleLayerZip =
        EQUAL(CPLGetExtensionSafe(pszOriFilename).c_str(), "shz");

    if (!m_bSingleLayerZip)
    {
        CPLString osLockFile(GetDescription());
        osLockFile += ".gdal.lock";
        VSIStatBufL sStat;
        if (VSIStatL(osLockFile, &sStat) == 0 &&
            sStat.st_mtime < time(nullptr) - 2 * knREFRESH_LOCK_FILE_DELAY_SEC)
        {
            CPLDebug("Shape", "Deleting stalled %s", osLockFile.c_str());
            VSIUnlink(osLockFile);
        }
    }

    return true;
}

/************************************************************************/
/*                            CreateZip()                               */
/************************************************************************/

bool OGRShapeDataSource::CreateZip(const char *pszOriFilename)
{
    CPLAssert(m_apoLayers.empty());

    void *hZIP = CPLCreateZip(pszOriFilename, nullptr);
    if (!hZIP)
        return false;
    if (CPLCloseZip(hZIP) != CE_None)
        return false;
    eAccess = GA_Update;
    m_bIsZip = true;
    m_bSingleLayerZip =
        EQUAL(CPLGetExtensionSafe(pszOriFilename).c_str(), "shz");
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRShapeDataSource::Open(GDALOpenInfo *poOpenInfo, bool bTestOpen,
                              bool bForceSingleFileDataSource)

{
    CPLAssert(m_apoLayers.empty());

    const char *pszNewName = poOpenInfo->pszFilename;
    const bool bUpdate = poOpenInfo->eAccess == GA_Update;
    CPLAssert(papszOpenOptions == nullptr);
    papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);

    eAccess = poOpenInfo->eAccess;

    m_bSingleFileDataSource = CPL_TO_BOOL(bForceSingleFileDataSource);

    /* -------------------------------------------------------------------- */
    /*      If m_bSingleFileDataSource is TRUE we don't try to do anything  */
    /*      else.                                                           */
    /*      This is only utilized when the OGRShapeDriver::Create()         */
    /*      method wants to create a stub OGRShapeDataSource for a          */
    /*      single shapefile.  The driver will take care of creating the    */
    /*      file by calling ICreateLayer().                                 */
    /* -------------------------------------------------------------------- */
    if (m_bSingleFileDataSource)
        return true;

    /* -------------------------------------------------------------------- */
    /*      Is the given path a directory or a regular file?                */
    /* -------------------------------------------------------------------- */
    if (!poOpenInfo->bStatOK)
    {
        if (!bTestOpen)
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s is neither a file or directory, Shape access failed.",
                     pszNewName);

        return false;
    }

    /* -------------------------------------------------------------------- */
    /*      Build a list of filenames we figure are Shape files.            */
    /* -------------------------------------------------------------------- */
    if (!poOpenInfo->bIsDirectory)
    {
        if (!OpenFile(pszNewName, bUpdate))
        {
            if (!bTestOpen)
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Failed to open shapefile %s.  "
                         "It may be corrupt or read-only file accessed in "
                         "update mode.",
                         pszNewName);

            return false;
        }

        m_bSingleFileDataSource = true;

        return true;
    }
    else
    {
        const CPLStringList aosCandidates(VSIReadDir(pszNewName));
        const int nCandidateCount = aosCandidates.size();
        bool bMightBeOldCoverage = false;
        std::set<CPLString> osLayerNameSet;

        for (int iCan = 0; iCan < nCandidateCount; iCan++)
        {
            const char *pszCandidate = aosCandidates[iCan];
            CPLString osLayerName(CPLGetBasenameSafe(pszCandidate));
#ifdef _WIN32
            // On Windows, as filenames are case insensitive, a shapefile layer
            // can be made of foo.shp and FOO.DBF, so to detect unique layer
            // names, put them upper case in the unique set used for detection.
            osLayerName.toupper();
#endif

            if (EQUAL(pszCandidate, "ARC"))
                bMightBeOldCoverage = true;

            if (strlen(pszCandidate) < 4 ||
                !EQUAL(pszCandidate + strlen(pszCandidate) - 4, ".shp"))
                continue;

            std::string osFilename =
                CPLFormFilenameSafe(pszNewName, pszCandidate, nullptr);

            osLayerNameSet.insert(osLayerName);
#ifdef IMMEDIATE_OPENING
            if (!OpenFile(osFilename.c_str(), bUpdate) && !bTestOpen)
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Failed to open shapefile %s.  "
                         "It may be corrupt or read-only file accessed in "
                         "update mode.",
                         osFilename.c_str());
                return false;
            }
#else
            m_oVectorLayerName.push_back(std::move(osFilename));
#endif
        }

        // Try and .dbf files without apparent associated shapefiles.
        for (int iCan = 0; iCan < nCandidateCount; iCan++)
        {
            const char *pszCandidate = aosCandidates[iCan];
            const std::string osLayerNameOri = CPLGetBasenameSafe(pszCandidate);
            CPLString osLayerName(osLayerNameOri);
#ifdef _WIN32
            osLayerName.toupper();
#endif

            // We don't consume .dbf files in a directory that looks like
            // an old style Arc/Info (for PC?) that unless we found at least
            // some shapefiles.  See Bug 493.
            if (bMightBeOldCoverage && osLayerNameSet.empty())
                continue;

            if (strlen(pszCandidate) < 4 ||
                !EQUAL(pszCandidate + strlen(pszCandidate) - 4, ".dbf"))
                continue;

            if (osLayerNameSet.find(osLayerName) != osLayerNameSet.end())
                continue;

            // We don't want to access .dbf files with an associated .tab
            // file, or it will never get recognised as a mapinfo dataset.
            bool bFoundTAB = false;
            for (int iCan2 = 0; iCan2 < nCandidateCount; iCan2++)
            {
                const char *pszCandidate2 = aosCandidates[iCan2];

                if (EQUALN(pszCandidate2, osLayerNameOri.c_str(),
                           osLayerNameOri.size()) &&
                    EQUAL(pszCandidate2 + osLayerNameOri.size(), ".tab"))
                    bFoundTAB = true;
            }

            if (bFoundTAB)
                continue;

            std::string osFilename =
                CPLFormFilenameSafe(pszNewName, pszCandidate, nullptr);

            osLayerNameSet.insert(osLayerName);

#ifdef IMMEDIATE_OPENING
            if (!OpenFile(osFilename.c_str(), bUpdate) && !bTestOpen)
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Failed to open dbf file %s.  "
                         "It may be corrupt or read-only file accessed in "
                         "update mode.",
                         osFilename.c_str());
                return false;
            }
#else
            m_oVectorLayerName.push_back(std::move(osFilename));
#endif
        }

#ifdef IMMEDIATE_OPENING
        const int nDirLayers = static_cast<int>(m_apoLayers.size());
#else
        const int nDirLayers = static_cast<int>(m_oVectorLayerName.size());
#endif

        CPLErrorReset();

        return nDirLayers > 0 || !bTestOpen;
    }
}

/************************************************************************/
/*                              OpenFile()                              */
/************************************************************************/

bool OGRShapeDataSource::OpenFile(const char *pszNewName, bool bUpdate)

{
    const std::string osExtension = CPLGetExtensionSafe(pszNewName);

    if (!EQUAL(osExtension.c_str(), "shp") &&
        !EQUAL(osExtension.c_str(), "shx") &&
        !EQUAL(osExtension.c_str(), "dbf"))
        return false;

    /* -------------------------------------------------------------------- */
    /*      SHPOpen() should include better (CPL based) error reporting,    */
    /*      and we should be trying to distinguish at this point whether    */
    /*      failure is a result of trying to open a non-shapefile, or       */
    /*      whether it was a shapefile and we want to report the error      */
    /*      up.                                                             */
    /*                                                                      */
    /*      Care is taken to suppress the error and only reissue it if      */
    /*      we think it is appropriate.                                     */
    /* -------------------------------------------------------------------- */
    const bool bRealUpdateAccess =
        bUpdate && (!IsZip() || !GetTemporaryUnzipDir().empty());
    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    SHPHandle hSHP = bRealUpdateAccess ? DS_SHPOpen(pszNewName, "r+")
                                       : DS_SHPOpen(pszNewName, "r");
    CPLPopErrorHandler();

    const bool bRestoreSHX =
        CPLTestBool(CPLGetConfigOption("SHAPE_RESTORE_SHX", "FALSE"));
    if (bRestoreSHX && EQUAL(CPLGetExtensionSafe(pszNewName).c_str(), "dbf") &&
        CPLGetLastErrorMsg()[0] != '\0')
    {
        CPLString osMsg = CPLGetLastErrorMsg();

        CPLError(CE_Warning, CPLE_AppDefined, "%s", osMsg.c_str());
    }
    else
    {
        if (hSHP == nullptr &&
            (!EQUAL(CPLGetExtensionSafe(pszNewName).c_str(), "dbf") ||
             strstr(CPLGetLastErrorMsg(), ".shp") == nullptr))
        {
            CPLString osMsg = CPLGetLastErrorMsg();

            CPLError(CE_Failure, CPLE_OpenFailed, "%s", osMsg.c_str());

            return false;
        }
        CPLErrorReset();
    }

    /* -------------------------------------------------------------------- */
    /*      Open the .dbf file, if it exists.  To open a dbf file, the      */
    /*      filename has to either refer to a successfully opened shp       */
    /*      file or has to refer to the actual .dbf file.                   */
    /* -------------------------------------------------------------------- */
    DBFHandle hDBF = nullptr;
    if (hSHP != nullptr ||
        EQUAL(CPLGetExtensionSafe(pszNewName).c_str(), "dbf"))
    {
        if (bRealUpdateAccess)
        {
            hDBF = DS_DBFOpen(pszNewName, "r+");
            if (hSHP != nullptr && hDBF == nullptr)
            {
                for (int i = 0; i < 2; i++)
                {
                    VSIStatBufL sStat;
                    const std::string osDBFName = CPLResetExtensionSafe(
                        pszNewName, (i == 0) ? "dbf" : "DBF");
                    VSILFILE *fp = nullptr;
                    if (VSIStatExL(osDBFName.c_str(), &sStat,
                                   VSI_STAT_EXISTS_FLAG) == 0)
                    {
                        fp = VSIFOpenL(osDBFName.c_str(), "r+");
                        if (fp == nullptr)
                        {
                            CPLError(CE_Failure, CPLE_OpenFailed,
                                     "%s exists, "
                                     "but cannot be opened in update mode",
                                     osDBFName.c_str());
                            SHPClose(hSHP);
                            return false;
                        }
                        VSIFCloseL(fp);
                        break;
                    }
                }
            }
        }
        else
        {
            hDBF = DS_DBFOpen(pszNewName, "r");
        }
    }
    else
    {
        hDBF = nullptr;
    }

    if (hDBF == nullptr && hSHP == nullptr)
        return false;

    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    auto poLayer = std::make_unique<OGRShapeLayer>(
        this, pszNewName, hSHP, hDBF,
        /* poSRS = */ nullptr,
        /* bSRSSet = */ false,
        /* osPrjFilename = */ std::string(), bUpdate, wkbNone);
    poLayer->SetModificationDate(
        CSLFetchNameValue(papszOpenOptions, "DBF_DATE_LAST_UPDATE"));
    poLayer->SetAutoRepack(CPLFetchBool(papszOpenOptions, "AUTO_REPACK", true));
    poLayer->SetWriteDBFEOFChar(
        CPLFetchBool(papszOpenOptions, "DBF_EOF_CHAR", true));

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    AddLayer(std::move(poLayer));

    return true;
}

/************************************************************************/
/*                             AddLayer()                               */
/************************************************************************/

void OGRShapeDataSource::AddLayer(std::unique_ptr<OGRShapeLayer> poLayer)
{
    m_apoLayers.push_back(std::move(poLayer));

    // If we reach the limit, then register all the already opened layers
    // Technically this code would not be necessary if there was not the
    // following initial test in SetLastUsedLayer() :
    //      if (static_cast<int>(m_apoLayers.size()) < MAX_SIMULTANEOUSLY_OPENED_LAYERS)
    //         return;
    if (static_cast<int>(m_apoLayers.size()) ==
            m_poPool->GetMaxSimultaneouslyOpened() &&
        m_poPool->GetSize() == 0)
    {
        for (auto &poIterLayer : m_apoLayers)
            m_poPool->SetLastUsedLayer(poIterLayer.get());
    }
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
                 "Invalid layer name for a shapefile: %s. Laundered to %s.",
                 pszLayerName, osRet.c_str());
    }
    return osRet;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRShapeDataSource::ICreateLayer(const char *pszLayerName,
                                 const OGRGeomFieldDefn *poGeomFieldDefn,
                                 CSLConstList papszOptions)

{
    // To ensure that existing layers are created.
    GetLayerCount();

    auto eType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSRS =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    /* -------------------------------------------------------------------- */
    /*      Check that the layer doesn't already exist.                     */
    /* -------------------------------------------------------------------- */
    if (GetLayerByName(pszLayerName) != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Layer '%s' already exists",
                 pszLayerName);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Verify we are in update mode.                                   */
    /* -------------------------------------------------------------------- */
    if (eAccess == GA_ReadOnly)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.  "
                 "New layer %s cannot be created.",
                 GetDescription(), pszLayerName);

        return nullptr;
    }

    if (m_bIsZip && m_bSingleLayerZip && m_apoLayers.size() == 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 ".shz only supports one single layer");
        return nullptr;
    }

    if (!UncompressIfNeeded())
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Figure out what type of layer we need.                          */
    /* -------------------------------------------------------------------- */
    int nShapeType = -1;

    if (wkbFlatten(eType) == wkbUnknown || eType == wkbLineString)
        nShapeType = SHPT_ARC;
    else if (eType == wkbPoint)
        nShapeType = SHPT_POINT;
    else if (eType == wkbPolygon || eType == wkbTriangle)
        nShapeType = SHPT_POLYGON;
    else if (eType == wkbMultiPoint)
        nShapeType = SHPT_MULTIPOINT;
    else if (eType == wkbPoint25D)
        nShapeType = SHPT_POINTZ;
    else if (eType == wkbPointM)
        nShapeType = SHPT_POINTM;
    else if (eType == wkbPointZM)
        nShapeType = SHPT_POINTZ;
    else if (eType == wkbLineString25D)
        nShapeType = SHPT_ARCZ;
    else if (eType == wkbLineStringM)
        nShapeType = SHPT_ARCM;
    else if (eType == wkbLineStringZM)
        nShapeType = SHPT_ARCZ;
    else if (eType == wkbMultiLineString)
        nShapeType = SHPT_ARC;
    else if (eType == wkbMultiLineString25D)
        nShapeType = SHPT_ARCZ;
    else if (eType == wkbMultiLineStringM)
        nShapeType = SHPT_ARCM;
    else if (eType == wkbMultiLineStringZM)
        nShapeType = SHPT_ARCZ;
    else if (eType == wkbPolygon25D || eType == wkbTriangleZ)
        nShapeType = SHPT_POLYGONZ;
    else if (eType == wkbPolygonM || eType == wkbTriangleM)
        nShapeType = SHPT_POLYGONM;
    else if (eType == wkbPolygonZM || eType == wkbTriangleZM)
        nShapeType = SHPT_POLYGONZ;
    else if (eType == wkbMultiPolygon)
        nShapeType = SHPT_POLYGON;
    else if (eType == wkbMultiPolygon25D)
        nShapeType = SHPT_POLYGONZ;
    else if (eType == wkbMultiPolygonM)
        nShapeType = SHPT_POLYGONM;
    else if (eType == wkbMultiPolygonZM)
        nShapeType = SHPT_POLYGONZ;
    else if (eType == wkbMultiPoint25D)
        nShapeType = SHPT_MULTIPOINTZ;
    else if (eType == wkbMultiPointM)
        nShapeType = SHPT_MULTIPOINTM;
    else if (eType == wkbMultiPointZM)
        nShapeType = SHPT_MULTIPOINTZ;
    else if (wkbFlatten(eType) == wkbTIN ||
             wkbFlatten(eType) == wkbPolyhedralSurface)
        nShapeType = SHPT_MULTIPATCH;
    else if (eType == wkbNone)
        nShapeType = SHPT_NULL;

    /* -------------------------------------------------------------------- */
    /*      Has the application overridden this with a special creation     */
    /*      option?                                                         */
    /* -------------------------------------------------------------------- */
    const char *pszOverride = CSLFetchNameValue(papszOptions, "SHPT");

    if (pszOverride == nullptr)
    {
        /* ignore */;
    }
    else if (EQUAL(pszOverride, "POINT"))
    {
        nShapeType = SHPT_POINT;
        eType = wkbPoint;
    }
    else if (EQUAL(pszOverride, "ARC"))
    {
        nShapeType = SHPT_ARC;
        eType = wkbLineString;
    }
    else if (EQUAL(pszOverride, "POLYGON"))
    {
        nShapeType = SHPT_POLYGON;
        eType = wkbPolygon;
    }
    else if (EQUAL(pszOverride, "MULTIPOINT"))
    {
        nShapeType = SHPT_MULTIPOINT;
        eType = wkbMultiPoint;
    }
    else if (EQUAL(pszOverride, "POINTZ"))
    {
        nShapeType = SHPT_POINTZ;
        eType = wkbPoint25D;
    }
    else if (EQUAL(pszOverride, "ARCZ"))
    {
        nShapeType = SHPT_ARCZ;
        eType = wkbLineString25D;
    }
    else if (EQUAL(pszOverride, "POLYGONZ"))
    {
        nShapeType = SHPT_POLYGONZ;
        eType = wkbPolygon25D;
    }
    else if (EQUAL(pszOverride, "MULTIPOINTZ"))
    {
        nShapeType = SHPT_MULTIPOINTZ;
        eType = wkbMultiPoint25D;
    }
    else if (EQUAL(pszOverride, "POINTM"))
    {
        nShapeType = SHPT_POINTM;
        eType = wkbPointM;
    }
    else if (EQUAL(pszOverride, "ARCM"))
    {
        nShapeType = SHPT_ARCM;
        eType = wkbLineStringM;
    }
    else if (EQUAL(pszOverride, "POLYGONM"))
    {
        nShapeType = SHPT_POLYGONM;
        eType = wkbPolygonM;
    }
    else if (EQUAL(pszOverride, "MULTIPOINTM"))
    {
        nShapeType = SHPT_MULTIPOINTM;
        eType = wkbMultiPointM;
    }
    else if (EQUAL(pszOverride, "POINTZM"))
    {
        nShapeType = SHPT_POINTZ;
        eType = wkbPointZM;
    }
    else if (EQUAL(pszOverride, "ARCZM"))
    {
        nShapeType = SHPT_ARCZ;
        eType = wkbLineStringZM;
    }
    else if (EQUAL(pszOverride, "POLYGONZM"))
    {
        nShapeType = SHPT_POLYGONZ;
        eType = wkbPolygonZM;
    }
    else if (EQUAL(pszOverride, "MULTIPOINTZM"))
    {
        nShapeType = SHPT_MULTIPOINTZ;
        eType = wkbMultiPointZM;
    }
    else if (EQUAL(pszOverride, "MULTIPATCH"))
    {
        nShapeType = SHPT_MULTIPATCH;
        eType = wkbUnknown;  // not ideal...
    }
    else if (EQUAL(pszOverride, "NONE") || EQUAL(pszOverride, "NULL"))
    {
        nShapeType = SHPT_NULL;
        eType = wkbNone;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unknown SHPT value of `%s' passed to Shapefile layer"
                 "creation.  Creation aborted.",
                 pszOverride);

        return nullptr;
    }

    if (nShapeType == -1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Geometry type of `%s' not supported in shapefiles.  "
                 "Type can be overridden with a layer creation option "
                 "of SHPT=POINT/ARC/POLYGON/MULTIPOINT/POINTZ/ARCZ/POLYGONZ/"
                 "MULTIPOINTZ/MULTIPATCH.",
                 OGRGeometryTypeToName(eType));
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      What filename do we use, excluding the extension?               */
    /* -------------------------------------------------------------------- */
    std::string osFilenameWithoutExt;

    if (m_bSingleFileDataSource && m_apoLayers.empty())
    {
        const std::string osPath = CPLGetPathSafe(GetDescription());
        const std::string osFBasename = CPLGetBasenameSafe(GetDescription());

        osFilenameWithoutExt =
            CPLFormFilenameSafe(osPath.c_str(), osFBasename.c_str(), nullptr);
    }
    else if (m_bSingleFileDataSource)
    {
        // This is a very weird use case : the user creates/open a datasource
        // made of a single shapefile 'foo.shp' and wants to add a new layer
        // to it, 'bar'. So we create a new shapefile 'bar.shp' in the same
        // directory as 'foo.shp'
        // So technically, we will not be any longer a single file
        // datasource ... Ahem ahem.
        const std::string osPath = CPLGetPathSafe(GetDescription());
        osFilenameWithoutExt = CPLFormFilenameSafe(
            osPath.c_str(), LaunderLayerName(pszLayerName).c_str(), nullptr);
    }
    else
    {
        const std::string osDir(m_osTemporaryUnzipDir.empty()
                                    ? std::string(GetDescription())
                                    : m_osTemporaryUnzipDir);
        osFilenameWithoutExt = CPLFormFilenameSafe(
            osDir.c_str(), LaunderLayerName(pszLayerName).c_str(), nullptr);
    }

    /* -------------------------------------------------------------------- */
    /*      Create the shapefile.                                           */
    /* -------------------------------------------------------------------- */
    const bool l_b2GBLimit =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "2GB_LIMIT", "FALSE"));

    SHPHandle hSHP = nullptr;

    if (nShapeType != SHPT_NULL)
    {
        const std::string osFilename =
            CPLFormFilenameSafe(nullptr, osFilenameWithoutExt.c_str(), "shp");

        hSHP = SHPCreateLL(osFilename.c_str(), nShapeType,
                           const_cast<SAHooks *>(VSI_SHP_GetHook(l_b2GBLimit)));

        if (hSHP == nullptr)
        {
            return nullptr;
        }

        SHPSetFastModeReadObject(hSHP, TRUE);
    }

    /* -------------------------------------------------------------------- */
    /*      Has a specific LDID been specified by the caller?               */
    /* -------------------------------------------------------------------- */
    const char *pszLDID = CSLFetchNameValue(papszOptions, "ENCODING");

    /* -------------------------------------------------------------------- */
    /*      Create a DBF file.                                              */
    /* -------------------------------------------------------------------- */
    const std::string osDBFFilename =
        CPLFormFilenameSafe(nullptr, osFilenameWithoutExt.c_str(), "dbf");

    DBFHandle hDBF = DBFCreateLL(
        osDBFFilename.c_str(), (pszLDID != nullptr) ? pszLDID : "LDID/87",
        const_cast<SAHooks *>(VSI_SHP_GetHook(m_b2GBLimit)));

    if (hDBF == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to create Shape DBF file `%s'.",
                 osDBFFilename.c_str());
        SHPClose(hSHP);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the .prj file, if required.                              */
    /* -------------------------------------------------------------------- */
    std::string osPrjFilename;
    OGRSpatialReference *poSRSClone = nullptr;
    if (poSRS != nullptr)
    {
        osPrjFilename =
            CPLFormFilenameSafe(nullptr, osFilenameWithoutExt.c_str(), "prj");
        poSRSClone = poSRS->Clone();

        char *pszWKT = nullptr;
        VSILFILE *fp = nullptr;
        const char *const apszOptions[] = {"FORMAT=WKT1_ESRI", nullptr};
        if (poSRSClone->exportToWkt(&pszWKT, apszOptions) == OGRERR_NONE &&
            (fp = VSIFOpenL(osPrjFilename.c_str(), "wt")) != nullptr)
        {
            VSIFWriteL(pszWKT, strlen(pszWKT), 1, fp);
            VSIFCloseL(fp);
        }

        CPLFree(pszWKT);
    }

    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    // OGRShapeLayer constructor expects a filename with an extension (that
    // could be random actually), otherwise this is going to cause problems with
    // layer names that have a dot (not speaking about the one before the shp)
    const std::string osSHPFilename =
        CPLFormFilenameSafe(nullptr, osFilenameWithoutExt.c_str(), "shp");

    auto poLayer = std::make_unique<OGRShapeLayer>(
        this, osSHPFilename.c_str(), hSHP, hDBF, poSRSClone,
        /* bSRSSet = */ true, osPrjFilename,
        /* bUpdate = */ true, eType);
    if (poSRSClone != nullptr)
    {
        poSRSClone->Release();
    }

    poLayer->SetResizeAtClose(CPLFetchBool(papszOptions, "RESIZE", false));
    poLayer->CreateSpatialIndexAtClose(
        CPLFetchBool(papszOptions, "SPATIAL_INDEX", false));
    poLayer->SetModificationDate(
        CSLFetchNameValue(papszOptions, "DBF_DATE_LAST_UPDATE"));
    poLayer->SetAutoRepack(CPLFetchBool(papszOptions, "AUTO_REPACK", true));
    poLayer->SetWriteDBFEOFChar(
        CPLFetchBool(papszOptions, "DBF_EOF_CHAR", true));

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    AddLayer(std::move(poLayer));

    return m_apoLayers.back().get();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRShapeDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return eAccess == GA_Update &&
               !(m_bIsZip && m_bSingleLayerZip && m_apoLayers.size() == 1);
    else if (EQUAL(pszCap, ODsCDeleteLayer))
        return eAccess == GA_Update && !(m_bIsZip && m_bSingleLayerZip);
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCRandomLayerWrite))
        return eAccess == GA_Update;

    return false;
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int OGRShapeDataSource::GetLayerCount()

{
#ifndef IMMEDIATE_OPENING
    if (!m_oVectorLayerName.empty())
    {
        for (size_t i = 0; i < m_oVectorLayerName.size(); i++)
        {
            const char *pszFilename = m_oVectorLayerName[i].c_str();
            const std::string osLayerName = CPLGetBasenameSafe(pszFilename);

            bool bFound = false;
            for (auto &poLayer : m_apoLayers)
            {
                if (poLayer->GetName() == osLayerName)
                {
                    bFound = true;
                    break;
                }
            }
            if (bFound)
                continue;

            if (!OpenFile(pszFilename, eAccess == GA_Update))
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Failed to open file %s."
                         "It may be corrupt or read-only file accessed in "
                         "update mode.",
                         pszFilename);
            }
        }
        m_oVectorLayerName.resize(0);
    }
#endif

    return static_cast<int>(m_apoLayers.size());
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRShapeDataSource::GetLayer(int iLayer)

{
    // To ensure that existing layers are created.
    GetLayerCount();

    if (iLayer < 0 || iLayer >= static_cast<int>(m_apoLayers.size()))
        return nullptr;

    return m_apoLayers[iLayer].get();
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGRShapeDataSource::GetLayerByName(const char *pszLayerNameIn)
{
#ifndef IMMEDIATE_OPENING
    if (!m_oVectorLayerName.empty())
    {
        for (auto &poLayer : m_apoLayers)
        {
            if (strcmp(poLayer->GetName(), pszLayerNameIn) == 0)
            {
                return poLayer.get();
            }
        }

        for (int j = 0; j < 2; j++)
        {
            for (size_t i = 0; i < m_oVectorLayerName.size(); i++)
            {
                const char *pszFilename = m_oVectorLayerName[i].c_str();
                const std::string osLayerName = CPLGetBasenameSafe(pszFilename);

                if (j == 0)
                {
                    if (osLayerName != pszLayerNameIn)
                        continue;
                }
                else
                {
                    if (!EQUAL(osLayerName.c_str(), pszLayerNameIn))
                        continue;
                }

                if (!OpenFile(pszFilename, eAccess == GA_Update))
                {
                    CPLError(CE_Failure, CPLE_OpenFailed,
                             "Failed to open file %s.  "
                             "It may be corrupt or read-only file accessed in "
                             "update mode.",
                             pszFilename);
                    return nullptr;
                }

                return m_apoLayers.back().get();
            }
        }

        return nullptr;
    }
#endif

    return GDALDataset::GetLayerByName(pszLayerNameIn);
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/*                                                                      */
/*      We override this to provide special handling of CREATE          */
/*      SPATIAL INDEX commands.  Support forms are:                     */
/*                                                                      */
/*        CREATE SPATIAL INDEX ON layer_name [DEPTH n]                  */
/*        DROP SPATIAL INDEX ON layer_name                              */
/*        REPACK layer_name                                             */
/*        RECOMPUTE EXTENT ON layer_name                                */
/************************************************************************/

OGRLayer *OGRShapeDataSource::ExecuteSQL(const char *pszStatement,
                                         OGRGeometry *poSpatialFilter,
                                         const char *pszDialect)

{
    if (EQUAL(pszStatement, "UNCOMPRESS"))
    {
        CPL_IGNORE_RET_VAL(UncompressIfNeeded());
        return nullptr;
    }

    if (EQUAL(pszStatement, "RECOMPRESS"))
    {
        RecompressIfNeeded(GetLayerNames());
        return nullptr;
    }
    /* ==================================================================== */
    /*      Handle command to drop a spatial index.                         */
    /* ==================================================================== */
    if (STARTS_WITH_CI(pszStatement, "REPACK "))
    {
        OGRShapeLayer *poLayer =
            cpl::down_cast<OGRShapeLayer *>(GetLayerByName(pszStatement + 7));

        if (poLayer != nullptr)
        {
            if (poLayer->Repack() != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "REPACK of layer '%s' failed.", pszStatement + 7);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No such layer as '%s' in REPACK.", pszStatement + 7);
        }
        return nullptr;
    }

    /* ==================================================================== */
    /*      Handle command to shrink columns to their minimum size.         */
    /* ==================================================================== */
    if (STARTS_WITH_CI(pszStatement, "RESIZE "))
    {
        OGRShapeLayer *poLayer =
            cpl::down_cast<OGRShapeLayer *>(GetLayerByName(pszStatement + 7));

        if (poLayer != nullptr)
        {
            poLayer->ResizeDBF();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No such layer as '%s' in RESIZE.", pszStatement + 7);
        }
        return nullptr;
    }

    /* ==================================================================== */
    /*      Handle command to recompute extent                             */
    /* ==================================================================== */
    if (STARTS_WITH_CI(pszStatement, "RECOMPUTE EXTENT ON "))
    {
        OGRShapeLayer *poLayer =
            cpl::down_cast<OGRShapeLayer *>(GetLayerByName(pszStatement + 20));

        if (poLayer != nullptr)
        {
            poLayer->RecomputeExtent();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No such layer as '%s' in RECOMPUTE EXTENT.",
                     pszStatement + 20);
        }
        return nullptr;
    }

    /* ==================================================================== */
    /*      Handle command to drop a spatial index.                         */
    /* ==================================================================== */
    if (STARTS_WITH_CI(pszStatement, "DROP SPATIAL INDEX ON "))
    {
        OGRShapeLayer *poLayer =
            cpl::down_cast<OGRShapeLayer *>(GetLayerByName(pszStatement + 22));

        if (poLayer != nullptr)
        {
            poLayer->DropSpatialIndex();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No such layer as '%s' in DROP SPATIAL INDEX.",
                     pszStatement + 22);
        }
        return nullptr;
    }

    /* ==================================================================== */
    /*      Handle all commands except spatial index creation generically.  */
    /* ==================================================================== */
    if (!STARTS_WITH_CI(pszStatement, "CREATE SPATIAL INDEX ON "))
    {
        char **papszTokens = CSLTokenizeString(pszStatement);
        if (CSLCount(papszTokens) >= 4 &&
            (EQUAL(papszTokens[0], "CREATE") ||
             EQUAL(papszTokens[0], "DROP")) &&
            EQUAL(papszTokens[1], "INDEX") && EQUAL(papszTokens[2], "ON"))
        {
            OGRShapeLayer *poLayer =
                cpl::down_cast<OGRShapeLayer *>(GetLayerByName(papszTokens[3]));
            if (poLayer != nullptr)
                poLayer->InitializeIndexSupport(poLayer->GetFullName());
        }
        CSLDestroy(papszTokens);

        return GDALDataset::ExecuteSQL(pszStatement, poSpatialFilter,
                                       pszDialect);
    }

    /* -------------------------------------------------------------------- */
    /*      Parse into keywords.                                            */
    /* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeString(pszStatement);

    if (CSLCount(papszTokens) < 5 || !EQUAL(papszTokens[0], "CREATE") ||
        !EQUAL(papszTokens[1], "SPATIAL") || !EQUAL(papszTokens[2], "INDEX") ||
        !EQUAL(papszTokens[3], "ON") || CSLCount(papszTokens) > 7 ||
        (CSLCount(papszTokens) == 7 && !EQUAL(papszTokens[5], "DEPTH")))
    {
        CSLDestroy(papszTokens);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Syntax error in CREATE SPATIAL INDEX command.\n"
                 "Was '%s'\n"
                 "Should be of form 'CREATE SPATIAL INDEX ON <table> "
                 "[DEPTH <n>]'",
                 pszStatement);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Get depth if provided.                                          */
    /* -------------------------------------------------------------------- */
    const int nDepth = CSLCount(papszTokens) == 7 ? atoi(papszTokens[6]) : 0;

    /* -------------------------------------------------------------------- */
    /*      What layer are we operating on.                                 */
    /* -------------------------------------------------------------------- */
    OGRShapeLayer *poLayer =
        cpl::down_cast<OGRShapeLayer *>(GetLayerByName(papszTokens[4]));

    if (poLayer == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Layer %s not recognised.",
                 papszTokens[4]);
        CSLDestroy(papszTokens);
        return nullptr;
    }

    CSLDestroy(papszTokens);

    poLayer->CreateSpatialIndex(nDepth);
    return nullptr;
}

/************************************************************************/
/*                     GetExtensionsForDeletion()                       */
/************************************************************************/

const char *const *OGRShapeDataSource::GetExtensionsForDeletion()
{
    static const char *const apszExtensions[] = {
        "shp",  "shx", "dbf", "sbn", "sbx", "prj", "idm", "ind", "qix", "cpg",
        "qpj",  // QGIS projection file
        nullptr};
    return apszExtensions;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRShapeDataSource::DeleteLayer(int iLayer)

{
    /* -------------------------------------------------------------------- */
    /*      Verify we are in update mode.                                   */
    /* -------------------------------------------------------------------- */
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.  "
                 "Layer %d cannot be deleted.",
                 GetDescription(), iLayer);

        return OGRERR_FAILURE;
    }

    // To ensure that existing layers are created.
    GetLayerCount();

    if (iLayer < 0 || iLayer >= static_cast<int>(m_apoLayers.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Layer %d not in legal range of 0 to %d.", iLayer,
                 static_cast<int>(m_apoLayers.size()) - 1);
        return OGRERR_FAILURE;
    }

    if (m_bIsZip && m_bSingleLayerZip)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 ".shz does not support layer deletion");
        return OGRERR_FAILURE;
    }

    if (!UncompressIfNeeded())
        return OGRERR_FAILURE;

    const std::string osLayerFilename = m_apoLayers[iLayer]->GetFullName();

    m_apoLayers.erase(m_apoLayers.begin() + iLayer);

    const char *const *papszExtensions =
        OGRShapeDataSource::GetExtensionsForDeletion();
    for (int iExt = 0; papszExtensions[iExt] != nullptr; iExt++)
    {
        const std::string osFile = CPLResetExtensionSafe(
            osLayerFilename.c_str(), papszExtensions[iExt]);
        VSIStatBufL sStatBuf;
        if (VSIStatL(osFile.c_str(), &sStatBuf) == 0)
            VSIUnlink(osFile.c_str());
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          SetLastUsedLayer()                          */
/************************************************************************/

void OGRShapeDataSource::SetLastUsedLayer(OGRShapeLayer *poLayer)
{
    // We could remove that check and things would still work in
    // 99.99% cases.
    // The only rationale for that test is to avoid breaking applications that
    // would deal with layers of the same datasource in different threads. In
    // GDAL < 1.9.0, this would work in most cases I can imagine as shapefile
    // layers are pretty much independent from each others (although it has
    // never been guaranteed to be a valid use case, and the shape driver is
    // likely more the exception than the rule in permitting accessing layers
    // from different threads !)  Anyway the LRU list mechanism leaves the door
    // open to concurrent accesses to it so when the datasource has not many
    // layers, we don't try to build the LRU list to avoid concurrency issues. I
    // haven't bothered making the analysis of how a mutex could be used to
    // protect that (my intuition is that it would need to be placed at the
    // beginning of OGRShapeLayer::TouchLayer() ).
    if (static_cast<int>(m_apoLayers.size()) <
        m_poPool->GetMaxSimultaneouslyOpened())
        return;

    m_poPool->SetLastUsedLayer(poLayer);
}

/************************************************************************/
//                            GetFileList()                             */
/************************************************************************/

char **OGRShapeDataSource::GetFileList()
{
    if (m_bIsZip)
    {
        return CSLAddString(nullptr, GetDescription());
    }
    CPLStringList oFileList;
    GetLayerCount();
    for (auto &poLayer : m_apoLayers)
    {
        poLayer->AddToFileList(oFileList);
    }
    return oFileList.StealList();
}

/************************************************************************/
//                          RefreshLockFile()                            */
/************************************************************************/

void OGRShapeDataSource::RefreshLockFile(void *_self)
{
    OGRShapeDataSource *self = static_cast<OGRShapeDataSource *>(_self);
    CPLAssert(self->m_psLockFile);
    CPLAcquireMutex(self->m_poRefreshLockFileMutex, 1000);
    self->m_bRefreshLockFileThreadStarted = true;
    CPLCondSignal(self->m_poRefreshLockFileCond);
    unsigned int nInc = 0;
    while (!(self->m_bExitRefreshLockFileThread))
    {
        auto ret = CPLCondTimedWait(self->m_poRefreshLockFileCond,
                                    self->m_poRefreshLockFileMutex,
                                    self->m_dfRefreshLockDelay);
        if (ret == COND_TIMED_WAIT_TIME_OUT)
        {
            CPLAssert(self->m_psLockFile);
            VSIFSeekL(self->m_psLockFile, 0, SEEK_SET);
            CPLString osTime;
            nInc++;
            osTime.Printf(CPL_FRMT_GUIB ", %u\n",
                          static_cast<GUIntBig>(time(nullptr)), nInc);
            VSIFWriteL(osTime.data(), 1, osTime.size(), self->m_psLockFile);
            VSIFFlushL(self->m_psLockFile);
        }
    }
    CPLReleaseMutex(self->m_poRefreshLockFileMutex);
}

/************************************************************************/
//                            RemoveLockFile()                          */
/************************************************************************/

void OGRShapeDataSource::RemoveLockFile()
{
    if (!m_psLockFile)
        return;

    // Ask the thread to terminate
    CPLAcquireMutex(m_poRefreshLockFileMutex, 1000);
    m_bExitRefreshLockFileThread = true;
    CPLCondSignal(m_poRefreshLockFileCond);
    CPLReleaseMutex(m_poRefreshLockFileMutex);
    CPLJoinThread(m_hRefreshLockFileThread);
    m_hRefreshLockFileThread = nullptr;

    // Close and remove lock file
    VSIFCloseL(m_psLockFile);
    m_psLockFile = nullptr;
    CPLString osLockFile(GetDescription());
    osLockFile += ".gdal.lock";
    VSIUnlink(osLockFile);
}

/************************************************************************/
//                         UncompressIfNeeded()                         */
/************************************************************************/

bool OGRShapeDataSource::UncompressIfNeeded()
{
    if (eAccess != GA_Update || !m_bIsZip || !m_osTemporaryUnzipDir.empty())
        return true;

    GetLayerCount();

    auto returnError = [this]()
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot uncompress %s",
                 GetDescription());
        return false;
    };

    if (m_apoLayers.size() > 1)
    {
        CPLString osLockFile(GetDescription());
        osLockFile += ".gdal.lock";
        VSIStatBufL sStat;
        if (VSIStatL(osLockFile, &sStat) == 0 &&
            sStat.st_mtime > time(nullptr) - 2 * knREFRESH_LOCK_FILE_DELAY_SEC)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot edit %s. Another task is editing it",
                     GetDescription());
            return false;
        }
        if (!m_poRefreshLockFileMutex)
        {
            m_poRefreshLockFileMutex = CPLCreateMutex();
            if (!m_poRefreshLockFileMutex)
                return false;
            CPLReleaseMutex(m_poRefreshLockFileMutex);
        }
        if (!m_poRefreshLockFileCond)
        {
            m_poRefreshLockFileCond = CPLCreateCond();
            if (!m_poRefreshLockFileCond)
                return false;
        }
        auto f = VSIFOpenL(osLockFile, "wb");
        if (!f)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot create lock file");
            return false;
        }
        m_psLockFile = f;
        CPLAcquireMutex(m_poRefreshLockFileMutex, 1000);
        m_bExitRefreshLockFileThread = false;
        m_bRefreshLockFileThreadStarted = false;
        CPLReleaseMutex(m_poRefreshLockFileMutex);
        // Config option mostly for testing purposes
        // coverity[tainted_data]
        m_dfRefreshLockDelay = CPLAtof(CPLGetConfigOption(
            "OGR_SHAPE_LOCK_DELAY",
            CPLSPrintf("%d", knREFRESH_LOCK_FILE_DELAY_SEC)));
        m_hRefreshLockFileThread =
            CPLCreateJoinableThread(OGRShapeDataSource::RefreshLockFile, this);
        if (!m_hRefreshLockFileThread)
        {
            VSIFCloseL(m_psLockFile);
            m_psLockFile = nullptr;
            VSIUnlink(osLockFile);
        }
        else
        {
            CPLAcquireMutex(m_poRefreshLockFileMutex, 1000);
            while (!m_bRefreshLockFileThreadStarted)
            {
                CPLCondWait(m_poRefreshLockFileCond, m_poRefreshLockFileMutex);
            }
            CPLReleaseMutex(m_poRefreshLockFileMutex);
        }
    }

    CPLString osVSIZipDirname(GetVSIZipPrefixeDir());
    vsi_l_offset nTotalUncompressedSize = 0;
    CPLStringList aosFiles(VSIReadDir(osVSIZipDirname));
    for (int i = 0; i < aosFiles.size(); i++)
    {
        const char *pszFilename = aosFiles[i];
        if (!EQUAL(pszFilename, ".") && !EQUAL(pszFilename, ".."))
        {
            const CPLString osSrcFile(
                CPLFormFilenameSafe(osVSIZipDirname, pszFilename, nullptr));
            VSIStatBufL sStat;
            if (VSIStatL(osSrcFile, &sStat) == 0)
            {
                nTotalUncompressedSize += sStat.st_size;
            }
        }
    }

    CPLString osTemporaryDir(GetDescription());
    osTemporaryDir += "_tmp_uncompressed";

    const char *pszUseVsimem =
        CPLGetConfigOption("OGR_SHAPE_USE_VSIMEM_FOR_TEMP", "AUTO");
    if (EQUAL(pszUseVsimem, "YES") ||
        (EQUAL(pszUseVsimem, "AUTO") && nTotalUncompressedSize > 0 &&
         nTotalUncompressedSize <
             static_cast<GUIntBig>(CPLGetUsablePhysicalRAM() / 10)))
    {
        osTemporaryDir = VSIMemGenerateHiddenFilename("shapedriver");
    }
    CPLDebug("Shape", "Uncompressing to %s", osTemporaryDir.c_str());

    VSIRmdirRecursive(osTemporaryDir);
    if (VSIMkdir(osTemporaryDir, 0755) != 0)
        return returnError();
    for (int i = 0; i < aosFiles.size(); i++)
    {
        const char *pszFilename = aosFiles[i];
        if (!EQUAL(pszFilename, ".") && !EQUAL(pszFilename, ".."))
        {
            const CPLString osSrcFile(
                CPLFormFilenameSafe(osVSIZipDirname, pszFilename, nullptr));
            const CPLString osDestFile(
                CPLFormFilenameSafe(osTemporaryDir, pszFilename, nullptr));
            if (CPLCopyFile(osDestFile, osSrcFile) != 0)
            {
                VSIRmdirRecursive(osTemporaryDir);
                return returnError();
            }
        }
    }

    m_osTemporaryUnzipDir = std::move(osTemporaryDir);

    for (auto &poLayer : m_apoLayers)
    {
        poLayer->UpdateFollowingDeOrRecompression();
    }

    return true;
}

/************************************************************************/
//                         RecompressIfNeeded()                         */
/************************************************************************/

bool OGRShapeDataSource::RecompressIfNeeded(
    const std::vector<CPLString> &layerNames)
{
    if (eAccess != GA_Update || !m_bIsZip || m_osTemporaryUnzipDir.empty())
        return true;

    auto returnError = [this]()
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot recompress %s",
                 GetDescription());
        RemoveLockFile();
        return false;
    };

    CPLStringList aosFiles(VSIReadDir(m_osTemporaryUnzipDir));
    CPLString osTmpZip(m_osTemporaryUnzipDir + ".zip");
    VSIUnlink(osTmpZip);
    CPLString osTmpZipWithVSIZip("/vsizip/{" + osTmpZip + '}');

    std::map<CPLString, int> oMapLayerOrder;
    for (size_t i = 0; i < layerNames.size(); i++)
        oMapLayerOrder[layerNames[i]] = static_cast<int>(i);

    std::vector<CPLString> sortedFiles;
    vsi_l_offset nTotalUncompressedSize = 0;
    for (int i = 0; i < aosFiles.size(); i++)
    {
        sortedFiles.emplace_back(aosFiles[i]);
        const CPLString osSrcFile(
            CPLFormFilenameSafe(m_osTemporaryUnzipDir, aosFiles[i], nullptr));
        VSIStatBufL sStat;
        if (VSIStatL(osSrcFile, &sStat) == 0)
        {
            nTotalUncompressedSize += sStat.st_size;
        }
    }

    // Sort files by their layer orders, and then for files of the same layer,
    // make shp appear first, and then by filename order
    std::sort(sortedFiles.begin(), sortedFiles.end(),
              [&oMapLayerOrder](const CPLString &a, const CPLString &b)
              {
                  int iA = INT_MAX;
                  auto oIterA =
                      oMapLayerOrder.find(CPLGetBasenameSafe(a).c_str());
                  if (oIterA != oMapLayerOrder.end())
                      iA = oIterA->second;
                  int iB = INT_MAX;
                  auto oIterB =
                      oMapLayerOrder.find(CPLGetBasenameSafe(b).c_str());
                  if (oIterB != oMapLayerOrder.end())
                      iB = oIterB->second;
                  if (iA < iB)
                      return true;
                  if (iA > iB)
                      return false;
                  if (iA != INT_MAX)
                  {
                      if (EQUAL(CPLGetExtensionSafe(a).c_str(), "shp"))
                          return true;
                      if (EQUAL(CPLGetExtensionSafe(b).c_str(), "shp"))
                          return false;
                  }
                  return a < b;
              });

    CPLConfigOptionSetter oZIP64Setter(
        "CPL_CREATE_ZIP64",
        nTotalUncompressedSize < 4000U * 1000 * 1000 ? "NO" : "YES", true);

    /* Maintain a handle on the ZIP opened */
    VSILFILE *fpZIP = VSIFOpenExL(osTmpZipWithVSIZip, "wb", true);
    if (fpZIP == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s: %s",
                 osTmpZipWithVSIZip.c_str(), VSIGetLastErrorMsg());
        return returnError();
    }

    for (const auto &osFilename : sortedFiles)
    {
        const char *pszFilename = osFilename.c_str();
        if (!EQUAL(pszFilename, ".") && !EQUAL(pszFilename, ".."))
        {
            const CPLString osSrcFile(CPLFormFilenameSafe(
                m_osTemporaryUnzipDir, pszFilename, nullptr));
            const CPLString osDestFile(
                CPLFormFilenameSafe(osTmpZipWithVSIZip, pszFilename, nullptr));
            if (CPLCopyFile(osDestFile, osSrcFile) != 0)
            {
                VSIFCloseL(fpZIP);
                return returnError();
            }
        }
    }

    VSIFCloseL(fpZIP);

    const bool bOverwrite =
        CPLTestBool(CPLGetConfigOption("OGR_SHAPE_PACK_IN_PLACE",
#ifdef _WIN32
                                       "YES"
#else
                                       "NO"
#endif
                                       ));
    if (bOverwrite)
    {
        VSILFILE *fpTarget = nullptr;
        for (int i = 0; i < 10; i++)
        {
            fpTarget = VSIFOpenL(GetDescription(), "rb+");
            if (fpTarget)
                break;
            CPLSleep(0.1);
        }
        if (!fpTarget)
            return returnError();
        bool bCopyOK = CopyInPlace(fpTarget, osTmpZip);
        VSIFCloseL(fpTarget);
        VSIUnlink(osTmpZip);
        if (!bCopyOK)
        {
            return returnError();
        }
    }
    else
    {
        if (VSIUnlink(GetDescription()) != 0 ||
            CPLMoveFile(GetDescription(), osTmpZip) != 0)
        {
            return returnError();
        }
    }

    VSIRmdirRecursive(m_osTemporaryUnzipDir);
    m_osTemporaryUnzipDir.clear();

    for (auto &poLayer : m_apoLayers)
    {
        poLayer->UpdateFollowingDeOrRecompression();
    }

    RemoveLockFile();

    return true;
}

/************************************************************************/
/*                            CopyInPlace()                             */
/************************************************************************/

bool OGRShapeDataSource::CopyInPlace(VSILFILE *fpTarget,
                                     const CPLString &osSourceFilename)
{
    return CPL_TO_BOOL(VSIOverwriteFile(fpTarget, osSourceFilename.c_str()));
}
