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

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*     GDALRasterPolygonizeAlgorithm::GDALRasterPolygonizeAlgorithm()   */
/************************************************************************/

GDALRasterPolygonizeAlgorithm::GDALRasterPolygonizeAlgorithm(
    bool standaloneStep)
    : GDALPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE))
{
    m_outputLayerName = "polygonize";

    AddProgressArg();
    if (standaloneStep)
    {
        AddOutputFormatArg(&m_format).AddMetadataItem(
            GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
        AddOpenOptionsArg(&m_openOptions);
        AddInputFormatsArg(&m_inputFormats)
            .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
        AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER);
        AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR)
            .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
        AddCreationOptionsArg(&m_creationOptions);
        AddLayerCreationOptionsArg(&m_layerCreationOptions);
        AddOverwriteArg(&m_overwrite);
        AddUpdateArg(&m_update);
        AddOverwriteLayerArg(&m_overwriteLayer);
        AddAppendLayerArg(&m_appendLayer);
        AddOutputLayerNameArg(&m_outputLayerName)
            .SetDefault(m_outputLayerName)
            .AddAlias("nln");
    }

    // gdal_polygonize specific options
    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("attribute-name", 0, _("Name of the field with the pixel value"),
           &m_attributeName)
        .SetDefault(m_attributeName);

    AddArg("connect-diagonal-pixels", 'c',
           _("Consider diagonal pixels as connected"), &m_connectDiagonalPixels)
        .SetDefault(m_connectDiagonalPixels);
}

/************************************************************************/
/*                GDALRasterPolygonizeAlgorithm::RunImpl()              */
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
/*                GDALRasterPolygonizeAlgorithm::RunStep()              */
/************************************************************************/

bool GDALRasterPolygonizeAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    GDALDataset *poDstDS = m_outputDataset.GetDatasetRef();
    std::unique_ptr<GDALDataset> poRetDS;
    std::string outputFilename = m_outputDataset.GetName();
    if (!poDstDS)
    {
        if (m_standaloneStep)
        {
            if (m_format.empty())
            {
                const auto aosFormats =
                    CPLStringList(GDALGetOutputDriversForDatasetName(
                        m_outputDataset.GetName().c_str(), GDAL_OF_VECTOR,
                        /* bSingleMatch = */ true,
                        /* bWarn = */ true));
                if (aosFormats.size() != 1)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot guess driver for %s",
                                m_outputDataset.GetName().c_str());
                    return false;
                }
                m_format = aosFormats[0];
            }
        }
        else
        {
            if (GetGDALDriverManager()->GetDriverByName("GPKG"))
            {
                outputFilename =
                    CPLGenerateTempFilenameSafe("_polygonize") + ".gpkg";
                m_format = "GPKG";
            }
            else
                m_format = "MEM";
        }

        auto poDriver =
            GetGDALDriverManager()->GetDriverByName(m_format.c_str());
        if (!poDriver)
        {
            // shouldn't happen given checks done in GDALAlgorithm
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot find driver %s",
                        m_format.c_str());
            return false;
        }

        poRetDS.reset(
            poDriver->Create(outputFilename.c_str(), 0, 0, 0, GDT_Unknown,
                             CPLStringList(m_creationOptions).List()));
        if (!poRetDS)
            return false;

        if (!m_standaloneStep && !outputFilename.empty())
            poRetDS->MarkSuppressOnClose();

        poDstDS = poRetDS.get();
    }

    auto poDstDriver = poDstDS->GetDriver();
    if (poDstDriver && EQUAL(poDstDriver->GetDescription(), "ESRI Shapefile") &&
        EQUAL(CPLGetExtensionSafe(poDstDS->GetDescription()).c_str(), "shp") &&
        poDstDS->GetLayerCount() <= 1)
    {
        m_outputLayerName = CPLGetBasenameSafe(poDstDS->GetDescription());
    }

    auto poDstLayer = poDstDS->GetLayerByName(m_outputLayerName.c_str());
    if (poDstLayer)
    {
        if (m_overwriteLayer)
        {
            int iLayer = -1;
            const int nLayerCount = poDstDS->GetLayerCount();
            for (iLayer = 0; iLayer < nLayerCount; iLayer++)
            {
                if (poDstDS->GetLayer(iLayer) == poDstLayer)
                    break;
            }

            if (iLayer < nLayerCount)
            {
                if (poDstDS->DeleteLayer(iLayer) != OGRERR_NONE)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot delete layer '%s'",
                                m_outputLayerName.c_str());
                    return false;
                }
            }
            poDstLayer = nullptr;
        }
        else if (!m_appendLayer)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Layer '%s' already exists. Specify the "
                        "--%s option to overwrite it, or --%s "
                        "to append to it.",
                        m_outputLayerName.c_str(),
                        GDAL_ARG_NAME_OVERWRITE_LAYER, GDAL_ARG_NAME_APPEND);
            return false;
        }
    }
    else if (m_appendLayer || m_overwriteLayer)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot find layer '%s'",
                    m_outputLayerName.c_str());
        return false;
    }

    auto poSrcBand = poSrcDS->GetRasterBand(m_band);
    const auto eDT = poSrcBand->GetRasterDataType();

    if (!poDstLayer)
    {
        poDstLayer = poDstDS->CreateLayer(
            m_outputLayerName.c_str(), poSrcDS->GetSpatialRef(), wkbPolygon,
            CPLStringList(m_layerCreationOptions).List());
        if (!poDstLayer)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot create layer '%s'",
                        m_outputLayerName.c_str());
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
                        m_attributeName.c_str(), m_outputLayerName.c_str());
            return false;
        }
    }

    const int iPixValField =
        poDstLayer->GetLayerDefn()->GetFieldIndex(m_attributeName.c_str());
    if (iPixValField < 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot find field '%s' in layer '%s'",
                    m_attributeName.c_str(), m_outputLayerName.c_str());
        return false;
    }

    CPLStringList aosPolygonizeOptions;
    if (m_connectDiagonalPixels)
    {
        aosPolygonizeOptions.SetNameValue("8CONNECTED", "8");
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

    if (ret && poRetDS)
    {
        if (!m_standaloneStep && !outputFilename.empty())
        {
            ret = poRetDS->FlushCache() == CE_None;
            VSIUnlink(outputFilename.c_str());
        }

        m_outputDataset.Set(std::move(poRetDS));
    }

    return ret;
}

GDALRasterPolygonizeAlgorithmStandalone::
    ~GDALRasterPolygonizeAlgorithmStandalone() = default;

//! @endcond
