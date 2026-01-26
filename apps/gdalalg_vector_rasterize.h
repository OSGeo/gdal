/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector rasterize" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_RASTERIZE_INCLUDED
#define GDALALG_VECTOR_RASTERIZE_INCLUDED

#include <algorithm>
#include <limits>

#include "gdalalg_abstract_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorRasterizeAlgorithm                     */
/************************************************************************/

class GDALVectorRasterizeAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "rasterize";
    static constexpr const char *DESCRIPTION =
        "Burns vector geometries into a raster.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_rasterize.html";

    explicit GDALVectorRasterizeAlgorithm(bool standaloneStep = false);

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    int GetInputType() const override
    {
        return GDAL_OF_VECTOR;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_RASTER;
    }

    std::vector<int> m_bands{};
    bool m_invert = false;
    bool m_allTouched = false;
    std::vector<double> m_burnValues{};
    std::string m_attributeName{};
    bool m_3d = false;
    bool m_add = false;
    std::string m_layerName{};  // mutually exclusive with m_sql
    std::string m_where{};
    std::string m_sql{};  // mutually exclusive with m_layerName
    std::string m_dialect{};
    double m_nodata = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> m_initValues{};
    std::string m_srs{};
    std::vector<std::string> m_transformerOption{};
    std::vector<double> m_targetExtent{};
    std::vector<double>
        m_targetResolution{};  // Mutually exclusive with targetSize
    bool m_tap = false;
    std::vector<int>
        m_targetSize{};  // Mutually exclusive with targetResolution
    std::string m_outputType{};
    std::string m_optimization{};  // {AUTO|VECTOR|RASTER}
};

/************************************************************************/
/*                GDALVectorRasterizeAlgorithmStandalone                */
/************************************************************************/

class GDALVectorRasterizeAlgorithmStandalone final
    : public GDALVectorRasterizeAlgorithm
{
  public:
    GDALVectorRasterizeAlgorithmStandalone()
        : GDALVectorRasterizeAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorRasterizeAlgorithmStandalone() override;
};

//! @endcond

#endif
