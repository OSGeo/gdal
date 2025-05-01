/******************************************************************************
*
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of Reclassifier
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#pragma once

#include "gdal.h"
#include "cpl_error.h"

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace gdal
{

/*! @cond Doxygen_Suppress */

class Reclassifier
{
  public:
    static constexpr char MAPPING_INTERVAL_SEP_CHAR = ';';
    static constexpr char MAPPING_FROMTO_SEP_CHAR = '=';

    struct Interval
    {
        double dfMin;
        double dfMax;
        bool bMinIncluded;
        bool bMaxIncluded;

        void SetToConstant(double dfVal);

        /** Parse an interval. The interval may be either a single constant value,
         *  or two comma-separated values enclosed by parentheses/brackets to
         *  represent open/closed intervals.
         *
         * @param pszText string from which to parse an interval
         * @param end pointer to first non-consumed character
         * @return CE_None on success, CE_Failure otherwise
         */
        CPLErr Parse(const char *pszText, char **end);

        bool IsConstant() const
        {
            return dfMin == dfMax;
        }

        bool Contains(double x) const
        {
            return (x > dfMin || (bMinIncluded && x == dfMin)) &&
                   (x < dfMax || (bMaxIncluded && x == dfMax));
        }
    };

    /** Initialize a Reclassifier from text. The text consists of a series of
     *  SOURCE=DEST mappings, separated by a semicolon.
     *
     *  Each SOURCE element much be one of:
     *  - a constant value
     *  - a range of values, such as (3, 4] or [7, inf]
     *  - the value NO_DATA, for which the provided NoData value will be
     *    substituted
     *  - the value DEFAULT, to define a DEST for any value that does not
     *    match another SOURCE mapping
     *
     *  Each DEST element must be one of:
     *  - a constant value
     *  - the value NO_DATA, for which the provided NoData value will be
     *    substituted
     *
     *  An error will be returned if:
     *  - NO_DATA is used by a NoData value is not defined.
     *  - a DEST value does not fit into the destination data type
     *
     * @param pszText text to parse
     * @param noDataValue NoData value
     * @param eBufType Destination data type
     * @return CE_None if no errors occurred, CE_Failure otherwise
     */
    CPLErr Init(const char *pszText, std::optional<double> noDataValue,
                GDALDataType eBufType);

    void SetDefaultValue(double value)
    {
        m_defaultValue = value;
    }

    void AddMapping(const Interval &interval, double dfDstVal);

    /** Reclassify a value
     *
     * @param srcVal the value to reclassify
     * @param bFoundInterval set to True if the value could be reclassified
     * @return the reclassified value
     */
    double Reclassify(double srcVal, bool &bFoundInterval) const;

  private:
    std::map<double, double> m_oConstantMappings{};
    std::vector<std::pair<Interval, double>> m_aoIntervalMappings{};
    std::optional<double> m_defaultValue{};
};

/*! @endcond */

}  // namespace gdal
