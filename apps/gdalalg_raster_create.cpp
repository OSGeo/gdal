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
/*        GDALRasterCreateAlgorithm::GDALRasterCreateAlgorithm()        */
/************************************************************************/

GDALRasterCreateAlgorithm::GDALRasterCreateAlgorithm(
    bool standaloneStep) noexcept
    : GDALRasterPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetAddDefaultArguments(false)
              .SetAutoOpenInputDatasets(true)
              .SetInputDatasetHelpMsg("Template raster dataset")
              .SetInputDatasetAlias("like")
              .SetInputDatasetMetaVar("TEMPLATE-DATASET")
              .SetInputDatasetRequired(false)
              .SetInputDatasetPositional(false)
              .SetInputDatasetMaxCount(1))
{
    AddRasterInputArgs(false, false);
    if (standaloneStep)
    {
        AddProgressArg();
        AddRasterOutputArgs(false);
    }

    auto checkResSizeInput = [this](const std::vector<std::string> &values,
                                    const std::string &arg_name,
                                    bool requiresInt)
    {
        for (const auto &s : values)
        {
            char *endptr = nullptr;
            const double val = CPLStrtod(s.c_str(), &endptr);
            bool ok = false;
            if (endptr == s.c_str() + s.size())
            {
                if (val >= 0 && val <= INT_MAX &&
                    (!requiresInt || static_cast<int>(val) == val))
                {
                    ok = true;
                }
            }
            else if (endptr &&
                     ((endptr[0] == ' ' && endptr[1] == '%' &&
                       endptr + 2 == s.c_str() + s.size()) ||
                      (endptr[0] == '%' && endptr + 1 == s.c_str() + s.size())))
            {
                if (val >= 0)
                {
                    ok = true;
                }
            }
            if (!ok)
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "Invalid %s value: %s'", arg_name.c_str(),
                            s.c_str());
                return false;
            }
        }
        return true;
    };

    AddArg("resolution", 0, _("Target resolution (in destination CRS units)"),
           &m_resolution_str)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetMinValueExcluded(0)
        .SetRepeatedArgAllowed(false)
        .SetDisplayHintAboutRepetition(false)
        .SetMetaVar("<xres[%]>,<yres[%]>")
        .SetMutualExclusionGroup("resolution-size")
        .AddValidationAction(
            [this, checkResSizeInput]()
            {
                return checkResSizeInput(m_resolution_str, "resolution", false);
            });

    // The same logic was applied in gdalalg_raster_resize.cpp, so we replicate it here for consistency.
    AddArg("size", 0,
           _("Target size in pixels (or percentage if using '%' suffix)"),
           &m_size_str)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetMinValueIncluded(0)
        .SetRepeatedArgAllowed(false)
        .SetDisplayHintAboutRepetition(false)
        .SetMetaVar("<width[%]>,<height[%]>")
        .SetMutualExclusionGroup("resolution-size")
        .AddValidationAction(
            [this, checkResSizeInput]()
            { return checkResSizeInput(m_size_str, "size", true); });

    AddArg("band-count", 0, _("Number of bands"), &m_bandCount)
        .SetDefault(m_bandCount)
        .SetMinValueIncluded(0);
    AddOutputDataTypeArg(&m_type).SetDefault(m_type);

    AddNodataArg(&m_nodata, /* noneAllowed = */ true);

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

    const auto inputArg = GetArg(GDAL_ARG_NAME_INPUT);
    CPLAssertNotNull(inputArg);

    AddArg("copy-metadata", 0, _("Copy metadata from input dataset"),
           &m_copyMetadata)
        .AddDirectDependency(*inputArg);
    AddArg("copy-overviews", 0,
           _("Create same overview levels as input dataset"), &m_copyOverviews)
        .AddDirectDependency(*inputArg);
}

/************************************************************************/
/*                 GDALRasterCreateAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALRasterCreateAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                        void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

/************************************************************************/
/*                 GDALRasterCreateAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterCreateAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_standaloneStep)
    {
        if (m_format.empty())
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
            m_format = aosFormats[0];
        }
    }
    else
    {
        m_format = "MEM";
    }

    OGRSpatialReference oSRS;

    GDALGeoTransform gt;
    bool bGTValid = false;

    CPLStringList aosCreationOptions(m_creationOptions);

    GDALDataset *poSrcDS = m_inputDataset.empty()
                               ? nullptr
                               : m_inputDataset.front().GetDatasetRef();

    constexpr double EPSILON = 1e-5;

    // Process size_str to fill m_size
    for (size_t i = 0; i < m_size_str.size(); i++)
    {
        const auto &s = m_size_str[i];
        char *endptr = nullptr;
        const double val = CPLStrtod(s.c_str(), &endptr);
        if (endptr &&
            ((endptr[0] == ' ' && endptr[1] == '%' &&
              endptr + 2 == s.c_str() + s.size()) ||
             (endptr[0] == '%' && endptr + 1 == s.c_str() + s.size())))
        {
            // Percentage
            if (!poSrcDS)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot use percentage size without input dataset");
                return false;
            }
            const int refSize = (i == 0) ? poSrcDS->GetRasterXSize()
                                         : poSrcDS->GetRasterYSize();
            const double dfSize = std::ceil((refSize * val / 100.0) - EPSILON);
            ;
            if (dfSize > INT_MAX)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Computed size is too large");
                return false;
            }
            m_size.push_back(static_cast<int>(dfSize));
        }
        else
        {
            m_size.push_back(static_cast<int>(val));
        }
    }

    std::vector<bool> abResIsPercentage(m_resolution_str.size(), false);

    // Process resolution_str to fill m_resolution
    for (size_t i = 0; i < m_resolution_str.size(); i++)
    {
        const auto &s = m_resolution_str[i];
        char *endptr = nullptr;
        const double val = CPLStrtod(s.c_str(), &endptr);

        if (endptr &&
            ((endptr[0] == ' ' && endptr[1] == '%' &&
              endptr + 2 == s.c_str() + s.size()) ||
             (endptr[0] == '%' && endptr + 1 == s.c_str() + s.size())))
        {
            // Percentage
            if (!poSrcDS)
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "Cannot use percentage resolution without input dataset");
                return false;
            }
            if (poSrcDS->GetGeoTransform(gt) == CE_None)
            {
                const double refRes =
                    (i == 0) ? std::abs(gt.xscale) : std::abs(gt.yscale);
                const double dfRes = refRes * (val / 100.0);
                m_resolution.push_back(dfRes);
            }
            else
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot get geotransform from input dataset");
                return false;
            }
            abResIsPercentage[i] = true;
        }
        else
        {
            m_resolution.push_back(val);
            abResIsPercentage[i] = false;
        }
    }

    if (poSrcDS)
    {
        if (m_size.empty())
        {
            m_size = std::vector<int>{poSrcDS->GetRasterXSize(),
                                      poSrcDS->GetRasterYSize()};
        }

        bGTValid = poSrcDS->GetGeoTransform(gt) == CE_None;

        // If one of the size is 0, compute it from the other size
        if (m_size[0] == 0 && m_size[1] > 0)
        {
            if (bGTValid)
            {
                const double ratio =
                    static_cast<double>(poSrcDS->GetRasterXSize()) /
                    static_cast<double>(poSrcDS->GetRasterYSize());
                const double dfWidth =
                    std::ceil(static_cast<double>(m_size[1]) * ratio - EPSILON);
                if (dfWidth > INT_MAX)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Computed width is too large");
                    return false;
                }
                m_size[0] = static_cast<int>(dfWidth);
            }
            else
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot get geotransform from input dataset");
                return false;
            }
        }
        else if (m_size[1] == 0 && m_size[0] > 0)
        {
            if (bGTValid)
            {
                const double ratio =
                    static_cast<double>(poSrcDS->GetRasterYSize()) /
                    static_cast<double>(poSrcDS->GetRasterXSize());
                const double dfHeight =
                    std::ceil(static_cast<double>(m_size[0]) * ratio - EPSILON);
                if (dfHeight > INT_MAX)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Computed height is too large");
                    return false;
                }
                m_size[1] = static_cast<int>(dfHeight);
            }
            else
            {
                m_size[1] = poSrcDS->GetRasterYSize();
            }
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

        if (m_nodata.empty() && m_bandCount > 0)
        {
            int bNoData = false;
            const double dfNoData =
                poSrcDS->GetRasterBand(1)->GetNoDataValue(&bNoData);
            if (bNoData)
                m_nodata = CPLSPrintf("%.17g", dfNoData);
        }

        // Replicate tiling of input datasets for a few popular output formats,
        // when compatible, and when the user hasn't specified creation options
        // affecting tiling.
        int nBlockXSize = 0, nBlockYSize = 0;
        if (m_bandCount > 0)
            poSrcDS->GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

        if (EQUAL(m_format.c_str(), "GTIFF") &&
            aosCreationOptions.FetchNameValue("TILED") == nullptr &&
            aosCreationOptions.FetchNameValue("BLOCKXSIZE") == nullptr &&
            aosCreationOptions.FetchNameValue("BLOCKYSIZE") == nullptr &&
            m_bandCount > 0)
        {
            if (nBlockXSize != poSrcDS->GetRasterXSize() &&
                (nBlockXSize % 16) == 0 && (nBlockYSize % 16) == 0)
            {
                aosCreationOptions.SetNameValue("TILED", "YES");
                aosCreationOptions.SetNameValue("BLOCKXSIZE",
                                                CPLSPrintf("%d", nBlockXSize));
                aosCreationOptions.SetNameValue("BLOCKYSIZE",
                                                CPLSPrintf("%d", nBlockYSize));
            }
        }
        else if (EQUAL(m_format.c_str(), "COG") &&
                 aosCreationOptions.FetchNameValue("BLOCKSIZE") == nullptr &&
                 m_bandCount > 0)
        {
            if (nBlockXSize != poSrcDS->GetRasterXSize() &&
                nBlockXSize == nBlockYSize && nBlockXSize >= 128 &&
                (nBlockXSize % 16) == 0)
            {
                aosCreationOptions.SetNameValue("BLOCKSIZE",
                                                CPLSPrintf("%d", nBlockXSize));
            }
        }
        else if (EQUAL(m_format.c_str(), "GPKG") &&
                 aosCreationOptions.FetchNameValue("BLOCKSIZE") == nullptr &&
                 aosCreationOptions.FetchNameValue("BLOCKXSIZE") == nullptr &&
                 aosCreationOptions.FetchNameValue("BLOCKYSIZE") == nullptr &&
                 m_bandCount > 0)
        {
            if (nBlockXSize != poSrcDS->GetRasterXSize() &&
                nBlockXSize >= 256 && nBlockXSize <= 4096 &&
                nBlockYSize >= 256 && nBlockYSize <= 4096)
            {
                aosCreationOptions.SetNameValue("BLOCKXSIZE",
                                                CPLSPrintf("%d", nBlockXSize));
                aosCreationOptions.SetNameValue("BLOCKYSIZE",
                                                CPLSPrintf("%d", nBlockYSize));
            }
        }

        // If resolution was explicitly set, then we need to recompute size
        if (!m_resolution.empty() && bGTValid)
        {
            // Set resolution from the other axis if 0
            if (m_resolution[0] == 0)
            {
                if (abResIsPercentage[1])
                {
                    m_resolution[0] = (std::abs(gt.xscale) / m_resolution[1]) /
                                      std::abs(gt.yscale);
                }
                else
                {
                    m_resolution[0] = m_resolution[1];
                }
            }

            if (m_resolution[1] == 0)
            {
                if (abResIsPercentage[0])
                {
                    m_resolution[1] = (std::abs(gt.yscale) / m_resolution[0]) /
                                      std::abs(gt.xscale);
                }
                else
                {
                    m_resolution[1] = m_resolution[0];
                }
            }

            const double dfXResRatio = std::abs(gt.xscale) / m_resolution[0];
            const double dfYResRatio = std::abs(gt.yscale) / m_resolution[1];
            const double dfWidth =
                std::ceil(poSrcDS->GetRasterXSize() * dfXResRatio - EPSILON);
            const double dfHeight =
                std::ceil(poSrcDS->GetRasterYSize() * dfYResRatio - EPSILON);
            if (dfWidth > INT_MAX || dfHeight > INT_MAX)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Computed size is too large");
                return false;
            }
            m_size = {static_cast<int>(dfWidth), static_cast<int>(dfHeight)};
        }
    }

    // If size is empty, try resolution from bbox
    if (m_size.empty() && m_bbox.size() == 4 && m_resolution.size() == 2 &&
        (m_bbox[3] - m_bbox[1] != 0) && (m_bbox[2] - m_bbox[0] != 0))
    {
        const double dfWidth =
            std::ceil((m_bbox[2] - m_bbox[0]) / m_resolution[0] - EPSILON);
        const double dfHeight =
            std::ceil((m_bbox[3] - m_bbox[1]) / m_resolution[1] - EPSILON);
        if (dfWidth > INT_MAX || dfHeight > INT_MAX)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Computed size is too large");
            return false;
        }
        m_size = {static_cast<int>(dfWidth), static_cast<int>(dfHeight)};
    }

    if (m_size.empty())
    {
        if (!m_resolution.empty() && m_bbox.empty())
        {
            ReportError(
                CE_Failure, CPLE_IllegalArg,
                "Cannot use resolution without 'bbox' or 'like' dataset");
        }
        else
        {
            ReportError(CE_Failure, CPLE_IllegalArg,
                        "Argument 'size' or 'resolution' or 'like' dataset "
                        "should be specified");
        }
        return false;
    }

    // Guess the size from bbox if only one of the two is specified
    if (m_size.size() == 2 && (m_size[0] == 0 || m_size[1] == 0) &&
        !(m_size[0] == 0 && m_size[1] == 0) && m_bbox.size() == 4 &&
        (m_bbox[3] - m_bbox[1] != 0) && (m_bbox[2] - m_bbox[0] != 0))
    {
        const double ratio = (m_bbox[2] - m_bbox[0]) / (m_bbox[3] - m_bbox[1]);
        if (m_size[0] == 0)
        {
            double dfWidth = std::ceil(m_size[1] * ratio - EPSILON);
            if (dfWidth > INT_MAX)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Too large computed width");
                return false;
            }
            m_size[0] = static_cast<int>(dfWidth);
        }
        else if (m_size[1] == 0)
        {
            double dfHeight = std::ceil(m_size[0] / ratio - EPSILON);
            if (dfHeight > INT_MAX)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Too large computed height");
                return false;
            }
            m_size[1] = static_cast<int>(dfHeight);
        }
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

    auto poDriver = GetGDALDriverManager()->GetDriverByName(m_format.c_str());
    if (!poDriver)
    {
        // shouldn't happen given checks done in GDALAlgorithm
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot find driver %s",
                    m_format.c_str());
        return false;
    }

    if (m_appendRaster)
    {
        if (poDriver->GetMetadataItem(GDAL_DCAP_CREATE_SUBDATASETS) == nullptr)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "-append option not supported for driver %s",
                        poDriver->GetDescription());
            return false;
        }
        aosCreationOptions.SetNameValue("APPEND_SUBDATASET", "YES");
    }

    auto poRetDS = std::unique_ptr<GDALDataset>(poDriver->Create(
        m_outputDataset.GetName().c_str(), m_size[0], m_size[1], m_bandCount,
        GDALGetDataTypeByName(m_type.c_str()), aosCreationOptions.List()));
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
        gt.xorig = m_bbox[0];
        gt.xscale = (m_bbox[2] - m_bbox[0]) / poRetDS->GetRasterXSize();
        gt.xrot = 0;
        gt.yorig = m_bbox[3];
        gt.yrot = 0;
        gt.yscale = -(m_bbox[3] - m_bbox[1]) / poRetDS->GetRasterYSize();
    }
    if (bGTValid)
    {
        if (poRetDS->SetGeoTransform(gt) != CE_None)
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

        // This should never happen because of the dependency set
        CPLAssertNotNull(poSrcDS);

        {
            const CPLStringList aosDomains(poSrcDS->GetMetadataDomainList());
            for (const char *domain : aosDomains)
            {
                if (!EQUAL(domain, GDAL_MDD_IMAGE_STRUCTURE))
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
                if (!EQUAL(domain, GDAL_MDD_IMAGE_STRUCTURE))
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
        // This should never happen because of the dependency set
        CPLAssertNotNull(poSrcDS);

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

GDALRasterCreateAlgorithmStandalone::~GDALRasterCreateAlgorithmStandalone() =
    default;

//! @endcond
