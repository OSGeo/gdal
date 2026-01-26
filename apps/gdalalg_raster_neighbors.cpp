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
#include <set>
#include <utility>
#include <vector>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

static const std::set<std::string> oSetKernelNames = {
    "u",     "v",       "equal",    "edge1",
    "edge2", "sharpen", "gaussian", "unsharp-masking"};

namespace
{
struct KernelDef
{
    int size = 0;
    std::vector<double> adfCoefficients{};
};
}  // namespace

// clang-format off
// Cf https://en.wikipedia.org/wiki/Kernel_(image_processing)
static const std::map<std::string, std::pair<int, std::vector<int>>> oMapKernelNameToMatrix = {
    { "u", { 3, {  0,  0,  0,
                  -1,  0,  1,
                   0,  0,  0 } } },
    { "v", { 3, {  0, -1,  0,
                   0,  0,  0,
                   0,  1,  0 } } },
    { "edge1", { 3, {  0, -1,  0,
                      -1,  4, -1,
                       0, -1,  0 } } },
    { "edge2", { 3, { -1, -1, -1,
                      -1,  8, -1,
                      -1, -1, -1 } } },
    { "sharpen",  { 3, { 0, -1,  0,
                        -1,  5, -1,
                         0, -1,  0 } } },
    { "gaussian-3x3", { 3, { 1, 2, 1,
                                  2, 4, 2,
                                  1, 2, 1 } } },
    { "gaussian-5x5", { 5, { 1, 4,   6,  4, 1,
                                  4, 16, 24, 16, 4,
                                  6, 24, 36, 24, 6,
                                  4, 16, 24, 16, 4,
                                  1, 4,   6,  4, 1, } } },
    { "unsharp-masking-5x5", { 5, { 1, 4,     6,  4, 1,
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
                                 const std::string &method,
                                 const KernelDef &kernelDef)
{
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
    poSource->SetKernel(kernelDef.size, /* separable = */ false,
                        kernelDef.adfCoefficients);
    poSource->SetNormalized(method != "sum");
    if (method != "sum" && method != "mean")
        poSource->SetFunction(method.c_str());

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
/*                   GDALNeighborsCreateVRTDerived()                    */
/************************************************************************/

static std::unique_ptr<GDALDataset>
GDALNeighborsCreateVRTDerived(GDALDataset *poSrcDS, int nBand,
                              GDALDataType eType, const std::string &noData,
                              const std::vector<std::string> &methods,
                              const std::vector<KernelDef> &aKernelDefs)
{
    CPLAssert(methods.size() == aKernelDefs.size());

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
    if (nBand != 0)
    {
        for (size_t i = 0; i < aKernelDefs.size() && ret; ++i)
        {
            ret =
                CreateDerivedBandXML(ds.get(), poSrcDS->GetRasterBand(nBand),
                                     eType, noData, methods[i], aKernelDefs[i]);
        }
    }
    else
    {
        for (int iBand = 1; iBand <= poSrcDS->GetRasterCount(); ++iBand)
        {
            for (size_t i = 0; i < aKernelDefs.size() && ret; ++i)
            {
                ret = CreateDerivedBandXML(ds.get(),
                                           poSrcDS->GetRasterBand(iBand), eType,
                                           noData, methods[i], aKernelDefs[i]);
            }
        }
    }
    if (!ret)
        ds.reset();
    return ds;
}

/************************************************************************/
/*     GDALRasterNeighborsAlgorithm::GDALRasterNeighborsAlgorithm()     */
/************************************************************************/

GDALRasterNeighborsAlgorithm::GDALRasterNeighborsAlgorithm(
    bool standaloneStep) noexcept
    : GDALRasterPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions().SetStandaloneStep(standaloneStep))
{
    AddBandArg(&m_band);

    AddArg("method", 0, _("Method to combine weighed source pixels"), &m_method)
        .SetChoices("mean", "sum", "min", "max", "stddev", "median", "mode");

    AddArg("size", 0, _("Neighborhood size"), &m_size)
        .SetMinValueIncluded(3)
        .SetMaxValueIncluded(99)
        .AddValidationAction(
            [this]()
            {
                if ((m_size % 2) != 1)
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "The value of 'size' must be an odd number.");
                    return false;
                }
                return true;
            });

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
                    ret.insert(ret.end(), oSetKernelNames.begin(),
                               oSetKernelNames.end());
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
                    else if (!cpl::contains(oSetKernelNames, kernel))
                    {
                        std::string osMsg =
                            "Valid values for 'kernel' argument are: ";
                        bool bFirst = true;
                        for (const auto &name : oSetKernelNames)
                        {
                            if (!bFirst)
                                osMsg += ", ";
                            bFirst = false;
                            osMsg += '\'';
                            osMsg += name;
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
            if (m_method.size() > 1 && m_method.size() != m_kernel.size())
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "The number of values for the 'method' argument should "
                    "be one or exactly the number of values of 'kernel'");
                return false;
            }

            if (m_size > 0)
            {
                for (const std::string &kernel : m_kernel)
                {
                    if (kernel == "gaussian")
                    {
                        if (m_size != 3 && m_size != 5)
                        {
                            ReportError(CE_Failure, CPLE_AppDefined,
                                        "Currently only size = 3 or 5 is "
                                        "supported for kernel '%s'",
                                        kernel.c_str());
                            return false;
                        }
                    }
                    else if (kernel == "unsharp-masking")
                    {
                        if (m_size != 5)
                        {
                            ReportError(CE_Failure, CPLE_AppDefined,
                                        "Currently only size = 5 is supported "
                                        "for kernel '%s'",
                                        kernel.c_str());
                            return false;
                        }
                    }
                    else if (kernel[0] == '[')
                    {
                        const CPLStringList aosValues(CSLTokenizeString2(
                            kernel.c_str(), "[] ,",
                            CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
                        const double dfSize =
                            static_cast<double>(aosValues.size());
                        const int size =
                            static_cast<int>(std::floor(sqrt(dfSize) + 0.5));
                        if (m_size != size)
                        {
                            ReportError(CE_Failure, CPLE_AppDefined,
                                        "Value of 'size' argument (%d) "
                                        "inconsistent with the one deduced "
                                        "from the kernel matrix (%d)",
                                        m_size, size);
                            return false;
                        }
                    }
                    else if (m_size != 3 && kernel != "equal" &&
                             kernel[0] != '[')
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Currently only size = 3 is supported for "
                                    "kernel '%s'",
                                    kernel.c_str());
                        return false;
                    }
                }
            }

            return true;
        });
}

/************************************************************************/
/*                            GetKernelDef()                            */
/************************************************************************/

static KernelDef GetKernelDef(const std::string &name, bool normalizeCoefs,
                              double weightIfNotNormalized)
{
    auto it = oMapKernelNameToMatrix.find(name);
    CPLAssert(it != oMapKernelNameToMatrix.end());
    KernelDef def;
    def.size = it->second.first;
    int nSum = 0;
    for (const int nVal : it->second.second)
        nSum += nVal;
    const double dfWeight = normalizeCoefs
                                ? 1.0 / (static_cast<double>(nSum) +
                                         std::numeric_limits<double>::min())
                                : weightIfNotNormalized;
    for (const int nVal : it->second.second)
    {
        def.adfCoefficients.push_back(nVal * dfWeight);
    }
    return def;
}

/************************************************************************/
/*               GDALRasterNeighborsAlgorithm::RunStep()                */
/************************************************************************/

bool GDALRasterNeighborsAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLAssert(m_band <= poSrcDS->GetRasterCount());

    auto eType = GDALGetDataTypeByName(m_type.c_str());
    if (eType == GDT_Unknown)
    {
        eType = GDT_Float64;
    }
    std::vector<KernelDef> aKernelDefs(m_kernel.size());
    std::vector<bool> abNullCoefficientSum(m_kernel.size());
    for (size_t i = 0; i < m_kernel.size(); ++i)
    {
        const std::string &kernel = m_kernel[i];
        if (!kernel.empty() && kernel[0] == '[')
        {
            const CPLStringList aosValues(
                CSLTokenizeString2(kernel.c_str(), "[] ,",
                                   CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
            const double dfSize = static_cast<double>(aosValues.size());
            KernelDef def;
            def.size = static_cast<int>(std::floor(sqrt(dfSize) + 0.5));
            double dfSum = 0;
            for (const char *pszC : cpl::Iterate(aosValues))
            {
                const double dfV = CPLAtof(pszC);
                dfSum += dfV;
                // Already validated to be numeric by the validation action
                def.adfCoefficients.push_back(dfV);
            }
            abNullCoefficientSum[i] = std::fabs(dfSum) < 1e-10;
            if (abNullCoefficientSum[i] && m_method.size() == m_kernel.size() &&
                m_method[i] == "mean")
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "Specifying method = 'mean' for a kernel whose sum of "
                    "coefficients is zero is not allowed. Use 'sum' instead");
                return false;
            }
            aKernelDefs[i] = std::move(def);
        }
    }

    if (m_method.empty())
    {
        for (size_t i = 0; i < m_kernel.size(); ++i)
        {
            const bool bIsZeroSumKernel =
                m_kernel[i] == "u" || m_kernel[i] == "v" ||
                m_kernel[i] == "edge1" || m_kernel[i] == "edge2" ||
                abNullCoefficientSum[i];
            m_method.push_back(bIsZeroSumKernel ? "sum" : "mean");
        }
    }
    else if (m_method.size() == 1)
    {
        const std::string lastValue(m_method.back());
        m_method.resize(m_kernel.size(), lastValue);
    }

    if (m_size == 0 && m_kernel[0][0] != '[')
        m_size = m_kernel[0] == "unsharp-masking" ? 5 : 3;

    for (size_t i = 0; i < m_kernel.size(); ++i)
    {
        const std::string &kernel = m_kernel[i];
        if (aKernelDefs[i].adfCoefficients.empty())
        {
            KernelDef def;
            if (kernel == "edge1" || kernel == "edge2" || kernel == "sharpen")
            {
                CPLAssert(m_size == 3);
                def = GetKernelDef(kernel, false, 1.0);
            }
            else if (kernel == "u" || kernel == "v")
            {
                CPLAssert(m_size == 3);
                def = GetKernelDef(kernel, false, 0.5);
            }
            else if (kernel == "equal")
            {
                def.size = m_size;
                const double dfWeight =
                    m_method[i] == "mean"
                        ? 1.0 / (static_cast<double>(m_size) * m_size +
                                 std::numeric_limits<double>::min())
                        : 1.0;
                def.adfCoefficients.resize(static_cast<size_t>(m_size) * m_size,
                                           dfWeight);
            }
            else if (kernel == "gaussian")
            {
                CPLAssert(m_size == 3 || m_size == 5);
                def = GetKernelDef(
                    m_size == 3 ? "gaussian-3x3" : "gaussian-5x5", true, 0.0);
            }
            else if (kernel == "unsharp-masking")
            {
                CPLAssert(m_size == 5);
                def = GetKernelDef("unsharp-masking-5x5", true, 0.0);
            }
            else
            {
                CPLAssert(false);
            }
            aKernelDefs[i] = std::move(def);
        }
    }

    auto vrt = GDALNeighborsCreateVRTDerived(poSrcDS, m_band, eType, m_nodata,
                                             m_method, aKernelDefs);
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
