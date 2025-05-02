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

#include <limits>

namespace gdal
{

CPLErr Reclassifier::Interval::Parse(const char *s, char **rest)
{
    const char *start = s;

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
                     "Interval must start with '(' or ']'");
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

    return CE_None;
}

void Reclassifier::Interval::SetToConstant(double dfVal)
{
    dfMin = dfVal;
    dfMax = dfVal;
    bMinIncluded = true;
    bMaxIncluded = true;
}

void Reclassifier::AddMapping(const Interval &interval,
                              std::optional<double> dfDstVal)
{
    if (interval.IsConstant())
    {
        m_oConstantMappings[interval.dfMin] = dfDstVal.value_or(interval.dfMin);
    }
    else
    {
        m_aoIntervalMappings.emplace_back(interval, dfDstVal);
    }
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

        if (STARTS_WITH_CI(start, "DEFAULT"))
        {
            bFromIsDefault = true;
            end = const_cast<char *>(start + 7);
        }
        else if (STARTS_WITH_CI(start, "NO_DATA"))
        {
            if (!noDataValue.has_value())
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Value mapped from NO_DATA, but NoData value is not set");
                return CE_Failure;
            }

            sInt.SetToConstant(noDataValue.value());
            end = const_cast<char *>(start + 7);
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

        if (bFromIsDefault)
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

    return CE_None;
}

double Reclassifier::Reclassify(double srcVal, bool &bFoundInterval) const
{
    bFoundInterval = false;
    auto oDstValIt = m_oConstantMappings.find(srcVal);
    if (oDstValIt != m_oConstantMappings.end())
    {
        bFoundInterval = true;
        return oDstValIt->second;
    }

    for (const auto &[sInt, dstVal] : m_aoIntervalMappings)
    {
        if (sInt.Contains(srcVal))
        {
            bFoundInterval = true;
            return dstVal.value_or(srcVal);
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

    return CE_None;
}

}  // namespace gdal