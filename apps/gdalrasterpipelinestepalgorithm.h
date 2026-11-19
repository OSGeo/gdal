/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster/vector pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALRASTERPIPELINESTEPALGORITHM_INCLUDED
#define GDALRASTERPIPELINESTEPALGORITHM_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalpipelinestepalgorithm.h"

/************************************************************************/
/*                   GDALRasterPipelineStepAlgorithm                    */
/************************************************************************/

class GDALRasterPipelineStepAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    ~GDALRasterPipelineStepAlgorithm() override;

  protected:
    GDALRasterPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    bool standaloneStep);

    GDALRasterPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    const ConstructorOptions &options);

    friend class GDALRasterPipelineAlgorithm;
    friend class GDALRasterMosaicStackCommonAlgorithm;

    int GetInputType() const override
    {
        return GDAL_OF_RASTER;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_RASTER;
    }

    void SetOutputVRTCompatible(bool b);
};

//! @endcond

#endif
