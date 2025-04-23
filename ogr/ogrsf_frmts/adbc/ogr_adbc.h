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
/*                 OGRArrowArrayToOGRFeatureAdapterLayer                */
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

    ~OGRArrowArrayToOGRFeatureAdapterLayer()
    {
        m_poLayerDefn->Release();
    }

    OGRFeatureDefn *GetLayerDefn() override
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

    int TestCapability(const char *pszCap) override
    {
        return EQUAL(pszCap, OLCCreateField) ||
               EQUAL(pszCap, OLCSequentialWrite);
    }

    OGRErr CreateField(const OGRFieldDefn *poFieldDefn, int) override
    {
        m_poLayerDefn->AddFieldDefn(poFieldDefn);
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
/*                            OGRADBCLayer                              */
/************************************************************************/

class OGRADBCDataset;

class OGRADBCLayer final : public OGRLayer,
                           public OGRGetNextFeatureThroughRaw<OGRADBCLayer>
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

  private:
    friend class OGRADBCDataset;

    OGRADBCDataset *m_poDS = nullptr;
    const std::string m_osBaseStatement{};    // as provided by user
    std::string m_osModifiedBaseStatement{};  // above tuned to use ST_AsWKB()
    std::string m_osModifiedSelect{};         // SELECT part of above
    std::string m_osAttributeFilter{};
    std::unique_ptr<AdbcStatement> m_statement{};
    std::unique_ptr<OGRArrowArrayToOGRFeatureAdapterLayer> m_poAdapterLayer{};
    std::unique_ptr<OGRArrowArrayStream> m_stream{};

    struct ArrowSchema m_schema
    {
    };

    bool m_bEOF = false;
    size_t m_nIdx = 0;
    GIntBig m_nFeatureID = 0;
    bool m_bIsParquetLayer = false;

    std::vector<GeomColBBOX>
        m_geomColBBOX{};                     // same size as GetGeomFieldCount()
    std::vector<OGREnvelope3D> m_extents{};  // same size as GetGeomFieldCount()

    OGRFeature *GetNextRawFeature();
    bool GetArrowStreamInternal(struct ArrowArrayStream *out_stream);
    GIntBig GetFeatureCountParquet();

    void BuildLayerDefn(bool bInternalUse);
    bool ReplaceStatement(const char *pszNewStatement);
    bool UpdateStatement();
    std::string GetCurrentStatement() const;

    CPL_DISALLOW_COPY_ASSIGN(OGRADBCLayer)

  public:
    OGRADBCLayer(OGRADBCDataset *poDS, const char *pszName,
                 const char *pszStatement,
                 std::unique_ptr<AdbcStatement> poStatement,
                 std::unique_ptr<OGRArrowArrayStream> poStream,
                 ArrowSchema *schema, bool bInternalUse);
    ~OGRADBCLayer() override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poAdapterLayer->GetLayerDefn();
    }

    void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRADBCLayer)
    int TestCapability(const char *) override;
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

  public:
    OGRADBCDataset() = default;
    ~OGRADBCDataset() override;

    bool Open(const GDALOpenInfo *poOpenInfo);

    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int idx) override
    {
        return (idx >= 0 && idx < GetLayerCount()) ? m_apoLayers[idx].get()
                                                   : nullptr;
    }

    OGRLayer *GetLayerByName(const char *pszName) override;

    std::unique_ptr<OGRADBCLayer> CreateLayer(const char *pszStatement,
                                              const char *pszLayerName,
                                              bool bInternalUse);

    std::unique_ptr<OGRADBCLayer> CreateInternalLayer(const char *pszStatement)
    {
        return CreateLayer(pszStatement, "temp", true);
    }

    OGRLayer *ExecuteSQL(const char *pszStatement, OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
};

/************************************************************************/
/*                            OGRADBCError                              */
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
