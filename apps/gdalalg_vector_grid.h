/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector grid" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GRID_INCLUDED
#define GDALALG_VECTOR_GRID_INCLUDED

#include "gdalalgorithm.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALVectorGridAlgorithm                         */
/************************************************************************/

class GDALVectorGridAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "grid";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_grid.html";

    GDALVectorGridAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
};

/************************************************************************/
/*                    GDALVectorGridAbstractAlgorithm                   */
/************************************************************************/

class GDALVectorGridAbstractAlgorithm /* non final */
    : public GDALAlgorithm
{
  protected:
    std::vector<std::string> m_inputFormats{};
    std::string m_outputFormat{};
    std::vector<std::string> m_openOptions{};
    GDALArgDatasetValue m_inputDataset{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite{false};
    std::vector<double> m_targetExtent{};
    std::vector<double>
        m_targetResolution{};  // Mutually exclusive with targetSize
    std::vector<int>
        m_targetSize{};  // Mutually exclusive with targetResolution
    std::string m_outputType{"Float64"};
    std::string m_crs{};
    std::vector<std::string> m_layers{};
    std::string m_sql{};
    std::string m_zField{};
    double m_zOffset = 0;
    double m_zMultiply = 1;
    std::vector<double> m_bbox{};

    // Common per-algorithm parameters
    double m_radius1 = 0.0;
    double m_radius2 = 0.0;
    double m_radius = 0.0;
    double m_angle = 0.0;
    int m_minPoints = 0;
    int m_maxPoints = std::numeric_limits<int>::max();
    int m_minPointsPerQuadrant = 0;
    int m_maxPointsPerQuadrant = std::numeric_limits<int>::max();
    double m_nodata = 0;

    GDALVectorGridAbstractAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL);

    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALInConstructionAlgorithmArg &AddRadiusArg();
    void AddRadius1AndRadius2Arg();
    GDALInConstructionAlgorithmArg &AddAngleArg();
    GDALInConstructionAlgorithmArg &AddMinPointsArg();
    GDALInConstructionAlgorithmArg &AddMaxPointsArg();
    void AddMinMaxPointsPerQuadrantArg();
    GDALInConstructionAlgorithmArg &AddNodataArg();

    virtual std::string GetGridAlgorithm() const = 0;
};

//! @endcond

#endif
