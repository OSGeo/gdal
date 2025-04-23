/******************************************************************************
 *
 * Project:  Memory Array Translator
 * Purpose:  Declaration of MEMDataset, and MEMRasterBand.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MEMDATASET_H_INCLUDED
#define MEMDATASET_H_INCLUDED

#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_rat.h"
#include "ogrsf_frmts.h"

#include <map>
#include <memory>

CPL_C_START
/* Caution: if changing this prototype, also change in
   swig/include/gdal_python.i where it is redefined */
GDALRasterBandH CPL_DLL MEMCreateRasterBand(GDALDataset *, int, GByte *,
                                            GDALDataType, int, int, int);
GDALRasterBandH CPL_DLL MEMCreateRasterBandEx(GDALDataset *, int, GByte *,
                                              GDALDataType, GSpacing, GSpacing,
                                              int);
CPL_C_END

/************************************************************************/
/*                            MEMDataset                                */
/************************************************************************/

class MEMRasterBand;
class OGRMemLayer;

class CPL_DLL MEMDataset CPL_NON_FINAL : public GDALDataset
{
    CPL_DISALLOW_COPY_ASSIGN(MEMDataset)

    friend class MEMRasterBand;

    int bGeoTransformSet;
    double adfGeoTransform[6];

    OGRSpatialReference m_oSRS{};

    std::vector<gdal::GCP> m_aoGCPs{};
    OGRSpatialReference m_oGCPSRS{};

    std::vector<std::unique_ptr<GDALDataset>> m_apoOverviewDS{};

    struct Private;
    std::unique_ptr<Private> m_poPrivate;

    std::vector<std::unique_ptr<OGRMemLayer>> m_apoLayers{};

#if 0
  protected:
    virtual int                 EnterReadWrite(GDALRWFlag eRWFlag);
    virtual void                LeaveReadWrite();
#endif

    friend void GDALRegister_MEM();

    // cppcheck-suppress unusedPrivateFunction
    static GDALDataset *CreateBase(const char *pszFilename, int nXSize,
                                   int nYSize, int nBands, GDALDataType eType,
                                   char **papszParamList);

  protected:
    bool CanBeCloned(int nScopeFlags, bool bCanShareState) const override;

    std::unique_ptr<GDALDataset> Clone(int nScopeFlags,
                                       bool bCanShareState) const override;

  public:
    MEMDataset();
    virtual ~MEMDataset();

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual CPLErr GetGeoTransform(double *) override;
    virtual CPLErr SetGeoTransform(double *) override;

    virtual void *GetInternalHandle(const char *) override;

    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    CPLErr SetGCPs(int nGCPCount, const GDAL_GCP *pasGCPList,
                   const OGRSpatialReference *poSRS) override;
    virtual CPLErr AddBand(GDALDataType eType,
                           char **papszOptions = nullptr) override;
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, BANDMAP_TYPE panBandMap,
                             GSpacing nPixelSpaceBuf, GSpacing nLineSpaceBuf,
                             GSpacing nBandSpaceBuf,
                             GDALRasterIOExtraArg *psExtraArg) override;
    virtual CPLErr IBuildOverviews(const char *pszResampling, int nOverviews,
                                   const int *panOverviewList, int nListBands,
                                   const int *panBandList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList papszOptions) override;

    virtual CPLErr CreateMaskBand(int nFlagsIn) override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override;

    void AddMEMBand(GDALRasterBandH hMEMBand);

    static GDALDataset *Open(GDALOpenInfo *);
    static MEMDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                              int nBands, GDALDataType eType,
                              char **papszParamList);
    static GDALDataset *
    CreateMultiDimensional(const char *pszFilename,
                           CSLConstList papszRootGroupOptions,
                           CSLConstList papszOptions);

    // Vector capabilities

    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    OGRErr DeleteLayer(int iLayer) override;

    int TestCapability(const char *) override;

    OGRLayer *ExecuteSQL(const char *pszStatement, OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;

    bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                        std::string &failureReason) override;

    bool DeleteFieldDomain(const std::string &name,
                           std::string &failureReason) override;

    bool UpdateFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                           std::string &failureReason) override;
};

/************************************************************************/
/*                            MEMRasterBand                             */
/************************************************************************/

class CPL_DLL MEMRasterBand CPL_NON_FINAL : public GDALPamRasterBand
{
  private:
    CPL_DISALLOW_COPY_ASSIGN(MEMRasterBand)

  protected:
    friend class MEMDataset;

    GByte *pabyData;
    GSpacing nPixelOffset;
    GSpacing nLineOffset;
    int bOwnData;

    bool m_bIsMask = false;

    MEMRasterBand(GByte *pabyDataIn, GDALDataType eTypeIn, int nXSizeIn,
                  int nYSizeIn, bool bOwnDataIn);

  public:
    MEMRasterBand(GDALDataset *poDS, int nBand, GByte *pabyData,
                  GDALDataType eType, GSpacing nPixelOffset,
                  GSpacing nLineOffset, int bAssumeOwnership,
                  const char *pszPixelType = nullptr);
    virtual ~MEMRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             GSpacing nPixelSpaceBuf, GSpacing nLineSpaceBuf,
                             GDALRasterIOExtraArg *psExtraArg) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;

    virtual CPLErr CreateMaskBand(int nFlagsIn) override;
    virtual bool IsMaskBand() const override;

    // Allow access to MEM driver's private internal memory buffer.
    GByte *GetData() const
    {
        return (pabyData);
    }
};

/************************************************************************/
/*                             OGRMemLayer                              */
/************************************************************************/

class IOGRMemLayerFeatureIterator;

class CPL_DLL OGRMemLayer CPL_NON_FINAL : public OGRLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMemLayer)

    typedef std::map<GIntBig, std::unique_ptr<OGRFeature>> FeatureMap;
    typedef FeatureMap::iterator FeatureIterator;

    OGRFeatureDefn *m_poFeatureDefn = nullptr;

    GIntBig m_nFeatureCount = 0;

    GIntBig m_iNextReadFID = 0;
    GIntBig m_nMaxFeatureCount = 0;  // Max size of papoFeatures.
    OGRFeature **m_papoFeatures = nullptr;
    bool m_bHasHoles = false;

    FeatureMap m_oMapFeatures{};
    FeatureIterator m_oMapFeaturesIter{};

    GIntBig m_iNextCreateFID = 0;

    bool m_bUpdatable = true;
    bool m_bAdvertizeUTF8 = false;

    bool m_bUpdated = false;

    std::string m_osFIDColumn{};

    GDALDataset *m_poDS{};

    // Only use it in the lifetime of a function where the list of features
    // doesn't change.
    IOGRMemLayerFeatureIterator *GetIterator();

  protected:
    OGRFeature *GetFeatureRef(GIntBig nFeatureId);

  public:
    // Clone poSRS if not nullptr
    OGRMemLayer(const char *pszName, const OGRSpatialReference *poSRS,
                OGRwkbGeometryType eGeomType);
    virtual ~OGRMemLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    virtual OGRErr SetNextByIndex(GIntBig nIndex) override;

    OGRFeature *GetFeature(GIntBig nFeatureId) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr IUpsertFeature(OGRFeature *poFeature) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;
    virtual OGRErr DeleteFeature(GIntBig nFID) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    GIntBig GetFeatureCount(int) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr DeleteField(int iField) override;
    virtual OGRErr ReorderFields(int *panMap) override;
    virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                  int nFlags) override;
    virtual OGRErr
    AlterGeomFieldDefn(int iGeomField,
                       const OGRGeomFieldDefn *poNewGeomFieldDefn,
                       int nFlagsIn) override;
    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomField,
                                   int bApproxOK = TRUE) override;

    int TestCapability(const char *) override;

    const char *GetFIDColumn() override
    {
        return m_osFIDColumn.c_str();
    }

    bool IsUpdatable() const
    {
        return m_bUpdatable;
    }

    void SetUpdatable(bool bUpdatableIn)
    {
        m_bUpdatable = bUpdatableIn;
    }

    void SetAdvertizeUTF8(bool bAdvertizeUTF8In)
    {
        m_bAdvertizeUTF8 = bAdvertizeUTF8In;
    }

    void SetFIDColumn(const char *pszFIDColumn)
    {
        m_osFIDColumn = pszFIDColumn;
    }

    bool HasBeenUpdated() const
    {
        return m_bUpdated;
    }

    void SetUpdated(bool bUpdated)
    {
        m_bUpdated = bUpdated;
    }

    GIntBig GetNextReadFID()
    {
        return m_iNextReadFID;
    }

    void SetDataset(GDALDataset *poDS)
    {
        m_poDS = poDS;
    }

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
};

#endif /* ndef MEMDATASET_H_INCLUDED */
