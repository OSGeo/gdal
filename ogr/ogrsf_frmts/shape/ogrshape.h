/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the Shapefile driver to implement
 *           integration with OGR.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRSHAPE_H_INCLUDED
#define OGRSHAPE_H_INCLUDED

#ifdef RENAME_INTERNAL_SHAPELIB_SYMBOLS
#include "gdal_shapelib_symbol_rename.h"
#endif

#include "ogrsf_frmts.h"
#include "shapefil.h"
#include "shp_vsi.h"
#include "ogrlayerpool.h"
#include <set>
#include <vector>

/* Was limited to 255 until OGR 1.10, but 254 seems to be a more */
/* conventional limit (http://en.wikipedia.org/wiki/Shapefile, */
/* http://www.clicketyclick.dk/databases/xbase/format/data_types.html, */
/* #5052 ) */
#define OGR_DBF_MAX_FIELD_WIDTH 254

/* ==================================================================== */
/*      Functions from Shape2ogr.cpp.                                   */
/* ==================================================================== */
OGRFeature *SHPReadOGRFeature(SHPHandle hSHP, DBFHandle hDBF,
                              OGRFeatureDefn *poDefn, int iShape,
                              SHPObject *psShape, const char *pszSHPEncoding,
                              bool &bHasWarnedWrongWindingOrder);
OGRGeometry *SHPReadOGRObject(SHPHandle hSHP, int iShape, SHPObject *psShape,
                              bool &bHasWarnedWrongWindingOrder);
OGRFeatureDefn *SHPReadOGRFeatureDefn(const char *pszName, SHPHandle hSHP,
                                      DBFHandle hDBF,
                                      const char *pszSHPEncoding,
                                      int bAdjustType);
OGRErr SHPWriteOGRFeature(SHPHandle hSHP, DBFHandle hDBF,
                          OGRFeatureDefn *m_poFeatureDefn,
                          OGRFeature *poFeature, const char *pszSHPEncoding,
                          bool *pbTruncationWarningEmitted, bool bRewind);

/************************************************************************/
/*                         OGRShapeGeomFieldDefn                        */
/************************************************************************/

class OGRShapeGeomFieldDefn final : public OGRGeomFieldDefn
{
    CPL_DISALLOW_COPY_ASSIGN(OGRShapeGeomFieldDefn)

    std::string m_osFullName{};
    mutable bool m_bSRSSet = false;
    mutable CPLString m_osPrjFile{};

  public:
    OGRShapeGeomFieldDefn(const char *pszFullNameIn, OGRwkbGeometryType eType,
                          int bSRSSetIn, OGRSpatialReference *poSRSIn)
        : OGRGeomFieldDefn("", eType), m_osFullName(pszFullNameIn),
          m_bSRSSet(CPL_TO_BOOL(bSRSSetIn))
    {
        SetSpatialRef(poSRSIn);
    }

    const OGRSpatialReference *GetSpatialRef() const override;

    void SetSRSSet()
    {
        m_bSRSSet = true;
    }

    const CPLString &GetPrjFilename() const
    {
        return m_osPrjFile;
    }

    void SetPrjFilename(const std::string &osFilename)
    {
        m_osPrjFile = osFilename;
    }
};

/************************************************************************/
/*                            OGRShapeLayer                             */
/************************************************************************/

class OGRShapeDataSource;

class OGRShapeLayer final : public OGRAbstractProxiedLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRShapeLayer)

    OGRShapeDataSource *m_poDS = nullptr;

    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    int m_iNextShapeId = 0;
    int m_nTotalShapeCount = 0;

    std::string m_osFullName{};

    SHPHandle m_hSHP = nullptr;
    DBFHandle m_hDBF = nullptr;

    bool m_bUpdateAccess = false;

    OGRwkbGeometryType m_eRequestedGeomType = wkbUnknown;
    int ResetGeomType(int nNewType);

    bool ScanIndices();

    GIntBig *m_panMatchingFIDs = nullptr;
    int m_iMatchingFID = 0;
    void ClearMatchingFIDs();

    OGRGeometry *m_poFilterGeomLastValid = nullptr;
    int m_nSpatialFIDCount = 0;
    int *m_panSpatialFIDs = nullptr;
    void ClearSpatialFIDs();

    bool m_bHeaderDirty = false;
    bool m_bSHPNeedsRepack = false;
    bool m_bCheckedForQIX = false;
    SHPTreeDiskHandle m_hQIX = nullptr;
    bool CheckForQIX();

    bool m_bCheckedForSBN = false;
    SBNSearchHandle m_hSBN = nullptr;
    bool CheckForSBN();

    bool m_bSbnSbxDeleted = false;

    CPLString ConvertCodePage(const char *);
    CPLString m_osEncoding{};

    bool m_bTruncationWarningEmitted = false;

    bool m_bHSHPWasNonNULL = false;  // Must try to reopen a .shp?
    bool m_bHDBFWasNonNULL = false;  // Must try to reopen a .dbf

    // Current state of opening of file descriptor to .shp and .dbf.

    typedef enum
    {
        FD_OPENED,
        FD_CLOSED,
        FD_CANNOT_REOPEN
    } FileDescriptorState;

    FileDescriptorState m_eFileDescriptorsState = FD_OPENED;

    bool TouchLayer();
    bool ReopenFileDescriptors();

    bool m_bResizeAtClose = false;

    void TruncateDBF();

    bool m_bCreateSpatialIndexAtClose = false;
    bool m_bRewindOnWrite = false;
    bool m_bHasWarnedWrongWindingOrder = false;
    bool m_bLastGetNextArrowArrayUsedOptimizedCodePath = false;

    bool m_bAutoRepack = false;

    typedef enum
    {
        YES,
        NO,
        MAYBE
    } NormandyState; /* French joke. "Peut'et' ben que oui, peut'et' ben que
                        non." Sorry :-) */

    NormandyState m_eNeedRepack = MAYBE;

    // Set of field names (in upper case). Built and invalidated when convenient
    std::set<CPLString> m_oSetUCFieldName{};

    bool StartUpdate(const char *pszOperation);

    void CloseUnderlyingLayer() override;

    // WARNING: Each of the below public methods should start with a call to
    // TouchLayer() and test its return value, so as to make sure that
    // the layer is properly re-opened if necessary.

  public:
    OGRErr CreateSpatialIndex(int nMaxDepth);
    OGRErr DropSpatialIndex();
    OGRErr Repack();
    OGRErr RecomputeExtent();
    OGRErr ResizeDBF();

    void SetResizeAtClose(bool bFlag)
    {
        m_bResizeAtClose = bFlag;
    }

    const char *GetFullName()
    {
        return m_osFullName.c_str();
    }

    void UpdateFollowingDeOrRecompression();

    OGRFeature *FetchShape(int iShapeId);
    int GetFeatureCountWithSpatialFilterOnly();

    OGRShapeLayer(OGRShapeDataSource *poDSIn, const char *pszName,
                  SHPHandle hSHP, DBFHandle hDBF,
                  const OGRSpatialReference *poSRS, bool bSRSSet,
                  const std::string &osPrjFilename, bool bUpdate,
                  OGRwkbGeometryType eReqType,
                  CSLConstList papszCreateOptions = nullptr);
    virtual ~OGRShapeLayer();

    GDALDataset *GetDataset() override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRErr SetNextByIndex(GIntBig nIndex) override;

    int GetNextArrowArray(struct ArrowArrayStream *,
                          struct ArrowArray *out_array) override;
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override;

    OGRFeature *GetFeature(GIntBig nFeatureId) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr SyncToDisk() override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    GIntBig GetFeatureCount(int) override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    OGRErr IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent3D,
                        bool bForce) override;

    OGRErr CreateField(const OGRFieldDefn *poField,
                       int bApproxOK = TRUE) override;
    OGRErr DeleteField(int iField) override;
    OGRErr ReorderFields(int *panMap) override;
    OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                          int nFlags) override;
    OGRErr AlterGeomFieldDefn(int iGeomField,
                              const OGRGeomFieldDefn *poNewGeomFieldDefn,
                              int nFlagsIn) override;

    int TestCapability(const char *) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRErr SetAttributeFilter(const char *) override;

    OGRErr Rename(const char *pszNewName) override;

    void AddToFileList(CPLStringList &oFileList);

    void CreateSpatialIndexAtClose(int bFlag)
    {
        m_bCreateSpatialIndexAtClose = CPL_TO_BOOL(bFlag);
    }

    void SetModificationDate(const char *pszStr);

    void SetAutoRepack(bool b)
    {
        m_bAutoRepack = b;
    }

    void SetWriteDBFEOFChar(bool b);
};

/************************************************************************/
/*                          OGRShapeDataSource                          */
/************************************************************************/

class OGRShapeDataSource final : public GDALDataset
{
    std::vector<std::unique_ptr<OGRShapeLayer>> m_apoLayers{};
    bool m_bSingleFileDataSource = false;
    std::unique_ptr<OGRLayerPool> m_poPool{};

    std::vector<CPLString> m_oVectorLayerName{};

    bool m_b2GBLimit = false;
    bool m_bIsZip = false;
    bool m_bSingleLayerZip = false;
    CPLString m_osTemporaryUnzipDir{};
    CPLMutex *m_poRefreshLockFileMutex = nullptr;
    CPLCond *m_poRefreshLockFileCond = nullptr;
    VSILFILE *m_psLockFile = nullptr;
    CPLJoinableThread *m_hRefreshLockFileThread = nullptr;
    bool m_bExitRefreshLockFileThread = false;
    bool m_bRefreshLockFileThreadStarted = false;
    double m_dfRefreshLockDelay = 0;

    std::vector<CPLString> GetLayerNames() const;
    void AddLayer(std::unique_ptr<OGRShapeLayer> poLayer);
    static void RefreshLockFile(void *_self);
    void RemoveLockFile();
    bool RecompressIfNeeded(const std::vector<CPLString> &layerNames);

    CPL_DISALLOW_COPY_ASSIGN(OGRShapeDataSource)

  public:
    OGRShapeDataSource();
    virtual ~OGRShapeDataSource();

    OGRLayerPool *GetPool() const
    {
        return m_poPool.get();
    }

    bool Open(GDALOpenInfo *poOpenInfo, bool bTestOpen,
              bool bForceSingleFileDataSource = false);
    bool OpenFile(const char *, bool bUpdate);
    bool OpenZip(GDALOpenInfo *poOpenInfo, const char *pszOriFilename);
    bool CreateZip(const char *pszOriFilename);

    int GetLayerCount() override;
    OGRLayer *GetLayer(int) override;
    OGRLayer *GetLayerByName(const char *) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    OGRLayer *ExecuteSQL(const char *pszStatement, OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;

    int TestCapability(const char *) override;
    OGRErr DeleteLayer(int iLayer) override;

    char **GetFileList() override;

    void SetLastUsedLayer(OGRShapeLayer *poLayer);
    void UnchainLayer(OGRShapeLayer *poLayer);

    bool UncompressIfNeeded();

    SHPHandle DS_SHPOpen(const char *pszShapeFile, const char *pszAccess);
    DBFHandle DS_DBFOpen(const char *pszDBFFile, const char *pszAccess);

    char **GetOpenOptions()
    {
        return papszOpenOptions;
    }

    static const char *const *GetExtensionsForDeletion();

    bool IsZip() const
    {
        return m_bIsZip;
    }

    CPLString GetVSIZipPrefixeDir() const
    {
        return CPLString("/vsizip/{").append(GetDescription()).append("}");
    }

    const CPLString &GetTemporaryUnzipDir() const
    {
        return m_osTemporaryUnzipDir;
    }

    static bool CopyInPlace(VSILFILE *fpTarget,
                            const CPLString &osSourceFilename);
};

#endif /* ndef OGRSHAPE_H_INCLUDED */
