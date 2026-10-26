/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "srsinfo" command
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2026, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_SRSINFO_INCLUDED
#define GDALALG_SRSINFO_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"

/************************************************************************/
/*                         GDALSRSInfoAlgorithm                         */
/************************************************************************/

class GDALSRSInfoAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "srsinfo";
    static constexpr const char *DESCRIPTION =
        "Return information on a spatial reference system.";
    static constexpr const char *HELP_URL = "/programs/gdal_srsinfo.html";

    GDALSRSInfoAlgorithm();

  private:
    std::string m_output{};

    bool RunImpl(GDALProgressFunc, void *) override;

    std::string m_srs{};
    bool m_validate{false};
    bool m_singleLineOutput{false};
    bool m_listEpsgNumbersForSrs{false};
    std::string m_outputTypeStr;

    // Flags to control the output type. The default is OUTPUT_TYPE_DEFAULT
    // that means "proj4" and "wkt2"
    enum class OutputType : uint32_t
    {
        OUTPUT_TYPE_PROJJSON = 1 << 0,
        OUTPUT_TYPE_PROJ4 = 1 << 1,
        OUTPUT_TYPE_WKT1 = 1 << 2,
        OUTPUT_TYPE_WKT_SIMPLE = 1 << 3,
        OUTPUT_TYPE_WKT_NOCT = 1 << 4,
        OUTPUT_TYPE_WKT2 = 1 << 5,
        OUTPUT_TYPE_WKT2_2015 = 1 << 6,
        OUTPUT_TYPE_WKT2_2019 = 1 << 7,
        OUTPUT_TYPE_WKT_ESRI = 1 << 8,
        OUTPUT_TYPE_MAPINFO = 1 << 9,
        OUTPUT_TYPE_XML = 1 << 10,

        // This flag is used to indicate the last output type. It is not used for output but for internal purposes only.
        OUTPUT_TYPE_LAST = 1 << 11,

        // Combinations of flags
        OUTPUT_TYPE_WKT_ALL = OUTPUT_TYPE_WKT1 | OUTPUT_TYPE_WKT_SIMPLE |
                              OUTPUT_TYPE_WKT_NOCT | OUTPUT_TYPE_WKT2 |
                              OUTPUT_TYPE_WKT2_2015 | OUTPUT_TYPE_WKT2_2019 |
                              OUTPUT_TYPE_WKT_ESRI,
        OUTPUT_TYPE_DEFAULT = OUTPUT_TYPE_PROJ4 | OUTPUT_TYPE_WKT2,
        OUTPUT_TYPE_ALL = 0xFFFFFFFF,
    };

    OutputType m_outputType{OutputType::OUTPUT_TYPE_DEFAULT};

    // Combined and alias output types
    static constexpr const char *OUTPUT_TYPE_DEFAULT_STR = "default";
    static constexpr const char *OUTPUT_TYPE_ALL_STR = "all";
    // Latest WKT available. Alias for OUTPUT_TYPE_WKT2_2019
    static constexpr const char *OUTPUT_TYPE_WKT_LATEST_STR = "wkt";
    static constexpr const char *OUTPUT_TYPE_WKT_ALL_STR = "wkt_all";

    // Individual output types
    static constexpr const char *OUTPUT_TYPE_PROJJSON_STR = "projjson";
    static constexpr const char *OUTPUT_TYPE_PROJ4_STR = "proj4";
    static constexpr const char *OUTPUT_TYPE_WKT1_STR = "wkt1";
    static constexpr const char *OUTPUT_TYPE_WKT_SIMPLE_STR = "wkt_simple";
    static constexpr const char *OUTPUT_TYPE_WKT_NOCT_STR = "wkt_noct";
    static constexpr const char *OUTPUT_TYPE_WKT2_STR = "wkt2";
    static constexpr const char *OUTPUT_TYPE_WKT2_2015_STR = "wkt2_2015";
    static constexpr const char *OUTPUT_TYPE_WKT2_2019_STR = "wkt2_2019";
    static constexpr const char *OUTPUT_TYPE_WKT_ESRI_STR = "wkt_esri";
    static constexpr const char *OUTPUT_TYPE_MAPINFO_STR = "mapinfo";
    static constexpr const char *OUTPUT_TYPE_XML_STR = "xml";

    CPLErr PrintSRSInfo(const OGRSpatialReference *srs, OutputType outputType,
                        bool prettyPrint, bool printSeparator) const;

    void PrintSRSOutputTypes(const OGRSpatialReference *srs,
                             OutputType outputTypes, bool prettyPrint) const;
};

//! @endcond

#endif
