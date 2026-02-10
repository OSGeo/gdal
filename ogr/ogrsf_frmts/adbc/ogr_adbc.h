/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Arrow Database Connectivity driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_ADBC_INCLUDED
#define OGR_ADBC_INCLUDED

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogrlayerarrow.h"

#include "ogr_adbc_internal.h"

/************************************************************************/
/*                OGRArrowArrayToOGRFeatureAdapterLayer                 */
/************************************************************************/

class OGRArrowArrayToOGRFeatureAdapterLayer final : public OGRLayer
{
    friend class OGRADBCLayer;
    OGRFeatureDefn *m_poLayerDefn = nullptr;
    std::vector<std::unique_ptr<OGRFeature>> m_apoFeatures{};

    CPL_DISALLOW_COPY_ASSIGN(OGRArrowArrayToOGRFeatureAdapterLayer)

  public:
    explicit OGRArrowArrayToOGRFeatureAdapterLayer(const char *pszName)
    {
        m_poLayerDefn = new OGRFeatureDefn(pszName);
        m_poLayerDefn->SetGeomType(wkbNone);
        m_poLayerDefn->Reference();
    }

    ~OGRArrowArrayToOGRFeatureAdapterLayer() override;

    using OGRLayer::GetLayerDefn;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_poLayerDefn;
    }

    void ResetReading() override
    {
    }

    OGRFeature *GetNextFeature() override
    {
        return nullptr;
    }

    int TestCapability(const char *pszCap) const override
    {
        return EQUAL(pszCap, OLCCreateField) ||
               EQUAL(pszCap, OLCSequentialWrite);
    }

    OGRErr CreateField(const OGRFieldDefn *poFieldDefn, int) override
    {
        m_poLayerDefn->AddFieldDefn(poFieldDefn);
        return OGRERR_NONE;
    }

    OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomFieldDefn,
                           int) override
    {
        m_poLayerDefn->AddGeomFieldDefn(poGeomFieldDefn);
        return OGRERR_NONE;
    }

    OGRErr ICreateFeature(OGRFeature *poFeature) override
    {
        m_apoFeatures.emplace_back(
            std::unique_ptr<OGRFeature>(poFeature->Clone()));
        return OGRERR_NONE;
    }
};

/************************************************************************/
/*                             OGRADBCLayer                             */
/************************************************************************/

class OGRADBCDataset;

class OGRADBCLayer /* non final */ : public OGRLayer,
                                     public OGRGetNextFeatureThroughRaw<
                                         OGRADBCLayer>
{
  public:
    //! Describe the bbox column of a geometry column
    struct GeomColBBOX
    {
        std::string osXMin{};  // empty if no bbox column
        std::string osYMin{};
        std::string osXMax{};
        std::string osYMax{};
    };

  protected:
    friend class OGRADBCDataset;

    OGRADBCDataset *m_poDS = nullptr;
    const std::string m_osBaseStatement{};    // as provided by user
    std::string m_osModifiedBaseStatement{};  // above tuned to use ST_AsWKB()
    std::string m_osModifiedSelect{};         // SELECT part of above
    std::string m_osAttributeFilter{};
    std::unique_ptr<AdbcStatement> m_statement{};
    std::unique_ptr<OGRArrowArrayToOGRFeatureAdapterLayer> m_poAdapterLayer{};
    std::unique_ptr<OGRArrowArrayStream> m_stream{};
    bool m_bInternalUse = false;
    bool m_bLayerDefinitionError = false;

    struct ArrowSchema m_schema{};

    bool m_bEOF = false;
    size_t m_nIdx = 0;
    GIntBig m_nFeatureID = 0;
    GIntBig m_nMaxFeatureID = -1;
    bool m_bIsParquetLayer = false;

    std::vector<GeomColBBOX>
        m_geomColBBOX{};                     // same size as GetGeomFieldCount()
    std::vector<OGREnvelope3D> m_extents{};  // same size as GetGeomFieldCount()
    std::string m_osFIDColName{};

    OGRFeature *GetNextRawFeature();
    bool GetArrowStreamInternal(struct ArrowArrayStream *out_stream);
    GIntBig GetFeatureCountSelectCountStar();
    GIntBig GetFeatureCountArrow();
    GIntBig GetFeatureCountParquet();

    bool BuildLayerDefnInit();
    virtual void BuildLayerDefn();
    bool ReplaceStatement(const char *pszNewStatement);
    bool UpdateStatement();
    virtual std::string GetCurrentStatement() const;

    virtual bool RunDeferredCreation()
    {
        return true;
    }

    CPL_DISALLOW_COPY_ASSIGN(OGRADBCLayer)

  public:
    OGRADBCLayer(OGRADBCDataset *poDS, const char *pszName,
                 const std::string &osStatement, bool bInternalUse);
    OGRADBCLayer(OGRADBCDataset *poDS, const char *pszName,
                 std::unique_ptr<OGRArrowArrayStream> poStream,
                 ArrowSchema *schema, bool bInternalUse);
    ~OGRADBCLayer() override;

    bool GotError();

    using OGRLayer::GetLayerDefn;
    const OGRFeatureDefn *GetLayerDefn() const override;

    const char *GetName() const override
    {
        return GetDescription();
    }

    void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRADBCLayer)
    int TestCapability(const char *) const override;
    GDALDataset *GetDataset() override;
    bool GetArrowStream(struct ArrowArrayStream *out_stream,
                        CSLConstList papszOptions = nullptr) override;
    GIntBig GetFeatureCount(int bForce) override;

    OGRErr SetAttributeFilter(const char *pszFilter) override;
    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;
    OGRErr IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent,
                        bool bForce) override;

    const char *GetFIDColumn() const override;
};

/************************************************************************/
/*                         OGRADBCBigQueryLayer                         */
/************************************************************************/

class OGRADBCBigQueryLayer final : public OGRADBCLayer
{
  private:
    friend class OGRADBCDataset;

    bool m_bDeferredCreation = false;

    void BuildLayerDefn() override;
    bool RunDeferredCreation() override;
    std::string GetCurrentStatement() const override;
    bool GetBigQueryDatasetAndTableId(std::string &osDatasetId,
                                      std::string &osTableId) const;

  public:
    OGRADBCBigQueryLayer(OGRADBCDataset *poDS, const char *pszName,
                         const std::string &osStatement, bool bInternalUse);

    int TestCapability(const char *) const override;
    GIntBig GetFeatureCount(int bForce) override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;
    OGRErr SetAttributeFilter(const char *pszFilter) override;

    OGRErr CreateField(const OGRFieldDefn *poField, int bApproxOK) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;

    void SetDeferredCreation(const char *pszFIDColName,
                             const OGRGeomFieldDefn *poGeomFieldDefn);
};

/************************************************************************/
/*                            OGRADBCDataset                            */
/************************************************************************/

class OGRADBCDataset final : public GDALDataset
{
    friend class OGRADBCLayer;

    AdbcDriver m_driver{};
    AdbcDatabase m_database{};
    std::unique_ptr<AdbcConnection> m_connection{};
    std::vector<std::unique_ptr<OGRLayer>> m_apoLayers{};
    std::string m_osParquetFilename{};
    bool m_bIsDuckDBDataset = false;
    bool m_bIsDuckDBDriver = false;
    bool m_bSpatialLoaded = false;
    bool m_bIsBigQuery = false;
    std::string m_osBigQueryDatasetId{};

  public:
    OGRADBCDataset() = default;
    ~OGRADBCDataset() override;

    CPLErr FlushCache(bool bAtClosing) override;

    bool Open(const GDALOpenInfo *poOpenInfo);

    int GetLayerCount() const override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    const OGRLayer *GetLayer(int idx) const override
    {
        return (idx >= 0 && idx < GetLayerCount()) ? m_apoLayers[idx].get()
                                                   : nullptr;
    }

    OGRLayer *GetLayerByName(const char *pszName) override;

    std::unique_ptr<OGRADBCLayer> CreateLayer(const char *pszStatement,
                                              const char *pszLayerName,
                                              bool bInternalUse);

    std::unique_ptr<OGRADBCLayer>
    CreateInternalLayer(const char *pszStatement) CPL_WARN_UNUSED_RESULT;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    OGRErr DeleteLayer(int iLayer) override;

    OGRLayer *ExecuteSQL(const char *pszStatement, OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;

    int TestCapability(const char *pszCap) const override;
};

/************************************************************************/
/*                             OGRADBCError                             */
/************************************************************************/

struct OGRADBCError
{
    AdbcError error{ADBC_ERROR_INIT};

    inline OGRADBCError() = default;

    inline ~OGRADBCError()
    {
        clear();
    }

    inline void clear()
    {
        if (error.release)
            error.release(&error);
        memset(&error, 0, sizeof(error));
    }

    inline const char *message() const
    {
        return error.message ? error.message : "";
    }

    inline operator AdbcError *()
    {
        return &error;
    }

    CPL_DISALLOW_COPY_ASSIGN(OGRADBCError)
};

#endif  // OGR_ADBC_INCLUDED
