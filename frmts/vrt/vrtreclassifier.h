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

/**
 * Class to manage reclassification of pixel values
 */
class Reclassifier
{
  public:
    /// Character separating elements in a list of mapping
    static constexpr char MAPPING_INTERVAL_SEP_CHAR = ';';

    /// Character separating source interval from target value
    static constexpr char MAPPING_FROMTO_SEP_CHAR = '=';

    /**
     * Internal struct to hold an interval of values to be reclassified
     */
    struct Interval
    {
        /// minimum value of range
        double dfMin;

        /// maximum value of range
        double dfMax;

        /// Set the interval to represent a single value [x,x]
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

        /// Returns true of the interval represents a single value [x,x]
        bool IsConstant() const
        {
            return dfMin == dfMax;
        }

        /// Returns true if the interval contains a value
        bool Contains(double x) const
        {
            return x >= dfMin && x <= dfMax;
        }

        /// Returns true if the intervals overlap
        bool Overlaps(const Interval &other) const;
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

    /** Set a mapping between an interval and (optionally) a destination value.
     *  If no destination value is provided, values matching the interval
     *  will be passed through unmodified. It will not be verified that these values
     *  fit within the destination data type.
     */
    void AddMapping(const Interval &interval, std::optional<double> dfDstVal);

    /** Reclassify a value
     *
     * @param srcVal the value to reclassify
     * @param bFoundInterval set to True if the value could be reclassified
     * @return the reclassified value
     */
    double Reclassify(double srcVal, bool &bFoundInterval) const;

    /** If true, values not matched by any interval will be
     *  returned unmodified. It will not be verified that these values
     *  fit within the destination data type.
     */
    void SetDefaultPassThrough(bool value)
    {
        m_defaultPassThrough = value;
    }

    /** Sets a default value for any value not matched by any interval.
     */
    void SetDefaultValue(double value)
    {
        m_defaultValue = value;
    }

    /** Sets a value for an input NaN value
     */
    void SetNaNValue(double value)
    {
        m_NaNValue = value;
    }

    /** Prepare reclassifier for use. No more mappings may be added.
     */
    CPLErr Finalize();

  private:
    /// mapping of ranges to outputs
    std::vector<std::pair<Interval, std::optional<double>>>
        m_aoIntervalMappings{};

    /// output value for NaN inputs
    std::optional<double> m_NaNValue{};

    /// output value for inputs not matching any Interval
    std::optional<double> m_defaultValue{};

    /// whether to pass unmatched inputs through unmodified
    bool m_defaultPassThrough{false};
};

}  // namespace gdal
