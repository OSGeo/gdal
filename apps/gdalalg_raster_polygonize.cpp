/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster polygonize" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_polygonize.h"
#include "gdalalg_vector_write.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*    GDALRasterPolygonizeAlgorithm::GDALRasterPolygonizeAlgorithm()    */
/************************************************************************/

GDALRasterPolygonizeAlgorithm::GDALRasterPolygonizeAlgorithm(
    bool standaloneStep)
    : GDALPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetAddUpsertArgument(false)
              .SetAddSkipErrorsArgument(false)
              .SetOutputLayerNameAvailableInPipelineStep(true)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE))
{
    if (standaloneStep)
    {
        AddProgressArg();
        AddRasterInputArgs(false, false);
        AddVectorOutputArgs(false, false);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
        AddOutputLayerNameArg(/* hiddenForCLI = */ false,
                              /* shortNameOutputLayerAllowed = */ false);
    }

    // gdal_polygonize specific options
    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("attribute-name", 0, _("Name of the field with the pixel value"),
           &m_attributeName)
        .SetDefault(m_attributeName);

    AddArg("connect-diagonal-pixels", 'c',
           _("Consider diagonal pixels as connected"), &m_connectDiagonalPixels)
        .SetDefault(m_connectDiagonalPixels);

    AddArg("commit-interval", 0, _("Commit interval"), &m_commitInterval)
        .SetHidden();
}

bool GDALRasterPolygonizeAlgorithm::CanHandleNextStep(
    GDALPipelineStepAlgorithm *poNextStep) const
{
    return poNextStep->GetName() == GDALVectorWriteAlgorithm::NAME &&
           poNextStep->GetOutputFormat() != "stream";
}

/************************************************************************/
/*               GDALRasterPolygonizeAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALRasterPolygonizeAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                            void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

/************************************************************************/
/*               GDALRasterPolygonizeAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterPolygonizeAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poWriteStep = ctxt.m_poNextUsableStep ? ctxt.m_poNextUsableStep : this;

    GDALDataset *poDstDS = nullptr;
    bool bTemporaryFile = false;
    std::unique_ptr<GDALDataset> poNewRetDS;
    std::string outputLayerName;
    OGRLayer *poDstLayer = nullptr;
    if (!CreateDatasetSingleOutputLayerIfNeeded(ctxt, "polygonize", poDstDS,
                                                bTemporaryFile, poNewRetDS,
                                                outputLayerName, poDstLayer))
    {
        return false;
    }

    auto poSrcBand = poSrcDS->GetRasterBand(m_band);
    const auto eDT = poSrcBand->GetRasterDataType();

    if (!poDstLayer)
    {
        poDstLayer = poDstDS->CreateLayer(
            outputLayerName.c_str(), poSrcDS->GetSpatialRef(), wkbPolygon,
            CPLStringList(poWriteStep->GetLayerCreationOptions()).List());
        if (!poDstLayer)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot create layer '%s'",
                        outputLayerName.c_str());
            return false;
        }

        OGRFieldDefn oFieldDefn(m_attributeName.c_str(),
                                !GDALDataTypeIsInteger(eDT) ? OFTReal
                                : eDT == GDT_Int64 || eDT == GDT_UInt64
                                    ? OFTInteger64
                                    : OFTInteger);
        if (poDstLayer->CreateField(&oFieldDefn) != OGRERR_NONE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot create field '%s' in layer '%s'",
                        m_attributeName.c_str(), outputLayerName.c_str());
            return false;
        }
    }

    const int iPixValField =
        poDstLayer->GetLayerDefn()->GetFieldIndex(m_attributeName.c_str());
    if (iPixValField < 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot find field '%s' in layer '%s'",
                    m_attributeName.c_str(), outputLayerName.c_str());
        return false;
    }

    CPLStringList aosPolygonizeOptions;
    if (m_connectDiagonalPixels)
    {
        aosPolygonizeOptions.SetNameValue("8CONNECTED", "8");
    }
    if (m_commitInterval)
    {
        aosPolygonizeOptions.SetNameValue("COMMIT_INTERVAL",
                                          CPLSPrintf("%d", m_commitInterval));
    }

    bool ret;
    if (GDALDataTypeIsInteger(eDT))
    {
        ret = GDALPolygonize(GDALRasterBand::ToHandle(poSrcBand),
                             GDALRasterBand::ToHandle(poSrcBand->GetMaskBand()),
                             OGRLayer::ToHandle(poDstLayer), iPixValField,
                             aosPolygonizeOptions.List(), ctxt.m_pfnProgress,
                             ctxt.m_pProgressData) == CE_None;
    }
    else
    {
        ret =
            GDALFPolygonize(GDALRasterBand::ToHandle(poSrcBand),
                            GDALRasterBand::ToHandle(poSrcBand->GetMaskBand()),
                            OGRLayer::ToHandle(poDstLayer), iPixValField,
                            aosPolygonizeOptions.List(), ctxt.m_pfnProgress,
                            ctxt.m_pProgressData) == CE_None;
    }

    if (ret && poNewRetDS)
    {
        if (bTemporaryFile)
        {
            ret = poNewRetDS->FlushCache() == CE_None;
#if !defined(__APPLE__)
            // For some unknown reason, unlinking the file on MacOSX
            // leads to later "disk I/O error". See https://github.com/OSGeo/gdal/issues/13794
            VSIUnlink(poNewRetDS->GetDescription());
#endif
        }

        m_outputDataset.Set(std::move(poNewRetDS));
    }

    return ret;
}

GDALRasterPolygonizeAlgorithmStandalone::
    ~GDALRasterPolygonizeAlgorithmStandalone() = default;

//! @endcond
