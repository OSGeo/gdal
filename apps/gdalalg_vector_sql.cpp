/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "sql" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_sql.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogrlayerpool.h"

#include <set>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*           GDALVectorSQLAlgorithm::GDALVectorSQLAlgorithm()           */
/************************************************************************/

GDALVectorSQLAlgorithm::GDALVectorSQLAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("sql", 0, _("SQL statement(s)"), &m_sql)
        .SetPositional()
        .SetRequired()
        .SetPackedValuesAllowed(false)
        .SetReadFromFileAtSyntaxAllowed()
        .SetMetaVar("<statement>|@<filename>")
        .SetRemoveSQLCommentsEnabled();
    AddArg("output-layer", standaloneStep ? 0 : 'l', _("Output layer name(s)"),
           &m_outputLayer);
    AddArg("dialect", 0, _("SQL dialect (e.g. OGRSQL, SQLITE)"), &m_dialect);
}

/************************************************************************/
/*                   GDALVectorSQLAlgorithmDataset                      */
/************************************************************************/

namespace
{
class GDALVectorSQLAlgorithmDataset final : public GDALDataset
{
    GDALDataset &m_oSrcDS;
    std::vector<OGRLayer *> m_layers{};

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSQLAlgorithmDataset)

  public:
    explicit GDALVectorSQLAlgorithmDataset(GDALDataset &oSrcDS)
        : m_oSrcDS(oSrcDS)
    {
    }

    ~GDALVectorSQLAlgorithmDataset() override
    {
        for (OGRLayer *poLayer : m_layers)
            m_oSrcDS.ReleaseResultSet(poLayer);
    }

    void AddLayer(OGRLayer *poLayer)
    {
        m_layers.push_back(poLayer);
    }

    int GetLayerCount() override
    {
        return static_cast<int>(m_layers.size());
    }

    OGRLayer *GetLayer(int idx) override
    {
        return idx >= 0 && idx < GetLayerCount() ? m_layers[idx] : nullptr;
    }
};
}  // namespace

/************************************************************************/
/*               GDALVectorSQLAlgorithmDatasetMultiLayer                */
/************************************************************************/

namespace
{

class ProxiedSQLLayer final : public OGRProxiedLayer
{
    OGRFeatureDefn *m_poLayerDefn = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(ProxiedSQLLayer)

  public:
    ProxiedSQLLayer(const std::string &osName, OGRLayerPool *poPoolIn,
                    OpenLayerFunc pfnOpenLayerIn,
                    ReleaseLayerFunc pfnReleaseLayerIn,
                    FreeUserDataFunc pfnFreeUserDataIn, void *pUserDataIn)
        : OGRProxiedLayer(poPoolIn, pfnOpenLayerIn, pfnReleaseLayerIn,
                          pfnFreeUserDataIn, pUserDataIn)
    {
        SetDescription(osName.c_str());
    }

    ~ProxiedSQLLayer()
    {
        if (m_poLayerDefn)
            m_poLayerDefn->Release();
    }

    const char *GetName() override
    {
        return GetDescription();
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        if (!m_poLayerDefn)
        {
            m_poLayerDefn = OGRProxiedLayer::GetLayerDefn()->Clone();
            m_poLayerDefn->SetName(GetDescription());
        }
        return m_poLayerDefn;
    }
};

class GDALVectorSQLAlgorithmDatasetMultiLayer final : public GDALDataset
{
    // We can't safely have 2 SQL layers active simultaneously on the same
    // source dataset. So each time we access one, we must close the last
    // active one.
    OGRLayerPool m_oPool{1};
    GDALDataset &m_oSrcDS;
    std::vector<std::unique_ptr<OGRLayer>> m_layers{};

    struct UserData
    {
        GDALDataset &oSrcDS;
        std::string osSQL{};
        std::string osDialect{};
        std::string osLayerName{};

        UserData(GDALDataset &oSrcDSIn, const std::string &osSQLIn,
                 const std::string &osDialectIn,
                 const std::string &osLayerNameIn)
            : oSrcDS(oSrcDSIn), osSQL(osSQLIn), osDialect(osDialectIn),
              osLayerName(osLayerNameIn)
        {
        }
        CPL_DISALLOW_COPY_ASSIGN(UserData)
    };

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSQLAlgorithmDatasetMultiLayer)

  public:
    explicit GDALVectorSQLAlgorithmDatasetMultiLayer(GDALDataset &oSrcDS)
        : m_oSrcDS(oSrcDS)
    {
    }

    void AddLayer(const std::string &osSQL, const std::string &osDialect,
                  const std::string &osLayerName)
    {
        const auto OpenLayer = [](void *pUserDataIn)
        {
            UserData *pUserData = static_cast<UserData *>(pUserDataIn);
            return pUserData->oSrcDS.ExecuteSQL(
                pUserData->osSQL.c_str(), nullptr,
                pUserData->osDialect.empty() ? nullptr
                                             : pUserData->osDialect.c_str());
        };

        const auto CloseLayer = [](OGRLayer *poLayer, void *pUserDataIn)
        {
            UserData *pUserData = static_cast<UserData *>(pUserDataIn);
            pUserData->oSrcDS.ReleaseResultSet(poLayer);
        };

        const auto DeleteUserData = [](void *pUserDataIn)
        { delete static_cast<UserData *>(pUserDataIn); };

        auto pUserData = new UserData(m_oSrcDS, osSQL, osDialect, osLayerName);
        auto poLayer = std::make_unique<ProxiedSQLLayer>(
            osLayerName, &m_oPool, OpenLayer, CloseLayer, DeleteUserData,
            pUserData);
        m_layers.push_back(std::move(poLayer));
    }

    int GetLayerCount() override
    {
        return static_cast<int>(m_layers.size());
    }

    OGRLayer *GetLayer(int idx) override
    {
        return idx >= 0 && idx < GetLayerCount() ? m_layers[idx].get()
                                                 : nullptr;
    }
};
}  // namespace

/************************************************************************/
/*                 GDALVectorSQLAlgorithm::RunStep()                    */
/************************************************************************/

bool GDALVectorSQLAlgorithm::RunStep(GDALProgressFunc, void *)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (!m_outputLayer.empty() && m_outputLayer.size() != m_sql.size())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "There should be as many layer names in --output-layer as "
                    "in --statement");
        return false;
    }

    if (m_sql.size() == 1)
    {
        auto outDS = std::make_unique<GDALVectorSQLAlgorithmDataset>(*poSrcDS);
        outDS->SetDescription(poSrcDS->GetDescription());

        const auto nErrorCounter = CPLGetErrorCounter();
        OGRLayer *poLayer = poSrcDS->ExecuteSQL(
            m_sql[0].c_str(), nullptr,
            m_dialect.empty() ? nullptr : m_dialect.c_str());
        if (!poLayer)
        {
            if (nErrorCounter == CPLGetErrorCounter())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Execution of the SQL statement '%s' did not "
                            "result in a result layer.",
                            m_sql[0].c_str());
            }
            return false;
        }

        if (!m_outputLayer.empty())
        {
            const std::string &osLayerName = m_outputLayer[0];
            poLayer->GetLayerDefn()->SetName(osLayerName.c_str());
            poLayer->SetDescription(osLayerName.c_str());
        }
        outDS->AddLayer(poLayer);
        m_outputDataset.Set(std::move(outDS));
    }
    else
    {
        // First pass to check all statements are valid and figure out layer
        // names
        std::set<std::string> setOutputLayerNames;
        std::vector<std::string> aosLayerNames;
        for (const std::string &sql : m_sql)
        {
            const auto nErrorCounter = CPLGetErrorCounter();
            auto poLayer = poSrcDS->ExecuteSQL(
                sql.c_str(), nullptr,
                m_dialect.empty() ? nullptr : m_dialect.c_str());
            if (!poLayer)
            {
                if (nErrorCounter == CPLGetErrorCounter())
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Execution of the SQL statement '%s' did not "
                                "result in a result layer.",
                                sql.c_str());
                }
                return false;
            }

            std::string osLayerName;

            if (!m_outputLayer.empty())
            {
                osLayerName = m_outputLayer[aosLayerNames.size()];
            }
            else if (cpl::contains(setOutputLayerNames,
                                   poLayer->GetDescription()))
            {
                int num = 1;
                do
                {
                    osLayerName = poLayer->GetDescription();
                    ++num;
                    osLayerName += std::to_string(num);
                } while (cpl::contains(setOutputLayerNames, osLayerName));
            }

            if (!osLayerName.empty())
            {
                poLayer->GetLayerDefn()->SetName(osLayerName.c_str());
                poLayer->SetDescription(osLayerName.c_str());
            }
            setOutputLayerNames.insert(poLayer->GetDescription());
            aosLayerNames.push_back(poLayer->GetDescription());

            poSrcDS->ReleaseResultSet(poLayer);
        }

        auto outDS =
            std::make_unique<GDALVectorSQLAlgorithmDatasetMultiLayer>(*poSrcDS);
        outDS->SetDescription(poSrcDS->GetDescription());

        for (size_t i = 0; i < aosLayerNames.size(); ++i)
        {
            outDS->AddLayer(m_sql[i], m_dialect, aosLayerNames[i]);
        }

        m_outputDataset.Set(std::move(outDS));
    }

    return true;
}

//! @endcond
