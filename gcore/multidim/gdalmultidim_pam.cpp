/******************************************************************************
 *
 * Name:     gdalmultidim_pam.cpp
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALPamMDArray and GDALPamMultiDim classes
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_error_internal.h"
#include "gdal_multidim.h"
#include "gdal_pam.h"
#include "gdal_pam_multidim.h"
#include "ogr_spatialref.h"

#include <map>

#if defined(__clang__) || defined(_MSC_VER)
#define COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT
#endif

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALPamMultiDim::Private                       */
/************************************************************************/

struct GDALPamMultiDim::Private
{
    std::string m_osFilename{};
    std::string m_osPamFilename{};

    struct Statistics
    {
        bool bHasStats = false;
        bool bApproxStats = false;
        double dfMin = 0;
        double dfMax = 0;
        double dfMean = 0;
        double dfStdDev = 0;
        GUInt64 nValidCount = 0;
    };

    struct ArrayInfo
    {
        std::shared_ptr<OGRSpatialReference> poSRS{};
        // cppcheck-suppress unusedStructMember
        Statistics stats{};
        std::string osOvrFilename{};
    };

    typedef std::pair<std::string, std::string> NameContext;
    std::map<NameContext, ArrayInfo> m_oMapArray{};
    std::vector<CPLXMLTreeCloser> m_apoOtherNodes{};
    bool m_bDirty = false;
    bool m_bLoaded = false;
};

/************************************************************************/
/*                           GDALPamMultiDim                            */
/************************************************************************/

GDALPamMultiDim::GDALPamMultiDim(const std::string &osFilename)
    : d(new Private())
{
    d->m_osFilename = osFilename;
}

/************************************************************************/
/*                 GDALPamMultiDim::~GDALPamMultiDim()                  */
/************************************************************************/

GDALPamMultiDim::~GDALPamMultiDim()
{
    if (d->m_bDirty)
        Save();
}

/************************************************************************/
/*                       GDALPamMultiDim::Load()                        */
/************************************************************************/

void GDALPamMultiDim::Load()
{
    if (d->m_bLoaded)
        return;
    d->m_bLoaded = true;

    const char *pszProxyPam = PamGetProxy(d->m_osFilename.c_str());
    d->m_osPamFilename =
        pszProxyPam ? std::string(pszProxyPam) : d->m_osFilename + ".aux.xml";
    CPLXMLTreeCloser oTree(nullptr);
    {
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
        oTree.reset(CPLParseXMLFile(d->m_osPamFilename.c_str()));
    }
    if (!oTree)
    {
        return;
    }
    const auto poPAMMultiDim = CPLGetXMLNode(oTree.get(), "=PAMDataset");
    if (!poPAMMultiDim)
        return;
    for (CPLXMLNode *psIter = poPAMMultiDim->psChild; psIter;
         psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Array") == 0)
        {
            const char *pszName = CPLGetXMLValue(psIter, "name", nullptr);
            if (!pszName)
                continue;
            const char *pszContext = CPLGetXMLValue(psIter, "context", "");
            const auto oKey =
                std::pair<std::string, std::string>(pszName, pszContext);

            /* --------------------------------------------------------------------
             */
            /*      Check for an SRS node. */
            /* --------------------------------------------------------------------
             */
            const CPLXMLNode *psSRSNode = CPLGetXMLNode(psIter, "SRS");
            if (psSRSNode)
            {
                std::shared_ptr<OGRSpatialReference> poSRS =
                    std::make_shared<OGRSpatialReference>();
                poSRS->SetFromUserInput(
                    CPLGetXMLValue(psSRSNode, nullptr, ""),
                    OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS);
                const char *pszMapping = CPLGetXMLValue(
                    psSRSNode, "dataAxisToSRSAxisMapping", nullptr);
                if (pszMapping)
                {
                    char **papszTokens =
                        CSLTokenizeStringComplex(pszMapping, ",", FALSE, FALSE);
                    std::vector<int> anMapping;
                    for (int i = 0; papszTokens && papszTokens[i]; i++)
                    {
                        anMapping.push_back(atoi(papszTokens[i]));
                    }
                    CSLDestroy(papszTokens);
                    poSRS->SetDataAxisToSRSAxisMapping(anMapping);
                }
                else
                {
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                }

                const char *pszCoordinateEpoch =
                    CPLGetXMLValue(psSRSNode, "coordinateEpoch", nullptr);
                if (pszCoordinateEpoch)
                    poSRS->SetCoordinateEpoch(CPLAtof(pszCoordinateEpoch));

                d->m_oMapArray[oKey].poSRS = std::move(poSRS);
            }

            const CPLXMLNode *psStatistics =
                CPLGetXMLNode(psIter, "Statistics");
            if (psStatistics)
            {
                Private::Statistics sStats;
                sStats.bHasStats = true;
                sStats.bApproxStats = CPLTestBool(
                    CPLGetXMLValue(psStatistics, "ApproxStats", "false"));
                sStats.dfMin =
                    CPLAtofM(CPLGetXMLValue(psStatistics, "Minimum", "0"));
                sStats.dfMax =
                    CPLAtofM(CPLGetXMLValue(psStatistics, "Maximum", "0"));
                sStats.dfMean =
                    CPLAtofM(CPLGetXMLValue(psStatistics, "Mean", "0"));
                sStats.dfStdDev =
                    CPLAtofM(CPLGetXMLValue(psStatistics, "StdDev", "0"));
                sStats.nValidCount = static_cast<GUInt64>(CPLAtoGIntBig(
                    CPLGetXMLValue(psStatistics, "ValidSampleCount", "0")));
                d->m_oMapArray[oKey].stats = sStats;
            }

            const char *pszOverviewFile =
                CPLGetXMLValue(psIter, "OverviewFile", nullptr);
            if (pszOverviewFile)
            {
                d->m_oMapArray[oKey].osOvrFilename = pszOverviewFile;
            }
        }
        else
        {
            CPLXMLNode *psNextBackup = psIter->psNext;
            psIter->psNext = nullptr;
            d->m_apoOtherNodes.emplace_back(
                CPLXMLTreeCloser(CPLCloneXMLTree(psIter)));
            psIter->psNext = psNextBackup;
        }
    }
}

/************************************************************************/
/*                       GDALPamMultiDim::Save()                        */
/************************************************************************/

void GDALPamMultiDim::Save()
{
    CPLXMLTreeCloser oTree(
        CPLCreateXMLNode(nullptr, CXT_Element, "PAMDataset"));
    for (const auto &poOtherNode : d->m_apoOtherNodes)
    {
        CPLAddXMLChild(oTree.get(), CPLCloneXMLTree(poOtherNode.get()));
    }
    for (const auto &kv : d->m_oMapArray)
    {
        CPLXMLNode *psArrayNode =
            CPLCreateXMLNode(oTree.get(), CXT_Element, "Array");
        CPLAddXMLAttributeAndValue(psArrayNode, "name", kv.first.first.c_str());
        if (!kv.first.second.empty())
        {
            CPLAddXMLAttributeAndValue(psArrayNode, "context",
                                       kv.first.second.c_str());
        }
        if (kv.second.poSRS)
        {
            char *pszWKT = nullptr;
            {
                CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
                const char *const apszOptions[] = {"FORMAT=WKT2", nullptr};
                kv.second.poSRS->exportToWkt(&pszWKT, apszOptions);
            }
            CPLXMLNode *psSRSNode =
                CPLCreateXMLElementAndValue(psArrayNode, "SRS", pszWKT);
            CPLFree(pszWKT);
            const auto &mapping =
                kv.second.poSRS->GetDataAxisToSRSAxisMapping();
            CPLString osMapping;
            for (size_t i = 0; i < mapping.size(); ++i)
            {
                if (!osMapping.empty())
                    osMapping += ",";
                osMapping += CPLSPrintf("%d", mapping[i]);
            }
            CPLAddXMLAttributeAndValue(psSRSNode, "dataAxisToSRSAxisMapping",
                                       osMapping.c_str());

            const double dfCoordinateEpoch =
                kv.second.poSRS->GetCoordinateEpoch();
            if (dfCoordinateEpoch > 0)
            {
                std::string osCoordinateEpoch =
                    CPLSPrintf("%f", dfCoordinateEpoch);
                if (osCoordinateEpoch.find('.') != std::string::npos)
                {
                    while (osCoordinateEpoch.back() == '0')
                        osCoordinateEpoch.resize(osCoordinateEpoch.size() - 1);
                }
                CPLAddXMLAttributeAndValue(psSRSNode, "coordinateEpoch",
                                           osCoordinateEpoch.c_str());
            }
        }

        if (kv.second.stats.bHasStats)
        {
            CPLXMLNode *psStats =
                CPLCreateXMLNode(psArrayNode, CXT_Element, "Statistics");
            CPLCreateXMLElementAndValue(psStats, "ApproxStats",
                                        kv.second.stats.bApproxStats ? "1"
                                                                     : "0");
            CPLCreateXMLElementAndValue(
                psStats, "Minimum", CPLSPrintf("%.17g", kv.second.stats.dfMin));
            CPLCreateXMLElementAndValue(
                psStats, "Maximum", CPLSPrintf("%.17g", kv.second.stats.dfMax));
            CPLCreateXMLElementAndValue(
                psStats, "Mean", CPLSPrintf("%.17g", kv.second.stats.dfMean));
            CPLCreateXMLElementAndValue(
                psStats, "StdDev",
                CPLSPrintf("%.17g", kv.second.stats.dfStdDev));
            CPLCreateXMLElementAndValue(
                psStats, "ValidSampleCount",
                CPLSPrintf(CPL_FRMT_GUIB, kv.second.stats.nValidCount));
        }

        if (!kv.second.osOvrFilename.empty())
        {
            CPLCreateXMLElementAndValue(psArrayNode, "OverviewFile",
                                        kv.second.osOvrFilename.c_str());
        }
    }

    int bSaved;
    CPLErrorAccumulator oErrorAccumulator;
    {
        auto oAccumulator = oErrorAccumulator.InstallForCurrentScope();
        CPL_IGNORE_RET_VAL(oAccumulator);
        bSaved =
            CPLSerializeXMLTreeToFile(oTree.get(), d->m_osPamFilename.c_str());
    }

    const char *pszNewPam = nullptr;
    if (!bSaved && PamGetProxy(d->m_osFilename.c_str()) == nullptr &&
        ((pszNewPam = PamAllocateProxy(d->m_osFilename.c_str())) != nullptr))
    {
        CPLErrorReset();
        CPLSerializeXMLTreeToFile(oTree.get(), pszNewPam);
    }
    else
    {
        oErrorAccumulator.ReplayErrors();
    }
}

/************************************************************************/
/*                GDALPamMultiDim::GetOverviewFilename()                */
/************************************************************************/

/** Return the file name of the overview filene name for the specified
 * array
 */
std::string
GDALPamMultiDim::GetOverviewFilename(const std::string &osArrayFullName,
                                     const std::string &osContext)
{
    Load();
    const auto oKey = std::make_pair(osArrayFullName, osContext);
    auto oIter = d->m_oMapArray.find(oKey);
    if (oIter != d->m_oMapArray.end())
        return oIter->second.osOvrFilename;

    return std::string();
}

/************************************************************************/
/*             GDALPamMultiDim::GenerateOverviewFilename()              */
/************************************************************************/

/** Ggenerate an overview filene name for the specified
 * array
 */
std::string
GDALPamMultiDim::GenerateOverviewFilename(const std::string &osArrayFullName,
                                          const std::string &osContext)
{
    Load();

    constexpr int ARBITRARY_ITERATION_COUNT = 1000;
    for (int i = 0; i < ARBITRARY_ITERATION_COUNT; ++i)
    {
        std::string osOvrFilename(d->m_osFilename);
        osOvrFilename += '.';
        osOvrFilename += std::to_string(i);
        osOvrFilename += ".ovr";
        VSIStatBufL sStatBuf;
        if (VSIStatL(osOvrFilename.c_str(), &sStatBuf) != 0)
        {
            d->m_bDirty = true;
            const auto oKey = std::make_pair(osArrayFullName, osContext);
            d->m_oMapArray[oKey].osOvrFilename = osOvrFilename;
            return osOvrFilename;
        }
    }
    CPLError(CE_Failure, CPLE_AppDefined,
             "Cannot establish overview filename for array %s",
             osArrayFullName.c_str());
    return std::string();
}

/************************************************************************/
/*                   GDALPamMultiDim::GetSpatialRef()                   */
/************************************************************************/

std::shared_ptr<OGRSpatialReference>
GDALPamMultiDim::GetSpatialRef(const std::string &osArrayFullName,
                               const std::string &osContext)
{
    Load();
    auto oIter =
        d->m_oMapArray.find(std::make_pair(osArrayFullName, osContext));
    if (oIter != d->m_oMapArray.end())
        return oIter->second.poSRS;
    return nullptr;
}

/************************************************************************/
/*                   GDALPamMultiDim::SetSpatialRef()                   */
/************************************************************************/

void GDALPamMultiDim::SetSpatialRef(const std::string &osArrayFullName,
                                    const std::string &osContext,
                                    const OGRSpatialReference *poSRS)
{
    Load();
    d->m_bDirty = true;
    if (poSRS && !poSRS->IsEmpty())
        d->m_oMapArray[std::make_pair(osArrayFullName, osContext)].poSRS.reset(
            poSRS->Clone());
    else
        d->m_oMapArray[std::make_pair(osArrayFullName, osContext)]
            .poSRS.reset();
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

CPLErr GDALPamMultiDim::GetStatistics(const std::string &osArrayFullName,
                                      const std::string &osContext,
                                      bool bApproxOK, double *pdfMin,
                                      double *pdfMax, double *pdfMean,
                                      double *pdfStdDev, GUInt64 *pnValidCount)
{
    Load();
    auto oIter =
        d->m_oMapArray.find(std::make_pair(osArrayFullName, osContext));
    if (oIter == d->m_oMapArray.end())
        return CE_Failure;
    const auto &stats = oIter->second.stats;
    if (!stats.bHasStats)
        return CE_Failure;
    if (!bApproxOK && stats.bApproxStats)
        return CE_Failure;
    if (pdfMin)
        *pdfMin = stats.dfMin;
    if (pdfMax)
        *pdfMax = stats.dfMax;
    if (pdfMean)
        *pdfMean = stats.dfMean;
    if (pdfStdDev)
        *pdfStdDev = stats.dfStdDev;
    if (pnValidCount)
        *pnValidCount = stats.nValidCount;
    return CE_None;
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

void GDALPamMultiDim::SetStatistics(const std::string &osArrayFullName,
                                    const std::string &osContext,
                                    bool bApproxStats, double dfMin,
                                    double dfMax, double dfMean,
                                    double dfStdDev, GUInt64 nValidCount)
{
    Load();
    d->m_bDirty = true;
    auto &stats =
        d->m_oMapArray[std::make_pair(osArrayFullName, osContext)].stats;
    stats.bHasStats = true;
    stats.bApproxStats = bApproxStats;
    stats.dfMin = dfMin;
    stats.dfMax = dfMax;
    stats.dfMean = dfMean;
    stats.dfStdDev = dfStdDev;
    stats.nValidCount = nValidCount;
}

/************************************************************************/
/*                          ClearStatistics()                           */
/************************************************************************/

void GDALPamMultiDim::ClearStatistics(const std::string &osArrayFullName,
                                      const std::string &osContext)
{
    Load();
    d->m_bDirty = true;
    d->m_oMapArray[std::make_pair(osArrayFullName, osContext)].stats.bHasStats =
        false;
}

/************************************************************************/
/*                          ClearStatistics()                           */
/************************************************************************/

void GDALPamMultiDim::ClearStatistics()
{
    Load();
    d->m_bDirty = true;
    for (auto &kv : d->m_oMapArray)
        kv.second.stats.bHasStats = false;
}

/************************************************************************/
/*                               GetPAM()                               */
/************************************************************************/

/*static*/ std::shared_ptr<GDALPamMultiDim>
GDALPamMultiDim::GetPAM(const std::shared_ptr<GDALMDArray> &poParent)
{
    auto poPamArray = dynamic_cast<GDALPamMDArray *>(poParent.get());
    if (poPamArray)
        return poPamArray->GetPAM();
    return nullptr;
}

/************************************************************************/
/*                            GDALPamMDArray                            */
/************************************************************************/

GDALPamMDArray::GDALPamMDArray(const std::string &osParentName,
                               const std::string &osName,
                               const std::shared_ptr<GDALPamMultiDim> &poPam,
                               const std::string &osContext)
    :
#if !defined(COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT)
      GDALAbstractMDArray(osParentName, osName),
#endif
      GDALMDArray(osParentName, osName, osContext), m_poPam(poPam)
{
}

/************************************************************************/
/*                   GDALPamMDArray::SetSpatialRef()                    */
/************************************************************************/

bool GDALPamMDArray::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    if (!m_poPam)
        return false;
    m_poPam->SetSpatialRef(GetFullName(), GetContext(), poSRS);
    return true;
}

/************************************************************************/
/*                   GDALPamMDArray::GetSpatialRef()                    */
/************************************************************************/

std::shared_ptr<OGRSpatialReference> GDALPamMDArray::GetSpatialRef() const
{
    if (!m_poPam)
        return nullptr;
    return m_poPam->GetSpatialRef(GetFullName(), GetContext());
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

CPLErr GDALPamMDArray::GetStatistics(bool bApproxOK, bool bForce,
                                     double *pdfMin, double *pdfMax,
                                     double *pdfMean, double *pdfStdDev,
                                     GUInt64 *pnValidCount,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    if (m_poPam && m_poPam->GetStatistics(GetFullName(), GetContext(),
                                          bApproxOK, pdfMin, pdfMax, pdfMean,
                                          pdfStdDev, pnValidCount) == CE_None)
    {
        return CE_None;
    }
    if (!bForce)
        return CE_Warning;

    return GDALMDArray::GetStatistics(bApproxOK, bForce, pdfMin, pdfMax,
                                      pdfMean, pdfStdDev, pnValidCount,
                                      pfnProgress, pProgressData);
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

bool GDALPamMDArray::SetStatistics(bool bApproxStats, double dfMin,
                                   double dfMax, double dfMean, double dfStdDev,
                                   GUInt64 nValidCount,
                                   CSLConstList /* papszOptions */)
{
    if (!m_poPam)
        return false;
    m_poPam->SetStatistics(GetFullName(), GetContext(), bApproxStats, dfMin,
                           dfMax, dfMean, dfStdDev, nValidCount);
    return true;
}

/************************************************************************/
/*                          ClearStatistics()                           */
/************************************************************************/

void GDALPamMDArray::ClearStatistics()
{
    if (!m_poPam)
        return;
    m_poPam->ClearStatistics(GetFullName(), GetContext());
}

//! @endcond
