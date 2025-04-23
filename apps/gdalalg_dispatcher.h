/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal subcommand dispatcher
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_DISPATCHER_INCLUDED
#define GDALALG_DISPATCHER_INCLUDED

#include "gdalalgorithm.h"

#include "gdal_priv.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALDispatcherAlgorithm                        */
/************************************************************************/

template <class RasterDispatcher, class VectorDispatcher>
class GDALDispatcherAlgorithm : public GDALAlgorithm
{
  public:
    GDALDispatcherAlgorithm(const std::string &name,
                            const std::string &description,
                            const std::string &helpURL)
        : GDALAlgorithm(name, description, helpURL),
          m_rasterDispatcher(std::make_unique<RasterDispatcher>(
              /* openForMixedRasterVector = */ true)),
          m_vectorDispatcher(std::make_unique<VectorDispatcher>())
    {
        // A "info" dispacher command is a shortcut for something like
        // "raster info", "vector info". Best to expose the latter.
        SetDisplayInJSONUsage(false);
    }

    bool
    ParseCommandLineArguments(const std::vector<std::string> &args) override;

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

  private:
    std::unique_ptr<RasterDispatcher> m_rasterDispatcher{};
    std::unique_ptr<VectorDispatcher> m_vectorDispatcher{};
    bool m_showUsage = true;

    bool RunImpl(GDALProgressFunc, void *) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The Run() method should not be called directly on the \"gdal "
                 "%s\" program.",
                 GetName().c_str());
        return false;
    }
};

/************************************************************************/
/*         GDALDispatcherAlgorithm::ParseCommandLineArguments()         */
/************************************************************************/

template <class RasterDispatcher, class VectorDispatcher>
bool GDALDispatcherAlgorithm<RasterDispatcher, VectorDispatcher>::
    ParseCommandLineArguments(const std::vector<std::string> &args)
{
    // We first try to process with the raster specific algorithm (that has
    // been instantiated in a special way to accept both raster and vector
    // input datasets). If the raster specific algorithm can parse successfully
    // the arguments *and* the dataset is a raster one, then continue processing
    // with it. Otherwise try with the vector specific algorithm.

    bool ok;
    if (args.size() > 1)
    {
        // Silence errors as it might be rather for the vector algorithm
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        ok = m_rasterDispatcher->ParseCommandLineArguments(args);
    }
    else
    {
        // If there's just a single argument, we don't need to silence errors
        // as this will trigger a legitimate error message about the subcommand.
        ok = m_rasterDispatcher->ParseCommandLineArguments(args);
    }

    if (m_rasterDispatcher->PropagateSpecialActionTo(this))
    {
        return true;
    }

    if (ok)
    {
        auto poDS = m_rasterDispatcher->GetDatasetRef();
        // cppcheck-suppress knownConditionTrueFalse
        if (poDS &&
            (poDS->GetRasterCount() > 0 || poDS->GetMetadata("SUBDATASETS")))
        {
            if (poDS->GetLayerCount() != 0)
            {
                m_showUsage = false;
                CPLError(CE_Failure, CPLE_AppDefined,
                         "'%s' has both raster and vector content. "
                         "Please use 'gdal raster %s' or 'gdal vector %s'.",
                         poDS->GetDescription(), GetName().c_str(),
                         GetName().c_str());
                return false;
            }

            m_selectedSubAlg = m_rasterDispatcher.get();
            std::vector<std::string> callPath(m_callPath);
            callPath.push_back("raster");
            m_selectedSubAlg->SetCallPath(callPath);

            return true;
        }
    }
    else if (args.size() <= 1)
    {
        return false;
    }

    auto poDSFromRaster = m_rasterDispatcher->GetDatasetRef();
    // cppcheck-suppress knownConditionTrueFalse
    if (poDSFromRaster)
    {
        m_vectorDispatcher->SetDataset(poDSFromRaster);
    }

    std::vector<std::string> argsWithoutInput;
    bool skipNext = false;
    for (const auto &arg : args)
    {
        if (arg == "-i" || arg == "--input")
        {
            skipNext = true;
        }
        else if (!skipNext)
        {
            if (!STARTS_WITH(arg.c_str(), "--input=") &&
                !(poDSFromRaster && arg == poDSFromRaster->GetDescription()))
            {
                argsWithoutInput.push_back(arg);
            }
        }
        else
        {
            skipNext = false;
        }
    }

    {
        CPLErrorStateBackuper oErrorHandler(CPLQuietErrorHandler);
        ok = m_vectorDispatcher->ParseCommandLineArguments(argsWithoutInput);
    }
    if (ok)
    {
        m_selectedSubAlg = m_vectorDispatcher.get();
        std::vector<std::string> callPath(m_callPath);
        callPath.push_back("vector");
        m_selectedSubAlg->SetCallPath(callPath);

        return true;
    }

    bool ret = false;
    for (const auto &arg : args)
    {
        VSIStatBufL sStat;
        if (VSIStatL(arg.c_str(), &sStat) == 0)
        {
            auto poDS =
                std::unique_ptr<GDALDataset>(GDALDataset::Open(arg.c_str()));
            if (poDS)
            {
                if (poDS->GetRasterCount() > 0 ||
                    poDS->GetMetadata("SUBDATASETS"))
                {
                    if (poDS->GetLayerCount() != 0)
                    {
                        m_showUsage = false;
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "'%s' has both raster and vector content. "
                                 "Please use 'gdal raster %s' or 'gdal "
                                 "vector %s'.",
                                 poDS->GetDescription(), GetName().c_str(),
                                 GetName().c_str());
                        return false;
                    }
                    m_rasterDispatcher = std::make_unique<RasterDispatcher>();
                    auto poDSRaw = poDS.get();
                    m_rasterDispatcher->SetDataset(poDS.release());
                    poDSRaw->Release();
                    m_selectedSubAlg = m_rasterDispatcher.get();
                    std::vector<std::string> callPath(m_callPath);
                    callPath.push_back("raster");
                    m_selectedSubAlg->SetCallPath(callPath);
                    ret = m_selectedSubAlg->ParseCommandLineArguments(
                        argsWithoutInput);
                    break;
                }
                else if (poDS->GetLayerCount() != 0)
                {
                    m_vectorDispatcher = std::make_unique<VectorDispatcher>();
                    auto poDSRaw = poDS.get();
                    m_vectorDispatcher->SetDataset(poDS.release());
                    poDSRaw->Release();
                    m_selectedSubAlg = m_vectorDispatcher.get();
                    std::vector<std::string> callPath(m_callPath);
                    callPath.push_back("vector");
                    m_selectedSubAlg->SetCallPath(callPath);
                    ret = m_selectedSubAlg->ParseCommandLineArguments(
                        argsWithoutInput);
                    break;
                }
            }
        }
    }

    return ret;
}

/************************************************************************/
/*                 GDALDispatcherAlgorithm::GetUsageForCLI()            */
/************************************************************************/

template <class RasterDispatcher, class VectorDispatcher>
std::string
GDALDispatcherAlgorithm<RasterDispatcher, VectorDispatcher>::GetUsageForCLI(
    bool shortUsage, const UsageOptions &usageOptions) const
{
    if (m_selectedSubAlg)
    {
        return m_selectedSubAlg->GetUsageForCLI(shortUsage, usageOptions);
    }
    if (m_showUsage)
    {
        return GDALAlgorithm::GetUsageForCLI(shortUsage, usageOptions);
    }
    return std::string();
}

//! @endcond

#endif  // GDALALG_DISPATCHER_INCLUDED
