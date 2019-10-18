/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes related to generic implementation of ExecuteSQL().
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_GENSQL_H_INCLUDED
#define OGR_GENSQL_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_swq.h"
#include "cpl_hash_set.h"
#include "cpl_string.h"

#include <vector>

/*! @cond Doxygen_Suppress */

#define GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(poFDefn, iGeom) \
    ((poFDefn)->GetFieldCount() + SPECIAL_FIELD_COUNT + (iGeom))

#define IS_GEOM_FIELD_INDEX(poFDefn, idx) \
    (((idx) >= (poFDefn)->GetFieldCount() + SPECIAL_FIELD_COUNT) && \
     ((idx) < (poFDefn)->GetFieldCount() + SPECIAL_FIELD_COUNT + (poFDefn)->GetGeomFieldCount()))

#define ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(poFDefn, idx) \
    ((idx) - ((poFDefn)->GetFieldCount() + SPECIAL_FIELD_COUNT))

/************************************************************************/
/*                        OGRGenSQLResultsLayer                         */
/************************************************************************/

class OGRGenSQLResultsLayer final: public OGRLayer
{
  private:
    GDALDataset *poSrcDS;
    OGRLayer    *poSrcLayer;
    void        *pSelectInfo;

    char        *pszWHERE;

    OGRLayer   **papoTableLayers;

    OGRFeatureDefn *poDefn;

    int        *panGeomFieldToSrcGeomField;

    size_t      nIndexSize;
    GIntBig    *panFIDIndex;
    int         bOrderByValid;

    GIntBig      nNextIndexFID;
    OGRFeature  *poSummaryFeature;

    int         iFIDFieldIndex;

    int         nExtraDSCount;
    GDALDataset **papoExtraDS;

    GIntBig     nIteratedFeatures;
    std::vector<CPLString> m_oDistinctList;

    int         PrepareSummary();

    OGRFeature *TranslateFeature( OGRFeature * );
    void        CreateOrderByIndex();
    void        ReadIndexFields( OGRFeature* poSrcFeat,
                                 int nOrderItems,
                                 OGRField *pasIndexFields );
    void        SortIndexSection( const OGRField *pasIndexFields,
                                  GIntBig *panMerged,
                                  size_t nStart, size_t nEntries );
    void        FreeIndexFields(OGRField *pasIndexFields,
                                size_t l_nIndexSize,
                                bool bFreeArray = true);
    int         Compare( const OGRField *pasFirst, const OGRField *pasSecond );

    void        ClearFilters();
    void        ApplyFiltersToSource();

    void        FindAndSetIgnoredFields();
    void        ExploreExprForIgnoredFields(swq_expr_node* expr, CPLHashSet* hSet);
    void        AddFieldDefnToSet(int iTable, int iColumn, CPLHashSet* hSet);

    int         ContainGeomSpecialField(swq_expr_node* expr);

    void        InvalidateOrderByIndex();

    int         MustEvaluateSpatialFilterOnGenSQL();

    CPL_DISALLOW_COPY_ASSIGN(OGRGenSQLResultsLayer)

  public:
                OGRGenSQLResultsLayer( GDALDataset *poSrcDS,
                                       void *pSelectInfo,
                                       OGRGeometry *poSpatFilter,
                                       const char *pszWHERE,
                                       const char *pszDialect );
    virtual     ~OGRGenSQLResultsLayer();

    virtual OGRGeometry *GetSpatialFilter() override;

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr      SetNextByIndex( GIntBig nIndex ) override;
    virtual OGRFeature *GetFeature( GIntBig nFID ) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual int         TestCapability( const char * ) override;

    virtual void        SetSpatialFilter( OGRGeometry * poGeom ) override { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * ) override;
    virtual OGRErr      SetAttributeFilter( const char * ) override;
};

/*! @endcond */

#endif /* ndef OGR_GENSQL_H_INCLUDED */
