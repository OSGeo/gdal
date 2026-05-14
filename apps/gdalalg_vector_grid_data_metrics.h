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
/*              GDALVectorGridDataMetricsAbstractAlgorithm              */
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
                                               const std::string &method,
                                               bool standaloneStep);

    ~GDALVectorGridDataMetricsAbstractAlgorithm() override;

    std::string GetGridAlgorithm() const override;

  private:
    std::string m_method{};
};

/************************************************************************/
/*                    GDALVectorGridMinimumAlgorithm                    */
/************************************************************************/

class GDALVectorGridMinimumAlgorithm /* non final */
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "minimum";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the minimum value "
        "in the search ellipse.";

    explicit GDALVectorGridMinimumAlgorithm(bool standaloneStep = false)
        : GDALVectorGridDataMetricsAbstractAlgorithm(
              NAME, DESCRIPTION, HELP_URL, "minimum", standaloneStep)
    {
    }

    ~GDALVectorGridMinimumAlgorithm() override;
};

/************************************************************************/
/*               GDALVectorGridMinimumAlgorithmStandalone               */
/************************************************************************/

class GDALVectorGridMinimumAlgorithmStandalone final
    : public GDALVectorGridMinimumAlgorithm
{
  public:
    GDALVectorGridMinimumAlgorithmStandalone()
        : GDALVectorGridMinimumAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridMinimumAlgorithmStandalone() override;
};

/************************************************************************/
/*                    GDALVectorGridMaximumAlgorithm                    */
/************************************************************************/

class GDALVectorGridMaximumAlgorithm /* non final */
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "maximum";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the maximum value "
        "in the search ellipse.";

    explicit GDALVectorGridMaximumAlgorithm(bool standaloneStep = false)
        : GDALVectorGridDataMetricsAbstractAlgorithm(
              NAME, DESCRIPTION, HELP_URL, "maximum", standaloneStep)
    {
    }

    ~GDALVectorGridMaximumAlgorithm() override;
};

/************************************************************************/
/*               GDALVectorGridMaximumAlgorithmStandalone               */
/************************************************************************/

class GDALVectorGridMaximumAlgorithmStandalone final
    : public GDALVectorGridMaximumAlgorithm
{
  public:
    GDALVectorGridMaximumAlgorithmStandalone()
        : GDALVectorGridMaximumAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridMaximumAlgorithmStandalone() override;
};

/************************************************************************/
/*                     GDALVectorGridRangeAlgorithm                     */
/************************************************************************/

class GDALVectorGridRangeAlgorithm /* non final */
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "range";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the difference "
        "between the minimum and maximum values in the search ellipse.";

    explicit GDALVectorGridRangeAlgorithm(bool standaloneStep = false)
        : GDALVectorGridDataMetricsAbstractAlgorithm(
              NAME, DESCRIPTION, HELP_URL, "range", standaloneStep)
    {
    }

    ~GDALVectorGridRangeAlgorithm() override;
};

/************************************************************************/
/*                GDALVectorGridRangeAlgorithmStandalone                */
/************************************************************************/

class GDALVectorGridRangeAlgorithmStandalone final
    : public GDALVectorGridRangeAlgorithm
{
  public:
    GDALVectorGridRangeAlgorithmStandalone()
        : GDALVectorGridRangeAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridRangeAlgorithmStandalone() override;
};

/************************************************************************/
/*                     GDALVectorGridCountAlgorithm                     */
/************************************************************************/

class GDALVectorGridCountAlgorithm /* non final */
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "count";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the number of "
        "points in the search ellipse.";

    explicit GDALVectorGridCountAlgorithm(bool standaloneStep = false)
        : GDALVectorGridDataMetricsAbstractAlgorithm(
              NAME, DESCRIPTION, HELP_URL, "count", standaloneStep)
    {
    }

    ~GDALVectorGridCountAlgorithm() override;
};

/************************************************************************/
/*                GDALVectorGridCountAlgorithmStandalone                */
/************************************************************************/

class GDALVectorGridCountAlgorithmStandalone final
    : public GDALVectorGridCountAlgorithm
{
  public:
    GDALVectorGridCountAlgorithmStandalone()
        : GDALVectorGridCountAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridCountAlgorithmStandalone() override;
};

/************************************************************************/
/*                GDALVectorGridAverageDistanceAlgorithm                */
/************************************************************************/

class GDALVectorGridAverageDistanceAlgorithm /* non final */
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "average-distance";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the average "
        "distance between the grid node (center of the search ellipse) and all "
        "of the data points in the search ellipse.";

    explicit GDALVectorGridAverageDistanceAlgorithm(bool standaloneStep = false)
        : GDALVectorGridDataMetricsAbstractAlgorithm(
              NAME, DESCRIPTION, HELP_URL, "average_distance", standaloneStep)
    {
    }

    ~GDALVectorGridAverageDistanceAlgorithm() override;
};

/************************************************************************/
/*           GDALVectorGridAverageDistanceAlgorithmStandalone           */
/************************************************************************/

class GDALVectorGridAverageDistanceAlgorithmStandalone final
    : public GDALVectorGridAverageDistanceAlgorithm
{
  public:
    GDALVectorGridAverageDistanceAlgorithmStandalone()
        : GDALVectorGridAverageDistanceAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridAverageDistanceAlgorithmStandalone() override;
};

/************************************************************************/
/*             GDALVectorGridAverageDistancePointsAlgorithm             */
/************************************************************************/

class GDALVectorGridAverageDistancePointsAlgorithm /* non final */
    : public GDALVectorGridDataMetricsAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "average-distance-points";
    static constexpr const char *DESCRIPTION =
        "Create a regular grid from scattered points using the average "
        "distance between the data points in the search ellipse.";

    explicit GDALVectorGridAverageDistancePointsAlgorithm(
        bool standaloneStep = false)
        : GDALVectorGridDataMetricsAbstractAlgorithm(
              NAME, DESCRIPTION, HELP_URL, "average_distance_pts",
              standaloneStep)
    {
    }

    ~GDALVectorGridAverageDistancePointsAlgorithm() override;
};

/************************************************************************/
/*        GDALVectorGridAverageDistancePointsAlgorithmStandalone        */
/************************************************************************/

class GDALVectorGridAverageDistancePointsAlgorithmStandalone final
    : public GDALVectorGridAverageDistancePointsAlgorithm
{
  public:
    GDALVectorGridAverageDistancePointsAlgorithmStandalone()
        : GDALVectorGridAverageDistancePointsAlgorithm(
              /* standaloneStep = */ true)
    {
    }

    ~GDALVectorGridAverageDistancePointsAlgorithmStandalone() override;
};

//! @endcond

#endif
