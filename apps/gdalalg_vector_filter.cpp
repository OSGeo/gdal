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
#include "ogr_p.h"

#include <set>

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
    AddActiveLayerArg(&m_activeLayer);
    AddBBOXArg(&m_bbox);
    AddArg("where", 0,
           _("Attribute query in a restricted form of the queries used in the "
             "SQL WHERE statement"),
           &m_where)
        .SetReadFromFileAtSyntaxAllowed()
        .SetMetaVar("<WHERE>|@<filename>")
        .SetRemoveSQLCommentsEnabled();
}

/************************************************************************/
/*               GDALVectorFilterAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALVectorFilterAlgorithm::RunStep(GDALProgressFunc, void *)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    const int nLayerCount = poSrcDS->GetLayerCount();

    bool ret = true;
    if (m_bbox.size() == 4)
    {
        const double xmin = m_bbox[0];
        const double ymin = m_bbox[1];
        const double xmax = m_bbox[2];
        const double ymax = m_bbox[3];
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (poSrcLayer && (m_activeLayer.empty() ||
                               m_activeLayer == poSrcLayer->GetDescription()))
                poSrcLayer->SetSpatialFilterRect(xmin, ymin, xmax, ymax);
        }
    }

    if (ret && !m_where.empty())
    {
        for (int i = 0; i < nLayerCount; ++i)
        {
            auto poSrcLayer = poSrcDS->GetLayer(i);
            ret = ret && (poSrcLayer != nullptr);
            if (ret && (m_activeLayer.empty() ||
                        m_activeLayer == poSrcLayer->GetDescription()))
            {
                ret = poSrcLayer->SetAttributeFilter(m_where.c_str()) ==
                      OGRERR_NONE;
            }
        }
    }

    if (ret)
    {
        m_outputDataset.Set(poSrcDS);
    }

    return ret;
}

//! @endcond
