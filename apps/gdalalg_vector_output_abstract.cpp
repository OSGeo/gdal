/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Class to abstract outputting to a vector layer
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_output_abstract.h"

#include "cpl_vsi.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*      GDALVectorOutputAbstractAlgorithm::AddAllOutputArgs()           */
/************************************************************************/

void GDALVectorOutputAbstractAlgorithm::AddAllOutputArgs()
{
    AddOutputFormatArg(&m_outputFormat)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    AddCreationOptionsArg(&m_creationOptions);
    AddLayerCreationOptionsArg(&m_layerCreationOptions);
    AddOverwriteArg(&m_overwrite).SetMutualExclusionGroup("overwrite-update");
    AddUpdateArg(&m_update).SetMutualExclusionGroup("overwrite-update");
    AddArg("overwrite-layer", 0,
           _("Whether overwriting existing layer is allowed"),
           &m_overwriteLayer)
        .SetDefault(false)
        .AddValidationAction(
            [this]
            {
                GetArg(GDAL_ARG_NAME_UPDATE)->Set(true);
                return true;
            });
    AddArg("append", 0, _("Whether appending to existing layer is allowed"),
           &m_appendLayer)
        .SetDefault(false)
        .AddValidationAction(
            [this]
            {
                GetArg(GDAL_ARG_NAME_UPDATE)->Set(true);
                return true;
            });
    {
        auto &arg = AddLayerNameArg(&m_outputLayerName)
                        .AddAlias("nln")
                        .SetMinCharCount(0);
        if (!m_outputLayerName.empty())
            arg.SetDefault(m_outputLayerName);
    }
}

/************************************************************************/
/*         GDALVectorOutputAbstractAlgorithm::SetupOutputDataset()      */
/************************************************************************/

GDALVectorOutputAbstractAlgorithm::SetupOutputDatasetRet
GDALVectorOutputAbstractAlgorithm::SetupOutputDataset()
{
    SetupOutputDatasetRet ret;

    GDALDataset *poDstDS = m_outputDataset.GetDatasetRef();
    std::unique_ptr<GDALDataset> poRetDS;
    if (!poDstDS)
    {
        if (m_outputFormat.empty())
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
                return ret;
            }
            m_outputFormat = aosFormats[0];
        }

        auto poDriver =
            GetGDALDriverManager()->GetDriverByName(m_outputFormat.c_str());
        if (!poDriver)
        {
            // shouldn't happen given checks done in GDALAlgorithm
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot find driver %s",
                        m_outputFormat.c_str());
            return ret;
        }

        poRetDS.reset(poDriver->Create(
            m_outputDataset.GetName().c_str(), 0, 0, 0, GDT_Unknown,
            CPLStringList(m_creationOptions).List()));
        if (!poRetDS)
            return ret;

        poDstDS = poRetDS.get();
    }

    auto poDstDriver = poDstDS->GetDriver();
    if (poDstDriver && EQUAL(poDstDriver->GetDescription(), "ESRI Shapefile") &&
        EQUAL(CPLGetExtensionSafe(poDstDS->GetDescription()).c_str(), "shp") &&
        poDstDS->GetLayerCount() <= 1)
    {
        m_outputLayerName = CPLGetBasenameSafe(poDstDS->GetDescription());
    }

    auto poDstLayer = m_outputLayerName.empty()
                          ? nullptr
                          : poDstDS->GetLayerByName(m_outputLayerName.c_str());
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
                    return ret;
                }
            }
            poDstLayer = nullptr;
        }
        else if (!m_appendLayer)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Layer '%s' already exists. Specify the "
                        "--overwrite-layer option to overwrite it, or --append "
                        "to append to it.",
                        m_outputLayerName.c_str());
            return ret;
        }
    }
    else if (m_appendLayer || m_overwriteLayer)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot find layer '%s'",
                    m_outputLayerName.c_str());
        return ret;
    }

    ret.newDS = std::move(poRetDS);
    ret.outDS = poDstDS;
    ret.layer = poDstLayer;
    return ret;
}

/************************************************************************/
/* GDALVectorOutputAbstractAlgorithm::SetDefaultOutputLayerNameIfNeeded */
/************************************************************************/

bool GDALVectorOutputAbstractAlgorithm::SetDefaultOutputLayerNameIfNeeded(
    GDALDataset *poOutDS)
{
    if (m_outputLayerName.empty())
    {
        VSIStatBufL sStat;
        auto poDriver = poOutDS->GetDriver();
        if (VSIStatL(m_outputDataset.GetName().c_str(), &sStat) == 0 ||
            (poDriver && EQUAL(poDriver->GetDescription(), "ESRI Shapefile")))
        {
            m_outputLayerName =
                CPLGetBasenameSafe(m_outputDataset.GetName().c_str());
        }
    }
    if (m_outputLayerName.empty())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Argument 'layer' must be specified");
        return false;
    }
    return true;
}

//! @endcond
