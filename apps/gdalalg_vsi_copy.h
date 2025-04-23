/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vsi copy" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VSI_COPY_INCLUDED
#define GDALALG_VSI_COPY_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                          GDALVSICopyAlgorithm                        */
/************************************************************************/

class GDALVSICopyAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "copy";
    static constexpr const char *DESCRIPTION =
        "Copy files located on GDAL Virtual System Interface (VSI).";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi_copy.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"cp"};
    }

    GDALVSICopyAlgorithm();

  private:
    std::string m_source{};
    std::string m_destination{};
    bool m_recursive = false;
    bool m_skip = false;

    bool RunImpl(GDALProgressFunc, void *) override;

    bool CopySingle(const std::string &src, const std::string &dst,
                    uint64_t size, GDALProgressFunc pfnProgress,
                    void *pProgressData) const;

    bool CopyRecursive(const std::string &src, const std::string &dst,
                       int depth, int maxdepth, uint64_t &curAmount,
                       uint64_t totalAmount, GDALProgressFunc pfnProgress,
                       void *pProgressData) const;
};

//! @endcond

#endif
