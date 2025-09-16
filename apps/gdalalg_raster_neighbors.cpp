/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "neighbors" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_neighbors.h"

#include "gdal_priv.h"
#include "gdal_priv_templates.hpp"
#include "vrtdataset.h"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

// clang-format off
// Cf https://en.wikipedia.org/wiki/Kernel_(image_processing)
static const std::map<std::string, std::pair<int, std::vector<int>>> oMapKernelNameToMatrix = {
    { "u", { 3, {  0,  0,  0,
                  -1,  0,  1,
                   0,  0,  0 } } },
    { "v", { 3, {  0, -1,  0,
                   0,  0,  0,
                   0,  1,  0 } } },
    { "one_3x3", { 3, {  1,  1,  1,
                         1,  1,  1,
                         1,  1,  1 } } },
    { "edge1", { 3, {  0, -1,  0,
                      -1,  4, -1,
                       0, -1,  0 } } },
    { "edge2", { 3, { -1, -1, -1,
                      -1,  8, -1,
                      -1, -1, -1 } } },
    { "sharpen",  { 3, { 0, -1,  0,
                        -1,  5, -1,
                         0, -1,  0 } } },
    { "box_blur", { 3, { 1, 1, 1,
                         1, 1, 1,
                         1, 1, 1 } } },
    { "gaussian_blur_3x3", { 3, { 1, 2, 1,
                                  2, 4, 2,
                                  1, 2, 1 } } },
    { "gaussian_blur_5x5", { 5, { 1, 4,   6,  4, 1,
                                  4, 16, 24, 16, 4,
                                  6, 24, 36, 24, 6,
                                  4, 16, 24, 16, 4,
                                  1, 4,   6,  4, 1, } } },
    { "unsharp_masking_5x5", { 5, { 1, 4,     6,  4, 1,
                                    4, 16,   24, 16, 4,
                                    6, 24, -476, 24, 6,
                                    4, 16,   24, 16, 4,
                                    1, 4,     6,  4, 1, } } },
};

// clang-format on

/************************************************************************/
/*                        CreateDerivedBandXML()                        */
/************************************************************************/

static bool CreateDerivedBandXML(VRTDataset *poVRTDS, GDALRasterBand *poSrcBand,
                                 GDALDataType eType, const std::string &noData,
                                 const std::string &function,
                                 const std::string &kernel)
{
    int nKernelSize = 0;
    std::vector<double> adfCoefficients;
    if (kernel.front() == '[')
    {
        const CPLStringList aosValues(
            CSLTokenizeString2(kernel.c_str(), "[] ,",
                               CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
        const double dfSize = static_cast<double>(aosValues.size());
        nKernelSize = static_cast<int>(std::floor(sqrt(dfSize) + 0.5));
        for (const char *pszC : cpl::Iterate(aosValues))
        {
            // Already validated to be numeric by the validation action
            adfCoefficients.push_back(CPLAtof(pszC));
        }
    }
    else
    {
        auto oIterKnownKernel = oMapKernelNameToMatrix.find(kernel);
        CPLAssert(oIterKnownKernel != oMapKernelNameToMatrix.end());
        int nSum = 0;
        nKernelSize = oIterKnownKernel->second.first;
        for (const int nVal : oIterKnownKernel->second.second)
            nSum += nVal;
        const double dfWeight =
            (kernel == "u" || kernel == "v") ? 0.5
            : (kernel == "one_3x3")
                ? 1.0
                : 1.0 / (static_cast<double>(nSum) +
                         std::numeric_limits<double>::min());
        for (const int nVal : oIterKnownKernel->second.second)
        {
            adfCoefficients.push_back(nVal * dfWeight);
        }
    }

    poVRTDS->AddBand(eType, nullptr);

    std::optional<double> dstNoData;
    bool autoSelectNoDataValue = false;
    if (noData.empty())
    {
        autoSelectNoDataValue = true;
    }
    else if (noData != "none")
    {
        // Already validated to be numeric by the validation action
        dstNoData = CPLAtof(noData.c_str());
    }

    auto poVRTBand = cpl::down_cast<VRTSourcedRasterBand *>(
        poVRTDS->GetRasterBand(poVRTDS->GetRasterCount()));

    auto poSource = std::make_unique<VRTKernelFilteredSource>();
    poSrcBand->GetDataset()->Reference();
    poSource->SetSrcBand(poSrcBand);
    poSource->SetKernel(nKernelSize, /* separable = */ false, adfCoefficients);
    poSource->SetNormalized(function != "sum");
    if (function != "sum" && function != "mean")
        poSource->SetFunction(function.c_str());

    int bSrcHasNoData = false;
    const double dfNoDataValue = poSrcBand->GetNoDataValue(&bSrcHasNoData);
    if (bSrcHasNoData)
    {
        poSource->SetNoDataValue(dfNoDataValue);
        if (autoSelectNoDataValue && !dstNoData.has_value())
        {
            dstNoData = dfNoDataValue;
        }
    }

    if (dstNoData.has_value())
    {
        if (!GDALIsValueExactAs(dstNoData.value(), eType))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Band output type %s cannot represent NoData value %g",
                     GDALGetDataTypeName(eType), dstNoData.value());
            return false;
        }

        poVRTBand->SetNoDataValue(dstNoData.value());
    }

    poVRTBand->AddSource(std::move(poSource));

    return true;
}

/************************************************************************/
/*                      GDALNeighborsCreateVRTDerived()                 */
/************************************************************************/

static std::unique_ptr<GDALDataset>
GDALNeighborsCreateVRTDerived(GDALDataset *poSrcDS, int nBand,
                              GDALDataType eType, const std::string &noData,
                              const std::vector<std::string> &functions,
                              const std::vector<std::string> &kernels)
{
    CPLAssert(functions.size() == kernels.size());

    auto ds = std::make_unique<VRTDataset>(poSrcDS->GetRasterXSize(),
                                           poSrcDS->GetRasterYSize());
    GDALGeoTransform gt;
    if (poSrcDS->GetGeoTransform(gt) == CE_None)
        ds->SetGeoTransform(gt);
    if (const OGRSpatialReference *poSRS = poSrcDS->GetSpatialRef())
    {
        ds->SetSpatialRef(poSRS);
    }

    bool ret = true;
    for (size_t i = 0; i < kernels.size() && ret; ++i)
    {
        ret = CreateDerivedBandXML(ds.get(), poSrcDS->GetRasterBand(nBand),
                                   eType, noData, functions[i], kernels[i]);
    }
    if (!ret)
        ds.reset();
    return ds;
}

/************************************************************************/
/*       GDALRasterNeighborsAlgorithm::GDALRasterNeighborsAlgorithm()   */
/************************************************************************/

GDALRasterNeighborsAlgorithm::GDALRasterNeighborsAlgorithm(
    bool standaloneStep) noexcept
    : GDALRasterPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions().SetStandaloneStep(standaloneStep))
{
    AddBandArg(&m_band);

    AddArg("function", 0, _("Function to combine weighed source pixels"),
           &m_function)
        .SetChoices("mean", "sum", "min", "max", "stddev", "median", "mode");

    AddArg("kernel", 0, _("Convolution kernel(s) to apply"), &m_kernel)
        .SetPackedValuesAllowed(false)
        .SetMinCount(1)
        .SetMinCharCount(1)
        .SetRequired()
        .SetAutoCompleteFunction(
            [](const std::string &v)
            {
                std::vector<std::string> ret;
                if (v.empty() || v.front() != '[')
                {
                    for (const auto &[key, value] : oMapKernelNameToMatrix)
                    {
                        CPL_IGNORE_RET_VAL(value);
                        ret.push_back(key);
                    }
                    ret.push_back(
                        "[[val00,val10,...,valN0],...,[val0N,val1N,...valNN]]");
                }
                return ret;
            })
        .AddValidationAction(
            [this]()
            {
                for (const std::string &kernel : m_kernel)
                {
                    if (kernel.front() == '[' && kernel.back() == ']')
                    {
                        const CPLStringList aosValues(CSLTokenizeString2(
                            kernel.c_str(), "[] ,",
                            CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
                        const double dfSize =
                            static_cast<double>(aosValues.size());
                        const int nSqrt = static_cast<int>(sqrt(dfSize) + 0.5);
                        if (!((aosValues.size() % 2) == 1 &&
                              nSqrt * nSqrt == aosValues.size()))
                        {
                            ReportError(
                                CE_Failure, CPLE_IllegalArg,
                                "The number of values in the 'kernel' "
                                "argument must be an odd square number.");
                            return false;
                        }
                        for (int i = 0; i < aosValues.size(); ++i)
                        {
                            if (CPLGetValueType(aosValues[i]) ==
                                CPL_VALUE_STRING)
                            {
                                ReportError(CE_Failure, CPLE_IllegalArg,
                                            "Non-numeric value found in the "
                                            "'kernel' argument");
                                return false;
                            }
                        }
                    }
                    else if (!cpl::contains(oMapKernelNameToMatrix, kernel))
                    {
                        std::string osMsg =
                            "Valid values for 'kernel' argument are: ";
                        bool bFirst = true;
                        for (const auto &[key, value] : oMapKernelNameToMatrix)
                        {
                            CPL_IGNORE_RET_VAL(value);
                            if (!bFirst)
                                osMsg += ", ";
                            bFirst = false;
                            osMsg += '\'';
                            osMsg += key;
                            osMsg += '\'';
                        }
                        osMsg += " or "
                                 "[[val00,val10,...,valN0],...,[val0N,val1N,..."
                                 "valNN]]";
                        ReportError(CE_Failure, CPLE_IllegalArg, "%s",
                                    osMsg.c_str());
                        return false;
                    }
                }
                return true;
            });

    AddOutputDataTypeArg(&m_type).SetDefault("Float64");

    AddNodataArg(&m_nodata, true);

    AddValidationAction(
        [this]()
        {
            if (m_function.size() > 1 && m_function.size() != m_kernel.size())
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "The number of values for the 'function' argument should "
                    "be one or exactly the number of values of 'kernel'");
                return false;
            }

            if (m_band == 0 &&
                m_inputDataset[0].GetDatasetRef()->GetRasterCount() > 1)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "'band' argument should be specified given input "
                            "dataset has several bands.");
                return false;
            }

            return true;
        });
}

/************************************************************************/
/*                GDALRasterNeighborsAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterNeighborsAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_band == 0)
        m_band = 1;
    CPLAssert(m_band <= poSrcDS->GetRasterCount());

    auto eType = GDALGetDataTypeByName(m_type.c_str());
    if (eType == GDT_Unknown)
    {
        eType = GDT_Float64;
    }

    if (m_function.size() <= 1)
    {
        while (m_function.size() < m_kernel.size())
        {
            m_function.push_back(
                m_function.empty()
                    ? ((m_kernel[0] == "u" || m_kernel[0] == "v") ? "sum"
                                                                  : "mean")
                    : m_function.back());
        }
    }

    auto vrt = GDALNeighborsCreateVRTDerived(poSrcDS, m_band, eType, m_nodata,
                                             m_function, m_kernel);
    const bool ret = vrt != nullptr;
    if (vrt)
    {
        m_outputDataset.Set(std::move(vrt));
    }
    return ret;
}

GDALRasterNeighborsAlgorithmStandalone::
    ~GDALRasterNeighborsAlgorithmStandalone() = default;

//! @endcond
