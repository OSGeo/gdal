/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "pansharpen" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_pansharpen.h"

#include "gdal_priv.h"
#include "cpl_minixml.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALRasterPansharpenAlgorithm::GetConstructorOptions()        */
/************************************************************************/

/* static */ GDALRasterPansharpenAlgorithm::ConstructorOptions
GDALRasterPansharpenAlgorithm::GetConstructorOptions(bool standaloneStep)
{
    ConstructorOptions opts;
    opts.SetStandaloneStep(standaloneStep);
    opts.SetAddDefaultArguments(false);
    opts.SetInputDatasetAlias("panchromatic");
    opts.SetInputDatasetHelpMsg(_("Input panchromatic raster dataset"));
    return opts;
}

/************************************************************************/
/*    GDALRasterPansharpenAlgorithm::GDALRasterPansharpenAlgorithm()    */
/************************************************************************/

GDALRasterPansharpenAlgorithm::GDALRasterPansharpenAlgorithm(
    bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      GetConstructorOptions(standaloneStep))
{
    const auto AddSpectralDatasetArg = [this]()
    {
        auto &arg = AddArg("spectral", 0, _("Input spectral band dataset"),
                           &m_spectralDatasets)
                        .SetPositional()
                        .SetRequired()
                        .SetMinCount(1)
                        // due to ",band=" comma syntax
                        .SetAutoOpenDataset(false)
                        // due to ",band=" comma syntax
                        .SetPackedValuesAllowed(false)
                        .SetMetaVar("SPECTRAL");

        SetAutoCompleteFunctionForFilename(arg, GDAL_OF_RASTER);
    };

    if (standaloneStep)
    {
        AddRasterInputArgs(false, false);
        AddSpectralDatasetArg();
        AddProgressArg();
        AddRasterOutputArgs(false);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
        AddSpectralDatasetArg();
    }

    AddArg("resampling", 'r', _("Resampling algorithm"), &m_resampling)
        .SetDefault(m_resampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average");
    AddArg("weights", 0, _("Weight for each input spectral band"), &m_weights);
    AddArg("nodata", 0, _("Override nodata value of input bands"), &m_nodata);
    AddArg("bit-depth", 0, _("Override bit depth of input bands"), &m_bitDepth)
        .SetMinValueIncluded(8);
    AddArg("spatial-extent-adjustment", 0,
           _("Select behavior when bands have not the same extent"),
           &m_spatialExtentAdjustment)
        .SetDefault(m_spatialExtentAdjustment)
        .SetChoices("union", "intersection", "none", "none-without-warning");
    AddNumThreadsArg(&m_numThreads, &m_numThreadsStr);
}

/************************************************************************/
/*               GDALRasterPansharpenAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterPansharpenAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poPanDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poPanDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (poPanDS->GetRasterCount() != 1)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Input panchromatic dataset must have a single band");
        return false;
    }

    // to keep in this scope to keep datasets of spectral bands open until
    // GDALCreatePansharpenedVRT() runs
    std::vector<std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser>>
        apoDatasetsToReleaseRef;
    std::vector<GDALRasterBandH> ahSpectralBands;

    for (auto &spectralDataset : m_spectralDatasets)
    {
        if (auto poSpectralDS = spectralDataset.GetDatasetRef())
        {
            for (int i = 1; i <= poSpectralDS->GetRasterCount(); ++i)
            {
                ahSpectralBands.push_back(
                    GDALRasterBand::ToHandle(poSpectralDS->GetRasterBand(i)));
            }
        }
        else
        {
            const auto &name = spectralDataset.GetName();
            std::string dsName(name);
            const auto pos = name.find(",band=");
            int iBand = 0;
            if (pos != std::string::npos)
            {
                dsName.resize(pos);
                iBand = atoi(name.c_str() + pos + strlen(",band="));
            }
            std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser> poDS(
                GDALDataset::Open(dsName.c_str(),
                                  GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
            if (!poDS)
                return false;

            if (iBand <= 0)
            {
                for (int i = 1; i <= poDS->GetRasterCount(); ++i)
                {
                    ahSpectralBands.push_back(
                        GDALRasterBand::ToHandle(poDS->GetRasterBand(i)));
                }
            }
            else if (iBand > poDS->GetRasterCount())
            {
                ReportError(CE_Failure, CPLE_IllegalArg, "Illegal band in '%s'",
                            name.c_str());
                return false;
            }
            else
            {
                ahSpectralBands.push_back(
                    GDALRasterBand::ToHandle(poDS->GetRasterBand(iBand)));
            }

            apoDatasetsToReleaseRef.push_back(std::move(poDS));
        }
    }

    CPLXMLTreeCloser root(CPLCreateXMLNode(nullptr, CXT_Element, "VRTDataset"));
    for (int i = 0; i < static_cast<int>(ahSpectralBands.size()); ++i)
    {
        auto psBandNode =
            CPLCreateXMLNode(root.get(), CXT_Element, "VRTRasterBand");
        CPLAddXMLAttributeAndValue(
            psBandNode, "dataType",
            GDALGetDataTypeName(GDALGetRasterDataType(ahSpectralBands[i])));
        CPLAddXMLAttributeAndValue(psBandNode, "band", CPLSPrintf("%d", i + 1));
        CPLAddXMLAttributeAndValue(psBandNode, "subClass",
                                   "VRTPansharpenedRasterBand");
    }
    CPLAddXMLAttributeAndValue(root.get(), "subClass",
                               "VRTPansharpenedDataset");
    auto psPansharpeningOptionsNode =
        CPLCreateXMLNode(root.get(), CXT_Element, "PansharpeningOptions");
    if (!m_weights.empty())
    {
        auto psAlgorithmOptionsNode = CPLCreateXMLNode(
            psPansharpeningOptionsNode, CXT_Element, "AlgorithmOptions");
        std::string osWeights;
        for (double w : m_weights)
        {
            if (!osWeights.empty())
                osWeights += ',';
            osWeights += CPLSPrintf("%.17g", w);
        }
        CPLCreateXMLElementAndValue(psAlgorithmOptionsNode, "Weights",
                                    osWeights.c_str());
    }
    CPLCreateXMLElementAndValue(psPansharpeningOptionsNode, "Resampling",
                                m_resampling.c_str());
    CPLCreateXMLElementAndValue(psPansharpeningOptionsNode, "NumThreads",
                                m_numThreadsStr.c_str());
    if (m_bitDepth > 0)
    {
        CPLCreateXMLElementAndValue(psPansharpeningOptionsNode, "BitDepth",
                                    CPLSPrintf("%d", m_bitDepth));
    }
    if (GetArg("nodata")->IsExplicitlySet())
    {
        CPLCreateXMLElementAndValue(psPansharpeningOptionsNode, "NoData",
                                    CPLSPrintf("%.17g", m_nodata));
    }
    CPLCreateXMLElementAndValue(
        psPansharpeningOptionsNode, "SpatialExtentAdjustment",
        CPLString(m_spatialExtentAdjustment).replaceAll('-', 0).c_str());
    for (int i = 0; i < static_cast<int>(ahSpectralBands.size()); ++i)
    {
        auto psSpectraBandNode = CPLCreateXMLNode(psPansharpeningOptionsNode,
                                                  CXT_Element, "SpectralBand");
        CPLAddXMLAttributeAndValue(psSpectraBandNode, "dstBand",
                                   CPLSPrintf("%d", i + 1));
    }
    CPLCharUniquePtr pszXML(CPLSerializeXMLTree(root.get()));
    auto poVRTDS = std::unique_ptr<GDALDataset>(
        GDALDataset::FromHandle(GDALCreatePansharpenedVRT(
            pszXML.get(), GDALRasterBand::ToHandle(poPanDS->GetRasterBand(1)),
            static_cast<int>(ahSpectralBands.size()), ahSpectralBands.data())));
    const bool bRet = poVRTDS != nullptr;
    if (poVRTDS)
    {
        m_outputDataset.Set(std::move(poVRTDS));
    }
    return bRet;
}

GDALRasterPansharpenAlgorithmStandalone::
    ~GDALRasterPansharpenAlgorithmStandalone() = default;

//! @endcond
