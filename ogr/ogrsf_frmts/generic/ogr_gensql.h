/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes related to generic implementation of ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_GENSQL_H_INCLUDED
#define OGR_GENSQL_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_swq.h"
#include "cpl_hash_set.h"
#include "cpl_string.h"

#include <vector>

/*! @cond Doxygen_Suppress */

#define GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(poFDefn, iGeom)                    \
    ((poFDefn)->GetFieldCount() + SPECIAL_FIELD_COUNT + (iGeom))

#define IS_GEOM_FIELD_INDEX(poFDefn, idx)                                      \
    (((idx) >= (poFDefn)->GetFieldCount() + SPECIAL_FIELD_COUNT) &&            \
     ((idx) < (poFDefn)->GetFieldCount() + SPECIAL_FIELD_COUNT +               \
                  (poFDefn)->GetGeomFieldCount()))

#define ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(poFDefn, idx)                      \
    ((idx) - ((poFDefn)->GetFieldCount() + SPECIAL_FIELD_COUNT))

/************************************************************************/
/*                        OGRGenSQLResultsLayer                         */
/************************************************************************/

class swq_select;

class OGRGenSQLResultsLayer final : public OGRLayer
{
  private:
    GDALDataset *m_poSrcDS = nullptr;
    OGRLayer *m_poSrcLayer = nullptr;
    std::unique_ptr<swq_select> m_pSelectInfo{};

    std::string m_osInitialWHERE{};
    bool m_bForwardWhereToSourceLayer = true;
    bool m_bEOF = false;

    // Array of source layers (owned by m_poSrcDS or m_apoExtraDS)
    std::vector<OGRLayer *> m_apoTableLayers{};

    // Array of extra datasets when referencing a table/layer by a dataset name
    std::vector<std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser>>
        m_apoExtraDS{};

    OGRFeatureDefn *m_poDefn = nullptr;

    std::vector<int> m_anGeomFieldToSrcGeomField{};

    std::vector<GIntBig> m_anFIDIndex{};
    bool m_bOrderByValid = false;

    GIntBig m_nNextIndexFID = 0;
    std::unique_ptr<OGRFeature> m_poSummaryFeature{};

    int m_iFIDFieldIndex = 0;

    GIntBig m_nIteratedFeatures = -1;
    std::vector<std::string> m_aosDistinctList{};

    bool PrepareSummary();

    std::unique_ptr<OGRFeature> TranslateFeature(std::unique_ptr<OGRFeature>);
    void CreateOrderByIndex();
    void ReadIndexFields(OGRFeature *poSrcFeat, int nOrderItems,
                         OGRField *pasIndexFields);
    void SortIndexSection(const OGRField *pasIndexFields, GIntBig *panMerged,
                          size_t nStart, size_t nEntries);
    void FreeIndexFields(OGRField *pasIndexFields, size_t l_nIndexSize);
    int Compare(const OGRField *pasFirst, const OGRField *pasSecond);

    void ClearFilters();
    void ApplyFiltersToSource();

    void FindAndSetIgnoredFields();
    void ExploreExprForIgnoredFields(swq_expr_node *expr, CPLHashSet *hSet);
    void AddFieldDefnToSet(int iTable, int iColumn, CPLHashSet *hSet);

    int ContainGeomSpecialField(swq_expr_node *expr);

    void InvalidateOrderByIndex();

    int MustEvaluateSpatialFilterOnGenSQL();

    CPL_DISALLOW_COPY_ASSIGN(OGRGenSQLResultsLayer)

  public:
    OGRGenSQLResultsLayer(GDALDataset *poSrcDS,
                          std::unique_ptr<swq_select> &&pSelectInfo,
                          const OGRGeometry *poSpatFilter, const char *pszWHERE,
                          const char *pszDialect);
    virtual ~OGRGenSQLResultsLayer();

    virtual OGRGeometry *GetSpatialFilter() override;

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr SetNextByIndex(GIntBig nIndex) override;
    virtual OGRFeature *GetFeature(GIntBig nFID) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;

    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce) override;

    virtual int TestCapability(const char *) override;

    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *) override;
    virtual OGRErr SetAttributeFilter(const char *) override;

    bool GetArrowStream(struct ArrowArrayStream *out_stream,
                        CSLConstList papszOptions = nullptr) override;

    int GetArrowSchema(struct ArrowArrayStream *stream,
                       struct ArrowSchema *out_schema) override;

  protected:
    friend struct OGRGenSQLResultsLayerArrowStreamPrivateData;

    int GetArrowSchemaForwarded(struct ArrowArrayStream *stream,
                                struct ArrowSchema *out_schema) const;

    int GetNextArrowArray(struct ArrowArrayStream *stream,
                          struct ArrowArray *out_array) override;

    int GetNextArrowArrayForwarded(struct ArrowArrayStream *stream,
                                   struct ArrowArray *out_array);
};

/*! @endcond */

#endif /* ndef OGR_GENSQL_H_INCLUDED */
