/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "read" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_read.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALVectorReadAlgorithm::GDALVectorReadAlgorithm()          */
/************************************************************************/

GDALVectorReadAlgorithm::GDALVectorReadAlgorithm()
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep =*/false)
{
    AddInputArgs(/* hiddenForCLI = */ false);
}

/************************************************************************/
/*                 GDALVectorPipelineReadOutputDataset                  */
/************************************************************************/

/** Class used by vector pipeline steps to create an output on-the-fly
 * dataset where they can store on-the-fly layers.
 */
class GDALVectorPipelineReadOutputDataset final : public GDALDataset
{
    GDALDataset &m_srcDS;
    std::vector<OGRLayer *> m_layers{};

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorPipelineReadOutputDataset)

  public:
    explicit GDALVectorPipelineReadOutputDataset(GDALDataset &oSrcDS);

    void AddLayer(OGRLayer &oSrcLayer);

    int GetLayerCount() override;

    OGRLayer *GetLayer(int idx) override;

    int TestCapability(const char *pszCap) override;

    void ResetReading() override;

    OGRFeature *GetNextFeature(OGRLayer **ppoBelongingLayer,
                               double *pdfProgressPct,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData) override;
};

/************************************************************************/
/*                 GDALVectorPipelineReadOutputDataset()                */
/************************************************************************/

GDALVectorPipelineReadOutputDataset::GDALVectorPipelineReadOutputDataset(
    GDALDataset &srcDS)
    : m_srcDS(srcDS)
{
    SetDescription(m_srcDS.GetDescription());
}

/************************************************************************/
/*            GDALVectorPipelineReadOutputDataset::AddLayer()           */
/************************************************************************/

void GDALVectorPipelineReadOutputDataset::AddLayer(OGRLayer &oSrcLayer)
{
    m_layers.push_back(&oSrcLayer);
}

/************************************************************************/
/*          GDALVectorPipelineReadOutputDataset::GetLayerCount()        */
/************************************************************************/

int GDALVectorPipelineReadOutputDataset::GetLayerCount()
{
    return static_cast<int>(m_layers.size());
}

/************************************************************************/
/*           GDALVectorPipelineReadOutputDataset::GetLayer()            */
/************************************************************************/

OGRLayer *GDALVectorPipelineReadOutputDataset::GetLayer(int idx)
{
    return idx >= 0 && idx < GetLayerCount() ? m_layers[idx] : nullptr;
}

/************************************************************************/
/*         GDALVectorPipelineReadOutputDataset::TestCapability()        */
/************************************************************************/

int GDALVectorPipelineReadOutputDataset::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCRandomLayerRead))
        return m_srcDS.TestCapability(pszCap);
    return false;
}

/************************************************************************/
/*           GDALVectorPipelineReadOutputDataset::ResetReading()        */
/************************************************************************/

void GDALVectorPipelineReadOutputDataset::ResetReading()
{
    m_srcDS.ResetReading();
}

/************************************************************************/
/*          GDALVectorPipelineReadOutputDataset::GetNextFeature()       */
/************************************************************************/

OGRFeature *GDALVectorPipelineReadOutputDataset::GetNextFeature(
    OGRLayer **ppoBelongingLayer, double *pdfProgressPct,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    while (true)
    {
        OGRLayer *poBelongingLayer = nullptr;
        auto poFeature = std::unique_ptr<OGRFeature>(m_srcDS.GetNextFeature(
            &poBelongingLayer, pdfProgressPct, pfnProgress, pProgressData));
        if (ppoBelongingLayer)
            *ppoBelongingLayer = poBelongingLayer;
        if (!poFeature)
            break;
        if (std::find(m_layers.begin(), m_layers.end(), poBelongingLayer) !=
            m_layers.end())
            return poFeature.release();
    }
    return nullptr;
}

/************************************************************************/
/*                  GDALVectorReadAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALVectorReadAlgorithm::RunStep(GDALProgressFunc, void *)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_inputLayerNames.empty())
    {
        m_outputDataset.Set(poSrcDS);
    }
    else
    {
        auto poOutDS =
            std::make_unique<GDALVectorPipelineReadOutputDataset>(*poSrcDS);
        for (const auto &srcLayerName : m_inputLayerNames)
        {
            auto poSrcLayer = poSrcDS->GetLayerByName(srcLayerName.c_str());
            if (!poSrcLayer)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot find source layer '%s'",
                            srcLayerName.c_str());
                return false;
            }
            poOutDS->AddLayer(*poSrcLayer);
        }
        m_outputDataset.Set(std::move(poOutDS));
    }
    return true;
}

//! @endcond
