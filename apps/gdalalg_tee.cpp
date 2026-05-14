/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "tee" pipeline step
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_tee.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*    GDALTeeStepAlgorithmAbstract::~GDALTeeStepAlgorithmAbstract()     */
/************************************************************************/

GDALTeeStepAlgorithmAbstract::~GDALTeeStepAlgorithmAbstract() = default;

/************************************************************************/
/*       GDALTeeStepAlgorithmAbstract::CopyFilenameBindingsFrom()       */
/************************************************************************/

void GDALTeeStepAlgorithmAbstract::CopyFilenameBindingsFrom(
    const GDALTeeStepAlgorithmAbstract *other)
{
    m_oMapNameToAlg = other->m_oMapNameToAlg;
}

/************************************************************************/
/*             GDALTeeStepAlgorithmAbstract::BindFilename()             */
/************************************************************************/

bool GDALTeeStepAlgorithmAbstract::BindFilename(
    const std::string &filename, GDALAbstractPipelineAlgorithm *alg,
    const std::vector<std::string> &args)
{
    if (cpl::contains(m_oMapNameToAlg, filename))
        return false;
    m_oMapNameToAlg[filename] = std::make_pair(alg, args);
    return true;
}

/************************************************************************/
/*           GDALTeeStepAlgorithmAbstract::HasOutputString()            */
/************************************************************************/

bool GDALTeeStepAlgorithmAbstract::HasOutputString() const
{
    for (const auto &oIter : m_oMapNameToAlg)
    {
        const auto &pipelineAlg = oIter.second.first;
        if (pipelineAlg->HasSteps())
        {
            if (pipelineAlg->HasOutputString())
                return true;
        }
        else
        {
            // Before the tee pipeline has been constructed by
            // GDALTeeStepAlgorithmBase::RunStep(), there is no clean way
            // of knowing if a (future) inner step will have an output string
            // argument, so try to instantiate a step alg from each pipeline
            // token and call HasOutputString() on it.
            const auto &pipelineArgs = oIter.second.second;
            for (const auto &arg : pipelineArgs)
            {
                auto alg = pipelineAlg->GetStepAlg(arg);
                if (!alg)
                {
                    alg = pipelineAlg->GetStepAlg(
                        arg + GDALAbstractPipelineAlgorithm::RASTER_SUFFIX);
                    if (!alg)
                        alg = pipelineAlg->GetStepAlg(
                            arg + GDALAbstractPipelineAlgorithm::VECTOR_SUFFIX);
                }
                if (alg && alg->HasOutputString())
                    return true;
            }
        }
    }
    return false;
}

GDALTeeRasterAlgorithm::~GDALTeeRasterAlgorithm() = default;

GDALTeeVectorAlgorithm::~GDALTeeVectorAlgorithm() = default;

//! @endcond
