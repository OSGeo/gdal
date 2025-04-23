/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster index" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_INDEX_INCLUDED
#define GDALALG_RASTER_INDEX_INCLUDED

#include "gdalalg_vector_output_abstract.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterIndexAlgorithm                         */
/************************************************************************/

class CPL_DLL GDALRasterIndexAlgorithm /* non final */
    : public GDALVectorOutputAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "index";
    static constexpr const char *DESCRIPTION =
        "Create a vector index of raster datasets.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_index.html";

    GDALRasterIndexAlgorithm();

    GDALRasterIndexAlgorithm(const std::string &name,
                             const std::string &description,
                             const std::string &helpURL);

  protected:
    void AddCommonOptions();

    // Virtual method that may be overridden by derived classes to add options
    // to GDALTileIndex()
    virtual bool AddExtraOptions([[maybe_unused]] CPLStringList &aosOptions)
    {
        return true;
    }

    std::vector<GDALArgDatasetValue> m_inputDatasets{};

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    bool m_recursive = false;
    std::vector<std::string> m_filenameFilter{};
    double m_minPixelSize = 0;
    double m_maxPixelSize = 0;
    std::string m_locationName = "location";
    bool m_writeAbsolutePaths = false;
    std::string m_crs{};
    std::string m_sourceCrsName{};
    std::string m_sourceCrsFormat = "auto";
    std::vector<std::string> m_metadata{};
};

//! @endcond

#endif
