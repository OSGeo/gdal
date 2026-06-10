/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Step of a gdal mdim pipeline
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALMDIMPIPELINESTEPALGORITHM_INCLUDED
#define GDALMDIMPIPELINESTEPALGORITHM_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalpipelinestepalgorithm.h"

/************************************************************************/
/*                    GDALMdimPipelineStepAlgorithm                     */
/************************************************************************/

class GDALMdimPipelineStepAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    ~GDALMdimPipelineStepAlgorithm() override;

  protected:
    GDALMdimPipelineStepAlgorithm(const std::string &name,
                                  const std::string &description,
                                  const std::string &helpURL,
                                  const ConstructorOptions &options);

    friend class GDALMdimPipelineAlgorithm;

    int GetInputType() const override
    {
        return GDAL_OF_MULTIDIM_RASTER;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_MULTIDIM_RASTER;
    }

    void SetOutputVRTCompatible(bool b);
};

//! @endcond

#endif
