/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "crs info" command
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2026, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_CRSINFO_INCLUDED
#define GDALALG_CRSINFO_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"

/************************************************************************/
/*                         GDALCRSInfoAlgorithm                         */
/************************************************************************/

class GDALCRSInfoAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "info";
    static constexpr const char *DESCRIPTION =
        "Return information on a spatial reference system.";
    static constexpr const char *HELP_URL = "/programs/gdal_crs_info.html";

    GDALCRSInfoAlgorithm();
    ~GDALCRSInfoAlgorithm() override = default;

  private:
    bool RunImpl(GDALProgressFunc, void *) override;

    void Print(const char *str) const;

    mutable std::string m_output{};
    std::string m_crs{};
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
        OUTPUT_TYPE_WKT2 = 1 << 3,
        OUTPUT_TYPE_WKT_ESRI = 1 << 4,
        OUTPUT_TYPE_MAPINFO = 1 << 5,
        OUTPUT_TYPE_XML = 1 << 6,

        // This flag is used to indicate the last output type. It is not used for output but for internal purposes only.
        OUTPUT_TYPE_LAST = 1 << 6,

        // Combinations of flags
        OUTPUT_TYPE_WKT_ALL =
            OUTPUT_TYPE_WKT1 | OUTPUT_TYPE_WKT2 | OUTPUT_TYPE_WKT_ESRI,
        OUTPUT_TYPE_DEFAULT = OUTPUT_TYPE_PROJ4 | OUTPUT_TYPE_WKT2,
        OUTPUT_TYPE_ALL = 0xFFFFFFFF,
    };

    OutputType m_outputType{OutputType::OUTPUT_TYPE_DEFAULT};

    // Combined and alias output types
    static constexpr const char *OUTPUT_TYPE_DEFAULT_STR = "default";
    static constexpr const char *OUTPUT_TYPE_ALL_STR = "all";
    // Latest WKT available.
    static constexpr const char *OUTPUT_TYPE_WKT_LATEST_STR = "wkt";
    static constexpr const char *OUTPUT_TYPE_WKT_ALL_STR = "wkt_all";

    // Individual output types
    static constexpr const char *OUTPUT_TYPE_PROJJSON_STR = "projjson";
    static constexpr const char *OUTPUT_TYPE_PROJ4_STR = "proj4";
    static constexpr const char *OUTPUT_TYPE_WKT1_STR = "wkt1";
    static constexpr const char *OUTPUT_TYPE_WKT2_STR = "wkt2";
    static constexpr const char *OUTPUT_TYPE_WKT_ESRI_STR = "wkt_esri";
    static constexpr const char *OUTPUT_TYPE_MAPINFO_STR = "mapinfo";
    static constexpr const char *OUTPUT_TYPE_XML_STR = "xml";

    CPLErr PrintCRSInfo(const OGRSpatialReference *crs, OutputType outputType,
                        bool prettyPrint, bool printSeparator) const;

    void PrintCRSOutputTypes(const OGRSpatialReference *crs,
                             OutputType outputTypes, bool prettyPrint) const;
};

//! @endcond

#endif
