/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster create" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_create.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "ogr_spatialref.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*           GDALRasterCreateAlgorithm::GDALRasterCreateAlgorithm()     */
/************************************************************************/

GDALRasterCreateAlgorithm::GDALRasterCreateAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_RASTER, GDAL_DCAP_CREATE});
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER, false).AddAlias("like");
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER);
    AddCreationOptionsArg(&m_creationOptions);
    const char *exclusionGroup = "overwrite-append";
    AddOverwriteArg(&m_overwrite).SetMutualExclusionGroup(exclusionGroup);
    AddArg(GDAL_ARG_NAME_APPEND, 0,
           _("Append as a subdataset to existing output"), &m_append)
        .SetDefault(false)
        .SetMutualExclusionGroup(exclusionGroup);
    AddArg("size", 0, _("Output size in pixels"), &m_size)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetMinValueIncluded(0)
        .SetRepeatedArgAllowed(false)
        .SetDisplayHintAboutRepetition(false)
        .SetMetaVar("<width>,<height>");
    AddArg("band-count", 0, _("Number of bands"), &m_bandCount)
        .SetDefault(m_bandCount)
        .SetMinValueIncluded(0);
    AddOutputDataTypeArg(&m_type).SetDefault(m_type);

    AddNodataDataTypeArg(&m_nodata, /* noneAllowed = */ true);

    AddArg("burn", 0, _("Burn value"), &m_burnValues);
    AddArg("crs", 0, _("Set CRS"), &m_crs)
        .AddHiddenAlias("a_srs")
        .SetIsCRSArg(/*noneAllowed=*/true);
    AddBBOXArg(&m_bbox);

    {
        auto &arg = AddArg("metadata", 0, _("Add metadata item"), &m_metadata)
                        .SetMetaVar("<KEY>=<VALUE>")
                        .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });
        arg.AddHiddenAlias("mo");
    }
    AddArg("copy-metadata", 0, _("Copy metadata from input dataset"),
           &m_copyMetadata);
    AddArg("copy-overviews", 0,
           _("Create same overview levels as input dataset"), &m_copyOverviews);
}

/************************************************************************/
/*                  GDALRasterCreateAlgorithm::RunImpl()                */
/************************************************************************/

bool GDALRasterCreateAlgorithm::RunImpl(GDALProgressFunc /* pfnProgress */,
                                        void * /*pProgressData */)
{
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_outputFormat.empty())
    {
        const auto aosFormats =
            CPLStringList(GDALGetOutputDriversForDatasetName(
                m_outputDataset.GetName().c_str(), GDAL_OF_RASTER,
                /* bSingleMatch = */ true,
                /* bWarn = */ true));
        if (aosFormats.size() != 1)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot guess driver for %s",
                        m_outputDataset.GetName().c_str());
            return false;
        }
        m_outputFormat = aosFormats[0];
    }

    OGRSpatialReference oSRS;

    double adfGT[6] = {0};
    bool bGTValid = false;

    auto poSrcDS = m_inputDataset.GetDatasetRef();
    if (poSrcDS)
    {
        if (m_size.empty())
        {
            m_size = std::vector<int>{poSrcDS->GetRasterXSize(),
                                      poSrcDS->GetRasterYSize()};
        }

        if (!GetArg("band-count")->IsExplicitlySet())
        {
            m_bandCount = poSrcDS->GetRasterCount();
        }

        if (!GetArg("datatype")->IsExplicitlySet())
        {
            if (m_bandCount > 0)
            {
                m_type = GDALGetDataTypeName(
                    poSrcDS->GetRasterBand(1)->GetRasterDataType());
            }
        }

        if (m_crs.empty())
        {
            if (const auto poSRS = poSrcDS->GetSpatialRef())
                oSRS = *poSRS;
        }

        if (m_bbox.empty())
        {
            bGTValid = poSrcDS->GetGeoTransform(adfGT) == CE_None;
        }

        if (m_nodata.empty() && m_bandCount > 0)
        {
            int bNoData = false;
            const double dfNoData =
                poSrcDS->GetRasterBand(1)->GetNoDataValue(&bNoData);
            if (bNoData)
                m_nodata = CPLSPrintf("%.17g", dfNoData);
        }
    }

    if (m_size.empty())
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "Argument 'size' should be specified, or 'like' dataset "
                    "should be specified");
        return false;
    }

    if (!m_burnValues.empty() && m_burnValues.size() != 1 &&
        static_cast<int>(m_burnValues.size()) != m_bandCount)
    {
        if (m_bandCount == 1)
        {
            ReportError(CE_Failure, CPLE_IllegalArg,
                        "One value should be provided for argument "
                        "'burn', given there is one band");
        }
        else
        {
            ReportError(CE_Failure, CPLE_IllegalArg,
                        "One or %d values should be provided for argument "
                        "'burn', given there are %d bands",
                        m_bandCount, m_bandCount);
        }
        return false;
    }

    auto poDriver =
        GetGDALDriverManager()->GetDriverByName(m_outputFormat.c_str());
    if (!poDriver)
    {
        // shouldn't happen given checks done in GDALAlgorithm
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot find driver %s",
                    m_outputFormat.c_str());
        return false;
    }

    if (m_append)
    {
        if (poDriver->GetMetadataItem(GDAL_DCAP_CREATE_SUBDATASETS) == nullptr)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "-append option not supported for driver %s",
                        poDriver->GetDescription());
            return false;
        }
        m_creationOptions.push_back("APPEND_SUBDATASET=YES");
    }

    auto poRetDS = std::unique_ptr<GDALDataset>(poDriver->Create(
        m_outputDataset.GetName().c_str(), m_size[0], m_size[1], m_bandCount,
        GDALGetDataTypeByName(m_type.c_str()),
        CPLStringList(m_creationOptions).List()));
    if (!poRetDS)
    {
        return false;
    }

    if (!m_crs.empty() && m_crs != "none" && m_crs != "null")
    {
        oSRS.SetFromUserInput(m_crs.c_str());
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    if (!oSRS.IsEmpty())
    {
        if (poRetDS->SetSpatialRef(&oSRS) != CE_None)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Setting CRS failed");
            return false;
        }
    }

    if (!m_bbox.empty())
    {
        if (poRetDS->GetRasterXSize() == 0 || poRetDS->GetRasterYSize() == 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot set extent because one of dataset height or "
                        "width is null");
            return false;
        }
        bGTValid = true;
        adfGT[0] = m_bbox[0];
        adfGT[1] = (m_bbox[2] - m_bbox[0]) / poRetDS->GetRasterXSize();
        adfGT[2] = 0;
        adfGT[3] = m_bbox[3];
        adfGT[4] = 0;
        adfGT[5] = -(m_bbox[3] - m_bbox[1]) / poRetDS->GetRasterYSize();
    }
    if (bGTValid)
    {
        if (poRetDS->SetGeoTransform(adfGT) != CE_None)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Setting extent failed");
            return false;
        }
    }

    if (!m_nodata.empty() && !EQUAL(m_nodata.c_str(), "none"))
    {
        for (int i = 0; i < poRetDS->GetRasterCount(); ++i)
        {
            bool bCannotBeExactlyRepresented = false;
            if (poRetDS->GetRasterBand(i + 1)->SetNoDataValueAsString(
                    m_nodata.c_str(), &bCannotBeExactlyRepresented) != CE_None)
            {
                if (bCannotBeExactlyRepresented)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Setting nodata value failed as it cannot be "
                                "represented on its data type");
                }
                else
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Setting nodata value failed");
                }
                return false;
            }
        }
    }

    if (m_copyMetadata)
    {
        if (!poSrcDS)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Argument 'copy-metadata' can only be set when an "
                        "input dataset is set");
            return false;
        }
        {
            const CPLStringList aosDomains(poSrcDS->GetMetadataDomainList());
            for (const char *domain : aosDomains)
            {
                if (!EQUAL(domain, "IMAGE_STRUCTURE"))
                {
                    if (poRetDS->SetMetadata(poSrcDS->GetMetadata(domain),
                                             domain) != CE_None)
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Cannot copy '%s' metadata domain", domain);
                        return false;
                    }
                }
            }
        }
        for (int i = 0; i < m_bandCount; ++i)
        {
            const CPLStringList aosDomains(
                poSrcDS->GetRasterBand(i + 1)->GetMetadataDomainList());
            for (const char *domain : aosDomains)
            {
                if (!EQUAL(domain, "IMAGE_STRUCTURE"))
                {
                    if (poRetDS->GetRasterBand(i + 1)->SetMetadata(
                            poSrcDS->GetRasterBand(i + 1)->GetMetadata(domain),
                            domain) != CE_None)
                    {
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "Cannot copy '%s' metadata domain for band %d",
                            domain, i + 1);
                        return false;
                    }
                }
            }
        }
    }

    const CPLStringList aosMD(m_metadata);
    for (const auto &[key, value] : cpl::IterateNameValue(aosMD))
    {
        if (poRetDS->SetMetadataItem(key, value) != CE_None)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "SetMetadataItem('%s', '%s') failed", key, value);
            return false;
        }
    }

    if (m_copyOverviews && m_bandCount > 0)
    {
        if (!poSrcDS)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Argument 'copy-overviews' can only be set when an "
                        "input dataset is set");
            return false;
        }
        if (poSrcDS->GetRasterXSize() != poRetDS->GetRasterXSize() ||
            poSrcDS->GetRasterYSize() != poRetDS->GetRasterYSize())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Argument 'copy-overviews' can only be set when the "
                        "input and output datasets have the same dimension");
            return false;
        }
        const int nOverviewCount =
            poSrcDS->GetRasterBand(1)->GetOverviewCount();
        std::vector<int> anLevels;
        for (int i = 0; i < nOverviewCount; ++i)
        {
            const auto poOvrBand = poSrcDS->GetRasterBand(1)->GetOverview(i);
            const int nOvrFactor = GDALComputeOvFactor(
                poOvrBand->GetXSize(), poSrcDS->GetRasterXSize(),
                poOvrBand->GetYSize(), poSrcDS->GetRasterYSize());
            anLevels.push_back(nOvrFactor);
        }
        if (poRetDS->BuildOverviews(
                "NONE", nOverviewCount, anLevels.data(),
                /* nListBands = */ 0, /* panBandList = */ nullptr,
                /* pfnProgress = */ nullptr, /* pProgressData = */ nullptr,
                /* options = */ nullptr) != CE_None)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Creating overview(s) failed");
            return false;
        }
    }

    if (!m_burnValues.empty())
    {
        for (int i = 0; i < m_bandCount; ++i)
        {
            const int burnValueIdx = m_burnValues.size() == 1 ? 0 : i;
            const auto poDstBand = poRetDS->GetRasterBand(i + 1);
            // cppcheck-suppress negativeContainerIndex
            if (poDstBand->Fill(m_burnValues[burnValueIdx]) != CE_None)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Setting burn value failed");
                return false;
            }
        }
        if (poRetDS->FlushCache(false) != CE_None)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Setting burn value failed");
            return false;
        }
    }

    m_outputDataset.Set(std::move(poRetDS));

    return true;
}

//! @endcond
