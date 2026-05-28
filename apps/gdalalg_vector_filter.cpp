/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "filter" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_filter.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include <set>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALVectorFilterAlgorithm::GDALVectorFilterAlgorithm()        */
/************************************************************************/

GDALVectorFilterAlgorithm::GDALVectorFilterAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    auto &layerArg = AddActiveLayerArg(&m_activeLayer);
    AddBBOXArg(&m_bbox);
    AddArg("where", 0,
           _("Attribute query in a restricted form of the queries used in the "
             "SQL WHERE statement"),
           &m_where)
        .SetReadFromFileAtSyntaxAllowed()
        .SetMetaVar("<WHERE>|@<filename>")
        .SetRemoveSQLCommentsEnabled()
        .SetAutoCompleteFunction(
            [this, &layerArg](const std::string &currentValue)
            { return CompleteWhere(layerArg, currentValue); });
    AddArg("update-extent", 0,
           _("Update layer extent to take into account the filter"),
           &m_updateExtent);
}

/************************************************************************/
/*              GDALVectorFilterAlgorithmLayerChangeExtent              */
/************************************************************************/

constexpr const char *const SQL_OPERATORS[] = {
    "=", "<>", "<", "<=", ">", ">=", "AND", "OR", "LIKE", "BETWEEN"};

static bool IsSQLOperator(const char *pszStr)
{
    return std::find_if(std::begin(SQL_OPERATORS), std::end(SQL_OPERATORS),
                        [pszStr](const char *pszStr2)
                        { return EQUAL(pszStr, pszStr2); }) !=
           std::end(SQL_OPERATORS);
}

static std::string GetSQLIdentifier(const std::string &name)
{
    if (name.find_first_of("'\" ") != std::string::npos)
    {
        char *pszEscaped = CPLEscapeString(name.c_str(), -1, CPLES_SQLI);
        std::string ret = std::string("\"").append(pszEscaped).append("\"");
        CPLFree(pszEscaped);
        return ret;
    }
    else
    {
        return name;
    }
}

static std::string GetSQLStringLiteral(const char *val)
{
    char *pszEscaped = CPLEscapeString(val, -1, CPLES_SQL);
    std::string ret = std::string("'").append(pszEscaped).append("'");
    CPLFree(pszEscaped);
    return ret;
}

std::vector<std::string>
GDALVectorFilterAlgorithm::CompleteWhere(const GDALAlgorithmArg &layerArg,
                                         const std::string &currentValue) const
{
    std::vector<std::string> ret;
    if (currentValue.empty() || currentValue[0] != '"' ||
        m_inputDataset.empty())
        return ret;

    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(m_inputDataset[0].GetName().c_str(),
                          GDAL_OF_VECTOR | GDAL_OF_READONLY));
    if (!poDS)
        return ret;

    // Collect field names
    std::string layerName;
    if (layerArg.IsExplicitlySet())
        layerName = layerArg.Get<std::string>();
    std::map<std::string, std::vector<OGRLayer *>> fieldNames;
    if (layerName.empty())
    {
        for (auto *poLayer : poDS->GetLayers())
        {
            for (const auto *poFieldDefn : poLayer->GetLayerDefn()->GetFields())
            {
                fieldNames[poFieldDefn->GetNameRef()].push_back(poLayer);
            }
        }
    }
    else if (auto *poLayer = poDS->GetLayerByName(layerName.c_str()))
    {
        for (const auto *poFieldDefn : poLayer->GetLayerDefn()->GetFields())
        {
            fieldNames[poFieldDefn->GetNameRef()].push_back(poLayer);
        }
    }
    if (fieldNames.empty())
        return ret;

    const CPLStringList aosTokens(CSLTokenizeString2(
        currentValue.c_str() + 1, " ",
        CSLT_HONOURSTRINGS | CSLT_HONOURSINGLEQUOTES | CSLT_PRESERVEQUOTES |
            CSLT_PRESERVEESCAPES | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));

    std::string prefix;
    const int nTokens = aosTokens.size();
    if (nTokens > 0 && cpl::contains(fieldNames, aosTokens[nTokens - 1]))
    {
        prefix = currentValue.substr(1);
        if (!prefix.empty() && prefix.back() != ' ')
            prefix += ' ';
        for (const char *op : SQL_OPERATORS)
        {
            if (nTokens > 1 && strcmp(op, "AND") == 0)
                break;
            ret.push_back(prefix + op);
        }
    }
    else
    {
        const bool bLastTokenIsSQLOperator =
            nTokens > 0 && IsSQLOperator(aosTokens[nTokens - 1]);
        const int nCompleteTokens =
            nTokens + (bLastTokenIsSQLOperator ? 0 : -1);
        for (int i = 0; i < nCompleteTokens; ++i)
        {
            if (!prefix.empty())
                prefix += ' ';
            prefix += aosTokens[i];
        }
        if (!prefix.empty())
            prefix += ' ';

        const char *pszLastFieldName = nullptr;
        OGRLayer *poLastFieldLayer = nullptr;
        OGRFieldType eLastFieldType = OFTString;
        if (nCompleteTokens >= 2)
        {
            const char *pszFieldName = aosTokens[nCompleteTokens - 2];
            const auto oIter = fieldNames.find(pszFieldName);
            if (oIter != fieldNames.end() && oIter->second.size() == 1)
            {
                const int nIdx =
                    oIter->second[0]->GetLayerDefn()->GetFieldIndex(
                        pszFieldName);
                if (nIdx >= 0)
                {
                    pszLastFieldName = pszFieldName;
                    poLastFieldLayer = oIter->second[0];
                    eLastFieldType = poLastFieldLayer->GetLayerDefn()
                                         ->GetFieldDefn(nIdx)
                                         ->GetType();
                }
            }
        }

        for (const auto &[name, layers] : fieldNames)
        {
            bool canAdd = false;
            if (!pszLastFieldName)
            {
                canAdd = true;
            }
            else if (name != pszLastFieldName)
            {
                if (layers.size() > 1)
                {
                    canAdd = true;
                }
                else if (layers[0]
                             ->GetLayerDefn()
                             ->GetFieldDefn(
                                 layers[0]->GetLayerDefn()->GetFieldIndex(
                                     name.c_str()))
                             ->GetType() == eLastFieldType)
                {
                    canAdd = true;
                }
            }
            if (canAdd)
            {
                ret.push_back(prefix + GetSQLIdentifier(name));
            }
        }

        if (pszLastFieldName && poLastFieldLayer)
        {
            auto poLayer = poLastFieldLayer;
            constexpr int NOT_TOO_LARGE = 1000;
            const auto nFeatureCount = poLayer->GetFeatureCount();
            if (nFeatureCount > 0 && nFeatureCount < NOT_TOO_LARGE)
            {
                constexpr int VALUES_COUNT = 10;
                const std::string osSQLField =
                    GetSQLIdentifier(pszLastFieldName);
                const std::string osSQLLayer =
                    GetSQLIdentifier(poLayer->GetName());
                if (eLastFieldType == OFTString)
                {
                    CPLString osSQL;
                    const char *pszDialect = nullptr;
                    if (GetGDALDriverManager()->GetDriverByName("SQLite"))
                    {
                        pszDialect = "SQLite";
                        // Find 10 most frequent strings
                        osSQL.Printf("SELECT %s, COUNT(%s) cnt FROM %s GROUP "
                                     "BY %s ORDER BY cnt DESC, %s ASC LIMIT %d",
                                     osSQLField.c_str(), osSQLField.c_str(),
                                     osSQLLayer.c_str(), osSQLField.c_str(),
                                     osSQLField.c_str(), VALUES_COUNT + 1);
                    }
                    else
                    {
                        osSQL.Printf("SELECT DISTINCT %s FROM %s LIMIT %d",
                                     osSQLField.c_str(), osSQLLayer.c_str(),
                                     VALUES_COUNT + 1);
                    }
                    auto poSQLLayer =
                        poDS->ExecuteSQL(osSQL.c_str(), nullptr, pszDialect);
                    if (poSQLLayer)
                    {
                        int nCount = 0;
                        for (auto &&poFeature : poSQLLayer)
                        {
                            if (nCount == VALUES_COUNT)
                            {
                                ret.push_back(prefix + "'...other values...");
                                break;
                            }
                            ret.push_back(prefix +
                                          GetSQLStringLiteral(
                                              poFeature->GetFieldAsString(0)));
                            nCount++;
                        }
                        poDS->ReleaseResultSet(poSQLLayer);
                    }
                }
                else if (eLastFieldType == OFTInteger ||
                         eLastFieldType == OFTInteger64 ||
                         eLastFieldType == OFTReal)
                {
                    CPLString osSQL;
                    const char *pszDialect = nullptr;
                    if (nFeatureCount > VALUES_COUNT + 2 &&
                        GetGDALDriverManager()->GetDriverByName("SQLite"))
                    {
                        pszDialect = "SQLite";
                        // Collect lowest and highest values
                        osSQL.Printf("SELECT DISTINCT %s FROM ("
                                     "SELECT * FROM (SELECT DISTINCT %s FROM "
                                     "%s ORDER BY %s ASC LIMIT %d) UNION ALL "
                                     "SELECT * FROM (SELECT DISTINCT %s FROM "
                                     "%s ORDER BY %s DESC LIMIT %d)"
                                     ") x ORDER BY %s",
                                     osSQLField.c_str(), osSQLField.c_str(),
                                     osSQLLayer.c_str(), osSQLField.c_str(),
                                     VALUES_COUNT / 2, osSQLField.c_str(),
                                     osSQLLayer.c_str(), osSQLField.c_str(),
                                     VALUES_COUNT / 2 + 1, osSQLField.c_str());
                    }
                    else
                    {
                        osSQL.Printf("SELECT DISTINCT %s FROM %s LIMIT %d",
                                     osSQLField.c_str(), osSQLLayer.c_str(),
                                     VALUES_COUNT + 1);
                    }
                    auto poSQLLayer =
                        poDS->ExecuteSQL(osSQL.c_str(), nullptr, pszDialect);
                    if (poSQLLayer)
                    {
                        int nCount = 0;
                        for (auto &&poFeature : poSQLLayer)
                        {
                            if (nCount == VALUES_COUNT)
                            {
                                if (pszDialect)
                                {
                                    ret.erase(ret.begin() + VALUES_COUNT / 2 +
                                              1);
                                    ret.push_back(
                                        prefix +
                                        poFeature->GetFieldAsString(0));
                                }
                                ret.push_back(prefix + "...other values...");
                                break;
                            }
                            ret.push_back(prefix +
                                          poFeature->GetFieldAsString(0));
                            nCount++;
                        }
                        poDS->ReleaseResultSet(poSQLLayer);
                    }
                }
            }
        }
    }
    return ret;
}

/************************************************************************/
/*              GDALVectorFilterAlgorithmLayerChangeExtent              */
/************************************************************************/

namespace
{
class GDALVectorFilterAlgorithmLayerChangeExtent final
    : public GDALVectorPipelinePassthroughLayer
{
  public:
    GDALVectorFilterAlgorithmLayerChangeExtent(
        OGRLayer &oSrcLayer, const OGREnvelope3D &sLayerEnvelope)
        : GDALVectorPipelinePassthroughLayer(oSrcLayer),
          m_sLayerEnvelope(sLayerEnvelope)
    {
    }

    OGRErr IGetExtent(int /*iGeomField*/, OGREnvelope *psExtent,
                      bool /* bForce */) override
    {
        if (m_sLayerEnvelope.IsInit())
        {
            *psExtent = m_sLayerEnvelope;
            return OGRERR_NONE;
        }
        else
        {
            return OGRERR_FAILURE;
        }
    }

    OGRErr IGetExtent3D(int /*iGeomField*/, OGREnvelope3D *psExtent,
                        bool /* bForce */) override
    {
        if (m_sLayerEnvelope.IsInit())
        {
            *psExtent = m_sLayerEnvelope;
            return OGRERR_NONE;
        }
        else
        {
            return OGRERR_FAILURE;
        }
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCFastGetExtent))
            return true;
        return m_srcLayer.TestCapability(pszCap);
    }

  private:
    const OGREnvelope3D m_sLayerEnvelope;
};

}  // namespace

/************************************************************************/
/*                 GDALVectorFilterAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALVectorFilterAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    const int nLayerCount = poSrcDS->GetLayerCount();

    bool ret = true;
    if (m_bbox.size() == 4)
    {
        const double xmin = m_bbox[0];
        const double ymin = m_bbox[1];
        const double xmax = m_bbox[2];
        const double ymax = m_bbox[3];
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (poSrcLayer && (m_activeLayer.empty() ||
                               m_activeLayer == poSrcLayer->GetDescription()))
                poSrcLayer->SetSpatialFilterRect(xmin, ymin, xmax, ymax);
        }
    }

    if (ret && !m_where.empty())
    {
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (ret && (m_activeLayer.empty() ||
                        m_activeLayer == poSrcLayer->GetDescription()))
            {
                ret = poSrcLayer->SetAttributeFilter(m_where.c_str()) ==
                      OGRERR_NONE;
            }
        }
    }

    if (ret)
    {
        auto outDS =
            std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

        int64_t nTotalFeatures = 0;
        if (m_updateExtent && ctxt.m_pfnProgress)
        {
            for (int i = 0; ret && i < nLayerCount; ++i)
            {
                auto poSrcLayer = poSrcDS->GetLayer(i);
                ret = (poSrcLayer != nullptr);
                if (ret)
                {
                    if (m_activeLayer.empty() ||
                        m_activeLayer == poSrcLayer->GetDescription())
                    {
                        if (poSrcLayer->TestCapability(OLCFastFeatureCount))
                        {
                            const auto nFC = poSrcLayer->GetFeatureCount(false);
                            if (nFC < 0)
                            {
                                nTotalFeatures = 0;
                                break;
                            }
                            nTotalFeatures += nFC;
                        }
                    }
                }
            }
        }

        int64_t nFeatureCounter = 0;
        for (int i = 0; ret && i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = (poSrcLayer != nullptr);
            if (ret)
            {
                if (m_updateExtent &&
                    (m_activeLayer.empty() ||
                     m_activeLayer == poSrcLayer->GetDescription()))
                {
                    OGREnvelope3D sLayerEnvelope, sFeatureEnvelope;
                    for (auto &&poFeature : poSrcLayer)
                    {
                        const auto poGeom = poFeature->GetGeometryRef();
                        if (poGeom && !poGeom->IsEmpty())
                        {
                            poGeom->getEnvelope(&sFeatureEnvelope);
                            sLayerEnvelope.Merge(sFeatureEnvelope);
                        }

                        ++nFeatureCounter;
                        if (nTotalFeatures > 0 && ctxt.m_pfnProgress &&
                            !ctxt.m_pfnProgress(
                                static_cast<double>(nFeatureCounter) /
                                    static_cast<double>(nTotalFeatures),
                                "", ctxt.m_pProgressData))
                        {
                            ReportError(CE_Failure, CPLE_UserInterrupt,
                                        "Interrupted by user");
                            return false;
                        }
                    }
                    outDS->AddLayer(
                        *poSrcLayer,
                        std::make_unique<
                            GDALVectorFilterAlgorithmLayerChangeExtent>(
                            *poSrcLayer, sLayerEnvelope));
                }
                else
                {
                    outDS->AddLayer(
                        *poSrcLayer,
                        std::make_unique<GDALVectorPipelinePassthroughLayer>(
                            *poSrcLayer));
                }
            }
        }

        if (ret)
            m_outputDataset.Set(std::move(outDS));
    }

    return ret;
}

GDALVectorFilterAlgorithmStandalone::~GDALVectorFilterAlgorithmStandalone() =
    default;

//! @endcond
