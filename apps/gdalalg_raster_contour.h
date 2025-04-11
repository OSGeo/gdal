/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster contour" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_CONTOUR_INCLUDED
#define GDALALG_RASTER_CONTOUR_INCLUDED

#include <limits>

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterContourAlgorithm                       */
/************************************************************************/

class GDALRasterContourAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "contour";
    static constexpr const char *DESCRIPTION =
        "Creates a vector contour from a raster elevation model (DEM).";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_contour.html";

    explicit GDALRasterContourAlgorithm();

    GDALDataset *GetDatasetRef()
    {
        return m_inputDataset.GetDatasetRef();
    }

    void SetDataset(GDALDataset *poDS)
    {
        auto arg = GetArg(GDAL_ARG_NAME_INPUT);
        arg->Set(poDS);
        arg->SetSkipIfAlreadySet();
    }

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_outputFormat{};
    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    std::vector<std::string> m_layerCreationOptions{};

    // gdal_contour specific arguments
    int m_band = 1;                   // -b
    std::string m_outputLayerName;    // -nln <name>
    std::string m_elevAttributeName;  // -a <name>
    std::string m_amin;               // -amin <value>
    std::string m_amax;               // -amax <value>
    bool m_3d = false;                // -3d
                                      // -inodata (skipped)
    double m_sNodata =
        std::numeric_limits<double>::quiet_NaN();  // -snodata <value>
    double m_interval =
        std::numeric_limits<double>::quiet_NaN();  // -i <interval>
    double m_offset =
        std::numeric_limits<double>::quiet_NaN();  // -off <offset>
    std::vector<std::string>
        m_levels;       // -fl <level>[,<level>...] MIN/MAX are also supported
    int m_expBase = 0;  // -e <base>
    bool m_polygonize = false;    // -p
    int m_groupTransactions = 0;  // gt <n>
    bool m_overwrite = false;     // -overwrite
};

//! @endcond

#endif
