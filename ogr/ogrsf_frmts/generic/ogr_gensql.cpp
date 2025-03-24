/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGenSQLResultsLayer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_swq.h"
#include "ogr_p.h"
#include "ogr_gensql.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_recordbatch.h"
#include "ogrlayerarrow.h"
#include "cpl_time.h"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <vector>

//! @cond Doxygen_Suppress
extern const swq_field_type SpecialFieldTypes[SPECIAL_FIELD_COUNT];

class OGRGenSQLGeomFieldDefn final : public OGRGeomFieldDefn
{
  public:
    explicit OGRGenSQLGeomFieldDefn(OGRGeomFieldDefn *poGeomFieldDefn)
        : OGRGeomFieldDefn(poGeomFieldDefn->GetNameRef(),
                           poGeomFieldDefn->GetType()),
          bForceGeomType(FALSE)
    {
        SetSpatialRef(poGeomFieldDefn->GetSpatialRef());
    }

    int bForceGeomType;
};

/************************************************************************/
/*               OGRGenSQLResultsLayerHasSpecialField()                 */
/************************************************************************/

static bool OGRGenSQLResultsLayerHasSpecialField(swq_expr_node *expr,
                                                 int nMinIndexForSpecialField)
{
    if (expr->eNodeType == SNT_COLUMN)
    {
        if (expr->table_index == 0)
        {
            return expr->field_index >= nMinIndexForSpecialField &&
                   expr->field_index <
                       nMinIndexForSpecialField + SPECIAL_FIELD_COUNT;
        }
    }
    else if (expr->eNodeType == SNT_OPERATION)
    {
        for (int i = 0; i < expr->nSubExprCount; i++)
        {
            if (OGRGenSQLResultsLayerHasSpecialField(expr->papoSubExpr[i],
                                                     nMinIndexForSpecialField))
                return true;
        }
    }
    return false;
}

/************************************************************************/
/*                       OGRGenSQLResultsLayer()                        */
/************************************************************************/

OGRGenSQLResultsLayer::OGRGenSQLResultsLayer(
    GDALDataset *poSrcDSIn, std::unique_ptr<swq_select> &&pSelectInfo,
    const OGRGeometry *poSpatFilter, const char *pszWHEREIn,
    const char *pszDialect)
    : m_poSrcDS(poSrcDSIn), m_pSelectInfo(std::move(pSelectInfo))
{
    swq_select *psSelectInfo = m_pSelectInfo.get();

    /* -------------------------------------------------------------------- */
    /*      Identify all the layers involved in the SELECT.                 */
    /* -------------------------------------------------------------------- */
    m_apoTableLayers.reserve(psSelectInfo->table_count);

    for (int iTable = 0; iTable < psSelectInfo->table_count; iTable++)
    {
        swq_table_def *psTableDef = psSelectInfo->table_defs + iTable;
        GDALDataset *poTableDS = m_poSrcDS;

        if (psTableDef->data_source != nullptr)
        {
            std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser> poNewDS(
                GDALDataset::Open(psTableDef->data_source,
                                  GDAL_OF_VECTOR | GDAL_OF_SHARED));
            if (!poNewDS)
            {
                if (strlen(CPLGetLastErrorMsg()) == 0)
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to open secondary datasource\n"
                             "`%s' required by JOIN.",
                             psTableDef->data_source);
                return;
            }

            m_apoExtraDS.emplace_back(std::move(poNewDS));
            poTableDS = m_apoExtraDS.back().get();
        }

        m_apoTableLayers.push_back(
            poTableDS->GetLayerByName(psTableDef->table_name));
        if (!m_apoTableLayers.back())
            return;
    }

    m_poSrcLayer = m_apoTableLayers[0];
    SetMetadata(m_poSrcLayer->GetMetadata("NATIVE_DATA"), "NATIVE_DATA");

    /* -------------------------------------------------------------------- */
    /*      If the user has explicitly requested a OGRSQL dialect, then    */
    /*      we should avoid to forward the where clause to the source layer */
    /*      when there is a risk it cannot understand it (#4022)            */
    /* -------------------------------------------------------------------- */
    m_bForwardWhereToSourceLayer = true;
    if (pszWHEREIn)
    {
        if (psSelectInfo->where_expr && pszDialect != nullptr &&
            EQUAL(pszDialect, "OGRSQL"))
        {
            const int nMinIndexForSpecialField =
                m_poSrcLayer->GetLayerDefn()->GetFieldCount();
            m_bForwardWhereToSourceLayer =
                !OGRGenSQLResultsLayerHasSpecialField(psSelectInfo->where_expr,
                                                      nMinIndexForSpecialField);
        }
        m_osInitialWHERE = pszWHEREIn;
    }

    /* -------------------------------------------------------------------- */
    /*      Prepare a feature definition based on the query.                */
    /* -------------------------------------------------------------------- */
    OGRFeatureDefn *poSrcDefn = m_poSrcLayer->GetLayerDefn();

    m_poDefn = new OGRFeatureDefn(psSelectInfo->table_defs[0].table_alias);
    SetDescription(m_poDefn->GetName());
    m_poDefn->SetGeomType(wkbNone);
    m_poDefn->Reference();

    m_iFIDFieldIndex = poSrcDefn->GetFieldCount();

    for (std::size_t iField = 0; iField < psSelectInfo->column_defs.size();
         iField++)
    {
        swq_col_def *psColDef = &psSelectInfo->column_defs[iField];
        OGRFieldDefn oFDefn("", OFTInteger);
        OGRGeomFieldDefn oGFDefn("", wkbUnknown);
        OGRFieldDefn *poSrcFDefn = nullptr;
        OGRGeomFieldDefn *poSrcGFDefn = nullptr;
        int bIsGeometry = FALSE;
        OGRFeatureDefn *poLayerDefn = nullptr;
        int iSrcGeomField = -1;

        if (psColDef->bHidden)
            continue;

        if (psColDef->table_index != -1)
            poLayerDefn =
                m_apoTableLayers[psColDef->table_index]->GetLayerDefn();

        if (psColDef->field_index > -1 && poLayerDefn != nullptr &&
            psColDef->field_index < poLayerDefn->GetFieldCount())
        {
            poSrcFDefn = poLayerDefn->GetFieldDefn(psColDef->field_index);
        }

        if (poLayerDefn != nullptr &&
            IS_GEOM_FIELD_INDEX(poLayerDefn, psColDef->field_index))
        {
            bIsGeometry = TRUE;
            iSrcGeomField = ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(
                poLayerDefn, psColDef->field_index);
            poSrcGFDefn = poLayerDefn->GetGeomFieldDefn(iSrcGeomField);
        }

        if (psColDef->target_type == SWQ_GEOMETRY)
            bIsGeometry = TRUE;

        if (psColDef->col_func == SWQCF_COUNT)
            bIsGeometry = FALSE;

        if (strlen(psColDef->field_name) == 0 && !bIsGeometry)
        {
            CPLFree(psColDef->field_name);
            psColDef->field_name = static_cast<char *>(CPLMalloc(40));
            snprintf(psColDef->field_name, 40, "FIELD_%d",
                     m_poDefn->GetFieldCount() + 1);
        }

        if (psColDef->field_alias != nullptr)
        {
            if (bIsGeometry)
                oGFDefn.SetName(psColDef->field_alias);
            else
                oFDefn.SetName(psColDef->field_alias);
        }
        else if (psColDef->col_func != SWQCF_NONE)
        {
            const swq_operation *op = swq_op_registrar::GetOperator(
                static_cast<swq_op>(psColDef->col_func));

            oFDefn.SetName(
                CPLSPrintf("%s_%s", op->pszName, psColDef->field_name));
        }
        else
        {
            CPLString osName;
            if (psColDef->table_name[0])
            {
                osName = psColDef->table_name;
                osName += ".";
            }
            osName += psColDef->field_name;

            if (bIsGeometry)
                oGFDefn.SetName(osName);
            else
                oFDefn.SetName(osName);
        }

        if (psColDef->col_func == SWQCF_COUNT)
            oFDefn.SetType(OFTInteger64);
        else if (poSrcFDefn != nullptr)
        {
            if (psColDef->col_func == SWQCF_STDDEV_POP ||
                psColDef->col_func == SWQCF_STDDEV_SAMP)
            {
                oFDefn.SetType(OFTReal);
            }
            else if (psColDef->col_func != SWQCF_AVG ||
                     psColDef->field_type == SWQ_DATE ||
                     psColDef->field_type == SWQ_TIME ||
                     psColDef->field_type == SWQ_TIMESTAMP)
            {
                oFDefn.SetType(poSrcFDefn->GetType());
                if (psColDef->col_func == SWQCF_NONE ||
                    psColDef->col_func == SWQCF_MIN ||
                    psColDef->col_func == SWQCF_MAX)
                {
                    oFDefn.SetSubType(poSrcFDefn->GetSubType());
                }
            }
            else
            {
                oFDefn.SetType(OFTReal);
            }

            if (psColDef->col_func != SWQCF_AVG &&
                psColDef->col_func != SWQCF_STDDEV_POP &&
                psColDef->col_func != SWQCF_STDDEV_SAMP &&
                psColDef->col_func != SWQCF_SUM)
            {
                oFDefn.SetWidth(poSrcFDefn->GetWidth());
                oFDefn.SetPrecision(poSrcFDefn->GetPrecision());
            }

            if (psColDef->col_func == SWQCF_NONE)
                oFDefn.SetDomainName(poSrcFDefn->GetDomainName());
        }
        else if (poSrcGFDefn != nullptr)
        {
            oGFDefn.SetType(poSrcGFDefn->GetType());
            oGFDefn.SetSpatialRef(poSrcGFDefn->GetSpatialRef());
        }
        else if (psColDef->field_index >= m_iFIDFieldIndex)
        {
            switch (SpecialFieldTypes[psColDef->field_index - m_iFIDFieldIndex])
            {
                case SWQ_INTEGER:
                    oFDefn.SetType(OFTInteger);
                    break;
                case SWQ_INTEGER64:
                    oFDefn.SetType(OFTInteger64);
                    break;
                case SWQ_FLOAT:
                    oFDefn.SetType(OFTReal);
                    break;
                default:
                    oFDefn.SetType(OFTString);
                    break;
            }
            if (psColDef->field_index - m_iFIDFieldIndex == SPF_FID &&
                m_poSrcLayer->GetMetadataItem(OLMD_FID64) != nullptr &&
                EQUAL(m_poSrcLayer->GetMetadataItem(OLMD_FID64), "YES"))
            {
                oFDefn.SetType(OFTInteger64);
            }
        }
        else
        {
            switch (psColDef->field_type)
            {
                case SWQ_INTEGER:
                    oFDefn.SetType(OFTInteger);
                    break;

                case SWQ_INTEGER64:
                    oFDefn.SetType(OFTInteger64);
                    break;

                case SWQ_BOOLEAN:
                    oFDefn.SetType(OFTInteger);
                    oFDefn.SetSubType(OFSTBoolean);
                    break;

                case SWQ_FLOAT:
                    oFDefn.SetType(OFTReal);
                    break;

                default:
                    oFDefn.SetType(OFTString);
                    break;
            }
        }

        /* setting up the target_type */
        switch (psColDef->target_type)
        {
            case SWQ_OTHER:
                break;
            case SWQ_INTEGER:
                oFDefn.SetType(OFTInteger);
                break;
            case SWQ_INTEGER64:
                oFDefn.SetType(OFTInteger64);
                break;
            case SWQ_BOOLEAN:
                oFDefn.SetType(OFTInteger);
                oFDefn.SetSubType(OFSTBoolean);
                break;
            case SWQ_FLOAT:
                oFDefn.SetType(OFTReal);
                break;
            case SWQ_STRING:
                oFDefn.SetType(OFTString);
                break;
            case SWQ_TIMESTAMP:
                oFDefn.SetType(OFTDateTime);
                break;
            case SWQ_DATE:
                oFDefn.SetType(OFTDate);
                break;
            case SWQ_TIME:
                oFDefn.SetType(OFTTime);
                break;
            case SWQ_GEOMETRY:
                break;

            default:
                CPLAssert(false);
                oFDefn.SetType(OFTString);
                break;
        }
        if (psColDef->target_subtype != OFSTNone)
            oFDefn.SetSubType(psColDef->target_subtype);

        if (psColDef->field_length > 0)
        {
            oFDefn.SetWidth(psColDef->field_length);
        }

        if (psColDef->field_precision >= 0)
        {
            oFDefn.SetPrecision(psColDef->field_precision);
        }

        if (bIsGeometry)
        {
            m_anGeomFieldToSrcGeomField.push_back(iSrcGeomField);
            /* Hack while drivers haven't been updated so that */
            /* poSrcDefn->GetGeomFieldDefn(0)->GetSpatialRef() ==
             * m_poSrcLayer->GetSpatialRef() */
            if (iSrcGeomField == 0 && poSrcDefn->GetGeomFieldCount() == 1 &&
                oGFDefn.GetSpatialRef() == nullptr)
            {
                oGFDefn.SetSpatialRef(m_poSrcLayer->GetSpatialRef());
            }
            int bForceGeomType = FALSE;
            if (psColDef->eGeomType != wkbUnknown)
            {
                oGFDefn.SetType(psColDef->eGeomType);
                bForceGeomType = TRUE;
            }
            if (psColDef->nSRID > 0)
            {
                OGRSpatialReference *poSRS = new OGRSpatialReference();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (poSRS->importFromEPSG(psColDef->nSRID) == OGRERR_NONE)
                {
                    oGFDefn.SetSpatialRef(poSRS);
                }
                poSRS->Release();
            }

            auto poMyGeomFieldDefn =
                std::make_unique<OGRGenSQLGeomFieldDefn>(&oGFDefn);
            poMyGeomFieldDefn->bForceGeomType = bForceGeomType;
            m_poDefn->AddGeomFieldDefn(std::move(poMyGeomFieldDefn));
        }
        else
            m_poDefn->AddFieldDefn(&oFDefn);
    }

    /* -------------------------------------------------------------------- */
    /*      Add implicit geometry field.                                    */
    /* -------------------------------------------------------------------- */
    if (psSelectInfo->query_mode == SWQM_RECORDSET &&
        m_poDefn->GetGeomFieldCount() == 0 &&
        poSrcDefn->GetGeomFieldCount() == 1 && !psSelectInfo->bExcludedGeometry)
    {
        psSelectInfo->column_defs.emplace_back();

        swq_col_def *col_def = &psSelectInfo->column_defs.back();

        memset(col_def, 0, sizeof(swq_col_def));
        const char *pszName = poSrcDefn->GetGeomFieldDefn(0)->GetNameRef();
        if (*pszName != '\0')
            col_def->field_name = CPLStrdup(pszName);
        else
            col_def->field_name =
                CPLStrdup(OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME);
        col_def->field_alias = nullptr;
        col_def->table_index = 0;
        col_def->field_index =
            GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(poSrcDefn, 0);
        col_def->field_type = SWQ_GEOMETRY;
        col_def->target_type = SWQ_GEOMETRY;

        m_anGeomFieldToSrcGeomField.push_back(0);

        m_poDefn->AddGeomFieldDefn(std::make_unique<OGRGenSQLGeomFieldDefn>(
            poSrcDefn->GetGeomFieldDefn(0)));

        /* Hack while drivers haven't been updated so that */
        /* poSrcDefn->GetGeomFieldDefn(0)->GetSpatialRef() ==
         * m_poSrcLayer->GetSpatialRef() */
        if (poSrcDefn->GetGeomFieldDefn(0)->GetSpatialRef() == nullptr)
        {
            m_poDefn->GetGeomFieldDefn(0)->SetSpatialRef(
                m_poSrcLayer->GetSpatialRef());
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Now that we have m_poSrcLayer, we can install a spatial filter  */
    /*      if there is one.                                                */
    /* -------------------------------------------------------------------- */
    if (poSpatFilter)
        OGRGenSQLResultsLayer::SetSpatialFilter(
            0, const_cast<OGRGeometry *>(poSpatFilter));

    OGRGenSQLResultsLayer::ResetReading();

    FindAndSetIgnoredFields();

    if (!m_bForwardWhereToSourceLayer)
        OGRLayer::SetAttributeFilter(m_osInitialWHERE.c_str());
}

/************************************************************************/
/*                       ~OGRGenSQLResultsLayer()                       */
/************************************************************************/

OGRGenSQLResultsLayer::~OGRGenSQLResultsLayer()

{
    if (m_nFeaturesRead > 0 && m_poDefn != nullptr)
    {
        CPLDebug("GenSQL", CPL_FRMT_GIB " features read on layer '%s'.",
                 m_nFeaturesRead, m_poDefn->GetName());
    }

    OGRGenSQLResultsLayer::ClearFilters();

    if (m_poDefn != nullptr)
    {
        m_poDefn->Release();
    }
}

/************************************************************************/
/*                            ClearFilters()                            */
/*                                                                      */
/*      Clear up all filters currently in place on the target layer,    */
/*      and joined layers.  We try not to leave them installed          */
/*      except when actively fetching features.                         */
/************************************************************************/

void OGRGenSQLResultsLayer::ClearFilters()

{
    /* -------------------------------------------------------------------- */
    /*      Clear any filters installed on the target layer.                */
    /* -------------------------------------------------------------------- */
    if (m_poSrcLayer != nullptr)
    {
        m_poSrcLayer->ResetReading();
        m_poSrcLayer->SetAttributeFilter("");
        m_poSrcLayer->SetSpatialFilter(nullptr);
    }

    /* -------------------------------------------------------------------- */
    /*      Clear any attribute filter installed on the joined layers.      */
    /* -------------------------------------------------------------------- */
    swq_select *psSelectInfo = m_pSelectInfo.get();

    if (psSelectInfo != nullptr)
    {
        for (int iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++)
        {
            swq_join_def *psJoinInfo = psSelectInfo->join_defs + iJoin;
            OGRLayer *poJoinLayer =
                m_apoTableLayers[psJoinInfo->secondary_table];

            poJoinLayer->SetAttributeFilter("");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Clear any ignored field lists installed on source layers        */
    /* -------------------------------------------------------------------- */
    if (psSelectInfo != nullptr)
    {
        for (int iTable = 0; iTable < psSelectInfo->table_count; iTable++)
        {
            OGRLayer *poLayer = m_apoTableLayers[iTable];
            poLayer->SetIgnoredFields(nullptr);
        }
    }
}

/************************************************************************/
/*                    MustEvaluateSpatialFilterOnGenSQL()               */
/************************************************************************/

int OGRGenSQLResultsLayer::MustEvaluateSpatialFilterOnGenSQL()
{
    int bEvaluateSpatialFilter = FALSE;
    if (m_poFilterGeom != nullptr && m_iGeomFieldFilter >= 0 &&
        m_iGeomFieldFilter < GetLayerDefn()->GetGeomFieldCount())
    {
        int iSrcGeomField = m_anGeomFieldToSrcGeomField[m_iGeomFieldFilter];
        if (iSrcGeomField < 0)
            bEvaluateSpatialFilter = TRUE;
    }
    return bEvaluateSpatialFilter;
}

/************************************************************************/
/*                       ApplyFiltersToSource()                         */
/************************************************************************/

void OGRGenSQLResultsLayer::ApplyFiltersToSource()
{
    if (m_bForwardWhereToSourceLayer && !m_osInitialWHERE.empty())
    {
        m_poSrcLayer->SetAttributeFilter(m_osInitialWHERE.c_str());
    }
    else
    {
        m_poSrcLayer->SetAttributeFilter(nullptr);
    }
    if (m_iGeomFieldFilter >= 0 &&
        m_iGeomFieldFilter < GetLayerDefn()->GetGeomFieldCount())
    {
        int iSrcGeomField = m_anGeomFieldToSrcGeomField[m_iGeomFieldFilter];
        if (iSrcGeomField >= 0)
            m_poSrcLayer->SetSpatialFilter(iSrcGeomField, m_poFilterGeom);
    }

    m_poSrcLayer->ResetReading();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGenSQLResultsLayer::ResetReading()

{
    swq_select *psSelectInfo = m_pSelectInfo.get();

    if (psSelectInfo->query_mode == SWQM_RECORDSET)
    {
        ApplyFiltersToSource();
    }

    m_nNextIndexFID = psSelectInfo->offset;
    m_nIteratedFeatures = -1;
    m_bEOF = false;
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/*                                                                      */
/*      If we already have an FID list, we can easily reposition        */
/*      ourselves in it.                                                */
/************************************************************************/

OGRErr OGRGenSQLResultsLayer::SetNextByIndex(GIntBig nIndex)

{
    if (nIndex < 0)
        return OGRERR_FAILURE;

    swq_select *psSelectInfo = m_pSelectInfo.get();

    if (psSelectInfo->limit >= 0)
    {
        m_nIteratedFeatures = nIndex;
        if (m_nIteratedFeatures >= psSelectInfo->limit)
        {
            return OGRERR_FAILURE;
        }
    }

    CreateOrderByIndex();

    if (nIndex > std::numeric_limits<GIntBig>::max() - psSelectInfo->offset)
    {
        m_bEOF = true;
        return OGRERR_FAILURE;
    }
    if (psSelectInfo->query_mode == SWQM_SUMMARY_RECORD ||
        psSelectInfo->query_mode == SWQM_DISTINCT_LIST || !m_anFIDIndex.empty())
    {
        m_nNextIndexFID = nIndex + psSelectInfo->offset;
        return OGRERR_NONE;
    }
    else
    {
        OGRErr eErr =
            m_poSrcLayer->SetNextByIndex(nIndex + psSelectInfo->offset);
        if (eErr != OGRERR_NONE)
            m_bEOF = true;
        return eErr;
    }
}

/************************************************************************/
/*                            IGetExtent()                              */
/************************************************************************/

OGRErr OGRGenSQLResultsLayer::IGetExtent(int iGeomField, OGREnvelope *psExtent,
                                         bool bForce)

{
    swq_select *psSelectInfo = m_pSelectInfo.get();

    if (psSelectInfo->query_mode == SWQM_RECORDSET)
    {
        int iSrcGeomField = m_anGeomFieldToSrcGeomField[iGeomField];
        if (iSrcGeomField >= 0)
            return m_poSrcLayer->GetExtent(iSrcGeomField, psExtent, bForce);
        else
            return OGRLayer::IGetExtent(iGeomField, psExtent, bForce);
    }
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRGenSQLResultsLayer::GetFeatureCount(int bForce)

{
    swq_select *psSelectInfo = m_pSelectInfo.get();

    CreateOrderByIndex();

    GIntBig nRet = 0;
    if (psSelectInfo->query_mode == SWQM_DISTINCT_LIST)
    {
        if (!PrepareSummary())
            return 0;

        if (psSelectInfo->column_summary.empty())
            return 0;

        nRet = psSelectInfo->column_summary[0].count;
    }
    else if (psSelectInfo->query_mode != SWQM_RECORDSET)
        return 1;
    else if (m_poAttrQuery == nullptr && !MustEvaluateSpatialFilterOnGenSQL())
    {
        nRet = m_poSrcLayer->GetFeatureCount(bForce);
    }
    else
    {
        nRet = OGRLayer::GetFeatureCount(bForce);
    }
    if (nRet < 0)
        return nRet;

    nRet = std::max(static_cast<GIntBig>(0), nRet - psSelectInfo->offset);
    if (psSelectInfo->limit >= 0)
        nRet = std::min(nRet, psSelectInfo->limit);
    return nRet;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGenSQLResultsLayer::TestCapability(const char *pszCap)

{
    const swq_select *psSelectInfo = m_pSelectInfo.get();

    if (EQUAL(pszCap, OLCFastSetNextByIndex))
    {
        if (psSelectInfo->query_mode == SWQM_SUMMARY_RECORD ||
            psSelectInfo->query_mode == SWQM_DISTINCT_LIST ||
            !m_anFIDIndex.empty())
            return TRUE;
        else
            return m_poSrcLayer->TestCapability(pszCap);
    }

    if (psSelectInfo->query_mode == SWQM_RECORDSET &&
        (EQUAL(pszCap, OLCFastFeatureCount) || EQUAL(pszCap, OLCRandomRead) ||
         EQUAL(pszCap, OLCFastGetExtent)))
        return m_poSrcLayer->TestCapability(pszCap);

    else if (psSelectInfo->query_mode != SWQM_RECORDSET)
    {
        if (EQUAL(pszCap, OLCFastFeatureCount))
            return TRUE;
    }

    if (EQUAL(pszCap, OLCStringsAsUTF8) || EQUAL(pszCap, OLCCurveGeometries) ||
        EQUAL(pszCap, OLCMeasuredGeometries) || EQUAL(pszCap, OLCZGeometries))
    {
        return m_poSrcLayer->TestCapability(pszCap);
    }

    else if (EQUAL(pszCap, OLCFastGetArrowStream))
    {
        // Make sure the SQL is something as simple as
        // "SELECT field1 [AS renamed], ... FROM ... WHERE ....", without
        // duplicated fields
        if (m_bForwardWhereToSourceLayer &&
            psSelectInfo->query_mode == SWQM_RECORDSET &&
            psSelectInfo->offset == 0 && psSelectInfo->join_count == 0 &&
            psSelectInfo->order_specs == 0)
        {
            std::set<int> oSetFieldIndex;
            int nLastIdxRegularField = -1;
            for (std::size_t iField = 0;
                 iField < psSelectInfo->column_defs.size(); iField++)
            {
                const swq_col_def *psColDef =
                    &psSelectInfo->column_defs[iField];
                if (psColDef->bHidden || psColDef->table_index < 0 ||
                    psColDef->col_func != SWQCF_NONE ||
                    cpl::contains(oSetFieldIndex, psColDef->field_index))
                {
                    return false;
                }

                oSetFieldIndex.insert(psColDef->field_index);

                const auto poLayerDefn =
                    m_apoTableLayers[psColDef->table_index]->GetLayerDefn();

                if (psColDef->field_index >= 0 && poLayerDefn != nullptr &&
                    psColDef->field_index < poLayerDefn->GetFieldCount())
                {
                    // We do not support re-ordered fields
                    if (psColDef->field_index <= nLastIdxRegularField)
                        return false;
                    nLastIdxRegularField = psColDef->field_index;
                }
                else if (poLayerDefn != nullptr &&
                         IS_GEOM_FIELD_INDEX(poLayerDefn,
                                             psColDef->field_index))
                {
                    // ok
                }
                else
                {
                    return false;
                }
            }
            return m_poSrcLayer->TestCapability(pszCap);
        }
    }

    return FALSE;
}

/************************************************************************/
/*                        ContainGeomSpecialField()                     */
/************************************************************************/

int OGRGenSQLResultsLayer::ContainGeomSpecialField(swq_expr_node *expr)
{
    if (expr->eNodeType == SNT_COLUMN)
    {
        if (expr->table_index == 0 && expr->field_index != -1)
        {
            OGRLayer *poLayer = m_apoTableLayers[expr->table_index];
            int nSpecialFieldIdx =
                expr->field_index - poLayer->GetLayerDefn()->GetFieldCount();
            if (nSpecialFieldIdx == SPF_OGR_GEOMETRY ||
                nSpecialFieldIdx == SPF_OGR_GEOM_WKT ||
                nSpecialFieldIdx == SPF_OGR_GEOM_AREA)
                return TRUE;
            if (expr->field_index ==
                GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(poLayer->GetLayerDefn(), 0))
                return TRUE;
            return FALSE;
        }
    }
    else if (expr->eNodeType == SNT_OPERATION)
    {
        for (int i = 0; i < expr->nSubExprCount; i++)
        {
            if (ContainGeomSpecialField(expr->papoSubExpr[i]))
                return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                           PrepareSummary()                           */
/************************************************************************/

bool OGRGenSQLResultsLayer::PrepareSummary()

{
    swq_select *psSelectInfo = m_pSelectInfo.get();

    if (m_poSummaryFeature)
        return true;

    m_poSummaryFeature = std::make_unique<OGRFeature>(m_poDefn);
    m_poSummaryFeature->SetFID(0);

    /* -------------------------------------------------------------------- */
    /*      Ensure our query parameters are in place on the source          */
    /*      layer.  And initialize reading.                                 */
    /* -------------------------------------------------------------------- */
    ApplyFiltersToSource();

    /* -------------------------------------------------------------------- */
    /*      Ignore geometry reading if no spatial filter in place and that  */
    /*      the where clause and no column references OGR_GEOMETRY,         */
    /*      OGR_GEOM_WKT or OGR_GEOM_AREA special fields.                   */
    /* -------------------------------------------------------------------- */

    struct TempGeomIgnoredSetter
    {
        OGRFeatureDefn &m_oDefn;
        const int m_bSaveIsGeomIgnored;

        explicit TempGeomIgnoredSetter(OGRFeatureDefn *poDefn)
            : m_oDefn(*poDefn),
              m_bSaveIsGeomIgnored(poDefn->IsGeometryIgnored())
        {
            m_oDefn.SetGeometryIgnored(true);
        }

        ~TempGeomIgnoredSetter()
        {
            m_oDefn.SetGeometryIgnored(m_bSaveIsGeomIgnored);
        }
    };

    auto poSrcLayerDefn = m_poSrcLayer->GetLayerDefn();
    std::unique_ptr<TempGeomIgnoredSetter> oTempGeomIgnoredSetter;

    if (m_poFilterGeom == nullptr &&
        (psSelectInfo->where_expr == nullptr ||
         !ContainGeomSpecialField(psSelectInfo->where_expr)))
    {
        bool bFoundGeomExpr = false;
        for (int iField = 0; iField < psSelectInfo->result_columns(); iField++)
        {
            const swq_col_def *psColDef = &psSelectInfo->column_defs[iField];
            if (psColDef->table_index == 0 && psColDef->field_index != -1)
            {
                OGRLayer *poLayer = m_apoTableLayers[psColDef->table_index];
                const int nSpecialFieldIdx =
                    psColDef->field_index -
                    poLayer->GetLayerDefn()->GetFieldCount();
                if (nSpecialFieldIdx == SPF_OGR_GEOMETRY ||
                    nSpecialFieldIdx == SPF_OGR_GEOM_WKT ||
                    nSpecialFieldIdx == SPF_OGR_GEOM_AREA)
                {
                    bFoundGeomExpr = true;
                    break;
                }
                if (psColDef->field_index ==
                    GEOM_FIELD_INDEX_TO_ALL_FIELD_INDEX(poLayer->GetLayerDefn(),
                                                        0))
                {
                    bFoundGeomExpr = true;
                    break;
                }
            }
            if (psColDef->expr != nullptr &&
                ContainGeomSpecialField(psColDef->expr))
            {
                bFoundGeomExpr = true;
                break;
            }
        }
        if (!bFoundGeomExpr)
        {
            // cppcheck-suppress unreadVariable
            oTempGeomIgnoredSetter =
                std::make_unique<TempGeomIgnoredSetter>(poSrcLayerDefn);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      We treat COUNT(*) as a special case, and fill with              */
    /*      GetFeatureCount().                                              */
    /* -------------------------------------------------------------------- */

    if (psSelectInfo->result_columns() == 1 &&
        psSelectInfo->column_defs[0].col_func == SWQCF_COUNT &&
        psSelectInfo->column_defs[0].field_index < 0)
    {
        GIntBig nRes = m_poSrcLayer->GetFeatureCount(TRUE);
        m_poSummaryFeature->SetField(0, nRes);

        if (CPL_INT64_FITS_ON_INT32(nRes))
        {
            m_poSummaryFeature.reset();
            m_poDefn->GetFieldDefn(0)->SetType(OFTInteger);
            m_poSummaryFeature = std::make_unique<OGRFeature>(m_poDefn);
            m_poSummaryFeature->SetFID(0);
            m_poSummaryFeature->SetField(0, static_cast<int>(nRes));
        }

        return TRUE;
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise, process all source feature through the summary       */
    /*      building facilities of SWQ.                                     */
    /* -------------------------------------------------------------------- */

    for (auto &&poSrcFeature : *m_poSrcLayer)
    {
        for (int iField = 0; iField < psSelectInfo->result_columns(); iField++)
        {
            const swq_col_def *psColDef = &psSelectInfo->column_defs[iField];
            const char *pszError = nullptr;

            if (psColDef->col_func == SWQCF_COUNT)
            {
                /* psColDef->field_index can be -1 in the case of a COUNT(*) */
                if (psColDef->field_index < 0)
                    pszError =
                        swq_select_summarize(psSelectInfo, iField, "", nullptr);
                else if (IS_GEOM_FIELD_INDEX(poSrcLayerDefn,
                                             psColDef->field_index))
                {
                    const int iSrcGeomField =
                        ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(
                            poSrcLayerDefn, psColDef->field_index);
                    const OGRGeometry *poGeom =
                        poSrcFeature->GetGeomFieldRef(iSrcGeomField);
                    if (poGeom != nullptr)
                        pszError = swq_select_summarize(psSelectInfo, iField,
                                                        "", nullptr);
                }
                else if (poSrcFeature->IsFieldSetAndNotNull(
                             psColDef->field_index))
                {
                    if (!psColDef->distinct_flag)
                    {
                        pszError = swq_select_summarize(psSelectInfo, iField,
                                                        "", nullptr);
                    }
                    else
                    {
                        const char *pszVal = poSrcFeature->GetFieldAsString(
                            psColDef->field_index);
                        pszError = swq_select_summarize(psSelectInfo, iField,
                                                        pszVal, nullptr);
                    }
                }
            }
            else
            {
                if (poSrcFeature->IsFieldSetAndNotNull(psColDef->field_index))
                {
                    if (!psColDef->distinct_flag &&
                        (psColDef->field_type == SWQ_BOOLEAN ||
                         psColDef->field_type == SWQ_INTEGER ||
                         psColDef->field_type == SWQ_INTEGER64 ||
                         psColDef->field_type == SWQ_FLOAT))
                    {
                        const double dfValue = poSrcFeature->GetFieldAsDouble(
                            psColDef->field_index);
                        pszError = swq_select_summarize(psSelectInfo, iField,
                                                        nullptr, &dfValue);
                    }
                    else
                    {
                        const char *pszVal = poSrcFeature->GetFieldAsString(
                            psColDef->field_index);
                        pszError = swq_select_summarize(psSelectInfo, iField,
                                                        pszVal, nullptr);
                    }
                }
                else
                {
                    pszError = swq_select_summarize(psSelectInfo, iField,
                                                    nullptr, nullptr);
                }
            }

            if (pszError)
            {
                m_poSummaryFeature.reset();

                CPLError(CE_Failure, CPLE_AppDefined, "%s", pszError);
                return false;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Clear away the filters we have installed till a next run through*/
    /*      the features.                                                   */
    /* -------------------------------------------------------------------- */
    ClearFilters();

    /* -------------------------------------------------------------------- */
    /*      Now apply the values to the summary feature.  If we are in      */
    /*      DISTINCT_LIST mode we don't do this step.                       */
    /* -------------------------------------------------------------------- */
    if (psSelectInfo->query_mode == SWQM_SUMMARY_RECORD)
    {
        for (int iField = 0; iField < psSelectInfo->result_columns(); iField++)
        {
            const swq_col_def *psColDef = &psSelectInfo->column_defs[iField];
            if (!psSelectInfo->column_summary.empty())
            {
                const swq_summary &oSummary =
                    psSelectInfo->column_summary[iField];
                if (psColDef->col_func == SWQCF_COUNT)
                {
                    if (CPL_INT64_FITS_ON_INT32(oSummary.count))
                    {
                        m_poSummaryFeature.reset();
                        m_poDefn->GetFieldDefn(iField)->SetType(OFTInteger);
                    }
                }
            }
        }

        if (!m_poSummaryFeature)
        {
            m_poSummaryFeature = std::make_unique<OGRFeature>(m_poDefn);
            m_poSummaryFeature->SetFID(0);
        }

        for (int iField = 0; iField < psSelectInfo->result_columns(); iField++)
        {
            const swq_col_def *psColDef = &psSelectInfo->column_defs[iField];
            if (!psSelectInfo->column_summary.empty())
            {
                const swq_summary &oSummary =
                    psSelectInfo->column_summary[iField];

                switch (psColDef->col_func)
                {
                    case SWQCF_NONE:
                    case SWQCF_CUSTOM:
                        break;

                    case SWQCF_AVG:
                    {
                        if (oSummary.count > 0)
                        {
                            const double dfAvg =
                                oSummary.sum() / oSummary.count;
                            if (psColDef->field_type == SWQ_DATE ||
                                psColDef->field_type == SWQ_TIME ||
                                psColDef->field_type == SWQ_TIMESTAMP)
                            {
                                struct tm brokendowntime;
                                CPLUnixTimeToYMDHMS(static_cast<GIntBig>(dfAvg),
                                                    &brokendowntime);
                                m_poSummaryFeature->SetField(
                                    iField, brokendowntime.tm_year + 1900,
                                    brokendowntime.tm_mon + 1,
                                    brokendowntime.tm_mday,
                                    brokendowntime.tm_hour,
                                    brokendowntime.tm_min,
                                    static_cast<float>(brokendowntime.tm_sec +
                                                       fmod(dfAvg, 1)),
                                    0);
                            }
                            else
                            {
                                m_poSummaryFeature->SetField(iField, dfAvg);
                            }
                        }
                        break;
                    }

                    case SWQCF_MIN:
                    {
                        if (oSummary.count > 0)
                        {
                            if (psColDef->field_type == SWQ_DATE ||
                                psColDef->field_type == SWQ_TIME ||
                                psColDef->field_type == SWQ_TIMESTAMP ||
                                psColDef->field_type == SWQ_STRING)
                                m_poSummaryFeature->SetField(
                                    iField, oSummary.osMin.c_str());
                            else
                                m_poSummaryFeature->SetField(iField,
                                                             oSummary.min);
                        }
                        break;
                    }

                    case SWQCF_MAX:
                    {
                        if (oSummary.count > 0)
                        {
                            if (psColDef->field_type == SWQ_DATE ||
                                psColDef->field_type == SWQ_TIME ||
                                psColDef->field_type == SWQ_TIMESTAMP ||
                                psColDef->field_type == SWQ_STRING)
                                m_poSummaryFeature->SetField(
                                    iField, oSummary.osMax.c_str());
                            else
                                m_poSummaryFeature->SetField(iField,
                                                             oSummary.max);
                        }
                        break;
                    }

                    case SWQCF_COUNT:
                    {
                        m_poSummaryFeature->SetField(iField, oSummary.count);
                        break;
                    }

                    case SWQCF_SUM:
                    {
                        if (oSummary.count > 0)
                            m_poSummaryFeature->SetField(iField,
                                                         oSummary.sum());
                        break;
                    }

                    case SWQCF_STDDEV_POP:
                    {
                        if (oSummary.count > 0)
                        {
                            const double dfVariance =
                                oSummary.sq_dist_from_mean_acc / oSummary.count;
                            m_poSummaryFeature->SetField(iField,
                                                         sqrt(dfVariance));
                        }
                        break;
                    }

                    case SWQCF_STDDEV_SAMP:
                    {
                        if (oSummary.count > 1)
                        {
                            const double dfSampleVariance =
                                oSummary.sq_dist_from_mean_acc /
                                (oSummary.count - 1);
                            m_poSummaryFeature->SetField(
                                iField, sqrt(dfSampleVariance));
                        }
                        break;
                    }
                }
            }
            else if (psColDef->col_func == SWQCF_COUNT)
                m_poSummaryFeature->SetField(iField, 0);
        }
    }

    return TRUE;
}

/************************************************************************/
/*                       OGRMultiFeatureFetcher()                       */
/************************************************************************/

typedef std::vector<std::unique_ptr<OGRFeature>> VectorOfUniquePtrFeature;

static swq_expr_node *OGRMultiFeatureFetcher(swq_expr_node *op,
                                             void *pFeatureList)

{
    auto &apoFeatures =
        *(static_cast<VectorOfUniquePtrFeature *>(pFeatureList));
    swq_expr_node *poRetNode = nullptr;

    CPLAssert(op->eNodeType == SNT_COLUMN);

    /* -------------------------------------------------------------------- */
    /*      What feature are we using?  The primary or one of the joined ones?*/
    /* -------------------------------------------------------------------- */
    if (op->table_index < 0 ||
        op->table_index >= static_cast<int>(apoFeatures.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Request for unexpected table_index (%d) in field fetcher.",
                 op->table_index);
        return nullptr;
    }

    OGRFeature *poFeature = apoFeatures[op->table_index].get();

    /* -------------------------------------------------------------------- */
    /*      Fetch the value.                                                */
    /* -------------------------------------------------------------------- */
    switch (op->field_type)
    {
        case SWQ_INTEGER:
        case SWQ_BOOLEAN:
            if (poFeature == nullptr ||
                !poFeature->IsFieldSetAndNotNull(op->field_index))
            {
                poRetNode = new swq_expr_node(0);
                poRetNode->is_null = TRUE;
            }
            else
                poRetNode = new swq_expr_node(
                    poFeature->GetFieldAsInteger(op->field_index));
            break;

        case SWQ_INTEGER64:
            if (poFeature == nullptr ||
                !poFeature->IsFieldSetAndNotNull(op->field_index))
            {
                poRetNode = new swq_expr_node(static_cast<GIntBig>(0));
                poRetNode->is_null = TRUE;
            }
            else
                poRetNode = new swq_expr_node(
                    poFeature->GetFieldAsInteger64(op->field_index));
            break;

        case SWQ_FLOAT:
            if (poFeature == nullptr ||
                !poFeature->IsFieldSetAndNotNull(op->field_index))
            {
                poRetNode = new swq_expr_node(0.0);
                poRetNode->is_null = TRUE;
            }
            else
                poRetNode = new swq_expr_node(
                    poFeature->GetFieldAsDouble(op->field_index));
            break;

        case SWQ_GEOMETRY:
            if (poFeature == nullptr)
            {
                poRetNode =
                    new swq_expr_node(static_cast<OGRGeometry *>(nullptr));
            }
            else
            {
                int iSrcGeomField = ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(
                    poFeature->GetDefnRef(), op->field_index);
                poRetNode = new swq_expr_node(
                    poFeature->GetGeomFieldRef(iSrcGeomField));
            }
            break;

        default:
            if (poFeature == nullptr ||
                !poFeature->IsFieldSetAndNotNull(op->field_index))
            {
                poRetNode = new swq_expr_node("");
                poRetNode->is_null = TRUE;
            }
            else
                poRetNode = new swq_expr_node(
                    poFeature->GetFieldAsString(op->field_index));
            break;
    }

    return poRetNode;
}

/************************************************************************/
/*                          GetFilterForJoin()                          */
/************************************************************************/

static CPLString GetFilterForJoin(swq_expr_node *poExpr, OGRFeature *poSrcFeat,
                                  OGRLayer *poJoinLayer, int secondary_table)
{
    if (poExpr->eNodeType == SNT_CONSTANT)
    {
        char *pszRes = poExpr->Unparse(nullptr, '"');
        CPLString osRes = pszRes;
        CPLFree(pszRes);
        return osRes;
    }

    if (poExpr->eNodeType == SNT_COLUMN)
    {
        CPLAssert(poExpr->field_index != -1);
        CPLAssert(poExpr->table_index == 0 ||
                  poExpr->table_index == secondary_table);

        if (poExpr->table_index == 0)
        {
            // if source key is null, we can't do join.
            if (!poSrcFeat->IsFieldSetAndNotNull(poExpr->field_index))
            {
                return "";
            }
            const auto poSrcFDefn = poSrcFeat->GetDefnRef();
            if (poExpr->field_index >= poSrcFDefn->GetFieldCount())
            {
                CPLAssert(poExpr->field_index <
                          poSrcFDefn->GetFieldCount() + SPECIAL_FIELD_COUNT);
                switch (SpecialFieldTypes[poExpr->field_index -
                                          poSrcFDefn->GetFieldCount()])
                {
                    case SWQ_INTEGER:
                    case SWQ_INTEGER64:
                        return CPLString().Printf(
                            CPL_FRMT_GIB, poSrcFeat->GetFieldAsInteger64(
                                              poExpr->field_index));
                        break;
                    case SWQ_FLOAT:
                        return CPLString().Printf(
                            "%.17g",
                            poSrcFeat->GetFieldAsDouble(poExpr->field_index));
                        break;
                    default:
                    {
                        char *pszEscaped = CPLEscapeString(
                            poSrcFeat->GetFieldAsString(poExpr->field_index),
                            -1, CPLES_SQL);
                        CPLString osRes = "'";
                        osRes += pszEscaped;
                        osRes += "'";
                        CPLFree(pszEscaped);
                        return osRes;
                    }
                }
            }
            else
            {
                const OGRFieldType ePrimaryFieldType =
                    poSrcFeat->GetFieldDefnRef(poExpr->field_index)->GetType();
                const OGRField *psSrcField =
                    poSrcFeat->GetRawFieldRef(poExpr->field_index);

                CPLString osRet;
                switch (ePrimaryFieldType)
                {
                    case OFTInteger:
                        osRet.Printf("%d", psSrcField->Integer);
                        break;

                    case OFTInteger64:
                        osRet.Printf(CPL_FRMT_GIB, psSrcField->Integer64);
                        break;

                    case OFTReal:
                        osRet.Printf("%.17g", psSrcField->Real);
                        break;

                    case OFTString:
                    {
                        char *pszEscaped = CPLEscapeString(
                            psSrcField->String,
                            static_cast<int>(strlen(psSrcField->String)),
                            CPLES_SQL);
                        osRet = "'";
                        osRet += pszEscaped;
                        osRet += "'";
                        CPLFree(pszEscaped);
                        break;
                    }

                    default:
                        CPLAssert(false);
                        break;
                }

                return osRet;
            }
        }

        if (poExpr->table_index == secondary_table)
        {
            const auto poJoinFDefn = poJoinLayer->GetLayerDefn();
            if (poExpr->field_index >= poJoinFDefn->GetFieldCount())
            {
                CPLAssert(poExpr->field_index <
                          poJoinFDefn->GetFieldCount() + SPECIAL_FIELD_COUNT);
                return SpecialFieldNames[poExpr->field_index -
                                         poJoinFDefn->GetFieldCount()];
            }
            else
            {
                const OGRFieldDefn *poSecondaryFieldDefn =
                    poJoinFDefn->GetFieldDefn(poExpr->field_index);
                return CPLSPrintf("\"%s\"", poSecondaryFieldDefn->GetNameRef());
            }
        }

        CPLAssert(false);
        return "";
    }

    if (poExpr->eNodeType == SNT_OPERATION)
    {
        /* ----------------------------------------------------------------- */
        /*      Operation - start by unparsing all the subexpressions.       */
        /* ----------------------------------------------------------------- */
        std::vector<char *> apszSubExpr;
        for (int i = 0; i < poExpr->nSubExprCount; i++)
        {
            CPLString osSubExpr =
                GetFilterForJoin(poExpr->papoSubExpr[i], poSrcFeat, poJoinLayer,
                                 secondary_table);
            if (osSubExpr.empty())
            {
                for (--i; i >= 0; i--)
                    CPLFree(apszSubExpr[i]);
                return "";
            }
            apszSubExpr.push_back(CPLStrdup(osSubExpr));
        }

        CPLString osExpr =
            poExpr->UnparseOperationFromUnparsedSubExpr(&apszSubExpr[0]);

        /* ----------------------------------------------------------------- */
        /*      cleanup subexpressions.                                      */
        /* ----------------------------------------------------------------- */
        for (int i = 0; i < poExpr->nSubExprCount; i++)
            CPLFree(apszSubExpr[i]);

        return osExpr;
    }

    return "";
}

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature> OGRGenSQLResultsLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeatUniquePtr)

{
    swq_select *psSelectInfo = m_pSelectInfo.get();
    VectorOfUniquePtrFeature apoFeatures;

    if (poSrcFeatUniquePtr == nullptr)
        return nullptr;

    m_nFeaturesRead++;

    apoFeatures.push_back(std::move(poSrcFeatUniquePtr));
    auto poSrcFeat = apoFeatures.front().get();

    /* -------------------------------------------------------------------- */
    /*      Fetch the corresponding features from any jointed tables.       */
    /* -------------------------------------------------------------------- */
    for (int iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++)
    {
        const swq_join_def *psJoinInfo = psSelectInfo->join_defs + iJoin;

        /* OGRMultiFeatureFetcher assumes that the features are pushed in */
        /* apoFeatures with increasing secondary_table, so make sure */
        /* we have taken care of this */
        CPLAssert(psJoinInfo->secondary_table == iJoin + 1);

        OGRLayer *poJoinLayer = m_apoTableLayers[psJoinInfo->secondary_table];

        const std::string osFilter =
            GetFilterForJoin(psJoinInfo->poExpr, poSrcFeat, poJoinLayer,
                             psJoinInfo->secondary_table);
        // CPLDebug("OGR", "Filter = %s\n", osFilter.c_str());

        // if source key is null, we can't do join.
        if (osFilter.empty())
        {
            apoFeatures.push_back(nullptr);
            continue;
        }

        std::unique_ptr<OGRFeature> poJoinFeature;

        poJoinLayer->ResetReading();
        if (poJoinLayer->SetAttributeFilter(osFilter.c_str()) == OGRERR_NONE)
            poJoinFeature.reset(poJoinLayer->GetNextFeature());

        apoFeatures.push_back(std::move(poJoinFeature));
    }

    /* -------------------------------------------------------------------- */
    /*      Create destination feature.                                     */
    /* -------------------------------------------------------------------- */
    auto poDstFeat = std::make_unique<OGRFeature>(m_poDefn);

    poDstFeat->SetFID(poSrcFeat->GetFID());

    poDstFeat->SetStyleString(poSrcFeat->GetStyleString());
    poDstFeat->SetNativeData(poSrcFeat->GetNativeData());
    poDstFeat->SetNativeMediaType(poSrcFeat->GetNativeMediaType());

    /* -------------------------------------------------------------------- */
    /*      Evaluate fields that are complex expressions.                   */
    /* -------------------------------------------------------------------- */
    int iRegularField = 0;
    int iGeomField = 0;
    swq_evaluation_context sContext;
    for (int iField = 0; iField < psSelectInfo->result_columns(); iField++)
    {
        const swq_col_def *psColDef = &psSelectInfo->column_defs[iField];

        if (psColDef->bHidden)
        {
            const char *pszDstFieldName = psColDef->field_alias
                                              ? psColDef->field_alias
                                              : psColDef->field_name;
            if (EQUAL(pszDstFieldName, "OGR_STYLE"))
            {
                if (psColDef->field_type == SWQ_STRING)
                {
                    // Does this column definition directly references a
                    // source field ?
                    if (psColDef->field_index >= 0)
                    {
                        if (!IS_GEOM_FIELD_INDEX(poSrcFeat->GetDefnRef(),
                                                 psColDef->field_index))
                        {
                            if (poSrcFeat->IsFieldSetAndNotNull(
                                    psColDef->field_index))
                            {
                                const char *pszVal =
                                    poSrcFeat->GetFieldAsString(
                                        psColDef->field_index);
                                poDstFeat->SetStyleString(pszVal);
                            }
                            else
                            {
                                poDstFeat->SetStyleString(nullptr);
                            }
                        }
                        else
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "OGR_STYLE HIDDEN field should reference "
                                     "a column of type String");
                        }
                    }
                    else
                    {
                        auto poResult = std::unique_ptr<swq_expr_node>(
                            psColDef->expr->Evaluate(OGRMultiFeatureFetcher,
                                                     &apoFeatures, sContext));

                        if (!poResult)
                        {
                            return nullptr;
                        }

                        poDstFeat->SetStyleString(poResult->is_null
                                                      ? nullptr
                                                      : poResult->string_value);
                    }
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "OGR_STYLE HIDDEN field should be of type String");
                }
            }
            continue;
        }

        // Does this column definition directly references a
        // source field ?
        // If so, skip it for now, as it will be taken into account in the
        // next loop.
        if (psColDef->field_index >= 0)
        {
            if (psColDef->field_type == SWQ_GEOMETRY ||
                psColDef->target_type == SWQ_GEOMETRY)
                iGeomField++;
            else
                iRegularField++;
            continue;
        }

        auto poResult = std::unique_ptr<swq_expr_node>(psColDef->expr->Evaluate(
            OGRMultiFeatureFetcher, &apoFeatures, sContext));

        if (!poResult)
        {
            return nullptr;
        }

        if (poResult->is_null)
        {
            if (poResult->field_type == SWQ_GEOMETRY)
                iGeomField++;
            else
                iRegularField++;
            continue;
        }

        switch (poResult->field_type)
        {
            case SWQ_BOOLEAN:
            case SWQ_INTEGER:
            case SWQ_INTEGER64:
                poDstFeat->SetField(iRegularField++,
                                    static_cast<GIntBig>(poResult->int_value));
                break;

            case SWQ_FLOAT:
                poDstFeat->SetField(iRegularField++, poResult->float_value);
                break;

            case SWQ_GEOMETRY:
            {
                OGRGenSQLGeomFieldDefn *poGeomFieldDefn =
                    cpl::down_cast<OGRGenSQLGeomFieldDefn *>(
                        poDstFeat->GetGeomFieldDefnRef(iGeomField));
                if (poGeomFieldDefn->bForceGeomType &&
                    poResult->geometry_value != nullptr)
                {
                    OGRwkbGeometryType eCurType =
                        wkbFlatten(poResult->geometry_value->getGeometryType());
                    OGRwkbGeometryType eReqType =
                        wkbFlatten(poGeomFieldDefn->GetType());
                    if (eCurType == wkbPolygon && eReqType == wkbMultiPolygon)
                    {
                        poResult->geometry_value = OGRGeometry::FromHandle(
                            OGR_G_ForceToMultiPolygon(OGRGeometry::ToHandle(
                                poResult->geometry_value)));
                    }
                    else if ((eCurType == wkbMultiPolygon ||
                              eCurType == wkbGeometryCollection) &&
                             eReqType == wkbPolygon)
                    {
                        poResult->geometry_value = OGRGeometry::FromHandle(
                            OGR_G_ForceToPolygon(OGRGeometry::ToHandle(
                                poResult->geometry_value)));
                    }
                    else if (eCurType == wkbLineString &&
                             eReqType == wkbMultiLineString)
                    {
                        poResult->geometry_value = OGRGeometry::FromHandle(
                            OGR_G_ForceToMultiLineString(OGRGeometry::ToHandle(
                                poResult->geometry_value)));
                    }
                    else if ((eCurType == wkbMultiLineString ||
                              eCurType == wkbGeometryCollection) &&
                             eReqType == wkbLineString)
                    {
                        poResult->geometry_value = OGRGeometry::FromHandle(
                            OGR_G_ForceToLineString(OGRGeometry::ToHandle(
                                poResult->geometry_value)));
                    }
                }
                poDstFeat->SetGeomField(iGeomField++, poResult->geometry_value);
                break;
            }

            default:
                poDstFeat->SetField(iRegularField++, poResult->string_value);
                break;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Copy fields from primary record to the destination feature.     */
    /* -------------------------------------------------------------------- */
    iRegularField = 0;
    iGeomField = 0;
    for (int iField = 0; iField < psSelectInfo->result_columns(); iField++)
    {
        const swq_col_def *psColDef = &psSelectInfo->column_defs[iField];

        if (psColDef->bHidden)
        {
            continue;
        }

        // Skip this column definition if it doesn't reference a field from
        // the main feature
        if (psColDef->table_index != 0)
        {
            if (psColDef->field_type == SWQ_GEOMETRY ||
                psColDef->target_type == SWQ_GEOMETRY)
                iGeomField++;
            else
                iRegularField++;
            continue;
        }

        if (IS_GEOM_FIELD_INDEX(poSrcFeat->GetDefnRef(), psColDef->field_index))
        {
            int iSrcGeomField = ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(
                poSrcFeat->GetDefnRef(), psColDef->field_index);
            poDstFeat->SetGeomField(iGeomField++,
                                    poSrcFeat->GetGeomFieldRef(iSrcGeomField));
        }
        else if (psColDef->field_index >= m_iFIDFieldIndex)
        {
            CPLAssert(psColDef->field_index <
                      m_iFIDFieldIndex + SPECIAL_FIELD_COUNT);
            switch (SpecialFieldTypes[psColDef->field_index - m_iFIDFieldIndex])
            {
                case SWQ_INTEGER:
                case SWQ_INTEGER64:
                    poDstFeat->SetField(
                        iRegularField,
                        poSrcFeat->GetFieldAsInteger64(psColDef->field_index));
                    break;
                case SWQ_FLOAT:
                    poDstFeat->SetField(
                        iRegularField,
                        poSrcFeat->GetFieldAsDouble(psColDef->field_index));
                    break;
                default:
                    poDstFeat->SetField(
                        iRegularField,
                        poSrcFeat->GetFieldAsString(psColDef->field_index));
            }
            iRegularField++;
        }
        else
        {
            switch (psColDef->target_type)
            {
                case SWQ_INTEGER:
                    poDstFeat->SetField(
                        iRegularField,
                        poSrcFeat->GetFieldAsInteger(psColDef->field_index));
                    break;

                case SWQ_INTEGER64:
                    poDstFeat->SetField(
                        iRegularField,
                        poSrcFeat->GetFieldAsInteger64(psColDef->field_index));
                    break;

                case SWQ_FLOAT:
                    poDstFeat->SetField(
                        iRegularField,
                        poSrcFeat->GetFieldAsDouble(psColDef->field_index));
                    break;

                case SWQ_STRING:
                case SWQ_TIMESTAMP:
                case SWQ_DATE:
                case SWQ_TIME:
                    poDstFeat->SetField(
                        iRegularField,
                        poSrcFeat->GetFieldAsString(psColDef->field_index));
                    break;

                case SWQ_GEOMETRY:
                    CPLAssert(false);
                    break;

                default:
                    poDstFeat->SetField(
                        iRegularField,
                        poSrcFeat->GetRawFieldRef(psColDef->field_index));
            }
            iRegularField++;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Copy values from any joined tables.                             */
    /* -------------------------------------------------------------------- */
    for (int iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++)
    {
        const swq_join_def *psJoinInfo = psSelectInfo->join_defs + iJoin;
        const OGRFeature *poJoinFeature = apoFeatures[iJoin + 1].get();

        if (poJoinFeature == nullptr)
            continue;

        // Copy over selected field values.
        iRegularField = 0;
        for (int iField = 0; iField < psSelectInfo->result_columns(); iField++)
        {
            const swq_col_def *psColDef = &psSelectInfo->column_defs[iField];

            if (psColDef->field_type == SWQ_GEOMETRY ||
                psColDef->target_type == SWQ_GEOMETRY)
                continue;

            if (psColDef->bHidden)
            {
                continue;
            }

            if (psColDef->table_index == psJoinInfo->secondary_table)
                poDstFeat->SetField(
                    iRegularField,
                    poJoinFeature->GetRawFieldRef(psColDef->field_index));

            iRegularField++;
        }
    }

    return poDstFeat;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGenSQLResultsLayer::GetNextFeature()

{
    swq_select *psSelectInfo = m_pSelectInfo.get();

    if (m_bEOF)
        return nullptr;
    if (psSelectInfo->limit >= 0 &&
        (m_nIteratedFeatures < 0 ? 0 : m_nIteratedFeatures) >=
            psSelectInfo->limit)
        return nullptr;

    CreateOrderByIndex();
    if (m_anFIDIndex.empty() && m_nIteratedFeatures < 0 &&
        psSelectInfo->offset > 0 && psSelectInfo->query_mode == SWQM_RECORDSET)
    {
        m_poSrcLayer->SetNextByIndex(psSelectInfo->offset);
    }
    if (m_nIteratedFeatures < 0)
        m_nIteratedFeatures = 0;

    /* -------------------------------------------------------------------- */
    /*      Handle summary sets.                                            */
    /* -------------------------------------------------------------------- */
    if (psSelectInfo->query_mode == SWQM_SUMMARY_RECORD ||
        psSelectInfo->query_mode == SWQM_DISTINCT_LIST)
    {
        m_nIteratedFeatures++;
        return GetFeature(m_nNextIndexFID++);
    }

    int bEvaluateSpatialFilter = MustEvaluateSpatialFilterOnGenSQL();

    /* -------------------------------------------------------------------- */
    /*      Handle ordered sets.                                            */
    /* -------------------------------------------------------------------- */
    while (true)
    {
        std::unique_ptr<OGRFeature> poSrcFeat;
        if (!m_anFIDIndex.empty())
        {
            /* --------------------------------------------------------------------
             */
            /*      Are we running in sorted mode?  If so, run the fid through
             */
            /*      the index. */
            /* --------------------------------------------------------------------
             */

            if (m_nNextIndexFID >= static_cast<GIntBig>(m_anFIDIndex.size()))
                return nullptr;

            poSrcFeat.reset(m_poSrcLayer->GetFeature(
                m_anFIDIndex[static_cast<size_t>(m_nNextIndexFID)]));
            m_nNextIndexFID++;
        }
        else
        {
            poSrcFeat.reset(m_poSrcLayer->GetNextFeature());
        }

        if (poSrcFeat == nullptr)
            return nullptr;

        auto poFeature = TranslateFeature(std::move(poSrcFeat));
        if (poFeature == nullptr)
            return nullptr;

        if ((m_poAttrQuery == nullptr ||
             m_poAttrQuery->Evaluate(poFeature.get())) &&
            (!bEvaluateSpatialFilter ||
             FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter))))
        {
            m_nIteratedFeatures++;
            return poFeature.release();
        }
    }

    return nullptr;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRGenSQLResultsLayer::GetFeature(GIntBig nFID)

{
    swq_select *psSelectInfo = m_pSelectInfo.get();

    CreateOrderByIndex();

    /* -------------------------------------------------------------------- */
    /*      Handle request for summary record.                              */
    /* -------------------------------------------------------------------- */
    if (psSelectInfo->query_mode == SWQM_SUMMARY_RECORD)
    {
        if (!PrepareSummary() || nFID != 0 || !m_poSummaryFeature)
            return nullptr;
        else
            return m_poSummaryFeature->Clone();
    }

    /* -------------------------------------------------------------------- */
    /*      Handle request for distinct list record.                        */
    /* -------------------------------------------------------------------- */
    if (psSelectInfo->query_mode == SWQM_DISTINCT_LIST)
    {
        if (!PrepareSummary())
            return nullptr;

        if (psSelectInfo->column_summary.empty())
            return nullptr;

        swq_summary &oSummary = psSelectInfo->column_summary[0];
        if (psSelectInfo->order_specs == 0)
        {
            if (nFID < 0 || nFID >= static_cast<GIntBig>(
                                        oSummary.oVectorDistinctValues.size()))
            {
                return nullptr;
            }
            const size_t nIdx = static_cast<size_t>(nFID);
            if (oSummary.oVectorDistinctValues[nIdx] != SZ_OGR_NULL)
            {
                m_poSummaryFeature->SetField(
                    0, oSummary.oVectorDistinctValues[nIdx].c_str());
            }
            else
                m_poSummaryFeature->SetFieldNull(0);
        }
        else
        {
            if (m_aosDistinctList.empty())
            {
                std::set<CPLString, swq_summary::Comparator>::const_iterator
                    oIter = oSummary.oSetDistinctValues.begin();
                std::set<CPLString, swq_summary::Comparator>::const_iterator
                    oEnd = oSummary.oSetDistinctValues.end();
                try
                {
                    m_aosDistinctList.reserve(
                        oSummary.oSetDistinctValues.size());
                    for (; oIter != oEnd; ++oIter)
                    {
                        m_aosDistinctList.push_back(*oIter);
                    }
                }
                catch (std::bad_alloc &)
                {
                    return nullptr;
                }
                oSummary.oSetDistinctValues.clear();
            }

            if (nFID < 0 ||
                nFID >= static_cast<GIntBig>(m_aosDistinctList.size()))
                return nullptr;

            const size_t nIdx = static_cast<size_t>(nFID);
            if (m_aosDistinctList[nIdx] != SZ_OGR_NULL)
                m_poSummaryFeature->SetField(0,
                                             m_aosDistinctList[nIdx].c_str());
            else
                m_poSummaryFeature->SetFieldNull(0);
        }

        m_poSummaryFeature->SetFID(nFID);

        return m_poSummaryFeature->Clone();
    }

    /* -------------------------------------------------------------------- */
    /*      Handle request for random record.                               */
    /* -------------------------------------------------------------------- */
    auto poSrcFeature =
        std::unique_ptr<OGRFeature>(m_poSrcLayer->GetFeature(nFID));
    if (poSrcFeature == nullptr)
        return nullptr;

    return TranslateFeature(std::move(poSrcFeature)).release();
}

/************************************************************************/
/*                          GetSpatialFilter()                          */
/************************************************************************/

OGRGeometry *OGRGenSQLResultsLayer::GetSpatialFilter()

{
    return nullptr;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *OGRGenSQLResultsLayer::GetLayerDefn()

{
    swq_select *psSelectInfo = m_pSelectInfo.get();
    if (psSelectInfo->query_mode == SWQM_SUMMARY_RECORD && !m_poSummaryFeature)
    {
        // Run PrepareSummary() is we have a COUNT column so as to be
        // able to downcast it from OFTInteger64 to OFTInteger
        for (int iField = 0; iField < psSelectInfo->result_columns(); iField++)
        {
            swq_col_def *psColDef = &psSelectInfo->column_defs[iField];
            if (psColDef->col_func == SWQCF_COUNT)
            {
                PrepareSummary();
                break;
            }
        }
    }

    return m_poDefn;
}

/************************************************************************/
/*                         FreeIndexFields()                            */
/************************************************************************/

void OGRGenSQLResultsLayer::FreeIndexFields(OGRField *pasIndexFields,
                                            size_t l_nIndexSize)
{
    swq_select *psSelectInfo = m_pSelectInfo.get();
    const int nOrderItems = psSelectInfo->order_specs;

    /* -------------------------------------------------------------------- */
    /*      Free the key field values.                                      */
    /* -------------------------------------------------------------------- */
    for (int iKey = 0; iKey < nOrderItems; iKey++)
    {
        swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;

        if (psKeyDef->field_index >= m_iFIDFieldIndex)
        {
            CPLAssert(psKeyDef->field_index <
                      m_iFIDFieldIndex + SPECIAL_FIELD_COUNT);
            /* warning: only special fields of type string should be deallocated
             */
            if (SpecialFieldTypes[psKeyDef->field_index - m_iFIDFieldIndex] ==
                SWQ_STRING)
            {
                for (size_t i = 0; i < l_nIndexSize; i++)
                {
                    OGRField *psField = pasIndexFields + iKey + i * nOrderItems;
                    CPLFree(psField->String);
                }
            }
            continue;
        }

        OGRFieldDefn *poFDefn =
            m_poSrcLayer->GetLayerDefn()->GetFieldDefn(psKeyDef->field_index);

        if (poFDefn->GetType() == OFTString)
        {
            for (size_t i = 0; i < l_nIndexSize; i++)
            {
                OGRField *psField = pasIndexFields + iKey + i * nOrderItems;

                if (!OGR_RawField_IsUnset(psField) &&
                    !OGR_RawField_IsNull(psField))
                    CPLFree(psField->String);
            }
        }
    }
}

/************************************************************************/
/*                         ReadIndexFields()                            */
/************************************************************************/

void OGRGenSQLResultsLayer::ReadIndexFields(OGRFeature *poSrcFeat,
                                            int nOrderItems,
                                            OGRField *pasIndexFields)
{
    swq_select *psSelectInfo = m_pSelectInfo.get();
    for (int iKey = 0; iKey < nOrderItems; iKey++)
    {
        const swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;
        OGRField *psDstField = pasIndexFields + iKey;

        if (psKeyDef->field_index >= m_iFIDFieldIndex)
        {
            CPLAssert(psKeyDef->field_index <
                      m_iFIDFieldIndex + SPECIAL_FIELD_COUNT);

            switch (SpecialFieldTypes[psKeyDef->field_index - m_iFIDFieldIndex])
            {
                case SWQ_INTEGER:
                case SWQ_INTEGER64:
                    // Yes, store Integer as Integer64.
                    // This is consistent with the test in Compare()
                    psDstField->Integer64 =
                        poSrcFeat->GetFieldAsInteger64(psKeyDef->field_index);
                    break;

                case SWQ_FLOAT:
                    psDstField->Real =
                        poSrcFeat->GetFieldAsDouble(psKeyDef->field_index);
                    break;

                default:
                    psDstField->String = CPLStrdup(
                        poSrcFeat->GetFieldAsString(psKeyDef->field_index));
                    break;
            }

            continue;
        }

        OGRFieldDefn *poFDefn =
            m_poSrcLayer->GetLayerDefn()->GetFieldDefn(psKeyDef->field_index);

        OGRField *psSrcField = poSrcFeat->GetRawFieldRef(psKeyDef->field_index);

        if (poFDefn->GetType() == OFTInteger ||
            poFDefn->GetType() == OFTInteger64 ||
            poFDefn->GetType() == OFTReal || poFDefn->GetType() == OFTDate ||
            poFDefn->GetType() == OFTTime || poFDefn->GetType() == OFTDateTime)
            memcpy(psDstField, psSrcField, sizeof(OGRField));
        else if (poFDefn->GetType() == OFTString)
        {
            if (poSrcFeat->IsFieldSetAndNotNull(psKeyDef->field_index))
                psDstField->String = CPLStrdup(psSrcField->String);
            else
                memcpy(psDstField, psSrcField, sizeof(OGRField));
        }
    }
}

/************************************************************************/
/*                         CreateOrderByIndex()                         */
/*                                                                      */
/*      This method is responsible for creating an index providing      */
/*      ordered access to the features according to the supplied        */
/*      ORDER BY clauses.                                               */
/*                                                                      */
/*      This is accomplished by making one pass through all the         */
/*      eligible source features, and capturing the order by fields     */
/*      of all records in memory.  A quick sort is then applied to      */
/*      this in memory copy of the order-by fields to create the        */
/*      required index.                                                 */
/*                                                                      */
/*      Keeping all the key values in memory will *not* scale up to     */
/*      very large input datasets.                                      */
/************************************************************************/

void OGRGenSQLResultsLayer::CreateOrderByIndex()

{
    swq_select *psSelectInfo = m_pSelectInfo.get();
    const int nOrderItems = psSelectInfo->order_specs;

    if (!(nOrderItems > 0 && psSelectInfo->query_mode == SWQM_RECORDSET))
        return;

    if (m_bOrderByValid)
        return;

    m_bOrderByValid = true;
    m_anFIDIndex.clear();

    ResetReading();

    /* -------------------------------------------------------------------- */
    /*      Optimize (memory-wise) ORDER BY ... LIMIT 1 [OFFSET 0] case.    */
    /* -------------------------------------------------------------------- */
    if (psSelectInfo->offset == 0 && psSelectInfo->limit == 1)
    {
        std::vector<OGRField> asCurrentFields(nOrderItems);
        std::vector<OGRField> asBestFields(nOrderItems);
        memset(asCurrentFields.data(), 0, sizeof(OGRField) * nOrderItems);
        memset(asBestFields.data(), 0, sizeof(OGRField) * nOrderItems);
        bool bFoundSrcFeature = false;
        GIntBig nBestFID = 0;
        for (auto &&poSrcFeat : *m_poSrcLayer)
        {
            ReadIndexFields(poSrcFeat.get(), nOrderItems,
                            asCurrentFields.data());
            if (!bFoundSrcFeature ||
                Compare(asCurrentFields.data(), asBestFields.data()) < 0)
            {
                bFoundSrcFeature = true;
                nBestFID = poSrcFeat->GetFID();
                FreeIndexFields(asBestFields.data(), 1);
                memcpy(asBestFields.data(), asCurrentFields.data(),
                       sizeof(OGRField) * nOrderItems);
            }
            else
            {
                FreeIndexFields(asCurrentFields.data(), 1);
            }
            memset(asCurrentFields.data(), 0, sizeof(OGRField) * nOrderItems);
        }
        FreeIndexFields(asBestFields.data(), 1);

        if (bFoundSrcFeature)
        {
            m_anFIDIndex.resize(1);
            m_anFIDIndex[0] = nBestFID;
        }
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Allocate set of key values, and the output index.               */
    /* -------------------------------------------------------------------- */
    size_t nFeaturesAlloc = 100;
    size_t nIndexSize = 0;
    std::vector<OGRField> asIndexFields(nOrderItems * nFeaturesAlloc);
    memset(asIndexFields.data(), 0,
           sizeof(OGRField) * nOrderItems * nFeaturesAlloc);
    std::vector<GIntBig> anFIDList;

    // Frees nIndexSize rows of asIndexFields
    struct IndexFieldsFreer
    {
        OGRGenSQLResultsLayer &m_oLayer;
        std::vector<OGRField> &m_asIndexFields;
        size_t &m_nIndexSize;

        IndexFieldsFreer(OGRGenSQLResultsLayer &poLayerIn,
                         std::vector<OGRField> &asIndexFieldsIn,
                         size_t &nIndexSizeIn)
            : m_oLayer(poLayerIn), m_asIndexFields(asIndexFieldsIn),
              m_nIndexSize(nIndexSizeIn)
        {
        }

        ~IndexFieldsFreer()
        {
            m_oLayer.FreeIndexFields(m_asIndexFields.data(), m_nIndexSize);
        }

        IndexFieldsFreer(const IndexFieldsFreer &) = delete;
        IndexFieldsFreer &operator=(const IndexFieldsFreer &) = delete;
    };

    IndexFieldsFreer oIndexFieldsFreer(*this, asIndexFields, nIndexSize);

    /* -------------------------------------------------------------------- */
    /*      Read in all the key values.                                     */
    /* -------------------------------------------------------------------- */

    for (auto &&poSrcFeat : *m_poSrcLayer)
    {
        if (nIndexSize == nFeaturesAlloc)
        {
            const uint64_t nNewFeaturesAlloc64 =
                static_cast<uint64_t>(nFeaturesAlloc) + nFeaturesAlloc / 3;
#if SIZEOF_SIZE_T == 4
            if (static_cast<size_t>(nNewFeaturesAlloc64) !=
                    nNewFeaturesAlloc64 ||
                static_cast<size_t>(sizeof(OGRField) * nOrderItems *
                                    nNewFeaturesAlloc64) !=
                    static_cast<uint64_t>(sizeof(OGRField)) * nOrderItems *
                        nNewFeaturesAlloc64)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot allocate pasIndexFields");
                return;
            }
#endif
            const size_t nNewFeaturesAlloc =
                static_cast<size_t>(nNewFeaturesAlloc64);

            try
            {
                asIndexFields.resize(nOrderItems * nNewFeaturesAlloc);
                anFIDList.reserve(nNewFeaturesAlloc);
            }
            catch (const std::bad_alloc &)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "CreateOrderByIndex(): out of memory");
                return;
            }

            memset(asIndexFields.data() + nFeaturesAlloc * nOrderItems, 0,
                   sizeof(OGRField) * nOrderItems *
                       (nNewFeaturesAlloc - nFeaturesAlloc));

            nFeaturesAlloc = nNewFeaturesAlloc;
        }

        ReadIndexFields(poSrcFeat.get(), nOrderItems,
                        asIndexFields.data() + nIndexSize * nOrderItems);

        anFIDList.push_back(poSrcFeat->GetFID());

        nIndexSize++;
    }

    // CPLDebug("GenSQL", "CreateOrderByIndex() = %zu features", nIndexSize);

    /* -------------------------------------------------------------------- */
    /*      Initialize m_anFIDIndex                                         */
    /* -------------------------------------------------------------------- */
    try
    {
        m_anFIDIndex.reserve(nIndexSize);
    }
    catch (const std::bad_alloc &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "CreateOrderByIndex(): out of memory");
        return;
    }
    for (size_t i = 0; i < nIndexSize; i++)
        m_anFIDIndex.push_back(static_cast<GIntBig>(i));

    /* -------------------------------------------------------------------- */
    /*      Quick sort the records.                                         */
    /* -------------------------------------------------------------------- */

    GIntBig *panMerged = static_cast<GIntBig *>(
        VSI_MALLOC_VERBOSE(sizeof(GIntBig) * nIndexSize));
    if (panMerged == nullptr)
    {
        m_anFIDIndex.clear();
        return;
    }

    // Note: this merge sort is slightly faster than std::sort()
    SortIndexSection(asIndexFields.data(), panMerged, 0, nIndexSize);
    VSIFree(panMerged);

    /* -------------------------------------------------------------------- */
    /*      Rework the FID map to map to real FIDs.                         */
    /* -------------------------------------------------------------------- */
    bool bAlreadySorted = true;
    for (size_t i = 0; i < nIndexSize; i++)
    {
        if (m_anFIDIndex[i] != static_cast<GIntBig>(i))
            bAlreadySorted = false;
        m_anFIDIndex[i] = anFIDList[static_cast<size_t>(m_anFIDIndex[i])];
    }

    /* If it is already sorted, then free than m_anFIDIndex array */
    /* so that GetNextFeature() can call a sequential GetNextFeature() */
    /* on the source array. Very useful for layers where random access */
    /* is slow. */
    /* Use case: the GML result of a WFS GetFeature with a SORTBY */
    if (bAlreadySorted)
    {
        m_anFIDIndex.clear();
    }

    ResetReading();
}

/************************************************************************/
/*                          SortIndexSection()                          */
/*                                                                      */
/*      Sort the records in a section of the index.                     */
/************************************************************************/

void OGRGenSQLResultsLayer::SortIndexSection(const OGRField *pasIndexFields,
                                             GIntBig *panMerged, size_t nStart,
                                             size_t nEntries)

{
    if (nEntries < 2)
        return;

    swq_select *psSelectInfo = m_pSelectInfo.get();
    const int nOrderItems = psSelectInfo->order_specs;

    size_t nFirstGroup = nEntries / 2;
    size_t nFirstStart = nStart;
    size_t nSecondGroup = nEntries - nFirstGroup;
    size_t nSecondStart = nStart + nFirstGroup;

    SortIndexSection(pasIndexFields, panMerged, nFirstStart, nFirstGroup);
    SortIndexSection(pasIndexFields, panMerged, nSecondStart, nSecondGroup);

    for (size_t iMerge = 0; iMerge < nEntries; ++iMerge)
    {
        int nResult = 0;

        if (nFirstGroup == 0)
            nResult = 1;
        else if (nSecondGroup == 0)
            nResult = -1;
        else
            nResult = Compare(
                pasIndexFields + m_anFIDIndex[nFirstStart] * nOrderItems,
                pasIndexFields + m_anFIDIndex[nSecondStart] * nOrderItems);

        if (nResult > 0)
        {
            panMerged[iMerge] = m_anFIDIndex[nSecondStart];
            nSecondStart++;
            nSecondGroup--;
        }
        else
        {
            panMerged[iMerge] = m_anFIDIndex[nFirstStart];
            nFirstStart++;
            nFirstGroup--;
        }
    }

    /* Copy the merge list back into the main index */
    memcpy(m_anFIDIndex.data() + nStart, panMerged, sizeof(GIntBig) * nEntries);
}

/************************************************************************/
/*                           ComparePrimitive()                         */
/************************************************************************/

template <class T> static inline int ComparePrimitive(const T &a, const T &b)
{
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

/************************************************************************/
/*                              Compare()                               */
/************************************************************************/

int OGRGenSQLResultsLayer::Compare(const OGRField *pasFirstTuple,
                                   const OGRField *pasSecondTuple)

{
    swq_select *psSelectInfo = m_pSelectInfo.get();
    int nResult = 0, iKey;

    for (iKey = 0; nResult == 0 && iKey < psSelectInfo->order_specs; iKey++)
    {
        swq_order_def *psKeyDef = psSelectInfo->order_defs + iKey;
        OGRFieldDefn *poFDefn = nullptr;

        if (psKeyDef->field_index >= m_iFIDFieldIndex)
            poFDefn = nullptr;
        else
            poFDefn = m_poSrcLayer->GetLayerDefn()->GetFieldDefn(
                psKeyDef->field_index);

        if (OGR_RawField_IsUnset(&pasFirstTuple[iKey]) ||
            OGR_RawField_IsNull(&pasFirstTuple[iKey]))
        {
            if (OGR_RawField_IsUnset(&pasSecondTuple[iKey]) ||
                OGR_RawField_IsNull(&pasSecondTuple[iKey]))
                nResult = 0;
            else
                nResult = -1;
        }
        else if (OGR_RawField_IsUnset(&pasSecondTuple[iKey]) ||
                 OGR_RawField_IsNull(&pasSecondTuple[iKey]))
        {
            nResult = 1;
        }
        else if (poFDefn == nullptr)
        {
            CPLAssert(psKeyDef->field_index <
                      m_iFIDFieldIndex + SPECIAL_FIELD_COUNT);
            switch (SpecialFieldTypes[psKeyDef->field_index - m_iFIDFieldIndex])
            {
                case SWQ_INTEGER:
                    // Yes, read Integer in Integer64.
                    // This is consistent with what is done ReadIndexFields()
                case SWQ_INTEGER64:
                    nResult = ComparePrimitive(pasFirstTuple[iKey].Integer64,
                                               pasSecondTuple[iKey].Integer64);
                    break;
                case SWQ_FLOAT:
                    nResult = ComparePrimitive(pasFirstTuple[iKey].Real,
                                               pasSecondTuple[iKey].Real);
                    break;
                case SWQ_STRING:
                    nResult = strcmp(pasFirstTuple[iKey].String,
                                     pasSecondTuple[iKey].String);
                    break;

                default:
                    CPLAssert(false);
                    nResult = 0;
            }
        }
        else if (poFDefn->GetType() == OFTInteger)
        {
            nResult = ComparePrimitive(pasFirstTuple[iKey].Integer,
                                       pasSecondTuple[iKey].Integer);
        }
        else if (poFDefn->GetType() == OFTInteger64)
        {
            nResult = ComparePrimitive(pasFirstTuple[iKey].Integer64,
                                       pasSecondTuple[iKey].Integer64);
        }
        else if (poFDefn->GetType() == OFTString)
        {
            nResult =
                strcmp(pasFirstTuple[iKey].String, pasSecondTuple[iKey].String);
        }
        else if (poFDefn->GetType() == OFTReal)
        {
            nResult = ComparePrimitive(pasFirstTuple[iKey].Real,
                                       pasSecondTuple[iKey].Real);
        }
        else if (poFDefn->GetType() == OFTDate ||
                 poFDefn->GetType() == OFTTime ||
                 poFDefn->GetType() == OFTDateTime)
        {
            nResult =
                OGRCompareDate(&pasFirstTuple[iKey], &pasSecondTuple[iKey]);
        }

        if (!(psKeyDef->ascending_flag))
            nResult *= -1;
    }

    return nResult;
}

/************************************************************************/
/*                         AddFieldDefnToSet()                          */
/************************************************************************/

void OGRGenSQLResultsLayer::AddFieldDefnToSet(int iTable, int iColumn,
                                              CPLHashSet *hSet)
{
    if (iTable != -1)
    {
        OGRLayer *poLayer = m_apoTableLayers[iTable];
        const auto poLayerDefn = poLayer->GetLayerDefn();
        const int nFieldCount = poLayerDefn->GetFieldCount();
        if (iColumn == -1)
        {
            for (int i = 0; i < nFieldCount; ++i)
            {
                OGRFieldDefn *poFDefn = poLayerDefn->GetFieldDefn(i);
                CPLHashSetInsert(hSet, poFDefn);
            }

            const int nGeomFieldCount = poLayerDefn->GetGeomFieldCount();
            for (int i = 0; i < nGeomFieldCount; ++i)
            {
                OGRGeomFieldDefn *poFDefn = poLayerDefn->GetGeomFieldDefn(i);
                CPLHashSetInsert(hSet, poFDefn);
            }
        }
        else
        {
            if (iColumn < nFieldCount)
            {
                OGRFieldDefn *poFDefn = poLayerDefn->GetFieldDefn(iColumn);
                CPLHashSetInsert(hSet, poFDefn);
            }
            else if (iColumn == nFieldCount + SPF_OGR_GEOMETRY ||
                     iColumn == nFieldCount + SPF_OGR_GEOM_WKT ||
                     iColumn == nFieldCount + SPF_OGR_GEOM_AREA)
            {
                auto poSrcGFDefn = poLayerDefn->GetGeomFieldDefn(0);
                CPLHashSetInsert(hSet, poSrcGFDefn);
            }
            else if (IS_GEOM_FIELD_INDEX(poLayerDefn, iColumn))
            {
                const int iSrcGeomField =
                    ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(poLayerDefn, iColumn);
                auto poSrcGFDefn = poLayerDefn->GetGeomFieldDefn(iSrcGeomField);
                CPLHashSetInsert(hSet, poSrcGFDefn);
            }
        }
    }
}

/************************************************************************/
/*                   ExploreExprForIgnoredFields()                      */
/************************************************************************/

void OGRGenSQLResultsLayer::ExploreExprForIgnoredFields(swq_expr_node *expr,
                                                        CPLHashSet *hSet)
{
    if (expr->eNodeType == SNT_COLUMN)
    {
        AddFieldDefnToSet(expr->table_index, expr->field_index, hSet);
    }
    else if (expr->eNodeType == SNT_OPERATION)
    {
        for (int i = 0; i < expr->nSubExprCount; i++)
            ExploreExprForIgnoredFields(expr->papoSubExpr[i], hSet);
    }
}

/************************************************************************/
/*                     FindAndSetIgnoredFields()                        */
/************************************************************************/

void OGRGenSQLResultsLayer::FindAndSetIgnoredFields()
{
    swq_select *psSelectInfo = m_pSelectInfo.get();
    CPLHashSet *hSet =
        CPLHashSetNew(CPLHashSetHashPointer, CPLHashSetEqualPointer, nullptr);

    /* -------------------------------------------------------------------- */
    /*      1st phase : explore the whole select infos to determine which   */
    /*      source fields are used                                          */
    /* -------------------------------------------------------------------- */
    for (int iField = 0; iField < psSelectInfo->result_columns(); iField++)
    {
        swq_col_def *psColDef = &psSelectInfo->column_defs[iField];
        AddFieldDefnToSet(psColDef->table_index, psColDef->field_index, hSet);
        if (psColDef->expr)
            ExploreExprForIgnoredFields(psColDef->expr, hSet);
    }

    if (psSelectInfo->where_expr)
        ExploreExprForIgnoredFields(psSelectInfo->where_expr, hSet);

    for (int iJoin = 0; iJoin < psSelectInfo->join_count; iJoin++)
    {
        swq_join_def *psJoinDef = psSelectInfo->join_defs + iJoin;
        ExploreExprForIgnoredFields(psJoinDef->poExpr, hSet);
    }

    for (int iOrder = 0; iOrder < psSelectInfo->order_specs; iOrder++)
    {
        swq_order_def *psOrderDef = psSelectInfo->order_defs + iOrder;
        AddFieldDefnToSet(psOrderDef->table_index, psOrderDef->field_index,
                          hSet);
    }

    /* -------------------------------------------------------------------- */
    /*      2nd phase : now, we can exclude the unused fields               */
    /* -------------------------------------------------------------------- */
    for (int iTable = 0; iTable < psSelectInfo->table_count; iTable++)
    {
        OGRLayer *poLayer = m_apoTableLayers[iTable];
        OGRFeatureDefn *poSrcFDefn = poLayer->GetLayerDefn();
        char **papszIgnoredFields = nullptr;
        const int nSrcFieldCount = poSrcFDefn->GetFieldCount();
        for (int iSrcField = 0; iSrcField < nSrcFieldCount; iSrcField++)
        {
            OGRFieldDefn *poFDefn = poSrcFDefn->GetFieldDefn(iSrcField);
            if (CPLHashSetLookup(hSet, poFDefn) == nullptr)
            {
                papszIgnoredFields =
                    CSLAddString(papszIgnoredFields, poFDefn->GetNameRef());
                // CPLDebug("OGR", "Adding %s to the list of ignored fields of
                // layer %s",
                //          poFDefn->GetNameRef(), poLayer->GetName());
            }
        }
        const int nSrcGeomFieldCount = poSrcFDefn->GetGeomFieldCount();
        for (int iSrcField = 0; iSrcField < nSrcGeomFieldCount; iSrcField++)
        {
            OGRGeomFieldDefn *poFDefn = poSrcFDefn->GetGeomFieldDefn(iSrcField);
            if (CPLHashSetLookup(hSet, poFDefn) == nullptr)
            {
                papszIgnoredFields =
                    CSLAddString(papszIgnoredFields, poFDefn->GetNameRef());
                // CPLDebug("OGR", "Adding %s to the list of ignored fields of
                // layer %s",
                //          poFDefn->GetNameRef(), poLayer->GetName());
            }
        }
        poLayer->SetIgnoredFields(
            const_cast<const char **>(papszIgnoredFields));
        CSLDestroy(papszIgnoredFields);
    }

    CPLHashSetDestroy(hSet);
}

/************************************************************************/
/*                       InvalidateOrderByIndex()                       */
/************************************************************************/

void OGRGenSQLResultsLayer::InvalidateOrderByIndex()
{
    m_anFIDIndex.clear();
    m_bOrderByValid = false;
}

/************************************************************************/
/*                       SetAttributeFilter()                           */
/************************************************************************/

OGRErr OGRGenSQLResultsLayer::SetAttributeFilter(const char *pszAttributeFilter)
{
    const std::string osAdditionalWHERE =
        pszAttributeFilter ? pszAttributeFilter : "";
    std::string osWHERE;
    if (!m_bForwardWhereToSourceLayer && !m_osInitialWHERE.empty())
    {
        if (!osAdditionalWHERE.empty())
            osWHERE += '(';
        osWHERE += m_osInitialWHERE;
        if (!osAdditionalWHERE.empty())
            osWHERE += ") AND (";
    }
    osWHERE += osAdditionalWHERE;
    if (!m_bForwardWhereToSourceLayer && !m_osInitialWHERE.empty() &&
        !osAdditionalWHERE.empty())
    {
        osWHERE += ')';
    }
    InvalidateOrderByIndex();
    return OGRLayer::SetAttributeFilter(osWHERE.empty() ? nullptr
                                                        : osWHERE.c_str());
}

/************************************************************************/
/*                       ISetSpatialFilter()                            */
/************************************************************************/

OGRErr OGRGenSQLResultsLayer::ISetSpatialFilter(int iGeomField,
                                                const OGRGeometry *poGeom)
{
    InvalidateOrderByIndex();
    return OGRLayer::ISetSpatialFilter(iGeomField, poGeom);
}

/************************************************************************/
/*                  OGRGenSQLResultsLayerArrowStreamPrivateData         */
/************************************************************************/

// Structure whose instances are set on the ArrowArrayStream::private_data
// member of the ArrowArrayStream returned by OGRGenSQLResultsLayer::GetArrowStream()
struct OGRGenSQLResultsLayerArrowStreamPrivateData
{
    // Member shared with OGRLayer::m_poSharedArrowArrayStreamPrivateData
    // If the layer pointed by poShared->poLayer is destroyed, before its
    // destruction, it nullifies poShared->poLayer, which we can detect.
    std::shared_ptr<OGRLayer::ArrowArrayStreamPrivateData> poShared{};

    // ArrowArrayStream to be used with poShared->poLayer
    struct ArrowArrayStream *psSrcLayerStream = nullptr;

    // Original release() callback of the ArrowArrayStream passed to
    // OGRGenSQLResultsLayer::GetArrowStream()
    void (*release_backup)(struct ArrowArrayStream *) = nullptr;

    // Original private_data member of the ArrowArrayStream passed to
    // OGRGenSQLResultsLayer::GetArrowStream()
    void *private_data_backup = nullptr;

    // Set as the ArrowArrayStream::release callback member of the
    // ArrowArrayStream returned by OGRGenSQLResultsLayer::GetArrowStream()
    static void Release(struct ArrowArrayStream *self)
    {
        OGRGenSQLResultsLayerArrowStreamPrivateData *psPrivateData =
            static_cast<OGRGenSQLResultsLayerArrowStreamPrivateData *>(
                self->private_data);

        // Release source layer stream
        if (psPrivateData->psSrcLayerStream->release)
            psPrivateData->psSrcLayerStream->release(
                psPrivateData->psSrcLayerStream);
        CPLFree(psPrivateData->psSrcLayerStream);

        // Release ourselves using the base method
        self->private_data = psPrivateData->private_data_backup;
        self->release = psPrivateData->release_backup;
        delete psPrivateData;
        if (self->release)
            self->release(self);
    }

    // Set as the ArrowArrayStream::get_schema callback member of the
    // ArrowArrayStream returned by OGRGenSQLResultsLayer::GetArrowStream()
    static int GetSchema(struct ArrowArrayStream *self, struct ArrowSchema *out)
    {
        OGRGenSQLResultsLayerArrowStreamPrivateData *psPrivateData =
            static_cast<OGRGenSQLResultsLayerArrowStreamPrivateData *>(
                self->private_data);
        auto poLayer = dynamic_cast<OGRGenSQLResultsLayer *>(
            psPrivateData->poShared->m_poLayer);
        if (!poLayer)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Calling get_schema() on a freed OGRLayer is not supported");
            return EINVAL;
        }
        return poLayer->GetArrowSchemaForwarded(self, out);
    }

    // Set as the ArrowArrayStream::get_next callback member of the
    // ArrowArrayStream returned by OGRGenSQLResultsLayer::GetArrowStream()
    static int GetNext(struct ArrowArrayStream *self, struct ArrowArray *out)
    {
        OGRGenSQLResultsLayerArrowStreamPrivateData *psPrivateData =
            static_cast<OGRGenSQLResultsLayerArrowStreamPrivateData *>(
                self->private_data);
        auto poLayer = dynamic_cast<OGRGenSQLResultsLayer *>(
            psPrivateData->poShared->m_poLayer);
        if (!poLayer)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Calling get_next() on a freed OGRLayer is not supported");
            return EINVAL;
        }
        return poLayer->GetNextArrowArrayForwarded(self, out);
    }
};

/************************************************************************/
/*                          GetArrowStream()                            */
/************************************************************************/

bool OGRGenSQLResultsLayer::GetArrowStream(struct ArrowArrayStream *out_stream,
                                           CSLConstList papszOptions)
{
    if (!TestCapability(OLCFastGetArrowStream) ||
        CPLTestBool(CPLGetConfigOption("OGR_GENSQL_STREAM_BASE_IMPL", "NO")))
    {
        CPLStringList aosOptions(papszOptions);
        aosOptions.SetNameValue("OGR_GENSQL_STREAM_BASE_IMPL", "YES");
        return OGRLayer::GetArrowStream(out_stream, aosOptions.List());
    }

    const swq_select *psSelectInfo = m_pSelectInfo.get();
    if (m_nIteratedFeatures != -1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetArrowStream() not supported on non-rewinded layer");
        return false;
    }
    CPLStringList aosOptions(papszOptions);
    if (psSelectInfo->limit > 0)
    {
        aosOptions.SetNameValue(
            "MAX_FEATURES_IN_BATCH",
            CPLSPrintf(CPL_FRMT_GIB,
                       std::min(psSelectInfo->limit,
                                CPLAtoGIntBig(aosOptions.FetchNameValueDef(
                                    "MAX_FEATURES_IN_BATCH", "65536")))));
    }
    bool bRet = OGRLayer::GetArrowStream(out_stream, aosOptions.List());
    if (bRet)
    {
        auto psSrcLayerStream = static_cast<struct ArrowArrayStream *>(
            CPLMalloc(sizeof(ArrowArrayStream)));
        if (m_poSrcLayer->GetArrowStream(psSrcLayerStream, aosOptions.List()))
        {
            auto psPrivateData =
                new OGRGenSQLResultsLayerArrowStreamPrivateData;
            CPLAssert(m_poSharedArrowArrayStreamPrivateData);
            psPrivateData->poShared = m_poSharedArrowArrayStreamPrivateData;
            psPrivateData->psSrcLayerStream = psSrcLayerStream;
            psPrivateData->release_backup = out_stream->release;
            psPrivateData->private_data_backup = out_stream->private_data;
            out_stream->get_schema =
                OGRGenSQLResultsLayerArrowStreamPrivateData::GetSchema;
            out_stream->get_next =
                OGRGenSQLResultsLayerArrowStreamPrivateData::GetNext;
            out_stream->release =
                OGRGenSQLResultsLayerArrowStreamPrivateData::Release;
            out_stream->private_data = psPrivateData;
        }
        else
        {
            if (psSrcLayerStream->release)
                psSrcLayerStream->release(psSrcLayerStream);
            CPLFree(psSrcLayerStream);
            if (out_stream->release)
                out_stream->release(out_stream);
            bRet = false;
        }
    }
    return bRet;
}

/************************************************************************/
/*                          GetArrowSchema()                            */
/************************************************************************/

int OGRGenSQLResultsLayer::GetArrowSchema(struct ArrowArrayStream *stream,
                                          struct ArrowSchema *out_schema)
{
    if (m_aosArrowArrayStreamOptions.FetchNameValue(
            "OGR_GENSQL_STREAM_BASE_IMPL") ||
        !TestCapability(OLCFastGetArrowStream))
    {
        return OGRLayer::GetArrowSchema(stream, out_schema);
    }

    return GetArrowSchemaForwarded(stream, out_schema);
}

/************************************************************************/
/*                      GetArrowSchemaForwarded()                       */
/************************************************************************/

int OGRGenSQLResultsLayer::GetArrowSchemaForwarded(
    struct ArrowArrayStream *stream, struct ArrowSchema *out_schema) const
{
    const swq_select *psSelectInfo = m_pSelectInfo.get();
    OGRGenSQLResultsLayerArrowStreamPrivateData *psPrivateData =
        static_cast<OGRGenSQLResultsLayerArrowStreamPrivateData *>(
            stream->private_data);
    int ret = m_poSrcLayer->GetArrowSchema(psPrivateData->psSrcLayerStream,
                                           out_schema);
    if (ret == 0)
    {
        struct ArrowSchema newSchema;
        ret = OGRCloneArrowSchema(out_schema, &newSchema) ? 0 : EIO;
        if (out_schema->release)
            out_schema->release(out_schema);
        if (ret == 0)
        {
            std::map<std::string, std::string> oMapSrcNameToRenamed;
            for (std::size_t iField = 0;
                 iField < psSelectInfo->column_defs.size(); iField++)
            {
                const swq_col_def *psColDef =
                    &psSelectInfo->column_defs[iField];
                CPLAssert(!psColDef->bHidden);
                CPLAssert(psColDef->table_index >= 0);
                CPLAssert(psColDef->col_func == SWQCF_NONE);

                const auto poLayerDefn =
                    m_apoTableLayers[psColDef->table_index]->GetLayerDefn();
                CPLAssert(poLayerDefn);

                if (psColDef->field_index >= 0 &&
                    psColDef->field_index < poLayerDefn->GetFieldCount())
                {
                    const auto poSrcFDefn =
                        poLayerDefn->GetFieldDefn(psColDef->field_index);
                    if (psColDef->field_alias)
                        oMapSrcNameToRenamed[poSrcFDefn->GetNameRef()] =
                            psColDef->field_alias;
                }
                else if (IS_GEOM_FIELD_INDEX(poLayerDefn,
                                             psColDef->field_index))
                {
                    const int iSrcGeomField =
                        ALL_FIELD_INDEX_TO_GEOM_FIELD_INDEX(
                            poLayerDefn, psColDef->field_index);
                    const auto poSrcGFDefn =
                        poLayerDefn->GetGeomFieldDefn(iSrcGeomField);
                    if (psColDef->field_alias)
                        oMapSrcNameToRenamed[poSrcGFDefn->GetNameRef()] =
                            psColDef->field_alias;
                }
            }

            for (int i = 0; i < newSchema.n_children; ++i)
            {
                const auto oIter =
                    oMapSrcNameToRenamed.find(newSchema.children[i]->name);
                if (oIter != oMapSrcNameToRenamed.end())
                {
                    CPLFree(const_cast<char *>(newSchema.children[i]->name));
                    newSchema.children[i]->name =
                        CPLStrdup(oIter->second.c_str());
                }
            }

            memcpy(out_schema, &newSchema, sizeof(newSchema));
        }
    }
    return ret;
}

/************************************************************************/
/*                      GetNextArrowArray()                             */
/************************************************************************/

int OGRGenSQLResultsLayer::GetNextArrowArray(struct ArrowArrayStream *stream,
                                             struct ArrowArray *out_array)
{
    if (m_aosArrowArrayStreamOptions.FetchNameValue(
            "OGR_GENSQL_STREAM_BASE_IMPL") ||
        !TestCapability(OLCFastGetArrowStream))
    {
        return OGRLayer::GetNextArrowArray(stream, out_array);
    }

    return GetNextArrowArrayForwarded(stream, out_array);
}

/************************************************************************/
/*                  GetNextArrowArrayForwarded()                        */
/************************************************************************/

int OGRGenSQLResultsLayer::GetNextArrowArrayForwarded(
    struct ArrowArrayStream *stream, struct ArrowArray *out_array)
{
    const swq_select *psSelectInfo = m_pSelectInfo.get();
    if (psSelectInfo->limit >= 0 && m_nIteratedFeatures >= psSelectInfo->limit)
    {
        memset(out_array, 0, sizeof(*out_array));
        return 0;
    }

    OGRGenSQLResultsLayerArrowStreamPrivateData *psPrivateData =
        static_cast<OGRGenSQLResultsLayerArrowStreamPrivateData *>(
            stream->private_data);
    const int ret = m_poSrcLayer->GetNextArrowArray(
        psPrivateData->psSrcLayerStream, out_array);
    if (ret == 0 && psSelectInfo->limit >= 0)
    {
        if (m_nIteratedFeatures < 0)
            m_nIteratedFeatures = 0;
        m_nIteratedFeatures += out_array->length;
        if (m_nIteratedFeatures > psSelectInfo->limit)
        {
            out_array->length -= m_nIteratedFeatures - psSelectInfo->limit;
            for (int i = 0; i < out_array->n_children; ++i)
            {
                out_array->children[i]->length -=
                    m_nIteratedFeatures - psSelectInfo->limit;
            }
        }
    }
    return ret;
}

//! @endcond
