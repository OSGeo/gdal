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

#include "cpl_conv.h"
#include "vrtreclassifier.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gdal
{

bool Reclassifier::Interval::Overlaps(const Interval &other) const
{
    if (dfMin > other.dfMax || dfMax < other.dfMin)
    {
        return false;
    }

    return true;
}

CPLErr Reclassifier::Interval::Parse(const char *s, char **rest)
{
    const char *start = s;
    bool bMinIncluded;
    bool bMaxIncluded;

    while (isspace(*start))
    {
        start++;
    }

    char *end;

    if (*start == '(')
    {
        bMinIncluded = false;
    }
    else if (*start == '[')
    {
        bMinIncluded = true;
    }
    else
    {
        double dfVal = CPLStrtod(start, &end);

        if (end == start)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Interval must start with '(' or '['");
            return CE_Failure;
        }

        SetToConstant(dfVal);

        if (rest != nullptr)
        {
            *rest = end;
        }

        return CE_None;
    }
    start++;

    while (isspace(*start))
    {
        start++;
    }

    if (STARTS_WITH_CI(start, "-inf"))
    {
        dfMin = -std::numeric_limits<double>::infinity();
        end = const_cast<char *>(start + 4);
    }
    else
    {
        dfMin = CPLStrtod(start, &end);
    }

    if (end == start || *end != ',')
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Expected a number");
        return CE_Failure;
    }
    start = end + 1;

    while (isspace(*start))
    {
        start++;
    }

    if (STARTS_WITH_CI(start, "inf"))
    {
        dfMax = std::numeric_limits<double>::infinity();
        end = const_cast<char *>(start + 3);
    }
    else
    {
        dfMax = CPLStrtod(start, &end);
    }

    if (end == start || (*end != ')' && *end != ']'))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Interval must end with ')' or ']");
        return CE_Failure;
    }
    if (*end == ')')
    {
        bMaxIncluded = false;
    }
    else
    {
        bMaxIncluded = true;
    }

    if (rest != nullptr)
    {
        *rest = end + 1;
    }

    if (std::isnan(dfMin) || std::isnan(dfMax))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "NaN is not a valid value for bounds of interval");
        return CE_Failure;
    }

    if (dfMin > dfMax)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Lower bound of interval must be lower or equal to upper "
                 "bound");
        return CE_Failure;
    }

    if (!bMinIncluded)
    {
        dfMin = std::nextafter(dfMin, std::numeric_limits<double>::infinity());
    }
    if (!bMaxIncluded)
    {
        dfMax = std::nextafter(dfMax, -std::numeric_limits<double>::infinity());
    }

    return CE_None;
}

void Reclassifier::Interval::SetToConstant(double dfVal)
{
    dfMin = dfVal;
    dfMax = dfVal;
}

CPLErr Reclassifier::Finalize()
{
    std::sort(m_aoIntervalMappings.begin(), m_aoIntervalMappings.end(),
              [](const auto &a, const auto &b)
              { return a.first.dfMin < b.first.dfMin; });

    for (std::size_t i = 1; i < m_aoIntervalMappings.size(); i++)
    {
        if (m_aoIntervalMappings[i - 1].first.Overlaps(
                m_aoIntervalMappings[i].first))
        {
            // Don't use [, ) notation because we will have modified those values for an open interval
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Interval from %g to %g (mapped to %g) overlaps with "
                     "interval from %g to %g (mapped to %g)",
                     m_aoIntervalMappings[i - 1].first.dfMin,
                     m_aoIntervalMappings[i - 1].first.dfMax,
                     m_aoIntervalMappings[i - 1].second.value_or(
                         std::numeric_limits<double>::quiet_NaN()),
                     m_aoIntervalMappings[i].first.dfMin,
                     m_aoIntervalMappings[i].first.dfMax,
                     m_aoIntervalMappings[i].second.value_or(
                         std::numeric_limits<double>::quiet_NaN()));
            return CE_Failure;
        }
    }

    return CE_None;
}

void Reclassifier::AddMapping(const Interval &interval,
                              std::optional<double> dfDstVal)
{
    m_aoIntervalMappings.emplace_back(interval, dfDstVal);
}

CPLErr Reclassifier::Init(const char *pszText,
                          std::optional<double> noDataValue,
                          GDALDataType eBufType)
{
    const char *start = pszText;
    char *end = const_cast<char *>(start);

    while (*end != '\0')
    {
        while (isspace(*start))
        {
            start++;
        }

        Interval sInt{};
        bool bFromIsDefault = false;
        bool bPassThrough = false;
        bool bFromNaN = false;

        if (STARTS_WITH_CI(start, "DEFAULT"))
        {
            bFromIsDefault = true;
            end = const_cast<char *>(start + 7);
        }
        else if (STARTS_WITH_CI(start, "NO_DATA"))
        {
            if (!noDataValue.has_value())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Value mapped from NO_DATA, but NoData value is "
                         "not set");
                return CE_Failure;
            }

            sInt.SetToConstant(noDataValue.value());
            end = const_cast<char *>(start + 7);
        }
        else if (STARTS_WITH_CI(start, "NAN"))
        {
            bFromNaN = true;
            end = const_cast<char *>(start + 3);
        }
        else
        {
            if (auto eErr = sInt.Parse(start, &end); eErr != CE_None)
            {
                return eErr;
            }
        }

        while (isspace(*end))
        {
            end++;
        }

        if (*end != MAPPING_FROMTO_SEP_CHAR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to parse mapping (expected '%c', got '%c')",
                     MAPPING_FROMTO_SEP_CHAR, *end);
            return CE_Failure;
        }

        start = end + 1;

        while (isspace(*start))
        {
            start++;
        }

        std::optional<double> dfDstVal{};
        if (STARTS_WITH(start, "NO_DATA"))
        {
            if (!noDataValue.has_value())
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Value mapped to NO_DATA, but NoData value is not set");
                return CE_Failure;
            }
            dfDstVal = noDataValue.value();
            end = const_cast<char *>(start) + 7;
        }
        else if (STARTS_WITH(start, "PASS_THROUGH"))
        {
            bPassThrough = true;
            end = const_cast<char *>(start + 12);
        }
        else
        {
            dfDstVal = CPLStrtod(start, &end);
            if (start == end)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to parse output value (expected number or "
                         "NO_DATA)");
                return CE_Failure;
            }
        }

        while (isspace(*end))
        {
            end++;
        }

        if (*end != '\0' && *end != MAPPING_INTERVAL_SEP_CHAR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to parse mapping (expected '%c' or end of string, "
                     "got '%c')",
                     MAPPING_INTERVAL_SEP_CHAR, *end);
            return CE_Failure;
        }

        if (dfDstVal.has_value() &&
            !GDALIsValueExactAs(dfDstVal.value(), eBufType))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Value %g cannot be represented as data type %s",
                     dfDstVal.value(), GDALGetDataTypeName(eBufType));
            return CE_Failure;
        }

        if (bFromNaN)
        {
            SetNaNValue(bPassThrough ? std::numeric_limits<double>::quiet_NaN()
                                     : dfDstVal.value());
        }
        else if (bFromIsDefault)
        {
            if (bPassThrough)
            {
                SetDefaultPassThrough(true);
            }
            else
            {
                SetDefaultValue(dfDstVal.value());
            }
        }
        else
        {
            AddMapping(sInt, dfDstVal);
        }

        start = end + 1;
    }

    return Finalize();
}

static std::optional<size_t> FindInterval(
    const std::vector<std::pair<Reclassifier::Interval, std::optional<double>>>
        &arr,
    double srcVal)
{
    if (arr.empty())
    {
        return std::nullopt;
    }

    size_t low = 0;
    size_t high = arr.size() - 1;

    while (low <= high)
    {
        auto mid = low + (high - low) / 2;

        const auto &mid_interval = arr[mid].first;
        if (mid_interval.Contains(srcVal))
        {
            return mid;
        }

        // Could an interval exist to the left?
        if (srcVal < mid_interval.dfMin)
        {
            if (mid == 0)
            {
                return std::nullopt;
            }
            high = mid - 1;
        }
        // Could an interval exist to the right?
        else if (srcVal > mid_interval.dfMax)
        {
            low = mid + 1;
        }
        else
        {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

double Reclassifier::Reclassify(double srcVal, bool &bFoundInterval) const
{
    bFoundInterval = false;

    if (std::isnan(srcVal))
    {
        if (m_NaNValue.has_value())
        {
            bFoundInterval = true;
            return m_NaNValue.value();
        }
    }
    else
    {
        auto nInterval = FindInterval(m_aoIntervalMappings, srcVal);
        if (nInterval.has_value())
        {
            bFoundInterval = true;
            return m_aoIntervalMappings[nInterval.value()].second.value_or(
                srcVal);
        }
    }

    if (m_defaultValue.has_value())
    {
        bFoundInterval = true;
        return m_defaultValue.value();
    }

    if (m_defaultPassThrough)
    {
        bFoundInterval = true;
        return srcVal;
    }

    return 0;
}

}  // namespace gdal
