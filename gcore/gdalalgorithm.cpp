/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDALAlgorithm class
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_json.h"
#include "cpl_levenshtein.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"

#include "gdalalgorithm.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>

#ifndef _
#define _(x) (x)
#endif

constexpr const char *GDAL_ARG_NAME_INPUT_FORMAT = "input-format";

constexpr const char *GDAL_ARG_NAME_OUTPUT_DATA_TYPE = "output-data-type";

constexpr const char *GDAL_ARG_NAME_OPEN_OPTION = "open-option";

constexpr const char *GDAL_ARG_NAME_APPEND_UPDATE = "append-update";

//! @cond Doxygen_Suppress
struct GDALAlgorithmArgHS
{
    GDALAlgorithmArg *ptr = nullptr;

    explicit GDALAlgorithmArgHS(GDALAlgorithmArg *arg) : ptr(arg)
    {
    }
};

//! @endcond

//! @cond Doxygen_Suppress
struct GDALArgDatasetValueHS
{
    GDALArgDatasetValue val{};
    GDALArgDatasetValue *ptr = nullptr;

    GDALArgDatasetValueHS() : ptr(&val)
    {
    }

    explicit GDALArgDatasetValueHS(GDALArgDatasetValue *arg) : ptr(arg)
    {
    }

    GDALArgDatasetValueHS(const GDALArgDatasetValueHS &) = delete;
    GDALArgDatasetValueHS &operator=(const GDALArgDatasetValueHS &) = delete;
};

//! @endcond

/************************************************************************/
/*                     GDALAlgorithmArgTypeIsList()                     */
/************************************************************************/

bool GDALAlgorithmArgTypeIsList(GDALAlgorithmArgType type)
{
    switch (type)
    {
        case GAAT_BOOLEAN:
        case GAAT_STRING:
        case GAAT_INTEGER:
        case GAAT_REAL:
        case GAAT_DATASET:
            break;

        case GAAT_STRING_LIST:
        case GAAT_INTEGER_LIST:
        case GAAT_REAL_LIST:
        case GAAT_DATASET_LIST:
            return true;
    }

    return false;
}

/************************************************************************/
/*                     GDALAlgorithmArgTypeName()                       */
/************************************************************************/

const char *GDALAlgorithmArgTypeName(GDALAlgorithmArgType type)
{
    switch (type)
    {
        case GAAT_BOOLEAN:
            break;
        case GAAT_STRING:
            return "string";
        case GAAT_INTEGER:
            return "integer";
        case GAAT_REAL:
            return "real";
        case GAAT_DATASET:
            return "dataset";
        case GAAT_STRING_LIST:
            return "string_list";
        case GAAT_INTEGER_LIST:
            return "integer_list";
        case GAAT_REAL_LIST:
            return "real_list";
        case GAAT_DATASET_LIST:
            return "dataset_list";
    }

    return "boolean";
}

/************************************************************************/
/*                     GDALAlgorithmArgDatasetTypeName()                */
/************************************************************************/

std::string GDALAlgorithmArgDatasetTypeName(GDALArgDatasetType type)
{
    std::string ret;
    if ((type & GDAL_OF_RASTER) != 0)
        ret = "raster";
    if ((type & GDAL_OF_VECTOR) != 0)
    {
        if (!ret.empty())
        {
            if ((type & GDAL_OF_MULTIDIM_RASTER) != 0)
                ret += ", ";
            else
                ret += " or ";
        }
        ret += "vector";
    }
    if ((type & GDAL_OF_MULTIDIM_RASTER) != 0)
    {
        if (!ret.empty())
        {
            ret += " or ";
        }
        ret += "multidimensional raster";
    }
    return ret;
}

/************************************************************************/
/*                     GDALAlgorithmArgDecl()                           */
/************************************************************************/

// cppcheck-suppress uninitMemberVar
GDALAlgorithmArgDecl::GDALAlgorithmArgDecl(const std::string &longName,
                                           char chShortName,
                                           const std::string &description,
                                           GDALAlgorithmArgType type)
    : m_longName(longName),
      m_shortName(chShortName ? std::string(&chShortName, 1) : std::string()),
      m_description(description), m_type(type),
      m_metaVar(CPLString(m_type == GAAT_BOOLEAN ? std::string() : longName)
                    .toupper()),
      m_maxCount(GDALAlgorithmArgTypeIsList(type) ? UNBOUNDED : 1)
{
    if (m_type == GAAT_BOOLEAN)
    {
        m_defaultValue = false;
    }
}

/************************************************************************/
/*               GDALAlgorithmArgDecl::SetMinCount()                    */
/************************************************************************/

GDALAlgorithmArgDecl &GDALAlgorithmArgDecl::SetMinCount(int count)
{
    if (!GDALAlgorithmArgTypeIsList(m_type))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetMinCount() illegal on scalar argument '%s'",
                 GetName().c_str());
    }
    else
    {
        m_minCount = count;
    }
    return *this;
}

/************************************************************************/
/*               GDALAlgorithmArgDecl::SetMaxCount()                    */
/************************************************************************/

GDALAlgorithmArgDecl &GDALAlgorithmArgDecl::SetMaxCount(int count)
{
    if (!GDALAlgorithmArgTypeIsList(m_type))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetMaxCount() illegal on scalar argument '%s'",
                 GetName().c_str());
    }
    else
    {
        m_maxCount = count;
    }
    return *this;
}

/************************************************************************/
/*                 GDALAlgorithmArg::~GDALAlgorithmArg()                */
/************************************************************************/

GDALAlgorithmArg::~GDALAlgorithmArg() = default;

/************************************************************************/
/*                         GDALAlgorithmArg::Set()                      */
/************************************************************************/

bool GDALAlgorithmArg::Set(bool value)
{
    if (m_decl.GetType() != GAAT_BOOLEAN)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Calling Set(bool) on argument '%s' of type %s is not supported",
            GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    return SetInternal(value);
}

bool GDALAlgorithmArg::ProcessString(std::string &value) const
{
    if (m_decl.IsReadFromFileAtSyntaxAllowed() && !value.empty() &&
        value.front() == '@')
    {
        GByte *pabyData = nullptr;
        if (VSIIngestFile(nullptr, value.c_str() + 1, &pabyData, nullptr,
                          10 * 1024 * 1024))
        {
            // Remove UTF-8 BOM
            size_t offset = 0;
            if (pabyData[0] == 0xEF && pabyData[1] == 0xBB &&
                pabyData[2] == 0xBF)
            {
                offset = 3;
            }
            value = reinterpret_cast<const char *>(pabyData + offset);
            VSIFree(pabyData);
        }
        else
        {
            return false;
        }
    }

    if (m_decl.IsRemoveSQLCommentsEnabled())
        value = CPLRemoveSQLComments(value);

    return true;
}

bool GDALAlgorithmArg::Set(const std::string &value)
{
    switch (m_decl.GetType())
    {
        case GAAT_BOOLEAN:
            if (EQUAL(value.c_str(), "1") || EQUAL(value.c_str(), "TRUE") ||
                EQUAL(value.c_str(), "YES") || EQUAL(value.c_str(), "ON"))
            {
                return Set(true);
            }
            else if (EQUAL(value.c_str(), "0") ||
                     EQUAL(value.c_str(), "FALSE") ||
                     EQUAL(value.c_str(), "NO") || EQUAL(value.c_str(), "OFF"))
            {
                return Set(false);
            }
            break;

        case GAAT_INTEGER:
        case GAAT_INTEGER_LIST:
        {
            errno = 0;
            char *endptr = nullptr;
            const auto v = std::strtoll(value.c_str(), &endptr, 10);
            if (errno == 0 && v >= INT_MIN && v <= INT_MAX &&
                endptr == value.c_str() + value.size())
            {
                if (m_decl.GetType() == GAAT_INTEGER)
                    return Set(static_cast<int>(v));
                else
                    return Set(std::vector<int>{static_cast<int>(v)});
            }
            break;
        }

        case GAAT_REAL:
        case GAAT_REAL_LIST:
        {
            char *endptr = nullptr;
            const double v = CPLStrtod(value.c_str(), &endptr);
            if (endptr == value.c_str() + value.size())
            {
                if (m_decl.GetType() == GAAT_REAL)
                    return Set(v);
                else
                    return Set(std::vector<double>{v});
            }
            break;
        }

        case GAAT_STRING:
            break;

        case GAAT_STRING_LIST:
            return Set(std::vector<std::string>{value});

        case GAAT_DATASET:
            return SetDatasetName(value);

        case GAAT_DATASET_LIST:
        {
            std::vector<GDALArgDatasetValue> v;
            v.resize(1);
            v[0].Set(value);
            return Set(std::move(v));
        }
    }

    if (m_decl.GetType() != GAAT_STRING)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling Set(std::string) on argument '%s' of type %s is not "
                 "supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }

    std::string newValue(value);
    return ProcessString(newValue) && SetInternal(newValue);
}

bool GDALAlgorithmArg::Set(int value)
{
    if (m_decl.GetType() == GAAT_BOOLEAN)
    {
        if (value == 1)
            return Set(true);
        else if (value == 0)
            return Set(false);
    }
    else if (m_decl.GetType() == GAAT_REAL)
    {
        return Set(static_cast<double>(value));
    }
    else if (m_decl.GetType() == GAAT_STRING)
    {
        return Set(std::to_string(value));
    }
    else if (m_decl.GetType() == GAAT_INTEGER_LIST)
    {
        return Set(std::vector<int>{value});
    }
    else if (m_decl.GetType() == GAAT_REAL_LIST)
    {
        return Set(std::vector<double>{static_cast<double>(value)});
    }
    else if (m_decl.GetType() == GAAT_STRING_LIST)
    {
        return Set(std::vector<std::string>{std::to_string(value)});
    }

    if (m_decl.GetType() != GAAT_INTEGER)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Calling Set(int) on argument '%s' of type %s is not supported",
            GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    return SetInternal(value);
}

bool GDALAlgorithmArg::Set(double value)
{
    if (m_decl.GetType() == GAAT_INTEGER && value >= INT_MIN &&
        value <= INT_MAX && static_cast<int>(value) == value)
    {
        return Set(static_cast<int>(value));
    }
    else if (m_decl.GetType() == GAAT_STRING)
    {
        return Set(std::to_string(value));
    }
    else if (m_decl.GetType() == GAAT_INTEGER_LIST && value >= INT_MIN &&
             value <= INT_MAX && static_cast<int>(value) == value)
    {
        return Set(std::vector<int>{static_cast<int>(value)});
    }
    else if (m_decl.GetType() == GAAT_REAL_LIST)
    {
        return Set(std::vector<double>{value});
    }
    else if (m_decl.GetType() == GAAT_STRING_LIST)
    {
        return Set(std::vector<std::string>{std::to_string(value)});
    }
    else if (m_decl.GetType() != GAAT_REAL)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Calling Set(double) on argument '%s' of type %s is not supported",
            GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    return SetInternal(value);
}

static bool CheckCanSetDatasetObject(const GDALAlgorithmArg *arg)
{
    if (arg->GetDatasetInputFlags() == GADV_NAME &&
        arg->GetDatasetOutputFlags() == GADV_OBJECT)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Dataset object '%s' is created by algorithm and cannot be set "
            "as an input.",
            arg->GetName().c_str());
        return false;
    }
    else if ((arg->GetDatasetInputFlags() & GADV_OBJECT) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A dataset cannot be set as an input argument of '%s'.",
                 arg->GetName().c_str());
        return false;
    }

    return true;
}

bool GDALAlgorithmArg::Set(GDALDataset *ds)
{
    if (m_decl.GetType() != GAAT_DATASET)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling Set(GDALDataset*, bool) on argument '%s' of type %s "
                 "is not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    if (!CheckCanSetDatasetObject(this))
        return false;
    m_explicitlySet = true;
    auto &val = *std::get<GDALArgDatasetValue *>(m_value);
    val.Set(ds);
    return RunAllActions();
}

bool GDALAlgorithmArg::Set(std::unique_ptr<GDALDataset> ds)
{
    if (m_decl.GetType() != GAAT_DATASET)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling Set(GDALDataset*, bool) on argument '%s' of type %s "
                 "is not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    if (!CheckCanSetDatasetObject(this))
        return false;
    m_explicitlySet = true;
    auto &val = *std::get<GDALArgDatasetValue *>(m_value);
    val.Set(std::move(ds));
    return RunAllActions();
}

bool GDALAlgorithmArg::SetDatasetName(const std::string &name)
{
    if (m_decl.GetType() != GAAT_DATASET)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling SetDatasetName() on argument '%s' of type %s is "
                 "not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    m_explicitlySet = true;
    std::get<GDALArgDatasetValue *>(m_value)->Set(name);
    return RunAllActions();
}

bool GDALAlgorithmArg::SetFrom(const GDALArgDatasetValue &other)
{
    if (m_decl.GetType() != GAAT_DATASET)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling SetFrom() on argument '%s' of type %s is "
                 "not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    if (!CheckCanSetDatasetObject(this))
        return false;
    m_explicitlySet = true;
    std::get<GDALArgDatasetValue *>(m_value)->SetFrom(other);
    return RunAllActions();
}

bool GDALAlgorithmArg::Set(const std::vector<std::string> &value)
{
    if (m_decl.GetType() == GAAT_INTEGER_LIST)
    {
        std::vector<int> v_i;
        for (const std::string &s : value)
        {
            errno = 0;
            char *endptr = nullptr;
            const auto v = std::strtoll(s.c_str(), &endptr, 10);
            if (errno == 0 && v >= INT_MIN && v <= INT_MAX &&
                endptr == s.c_str() + s.size())
            {
                v_i.push_back(static_cast<int>(v));
            }
            else
            {
                break;
            }
        }
        if (v_i.size() == value.size())
            return Set(v_i);
    }
    else if (m_decl.GetType() == GAAT_REAL_LIST)
    {
        std::vector<double> v_d;
        for (const std::string &s : value)
        {
            char *endptr = nullptr;
            const double v = CPLStrtod(s.c_str(), &endptr);
            if (endptr == s.c_str() + s.size())
            {
                v_d.push_back(v);
            }
            else
            {
                break;
            }
        }
        if (v_d.size() == value.size())
            return Set(v_d);
    }
    else if ((m_decl.GetType() == GAAT_INTEGER ||
              m_decl.GetType() == GAAT_REAL ||
              m_decl.GetType() == GAAT_STRING) &&
             value.size() == 1)
    {
        return Set(value[0]);
    }

    if (m_decl.GetType() != GAAT_STRING_LIST)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling Set(const std::vector<std::string> &) on argument "
                 "'%s' of type %s is not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }

    if (m_decl.IsReadFromFileAtSyntaxAllowed() ||
        m_decl.IsRemoveSQLCommentsEnabled())
    {
        std::vector<std::string> newValue(value);
        for (auto &s : newValue)
        {
            if (!ProcessString(s))
                return false;
        }
        return SetInternal(newValue);
    }
    else
    {
        return SetInternal(value);
    }
}

bool GDALAlgorithmArg::Set(const std::vector<int> &value)
{
    if (m_decl.GetType() == GAAT_REAL_LIST)
    {
        std::vector<double> v_d;
        for (int i : value)
            v_d.push_back(i);
        return Set(v_d);
    }
    else if (m_decl.GetType() == GAAT_STRING_LIST)
    {
        std::vector<std::string> v_s;
        for (int i : value)
            v_s.push_back(std::to_string(i));
        return Set(v_s);
    }
    else if ((m_decl.GetType() == GAAT_INTEGER ||
              m_decl.GetType() == GAAT_REAL ||
              m_decl.GetType() == GAAT_STRING) &&
             value.size() == 1)
    {
        return Set(value[0]);
    }

    if (m_decl.GetType() != GAAT_INTEGER_LIST)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling Set(const std::vector<int> &) on argument '%s' of "
                 "type %s is not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    return SetInternal(value);
}

bool GDALAlgorithmArg::Set(const std::vector<double> &value)
{
    if (m_decl.GetType() == GAAT_INTEGER_LIST)
    {
        std::vector<int> v_i;
        for (double d : value)
        {
            if (d >= INT_MIN && d <= INT_MAX && static_cast<int>(d) == d)
            {
                v_i.push_back(static_cast<int>(d));
            }
            else
            {
                break;
            }
        }
        if (v_i.size() == value.size())
            return Set(v_i);
    }
    else if (m_decl.GetType() == GAAT_STRING_LIST)
    {
        std::vector<std::string> v_s;
        for (double d : value)
            v_s.push_back(std::to_string(d));
        return Set(v_s);
    }
    else if ((m_decl.GetType() == GAAT_INTEGER ||
              m_decl.GetType() == GAAT_REAL ||
              m_decl.GetType() == GAAT_STRING) &&
             value.size() == 1)
    {
        return Set(value[0]);
    }

    if (m_decl.GetType() != GAAT_REAL_LIST)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling Set(const std::vector<double> &) on argument '%s' of "
                 "type %s is not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    return SetInternal(value);
}

bool GDALAlgorithmArg::Set(std::vector<GDALArgDatasetValue> &&value)
{
    if (m_decl.GetType() != GAAT_DATASET_LIST)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling Set(const std::vector<GDALArgDatasetValue> &&) on "
                 "argument '%s' of type %s is not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    m_explicitlySet = true;
    *std::get<std::vector<GDALArgDatasetValue> *>(m_value) = std::move(value);
    return RunAllActions();
}

GDALAlgorithmArg &
GDALAlgorithmArg::operator=(std::unique_ptr<GDALDataset> value)
{
    Set(std::move(value));
    return *this;
}

bool GDALAlgorithmArg::Set(const OGRSpatialReference &value)
{
    const char *const apszOptions[] = {"FORMAT=WKT2_2019", nullptr};
    return Set(value.exportToWkt(apszOptions));
}

bool GDALAlgorithmArg::SetFrom(const GDALAlgorithmArg &other)
{
    if (m_decl.GetType() != other.GetType())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling SetFrom() on argument '%s' of type %s whereas "
                 "other argument type is %s is not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()),
                 GDALAlgorithmArgTypeName(other.GetType()));
        return false;
    }

    switch (m_decl.GetType())
    {
        case GAAT_BOOLEAN:
            *std::get<bool *>(m_value) = *std::get<bool *>(other.m_value);
            break;
        case GAAT_STRING:
            *std::get<std::string *>(m_value) =
                *std::get<std::string *>(other.m_value);
            break;
        case GAAT_INTEGER:
            *std::get<int *>(m_value) = *std::get<int *>(other.m_value);
            break;
        case GAAT_REAL:
            *std::get<double *>(m_value) = *std::get<double *>(other.m_value);
            break;
        case GAAT_DATASET:
            return SetFrom(other.Get<GDALArgDatasetValue>());
        case GAAT_STRING_LIST:
            *std::get<std::vector<std::string> *>(m_value) =
                *std::get<std::vector<std::string> *>(other.m_value);
            break;
        case GAAT_INTEGER_LIST:
            *std::get<std::vector<int> *>(m_value) =
                *std::get<std::vector<int> *>(other.m_value);
            break;
        case GAAT_REAL_LIST:
            *std::get<std::vector<double> *>(m_value) =
                *std::get<std::vector<double> *>(other.m_value);
            break;
        case GAAT_DATASET_LIST:
        {
            std::get<std::vector<GDALArgDatasetValue> *>(m_value)->clear();
            for (const auto &val :
                 *std::get<std::vector<GDALArgDatasetValue> *>(other.m_value))
            {
                GDALArgDatasetValue v;
                v.SetFrom(val);
                std::get<std::vector<GDALArgDatasetValue> *>(m_value)
                    ->push_back(std::move(v));
            }
            break;
        }
    }
    m_explicitlySet = true;
    return RunAllActions();
}

/************************************************************************/
/*                  GDALAlgorithmArg::RunAllActions()                   */
/************************************************************************/

bool GDALAlgorithmArg::RunAllActions()
{
    if (!RunValidationActions())
        return false;
    RunActions();
    return true;
}

/************************************************************************/
/*                      GDALAlgorithmArg::RunActions()                  */
/************************************************************************/

void GDALAlgorithmArg::RunActions()
{
    for (const auto &f : m_actions)
        f();
}

/************************************************************************/
/*                    GDALAlgorithmArg::ValidateChoice()                */
/************************************************************************/

// Returns the canonical value if matching a valid choice, or empty string
// otherwise.
std::string GDALAlgorithmArg::ValidateChoice(const std::string &value) const
{
    for (const std::string &choice : GetChoices())
    {
        if (EQUAL(value.c_str(), choice.c_str()))
        {
            return choice;
        }
    }

    for (const std::string &choice : GetHiddenChoices())
    {
        if (EQUAL(value.c_str(), choice.c_str()))
        {
            return choice;
        }
    }

    std::string expected;
    for (const auto &choice : GetChoices())
    {
        if (!expected.empty())
            expected += ", ";
        expected += '\'';
        expected += choice;
        expected += '\'';
    }
    CPLError(CE_Failure, CPLE_IllegalArg,
             "Invalid value '%s' for string argument '%s'. Should be "
             "one among %s.",
             value.c_str(), GetName().c_str(), expected.c_str());
    return std::string();
}

/************************************************************************/
/*                   GDALAlgorithmArg::ValidateIntRange()               */
/************************************************************************/

bool GDALAlgorithmArg::ValidateIntRange(int val) const
{
    bool ret = true;

    const auto [minVal, minValIsIncluded] = GetMinValue();
    if (!std::isnan(minVal))
    {
        if (minValIsIncluded && val < minVal)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Value of argument '%s' is %d, but should be >= %d",
                     GetName().c_str(), val, static_cast<int>(minVal));
            ret = false;
        }
        else if (!minValIsIncluded && val <= minVal)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Value of argument '%s' is %d, but should be > %d",
                     GetName().c_str(), val, static_cast<int>(minVal));
            ret = false;
        }
    }

    const auto [maxVal, maxValIsIncluded] = GetMaxValue();
    if (!std::isnan(maxVal))
    {

        if (maxValIsIncluded && val > maxVal)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Value of argument '%s' is %d, but should be <= %d",
                     GetName().c_str(), val, static_cast<int>(maxVal));
            ret = false;
        }
        else if (!maxValIsIncluded && val >= maxVal)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Value of argument '%s' is %d, but should be < %d",
                     GetName().c_str(), val, static_cast<int>(maxVal));
            ret = false;
        }
    }

    return ret;
}

/************************************************************************/
/*                   GDALAlgorithmArg::ValidateRealRange()              */
/************************************************************************/

bool GDALAlgorithmArg::ValidateRealRange(double val) const
{
    bool ret = true;

    const auto [minVal, minValIsIncluded] = GetMinValue();
    if (!std::isnan(minVal))
    {
        if (minValIsIncluded && !(val >= minVal))
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Value of argument '%s' is %g, but should be >= %g",
                     GetName().c_str(), val, minVal);
            ret = false;
        }
        else if (!minValIsIncluded && !(val > minVal))
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Value of argument '%s' is %g, but should be > %g",
                     GetName().c_str(), val, minVal);
            ret = false;
        }
    }

    const auto [maxVal, maxValIsIncluded] = GetMaxValue();
    if (!std::isnan(maxVal))
    {

        if (maxValIsIncluded && !(val <= maxVal))
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Value of argument '%s' is %g, but should be <= %g",
                     GetName().c_str(), val, maxVal);
            ret = false;
        }
        else if (!maxValIsIncluded && !(val < maxVal))
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Value of argument '%s' is %g, but should be < %g",
                     GetName().c_str(), val, maxVal);
            ret = false;
        }
    }

    return ret;
}

/************************************************************************/
/*                    GDALAlgorithmArg::RunValidationActions()          */
/************************************************************************/

bool GDALAlgorithmArg::RunValidationActions()
{
    bool ret = true;

    if (GetType() == GAAT_STRING && !GetChoices().empty())
    {
        auto &val = Get<std::string>();
        const std::string validVal = ValidateChoice(val);
        if (validVal.empty())
            ret = false;
        else
            val = validVal;
    }
    else if (GetType() == GAAT_STRING_LIST && !GetChoices().empty())
    {
        auto &values = Get<std::vector<std::string>>();
        for (std::string &val : values)
        {
            const std::string validVal = ValidateChoice(val);
            if (validVal.empty())
                ret = false;
            else
                val = validVal;
        }
    }

    if (GetType() == GAAT_STRING)
    {
        const int nMinCharCount = GetMinCharCount();
        if (nMinCharCount > 0)
        {
            const auto &val = Get<std::string>();
            if (val.size() < static_cast<size_t>(nMinCharCount))
            {
                CPLError(
                    CE_Failure, CPLE_IllegalArg,
                    "Value of argument '%s' is '%s', but should have at least "
                    "%d character(s)",
                    GetName().c_str(), val.c_str(), nMinCharCount);
                ret = false;
            }
        }
    }
    else if (GetType() == GAAT_STRING_LIST)
    {
        const int nMinCharCount = GetMinCharCount();
        if (nMinCharCount > 0)
        {
            for (const auto &val : Get<std::vector<std::string>>())
            {
                if (val.size() < static_cast<size_t>(nMinCharCount))
                {
                    CPLError(
                        CE_Failure, CPLE_IllegalArg,
                        "Value of argument '%s' is '%s', but should have at "
                        "least %d character(s)",
                        GetName().c_str(), val.c_str(), nMinCharCount);
                    ret = false;
                }
            }
        }
    }
    else if (GetType() == GAAT_INTEGER)
    {
        ret = ValidateIntRange(Get<int>()) && ret;
    }
    else if (GetType() == GAAT_INTEGER_LIST)
    {
        for (int v : Get<std::vector<int>>())
            ret = ValidateIntRange(v) && ret;
    }
    else if (GetType() == GAAT_REAL)
    {
        ret = ValidateRealRange(Get<double>()) && ret;
    }
    else if (GetType() == GAAT_REAL_LIST)
    {
        for (double v : Get<std::vector<double>>())
            ret = ValidateRealRange(v) && ret;
    }

    if (GDALAlgorithmArgTypeIsList(GetType()))
    {
        int valueCount = 0;
        if (GetType() == GAAT_STRING_LIST)
        {
            valueCount =
                static_cast<int>(Get<std::vector<std::string>>().size());
        }
        else if (GetType() == GAAT_INTEGER_LIST)
        {
            valueCount = static_cast<int>(Get<std::vector<int>>().size());
        }
        else if (GetType() == GAAT_REAL_LIST)
        {
            valueCount = static_cast<int>(Get<std::vector<double>>().size());
        }
        else if (GetType() == GAAT_DATASET_LIST)
        {
            valueCount = static_cast<int>(
                Get<std::vector<GDALArgDatasetValue>>().size());
        }

        if (valueCount != GetMinCount() && GetMinCount() == GetMaxCount())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "%d value%s been specified for argument '%s', "
                        "whereas exactly %d %s expected.",
                        valueCount, valueCount > 1 ? "s have" : " has",
                        GetName().c_str(), GetMinCount(),
                        GetMinCount() > 1 ? "were" : "was");
            ret = false;
        }
        else if (valueCount < GetMinCount())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Only %d value%s been specified for argument '%s', "
                        "whereas at least %d %s expected.",
                        valueCount, valueCount > 1 ? "s have" : " has",
                        GetName().c_str(), GetMinCount(),
                        GetMinCount() > 1 ? "were" : "was");
            ret = false;
        }
        else if (valueCount > GetMaxCount())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "%d value%s been specified for argument '%s', "
                        "whereas at most %d %s expected.",
                        valueCount, valueCount > 1 ? "s have" : " has",
                        GetName().c_str(), GetMaxCount(),
                        GetMaxCount() > 1 ? "were" : "was");
            ret = false;
        }
    }

    if (ret)
    {
        for (const auto &f : m_validationActions)
        {
            if (!f())
                ret = false;
        }
    }

    return ret;
}

/************************************************************************/
/*                    GDALAlgorithmArg::ReportError()                   */
/************************************************************************/

void GDALAlgorithmArg::ReportError(CPLErr eErrClass, CPLErrorNum err_no,
                                   const char *fmt, ...) const
{
    va_list args;
    va_start(args, fmt);
    if (m_owner)
    {
        m_owner->ReportError(eErrClass, err_no, "%s",
                             CPLString().vPrintf(fmt, args).c_str());
    }
    else
    {
        CPLError(eErrClass, err_no, "%s",
                 CPLString().vPrintf(fmt, args).c_str());
    }
    va_end(args);
}

/************************************************************************/
/*                    GDALAlgorithmArg::Serialize()                     */
/************************************************************************/

bool GDALAlgorithmArg::Serialize(std::string &serializedArg) const
{
    serializedArg.clear();

    if (!IsExplicitlySet())
    {
        return false;
    }

    std::string ret = "--";
    ret += GetName();
    if (GetType() == GAAT_BOOLEAN)
    {
        serializedArg = std::move(ret);
        return true;
    }

    const auto AppendString = [&ret](const std::string &str)
    {
        if (str.find('"') != std::string::npos ||
            str.find(' ') != std::string::npos ||
            str.find('\\') != std::string::npos ||
            str.find(',') != std::string::npos)
        {
            ret += '"';
            ret +=
                CPLString(str).replaceAll('\\', "\\\\").replaceAll('"', "\\\"");
            ret += '"';
        }
        else
        {
            ret += str;
        }
    };

    const auto AddListValueSeparator = [this, &ret]()
    {
        if (GetPackedValuesAllowed())
        {
            ret += ',';
        }
        else
        {
            ret += " --";
            ret += GetName();
            ret += ' ';
        }
    };

    ret += ' ';
    switch (GetType())
    {
        case GAAT_BOOLEAN:
            break;
        case GAAT_STRING:
        {
            const auto &val = Get<std::string>();
            AppendString(val);
            break;
        }
        case GAAT_INTEGER:
        {
            ret += CPLSPrintf("%d", Get<int>());
            break;
        }
        case GAAT_REAL:
        {
            ret += CPLSPrintf("%.17g", Get<double>());
            break;
        }
        case GAAT_DATASET:
        {
            const auto &val = Get<GDALArgDatasetValue>();
            const auto &str = val.GetName();
            if (str.empty())
            {
                return false;
            }
            AppendString(str);
            break;
        }
        case GAAT_STRING_LIST:
        {
            const auto &vals = Get<std::vector<std::string>>();
            for (size_t i = 0; i < vals.size(); ++i)
            {
                if (i > 0)
                    AddListValueSeparator();
                AppendString(vals[i]);
            }
            break;
        }
        case GAAT_INTEGER_LIST:
        {
            const auto &vals = Get<std::vector<int>>();
            for (size_t i = 0; i < vals.size(); ++i)
            {
                if (i > 0)
                    AddListValueSeparator();
                ret += CPLSPrintf("%d", vals[i]);
            }
            break;
        }
        case GAAT_REAL_LIST:
        {
            const auto &vals = Get<std::vector<double>>();
            for (size_t i = 0; i < vals.size(); ++i)
            {
                if (i > 0)
                    AddListValueSeparator();
                ret += CPLSPrintf("%.17g", vals[i]);
            }
            break;
        }
        case GAAT_DATASET_LIST:
        {
            const auto &vals = Get<std::vector<GDALArgDatasetValue>>();
            for (size_t i = 0; i < vals.size(); ++i)
            {
                if (i > 0)
                    AddListValueSeparator();
                const auto &val = vals[i];
                const auto &str = val.GetName();
                if (str.empty())
                {
                    return false;
                }
                AppendString(str);
            }
            break;
        }
    }

    serializedArg = std::move(ret);
    return true;
}

/************************************************************************/
/*              GDALInConstructionAlgorithmArg::AddAlias()              */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALInConstructionAlgorithmArg::AddAlias(const std::string &alias)
{
    m_decl.AddAlias(alias);
    if (m_owner)
        m_owner->AddAliasFor(this, alias);
    return *this;
}

/************************************************************************/
/*            GDALInConstructionAlgorithmArg::AddHiddenAlias()          */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALInConstructionAlgorithmArg::AddHiddenAlias(const std::string &alias)
{
    m_decl.AddHiddenAlias(alias);
    if (m_owner)
        m_owner->AddAliasFor(this, alias);
    return *this;
}

/************************************************************************/
/*           GDALInConstructionAlgorithmArg::AddShortNameAlias()        */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALInConstructionAlgorithmArg::AddShortNameAlias(char shortNameAlias)
{
    m_decl.AddShortNameAlias(shortNameAlias);
    if (m_owner)
        m_owner->AddShortNameAliasFor(this, shortNameAlias);
    return *this;
}

/************************************************************************/
/*             GDALInConstructionAlgorithmArg::SetPositional()          */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALInConstructionAlgorithmArg::SetPositional()
{
    m_decl.SetPositional();
    if (m_owner)
        m_owner->SetPositional(this);
    return *this;
}

/************************************************************************/
/*              GDALArgDatasetValue::GDALArgDatasetValue()              */
/************************************************************************/

GDALArgDatasetValue::GDALArgDatasetValue(GDALDataset *poDS)
    : m_poDS(poDS), m_name(m_poDS ? m_poDS->GetDescription() : std::string()),
      m_nameSet(true)
{
    if (m_poDS)
        m_poDS->Reference();
}

/************************************************************************/
/*              GDALArgDatasetValue::Set()                              */
/************************************************************************/

void GDALArgDatasetValue::Set(const std::string &name)
{
    Close();
    m_name = name;
    m_nameSet = true;
    if (m_ownerArg)
        m_ownerArg->NotifyValueSet();
}

/************************************************************************/
/*              GDALArgDatasetValue::Set()                              */
/************************************************************************/

void GDALArgDatasetValue::Set(std::unique_ptr<GDALDataset> poDS)
{
    Close();
    m_poDS = poDS.release();
    m_name = m_poDS ? m_poDS->GetDescription() : std::string();
    m_nameSet = true;
    if (m_ownerArg)
        m_ownerArg->NotifyValueSet();
}

/************************************************************************/
/*              GDALArgDatasetValue::Set()                              */
/************************************************************************/

void GDALArgDatasetValue::Set(GDALDataset *poDS)
{
    Close();
    m_poDS = poDS;
    if (m_poDS)
        m_poDS->Reference();
    m_name = m_poDS ? m_poDS->GetDescription() : std::string();
    m_nameSet = true;
    if (m_ownerArg)
        m_ownerArg->NotifyValueSet();
}

/************************************************************************/
/*                   GDALArgDatasetValue::SetFrom()                     */
/************************************************************************/

void GDALArgDatasetValue::SetFrom(const GDALArgDatasetValue &other)
{
    Close();
    m_name = other.m_name;
    m_nameSet = other.m_nameSet;
    m_poDS = other.m_poDS;
    if (m_poDS)
        m_poDS->Reference();
}

/************************************************************************/
/*              GDALArgDatasetValue::~GDALArgDatasetValue()             */
/************************************************************************/

GDALArgDatasetValue::~GDALArgDatasetValue()
{
    Close();
}

/************************************************************************/
/*                     GDALArgDatasetValue::Close()                     */
/************************************************************************/

bool GDALArgDatasetValue::Close()
{
    bool ret = true;
    if (m_poDS && m_poDS->Dereference() == 0)
    {
        ret = m_poDS->Close() == CE_None;
        delete m_poDS;
    }
    m_poDS = nullptr;
    return ret;
}

/************************************************************************/
/*                      GDALArgDatasetValue::operator=()                */
/************************************************************************/

GDALArgDatasetValue &GDALArgDatasetValue::operator=(GDALArgDatasetValue &&other)
{
    Close();
    m_poDS = other.m_poDS;
    m_name = other.m_name;
    m_nameSet = other.m_nameSet;
    other.m_poDS = nullptr;
    other.m_name.clear();
    other.m_nameSet = false;
    return *this;
}

/************************************************************************/
/*                   GDALArgDatasetValue::GetDataset()                  */
/************************************************************************/

GDALDataset *GDALArgDatasetValue::GetDatasetIncreaseRefCount()
{
    if (m_poDS)
        m_poDS->Reference();
    return m_poDS;
}

/************************************************************************/
/*               GDALArgDatasetValue(GDALArgDatasetValue &&other)       */
/************************************************************************/

GDALArgDatasetValue::GDALArgDatasetValue(GDALArgDatasetValue &&other)
    : m_poDS(other.m_poDS), m_name(other.m_name), m_nameSet(other.m_nameSet)
{
    other.m_poDS = nullptr;
    other.m_name.clear();
}

/************************************************************************/
/*              GDALInConstructionAlgorithmArg::SetIsCRSArg()           */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALInConstructionAlgorithmArg::SetIsCRSArg(
    bool noneAllowed, const std::vector<std::string> &specialValues)
{
    if (GetType() != GAAT_STRING)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "SetIsCRSArg() can only be called on a String argument");
        return *this;
    }
    AddValidationAction(
        [this, noneAllowed, specialValues]()
        {
            const std::string &osVal =
                static_cast<const GDALInConstructionAlgorithmArg *>(this)
                    ->Get<std::string>();
            if ((!noneAllowed || (osVal != "none" && osVal != "null")) &&
                std::find(specialValues.begin(), specialValues.end(), osVal) ==
                    specialValues.end())
            {
                OGRSpatialReference oSRS;
                if (oSRS.SetFromUserInput(osVal.c_str()) != OGRERR_NONE)
                {
                    m_owner->ReportError(CE_Failure, CPLE_AppDefined,
                                         "Invalid value for '%s' argument",
                                         GetName().c_str());
                    return false;
                }
            }
            return true;
        });

    SetAutoCompleteFunction(
        [noneAllowed, specialValues](const std::string &currentValue)
        {
            std::vector<std::string> oRet;
            if (noneAllowed)
                oRet.push_back("none");
            oRet.insert(oRet.end(), specialValues.begin(), specialValues.end());
            if (!currentValue.empty())
            {
                const CPLStringList aosTokens(
                    CSLTokenizeString2(currentValue.c_str(), ":", 0));
                int nCount = 0;
                std::unique_ptr<OSRCRSInfo *, decltype(&OSRDestroyCRSInfoList)>
                    pCRSList(OSRGetCRSInfoListFromDatabase(aosTokens[0],
                                                           nullptr, &nCount),
                             OSRDestroyCRSInfoList);
                std::string osCode;
                for (int i = 0; i < nCount; ++i)
                {
                    const auto *entry = (pCRSList.get())[i];
                    if (aosTokens.size() == 1 ||
                        STARTS_WITH(entry->pszCode, aosTokens[1]))
                    {
                        if (oRet.empty())
                            osCode = entry->pszCode;
                        oRet.push_back(std::string(entry->pszCode)
                                           .append(" -- ")
                                           .append(entry->pszName));
                    }
                }
                if (oRet.size() == 1)
                {
                    // If there is a single match, remove the name from the suggestion.
                    oRet.clear();
                    oRet.push_back(osCode);
                }
            }
            if (currentValue.empty() || oRet.empty())
            {
                const CPLStringList aosAuthorities(
                    OSRGetAuthorityListFromDatabase());
                for (const char *pszAuth : cpl::Iterate(aosAuthorities))
                {
                    int nCount = 0;
                    OSRDestroyCRSInfoList(OSRGetCRSInfoListFromDatabase(
                        pszAuth, nullptr, &nCount));
                    if (nCount)
                        oRet.push_back(std::string(pszAuth).append(":"));
                }
            }
            return oRet;
        });

    return *this;
}

/************************************************************************/
/*                     GDALAlgorithm::GDALAlgorithm()                  */
/************************************************************************/

GDALAlgorithm::GDALAlgorithm(const std::string &name,
                             const std::string &description,
                             const std::string &helpURL)
    : m_name(name), m_description(description), m_helpURL(helpURL),
      m_helpFullURL(!m_helpURL.empty() && m_helpURL[0] == '/'
                        ? "https://gdal.org" + m_helpURL
                        : m_helpURL)
{
    AddArg("help", 'h', _("Display help message and exit"), &m_helpRequested)
        .SetOnlyForCLI()
        .SetCategory(GAAC_COMMON)
        .AddAction([this]() { m_specialActionRequested = true; });
    AddArg("help-doc", 0, _("Display help message for use by documentation"),
           &m_helpDocRequested)
        .SetHidden()
        .AddAction([this]() { m_specialActionRequested = true; });
    AddArg("json-usage", 0, _("Display usage as JSON document and exit"),
           &m_JSONUsageRequested)
        .SetOnlyForCLI()
        .SetCategory(GAAC_COMMON)
        .AddAction([this]() { m_specialActionRequested = true; });
    AddArg("config", 0, _("Configuration option"), &m_dummyConfigOptions)
        .SetMetaVar("<KEY>=<VALUE>")
        .SetOnlyForCLI()
        .SetCategory(GAAC_COMMON);
}

/************************************************************************/
/*                     GDALAlgorithm::~GDALAlgorithm()                  */
/************************************************************************/

GDALAlgorithm::~GDALAlgorithm() = default;

/************************************************************************/
/*                    GDALAlgorithm::ParseArgument()                    */
/************************************************************************/

bool GDALAlgorithm::ParseArgument(
    GDALAlgorithmArg *arg, const std::string &name, const std::string &value,
    std::map<
        GDALAlgorithmArg *,
        std::variant<std::vector<std::string>, std::vector<int>,
                     std::vector<double>, std::vector<GDALArgDatasetValue>>>
        &inConstructionValues)
{
    const bool isListArg = GDALAlgorithmArgTypeIsList(arg->GetType());
    if (arg->IsExplicitlySet() && !isListArg)
    {
        // Hack for "gdal info" to be able to pass an opened raster dataset
        // by "gdal raster info" to the "gdal vector info" algorithm.
        if (arg->SkipIfAlreadySet())
        {
            arg->SetSkipIfAlreadySet(false);
            return true;
        }

        ReportError(CE_Failure, CPLE_IllegalArg,
                    "Argument '%s' has already been specified.", name.c_str());
        return false;
    }

    if (!arg->GetRepeatedArgAllowed() &&
        cpl::contains(inConstructionValues, arg))
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "Argument '%s' has already been specified.", name.c_str());
        return false;
    }

    switch (arg->GetType())
    {
        case GAAT_BOOLEAN:
        {
            if (value.empty() || value == "true")
                return arg->Set(true);
            else if (value == "false")
                return arg->Set(false);
            else
            {
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "Invalid value '%s' for boolean argument '%s'. Should be "
                    "'true' or 'false'.",
                    value.c_str(), name.c_str());
                return false;
            }
        }

        case GAAT_STRING:
        {
            return arg->Set(value);
        }

        case GAAT_INTEGER:
        {
            errno = 0;
            char *endptr = nullptr;
            const auto val = std::strtol(value.c_str(), &endptr, 10);
            if (errno == 0 && endptr &&
                endptr == value.c_str() + value.size() && val >= INT_MIN &&
                val <= INT_MAX)
            {
                return arg->Set(static_cast<int>(val));
            }
            else
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "Expected integer value for argument '%s', "
                            "but got '%s'.",
                            name.c_str(), value.c_str());
                return false;
            }
        }

        case GAAT_REAL:
        {
            char *endptr = nullptr;
            double dfValue = CPLStrtod(value.c_str(), &endptr);
            if (endptr != value.c_str() + value.size())
            {
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "Expected real value for argument '%s', but got '%s'.",
                    name.c_str(), value.c_str());
                return false;
            }
            return arg->Set(dfValue);
        }

        case GAAT_DATASET:
        {
            return arg->SetDatasetName(value);
        }

        case GAAT_STRING_LIST:
        {
            const CPLStringList aosTokens(
                arg->GetPackedValuesAllowed()
                    ? CSLTokenizeString2(value.c_str(), ",", CSLT_HONOURSTRINGS)
                    : CSLAddString(nullptr, value.c_str()));
            if (!cpl::contains(inConstructionValues, arg))
            {
                inConstructionValues[arg] = std::vector<std::string>();
            }
            auto &valueVector =
                std::get<std::vector<std::string>>(inConstructionValues[arg]);
            for (const char *v : aosTokens)
            {
                valueVector.push_back(v);
            }
            break;
        }

        case GAAT_INTEGER_LIST:
        {
            const CPLStringList aosTokens(
                arg->GetPackedValuesAllowed()
                    ? CSLTokenizeString2(
                          value.c_str(), ",",
                          CSLT_HONOURSTRINGS | CSLT_STRIPLEADSPACES |
                              CSLT_STRIPENDSPACES | CSLT_ALLOWEMPTYTOKENS)
                    : CSLAddString(nullptr, value.c_str()));
            if (!cpl::contains(inConstructionValues, arg))
            {
                inConstructionValues[arg] = std::vector<int>();
            }
            auto &valueVector =
                std::get<std::vector<int>>(inConstructionValues[arg]);
            for (const char *v : aosTokens)
            {
                errno = 0;
                char *endptr = nullptr;
                const auto val = std::strtol(v, &endptr, 10);
                if (errno == 0 && endptr && endptr == v + strlen(v) &&
                    val >= INT_MIN && val <= INT_MAX && strlen(v) > 0)
                {
                    valueVector.push_back(static_cast<int>(val));
                }
                else
                {
                    ReportError(
                        CE_Failure, CPLE_IllegalArg,
                        "Expected list of integer value for argument '%s', "
                        "but got '%s'.",
                        name.c_str(), value.c_str());
                    return false;
                }
            }
            break;
        }

        case GAAT_REAL_LIST:
        {
            const CPLStringList aosTokens(
                arg->GetPackedValuesAllowed()
                    ? CSLTokenizeString2(
                          value.c_str(), ",",
                          CSLT_HONOURSTRINGS | CSLT_STRIPLEADSPACES |
                              CSLT_STRIPENDSPACES | CSLT_ALLOWEMPTYTOKENS)
                    : CSLAddString(nullptr, value.c_str()));
            if (!cpl::contains(inConstructionValues, arg))
            {
                inConstructionValues[arg] = std::vector<double>();
            }
            auto &valueVector =
                std::get<std::vector<double>>(inConstructionValues[arg]);
            for (const char *v : aosTokens)
            {
                char *endptr = nullptr;
                double dfValue = CPLStrtod(v, &endptr);
                if (strlen(v) == 0 || endptr != v + strlen(v))
                {
                    ReportError(
                        CE_Failure, CPLE_IllegalArg,
                        "Expected list of real value for argument '%s', "
                        "but got '%s'.",
                        name.c_str(), value.c_str());
                    return false;
                }
                valueVector.push_back(dfValue);
            }
            break;
        }

        case GAAT_DATASET_LIST:
        {
            const CPLStringList aosTokens(
                CSLTokenizeString2(value.c_str(), ",", CSLT_HONOURSTRINGS));
            if (!cpl::contains(inConstructionValues, arg))
            {
                inConstructionValues[arg] = std::vector<GDALArgDatasetValue>();
            }
            auto &valueVector = std::get<std::vector<GDALArgDatasetValue>>(
                inConstructionValues[arg]);
            for (const char *v : aosTokens)
            {
                valueVector.push_back(GDALArgDatasetValue(v));
            }
            break;
        }
    }

    return true;
}

/************************************************************************/
/*               GDALAlgorithm::ParseCommandLineArguments()             */
/************************************************************************/

bool GDALAlgorithm::ParseCommandLineArguments(
    const std::vector<std::string> &args)
{
    if (m_parsedSubStringAlreadyCalled)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "ParseCommandLineArguments() can only be called once per "
                    "instance.");
        return false;
    }
    m_parsedSubStringAlreadyCalled = true;

    // AWS like syntax supported too (not advertized)
    if (args.size() == 1 && args[0] == "help")
    {
        auto arg = GetArg("help");
        assert(arg);
        arg->Set(true);
        arg->RunActions();
        return true;
    }

    if (HasSubAlgorithms())
    {
        if (args.empty())
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Missing %s name.",
                        m_callPath.size() == 1 ? "command" : "subcommand");
            return false;
        }
        if (!args[0].empty() && args[0][0] == '-')
        {
            // go on argument parsing
        }
        else
        {
            const auto nCounter = CPLGetErrorCounter();
            m_selectedSubAlgHolder = InstantiateSubAlgorithm(args[0]);
            if (m_selectedSubAlgHolder)
            {
                m_selectedSubAlg = m_selectedSubAlgHolder.get();
                m_selectedSubAlg->SetReferencePathForRelativePaths(
                    m_referencePath);
                m_selectedSubAlg->m_executionForStreamOutput =
                    m_executionForStreamOutput;
                m_selectedSubAlg->m_calledFromCommandLine =
                    m_calledFromCommandLine;
                bool bRet = m_selectedSubAlg->ParseCommandLineArguments(
                    std::vector<std::string>(args.begin() + 1, args.end()));
                m_selectedSubAlg->PropagateSpecialActionTo(this);
                return bRet;
            }
            else
            {
                if (!(CPLGetErrorCounter() == nCounter + 1 &&
                      strstr(CPLGetLastErrorMsg(), "Do you mean")))
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Unknown command: '%s'", args[0].c_str());
                }
                return false;
            }
        }
    }

    std::map<
        GDALAlgorithmArg *,
        std::variant<std::vector<std::string>, std::vector<int>,
                     std::vector<double>, std::vector<GDALArgDatasetValue>>>
        inConstructionValues;

    std::vector<std::string> lArgs(args);
    for (size_t i = 0; i < lArgs.size(); /* incremented in loop */)
    {
        const auto &strArg = lArgs[i];
        GDALAlgorithmArg *arg = nullptr;
        std::string name;
        std::string value;
        bool hasValue = false;
        if (strArg.size() >= 2 && strArg[0] == '-' && strArg[1] == '-')
        {
            const auto equalPos = strArg.find('=');
            name = (equalPos != std::string::npos) ? strArg.substr(0, equalPos)
                                                   : strArg;
            const std::string nameWithoutDash = name.substr(2);
            const auto iterArg = m_mapLongNameToArg.find(nameWithoutDash);
            if (iterArg == m_mapLongNameToArg.end())
            {
                const std::string bestCandidate =
                    GetSuggestionForArgumentName(nameWithoutDash);
                if (!bestCandidate.empty())
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Option '%s' is unknown. Do you mean '--%s'?",
                                name.c_str(), bestCandidate.c_str());
                }
                else
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Option '%s' is unknown.", name.c_str());
                }
                return false;
            }
            arg = iterArg->second;
            if (equalPos != std::string::npos)
            {
                hasValue = true;
                value = strArg.substr(equalPos + 1);
            }
        }
        else if (strArg.size() >= 2 && strArg[0] == '-' &&
                 CPLGetValueType(strArg.c_str()) == CPL_VALUE_STRING)
        {
            for (size_t j = 1; j < strArg.size(); ++j)
            {
                name.clear();
                name += strArg[j];
                const auto iterArg = m_mapShortNameToArg.find(name);
                if (iterArg == m_mapShortNameToArg.end())
                {
                    const std::string nameWithoutDash = strArg.substr(1);
                    if (m_mapLongNameToArg.find(nameWithoutDash) !=
                        m_mapLongNameToArg.end())
                    {
                        ReportError(CE_Failure, CPLE_IllegalArg,
                                    "Short name option '%s' is unknown. Do you "
                                    "mean '--%s' (with leading double dash) ?",
                                    name.c_str(), nameWithoutDash.c_str());
                    }
                    else
                    {
                        const std::string bestCandidate =
                            GetSuggestionForArgumentName(nameWithoutDash);
                        if (!bestCandidate.empty())
                        {
                            ReportError(
                                CE_Failure, CPLE_IllegalArg,
                                "Short name option '%s' is unknown. Do you "
                                "mean '--%s' (with leading double dash) ?",
                                name.c_str(), bestCandidate.c_str());
                        }
                        else
                        {
                            ReportError(CE_Failure, CPLE_IllegalArg,
                                        "Short name option '%s' is unknown.",
                                        name.c_str());
                        }
                    }
                    return false;
                }
                arg = iterArg->second;
                if (strArg.size() > 2)
                {
                    if (arg->GetType() != GAAT_BOOLEAN)
                    {
                        ReportError(CE_Failure, CPLE_IllegalArg,
                                    "Invalid argument '%s'. Option '%s' is not "
                                    "a boolean option.",
                                    strArg.c_str(), name.c_str());
                        return false;
                    }

                    if (!ParseArgument(arg, name, "true", inConstructionValues))
                        return false;
                }
            }
            if (strArg.size() > 2)
            {
                lArgs.erase(lArgs.begin() + i);
                continue;
            }
        }
        else
        {
            ++i;
            continue;
        }
        CPLAssert(arg);

        if (arg && arg->GetType() == GAAT_BOOLEAN)
        {
            if (!hasValue)
            {
                hasValue = true;
                value = "true";
            }
        }

        if (!hasValue)
        {
            if (i + 1 == lArgs.size())
            {
                if (m_parseForAutoCompletion)
                {
                    lArgs.erase(lArgs.begin() + i);
                    break;
                }
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "Expected value for argument '%s', but ran short of tokens",
                    name.c_str());
                return false;
            }
            value = lArgs[i + 1];
            lArgs.erase(lArgs.begin() + i + 1);
        }

        if (arg && !ParseArgument(arg, name, value, inConstructionValues))
            return false;

        lArgs.erase(lArgs.begin() + i);
    }

    if (m_specialActionRequested)
    {
        return true;
    }

    const auto ProcessInConstructionValues = [&inConstructionValues]()
    {
        for (auto &[arg, value] : inConstructionValues)
        {
            if (arg->GetType() == GAAT_STRING_LIST)
            {
                if (!arg->Set(std::get<std::vector<std::string>>(
                        inConstructionValues[arg])))
                {
                    return false;
                }
            }
            else if (arg->GetType() == GAAT_INTEGER_LIST)
            {
                if (!arg->Set(
                        std::get<std::vector<int>>(inConstructionValues[arg])))
                {
                    return false;
                }
            }
            else if (arg->GetType() == GAAT_REAL_LIST)
            {
                if (!arg->Set(std::get<std::vector<double>>(
                        inConstructionValues[arg])))
                {
                    return false;
                }
            }
            else if (arg->GetType() == GAAT_DATASET_LIST)
            {
                if (!arg->Set(
                        std::move(std::get<std::vector<GDALArgDatasetValue>>(
                            inConstructionValues[arg]))))
                {
                    return false;
                }
            }
        }
        return true;
    };

    // Process positional arguments that have not been set through their
    // option name.
    size_t i = 0;
    size_t iCurPosArg = 0;
    while (i < lArgs.size() && iCurPosArg < m_positionalArgs.size())
    {
        GDALAlgorithmArg *arg = m_positionalArgs[iCurPosArg];
        while (arg->IsExplicitlySet())
        {
            ++iCurPosArg;
            if (iCurPosArg == m_positionalArgs.size())
                break;
            arg = m_positionalArgs[iCurPosArg];
        }
        if (iCurPosArg == m_positionalArgs.size())
        {
            break;
        }
        if (GDALAlgorithmArgTypeIsList(arg->GetType()) &&
            arg->GetMinCount() != arg->GetMaxCount())
        {
            if (iCurPosArg == 0)
            {
                size_t nCountAtEnd = 0;
                for (size_t j = 1; j < m_positionalArgs.size(); j++)
                {
                    const auto *otherArg = m_positionalArgs[j];
                    if (GDALAlgorithmArgTypeIsList(otherArg->GetType()))
                    {
                        if (otherArg->GetMinCount() != otherArg->GetMaxCount())
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Ambiguity in definition of positional "
                                "argument "
                                "'%s' given it has a varying number of values, "
                                "but follows argument '%s' which also has a "
                                "varying number of values",
                                otherArg->GetName().c_str(),
                                arg->GetName().c_str());
                            ProcessInConstructionValues();
                            return false;
                        }
                        nCountAtEnd += otherArg->GetMinCount();
                    }
                    else
                    {
                        if (!otherArg->IsRequired())
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Ambiguity in definition of positional "
                                "argument "
                                "'%s', given it is not required but follows "
                                "argument '%s' which has a varying number of "
                                "values",
                                otherArg->GetName().c_str(),
                                arg->GetName().c_str());
                            ProcessInConstructionValues();
                            return false;
                        }
                        nCountAtEnd++;
                    }
                }
                if (lArgs.size() < nCountAtEnd)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Not enough positional values.");
                    ProcessInConstructionValues();
                    return false;
                }
                for (; i < lArgs.size() - nCountAtEnd; ++i)
                {
                    if (!ParseArgument(arg, arg->GetName().c_str(), lArgs[i],
                                       inConstructionValues))
                    {
                        ProcessInConstructionValues();
                        return false;
                    }
                }
            }
            else if (iCurPosArg == m_positionalArgs.size() - 1)
            {
                for (; i < lArgs.size(); ++i)
                {
                    if (!ParseArgument(arg, arg->GetName().c_str(), lArgs[i],
                                       inConstructionValues))
                    {
                        ProcessInConstructionValues();
                        return false;
                    }
                }
            }
            else
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Ambiguity in definition of positional arguments: "
                            "arguments with varying number of values must be "
                            "first or last one.");
                return false;
            }
        }
        else
        {
            if (lArgs.size() - i < static_cast<size_t>(arg->GetMaxCount()))
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Not enough positional values.");
                return false;
            }
            const size_t iMax = i + arg->GetMaxCount();
            for (; i < iMax; ++i)
            {
                if (!ParseArgument(arg, arg->GetName().c_str(), lArgs[i],
                                   inConstructionValues))
                {
                    ProcessInConstructionValues();
                    return false;
                }
            }
        }
        ++iCurPosArg;
    }

    if (i < lArgs.size())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Positional values starting at '%s' are not expected.",
                    lArgs[i].c_str());
        return false;
    }

    if (!ProcessInConstructionValues())
    {
        return false;
    }

    // Skip to first unset positional argument.
    while (iCurPosArg < m_positionalArgs.size() &&
           m_positionalArgs[iCurPosArg]->IsExplicitlySet())
    {
        ++iCurPosArg;
    }
    // Check if this positional argument is required.
    if (iCurPosArg < m_positionalArgs.size() &&
        (GDALAlgorithmArgTypeIsList(m_positionalArgs[iCurPosArg]->GetType())
             ? m_positionalArgs[iCurPosArg]->GetMinCount() > 0
             : m_positionalArgs[iCurPosArg]->IsRequired()))
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Positional arguments starting at '%s' have not been "
                    "specified.",
                    m_positionalArgs[iCurPosArg]->GetMetaVar().c_str());
        return false;
    }

    return m_skipValidationInParseCommandLine || ValidateArguments();
}

/************************************************************************/
/*                     GDALAlgorithm::ReportError()                     */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALAlgorithm::ReportError(CPLErr eErrClass, CPLErrorNum err_no,
                                const char *fmt, ...) const
{
    va_list args;
    va_start(args, fmt);
    CPLError(eErrClass, err_no, "%s",
             std::string(m_name)
                 .append(": ")
                 .append(CPLString().vPrintf(fmt, args))
                 .c_str());
    va_end(args);
}

//! @endcond

/************************************************************************/
/*                   GDALAlgorithm::ProcessDatasetArg()                 */
/************************************************************************/

bool GDALAlgorithm::ProcessDatasetArg(GDALAlgorithmArg *arg,
                                      GDALAlgorithm *algForOutput)
{
    bool ret = true;

    const auto updateArg = algForOutput->GetArg(GDAL_ARG_NAME_UPDATE);
    const auto appendUpdateArg =
        algForOutput->GetArg(GDAL_ARG_NAME_APPEND_UPDATE);
    const bool hasUpdateArg = updateArg && updateArg->GetType() == GAAT_BOOLEAN;
    const bool hasAppendUpdateArg =
        appendUpdateArg && appendUpdateArg->GetType() == GAAT_BOOLEAN;
    const bool appendUpdate =
        hasAppendUpdateArg && appendUpdateArg->Get<bool>();
    const bool update =
        (hasUpdateArg && updateArg->Get<bool>()) || appendUpdate;
    const auto overwriteArg = algForOutput->GetArg(GDAL_ARG_NAME_OVERWRITE);
    const bool overwrite =
        (arg->IsOutput() && overwriteArg &&
         overwriteArg->GetType() == GAAT_BOOLEAN && overwriteArg->Get<bool>());
    auto outputArg = algForOutput->GetArg(GDAL_ARG_NAME_OUTPUT);
    auto &val = [arg]() -> GDALArgDatasetValue &
    {
        if (arg->GetType() == GAAT_DATASET_LIST)
            return arg->Get<std::vector<GDALArgDatasetValue>>()[0];
        else
            return arg->Get<GDALArgDatasetValue>();
    }();
    const bool onlyInputSpecifiedInUpdateAndOutputNotRequired =
        arg->GetName() == GDAL_ARG_NAME_INPUT && outputArg &&
        !outputArg->IsExplicitlySet() && !outputArg->IsRequired() && update &&
        !overwrite;
    if (!val.GetDatasetRef() && !val.IsNameSet())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Argument '%s' has no dataset object or dataset name.",
                    arg->GetName().c_str());
        ret = false;
    }
    else if (val.GetDatasetRef() && !CheckCanSetDatasetObject(arg))
    {
        return false;
    }
    else if (!val.GetDatasetRef() && arg->AutoOpenDataset() &&
             (!arg->IsOutput() || (arg == outputArg && update && !overwrite) ||
              onlyInputSpecifiedInUpdateAndOutputNotRequired))
    {
        int flags = arg->GetDatasetType();
        bool assignToOutputArg = false;

        // Check if input and output parameters point to the same
        // filename (for vector datasets)
        if (arg->GetName() == GDAL_ARG_NAME_INPUT && update && !overwrite &&
            outputArg && outputArg->GetType() == GAAT_DATASET)
        {
            auto &outputVal = outputArg->Get<GDALArgDatasetValue>();
            if (!outputVal.GetDatasetRef() &&
                outputVal.GetName() == val.GetName() &&
                (outputArg->GetDatasetInputFlags() & GADV_OBJECT) != 0)
            {
                assignToOutputArg = true;
                flags |= GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR;
            }
            else if (onlyInputSpecifiedInUpdateAndOutputNotRequired)
            {
                assignToOutputArg = true;
                flags |= GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR;
            }
        }

        if (!arg->IsOutput() || arg->GetDatasetInputFlags() == GADV_NAME)
            flags |= GDAL_OF_VERBOSE_ERROR;
        if ((arg == outputArg || !outputArg) && update)
            flags |= GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR;

        const auto readOnlyArg = algForOutput->GetArg(GDAL_ARG_NAME_READ_ONLY);
        const bool readOnly =
            (readOnlyArg && readOnlyArg->GetType() == GAAT_BOOLEAN &&
             readOnlyArg->Get<bool>());
        if (readOnly)
            flags &= ~GDAL_OF_UPDATE;

        CPLStringList aosOpenOptions;
        CPLStringList aosAllowedDrivers;
        if (arg->IsInput())
        {
            const auto ooArg = GetArg(GDAL_ARG_NAME_OPEN_OPTION);
            if (ooArg && ooArg->GetType() == GAAT_STRING_LIST)
                aosOpenOptions =
                    CPLStringList(ooArg->Get<std::vector<std::string>>());

            const auto ifArg = GetArg(GDAL_ARG_NAME_INPUT_FORMAT);
            if (ifArg && ifArg->GetType() == GAAT_STRING_LIST)
                aosAllowedDrivers =
                    CPLStringList(ifArg->Get<std::vector<std::string>>());
        }

        std::string osDatasetName = val.GetName();
        if (!m_referencePath.empty())
        {
            osDatasetName = GDALDataset::BuildFilename(
                osDatasetName.c_str(), m_referencePath.c_str(), true);
        }
        if (osDatasetName == "-" && (flags & GDAL_OF_UPDATE) == 0)
            osDatasetName = "/vsistdin/";

        // Handle special case of overview delete in GTiff which would fail
        // if it is COG without IGNORE_COG_LAYOUT_BREAK=YES open option.
        if ((flags & GDAL_OF_UPDATE) != 0 && m_callPath.size() == 4 &&
            m_callPath[2] == "overview" && m_callPath[3] == "delete" &&
            aosOpenOptions.FetchNameValue("IGNORE_COG_LAYOUT_BREAK") == nullptr)
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            GDALDriverH hDrv =
                GDALIdentifyDriver(osDatasetName.c_str(), nullptr);
            if (hDrv && EQUAL(GDALGetDescription(hDrv), "GTiff"))
            {
                // Cleaning does not break COG layout
                aosOpenOptions.SetNameValue("IGNORE_COG_LAYOUT_BREAK", "YES");
            }
        }

        auto poDS =
            GDALDataset::Open(osDatasetName.c_str(), flags,
                              aosAllowedDrivers.List(), aosOpenOptions.List());
        if (poDS)
        {
            if (assignToOutputArg)
            {
                // Avoid opening twice the same datasource if it is both
                // the input and output.
                // Known to cause problems with at least FGdb, SQLite
                // and GPKG drivers. See #4270
                // Restrict to those 3 drivers. For example it is known
                // to break with the PG driver due to the way it
                // manages transactions.
                auto poDriver = poDS->GetDriver();
                if (poDriver && (EQUAL(poDriver->GetDescription(), "FileGDB") ||
                                 EQUAL(poDriver->GetDescription(), "SQLite") ||
                                 EQUAL(poDriver->GetDescription(), "GPKG")))
                {
                    outputArg->Get<GDALArgDatasetValue>().Set(poDS);
                }
                else if (onlyInputSpecifiedInUpdateAndOutputNotRequired)
                {
                    outputArg->Get<GDALArgDatasetValue>().Set(poDS);
                }
            }
            val.Set(poDS);
            poDS->ReleaseRef();
        }
        else
        {
            ret = false;
        }
    }
    else if (onlyInputSpecifiedInUpdateAndOutputNotRequired &&
             val.GetDatasetRef())
    {
        outputArg->Get<GDALArgDatasetValue>().Set(val.GetDatasetRef());
    }

    // Deal with overwriting the output dataset
    if (ret && arg == outputArg && val.GetDatasetRef() == nullptr)
    {
        const auto appendArg = algForOutput->GetArg(GDAL_ARG_NAME_APPEND);
        const bool hasAppendArg =
            appendArg && appendArg->GetType() == GAAT_BOOLEAN;
        const bool append = (hasAppendArg && appendArg->Get<bool>());
        if (!append && !appendUpdate)
        {
            // If outputting to MEM, do not try to erase a real file of the same name!
            const auto outputFormatArg =
                algForOutput->GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT);
            if (!(outputFormatArg &&
                  outputFormatArg->GetType() == GAAT_STRING &&
                  (EQUAL(outputFormatArg->Get<std::string>().c_str(), "MEM") ||
                   EQUAL(outputFormatArg->Get<std::string>().c_str(),
                         "Memory"))))
            {
                const char *pszType = "";
                GDALDriver *poDriver = nullptr;
                if (!val.GetName().empty() &&
                    GDALDoesFileOrDatasetExist(val.GetName().c_str(), &pszType,
                                               &poDriver))
                {
                    if (!overwrite)
                    {
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "%s '%s' already exists. Specify the --overwrite "
                            "option to overwrite it%s.",
                            pszType, val.GetName().c_str(),
                            hasAppendArg || hasAppendUpdateArg
                                ? " or the --append option to append to it"
                            : hasUpdateArg
                                ? " or the --update option to update it"
                                : "");
                        return false;
                    }
                    else if (EQUAL(pszType, "File"))
                    {
                        VSIUnlink(val.GetName().c_str());
                    }
                    else if (EQUAL(pszType, "Directory"))
                    {
                        // We don't want the user to accidentally erase a non-GDAL dataset
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Directory '%s' already exists, but is not "
                                    "recognized as a valid GDAL dataset. "
                                    "Please manually delete it before retrying",
                                    val.GetName().c_str());
                        return false;
                    }
                    else if (poDriver)
                    {
                        CPLStringList aosDrivers;
                        aosDrivers.AddString(poDriver->GetDescription());
                        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                        GDALDriver::QuietDelete(val.GetName().c_str(),
                                                aosDrivers.List());
                    }
                }
            }
        }
    }

    return ret;
}

/************************************************************************/
/*                   GDALAlgorithm::ValidateArguments()                 */
/************************************************************************/

bool GDALAlgorithm::ValidateArguments()
{
    if (m_selectedSubAlg)
        return m_selectedSubAlg->ValidateArguments();

    if (m_specialActionRequested)
        return true;

    // If only --output=format=MEM is specified and not --output,
    // then set empty name for --output.
    auto outputArg = GetArg(GDAL_ARG_NAME_OUTPUT);
    auto outputFormatArg = GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT);
    if (outputArg && outputFormatArg && outputFormatArg->IsExplicitlySet() &&
        !outputArg->IsExplicitlySet() &&
        outputFormatArg->GetType() == GAAT_STRING &&
        EQUAL(outputFormatArg->Get<std::string>().c_str(), "MEM") &&
        outputArg->GetType() == GAAT_DATASET &&
        (outputArg->GetDatasetInputFlags() & GADV_NAME))
    {
        outputArg->Get<GDALArgDatasetValue>().Set("");
    }

    // The method may emit several errors if several constraints are not met.
    bool ret = true;
    std::map<std::string, std::string> mutualExclusionGroupUsed;
    for (auto &arg : m_args)
    {
        // Check mutually exclusive arguments
        if (arg->IsExplicitlySet())
        {
            const auto &mutualExclusionGroup = arg->GetMutualExclusionGroup();
            if (!mutualExclusionGroup.empty())
            {
                auto oIter =
                    mutualExclusionGroupUsed.find(mutualExclusionGroup);
                if (oIter != mutualExclusionGroupUsed.end())
                {
                    ret = false;
                    ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "Argument '%s' is mutually exclusive with '%s'.",
                        arg->GetName().c_str(), oIter->second.c_str());
                }
                else
                {
                    mutualExclusionGroupUsed[mutualExclusionGroup] =
                        arg->GetName();
                }
            }
        }

        if (arg->IsRequired() && !arg->IsExplicitlySet() &&
            !arg->HasDefaultValue())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Required argument '%s' has not been specified.",
                        arg->GetName().c_str());
            ret = false;
        }
        else if (arg->IsExplicitlySet() && arg->GetType() == GAAT_DATASET)
        {
            if (!ProcessDatasetArg(arg.get(), this))
                ret = false;
        }

        if (arg->IsExplicitlySet() && arg->GetType() == GAAT_DATASET_LIST &&
            arg->AutoOpenDataset())
        {
            auto &listVal = arg->Get<std::vector<GDALArgDatasetValue>>();
            if (listVal.size() == 1)
            {
                if (!ProcessDatasetArg(arg.get(), this))
                    ret = false;
            }
            else
            {
                for (auto &val : listVal)
                {
                    if (!val.GetDatasetRef() && val.GetName().empty())
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Argument '%s' has no dataset object or "
                                    "dataset name.",
                                    arg->GetName().c_str());
                        ret = false;
                    }
                    else if (!val.GetDatasetRef())
                    {
                        int flags =
                            arg->GetDatasetType() | GDAL_OF_VERBOSE_ERROR;

                        CPLStringList aosOpenOptions;
                        CPLStringList aosAllowedDrivers;
                        if (arg->GetName() == GDAL_ARG_NAME_INPUT)
                        {
                            const auto ooArg =
                                GetArg(GDAL_ARG_NAME_OPEN_OPTION);
                            if (ooArg && ooArg->GetType() == GAAT_STRING_LIST)
                            {
                                aosOpenOptions = CPLStringList(
                                    ooArg->Get<std::vector<std::string>>());
                            }

                            const auto ifArg =
                                GetArg(GDAL_ARG_NAME_INPUT_FORMAT);
                            if (ifArg && ifArg->GetType() == GAAT_STRING_LIST)
                            {
                                aosAllowedDrivers = CPLStringList(
                                    ifArg->Get<std::vector<std::string>>());
                            }

                            const auto updateArg = GetArg(GDAL_ARG_NAME_UPDATE);
                            if (updateArg &&
                                updateArg->GetType() == GAAT_BOOLEAN &&
                                updateArg->Get<bool>())
                            {
                                flags |= GDAL_OF_UPDATE;
                            }
                        }

                        auto poDS = std::unique_ptr<GDALDataset>(
                            GDALDataset::Open(val.GetName().c_str(), flags,
                                              aosAllowedDrivers.List(),
                                              aosOpenOptions.List()));
                        if (poDS)
                        {
                            val.Set(std::move(poDS));
                        }
                        else
                        {
                            ret = false;
                        }
                    }
                }
            }
        }

        if (arg->IsExplicitlySet() && !arg->RunValidationActions())
            ret = false;
    }

    for (const auto &f : m_validationActions)
    {
        if (!f())
            ret = false;
    }

    return ret;
}

/************************************************************************/
/*                GDALAlgorithm::InstantiateSubAlgorithm                */
/************************************************************************/

std::unique_ptr<GDALAlgorithm>
GDALAlgorithm::InstantiateSubAlgorithm(const std::string &name,
                                       bool suggestionAllowed) const
{
    auto ret = m_subAlgRegistry.Instantiate(name);
    auto childCallPath = m_callPath;
    childCallPath.push_back(name);
    if (!ret)
    {
        ret = GDALGlobalAlgorithmRegistry::GetSingleton()
                  .InstantiateDeclaredSubAlgorithm(childCallPath);
    }
    if (ret)
    {
        ret->SetCallPath(childCallPath);
    }
    else if (suggestionAllowed)
    {
        std::string bestCandidate;
        size_t bestDistance = std::numeric_limits<size_t>::max();
        for (const std::string &candidate : GetSubAlgorithmNames())
        {
            const size_t distance =
                CPLLevenshteinDistance(name.c_str(), candidate.c_str(),
                                       /* transpositionAllowed = */ true);
            if (distance < bestDistance)
            {
                bestCandidate = candidate;
                bestDistance = distance;
            }
            else if (distance == bestDistance)
            {
                bestCandidate.clear();
            }
        }
        if (!bestCandidate.empty() && bestDistance <= 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Algorithm '%s' is unknown. Do you mean '%s'?",
                     name.c_str(), bestCandidate.c_str());
        }
    }
    return ret;
}

/************************************************************************/
/*             GDALAlgorithm::GetSuggestionForArgumentName()            */
/************************************************************************/

std::string
GDALAlgorithm::GetSuggestionForArgumentName(const std::string &osName) const
{
    if (osName.size() >= 3)
    {
        std::string bestCandidate;
        size_t bestDistance = std::numeric_limits<size_t>::max();
        for (const auto &[key, value] : m_mapLongNameToArg)
        {
            CPL_IGNORE_RET_VAL(value);
            const size_t distance = CPLLevenshteinDistance(
                osName.c_str(), key.c_str(), /* transpositionAllowed = */ true);
            if (distance < bestDistance)
            {
                bestCandidate = key;
                bestDistance = distance;
            }
            else if (distance == bestDistance)
            {
                bestCandidate.clear();
            }
        }
        if (!bestCandidate.empty() &&
            bestDistance <= (bestCandidate.size() >= 4U ? 2U : 1U))
        {
            return bestCandidate;
        }
    }
    return std::string();
}

/************************************************************************/
/*                      GDALAlgorithm::GetArg()                         */
/************************************************************************/

const GDALAlgorithmArg *GDALAlgorithm::GetArg(const std::string &osName,
                                              bool suggestionAllowed) const
{
    const auto nPos = osName.find_first_not_of('-');
    if (nPos == std::string::npos)
        return nullptr;
    const std::string osKey = osName.substr(nPos);
    {
        const auto oIter = m_mapLongNameToArg.find(osKey);
        if (oIter != m_mapLongNameToArg.end())
            return oIter->second;
    }
    {
        const auto oIter = m_mapShortNameToArg.find(osKey);
        if (oIter != m_mapShortNameToArg.end())
            return oIter->second;
    }

    if (suggestionAllowed)
    {
        const std::string bestCandidate = GetSuggestionForArgumentName(osName);
        ;
        if (!bestCandidate.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Argument '%s' is unknown. Do you mean '%s'?",
                     osName.c_str(), bestCandidate.c_str());
        }
    }

    return nullptr;
}

/************************************************************************/
/*                   GDALAlgorithm::AddAliasFor()                       */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALAlgorithm::AddAliasFor(GDALInConstructionAlgorithmArg *arg,
                                const std::string &alias)
{
    if (cpl::contains(m_mapLongNameToArg, alias))
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Name '%s' already declared.",
                    alias.c_str());
    }
    else
    {
        m_mapLongNameToArg[alias] = arg;
    }
}

//! @endcond

/************************************************************************/
/*                 GDALAlgorithm::AddShortNameAliasFor()                */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALAlgorithm::AddShortNameAliasFor(GDALInConstructionAlgorithmArg *arg,
                                         char shortNameAlias)
{
    std::string alias;
    alias += shortNameAlias;
    if (cpl::contains(m_mapShortNameToArg, alias))
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Short name '%s' already declared.", alias.c_str());
    }
    else
    {
        m_mapShortNameToArg[alias] = arg;
    }
}

//! @endcond

/************************************************************************/
/*                   GDALAlgorithm::SetPositional()                     */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALAlgorithm::SetPositional(GDALInConstructionAlgorithmArg *arg)
{
    CPLAssert(std::find(m_positionalArgs.begin(), m_positionalArgs.end(),
                        arg) == m_positionalArgs.end());
    m_positionalArgs.push_back(arg);
}

//! @endcond

/************************************************************************/
/*                  GDALAlgorithm::HasSubAlgorithms()                   */
/************************************************************************/

bool GDALAlgorithm::HasSubAlgorithms() const
{
    if (!m_subAlgRegistry.empty())
        return true;
    return !GDALGlobalAlgorithmRegistry::GetSingleton()
                .GetDeclaredSubAlgorithmNames(m_callPath)
                .empty();
}

/************************************************************************/
/*                GDALAlgorithm::GetSubAlgorithmNames()                 */
/************************************************************************/

std::vector<std::string> GDALAlgorithm::GetSubAlgorithmNames() const
{
    std::vector<std::string> ret = m_subAlgRegistry.GetNames();
    const auto other = GDALGlobalAlgorithmRegistry::GetSingleton()
                           .GetDeclaredSubAlgorithmNames(m_callPath);
    ret.insert(ret.end(), other.begin(), other.end());
    if (!other.empty())
        std::sort(ret.begin(), ret.end());
    return ret;
}

/************************************************************************/
/*                     GDALAlgorithm::AddArg()                          */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(std::unique_ptr<GDALInConstructionAlgorithmArg> arg)
{
    auto argRaw = arg.get();
    const auto &longName = argRaw->GetName();
    if (!longName.empty())
    {
        if (longName[0] == '-')
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Long name '%s' should not start with '-'",
                        longName.c_str());
        }
        if (longName.find('=') != std::string::npos)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Long name '%s' should not contain a '=' character",
                        longName.c_str());
        }
        if (cpl::contains(m_mapLongNameToArg, longName))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Long name '%s' already declared", longName.c_str());
        }
        m_mapLongNameToArg[longName] = argRaw;
    }
    const auto &shortName = argRaw->GetShortName();
    if (!shortName.empty())
    {
        if (shortName.size() != 1 ||
            !((shortName[0] >= 'a' && shortName[0] <= 'z') ||
              (shortName[0] >= 'A' && shortName[0] <= 'Z') ||
              (shortName[0] >= '0' && shortName[0] <= '9')))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Short name '%s' should be a single letter or digit",
                        shortName.c_str());
        }
        if (cpl::contains(m_mapShortNameToArg, shortName))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Short name '%s' already declared", shortName.c_str());
        }
        m_mapShortNameToArg[shortName] = argRaw;
    }
    m_args.emplace_back(std::move(arg));
    return *(
        cpl::down_cast<GDALInConstructionAlgorithmArg *>(m_args.back().get()));
}

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(const std::string &longName, char chShortName,
                      const std::string &helpMessage, bool *pValue)
{
    return AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
        this,
        GDALAlgorithmArgDecl(longName, chShortName, helpMessage, GAAT_BOOLEAN),
        pValue));
}

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(const std::string &longName, char chShortName,
                      const std::string &helpMessage, std::string *pValue)
{
    return AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
        this,
        GDALAlgorithmArgDecl(longName, chShortName, helpMessage, GAAT_STRING),
        pValue));
}

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(const std::string &longName, char chShortName,
                      const std::string &helpMessage, int *pValue)
{
    return AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
        this,
        GDALAlgorithmArgDecl(longName, chShortName, helpMessage, GAAT_INTEGER),
        pValue));
}

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(const std::string &longName, char chShortName,
                      const std::string &helpMessage, double *pValue)
{
    return AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
        this,
        GDALAlgorithmArgDecl(longName, chShortName, helpMessage, GAAT_REAL),
        pValue));
}

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(const std::string &longName, char chShortName,
                      const std::string &helpMessage,
                      GDALArgDatasetValue *pValue, GDALArgDatasetType type)
{
    auto &arg = AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
                           this,
                           GDALAlgorithmArgDecl(longName, chShortName,
                                                helpMessage, GAAT_DATASET),
                           pValue))
                    .SetDatasetType(type);
    pValue->SetOwnerArgument(&arg);
    return arg;
}

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(const std::string &longName, char chShortName,
                      const std::string &helpMessage,
                      std::vector<std::string> *pValue)
{
    return AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
        this,
        GDALAlgorithmArgDecl(longName, chShortName, helpMessage,
                             GAAT_STRING_LIST),
        pValue));
}

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(const std::string &longName, char chShortName,
                      const std::string &helpMessage, std::vector<int> *pValue)
{
    return AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
        this,
        GDALAlgorithmArgDecl(longName, chShortName, helpMessage,
                             GAAT_INTEGER_LIST),
        pValue));
}

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(const std::string &longName, char chShortName,
                      const std::string &helpMessage,
                      std::vector<double> *pValue)
{
    return AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
        this,
        GDALAlgorithmArgDecl(longName, chShortName, helpMessage,
                             GAAT_REAL_LIST),
        pValue));
}

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddArg(const std::string &longName, char chShortName,
                      const std::string &helpMessage,
                      std::vector<GDALArgDatasetValue> *pValue,
                      GDALArgDatasetType type)
{
    return AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
                      this,
                      GDALAlgorithmArgDecl(longName, chShortName, helpMessage,
                                           GAAT_DATASET_LIST),
                      pValue))
        .SetDatasetType(type);
}

/************************************************************************/
/*                               MsgOrDefault()                         */
/************************************************************************/

inline const char *MsgOrDefault(const char *helpMessage,
                                const char *defaultMessage)
{
    return helpMessage ? helpMessage : defaultMessage;
}

/************************************************************************/
/*          GDALAlgorithm::SetAutoCompleteFunctionForFilename()         */
/************************************************************************/

/* static */
void GDALAlgorithm::SetAutoCompleteFunctionForFilename(
    GDALInConstructionAlgorithmArg &arg, GDALArgDatasetType type)
{
    arg.SetAutoCompleteFunction(
        [type](const std::string &currentValue) -> std::vector<std::string>
        {
            std::vector<std::string> oRet;

            {
                CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                VSIStatBufL sStat;
                if (!currentValue.empty() && currentValue.back() != '/' &&
                    VSIStatL(currentValue.c_str(), &sStat) == 0)
                {
                    return oRet;
                }
            }

            auto poDM = GetGDALDriverManager();
            std::set<std::string> oExtensions;
            if (type)
            {
                for (int i = 0; i < poDM->GetDriverCount(); ++i)
                {
                    auto poDriver = poDM->GetDriver(i);
                    if (((type & GDAL_OF_RASTER) != 0 &&
                         poDriver->GetMetadataItem(GDAL_DCAP_RASTER)) ||
                        ((type & GDAL_OF_VECTOR) != 0 &&
                         poDriver->GetMetadataItem(GDAL_DCAP_VECTOR)) ||
                        ((type & GDAL_OF_MULTIDIM_RASTER) != 0 &&
                         poDriver->GetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER)))
                    {
                        const char *pszExtensions =
                            poDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS);
                        if (pszExtensions)
                        {
                            const CPLStringList aosExts(
                                CSLTokenizeString2(pszExtensions, " ", 0));
                            for (const char *pszExt : cpl::Iterate(aosExts))
                                oExtensions.insert(CPLString(pszExt).tolower());
                        }
                    }
                }
            }

            std::string osDir;
            const CPLStringList aosVSIPrefixes(VSIGetFileSystemsPrefixes());
            std::string osPrefix;
            if (STARTS_WITH(currentValue.c_str(), "/vsi"))
            {
                for (const char *pszPrefix : cpl::Iterate(aosVSIPrefixes))
                {
                    if (STARTS_WITH(currentValue.c_str(), pszPrefix))
                    {
                        osPrefix = pszPrefix;
                        break;
                    }
                }
                if (osPrefix.empty())
                    return aosVSIPrefixes;
                if (currentValue == osPrefix)
                    osDir = osPrefix;
            }
            if (osDir.empty())
            {
                osDir = CPLGetDirnameSafe(currentValue.c_str());
                if (!osPrefix.empty() && osDir.size() < osPrefix.size())
                    osDir = std::move(osPrefix);
            }

            auto psDir = VSIOpenDir(osDir.c_str(), 0, nullptr);
            const std::string osSep = VSIGetDirectorySeparator(osDir.c_str());
            if (currentValue.empty())
                osDir.clear();
            const std::string currentFilename =
                CPLGetFilename(currentValue.c_str());
            if (psDir)
            {
                while (const VSIDIREntry *psEntry = VSIGetNextDirEntry(psDir))
                {
                    if ((currentFilename.empty() ||
                         STARTS_WITH(psEntry->pszName,
                                     currentFilename.c_str())) &&
                        strcmp(psEntry->pszName, ".") != 0 &&
                        strcmp(psEntry->pszName, "..") != 0 &&
                        (oExtensions.empty() ||
                         !strstr(psEntry->pszName, ".aux.xml")))
                    {
                        if (oExtensions.empty() ||
                            cpl::contains(
                                oExtensions,
                                CPLString(CPLGetExtensionSafe(psEntry->pszName))
                                    .tolower()) ||
                            VSI_ISDIR(psEntry->nMode))
                        {
                            std::string osVal;
                            if (osDir.empty())
                                osVal = psEntry->pszName;
                            else
                                osVal = CPLFormFilenameSafe(
                                    osDir.c_str(), psEntry->pszName, nullptr);
                            if (VSI_ISDIR(psEntry->nMode))
                                osVal += osSep;
                            oRet.push_back(std::move(osVal));
                        }
                    }
                }
                VSICloseDir(psDir);
            }
            return oRet;
        });
}

/************************************************************************/
/*                 GDALAlgorithm::AddInputDatasetArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALAlgorithm::AddInputDatasetArg(
    GDALArgDatasetValue *pValue, GDALArgDatasetType type,
    bool positionalAndRequired, const char *helpMessage)
{
    auto &arg = AddArg(
        GDAL_ARG_NAME_INPUT, 'i',
        MsgOrDefault(helpMessage,
                     CPLSPrintf("Input %s dataset",
                                GDALAlgorithmArgDatasetTypeName(type).c_str())),
        pValue, type);
    if (positionalAndRequired)
        arg.SetPositional().SetRequired();

    SetAutoCompleteFunctionForFilename(arg, type);

    AddValidationAction(
        [pValue]()
        {
            if (pValue->GetName() == "-")
                pValue->Set("/vsistdin/");
            return true;
        });

    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddInputDatasetArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALAlgorithm::AddInputDatasetArg(
    std::vector<GDALArgDatasetValue> *pValue, GDALArgDatasetType type,
    bool positionalAndRequired, const char *helpMessage)
{
    auto &arg = AddArg(
        GDAL_ARG_NAME_INPUT, 'i',
        MsgOrDefault(helpMessage,
                     CPLSPrintf("Input %s datasets",
                                GDALAlgorithmArgDatasetTypeName(type).c_str())),
        pValue, type);
    if (positionalAndRequired)
        arg.SetPositional().SetRequired();

    AddValidationAction(
        [pValue]()
        {
            for (auto &val : *pValue)
            {
                if (val.GetName() == "-")
                    val.Set("/vsistdin/");
            }
            return true;
        });
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOutputDatasetArg()                 */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALAlgorithm::AddOutputDatasetArg(
    GDALArgDatasetValue *pValue, GDALArgDatasetType type,
    bool positionalAndRequired, const char *helpMessage)
{
    auto &arg =
        AddArg(GDAL_ARG_NAME_OUTPUT, 'o',
               MsgOrDefault(
                   helpMessage,
                   CPLSPrintf("Output %s dataset",
                              GDALAlgorithmArgDatasetTypeName(type).c_str())),
               pValue, type)
            .SetIsInput(true)
            .SetIsOutput(true)
            .SetDatasetInputFlags(GADV_NAME)
            .SetDatasetOutputFlags(GADV_OBJECT);
    if (positionalAndRequired)
        arg.SetPositional().SetRequired();

    AddValidationAction(
        [this, &arg, pValue]()
        {
            if (pValue->GetName() == "-")
                pValue->Set("/vsistdout/");

            auto outputFormatArg = GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT);
            if (outputFormatArg && outputFormatArg->GetType() == GAAT_STRING &&
                (!outputFormatArg->IsExplicitlySet() ||
                 outputFormatArg->Get<std::string>().empty()) &&
                arg.IsExplicitlySet())
            {
                const auto vrtCompatible =
                    outputFormatArg->GetMetadataItem(GAAMDI_VRT_COMPATIBLE);
                if (vrtCompatible && !vrtCompatible->empty() &&
                    vrtCompatible->front() == "false" &&
                    EQUAL(
                        CPLGetExtensionSafe(pValue->GetName().c_str()).c_str(),
                        "VRT"))
                {
                    ReportError(
                        CE_Failure, CPLE_NotSupported,
                        "VRT output is not supported.%s",
                        outputFormatArg->GetDescription().find("GDALG") !=
                                std::string::npos
                            ? " Consider using the GDALG driver instead (files "
                              "with .gdalg.json extension)"
                            : "");
                    return false;
                }
                else if (pValue->GetName().size() > strlen(".gdalg.json") &&
                         EQUAL(pValue->GetName()
                                   .substr(pValue->GetName().size() -
                                           strlen(".gdalg.json"))
                                   .c_str(),
                               ".gdalg.json") &&
                         outputFormatArg->GetDescription().find("GDALG") ==
                             std::string::npos)
                {
                    ReportError(CE_Failure, CPLE_NotSupported,
                                "GDALG output is not supported");
                    return false;
                }
            }
            return true;
        });

    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOverwriteArg()                     */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddOverwriteArg(bool *pValue, const char *helpMessage)
{
    return AddArg(GDAL_ARG_NAME_OVERWRITE, 0,
                  MsgOrDefault(
                      helpMessage,
                      _("Whether overwriting existing output is allowed")),
                  pValue)
        .SetDefault(false);
}

/************************************************************************/
/*                 GDALAlgorithm::AddUpdateArg()                        */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddUpdateArg(bool *pValue, const char *helpMessage)
{
    return AddArg(GDAL_ARG_NAME_UPDATE, 0,
                  MsgOrDefault(
                      helpMessage,
                      _("Whether to open existing dataset in update mode")),
                  pValue)
        .SetDefault(false);
}

/************************************************************************/
/*                GDALAlgorithm::AddAppendUpdateArg()                   */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddAppendUpdateArg(bool *pValue, const char *helpMessage)
{
    return AddArg("append", 0,
                  MsgOrDefault(helpMessage,
                               _("Whether to append to an existing dataset")),
                  pValue)
        .AddHiddenAlias(GDAL_ARG_NAME_APPEND_UPDATE)
        .SetDefault(false);
}

/************************************************************************/
/*                 GDALAlgorithm::AddOptionsSuggestions()               */
/************************************************************************/

/* static */
bool GDALAlgorithm::AddOptionsSuggestions(const char *pszXML, int datasetType,
                                          const std::string &currentValue,
                                          std::vector<std::string> &oRet)
{
    if (!pszXML)
        return false;
    CPLXMLTreeCloser poTree(CPLParseXMLString(pszXML));
    if (!poTree)
        return false;
    for (const CPLXMLNode *psChild = poTree.get()->psChild; psChild;
         psChild = psChild->psNext)
    {
        const char *pszName = CPLGetXMLValue(psChild, "name", nullptr);
        if (pszName && currentValue == pszName &&
            EQUAL(psChild->pszValue, "Option"))
        {
            const char *pszType = CPLGetXMLValue(psChild, "type", "");
            const char *pszMin = CPLGetXMLValue(psChild, "min", nullptr);
            const char *pszMax = CPLGetXMLValue(psChild, "max", nullptr);
            if (EQUAL(pszType, "string-select"))
            {
                for (const CPLXMLNode *psChild2 = psChild->psChild; psChild2;
                     psChild2 = psChild2->psNext)
                {
                    if (EQUAL(psChild2->pszValue, "Value"))
                    {
                        oRet.push_back(CPLGetXMLValue(psChild2, "", ""));
                    }
                }
            }
            else if (EQUAL(pszType, "boolean"))
            {
                oRet.push_back("NO");
                oRet.push_back("YES");
            }
            else if (EQUAL(pszType, "int"))
            {
                if (pszMin && pszMax && atoi(pszMax) - atoi(pszMin) > 0 &&
                    atoi(pszMax) - atoi(pszMin) < 25)
                {
                    const int nMax = atoi(pszMax);
                    for (int i = atoi(pszMin); i <= nMax; ++i)
                        oRet.push_back(std::to_string(i));
                }
            }

            if (oRet.empty())
            {
                if (pszMin && pszMax)
                {
                    oRet.push_back(std::string("##"));
                    oRet.push_back(std::string("validity range: [")
                                       .append(pszMin)
                                       .append(",")
                                       .append(pszMax)
                                       .append("]"));
                }
                else if (pszMin)
                {
                    oRet.push_back(std::string("##"));
                    oRet.push_back(
                        std::string("validity range: >= ").append(pszMin));
                }
                else if (pszMax)
                {
                    oRet.push_back(std::string("##"));
                    oRet.push_back(
                        std::string("validity range: <= ").append(pszMax));
                }
                else if (const char *pszDescription =
                             CPLGetXMLValue(psChild, "description", nullptr))
                {
                    oRet.push_back(std::string("##"));
                    oRet.push_back(std::string("type: ")
                                       .append(pszType)
                                       .append(", description: ")
                                       .append(pszDescription));
                }
            }

            return true;
        }
    }

    for (const CPLXMLNode *psChild = poTree.get()->psChild; psChild;
         psChild = psChild->psNext)
    {
        const char *pszName = CPLGetXMLValue(psChild, "name", nullptr);
        if (pszName && EQUAL(psChild->pszValue, "Option"))
        {
            const char *pszScope = CPLGetXMLValue(psChild, "scope", nullptr);
            if (!pszScope ||
                (EQUAL(pszScope, "raster") &&
                 (datasetType & GDAL_OF_RASTER) != 0) ||
                (EQUAL(pszScope, "vector") &&
                 (datasetType & GDAL_OF_VECTOR) != 0))
            {
                oRet.push_back(std::string(pszName).append("="));
            }
        }
    }

    return false;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOpenOptionsArg()                   */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddOpenOptionsArg(std::vector<std::string> *pValue,
                                 const char *helpMessage)
{
    auto &arg = AddArg(GDAL_ARG_NAME_OPEN_OPTION, 0,
                       MsgOrDefault(helpMessage, _("Open options")), pValue)
                    .AddAlias("oo")
                    .SetMetaVar("<KEY>=<VALUE>")
                    .SetPackedValuesAllowed(false)
                    .SetCategory(GAAC_ADVANCED);

    arg.AddValidationAction([this, &arg]()
                            { return ParseAndValidateKeyValue(arg); });

    arg.SetAutoCompleteFunction(
        [this](const std::string &currentValue)
        {
            std::vector<std::string> oRet;

            int datasetType =
                GDAL_OF_RASTER | GDAL_OF_VECTOR | GDAL_OF_MULTIDIM_RASTER;
            auto inputArg = GetArg(GDAL_ARG_NAME_INPUT);
            if (inputArg && (inputArg->GetType() == GAAT_DATASET ||
                             inputArg->GetType() == GAAT_DATASET_LIST))
            {
                datasetType = inputArg->GetDatasetType();
            }

            auto inputFormat = GetArg(GDAL_ARG_NAME_INPUT_FORMAT);
            if (inputFormat && inputFormat->GetType() == GAAT_STRING_LIST &&
                inputFormat->IsExplicitlySet())
            {
                const auto &aosAllowedDrivers =
                    inputFormat->Get<std::vector<std::string>>();
                if (aosAllowedDrivers.size() == 1)
                {
                    auto poDriver = GetGDALDriverManager()->GetDriverByName(
                        aosAllowedDrivers[0].c_str());
                    if (poDriver)
                    {
                        AddOptionsSuggestions(
                            poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST),
                            datasetType, currentValue, oRet);
                    }
                    return oRet;
                }
            }

            if (inputArg && inputArg->GetType() == GAAT_DATASET)
            {
                auto poDM = GetGDALDriverManager();
                auto &datasetValue = inputArg->Get<GDALArgDatasetValue>();
                const auto &osDSName = datasetValue.GetName();
                const std::string osExt = CPLGetExtensionSafe(osDSName.c_str());
                if (!osExt.empty())
                {
                    std::set<std::string> oVisitedExtensions;
                    for (int i = 0; i < poDM->GetDriverCount(); ++i)
                    {
                        auto poDriver = poDM->GetDriver(i);
                        if (((datasetType & GDAL_OF_RASTER) != 0 &&
                             poDriver->GetMetadataItem(GDAL_DCAP_RASTER)) ||
                            ((datasetType & GDAL_OF_VECTOR) != 0 &&
                             poDriver->GetMetadataItem(GDAL_DCAP_VECTOR)) ||
                            ((datasetType & GDAL_OF_MULTIDIM_RASTER) != 0 &&
                             poDriver->GetMetadataItem(
                                 GDAL_DCAP_MULTIDIM_RASTER)))
                        {
                            const char *pszExtensions =
                                poDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS);
                            if (pszExtensions)
                            {
                                const CPLStringList aosExts(
                                    CSLTokenizeString2(pszExtensions, " ", 0));
                                for (const char *pszExt : cpl::Iterate(aosExts))
                                {
                                    if (EQUAL(pszExt, osExt.c_str()) &&
                                        !cpl::contains(oVisitedExtensions,
                                                       pszExt))
                                    {
                                        oVisitedExtensions.insert(pszExt);
                                        if (AddOptionsSuggestions(
                                                poDriver->GetMetadataItem(
                                                    GDAL_DMD_OPENOPTIONLIST),
                                                datasetType, currentValue,
                                                oRet))
                                        {
                                            return oRet;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return oRet;
        });

    return arg;
}

/************************************************************************/
/*                            ValidateFormat()                          */
/************************************************************************/

bool GDALAlgorithm::ValidateFormat(const GDALAlgorithmArg &arg,
                                   bool bStreamAllowed,
                                   bool bGDALGAllowed) const
{
    if (arg.GetChoices().empty())
    {
        const auto Validate =
            [this, &arg, bStreamAllowed, bGDALGAllowed](const std::string &val)
        {
            if (bStreamAllowed && EQUAL(val.c_str(), "stream"))
                return true;

            if (EQUAL(val.c_str(), "GDALG") && arg.IsOutput())
            {
                if (bGDALGAllowed)
                {
                    return true;
                }
                else
                {
                    ReportError(CE_Failure, CPLE_NotSupported,
                                "GDALG output is not supported.");
                    return false;
                }
            }

            const auto vrtCompatible =
                arg.GetMetadataItem(GAAMDI_VRT_COMPATIBLE);
            if (vrtCompatible && !vrtCompatible->empty() &&
                vrtCompatible->front() == "false" && EQUAL(val.c_str(), "VRT"))
            {
                ReportError(CE_Failure, CPLE_NotSupported,
                            "VRT output is not supported.%s",
                            bGDALGAllowed
                                ? " Consider using the GDALG driver instead "
                                  "(files with .gdalg.json extension)."
                                : "");
                return false;
            }

            auto hDriver = GDALGetDriverByName(val.c_str());
            if (!hDriver)
            {
                auto poMissingDriver =
                    GetGDALDriverManager()->GetHiddenDriverByName(val.c_str());
                if (poMissingDriver)
                {
                    const std::string msg =
                        GDALGetMessageAboutMissingPluginDriver(poMissingDriver);
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Invalid value for argument '%s'. Driver '%s' "
                                "not found but it known. However plugin %s",
                                arg.GetName().c_str(), val.c_str(),
                                msg.c_str());
                }
                else
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Invalid value for argument '%s'. Driver '%s' "
                                "does not exist.",
                                arg.GetName().c_str(), val.c_str());
                }
                return false;
            }

            const auto caps = arg.GetMetadataItem(GAAMDI_REQUIRED_CAPABILITIES);
            if (caps)
            {
                for (const std::string &cap : *caps)
                {
                    const char *pszVal =
                        GDALGetMetadataItem(hDriver, cap.c_str(), nullptr);
                    if (!(pszVal && pszVal[0]))
                    {
                        if (cap == GDAL_DCAP_CREATECOPY &&
                            std::find(caps->begin(), caps->end(),
                                      GDAL_DCAP_RASTER) != caps->end() &&
                            GDALGetMetadataItem(hDriver, GDAL_DCAP_RASTER,
                                                nullptr) &&
                            GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE,
                                                nullptr))
                        {
                            // if it supports Create, it supports CreateCopy
                        }
                        else if (cap == GDAL_DMD_EXTENSIONS)
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Invalid value for argument '%s'. Driver '%s' "
                                "does "
                                "not advertise any file format extension.",
                                arg.GetName().c_str(), val.c_str());
                            return false;
                        }
                        else
                        {
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Invalid value for argument '%s'. Driver '%s' "
                                "does "
                                "not expose the required '%s' capability.",
                                arg.GetName().c_str(), val.c_str(),
                                cap.c_str());
                            return false;
                        }
                    }
                }
            }
            return true;
        };

        if (arg.GetType() == GAAT_STRING)
        {
            return Validate(arg.Get<std::string>());
        }
        else if (arg.GetType() == GAAT_STRING_LIST)
        {
            for (const auto &val : arg.Get<std::vector<std::string>>())
            {
                if (!Validate(val))
                    return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                    FormatAutoCompleteFunction()                      */
/************************************************************************/

static std::vector<std::string>
FormatAutoCompleteFunction(const GDALAlgorithmArg &arg,
                           bool /* bStreamAllowed */, bool bGDALGAllowed)
{
    std::vector<std::string> res;
    auto poDM = GetGDALDriverManager();
    const auto vrtCompatible = arg.GetMetadataItem(GAAMDI_VRT_COMPATIBLE);
    const auto caps = arg.GetMetadataItem(GAAMDI_REQUIRED_CAPABILITIES);
    for (int i = 0; i < poDM->GetDriverCount(); ++i)
    {
        auto poDriver = poDM->GetDriver(i);

        if (vrtCompatible && !vrtCompatible->empty() &&
            vrtCompatible->front() == "false" &&
            EQUAL(poDriver->GetDescription(), "VRT"))
        {
            // do nothing
        }
        else if (caps)
        {
            bool ok = true;
            for (const std::string &cap : *caps)
            {
                if (cap == GDAL_ALG_DCAP_RASTER_OR_MULTIDIM_RASTER)
                {
                    if (!poDriver->GetMetadataItem(GDAL_DCAP_RASTER) &&
                        !poDriver->GetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER))
                    {
                        ok = false;
                        break;
                    }
                }
                else if (const char *pszVal =
                             poDriver->GetMetadataItem(cap.c_str());
                         pszVal && pszVal[0])
                {
                }
                else if (cap == GDAL_DCAP_CREATECOPY &&
                         (std::find(caps->begin(), caps->end(),
                                    GDAL_DCAP_RASTER) != caps->end() &&
                          poDriver->GetMetadataItem(GDAL_DCAP_RASTER)) &&
                         poDriver->GetMetadataItem(GDAL_DCAP_CREATE))
                {
                    // if it supports Create, it supports CreateCopy
                }
                else
                {
                    ok = false;
                    break;
                }
            }
            if (ok)
            {
                res.push_back(poDriver->GetDescription());
            }
        }
    }
    if (bGDALGAllowed)
        res.push_back("GDALG");
    return res;
}

/************************************************************************/
/*                 GDALAlgorithm::AddInputFormatsArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddInputFormatsArg(std::vector<std::string> *pValue,
                                  const char *helpMessage)
{
    auto &arg = AddArg(GDAL_ARG_NAME_INPUT_FORMAT, 0,
                       MsgOrDefault(helpMessage, _("Input formats")), pValue)
                    .AddAlias("if")
                    .SetCategory(GAAC_ADVANCED);
    arg.AddValidationAction([this, &arg]()
                            { return ValidateFormat(arg, false, false); });
    arg.SetAutoCompleteFunction(
        [&arg](const std::string &)
        { return FormatAutoCompleteFunction(arg, false, false); });
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOutputFormatArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddOutputFormatArg(std::string *pValue, bool bStreamAllowed,
                                  bool bGDALGAllowed, const char *helpMessage)
{
    auto &arg = AddArg(GDAL_ARG_NAME_OUTPUT_FORMAT, 'f',
                       MsgOrDefault(helpMessage,
                                    bGDALGAllowed
                                        ? _("Output format (\"GDALG\" allowed)")
                                        : _("Output format")),
                       pValue)
                    .AddAlias("of")
                    .AddAlias("format");
    arg.AddValidationAction(
        [this, &arg, bStreamAllowed, bGDALGAllowed]()
        { return ValidateFormat(arg, bStreamAllowed, bGDALGAllowed); });
    arg.SetAutoCompleteFunction(
        [&arg, bStreamAllowed, bGDALGAllowed](const std::string &) {
            return FormatAutoCompleteFunction(arg, bStreamAllowed,
                                              bGDALGAllowed);
        });
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOutputDataTypeArg()                */
/************************************************************************/
GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddOutputDataTypeArg(std::string *pValue,
                                    const char *helpMessage)
{
    auto &arg =
        AddArg(GDAL_ARG_NAME_OUTPUT_DATA_TYPE, 0,
               MsgOrDefault(helpMessage, _("Output data type")), pValue)
            .AddAlias("ot")
            .AddAlias("datatype")
            .AddMetadataItem("type", {"GDALDataType"})
            .SetChoices("Byte", "Int8", "UInt16", "Int16", "UInt32", "Int32",
                        "UInt64", "Int64", "CInt16", "CInt32", "Float16",
                        "Float32", "Float64", "CFloat32", "CFloat64");
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddNodataDataTypeArg()                */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddNodataDataTypeArg(std::string *pValue, bool noneAllowed,
                                    const std::string &optionName,
                                    const char *helpMessage)
{
    auto &arg =
        AddArg(optionName, 0,
               MsgOrDefault(helpMessage,
                            _("Assign a specified nodata value to output bands "
                              "('none', numeric value, 'nan', 'inf', '-inf')")),
               pValue);
    arg.AddValidationAction(
        [this, pValue, noneAllowed, optionName]()
        {
            if (!(noneAllowed && EQUAL(pValue->c_str(), "none")))
            {
                char *endptr = nullptr;
                CPLStrtod(pValue->c_str(), &endptr);
                if (endptr != pValue->c_str() + pValue->size())
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Value of '%s' should be 'none', a "
                                "numeric value, 'nan', 'inf' or '-inf'",
                                optionName.c_str());
                    return false;
                }
            }
            return true;
        });
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOutputStringArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddOutputStringArg(std::string *pValue, const char *helpMessage)
{
    return AddArg(
               "output-string", 0,
               MsgOrDefault(helpMessage,
                            _("Output string, in which the result is placed")),
               pValue)
        .SetHiddenForCLI()
        .SetIsInput(false)
        .SetIsOutput(true);
}

/************************************************************************/
/*                    GDALAlgorithm::AddLayerNameArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddLayerNameArg(std::string *pValue, const char *helpMessage)
{
    return AddArg("layer", 'l', MsgOrDefault(helpMessage, _("Layer name")),
                  pValue);
}

/************************************************************************/
/*                    GDALAlgorithm::AddLayerNameArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddLayerNameArg(std::vector<std::string> *pValue,
                               const char *helpMessage)
{
    return AddArg("layer", 'l', MsgOrDefault(helpMessage, _("Layer name")),
                  pValue);
}

/************************************************************************/
/*                    GDALAlgorithm::AddBandArg()                       */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddBandArg(int *pValue, const char *helpMessage)
{
    auto &arg =
        AddArg("band", 'b',
               MsgOrDefault(helpMessage, _("Input band (1-based index)")),
               pValue)
            .AddValidationAction(
                [pValue]()
                {
                    if (*pValue <= 0)
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "Value of 'band' should greater or equal to 1.");
                        return false;
                    }
                    return true;
                });

    AddValidationAction(
        [this, &arg, pValue]()
        {
            auto inputDatasetArg = GetArg(GDAL_ARG_NAME_INPUT);
            if (arg.IsExplicitlySet() && inputDatasetArg &&
                inputDatasetArg->GetType() == GAAT_DATASET &&
                inputDatasetArg->IsExplicitlySet() &&
                (inputDatasetArg->GetDatasetType() & GDAL_OF_RASTER) != 0)
            {
                auto poDS =
                    inputDatasetArg->Get<GDALArgDatasetValue>().GetDatasetRef();
                if (poDS && *pValue > poDS->GetRasterCount())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Value of 'band' should be greater or equal than "
                             "1 and less or equal than %d.",
                             poDS->GetRasterCount());
                    return false;
                }
            }
            return true;
        });

    return arg;
}

/************************************************************************/
/*                    GDALAlgorithm::AddBandArg()                       */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddBandArg(std::vector<int> *pValue, const char *helpMessage)
{
    auto &arg =
        AddArg("band", 'b',
               MsgOrDefault(helpMessage, _("Input band(s) (1-based index)")),
               pValue)
            .AddValidationAction(
                [pValue]()
                {
                    for (int val : *pValue)
                    {
                        if (val <= 0)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value of 'band' should greater or equal "
                                     "to 1.");
                            return false;
                        }
                    }
                    return true;
                });

    AddValidationAction(
        [this, &arg, pValue]()
        {
            auto inputDatasetArg = GetArg(GDAL_ARG_NAME_INPUT);
            if (arg.IsExplicitlySet() && inputDatasetArg &&
                inputDatasetArg->GetType() == GAAT_DATASET &&
                inputDatasetArg->IsExplicitlySet() &&
                (inputDatasetArg->GetDatasetType() & GDAL_OF_RASTER) != 0)
            {
                auto poDS =
                    inputDatasetArg->Get<GDALArgDatasetValue>().GetDatasetRef();
                if (poDS)
                {
                    for (int val : *pValue)
                    {
                        if (val > poDS->GetRasterCount())
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value of 'band' should be greater or "
                                     "equal than 1 and less or equal than %d.",
                                     poDS->GetRasterCount());
                            return false;
                        }
                    }
                }
            }
            return true;
        });

    return arg;
}

/************************************************************************/
/*                     ParseAndValidateKeyValue()                       */
/************************************************************************/

bool GDALAlgorithm::ParseAndValidateKeyValue(GDALAlgorithmArg &arg)
{
    const auto Validate = [this, &arg](const std::string &val)
    {
        if (val.find('=') == std::string::npos)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Invalid value for argument '%s'. <KEY>=<VALUE> expected",
                arg.GetName().c_str());
            return false;
        }

        return true;
    };

    if (arg.GetType() == GAAT_STRING)
    {
        return Validate(arg.Get<std::string>());
    }
    else if (arg.GetType() == GAAT_STRING_LIST)
    {
        std::vector<std::string> &vals = arg.Get<std::vector<std::string>>();
        if (vals.size() == 1)
        {
            // Try to split A=B,C=D into A=B and C=D if there is no ambiguity
            std::vector<std::string> newVals;
            std::string curToken;
            bool canSplitOnComma = true;
            char lastSep = 0;
            bool inString = false;
            bool equalFoundInLastToken = false;
            for (char c : vals[0])
            {
                if (!inString && c == ',')
                {
                    if (lastSep != '=' || !equalFoundInLastToken)
                    {
                        canSplitOnComma = false;
                        break;
                    }
                    lastSep = c;
                    newVals.push_back(curToken);
                    curToken.clear();
                    equalFoundInLastToken = false;
                }
                else if (!inString && c == '=')
                {
                    if (lastSep == '=')
                    {
                        canSplitOnComma = false;
                        break;
                    }
                    equalFoundInLastToken = true;
                    lastSep = c;
                    curToken += c;
                }
                else if (c == '"')
                {
                    inString = !inString;
                    curToken += c;
                }
                else
                {
                    curToken += c;
                }
            }
            if (canSplitOnComma && !inString && equalFoundInLastToken)
            {
                if (!curToken.empty())
                    newVals.emplace_back(std::move(curToken));
                vals = std::move(newVals);
            }
        }

        for (const auto &val : vals)
        {
            if (!Validate(val))
                return false;
        }
    }

    return true;
}

/************************************************************************/
/*                             IsGDALGOutput()                          */
/************************************************************************/

bool GDALAlgorithm::IsGDALGOutput() const
{
    bool isGDALGOutput = false;
    const auto outputFormatArg = GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT);
    const auto outputArg = GetArg(GDAL_ARG_NAME_OUTPUT);
    if (outputArg && outputArg->GetType() == GAAT_DATASET &&
        outputArg->IsExplicitlySet())
    {
        if (outputFormatArg && outputFormatArg->GetType() == GAAT_STRING &&
            outputFormatArg->IsExplicitlySet())
        {
            const auto &val =
                outputFormatArg->GDALAlgorithmArg::Get<std::string>();
            isGDALGOutput = EQUAL(val.c_str(), "GDALG");
        }
        else
        {
            const auto &filename =
                outputArg->GDALAlgorithmArg::Get<GDALArgDatasetValue>();
            isGDALGOutput =
                filename.GetName().size() > strlen(".gdalg.json") &&
                EQUAL(filename.GetName().c_str() + filename.GetName().size() -
                          strlen(".gdalg.json"),
                      ".gdalg.json");
        }
    }
    return isGDALGOutput;
}

/************************************************************************/
/*                          ProcessGDALGOutput()                        */
/************************************************************************/

GDALAlgorithm::ProcessGDALGOutputRet GDALAlgorithm::ProcessGDALGOutput()
{
    if (!SupportsStreamedOutput())
        return ProcessGDALGOutputRet::NOT_GDALG;

    if (IsGDALGOutput())
    {
        const auto outputArg = GetArg(GDAL_ARG_NAME_OUTPUT);
        const auto &filename =
            outputArg->GDALAlgorithmArg::Get<GDALArgDatasetValue>().GetName();
        VSIStatBufL sStat;
        if (VSIStatL(filename.c_str(), &sStat) == 0)
        {
            const auto overwriteArg = GetArg(GDAL_ARG_NAME_OVERWRITE);
            if (overwriteArg && overwriteArg->GetType() == GAAT_BOOLEAN)
            {
                if (!overwriteArg->GDALAlgorithmArg::Get<bool>())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "File '%s' already exists. Specify the "
                             "--overwrite option to overwrite it.",
                             filename.c_str());
                    return ProcessGDALGOutputRet::GDALG_ERROR;
                }
            }
        }

        std::string osCommandLine;

        for (const auto &path : GDALAlgorithm::m_callPath)
        {
            if (!osCommandLine.empty())
                osCommandLine += ' ';
            osCommandLine += path;
        }

        for (const auto &arg : GetArgs())
        {
            if (arg->IsExplicitlySet() &&
                arg->GetName() != GDAL_ARG_NAME_OUTPUT &&
                arg->GetName() != GDAL_ARG_NAME_OUTPUT_FORMAT &&
                arg->GetName() != GDAL_ARG_NAME_UPDATE &&
                arg->GetName() != GDAL_ARG_NAME_OVERWRITE)
            {
                osCommandLine += ' ';
                std::string strArg;
                if (!arg->Serialize(strArg))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot serialize argument %s",
                             arg->GetName().c_str());
                    return ProcessGDALGOutputRet::GDALG_ERROR;
                }
                osCommandLine += strArg;
            }
        }

        osCommandLine += " --output-format stream --output streamed_dataset";

        return SaveGDALG(filename, osCommandLine)
                   ? ProcessGDALGOutputRet::GDALG_OK
                   : ProcessGDALGOutputRet::GDALG_ERROR;
    }

    return ProcessGDALGOutputRet::NOT_GDALG;
}

/************************************************************************/
/*                      GDALAlgorithm::SaveGDALG()                      */
/************************************************************************/

/* static */ bool GDALAlgorithm::SaveGDALG(const std::string &filename,
                                           const std::string &commandLine)
{
    CPLJSONDocument oDoc;
    oDoc.GetRoot().Add("type", "gdal_streamed_alg");
    oDoc.GetRoot().Add("command_line", commandLine);
    oDoc.GetRoot().Add("gdal_version", GDALVersionInfo("VERSION_NUM"));

    return oDoc.Save(filename);
}

/************************************************************************/
/*                 GDALAlgorithm::AddCreationOptionsArg()               */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddCreationOptionsArg(std::vector<std::string> *pValue,
                                     const char *helpMessage)
{
    auto &arg = AddArg("creation-option", 0,
                       MsgOrDefault(helpMessage, _("Creation option")), pValue)
                    .AddAlias("co")
                    .SetMetaVar("<KEY>=<VALUE>")
                    .SetPackedValuesAllowed(false);
    arg.AddValidationAction([this, &arg]()
                            { return ParseAndValidateKeyValue(arg); });

    arg.SetAutoCompleteFunction(
        [this](const std::string &currentValue)
        {
            std::vector<std::string> oRet;

            int datasetType =
                GDAL_OF_RASTER | GDAL_OF_VECTOR | GDAL_OF_MULTIDIM_RASTER;
            auto outputArg = GetArg(GDAL_ARG_NAME_OUTPUT);
            if (outputArg && (outputArg->GetType() == GAAT_DATASET ||
                              outputArg->GetType() == GAAT_DATASET_LIST))
            {
                datasetType = outputArg->GetDatasetType();
            }

            auto outputFormat = GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT);
            if (outputFormat && outputFormat->GetType() == GAAT_STRING &&
                outputFormat->IsExplicitlySet())
            {
                auto poDriver = GetGDALDriverManager()->GetDriverByName(
                    outputFormat->Get<std::string>().c_str());
                if (poDriver)
                {
                    AddOptionsSuggestions(
                        poDriver->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST),
                        datasetType, currentValue, oRet);
                }
                return oRet;
            }

            if (outputArg && outputArg->GetType() == GAAT_DATASET)
            {
                auto poDM = GetGDALDriverManager();
                auto &datasetValue = outputArg->Get<GDALArgDatasetValue>();
                const auto &osDSName = datasetValue.GetName();
                const std::string osExt = CPLGetExtensionSafe(osDSName.c_str());
                if (!osExt.empty())
                {
                    std::set<std::string> oVisitedExtensions;
                    for (int i = 0; i < poDM->GetDriverCount(); ++i)
                    {
                        auto poDriver = poDM->GetDriver(i);
                        if (((datasetType & GDAL_OF_RASTER) != 0 &&
                             poDriver->GetMetadataItem(GDAL_DCAP_RASTER)) ||
                            ((datasetType & GDAL_OF_VECTOR) != 0 &&
                             poDriver->GetMetadataItem(GDAL_DCAP_VECTOR)) ||
                            ((datasetType & GDAL_OF_MULTIDIM_RASTER) != 0 &&
                             poDriver->GetMetadataItem(
                                 GDAL_DCAP_MULTIDIM_RASTER)))
                        {
                            const char *pszExtensions =
                                poDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS);
                            if (pszExtensions)
                            {
                                const CPLStringList aosExts(
                                    CSLTokenizeString2(pszExtensions, " ", 0));
                                for (const char *pszExt : cpl::Iterate(aosExts))
                                {
                                    if (EQUAL(pszExt, osExt.c_str()) &&
                                        !cpl::contains(oVisitedExtensions,
                                                       pszExt))
                                    {
                                        oVisitedExtensions.insert(pszExt);
                                        if (AddOptionsSuggestions(
                                                poDriver->GetMetadataItem(
                                                    GDAL_DMD_CREATIONOPTIONLIST),
                                                datasetType, currentValue,
                                                oRet))
                                        {
                                            return oRet;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return oRet;
        });

    return arg;
}

/************************************************************************/
/*                GDALAlgorithm::AddLayerCreationOptionsArg()           */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddLayerCreationOptionsArg(std::vector<std::string> *pValue,
                                          const char *helpMessage)
{
    auto &arg =
        AddArg("layer-creation-option", 0,
               MsgOrDefault(helpMessage, _("Layer creation option")), pValue)
            .AddAlias("lco")
            .SetMetaVar("<KEY>=<VALUE>")
            .SetPackedValuesAllowed(false);
    arg.AddValidationAction([this, &arg]()
                            { return ParseAndValidateKeyValue(arg); });

    arg.SetAutoCompleteFunction(
        [this](const std::string &currentValue)
        {
            std::vector<std::string> oRet;

            auto outputFormat = GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT);
            if (outputFormat && outputFormat->GetType() == GAAT_STRING &&
                outputFormat->IsExplicitlySet())
            {
                auto poDriver = GetGDALDriverManager()->GetDriverByName(
                    outputFormat->Get<std::string>().c_str());
                if (poDriver)
                {
                    AddOptionsSuggestions(poDriver->GetMetadataItem(
                                              GDAL_DS_LAYER_CREATIONOPTIONLIST),
                                          GDAL_OF_VECTOR, currentValue, oRet);
                }
                return oRet;
            }

            auto outputArg = GetArg(GDAL_ARG_NAME_OUTPUT);
            if (outputArg && outputArg->GetType() == GAAT_DATASET)
            {
                auto poDM = GetGDALDriverManager();
                auto &datasetValue = outputArg->Get<GDALArgDatasetValue>();
                const auto &osDSName = datasetValue.GetName();
                const std::string osExt = CPLGetExtensionSafe(osDSName.c_str());
                if (!osExt.empty())
                {
                    std::set<std::string> oVisitedExtensions;
                    for (int i = 0; i < poDM->GetDriverCount(); ++i)
                    {
                        auto poDriver = poDM->GetDriver(i);
                        if (poDriver->GetMetadataItem(GDAL_DCAP_VECTOR))
                        {
                            const char *pszExtensions =
                                poDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS);
                            if (pszExtensions)
                            {
                                const CPLStringList aosExts(
                                    CSLTokenizeString2(pszExtensions, " ", 0));
                                for (const char *pszExt : cpl::Iterate(aosExts))
                                {
                                    if (EQUAL(pszExt, osExt.c_str()) &&
                                        !cpl::contains(oVisitedExtensions,
                                                       pszExt))
                                    {
                                        oVisitedExtensions.insert(pszExt);
                                        if (AddOptionsSuggestions(
                                                poDriver->GetMetadataItem(
                                                    GDAL_DS_LAYER_CREATIONOPTIONLIST),
                                                GDAL_OF_VECTOR, currentValue,
                                                oRet))
                                        {
                                            return oRet;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return oRet;
        });

    return arg;
}

/************************************************************************/
/*                        GDALAlgorithm::AddBBOXArg()                   */
/************************************************************************/

/** Add bbox=xmin,ymin,xmax,ymax argument. */
GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddBBOXArg(std::vector<double> *pValue, const char *helpMessage)
{
    auto &arg = AddArg("bbox", 0,
                       MsgOrDefault(helpMessage,
                                    _("Bounding box as xmin,ymin,xmax,ymax")),
                       pValue)
                    .SetRepeatedArgAllowed(false)
                    .SetMinCount(4)
                    .SetMaxCount(4)
                    .SetDisplayHintAboutRepetition(false);
    arg.AddValidationAction(
        [&arg]()
        {
            const auto &val = arg.Get<std::vector<double>>();
            CPLAssert(val.size() == 4);
            if (!(val[0] <= val[2]) || !(val[1] <= val[3]))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Value of 'bbox' should be xmin,ymin,xmax,ymax with "
                         "xmin <= xmax and ymin <= ymax");
                return false;
            }
            return true;
        });
    return arg;
}

/************************************************************************/
/*                  GDALAlgorithm::AddActiveLayerArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddActiveLayerArg(std::string *pValue, const char *helpMessage)
{
    return AddArg("active-layer", 0,
                  MsgOrDefault(helpMessage,
                               _("Set active layer (if not specified, all)")),
                  pValue);
}

/************************************************************************/
/*                  GDALAlgorithm::AddNumThreadsArg()                   */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddNumThreadsArg(int *pValue, std::string *pStrValue,
                                const char *helpMessage)
{
    auto &arg =
        AddArg("num-threads", 'j',
               MsgOrDefault(helpMessage, _("Number of jobs (or ALL_CPUS)")),
               pStrValue);
    auto lambda = [this, &arg, pValue, pStrValue]
    {
        int nNumCPUs = std::max(1, CPLGetNumCPUs());
        const char *pszThreads =
            CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
        if (pszThreads && !EQUAL(pszThreads, "ALL_CPUS"))
        {
            nNumCPUs = std::clamp(atoi(pszThreads), 1, nNumCPUs);
        }
        if (EQUAL(pStrValue->c_str(), "ALL_CPUS"))
        {
            *pValue = nNumCPUs;
            return true;
        }
        else
        {
            char *endptr = nullptr;
            const auto res = std::strtol(pStrValue->c_str(), &endptr, 10);
            if (endptr == pStrValue->c_str() + pStrValue->size() && res >= 0 &&
                res <= INT_MAX)
            {
                *pValue = std::min(static_cast<int>(res), nNumCPUs);
                return true;
            }
            ReportError(CE_Failure, CPLE_IllegalArg,
                        "Invalid value for '%s' argument",
                        arg.GetName().c_str());
            return false;
        }
    };
    if (!pStrValue->empty())
    {
        arg.SetDefault(*pStrValue);
        lambda();
    }
    arg.AddValidationAction(std::move(lambda));
    return arg;
}

/************************************************************************/
/*                  GDALAlgorithm::AddProgressArg()                     */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALAlgorithm::AddProgressArg()
{
    return AddArg("progress", 0, _("Display progress bar"),
                  &m_progressBarRequested)
        .SetOnlyForCLI()
        .SetCategory(GAAC_COMMON);
}

/************************************************************************/
/*                    ProgressWithErrorIfFailed                         */
/************************************************************************/

namespace
{
struct ProgressWithErrorIfFailed
{
    GDALProgressFunc pfnProgress = nullptr;
    void *pProgressData = nullptr;

    static int CPL_STDCALL ProgressFunc(double dfPct, const char *pszMsg,
                                        void *userData)
    {
        ProgressWithErrorIfFailed *self =
            static_cast<ProgressWithErrorIfFailed *>(userData);
        if (!self->pfnProgress(dfPct, pszMsg, self->pProgressData))
        {
            if (CPLGetLastErrorType() != CE_Failure)
            {
                CPLError(CE_Failure, CPLE_UserInterrupt,
                         "Processing interrupted by user");
            }
            return false;
        }
        return true;
    }
};
}  // namespace

/************************************************************************/
/*                       GDALAlgorithm::Run()                           */
/************************************************************************/

bool GDALAlgorithm::Run(GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (m_selectedSubAlg)
    {
        if (m_calledFromCommandLine)
            m_selectedSubAlg->m_calledFromCommandLine = true;
        return m_selectedSubAlg->Run(pfnProgress, pProgressData);
    }

    if (m_helpRequested || m_helpDocRequested)
    {
        if (m_calledFromCommandLine)
            printf("%s", GetUsageForCLI(false).c_str()); /*ok*/
        return true;
    }

    if (m_JSONUsageRequested)
    {
        if (m_calledFromCommandLine)
            printf("%s", GetUsageAsJSON().c_str()); /*ok*/
        return true;
    }

    if (!ValidateArguments())
        return false;

    if (!m_dummyConfigOptions.empty())
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                    "Configuration options passed with the 'config' argument "
                    "are ignored");
    }

    switch (ProcessGDALGOutput())
    {
        case ProcessGDALGOutputRet::GDALG_ERROR:
            return false;

        case ProcessGDALGOutputRet::GDALG_OK:
            return true;

        case ProcessGDALGOutputRet::NOT_GDALG:
            break;
    }

    if (m_executionForStreamOutput)
    {
        if (!CheckSafeForStreamOutput())
        {
            return false;
        }
    }

    if (pfnProgress)
    {
        ProgressWithErrorIfFailed sProgressWithErrorIfFailed;
        sProgressWithErrorIfFailed.pfnProgress = pfnProgress;
        sProgressWithErrorIfFailed.pProgressData = pProgressData;
        return RunImpl(ProgressWithErrorIfFailed::ProgressFunc,
                       &sProgressWithErrorIfFailed);
    }
    else
    {
        return RunImpl(nullptr, nullptr);
    }
}

/************************************************************************/
/*              GDALAlgorithm::CheckSafeForStreamOutput()               */
/************************************************************************/

bool GDALAlgorithm::CheckSafeForStreamOutput()
{
    const auto outputFormatArg = GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT);
    if (outputFormatArg && outputFormatArg->GetType() == GAAT_STRING)
    {
        const auto &val = outputFormatArg->GDALAlgorithmArg::Get<std::string>();
        if (!EQUAL(val.c_str(), "stream"))
        {
            // For security reasons, to avoid that reading a .gdalg.json file
            // writes a file on the file system.
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "in streamed execution, --format stream should be used");
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                     GDALAlgorithm::Finalize()                        */
/************************************************************************/

bool GDALAlgorithm::Finalize()
{
    bool ret = true;
    if (m_selectedSubAlg)
        ret = m_selectedSubAlg->Finalize();

    for (auto &arg : m_args)
    {
        if (arg->GetType() == GAAT_DATASET)
        {
            ret = arg->Get<GDALArgDatasetValue>().Close() && ret;
        }
        else if (arg->GetType() == GAAT_DATASET_LIST)
        {
            for (auto &ds : arg->Get<std::vector<GDALArgDatasetValue>>())
            {
                ret = ds.Close() && ret;
            }
        }
    }
    return ret;
}

/************************************************************************/
/*                   GDALAlgorithm::GetArgNamesForCLI()                 */
/************************************************************************/

std::pair<std::vector<std::pair<GDALAlgorithmArg *, std::string>>, size_t>
GDALAlgorithm::GetArgNamesForCLI() const
{
    std::vector<std::pair<GDALAlgorithmArg *, std::string>> options;

    size_t maxOptLen = 0;
    for (const auto &arg : m_args)
    {
        if (arg->IsHidden() || arg->IsHiddenForCLI())
            continue;
        std::string opt;
        bool addComma = false;
        if (!arg->GetShortName().empty())
        {
            opt += '-';
            opt += arg->GetShortName();
            addComma = true;
        }
        for (char alias : arg->GetShortNameAliases())
        {
            if (addComma)
                opt += ", ";
            opt += "-";
            opt += alias;
            addComma = true;
        }
        for (const std::string &alias : arg->GetAliases())
        {
            if (addComma)
                opt += ", ";
            opt += "--";
            opt += alias;
            addComma = true;
        }
        if (!arg->GetName().empty())
        {
            if (addComma)
                opt += ", ";
            opt += "--";
            opt += arg->GetName();
        }
        const auto &metaVar = arg->GetMetaVar();
        if (!metaVar.empty())
        {
            opt += ' ';
            if (metaVar.front() != '<')
                opt += '<';
            opt += metaVar;
            if (metaVar.back() != '>')
                opt += '>';
        }
        maxOptLen = std::max(maxOptLen, opt.size());
        options.emplace_back(arg.get(), opt);
    }

    return std::make_pair(std::move(options), maxOptLen);
}

/************************************************************************/
/*                    GDALAlgorithm::GetUsageForCLI()                   */
/************************************************************************/

std::string
GDALAlgorithm::GetUsageForCLI(bool shortUsage,
                              const UsageOptions &usageOptions) const
{
    if (m_selectedSubAlg)
        return m_selectedSubAlg->GetUsageForCLI(shortUsage, usageOptions);

    std::string osRet(usageOptions.isPipelineStep ? "*" : "Usage:");
    std::string osPath;
    for (const std::string &s : m_callPath)
    {
        if (!osPath.empty())
            osPath += ' ';
        osPath += s;
    }
    osRet += ' ';
    osRet += osPath;

    bool hasNonPositionals = false;
    for (const auto &arg : m_args)
    {
        if (!arg->IsHidden() && !arg->IsHiddenForCLI() && !arg->IsPositional())
            hasNonPositionals = true;
    }

    if (HasSubAlgorithms())
    {
        if (m_callPath.size() == 1)
        {
            osRet += " <COMMAND>";
            if (hasNonPositionals)
                osRet += " [OPTIONS]";
            osRet += "\nwhere <COMMAND> is one of:\n";
        }
        else
        {
            osRet += " <SUBCOMMAND>";
            if (hasNonPositionals)
                osRet += " [OPTIONS]";
            osRet += "\nwhere <SUBCOMMAND> is one of:\n";
        }
        size_t maxNameLen = 0;
        for (const auto &subAlgName : GetSubAlgorithmNames())
        {
            maxNameLen = std::max(maxNameLen, subAlgName.size());
        }
        for (const auto &subAlgName : GetSubAlgorithmNames())
        {
            auto subAlg = InstantiateSubAlgorithm(subAlgName);
            if (subAlg)
            {
                const std::string &name(subAlg->GetName());
                osRet += "  - ";
                osRet += name;
                osRet += ": ";
                osRet.append(maxNameLen - name.size(), ' ');
                osRet += subAlg->GetDescription();
                if (!subAlg->m_aliases.empty())
                {
                    bool first = true;
                    for (const auto &alias : subAlg->GetAliases())
                    {
                        if (alias ==
                            GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR)
                            break;
                        if (first)
                            osRet += " (alias: ";
                        else
                            osRet += ", ";
                        osRet += alias;
                        first = false;
                    }
                    if (!first)
                    {
                        osRet += ')';
                    }
                }
            }
            osRet += '\n';
        }

        if (shortUsage && hasNonPositionals)
        {
            osRet += "\nTry '";
            osRet += osPath;
            osRet += " --help' for help.\n";
        }
    }
    else
    {
        if (!m_args.empty())
        {
            if (hasNonPositionals)
                osRet += " [OPTIONS]";
            for (const auto *arg : m_positionalArgs)
            {
                const bool optional =
                    (!arg->IsRequired() && !(GetName() == "pipeline" &&
                                             arg->GetName() == "pipeline"));
                osRet += ' ';
                if (optional)
                    osRet += '[';
                const std::string &metavar = arg->GetMetaVar();
                if (!metavar.empty() && metavar[0] == '<')
                {
                    osRet += metavar;
                }
                else
                {
                    osRet += '<';
                    osRet += metavar;
                    osRet += '>';
                }
                if (arg->GetType() == GAAT_DATASET_LIST &&
                    arg->GetMaxCount() > 1)
                {
                    osRet += "...";
                }
                if (optional)
                    osRet += ']';
            }
        }

        const size_t nLenFirstLine = osRet.size();
        osRet += '\n';
        if (usageOptions.isPipelineStep)
        {
            osRet.append(nLenFirstLine, '-');
            osRet += '\n';
        }

        if (shortUsage)
        {
            osRet += "Try '";
            osRet += osPath;
            osRet += " --help' for help.\n";
            return osRet;
        }

        osRet += '\n';
        osRet += m_description;
        osRet += '\n';
    }

    if (!m_args.empty() && !shortUsage)
    {
        std::vector<std::pair<GDALAlgorithmArg *, std::string>> options;
        size_t maxOptLen;
        std::tie(options, maxOptLen) = GetArgNamesForCLI();
        if (usageOptions.maxOptLen)
            maxOptLen = usageOptions.maxOptLen;

        const auto OutputArg =
            [this, maxOptLen, &osRet](const GDALAlgorithmArg *arg,
                                      const std::string &opt)
        {
            osRet += "  ";
            osRet += opt;
            osRet += "  ";
            osRet.append(maxOptLen - opt.size(), ' ');
            osRet += arg->GetDescription();

            const auto &choices = arg->GetChoices();
            if (!choices.empty())
            {
                osRet += ". ";
                osRet += arg->GetMetaVar();
                osRet += '=';
                bool firstChoice = true;
                for (const auto &choice : choices)
                {
                    if (!firstChoice)
                        osRet += '|';
                    osRet += choice;
                    firstChoice = false;
                }
            }

            if (arg->GetType() == GAAT_DATASET ||
                arg->GetType() == GAAT_DATASET_LIST)
            {
                if (arg->GetDatasetInputFlags() == GADV_NAME &&
                    arg->GetDatasetOutputFlags() == GADV_OBJECT)
                {
                    osRet += " (created by algorithm)";
                }
            }

            if (arg->GetType() == GAAT_STRING && arg->HasDefaultValue())
            {
                osRet += " (default: ";
                osRet += arg->GetDefault<std::string>();
                osRet += ')';
            }
            else if (arg->GetType() == GAAT_BOOLEAN && arg->HasDefaultValue())
            {
                if (arg->GetDefault<bool>())
                    osRet += " (default: true)";
            }
            else if (arg->GetType() == GAAT_INTEGER && arg->HasDefaultValue())
            {
                osRet += " (default: ";
                osRet += CPLSPrintf("%d", arg->GetDefault<int>());
                osRet += ')';
            }
            else if (arg->GetType() == GAAT_REAL && arg->HasDefaultValue())
            {
                osRet += " (default: ";
                osRet += CPLSPrintf("%g", arg->GetDefault<double>());
                osRet += ')';
            }
            else if (arg->GetType() == GAAT_STRING_LIST &&
                     arg->HasDefaultValue())
            {
                const auto &defaultVal =
                    arg->GetDefault<std::vector<std::string>>();
                if (defaultVal.size() == 1)
                {
                    osRet += " (default: ";
                    osRet += defaultVal[0];
                    osRet += ')';
                }
            }
            else if (arg->GetType() == GAAT_INTEGER_LIST &&
                     arg->HasDefaultValue())
            {
                const auto &defaultVal = arg->GetDefault<std::vector<int>>();
                if (defaultVal.size() == 1)
                {
                    osRet += " (default: ";
                    osRet += CPLSPrintf("%d", defaultVal[0]);
                    osRet += ')';
                }
            }
            else if (arg->GetType() == GAAT_REAL_LIST && arg->HasDefaultValue())
            {
                const auto &defaultVal = arg->GetDefault<std::vector<double>>();
                if (defaultVal.size() == 1)
                {
                    osRet += " (default: ";
                    osRet += CPLSPrintf("%g", defaultVal[0]);
                    osRet += ')';
                }
            }

            if (arg->GetDisplayHintAboutRepetition())
            {
                if (arg->GetMinCount() > 0 &&
                    arg->GetMinCount() == arg->GetMaxCount())
                {
                    if (arg->GetMinCount() != 1)
                        osRet += CPLSPrintf(" [%d values]", arg->GetMaxCount());
                }
                else if (arg->GetMinCount() > 0 &&
                         arg->GetMaxCount() < GDALAlgorithmArgDecl::UNBOUNDED)
                {
                    osRet += CPLSPrintf(" [%d..%d values]", arg->GetMinCount(),
                                        arg->GetMaxCount());
                }
                else if (arg->GetMinCount() > 0)
                {
                    osRet += CPLSPrintf(" [%d.. values]", arg->GetMinCount());
                }
                else if (arg->GetMaxCount() > 1)
                {
                    osRet += " [may be repeated]";
                }
            }

            if (arg->IsRequired())
            {
                osRet += " [required]";
            }

            osRet += '\n';

            const auto &mutualExclusionGroup = arg->GetMutualExclusionGroup();
            if (!mutualExclusionGroup.empty())
            {
                std::string otherArgs;
                for (const auto &otherArg : m_args)
                {
                    if (otherArg->IsHidden() || otherArg->IsHiddenForCLI() ||
                        otherArg.get() == arg)
                        continue;
                    if (otherArg->GetMutualExclusionGroup() ==
                        mutualExclusionGroup)
                    {
                        if (!otherArgs.empty())
                            otherArgs += ", ";
                        otherArgs += "--";
                        otherArgs += otherArg->GetName();
                    }
                }
                if (!otherArgs.empty())
                {
                    osRet += "  ";
                    osRet += "  ";
                    osRet.append(maxOptLen, ' ');
                    osRet += "Mutually exclusive with ";
                    osRet += otherArgs;
                    osRet += '\n';
                }
            }
        };

        if (!m_positionalArgs.empty())
        {
            osRet += "\nPositional arguments:\n";
            for (const auto &[arg, opt] : options)
            {
                if (arg->IsPositional())
                    OutputArg(arg, opt);
            }
        }

        if (hasNonPositionals)
        {
            bool hasCommon = false;
            bool hasBase = false;
            bool hasAdvanced = false;
            bool hasEsoteric = false;
            std::vector<std::string> categories;
            for (const auto &iter : options)
            {
                const auto &arg = iter.first;
                if (!arg->IsPositional())
                {
                    const auto &category = arg->GetCategory();
                    if (category == GAAC_COMMON)
                    {
                        hasCommon = true;
                    }
                    else if (category == GAAC_BASE)
                    {
                        hasBase = true;
                    }
                    else if (category == GAAC_ADVANCED)
                    {
                        hasAdvanced = true;
                    }
                    else if (category == GAAC_ESOTERIC)
                    {
                        hasEsoteric = true;
                    }
                    else if (std::find(categories.begin(), categories.end(),
                                       category) == categories.end())
                    {
                        categories.push_back(category);
                    }
                }
            }
            if (hasAdvanced)
                categories.insert(categories.begin(), GAAC_ADVANCED);
            if (hasBase)
                categories.insert(categories.begin(), GAAC_BASE);
            if (hasCommon && !usageOptions.isPipelineStep)
                categories.insert(categories.begin(), GAAC_COMMON);
            if (hasEsoteric)
                categories.push_back(GAAC_ESOTERIC);

            for (const auto &category : categories)
            {
                osRet += "\n";
                if (category != GAAC_BASE)
                {
                    osRet += category;
                    osRet += ' ';
                }
                osRet += "Options:\n";
                for (const auto &[arg, opt] : options)
                {
                    if (!arg->IsPositional() && arg->GetCategory() == category)
                        OutputArg(arg, opt);
                }
            }
        }
    }

    if (!m_longDescription.empty())
    {
        osRet += '\n';
        osRet += m_longDescription;
        osRet += '\n';
    }

    if (!m_helpDocRequested && !usageOptions.isPipelineMain)
    {
        if (!m_helpURL.empty())
        {
            osRet += "\nFor more details, consult ";
            osRet += GetHelpFullURL();
            osRet += '\n';
        }
        osRet += GetUsageForCLIEnd();
    }

    return osRet;
}

/************************************************************************/
/*                   GDALAlgorithm::GetUsageForCLIEnd()                 */
/************************************************************************/

//! @cond Doxygen_Suppress
std::string GDALAlgorithm::GetUsageForCLIEnd() const
{
    std::string osRet;

    if (!m_callPath.empty() && m_callPath[0] == "gdal")
    {
        osRet += "\nWARNING: the gdal command is provisionally provided as an "
                 "alternative interface to GDAL and OGR command line "
                 "utilities.\nThe project reserves the right to modify, "
                 "rename, reorganize, and change the behavior of the utility\n"
                 "until it is officially frozen in a future feature release of "
                 "GDAL.\n";
    }
    return osRet;
}

//! @endcond

/************************************************************************/
/*                    GDALAlgorithm::GetUsageAsJSON()                   */
/************************************************************************/

std::string GDALAlgorithm::GetUsageAsJSON() const
{
    CPLJSONDocument oDoc;
    auto oRoot = oDoc.GetRoot();

    if (m_displayInJSONUsage)
    {
        oRoot.Add("name", m_name);
        CPLJSONArray jFullPath;
        for (const std::string &s : m_callPath)
        {
            jFullPath.Add(s);
        }
        oRoot.Add("full_path", jFullPath);
    }

    oRoot.Add("description", m_description);
    if (!m_helpURL.empty())
    {
        oRoot.Add("short_url", m_helpURL);
        oRoot.Add("url", GetHelpFullURL());
    }

    CPLJSONArray jSubAlgorithms;
    for (const auto &subAlgName : GetSubAlgorithmNames())
    {
        auto subAlg = InstantiateSubAlgorithm(subAlgName);
        if (subAlg && subAlg->m_displayInJSONUsage)
        {
            CPLJSONDocument oSubDoc;
            CPL_IGNORE_RET_VAL(oSubDoc.LoadMemory(subAlg->GetUsageAsJSON()));
            jSubAlgorithms.Add(oSubDoc.GetRoot());
        }
    }
    oRoot.Add("sub_algorithms", jSubAlgorithms);

    const auto ProcessArg = [](const GDALAlgorithmArg *arg)
    {
        CPLJSONObject jArg;
        jArg.Add("name", arg->GetName());
        jArg.Add("type", GDALAlgorithmArgTypeName(arg->GetType()));
        jArg.Add("description", arg->GetDescription());

        const auto &metaVar = arg->GetMetaVar();
        if (!metaVar.empty() && metaVar != CPLString(arg->GetName()).toupper())
        {
            if (metaVar.front() == '<' && metaVar.back() == '>' &&
                metaVar.substr(1, metaVar.size() - 2).find('>') ==
                    std::string::npos)
                jArg.Add("metavar", metaVar.substr(1, metaVar.size() - 2));
            else
                jArg.Add("metavar", metaVar);
        }

        const auto &choices = arg->GetChoices();
        if (!choices.empty())
        {
            CPLJSONArray jChoices;
            for (const auto &choice : choices)
                jChoices.Add(choice);
            jArg.Add("choices", jChoices);
        }
        if (arg->HasDefaultValue())
        {
            switch (arg->GetType())
            {
                case GAAT_BOOLEAN:
                    jArg.Add("default", arg->GetDefault<bool>());
                    break;
                case GAAT_STRING:
                    jArg.Add("default", arg->GetDefault<std::string>());
                    break;
                case GAAT_INTEGER:
                    jArg.Add("default", arg->GetDefault<int>());
                    break;
                case GAAT_REAL:
                    jArg.Add("default", arg->GetDefault<double>());
                    break;
                case GAAT_STRING_LIST:
                {
                    const auto &val =
                        arg->GetDefault<std::vector<std::string>>();
                    if (val.size() == 1)
                    {
                        jArg.Add("default", val[0]);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unhandled default value for arg %s",
                                 arg->GetName().c_str());
                    }
                    break;
                }
                case GAAT_INTEGER_LIST:
                {
                    const auto &val = arg->GetDefault<std::vector<int>>();
                    if (val.size() == 1)
                    {
                        jArg.Add("default", val[0]);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unhandled default value for arg %s",
                                 arg->GetName().c_str());
                    }
                    break;
                }
                case GAAT_REAL_LIST:
                {
                    const auto &val = arg->GetDefault<std::vector<double>>();
                    if (val.size() == 1)
                    {
                        jArg.Add("default", val[0]);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unhandled default value for arg %s",
                                 arg->GetName().c_str());
                    }
                    break;
                }
                case GAAT_DATASET:
                case GAAT_DATASET_LIST:
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unhandled default value for arg %s",
                             arg->GetName().c_str());
                    break;
            }
        }

        const auto [minVal, minValIsIncluded] = arg->GetMinValue();
        if (!std::isnan(minVal))
        {
            if (arg->GetType() == GAAT_INTEGER ||
                arg->GetType() == GAAT_INTEGER_LIST)
                jArg.Add("min_value", static_cast<int>(minVal));
            else
                jArg.Add("min_value", minVal);
            jArg.Add("min_value_is_included", minValIsIncluded);
        }

        const auto [maxVal, maxValIsIncluded] = arg->GetMaxValue();
        if (!std::isnan(maxVal))
        {
            if (arg->GetType() == GAAT_INTEGER ||
                arg->GetType() == GAAT_INTEGER_LIST)
                jArg.Add("max_value", static_cast<int>(maxVal));
            else
                jArg.Add("max_value", maxVal);
            jArg.Add("max_value_is_included", maxValIsIncluded);
        }

        jArg.Add("required", arg->IsRequired());
        if (GDALAlgorithmArgTypeIsList(arg->GetType()))
        {
            jArg.Add("packed_values_allowed", arg->GetPackedValuesAllowed());
            jArg.Add("repeated_arg_allowed", arg->GetRepeatedArgAllowed());
            jArg.Add("min_count", arg->GetMinCount());
            jArg.Add("max_count", arg->GetMaxCount());
        }
        jArg.Add("category", arg->GetCategory());

        if (arg->GetType() == GAAT_DATASET ||
            arg->GetType() == GAAT_DATASET_LIST)
        {
            {
                CPLJSONArray jAr;
                if (arg->GetDatasetType() & GDAL_OF_RASTER)
                    jAr.Add("raster");
                if (arg->GetDatasetType() & GDAL_OF_VECTOR)
                    jAr.Add("vector");
                if (arg->GetDatasetType() & GDAL_OF_MULTIDIM_RASTER)
                    jAr.Add("multidim_raster");
                jArg.Add("dataset_type", jAr);
            }

            const auto GetFlags = [](int flags)
            {
                CPLJSONArray jAr;
                if (flags & GADV_NAME)
                    jAr.Add("name");
                if (flags & GADV_OBJECT)
                    jAr.Add("dataset");
                return jAr;
            };

            if (arg->IsInput())
            {
                jArg.Add("input_flags", GetFlags(arg->GetDatasetInputFlags()));
            }
            if (arg->IsOutput())
            {
                jArg.Add("output_flags",
                         GetFlags(arg->GetDatasetOutputFlags()));
            }
        }

        const auto &mutualExclusionGroup = arg->GetMutualExclusionGroup();
        if (!mutualExclusionGroup.empty())
        {
            jArg.Add("mutual_exclusion_group", mutualExclusionGroup);
        }

        const auto &metadata = arg->GetMetadata();
        if (!metadata.empty())
        {
            CPLJSONObject jMetadata;
            for (const auto &[key, values] : metadata)
            {
                CPLJSONArray jValue;
                for (const auto &value : values)
                    jValue.Add(value);
                jMetadata.Add(key, jValue);
            }
            jArg.Add("metadata", jMetadata);
        }

        return jArg;
    };

    {
        CPLJSONArray jArgs;
        for (const auto &arg : m_args)
        {
            if (!arg->IsHidden() && !arg->IsOnlyForCLI() && arg->IsInput() &&
                !arg->IsOutput())
                jArgs.Add(ProcessArg(arg.get()));
        }
        oRoot.Add("input_arguments", jArgs);
    }

    {
        CPLJSONArray jArgs;
        for (const auto &arg : m_args)
        {
            if (!arg->IsHidden() && !arg->IsOnlyForCLI() && !arg->IsInput() &&
                arg->IsOutput())
                jArgs.Add(ProcessArg(arg.get()));
        }
        oRoot.Add("output_arguments", jArgs);
    }

    {
        CPLJSONArray jArgs;
        for (const auto &arg : m_args)
        {
            if (!arg->IsHidden() && !arg->IsOnlyForCLI() && arg->IsInput() &&
                arg->IsOutput())
                jArgs.Add(ProcessArg(arg.get()));
        }
        oRoot.Add("input_output_arguments", jArgs);
    }

    if (m_supportsStreamedOutput)
    {
        oRoot.Add("supports_streamed_output", true);
    }

    return oDoc.SaveAsString();
}

/************************************************************************/
/*                    GDALAlgorithm::GetAutoComplete()                  */
/************************************************************************/

std::vector<std::string>
GDALAlgorithm::GetAutoComplete(std::vector<std::string> &args,
                               bool lastWordIsComplete, bool showAllOptions)
{
    // Get inner-most algorithm
    std::unique_ptr<GDALAlgorithm> curAlgHolder;
    GDALAlgorithm *curAlg = this;
    while (!args.empty() && !args.front().empty() && args.front()[0] != '-')
    {
        auto subAlg = curAlg->InstantiateSubAlgorithm(
            args.front(), /* suggestionAllowed = */ false);
        if (!subAlg)
            break;
        if (args.size() == 1 && !lastWordIsComplete)
        {
            int nCount = 0;
            for (const auto &subAlgName : curAlg->GetSubAlgorithmNames())
            {
                if (STARTS_WITH(subAlgName.c_str(), args.front().c_str()))
                    nCount++;
            }
            if (nCount >= 2)
            {
                return curAlg->GetSubAlgorithmNames();
            }
        }
        showAllOptions = false;
        args.erase(args.begin());
        curAlgHolder = std::move(subAlg);
        curAlg = curAlgHolder.get();
    }
    if (curAlg != this)
    {
        return curAlg->GetAutoComplete(args, lastWordIsComplete,
                                       /* showAllOptions = */ false);
    }

    std::vector<std::string> ret;

    std::string option;
    std::string value;
    ExtractLastOptionAndValue(args, option, value);

    if (option.empty() && !args.empty() && !args.back().empty() &&
        args.back()[0] == '-')
    {
        // List available options
        for (const auto &arg : GetArgs())
        {
            if (arg->IsHidden() || arg->IsHiddenForCLI() ||
                (!showAllOptions &&
                 (arg->GetName() == "help" || arg->GetName() == "config" ||
                  arg->GetName() == "version" ||
                  arg->GetName() == "json-usage")))
            {
                continue;
            }
            if (!arg->GetShortName().empty())
            {
                ret.push_back(std::string("-").append(arg->GetShortName()));
            }
            for (const std::string &alias : arg->GetAliases())
            {
                ret.push_back(std::string("--").append(alias));
            }
            if (!arg->GetName().empty())
            {
                ret.push_back(std::string("--").append(arg->GetName()));
            }
        }
    }
    else if (!option.empty())
    {
        // List possible choices for current option
        auto arg = GetArg(option);
        if (arg && arg->GetType() != GAAT_BOOLEAN)
        {
            ret = arg->GetChoices();
            if (ret.empty())
            {
                {
                    CPLErrorStateBackuper oErrorQuieter(CPLQuietErrorHandler);
                    SetParseForAutoCompletion();
                    CPL_IGNORE_RET_VAL(ParseCommandLineArguments(args));
                }
                ret = arg->GetAutoCompleteChoices(value);
            }
            if (ret.empty())
            {
                ret.push_back("**");
                ret.push_back(
                    std::string("description: ").append(arg->GetDescription()));
            }
        }
    }
    else if (!args.empty() && STARTS_WITH(args.back().c_str(), "/vsi"))
    {
        auto arg = GetArg(GDAL_ARG_NAME_INPUT);
        for (const char *name :
             {"dataset", "filename", "like", "source", "destination"})
        {
            if (!arg)
            {
                arg = GetArg(name);
            }
        }
        if (arg)
        {
            ret = arg->GetAutoCompleteChoices(args.back());
        }
    }
    else
    {
        // List possible sub-algorithms
        ret = GetSubAlgorithmNames();
    }

    return ret;
}

/************************************************************************/
/*             GDALAlgorithm::ExtractLastOptionAndValue()               */
/************************************************************************/

void GDALAlgorithm::ExtractLastOptionAndValue(std::vector<std::string> &args,
                                              std::string &option,
                                              std::string &value) const
{
    if (!args.empty() && !args.back().empty() && args.back()[0] == '-')
    {
        const auto nPosEqual = args.back().find('=');
        if (nPosEqual == std::string::npos)
        {
            // Deal with "gdal ... --option"
            if (GetArg(args.back()))
            {
                option = args.back();
                args.pop_back();
            }
        }
        else
        {
            // Deal with "gdal ... --option=<value>"
            if (GetArg(args.back().substr(0, nPosEqual)))
            {
                option = args.back().substr(0, nPosEqual);
                value = args.back().substr(nPosEqual + 1);
                args.pop_back();
            }
        }
    }
    else if (args.size() >= 2 && !args[args.size() - 2].empty() &&
             args[args.size() - 2][0] == '-')
    {
        // Deal with "gdal ... --option <value>"
        auto arg = GetArg(args[args.size() - 2]);
        if (arg && arg->GetType() != GAAT_BOOLEAN)
        {
            option = args[args.size() - 2];
            value = args.back();
            args.pop_back();
        }
    }

    const auto IsKeyValueOption = [](const std::string &osStr)
    {
        return osStr == "--co" || osStr == "--creation-option" ||
               osStr == "--lco" || osStr == "--layer-creation-option" ||
               osStr == "--oo" || osStr == "--open-option";
    };

    if (IsKeyValueOption(option))
    {
        const auto nPosEqual = value.find('=');
        if (nPosEqual != std::string::npos)
        {
            value.resize(nPosEqual);
        }
    }
}

/************************************************************************/
/*                        GDALAlgorithmRelease()                        */
/************************************************************************/

/** Release a handle to an algorithm.
 *
 * @since 3.11
 */
void GDALAlgorithmRelease(GDALAlgorithmH hAlg)
{
    delete hAlg;
}

/************************************************************************/
/*                        GDALAlgorithmGetName()                        */
/************************************************************************/

/** Return the algorithm name.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @return algorithm name whose lifetime is bound to hAlg and which must not
 * be freed.
 * @since 3.11
 */
const char *GDALAlgorithmGetName(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    return hAlg->ptr->GetName().c_str();
}

/************************************************************************/
/*                     GDALAlgorithmGetDescription()                    */
/************************************************************************/

/** Return the algorithm (short) description.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @return algorithm description whose lifetime is bound to hAlg and which must
 * not be freed.
 * @since 3.11
 */
const char *GDALAlgorithmGetDescription(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    return hAlg->ptr->GetDescription().c_str();
}

/************************************************************************/
/*                     GDALAlgorithmGetLongDescription()                */
/************************************************************************/

/** Return the algorithm (longer) description.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @return algorithm description whose lifetime is bound to hAlg and which must
 * not be freed.
 * @since 3.11
 */
const char *GDALAlgorithmGetLongDescription(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    return hAlg->ptr->GetLongDescription().c_str();
}

/************************************************************************/
/*                     GDALAlgorithmGetHelpFullURL()                    */
/************************************************************************/

/** Return the algorithm full URL.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @return algorithm URL whose lifetime is bound to hAlg and which must
 * not be freed.
 * @since 3.11
 */
const char *GDALAlgorithmGetHelpFullURL(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    return hAlg->ptr->GetHelpFullURL().c_str();
}

/************************************************************************/
/*                     GDALAlgorithmHasSubAlgorithms()                  */
/************************************************************************/

/** Return whether the algorithm has sub-algorithms.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmHasSubAlgorithms(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, false);
    return hAlg->ptr->HasSubAlgorithms();
}

/************************************************************************/
/*                 GDALAlgorithmGetSubAlgorithmNames()                  */
/************************************************************************/

/** Get the names of registered algorithms.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @return a NULL terminated list of names, which must be destroyed with
 * CSLDestroy()
 * @since 3.11
 */
char **GDALAlgorithmGetSubAlgorithmNames(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    return CPLStringList(hAlg->ptr->GetSubAlgorithmNames()).StealList();
}

/************************************************************************/
/*                GDALAlgorithmInstantiateSubAlgorithm()                */
/************************************************************************/

/** Instantiate an algorithm by its name (or its alias).
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @param pszSubAlgName Algorithm name. Must NOT be null.
 * @return an handle to the algorithm (to be freed with GDALAlgorithmRelease),
 * or NULL if the algorithm does not exist or another error occurred.
 * @since 3.11
 */
GDALAlgorithmH GDALAlgorithmInstantiateSubAlgorithm(GDALAlgorithmH hAlg,
                                                    const char *pszSubAlgName)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    VALIDATE_POINTER1(pszSubAlgName, __func__, nullptr);
    auto subAlg = hAlg->ptr->InstantiateSubAlgorithm(pszSubAlgName);
    return subAlg
               ? std::make_unique<GDALAlgorithmHS>(std::move(subAlg)).release()
               : nullptr;
}

/************************************************************************/
/*                GDALAlgorithmParseCommandLineArguments()              */
/************************************************************************/

/** Parse a command line argument, which does not include the algorithm
 * name, to set the value of corresponding arguments.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @param papszArgs NULL-terminated list of arguments, not including the algorithm name.
 * @return true if successful, false otherwise
 * @since 3.11
 */

bool GDALAlgorithmParseCommandLineArguments(GDALAlgorithmH hAlg,
                                            CSLConstList papszArgs)
{
    VALIDATE_POINTER1(hAlg, __func__, false);
    return hAlg->ptr->ParseCommandLineArguments(CPLStringList(papszArgs));
}

/************************************************************************/
/*                  GDALAlgorithmGetActualAlgorithm()                   */
/************************************************************************/

/** Return the actual algorithm that is going to be invoked, when the
 * current algorithm has sub-algorithms.
 *
 * Only valid after GDALAlgorithmParseCommandLineArguments() has been called.
 *
 * Note that the lifetime of the returned algorithm does not exceed the one of
 * the hAlg instance that owns it.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @return an handle to the algorithm (to be freed with GDALAlgorithmRelease).
 * @since 3.11
 */
GDALAlgorithmH GDALAlgorithmGetActualAlgorithm(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    return GDALAlgorithmHS::FromRef(hAlg->ptr->GetActualAlgorithm()).release();
}

/************************************************************************/
/*                          GDALAlgorithmRun()                          */
/************************************************************************/

/** Execute the algorithm, starting with ValidateArguments() and then
 * calling RunImpl().
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @param pfnProgress Progress callback. May be null.
 * @param pProgressData Progress callback user data. May be null.
 * @return true if successful, false otherwise
 * @since 3.11
 */

bool GDALAlgorithmRun(GDALAlgorithmH hAlg, GDALProgressFunc pfnProgress,
                      void *pProgressData)
{
    VALIDATE_POINTER1(hAlg, __func__, false);
    return hAlg->ptr->Run(pfnProgress, pProgressData);
}

/************************************************************************/
/*                       GDALAlgorithmFinalize()                        */
/************************************************************************/

/** Complete any pending actions, and return the final status.
 * This is typically useful for algorithm that generate an output dataset.
 *
 * Note that this function does *NOT* release memory associated with the
 * algorithm. GDALAlgorithmRelease() must still be called afterwards.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @return true if successful, false otherwise
 * @since 3.11
 */

bool GDALAlgorithmFinalize(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, false);
    return hAlg->ptr->Finalize();
}

/************************************************************************/
/*                    GDALAlgorithmGetUsageAsJSON()                     */
/************************************************************************/

/** Return the usage of the algorithm as a JSON-serialized string.
 *
 * This can be used to dynamically generate interfaces to algorithms.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @return a string that must be freed with CPLFree()
 * @since 3.11
 */
char *GDALAlgorithmGetUsageAsJSON(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    return CPLStrdup(hAlg->ptr->GetUsageAsJSON().c_str());
}

/************************************************************************/
/*                      GDALAlgorithmGetArgNames()                      */
/************************************************************************/

/** Return the list of available argument names.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @return a NULL terminated list of names, which must be destroyed with
 * CSLDestroy()
 * @since 3.11
 */
char **GDALAlgorithmGetArgNames(GDALAlgorithmH hAlg)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    CPLStringList list;
    for (const auto &arg : hAlg->ptr->GetArgs())
        list.AddString(arg->GetName().c_str());
    return list.StealList();
}

/************************************************************************/
/*                        GDALAlgorithmGetArg()                         */
/************************************************************************/

/** Return an argument from its name.
 *
 * The lifetime of the returned object does not exceed the one of hAlg.
 *
 * @param hAlg Handle to an algorithm. Must NOT be null.
 * @param pszArgName Argument name. Must NOT be null.
 * @return an argument that must be released with GDALAlgorithmArgRelease(),
 * or nullptr in case of error
 * @since 3.11
 */
GDALAlgorithmArgH GDALAlgorithmGetArg(GDALAlgorithmH hAlg,
                                      const char *pszArgName)
{
    VALIDATE_POINTER1(hAlg, __func__, nullptr);
    VALIDATE_POINTER1(pszArgName, __func__, nullptr);
    auto arg = hAlg->ptr->GetArg(pszArgName);
    if (!arg)
        return nullptr;
    return std::make_unique<GDALAlgorithmArgHS>(arg).release();
}

/************************************************************************/
/*                       GDALAlgorithmArgRelease()                      */
/************************************************************************/

/** Release a handle to an argument.
 *
 * @since 3.11
 */
void GDALAlgorithmArgRelease(GDALAlgorithmArgH hArg)
{
    delete hArg;
}

/************************************************************************/
/*                      GDALAlgorithmArgGetName()                       */
/************************************************************************/

/** Return the name of an argument.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return argument name whose lifetime is bound to hArg and which must not
 * be freed.
 * @since 3.11
 */
const char *GDALAlgorithmArgGetName(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    return hArg->ptr->GetName().c_str();
}

/************************************************************************/
/*                       GDALAlgorithmArgGetType()                      */
/************************************************************************/

/** Get the type of an argument
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
GDALAlgorithmArgType GDALAlgorithmArgGetType(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, GAAT_STRING);
    return hArg->ptr->GetType();
}

/************************************************************************/
/*                   GDALAlgorithmArgGetDescription()                   */
/************************************************************************/

/** Return the description of an argument.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return argument descriptioin whose lifetime is bound to hArg and which must not
 * be freed.
 * @since 3.11
 */
const char *GDALAlgorithmArgGetDescription(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    return hArg->ptr->GetDescription().c_str();
}

/************************************************************************/
/*                   GDALAlgorithmArgGetShortName()                     */
/************************************************************************/

/** Return the short name, or empty string if there is none
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return short name whose lifetime is bound to hArg and which must not
 * be freed.
 * @since 3.11
 */
const char *GDALAlgorithmArgGetShortName(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    return hArg->ptr->GetShortName().c_str();
}

/************************************************************************/
/*                    GDALAlgorithmArgGetAliases()                      */
/************************************************************************/

/** Return the aliases (potentially none)
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return a NULL terminated list of names, which must be destroyed with
 * CSLDestroy()

 * @since 3.11
 */
char **GDALAlgorithmArgGetAliases(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    return CPLStringList(hArg->ptr->GetAliases()).StealList();
}

/************************************************************************/
/*                    GDALAlgorithmArgGetMetaVar()                      */
/************************************************************************/

/** Return the "meta-var" hint.
 *
 * By default, the meta-var value is the long name of the argument in
 * upper case.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return meta-var hint whose lifetime is bound to hArg and which must not
 * be freed.
 * @since 3.11
 */
const char *GDALAlgorithmArgGetMetaVar(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    return hArg->ptr->GetMetaVar().c_str();
}

/************************************************************************/
/*                   GDALAlgorithmArgGetCategory()                      */
/************************************************************************/

/** Return the argument category
 *
 * GAAC_COMMON, GAAC_BASE, GAAC_ADVANCED, GAAC_ESOTERIC or a custom category.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return category whose lifetime is bound to hArg and which must not
 * be freed.
 * @since 3.11
 */
const char *GDALAlgorithmArgGetCategory(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    return hArg->ptr->GetCategory().c_str();
}

/************************************************************************/
/*                   GDALAlgorithmArgIsPositional()                     */
/************************************************************************/

/** Return if the argument is a positional one.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgIsPositional(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->IsPositional();
}

/************************************************************************/
/*                   GDALAlgorithmArgIsRequired()                       */
/************************************************************************/

/** Return whether the argument is required. Defaults to false.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgIsRequired(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->IsRequired();
}

/************************************************************************/
/*                   GDALAlgorithmArgGetMinCount()                      */
/************************************************************************/

/** Return the minimum number of values for the argument.
 *
 * Defaults to 0.
 * Only applies to list type of arguments.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
int GDALAlgorithmArgGetMinCount(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, 0);
    return hArg->ptr->GetMinCount();
}

/************************************************************************/
/*                   GDALAlgorithmArgGetMaxCount()                      */
/************************************************************************/

/** Return the maximum number of values for the argument.
 *
 * Defaults to 1 for scalar types, and INT_MAX for list types.
 * Only applies to list type of arguments.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
int GDALAlgorithmArgGetMaxCount(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, 0);
    return hArg->ptr->GetMaxCount();
}

/************************************************************************/
/*                GDALAlgorithmArgGetPackedValuesAllowed()              */
/************************************************************************/

/** Return whether, for list type of arguments, several values, space
 * separated, may be specified. That is "--foo=bar,baz".
 * The default is true.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgGetPackedValuesAllowed(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->GetPackedValuesAllowed();
}

/************************************************************************/
/*                GDALAlgorithmArgGetRepeatedArgAllowed()               */
/************************************************************************/

/** Return whether, for list type of arguments, the argument may be
 * repeated. That is "--foo=bar --foo=baz".
 * The default is true.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgGetRepeatedArgAllowed(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->GetRepeatedArgAllowed();
}

/************************************************************************/
/*                    GDALAlgorithmArgGetChoices()                      */
/************************************************************************/

/** Return the allowed values (as strings) for the argument.
 *
 * Only honored for GAAT_STRING and GAAT_STRING_LIST types.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return a NULL terminated list of names, which must be destroyed with
 * CSLDestroy()

 * @since 3.11
 */
char **GDALAlgorithmArgGetChoices(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    return CPLStringList(hArg->ptr->GetChoices()).StealList();
}

/************************************************************************/
/*                  GDALAlgorithmArgGetMetadataItem()                   */
/************************************************************************/

/** Return the values of the metadata item of an argument.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param pszItem Name of the item. Must NOT be null.
 * @return a NULL terminated list of values, which must be destroyed with
 * CSLDestroy()

 * @since 3.11
 */
char **GDALAlgorithmArgGetMetadataItem(GDALAlgorithmArgH hArg,
                                       const char *pszItem)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    VALIDATE_POINTER1(pszItem, __func__, nullptr);
    const auto pVecOfStrings = hArg->ptr->GetMetadataItem(pszItem);
    return pVecOfStrings ? CPLStringList(*pVecOfStrings).StealList() : nullptr;
}

/************************************************************************/
/*                   GDALAlgorithmArgIsExplicitlySet()                  */
/************************************************************************/

/** Return whether the argument value has been explicitly set with Set()
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgIsExplicitlySet(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->IsExplicitlySet();
}

/************************************************************************/
/*                   GDALAlgorithmArgHasDefaultValue()                  */
/************************************************************************/

/** Return if the argument has a declared default value.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgHasDefaultValue(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->HasDefaultValue();
}

/************************************************************************/
/*                   GDALAlgorithmArgIsHiddenForCLI()                   */
/************************************************************************/

/** Return whether the argument must not be mentioned in CLI usage.
 *
 * For example, "output-value" for "gdal raster info", which is only
 * meant when the algorithm is used from a non-CLI context.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgIsHiddenForCLI(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->IsHiddenForCLI();
}

/************************************************************************/
/*                   GDALAlgorithmArgIsOnlyForCLI()                     */
/************************************************************************/

/** Return whether the argument is only for CLI usage.
 *
 * For example "--help"
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgIsOnlyForCLI(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->IsOnlyForCLI();
}

/************************************************************************/
/*                     GDALAlgorithmArgIsInput()                        */
/************************************************************************/

/** Indicate whether the value of the argument is read-only during the
 * execution of the algorithm.
 *
 * Default is true.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgIsInput(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->IsInput();
}

/************************************************************************/
/*                     GDALAlgorithmArgIsOutput()                       */
/************************************************************************/

/** Return whether (at least part of) the value of the argument is set
 * during the execution of the algorithm.
 *
 * For example, "output-value" for "gdal raster info"
 * Default is false.
 * An argument may return both IsInput() and IsOutput() as true.
 * For example the "gdal raster convert" algorithm consumes the dataset
 * name of its "output" argument, and sets the dataset object during its
 * execution.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgIsOutput(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->IsOutput();
}

/************************************************************************/
/*                 GDALAlgorithmArgGetDatasetType()                     */
/************************************************************************/

/** Get which type of dataset is allowed / generated.
 *
 * Binary-or combination of GDAL_OF_RASTER, GDAL_OF_VECTOR and
 * GDAL_OF_MULTIDIM_RASTER.
 * Only applies to arguments of type GAAT_DATASET or GAAT_DATASET_LIST.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
GDALArgDatasetType GDALAlgorithmArgGetDatasetType(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, 0);
    return hArg->ptr->GetDatasetType();
}

/************************************************************************/
/*                   GDALAlgorithmArgGetDatasetInputFlags()             */
/************************************************************************/

/** Indicates which components among name and dataset are accepted as
 * input, when this argument serves as an input.
 *
 * If the GADV_NAME bit is set, it indicates a dataset name is accepted as
 * input.
 * If the GADV_OBJECT bit is set, it indicates a dataset object is
 * accepted as input.
 * If both bits are set, the algorithm can accept either a name or a dataset
 * object.
 * Only applies to arguments of type GAAT_DATASET or GAAT_DATASET_LIST.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return string whose lifetime is bound to hAlg and which must not
 * be freed.
 * @since 3.11
 */
int GDALAlgorithmArgGetDatasetInputFlags(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, 0);
    return hArg->ptr->GetDatasetInputFlags();
}

/************************************************************************/
/*                  GDALAlgorithmArgGetDatasetOutputFlags()             */
/************************************************************************/

/** Indicates which components among name and dataset are modified,
 * when this argument serves as an output.
 *
 * If the GADV_NAME bit is set, it indicates a dataset name is generated as
 * output (that is the algorithm will generate the name. Rarely used).
 * If the GADV_OBJECT bit is set, it indicates a dataset object is
 * generated as output, and available for use after the algorithm has
 * completed.
 * Only applies to arguments of type GAAT_DATASET or GAAT_DATASET_LIST.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return string whose lifetime is bound to hAlg and which must not
 * be freed.
 * @since 3.11
 */
int GDALAlgorithmArgGetDatasetOutputFlags(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, 0);
    return hArg->ptr->GetDatasetOutputFlags();
}

/************************************************************************/
/*               GDALAlgorithmArgGetMutualExclusionGroup()              */
/************************************************************************/

/** Return the name of the mutual exclusion group to which this argument
 * belongs to.
 *
 * Or empty string if it does not belong to any exclusion group.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return string whose lifetime is bound to hArg and which must not
 * be freed.
 * @since 3.11
 */
const char *GDALAlgorithmArgGetMutualExclusionGroup(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    return hArg->ptr->GetMutualExclusionGroup().c_str();
}

/************************************************************************/
/*                    GDALAlgorithmArgGetAsBoolean()                    */
/************************************************************************/

/** Return the argument value as a boolean.
 *
 * Must only be called on arguments whose type is GAAT_BOOLEAN.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
bool GDALAlgorithmArgGetAsBoolean(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    if (hArg->ptr->GetType() != GAAT_BOOLEAN)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s must only be called on arguments of type GAAT_BOOLEAN",
                 __func__);
        return false;
    }
    return hArg->ptr->Get<bool>();
}

/************************************************************************/
/*                    GDALAlgorithmArgGetAsBoolean()                    */
/************************************************************************/

/** Return the argument value as a string.
 *
 * Must only be called on arguments whose type is GAAT_STRING.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return string whose lifetime is bound to hArg and which must not
 * be freed.
 * @since 3.11
 */
const char *GDALAlgorithmArgGetAsString(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    if (hArg->ptr->GetType() != GAAT_STRING)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s must only be called on arguments of type GAAT_STRING",
                 __func__);
        return nullptr;
    }
    return hArg->ptr->Get<std::string>().c_str();
}

/************************************************************************/
/*                 GDALAlgorithmArgGetAsDatasetValue()                  */
/************************************************************************/

/** Return the argument value as a GDALArgDatasetValueH.
 *
 * Must only be called on arguments whose type is GAAT_DATASET
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return handle to a GDALArgDatasetValue that must be released with
 * GDALArgDatasetValueRelease(). The lifetime of that handle does not exceed
 * the one of hArg.
 * @since 3.11
 */
GDALArgDatasetValueH GDALAlgorithmArgGetAsDatasetValue(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    if (hArg->ptr->GetType() != GAAT_DATASET)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s must only be called on arguments of type GAAT_DATASET",
                 __func__);
        return nullptr;
    }
    return std::make_unique<GDALArgDatasetValueHS>(
               &(hArg->ptr->Get<GDALArgDatasetValue>()))
        .release();
}

/************************************************************************/
/*                    GDALAlgorithmArgGetAsInteger()                    */
/************************************************************************/

/** Return the argument value as a integer.
 *
 * Must only be called on arguments whose type is GAAT_INTEGER
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
int GDALAlgorithmArgGetAsInteger(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, 0);
    if (hArg->ptr->GetType() != GAAT_INTEGER)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s must only be called on arguments of type GAAT_INTEGER",
                 __func__);
        return 0;
    }
    return hArg->ptr->Get<int>();
}

/************************************************************************/
/*                    GDALAlgorithmArgGetAsDouble()                     */
/************************************************************************/

/** Return the argument value as a double.
 *
 * Must only be called on arguments whose type is GAAT_REAL
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @since 3.11
 */
double GDALAlgorithmArgGetAsDouble(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, 0);
    if (hArg->ptr->GetType() != GAAT_REAL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s must only be called on arguments of type GAAT_REAL",
                 __func__);
        return 0;
    }
    return hArg->ptr->Get<double>();
}

/************************************************************************/
/*                   GDALAlgorithmArgGetAsStringList()                  */
/************************************************************************/

/** Return the argument value as a double.
 *
 * Must only be called on arguments whose type is GAAT_STRING_LIST.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @return a NULL terminated list of names, which must be destroyed with
 * CSLDestroy()

 * @since 3.11
 */
char **GDALAlgorithmArgGetAsStringList(GDALAlgorithmArgH hArg)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    if (hArg->ptr->GetType() != GAAT_STRING_LIST)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s must only be called on arguments of type GAAT_STRING_LIST",
                 __func__);
        return nullptr;
    }
    return CPLStringList(hArg->ptr->Get<std::vector<std::string>>())
        .StealList();
}

/************************************************************************/
/*                  GDALAlgorithmArgGetAsIntegerList()                  */
/************************************************************************/

/** Return the argument value as a integer.
 *
 * Must only be called on arguments whose type is GAAT_INTEGER
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param[out] pnCount Pointer to the number of values in the list. Must NOT be null.
 * @since 3.11
 */
const int *GDALAlgorithmArgGetAsIntegerList(GDALAlgorithmArgH hArg,
                                            size_t *pnCount)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    if (hArg->ptr->GetType() != GAAT_INTEGER_LIST)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "%s must only be called on arguments of type GAAT_INTEGER_LIST",
            __func__);
        *pnCount = 0;
        return nullptr;
    }
    const auto &val = hArg->ptr->Get<std::vector<int>>();
    *pnCount = val.size();
    return val.data();
}

/************************************************************************/
/*                  GDALAlgorithmArgGetAsDoubleList()                   */
/************************************************************************/

/** Return the argument value as a integer.
 *
 * Must only be called on arguments whose type is GAAT_INTEGER
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param[out] pnCount Pointer to the number of values in the list. Must NOT be null.
 * @since 3.11
 */
const double *GDALAlgorithmArgGetAsDoubleList(GDALAlgorithmArgH hArg,
                                              size_t *pnCount)
{
    VALIDATE_POINTER1(hArg, __func__, nullptr);
    VALIDATE_POINTER1(pnCount, __func__, nullptr);
    if (hArg->ptr->GetType() != GAAT_REAL_LIST)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s must only be called on arguments of type GAAT_REAL_LIST",
                 __func__);
        *pnCount = 0;
        return nullptr;
    }
    const auto &val = hArg->ptr->Get<std::vector<double>>();
    *pnCount = val.size();
    return val.data();
}

/************************************************************************/
/*                    GDALAlgorithmArgSetAsBoolean()                    */
/************************************************************************/

/** Set the value for a GAAT_BOOLEAN argument.
 *
 * It cannot be called several times for a given argument.
 * Validation checks and other actions are run.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param value value.
 * @return true if success.
 * @since 3.11
 */

bool GDALAlgorithmArgSetAsBoolean(GDALAlgorithmArgH hArg, bool value)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->Set(value);
}

/************************************************************************/
/*                    GDALAlgorithmArgSetAsString()                     */
/************************************************************************/

/** Set the value for a GAAT_STRING argument.
 *
 * It cannot be called several times for a given argument.
 * Validation checks and other actions are run.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param value value (may be null)
 * @return true if success.
 * @since 3.11
 */

bool GDALAlgorithmArgSetAsString(GDALAlgorithmArgH hArg, const char *value)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->Set(value ? value : "");
}

/************************************************************************/
/*                    GDALAlgorithmArgSetAsInteger()                    */
/************************************************************************/

/** Set the value for a GAAT_INTEGER (or GAAT_REAL) argument.
 *
 * It cannot be called several times for a given argument.
 * Validation checks and other actions are run.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param value value.
 * @return true if success.
 * @since 3.11
 */

bool GDALAlgorithmArgSetAsInteger(GDALAlgorithmArgH hArg, int value)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->Set(value);
}

/************************************************************************/
/*                    GDALAlgorithmArgSetAsDouble()                     */
/************************************************************************/

/** Set the value for a GAAT_REAL argument.
 *
 * It cannot be called several times for a given argument.
 * Validation checks and other actions are run.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param value value.
 * @return true if success.
 * @since 3.11
 */

bool GDALAlgorithmArgSetAsDouble(GDALAlgorithmArgH hArg, double value)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->Set(value);
}

/************************************************************************/
/*                 GDALAlgorithmArgSetAsDatasetValue()                  */
/************************************************************************/

/** Set the value for a GAAT_DATASET argument.
 *
 * It cannot be called several times for a given argument.
 * Validation checks and other actions are run.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param value Handle to a GDALArgDatasetValue. Must NOT be null.
 * @return true if success.
 * @since 3.11
 */
bool GDALAlgorithmArgSetAsDatasetValue(GDALAlgorithmArgH hArg,
                                       GDALArgDatasetValueH value)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    VALIDATE_POINTER1(value, __func__, false);
    return hArg->ptr->SetFrom(*(value->ptr));
}

/************************************************************************/
/*                     GDALAlgorithmArgSetDataset()                     */
/************************************************************************/

/** Set dataset object, increasing its reference counter.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param hDS Dataset object. May be null.
 * @return true if success.
 * @since 3.11
 */

bool GDALAlgorithmArgSetDataset(GDALAlgorithmArgH hArg, GDALDatasetH hDS)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->Set(GDALDataset::FromHandle(hDS));
}

/************************************************************************/
/*                  GDALAlgorithmArgSetAsStringList()                   */
/************************************************************************/

/** Set the value for a GAAT_STRING_LIST argument.
 *
 * It cannot be called several times for a given argument.
 * Validation checks and other actions are run.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param value value as a NULL terminated list (may be null)
 * @return true if success.
 * @since 3.11
 */

bool GDALAlgorithmArgSetAsStringList(GDALAlgorithmArgH hArg, CSLConstList value)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->Set(
        static_cast<std::vector<std::string>>(CPLStringList(value)));
}

/************************************************************************/
/*                  GDALAlgorithmArgSetAsIntegerList()                  */
/************************************************************************/

/** Set the value for a GAAT_INTEGER_LIST argument.
 *
 * It cannot be called several times for a given argument.
 * Validation checks and other actions are run.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param nCount Number of values in pnValues.
 * @param pnValues Pointer to an array of integer values of size nCount.
 * @return true if success.
 * @since 3.11
 */
bool GDALAlgorithmArgSetAsIntegerList(GDALAlgorithmArgH hArg, size_t nCount,
                                      const int *pnValues)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->Set(std::vector<int>(pnValues, pnValues + nCount));
}

/************************************************************************/
/*                   GDALAlgorithmArgSetAsDoubleList()                  */
/************************************************************************/

/** Set the value for a GAAT_REAL_LIST argument.
 *
 * It cannot be called several times for a given argument.
 * Validation checks and other actions are run.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param nCount Number of values in pnValues.
 * @param pnValues Pointer to an array of double values of size nCount.
 * @return true if success.
 * @since 3.11
 */
bool GDALAlgorithmArgSetAsDoubleList(GDALAlgorithmArgH hArg, size_t nCount,
                                     const double *pnValues)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    return hArg->ptr->Set(std::vector<double>(pnValues, pnValues + nCount));
}

/************************************************************************/
/*                     GDALAlgorithmArgSetDatasets()                    */
/************************************************************************/

/** Set dataset objects to a GAAT_DATASET_LIST argument, increasing their reference counter.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param nCount Number of values in pnValues.
 * @param pahDS Pointer to an array of dataset of size nCount.
 * @return true if success.
 * @since 3.11
 */

bool GDALAlgorithmArgSetDatasets(GDALAlgorithmArgH hArg, size_t nCount,
                                 GDALDatasetH *pahDS)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    std::vector<GDALArgDatasetValue> values;
    for (size_t i = 0; i < nCount; ++i)
    {
        values.emplace_back(GDALDataset::FromHandle(pahDS[i]));
    }
    return hArg->ptr->Set(std::move(values));
}

/************************************************************************/
/*                    GDALAlgorithmArgSetDatasetNames()                 */
/************************************************************************/

/** Set dataset names to a GAAT_DATASET_LIST argument.
 *
 * @param hArg Handle to an argument. Must NOT be null.
 * @param names Dataset names as a NULL terminated list (may be null)
 * @return true if success.
 * @since 3.11
 */

bool GDALAlgorithmArgSetDatasetNames(GDALAlgorithmArgH hArg, CSLConstList names)
{
    VALIDATE_POINTER1(hArg, __func__, false);
    std::vector<GDALArgDatasetValue> values;
    for (size_t i = 0; names[i]; ++i)
    {
        values.emplace_back(names[i]);
    }
    return hArg->ptr->Set(std::move(values));
}

/************************************************************************/
/*                      GDALArgDatasetValueCreate()                     */
/************************************************************************/

/** Instantiate an empty GDALArgDatasetValue
 *
 * @return new handle to free with GDALArgDatasetValueRelease()
 * @since 3.11
 */
GDALArgDatasetValueH GDALArgDatasetValueCreate()
{
    return std::make_unique<GDALArgDatasetValueHS>().release();
}

/************************************************************************/
/*                      GDALArgDatasetValueRelease()                    */
/************************************************************************/

/** Release a handle to a GDALArgDatasetValue
 *
 * @since 3.11
 */
void GDALArgDatasetValueRelease(GDALArgDatasetValueH hValue)
{
    delete hValue;
}

/************************************************************************/
/*                    GDALArgDatasetValueGetName()                      */
/************************************************************************/

/** Return the name component of the GDALArgDatasetValue
 *
 * @param hValue Handle to a GDALArgDatasetValue. Must NOT be null.
 * @return string whose lifetime is bound to hAlg and which must not
 * be freed.
 * @since 3.11
 */
const char *GDALArgDatasetValueGetName(GDALArgDatasetValueH hValue)
{
    VALIDATE_POINTER1(hValue, __func__, nullptr);
    return hValue->ptr->GetName().c_str();
}

/************************************************************************/
/*               GDALArgDatasetValueGetDatasetRef()                     */
/************************************************************************/

/** Return the dataset component of the GDALArgDatasetValue.
 *
 * This does not modify the reference counter, hence the lifetime of the
 * returned object is not guaranteed to exceed the one of hValue.
 *
 * @param hValue Handle to a GDALArgDatasetValue. Must NOT be null.
 * @since 3.11
 */
GDALDatasetH GDALArgDatasetValueGetDatasetRef(GDALArgDatasetValueH hValue)
{
    VALIDATE_POINTER1(hValue, __func__, nullptr);
    return GDALDataset::ToHandle(hValue->ptr->GetDatasetRef());
}

/************************************************************************/
/*               GDALArgDatasetValueGetDatasetIncreaseRefCount()        */
/************************************************************************/

/** Return the dataset component of the GDALArgDatasetValue, and increase its
 * reference count if not null. Once done with the dataset, the caller should
 * call GDALReleaseDataset().
 *
 * @param hValue Handle to a GDALArgDatasetValue. Must NOT be null.
 * @since 3.11
 */
GDALDatasetH
GDALArgDatasetValueGetDatasetIncreaseRefCount(GDALArgDatasetValueH hValue)
{
    VALIDATE_POINTER1(hValue, __func__, nullptr);
    return GDALDataset::ToHandle(hValue->ptr->GetDatasetIncreaseRefCount());
}

/************************************************************************/
/*                    GDALArgDatasetValueSetName()                      */
/************************************************************************/

/** Set dataset name
 *
 * @param hValue Handle to a GDALArgDatasetValue. Must NOT be null.
 * @param pszName Dataset name. May be null.
 * @since 3.11
 */

void GDALArgDatasetValueSetName(GDALArgDatasetValueH hValue,
                                const char *pszName)
{
    VALIDATE_POINTER0(hValue, __func__);
    hValue->ptr->Set(pszName ? pszName : "");
}

/************************************************************************/
/*                  GDALArgDatasetValueSetDataset()                     */
/************************************************************************/

/** Set dataset object, increasing its reference counter.
 *
 * @param hValue Handle to a GDALArgDatasetValue. Must NOT be null.
 * @param hDS Dataset object. May be null.
 * @since 3.11
 */

void GDALArgDatasetValueSetDataset(GDALArgDatasetValueH hValue,
                                   GDALDatasetH hDS)
{
    VALIDATE_POINTER0(hValue, __func__);
    hValue->ptr->Set(GDALDataset::FromHandle(hDS));
}
