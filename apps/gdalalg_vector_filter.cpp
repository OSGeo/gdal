/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "filter" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_filter.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*         GDALVectorFilterAlgorithm::GDALVectorFilterAlgorithm()       */
/************************************************************************/

GDALVectorFilterAlgorithm::GDALVectorFilterAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    auto &arg =
        AddArg("bbox", 0, _("Bounding box as xmin,ymin,xmax,ymax"), &m_bbox)
            .SetRepeatedArgAllowed(false)
            .SetMinCount(4)
            .SetMaxCount(4)
            .SetDisplayHintAboutRepetition(false);
    arg.AddValidationAction(
        [&arg]()
        {
            const auto &val = arg.Get<std::vector<double>>();
            CPLAssert(val.size() == 4);
            if (!(val[0] <= val[2]) || !(val[1] <= val[3]))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Value of 'bbox' should be xmin,ymin,xmax,ymax with "
                         "xmin <= xmax and ymin <= ymax");
                return false;
            }
            return true;
        });
}

/************************************************************************/
/*               GDALVectorFilterAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALVectorFilterAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    bool ret = true;
    if (m_bbox.size() == 4)
    {
        const double xmin = m_bbox[0];
        const double ymin = m_bbox[1];
        const double xmax = m_bbox[2];
        const double ymax = m_bbox[3];
        auto poSrcDS = m_inputDataset.GetDatasetRef();
        const int nLayerCount = poSrcDS->GetLayerCount();
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (poSrcLayer)
                poSrcLayer->SetSpatialFilterRect(xmin, ymin, xmax, ymax);
        }
    }

    if (ret)
    {
        m_outputDataset.Set(m_inputDataset.GetDatasetRef());
    }

    return ret;
}

//! @endcond
