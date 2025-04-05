/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector grid minimum/maximum/range/count/average-distance/average-distance-pts" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GRID_DATA_METRICS_INCLUDED
#define GDALALG_VECTOR_GRID_DATA_METRICS_INCLUDED

#include "gdalalg_vector_grid.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALVectorGridDataMetricsAbstractAlgorithm           */
/************************************************************************/

class GDALVectorGridDataMetricsAbstractAlgorithm /* non final */
    : public GDALVectorGridAbstractAlgorithm
{
  public:
    static constexpr const char *HELP_URL = "/programs/gdal_vector_grid.html";

  protected:
    GDALVectorGridDataMetricsAbstractAlgorithm(const std::string &name,
                                               const std::string &description,
                                               const std::string &helpURL,
                                               const std::string &method);

    std::string GetGridAlgorithm() const override;

  private:
    std::string m_method{};
};

/************************************************************************/
/*                      GDALVectorGridMinimumAlgorithm                  */
/************************************************************************/

class GDALVectorGridMinimumAlgorithm final
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "minimum";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the minimum value "
        "in the search ellipse.";

    GDALVectorGridMinimumAlgorithm()
        : GDALVectorGridDataMetricsAbstractAlgorithm(NAME, DESCRIPTION,
                                                     HELP_URL, "minimum")
    {
    }
};

/************************************************************************/
/*                      GDALVectorGridMaximumAlgorithm                  */
/************************************************************************/

class GDALVectorGridMaximumAlgorithm final
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "maximum";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the maximum value "
        "in the search ellipse.";

    GDALVectorGridMaximumAlgorithm()
        : GDALVectorGridDataMetricsAbstractAlgorithm(NAME, DESCRIPTION,
                                                     HELP_URL, "maximum")
    {
    }
};

/************************************************************************/
/*                       GDALVectorGridRangeAlgorithm                   */
/************************************************************************/

class GDALVectorGridRangeAlgorithm final
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "range";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the difference "
        "between the minimum and maximum values in the search ellipse.";

    GDALVectorGridRangeAlgorithm()
        : GDALVectorGridDataMetricsAbstractAlgorithm(NAME, DESCRIPTION,
                                                     HELP_URL, "range")
    {
    }
};

/************************************************************************/
/*                       GDALVectorGridCountAlgorithm                   */
/************************************************************************/

class GDALVectorGridCountAlgorithm final
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "count";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the number of "
        "points in the search ellipse.";

    GDALVectorGridCountAlgorithm()
        : GDALVectorGridDataMetricsAbstractAlgorithm(NAME, DESCRIPTION,
                                                     HELP_URL, "count")
    {
    }
};

/************************************************************************/
/*                 GDALVectorGridAverageDistanceAlgorithm               */
/************************************************************************/

class GDALVectorGridAverageDistanceAlgorithm final
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "average-distance";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the average "
        "distance between the grid node (center of the search ellipse) and all "
        "of the data points in the search ellipse.";

    GDALVectorGridAverageDistanceAlgorithm()
        : GDALVectorGridDataMetricsAbstractAlgorithm(
              NAME, DESCRIPTION, HELP_URL, "average_distance")
    {
    }
};

/************************************************************************/
/*             GDALVectorGridAverageDistancePointsAlgorithm             */
/************************************************************************/

class GDALVectorGridAverageDistancePointsAlgorithm final
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "average-distance-points";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the average "
        "distance between the data points in the search ellipse.";

    GDALVectorGridAverageDistancePointsAlgorithm()
        : GDALVectorGridDataMetricsAbstractAlgorithm(
              NAME, DESCRIPTION, HELP_URL, "average_distance_pts")
    {
    }
};

//! @endcond

#endif
