/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector concat" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_concat.h"
#include "gdalalg_vector_write.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "ogrsf_frmts.h"

#include "ogrlayerdecorator.h"
#include "ogrunionlayer.h"
#include "ogrwarpedlayer.h"

#include <algorithm>
#include <set>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALVectorConcatAlgorithm::GDALVectorConcatAlgorithm()        */
/************************************************************************/

GDALVectorConcatAlgorithm::GDALVectorConcatAlgorithm(bool bStandalone)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL, bStandalone)
{
    if (!bStandalone)
    {
        AddInputArgs(/* hiddenForCLI = */ false);
    }

    AddArg(
        "mode", 0,
        _("Determine the strategy to create output layers from source layers "),
        &m_mode)
        .SetChoices("merge-per-layer-name", "stack", "single")
        .SetDefault(m_mode);
    AddArg("output-layer", 0,
           _("Name of the output vector layer (single mode), or template to "
             "name the output vector layers (stack mode)"),
           &m_layerNameTemplate);
    AddArg("source-layer-field-name", 0,
           _("Name of the new field to add to contain identificoncation of the "
             "source layer, with value determined from "
             "'source-layer-field-content'"),
           &m_sourceLayerFieldName);
    AddArg("source-layer-field-content", 0,
           _("A string, possibly using {AUTO_NAME}, {DS_NAME}, {DS_BASENAME}, "
             "{DS_INDEX}, {LAYER_NAME}, {LAYER_INDEX}"),
           &m_sourceLayerFieldContent);
    AddArg("field-strategy", 0,
           _("How to determine target fields from source fields"),
           &m_fieldStrategy)
        .SetChoices("union", "intersection")
        .SetDefault(m_fieldStrategy);
    AddArg("src-crs", 's', _("Source CRS"), &m_srsCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("s_srs");
    AddArg("dst-crs", 'd', _("Destination CRS"), &m_dstCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("t_srs");
}

GDALVectorConcatAlgorithm::~GDALVectorConcatAlgorithm() = default;

/************************************************************************/
/*                   GDALVectorConcatOutputDataset                      */
/************************************************************************/

class GDALVectorConcatOutputDataset final : public GDALDataset
{
    std::vector<std::unique_ptr<OGRLayer>> m_layers{};

  public:
    GDALVectorConcatOutputDataset() = default;

    void AddLayer(std::unique_ptr<OGRLayer> layer)
    {
        m_layers.push_back(std::move(layer));
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

    int TestCapability(const char *pszCap) override
    {
        if (EQUAL(pszCap, ODsCCurveGeometries) ||
            EQUAL(pszCap, ODsCMeasuredGeometries) ||
            EQUAL(pszCap, ODsCZGeometries))
        {
            return true;
        }
        return false;
    }
};

/************************************************************************/
/*                     GDALVectorConcatRenamedLayer                     */
/************************************************************************/

class GDALVectorConcatRenamedLayer final : public OGRLayerDecorator
{
  public:
    GDALVectorConcatRenamedLayer(OGRLayer *poSrcLayer,
                                 const std::string &newName)
        : OGRLayerDecorator(poSrcLayer, false), m_newName(newName)
    {
    }

    const char *GetName() override
    {
        return m_newName.c_str();
    }

  private:
    const std::string m_newName;
};

/************************************************************************/
/*                         BuildLayerName()                             */
/************************************************************************/

static std::string BuildLayerName(const std::string &layerNameTemplate,
                                  int dsIdx, const std::string &dsName,
                                  int lyrIdx, const std::string &lyrName)
{
    CPLString ret = layerNameTemplate;
    std::string baseName;
    VSIStatBufL sStat;
    if (VSIStatL(dsName.c_str(), &sStat) == 0)
        baseName = CPLGetBasenameSafe(dsName.c_str());

    if (baseName == lyrName)
    {
        ret = ret.replaceAll("{AUTO_NAME}", baseName);
    }
    else
    {
        ret = ret.replaceAll("{AUTO_NAME}",
                             std::string(baseName.empty() ? dsName : baseName)
                                 .append("_")
                                 .append(lyrName));
    }

    ret =
        ret.replaceAll("{DS_BASENAME}", !baseName.empty() ? baseName : dsName);
    ret = ret.replaceAll("{DS_NAME}", dsName);
    ret = ret.replaceAll("{DS_INDEX}", CPLSPrintf("%d", dsIdx));
    ret = ret.replaceAll("{LAYER_NAME}", lyrName);
    ret = ret.replaceAll("{LAYER_INDEX}", CPLSPrintf("%d", lyrIdx));

    return std::string(std::move(ret));
}

namespace
{

/************************************************************************/
/*                            OpenProxiedLayer()                        */
/************************************************************************/

struct PooledInitData
{
    std::unique_ptr<GDALDataset> poDS{};
    std::string osDatasetName{};
    std::vector<std::string> *pInputFormats = nullptr;
    std::vector<std::string> *pOpenOptions = nullptr;
    int iLayer = 0;
};

static OGRLayer *OpenProxiedLayer(void *pUserData)
{
    PooledInitData *pData = static_cast<PooledInitData *>(pUserData);
    pData->poDS.reset(GDALDataset::Open(
        pData->osDatasetName.c_str(), GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
        pData->pInputFormats ? CPLStringList(*(pData->pInputFormats)).List()
                             : nullptr,
        pData->pOpenOptions ? CPLStringList(*(pData->pOpenOptions)).List()
                            : nullptr,
        nullptr));
    if (!pData->poDS)
        return nullptr;
    return pData->poDS->GetLayer(pData->iLayer);
}

/************************************************************************/
/*                         ReleaseProxiedLayer()                        */
/************************************************************************/

static void ReleaseProxiedLayer(OGRLayer *, void *pUserData)
{
    PooledInitData *pData = static_cast<PooledInitData *>(pUserData);
    pData->poDS.reset();
}

/************************************************************************/
/*                        FreeProxiedLayerUserData()                    */
/************************************************************************/

static void FreeProxiedLayerUserData(void *pUserData)
{
    delete static_cast<PooledInitData *>(pUserData);
}

}  // namespace

/************************************************************************/
/*                   GDALVectorConcatAlgorithm::RunStep()               */
/************************************************************************/

bool GDALVectorConcatAlgorithm::RunStep(GDALProgressFunc, void *)
{
    std::unique_ptr<OGRSpatialReference> poSrcCRS;
    if (!m_srsCrs.empty())
    {
        poSrcCRS = std::make_unique<OGRSpatialReference>();
        poSrcCRS->SetFromUserInput(m_srsCrs.c_str());
        poSrcCRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    OGRSpatialReference oDstCRS;
    if (!m_dstCrs.empty())
    {
        oDstCRS.SetFromUserInput(m_dstCrs.c_str());
        oDstCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    struct LayerDesc
    {
        int iDS = 0;
        int iLayer = 0;
        std::string osDatasetName{};
    };

    if (m_layerNameTemplate.empty())
    {
        if (m_mode == "single")
            m_layerNameTemplate = "merged";
        else if (m_mode == "stack")
            m_layerNameTemplate = "{AUTO_NAME}";
    }
    else if (m_mode == "merge-per-layer-name")
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "'layer-name' name argument cannot be specified in "
                    "mode=merge-per-layer-name");
        return false;
    }

    if (m_sourceLayerFieldContent.empty())
        m_sourceLayerFieldContent = "{AUTO_NAME}";
    else if (m_sourceLayerFieldName.empty())
        m_sourceLayerFieldName = "source_ds_lyr";

    const int nMaxSimultaneouslyOpened =
        std::max(atoi(CPLGetConfigOption(
                     "GDAL_VECTOR_CONCAT_MAX_OPENED_DATASETS", "100")),
                 1);

    // First pass on input layers
    std::map<std::string, std::vector<LayerDesc>> allLayerNames;
    int iDS = 0;
    int countNonOpenedDS = 0;
    for (auto &srcDS : m_inputDataset)
    {
        GDALDataset *poSrcDS = srcDS.GetDatasetRef();
        std::unique_ptr<GDALDataset> poTmpDS;
        if (!poSrcDS)
        {
            poTmpDS.reset(GDALDataset::Open(
                srcDS.GetName().c_str(), GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
                CPLStringList(m_inputFormats).List(),
                CPLStringList(m_openOptions).List(), nullptr));
            poSrcDS = poTmpDS.get();
            if (!poSrcDS)
                return false;
            if (static_cast<int>(m_inputDataset.size()) <=
                nMaxSimultaneouslyOpened)
            {
                srcDS.Set(std::move(poTmpDS));
                poSrcDS = srcDS.GetDatasetRef();
            }
            else
            {
                ++countNonOpenedDS;
            }
        }

        int iLayer = 0;
        for (const auto &poLayer : poSrcDS->GetLayers())
        {
            if (m_inputLayerNames.empty() ||
                std::find(m_inputLayerNames.begin(), m_inputLayerNames.end(),
                          poLayer->GetName()) != m_inputLayerNames.end())
            {
                if (!m_dstCrs.empty() && m_srsCrs.empty() &&
                    poLayer->GetSpatialRef() == nullptr)
                {
                    ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "Layer '%s' of '%s' has no spatial reference system",
                        poLayer->GetName(), poSrcDS->GetDescription());
                    return false;
                }
                LayerDesc layerDesc;
                layerDesc.iDS = iDS;
                layerDesc.iLayer = iLayer;
                layerDesc.osDatasetName = poSrcDS->GetDescription();
                const std::string outLayerName =
                    m_mode == "single" ? m_layerNameTemplate
                    : m_mode == "merge-per-layer-name"
                        ? std::string(poLayer->GetName())
                        : BuildLayerName(m_layerNameTemplate, iDS,
                                         poSrcDS->GetDescription(), iLayer,
                                         poLayer->GetName());
                CPLDebugOnly("gdal_vector_concat", "%s,%s->%s",
                             poSrcDS->GetDescription(), poLayer->GetName(),
                             outLayerName.c_str());
                allLayerNames[outLayerName].push_back(std::move(layerDesc));
            }
            ++iLayer;
        }
        ++iDS;
    }

    auto poUnionDS = std::make_unique<GDALVectorConcatOutputDataset>();

    if (countNonOpenedDS > nMaxSimultaneouslyOpened)
        m_poLayerPool =
            std::make_unique<OGRLayerPool>(nMaxSimultaneouslyOpened);

    bool ret = true;
    for (const auto &[outLayerName, listOfLayers] : allLayerNames)
    {
        const int nLayerCount = static_cast<int>(listOfLayers.size());
        std::unique_ptr<OGRLayer *, VSIFreeReleaser> papoSrcLayers(
            static_cast<OGRLayer **>(
                CPLCalloc(nLayerCount, sizeof(OGRLayer *))));
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto &srcDS = m_inputDataset[listOfLayers[i].iDS];
            GDALDataset *poSrcDS = srcDS.GetDatasetRef();
            std::unique_ptr<GDALDataset> poTmpDS;
            if (!poSrcDS)
            {
                poTmpDS.reset(GDALDataset::Open(
                    listOfLayers[i].osDatasetName.c_str(),
                    GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
                    CPLStringList(m_inputFormats).List(),
                    CPLStringList(m_openOptions).List(), nullptr));
                poSrcDS = poTmpDS.get();
                if (!poSrcDS)
                    return false;
            }
            OGRLayer *poSrcLayer = poSrcDS->GetLayer(listOfLayers[i].iLayer);

            if (m_poLayerPool)
            {
                auto pData = std::make_unique<PooledInitData>();
                pData->osDatasetName = listOfLayers[i].osDatasetName;
                pData->pInputFormats = &m_inputFormats;
                pData->pOpenOptions = &m_openOptions;
                pData->iLayer = listOfLayers[i].iLayer;
                auto proxiedLayer = std::make_unique<OGRProxiedLayer>(
                    m_poLayerPool.get(), OpenProxiedLayer, ReleaseProxiedLayer,
                    FreeProxiedLayerUserData, pData.release());
                proxiedLayer->SetDescription(poSrcLayer->GetDescription());
                m_tempLayersKeeper.push_back(std::move(proxiedLayer));
                poSrcLayer = m_tempLayersKeeper.back().get();
            }
            else if (poTmpDS)
            {
                srcDS.Set(std::move(poTmpDS));
            }

            if (m_sourceLayerFieldName.empty())
            {
                papoSrcLayers.get()[i] = poSrcLayer;
            }
            else
            {
                const std::string newSrcLayerName = BuildLayerName(
                    m_sourceLayerFieldContent, listOfLayers[i].iDS,
                    listOfLayers[i].osDatasetName.c_str(),
                    listOfLayers[i].iLayer, poSrcLayer->GetName());
                ret = !newSrcLayerName.empty() && ret;
                auto poTmpLayer =
                    std::make_unique<GDALVectorConcatRenamedLayer>(
                        poSrcLayer, newSrcLayerName);
                m_tempLayersKeeper.push_back(std::move(poTmpLayer));
                papoSrcLayers.get()[i] = m_tempLayersKeeper.back().get();
            }
        }

        // Auto-wrap source layers if needed
        if (!m_dstCrs.empty())
        {
            for (int i = 0; i < nLayerCount; ++i)
            {
                OGRSpatialReference *poSrcLayerCRS;
                if (poSrcCRS)
                    poSrcLayerCRS = poSrcCRS.get();
                else
                    poSrcLayerCRS = papoSrcLayers.get()[i]->GetSpatialRef();
                if (!poSrcLayerCRS->IsSame(&oDstCRS))
                {
                    auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                        OGRCreateCoordinateTransformation(poSrcLayerCRS,
                                                          &oDstCRS));
                    auto poReversedCT =
                        std::unique_ptr<OGRCoordinateTransformation>(
                            OGRCreateCoordinateTransformation(&oDstCRS,
                                                              poSrcLayerCRS));
                    ret = (poCT != nullptr) && (poReversedCT != nullptr);
                    if (ret)
                    {
                        m_tempLayersKeeper.push_back(
                            std::make_unique<OGRWarpedLayer>(
                                papoSrcLayers.get()[i], /* iGeomField = */ 0,
                                /*bTakeOwnership = */ false, poCT.release(),
                                poReversedCT.release()));
                        papoSrcLayers.get()[i] =
                            m_tempLayersKeeper.back().get();
                    }
                }
            }
        }

        auto poUnionLayer = std::make_unique<OGRUnionLayer>(
            outLayerName.c_str(), nLayerCount, papoSrcLayers.release(),
            /* bTakeLayerOwnership = */ false);

        if (!m_sourceLayerFieldName.empty())
        {
            poUnionLayer->SetSourceLayerFieldName(
                m_sourceLayerFieldName.c_str());
        }

        const FieldUnionStrategy eStrategy =
            m_fieldStrategy == "union" ? FIELD_UNION_ALL_LAYERS
                                       : FIELD_INTERSECTION_ALL_LAYERS;
        poUnionLayer->SetFields(eStrategy, 0, nullptr, 0, nullptr);

        poUnionDS->AddLayer(std::move(poUnionLayer));
    }

    if (ret)
    {
        m_outputDataset.Set(std::move(poUnionDS));
    }
    return ret;
}

/************************************************************************/
/*                GDALVectorConcatAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALVectorConcatAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                        void *pProgressData)
{
    if (m_standaloneStep)
    {
        GDALVectorWriteAlgorithm writeAlg;
        for (auto &arg : writeAlg.GetArgs())
        {
            if (arg->GetName() != "output-layer")
            {
                auto stepArg = GetArg(arg->GetName());
                if (stepArg && stepArg->IsExplicitlySet())
                {
                    arg->SetSkipIfAlreadySet(true);
                    arg->SetFrom(*stepArg);
                }
            }
        }

        // Already checked by GDALAlgorithm::Run()
        CPLAssert(!m_executionForStreamOutput ||
                  EQUAL(m_format.c_str(), "stream"));

        m_standaloneStep = false;
        bool ret = Run(pfnProgress, pProgressData);
        m_standaloneStep = true;
        if (ret)
        {
            if (m_format == "stream")
            {
                ret = true;
            }
            else
            {
                writeAlg.m_inputDataset.clear();
                writeAlg.m_inputDataset.resize(1);
                writeAlg.m_inputDataset[0].Set(m_outputDataset.GetDatasetRef());
                if (writeAlg.Run(pfnProgress, pProgressData))
                {
                    m_outputDataset.Set(
                        writeAlg.m_outputDataset.GetDatasetRef());
                    ret = true;
                }
            }
        }

        return ret;
    }
    else
    {
        return RunStep(pfnProgress, pProgressData);
    }
}

//! @endcond
