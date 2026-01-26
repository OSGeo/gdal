/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/VRT driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_VRT_H_INCLUDED
#define OGR_VRT_H_INCLUDED

#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "ogrlayerpool.h"
#include "ogrsf_frmts.h"

#include <set>
#include <string>
#include <vector>

typedef enum
{
    VGS_None,
    VGS_Direct,
    VGS_PointFromColumns,
    VGS_WKT,
    VGS_WKB,
    VGS_Shape
} OGRVRTGeometryStyle;

/************************************************************************/
/*                         OGRVRTGeomFieldProps                         */
/************************************************************************/

class OGRVRTGeomFieldProps
{
  public:
    CPLString osName{};  // Name of the VRT geometry field */
    OGRwkbGeometryType eGeomType = wkbUnknown;
    const OGRSpatialReference *poSRS = nullptr;

    bool bSrcClip = false;
    std::unique_ptr<OGRGeometry> poSrcRegion{};

    // Geometry interpretation related.
    OGRVRTGeometryStyle eGeometryStyle = VGS_Direct;

    // Points to a OGRField for VGS_WKT, VGS_WKB, VGS_Shape and OGRGeomField
    // for VGS_Direct.
    int iGeomField = -1;

    // VGS_PointFromColumn
    int iGeomXField = -1;
    int iGeomYField = -1;
    int iGeomZField = -1;
    int iGeomMField = -1;
    bool bReportSrcColumn = true;
    bool bUseSpatialSubquery = false;
    bool bNullable = true;

    OGREnvelope sStaticEnvelope{};

    OGRGeomCoordinatePrecision sCoordinatePrecision{};

    OGRVRTGeomFieldProps();
    ~OGRVRTGeomFieldProps();

  private:
    CPL_DISALLOW_COPY_ASSIGN(OGRVRTGeomFieldProps)
};

/************************************************************************/
/*                             OGRVRTLayer                              */
/************************************************************************/

class OGRVRTDataSource;

class OGRVRTLayer final : public OGRLayer
{
  protected:
    OGRVRTDataSource *poDS = nullptr;
    std::vector<std::unique_ptr<OGRVRTGeomFieldProps>> apoGeomFieldProps{};

    bool bHasFullInitialized = false;
    CPLString osName{};
    CPLXMLNode *psLTree = nullptr;
    CPLString osVRTDirectory{};

    OGRFeatureDefn *poFeatureDefn = nullptr;

    std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser> poSrcDS{};
    OGRLayer *poSrcLayer = nullptr;
    OGRFeatureDefn *poSrcFeatureDefn = nullptr;
    bool bNeedReset = true;
    bool bSrcLayerFromSQL = false;
    bool bSrcDSShared = false;
    bool bAttrFilterPassThrough = false;

    char *pszAttrFilter = nullptr;

    int iFIDField = -1;  // -1 means pass through.
    CPLString osFIDFieldName{};
    int iStyleField = -1;  // -1 means pass through.

    // Attribute mapping.
    std::vector<int> anSrcField{};
    std::vector<int> abDirectCopy{};

    bool bUpdate = false;

    GIntBig nFeatureCount = -1;

    bool bError = false;

    bool m_bEmptyResultSet = false;

    OGRFeature *TranslateFeature(OGRFeature *&, int bUseSrcRegion);
    OGRFeature *TranslateVRTFeatureToSrcFeature(OGRFeature *poVRTFeature);

    bool ResetSourceReading();

    bool FullInitialize();

    OGRFeatureDefn *GetSrcLayerDefn();
    void ClipAndAssignSRS(OGRFeature *poFeature);

    bool ParseGeometryField(CPLXMLNode *psNode, CPLXMLNode *psNodeParent,
                            OGRVRTGeomFieldProps *poProps);

    CPL_DISALLOW_COPY_ASSIGN(OGRVRTLayer)

  public:
    explicit OGRVRTLayer(OGRVRTDataSource *poDSIn);
    ~OGRVRTLayer() override;

    bool FastInitialize(CPLXMLNode *psLTree, const char *pszVRTDirectory,
                        int bUpdate);

    const char *GetName() const override
    {
        return osName.c_str();
    }

    OGRwkbGeometryType GetGeomType() const override;

    /* -------------------------------------------------------------------- */
    /*      Caution : all the below methods should care of calling          */
    /*      FullInitialize() if not already done                            */
    /* -------------------------------------------------------------------- */

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    OGRFeature *GetFeature(GIntBig nFeatureId) override;

    OGRErr SetNextByIndex(GIntBig nIndex) override;

    const OGRFeatureDefn *GetLayerDefn() const override;

    GIntBig GetFeatureCount(int) override;

    OGRErr SetAttributeFilter(const char *) override;

    int TestCapability(const char *) const override;

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce = TRUE) override;

    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *poGeomIn) override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    OGRErr ISetFeature(OGRFeature *poFeature) override;

    OGRErr DeleteFeature(GIntBig nFID) override;

    OGRErr SyncToDisk() override;

    const char *GetFIDColumn() const override;

    OGRErr StartTransaction() override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;

    OGRErr SetIgnoredFields(CSLConstList papszFields) override;

    GDALDataset *GetSrcDataset();
};

/************************************************************************/
/*                           OGRVRTDataSource                           */
/************************************************************************/

typedef enum
{
    OGR_VRT_PROXIED_LAYER,
    OGR_VRT_LAYER,
    OGR_VRT_OTHER_LAYER,
} OGRLayerType;

class OGRVRTDataSource final : public GDALDataset
{
    std::unique_ptr<OGRLayerPool> poLayerPool{};

    OGRLayer **papoLayers = nullptr;
    OGRLayerType *paeLayerType = nullptr;
    int nLayers = 0;

    CPLXMLNode *psTree = nullptr;

    int nCallLevel = 0;

    std::set<std::string> aosOtherDSNameSet{};

    OGRVRTDataSource *poParentDS = nullptr;
    bool bRecursionDetected = false;

    OGRLayer *InstantiateWarpedLayer(CPLXMLNode *psLTree,
                                     const char *pszVRTDirectory, int bUpdate,
                                     int nRecLevel);
    OGRLayer *InstantiateUnionLayer(CPLXMLNode *psLTree,
                                    const char *pszVRTDirectory, int bUpdate,
                                    int nRecLevel);

    CPL_DISALLOW_COPY_ASSIGN(OGRVRTDataSource)

  public:
    explicit OGRVRTDataSource(GDALDriver *poDriver);
    ~OGRVRTDataSource() override;

    int CloseDependentDatasets() override;

    OGRLayer *InstantiateLayer(CPLXMLNode *psLTree, const char *pszVRTDirectory,
                               int bUpdate, int nRecLevel = 0);

    OGRLayer *InstantiateLayerInternal(CPLXMLNode *psLTree,
                                       const char *pszVRTDirectory, int bUpdate,
                                       int nRecLevel);

    bool Initialize(CPLXMLNode *psXML, const char *pszName, int bUpdate);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;

    int TestCapability(const char *) const override;

    char **GetFileList() override;

    // Anti-recursion mechanism for standard Open.
    void SetCallLevel(int nCallLevelIn)
    {
        nCallLevel = nCallLevelIn;
    }

    int GetCallLevel() const
    {
        return nCallLevel;
    }

    void SetParentDS(OGRVRTDataSource *poParentDSIn)
    {
        poParentDS = poParentDSIn;
    }

    OGRVRTDataSource *GetParentDS()
    {
        return poParentDS;
    }

    void SetRecursionDetected()
    {
        bRecursionDetected = true;
    }

    bool GetRecursionDetected() const
    {
        return bRecursionDetected;
    }

    // Anti-recursion mechanism for shared Open.
    void AddForbiddenNames(const char *pszOtherDSName);
    bool IsInForbiddenNames(const char *pszOtherDSName) const;
};

#endif  // ndef OGR_VRT_H_INCLUDED
