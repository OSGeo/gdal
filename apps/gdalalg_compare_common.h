/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Common code between raster compare and mdim compare
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_COMPARE_COMMON_INCLUDED
#define GDALALG_COMPARE_COMMON_INCLUDED

#include "gdalalgorithm.h"

#include <algorithm>
#include <string>
#include <vector>

//! @cond Doxygen_Suppress

class GDALAlgorithm;
class GDALDataset;

/************************************************************************/
/*                          GDALCompareCommon                           */
/************************************************************************/

class GDALCompareCommon
{
  public:
    virtual ~GDALCompareCommon();

  protected:
    GDALCompareCommon();

    static constexpr const char *METRIC_ALL = "all";
    static constexpr const char *METRIC_NONE = "none";
    static constexpr const char *METRIC_DIFF = "diff";
    static constexpr const char *METRIC_RMSD = "RMSD";
    static constexpr const char *METRIC_PSNR = "PSNR";

    static constexpr const char *METRIC_DEFAULT = METRIC_DIFF;
    std::vector<std::string> m_metrics{METRIC_DEFAULT};

    GDALArgDatasetValue m_referenceDataset{};

    std::vector<std::string> m_array{};

    bool m_skipBinary = false;
    int m_retCode = 0;

    bool HasMetric(const char *pszMetric) const
    {
        CPLAssert(!EQUAL(pszMetric, METRIC_ALL));
        return std::find(m_metrics.begin(), m_metrics.end(), pszMetric) !=
                   m_metrics.end() ||
               (!EQUAL(pszMetric, METRIC_NONE) &&
                std::find(m_metrics.begin(), m_metrics.end(), METRIC_ALL) !=
                    m_metrics.end());
    }

    static bool BinaryComparison(GDALAlgorithm *alg,
                                 std::vector<std::string> &aosReport,
                                 GDALDataset *poRefDS, GDALDataset *poInputDS);
};

//! @endcond

#endif
