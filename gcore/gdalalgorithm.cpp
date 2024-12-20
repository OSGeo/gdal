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

#include "gdalalgorithm.h"
#include "gdal_priv.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <map>

#ifndef _
#define _(x) (x)
#endif

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
/*                     GDALArgDatasetValueTypeName()                    */
/************************************************************************/

std::string GDALArgDatasetValueTypeName(GDALArgDatasetValueType type)
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

bool GDALAlgorithmArg::Set(const std::string &value)
{
    if (m_decl.GetType() != GAAT_STRING)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling Set(std::string) on argument '%s' of type %s is not "
                 "supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }

    std::string newValue(value);
    if (m_decl.IsReadFromFileAtSyntaxAllowed() && !value.empty() &&
        value.front() == '@')
    {
        GByte *pabyData = nullptr;
        if (VSIIngestFile(nullptr, value.c_str() + 1, &pabyData, nullptr,
                          1024 * 1024))
        {
            // Remove UTF-8 BOM
            size_t offset = 0;
            if (pabyData[0] == 0xEF && pabyData[1] == 0xBB &&
                pabyData[2] == 0xBF)
            {
                offset = 3;
            }
            newValue = reinterpret_cast<const char *>(pabyData + offset);
            VSIFree(pabyData);
        }
        else
        {
            return false;
        }
    }

    if (m_decl.IsRemoveSQLCommentsEnabled())
        newValue = CPLRemoveSQLComments(newValue);

    return SetInternal(newValue);
}

bool GDALAlgorithmArg::Set(int value)
{
    if (m_decl.GetType() == GAAT_REAL)
    {
        return Set(static_cast<double>(value));
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
    if (m_decl.GetType() != GAAT_REAL)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Calling Set(double) on argument '%s' of type %s is not supported",
            GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    return SetInternal(value);
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
    auto &val = *std::get<GDALArgDatasetValue *>(m_value);
    if (val.GetInputFlags() == GADV_NAME && val.GetOutputFlags() == GADV_OBJECT)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Dataset object '%s' is created by algorithm and cannot be set "
            "as an input.",
            GetName().c_str());
        return false;
    }
    m_explicitlySet = true;
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
    auto &val = *std::get<GDALArgDatasetValue *>(m_value);
    if (val.GetInputFlags() == GADV_NAME && val.GetOutputFlags() == GADV_OBJECT)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Dataset object '%s' is created by algorithm and cannot be set "
            "as an input.",
            GetName().c_str());
        return false;
    }
    m_explicitlySet = true;
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
    m_explicitlySet = true;
    std::get<GDALArgDatasetValue *>(m_value)->SetFrom(other);
    return RunAllActions();
}

bool GDALAlgorithmArg::Set(const std::vector<std::string> &value)
{
    if (m_decl.GetType() != GAAT_STRING_LIST)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Calling Set(const std::vector<std::string> &) on argument "
                 "'%s' of type %s is not supported",
                 GetName().c_str(), GDALAlgorithmArgTypeName(m_decl.GetType()));
        return false;
    }
    return SetInternal(value);
}

bool GDALAlgorithmArg::Set(const std::vector<int> &value)
{
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
/*                    GDALAlgorithmArg::RunValidationActions()          */
/************************************************************************/

bool GDALAlgorithmArg::RunValidationActions()
{
    for (const auto &f : m_validationActions)
    {
        if (!f())
            return false;
    }
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
    m_type = other.m_type;
    m_inputFlags = other.m_inputFlags;
    m_outputFlags = other.m_outputFlags;
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
    : m_poDS(other.m_poDS), m_name(other.m_name), m_nameSet(other.m_nameSet),
      m_type(other.m_type), m_inputFlags(other.m_inputFlags),
      m_outputFlags(other.m_outputFlags)
{
    other.m_poDS = nullptr;
    other.m_name.clear();
}

/************************************************************************/
/*              GDALInConstructionAlgorithmArg::SetIsCRSArg()           */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALInConstructionAlgorithmArg::SetIsCRSArg(bool noneAllowed)
{
    if (GetType() != GAAT_STRING)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "SetIsCRSArg() can only be called on a String argument");
        return *this;
    }
    return AddValidationAction(
        [this, noneAllowed]()
        {
            const std::string &osVal =
                static_cast<const GDALInConstructionAlgorithmArg *>(this)
                    ->Get<std::string>();
            if (!noneAllowed || (osVal != "none" && osVal != "null"))
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
    AddArg("version", 0, _("Display GDAL version and exit"), &m_dummyBoolean)
        .SetOnlyForCLI()
        .SetCategory(GAAC_COMMON);
    AddArg("json-usage", 0, _("Display usage as JSON document and exit"),
           &m_JSONUsageRequested)
        .SetOnlyForCLI()
        .SetCategory(GAAC_COMMON)
        .AddAction([this]() { m_specialActionRequested = true; });
    AddArg("drivers", 0, _("Display driver list as JSON document and exit"),
           &m_dummyBoolean)
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
            const auto &choices = arg->GetChoices();
            if (!choices.empty() && std::find(choices.begin(), choices.end(),
                                              value) == choices.end())
            {
                std::string expected;
                for (const auto &choice : choices)
                {
                    if (!expected.empty())
                        expected += ", ";
                    expected += '\'';
                    expected += choice;
                    expected += '\'';
                }
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "Invalid value '%s' for string argument '%s'. Should be "
                    "one among %s.",
                    value.c_str(), name.c_str(), expected.c_str());
                return false;
            }

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
            const auto &choices = arg->GetChoices();
            for (const char *v : aosTokens)
            {
                if (!choices.empty() &&
                    std::find(choices.begin(), choices.end(), v) ==
                        choices.end())
                {
                    std::string expected;
                    for (const auto &choice : choices)
                    {
                        if (!expected.empty())
                            expected += ", ";
                        expected += '\'';
                        expected += choice;
                        expected += '\'';
                    }
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Invalid value '%s' for string argument '%s'. "
                                "Should be "
                                "one among %s.",
                                v, name.c_str(), expected.c_str());
                    return false;
                }

                valueVector.push_back(v);
            }
            break;
        }

        case GAAT_INTEGER_LIST:
        {
            const CPLStringList aosTokens(
                arg->GetPackedValuesAllowed()
                    ? CSLTokenizeString2(value.c_str(), ",", CSLT_HONOURSTRINGS)
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
                    val >= INT_MIN && val <= INT_MAX)
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
                    ? CSLTokenizeString2(value.c_str(), ",", CSLT_HONOURSTRINGS)
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
                if (endptr != v + strlen(v))
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
            m_shortCutAlg = InstantiateSubAlgorithm(args[0]);
            if (m_shortCutAlg)
            {
                m_selectedSubAlg = m_shortCutAlg.get();
                bool bRet = m_selectedSubAlg->ParseCommandLineArguments(
                    std::vector<std::string>(args.begin() + 1, args.end()));
                m_selectedSubAlg->PropagateSpecialActionTo(this);
                return bRet;
            }
            else
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Unknown command: '%s'", args[0].c_str());
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
            auto iterArg = m_mapLongNameToArg.find(name.substr(2));
            if (iterArg == m_mapLongNameToArg.end())
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "Long name option '%s' is unknown.", name.c_str());
                return false;
            }
            arg = iterArg->second;
            if (equalPos != std::string::npos)
            {
                hasValue = true;
                value = strArg.substr(equalPos + 1);
            }
        }
        else if (strArg.size() >= 2 && strArg[0] == '-')
        {
            if (strArg.size() != 2)
            {
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "Option '%s' not recognized. Should be either a long "
                    "option or a one-letter short option.",
                    strArg.c_str());
                return false;
            }
            name = strArg;
            auto iterArg = m_mapShortNameToArg.find(name.substr(1));
            if (iterArg == m_mapShortNameToArg.end())
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "Short name option '%s' is unknown.", name.c_str());
                return false;
            }
            arg = iterArg->second;
        }
        else
        {
            ++i;
            continue;
        }
        assert(arg);

        if (arg->GetType() == GAAT_BOOLEAN)
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
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "Expected value for argument '%s', but ran short of tokens",
                    name.c_str());
                return false;
            }
            value = lArgs[i + 1];
            lArgs.erase(lArgs.begin() + i + 1);
        }

        if (!ParseArgument(arg, name, value, inConstructionValues))
            return false;

        lArgs.erase(lArgs.begin() + i);
    }

    if (m_specialActionRequested)
    {
        return true;
    }

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
                            return false;
                        }
                        nCountAtEnd++;
                    }
                }
                if (lArgs.size() < nCountAtEnd)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Not enough positional values.");
                    return false;
                }
                for (; i < lArgs.size() - nCountAtEnd; ++i)
                {
                    if (!ParseArgument(arg, arg->GetName().c_str(), lArgs[i],
                                       inConstructionValues))
                    {
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
                    return false;
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
    if (iCurPosArg < m_positionalArgs.size() &&
        (GDALAlgorithmArgTypeIsList(m_positionalArgs[iCurPosArg]->GetType())
             ? m_positionalArgs[iCurPosArg]->GetMinCount() > 0
             : m_positionalArgs[iCurPosArg]->IsRequired()))
    {
        while (iCurPosArg < m_positionalArgs.size() &&
               m_positionalArgs[iCurPosArg]->IsExplicitlySet())
        {
            ++iCurPosArg;
        }
        if (iCurPosArg < m_positionalArgs.size())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Positional arguments starting at '%s' have not been "
                        "specified.",
                        m_positionalArgs[iCurPosArg]->GetMetaVar().c_str());
            return false;
        }
    }

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
            if (!arg->Set(
                    std::get<std::vector<double>>(inConstructionValues[arg])))
            {
                return false;
            }
        }
        else if (arg->GetType() == GAAT_DATASET_LIST)
        {
            if (!arg->Set(std::move(std::get<std::vector<GDALArgDatasetValue>>(
                    inConstructionValues[arg]))))
            {
                return false;
            }
        }
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
    const bool update = (updateArg && updateArg->GetType() == GAAT_BOOLEAN &&
                         updateArg->Get<bool>());
    const auto overwriteArg = algForOutput->GetArg("overwrite");
    const bool overwrite =
        (arg->IsOutput() && overwriteArg &&
         overwriteArg->GetType() == GAAT_BOOLEAN && overwriteArg->Get<bool>());
    auto outputArg = algForOutput->GetArg(GDAL_ARG_NAME_OUTPUT);
    auto &val = arg->Get<GDALArgDatasetValue>();
    if (!val.GetDatasetRef() && !val.IsNameSet())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Argument '%s' has no dataset object or dataset name.",
                    arg->GetName().c_str());
        ret = false;
    }
    else if (!val.GetDatasetRef() &&
             (!arg->IsOutput() || (arg == outputArg && update && !overwrite)))
    {
        int flags = val.GetType();
        bool assignToOutputArg = false;

        // Check if input and output parameters point to the same
        // filename (for vector datasets)
        if (arg->GetName() == GDAL_ARG_NAME_INPUT && update && !overwrite &&
            outputArg && outputArg->GetType() == GAAT_DATASET)
        {
            auto &outputVal = outputArg->Get<GDALArgDatasetValue>();
            if (!outputVal.GetDatasetRef() &&
                outputVal.GetName() == val.GetName() &&
                (outputVal.GetInputFlags() & GADV_OBJECT) != 0)
            {
                assignToOutputArg = true;
                flags |= GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR;
            }
        }

        if (!arg->IsOutput() || val.GetInputFlags() == GADV_NAME)
            flags |= GDAL_OF_VERBOSE_ERROR;
        if ((arg == outputArg || !outputArg) && update)
            flags |= GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR;

        CPLStringList aosOpenOptions;
        CPLStringList aosAllowedDrivers;
        if (arg->GetName() == GDAL_ARG_NAME_INPUT)
        {
            const auto ooArg = GetArg("open-option");
            if (ooArg && ooArg->GetType() == GAAT_STRING_LIST)
                aosOpenOptions =
                    CPLStringList(ooArg->Get<std::vector<std::string>>());

            const auto ifArg = GetArg("input-format");
            if (ifArg && ifArg->GetType() == GAAT_STRING_LIST)
                aosAllowedDrivers =
                    CPLStringList(ifArg->Get<std::vector<std::string>>());
        }

        auto poDS =
            GDALDataset::Open(val.GetName().c_str(), flags,
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
            }
            val.Set(poDS);
            poDS->ReleaseRef();
        }
        else
        {
            ret = false;
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
        else if (arg->IsExplicitlySet() &&
                 GDALAlgorithmArgTypeIsList(arg->GetType()))
        {
            int valueCount = 0;
            if (arg->GetType() == GAAT_STRING_LIST)
            {
                valueCount = static_cast<int>(
                    arg->Get<std::vector<std::string>>().size());
            }
            else if (arg->GetType() == GAAT_INTEGER_LIST)
            {
                valueCount =
                    static_cast<int>(arg->Get<std::vector<int>>().size());
            }
            else if (arg->GetType() == GAAT_REAL_LIST)
            {
                valueCount =
                    static_cast<int>(arg->Get<std::vector<double>>().size());
            }
            else if (arg->GetType() == GAAT_DATASET_LIST)
            {
                valueCount = static_cast<int>(
                    arg->Get<std::vector<GDALArgDatasetValue>>().size());
            }

            if (valueCount != arg->GetMinCount() &&
                arg->GetMinCount() == arg->GetMaxCount())
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "%d value(s) have been specified for argument '%s', "
                    "whereas exactly %d were expected.",
                    valueCount, arg->GetName().c_str(), arg->GetMinCount());
                ret = false;
            }
            else if (valueCount < arg->GetMinCount())
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "Only %d value(s) have been specified for argument '%s', "
                    "whereas at least %d were expected.",
                    valueCount, arg->GetName().c_str(), arg->GetMinCount());
                ret = false;
            }
            else if (valueCount > arg->GetMaxCount())
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "%d values have been specified for argument '%s', "
                            "whereas at most %d were expected.",
                            valueCount, arg->GetName().c_str(),
                            arg->GetMaxCount());
                ret = false;
            }
        }

        if (arg->IsExplicitlySet() && arg->GetType() == GAAT_DATASET_LIST)
        {
            auto &listVal = arg->Get<std::vector<GDALArgDatasetValue>>();
            for (auto &val : listVal)
            {
                if (!val.GetDatasetRef() && val.GetName().empty())
                {
                    ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "Argument '%s' has no dataset object or dataset name.",
                        arg->GetName().c_str());
                    ret = false;
                }
                else if (!val.GetDatasetRef())
                {
                    int flags = val.GetType() | GDAL_OF_VERBOSE_ERROR;

                    CPLStringList aosOpenOptions;
                    CPLStringList aosAllowedDrivers;
                    if (arg->GetName() == GDAL_ARG_NAME_INPUT)
                    {
                        const auto ooArg = GetArg("open-option");
                        if (ooArg && ooArg->GetType() == GAAT_STRING_LIST)
                        {
                            aosOpenOptions = CPLStringList(
                                ooArg->Get<std::vector<std::string>>());
                        }

                        const auto ifArg = GetArg("input-format");
                        if (ifArg && ifArg->GetType() == GAAT_STRING_LIST)
                        {
                            aosAllowedDrivers = CPLStringList(
                                ifArg->Get<std::vector<std::string>>());
                        }

                        const auto updateArg = GetArg(GDAL_ARG_NAME_UPDATE);
                        if (updateArg && updateArg->GetType() == GAAT_BOOLEAN &&
                            updateArg->Get<bool>())
                        {
                            flags |= GDAL_OF_UPDATE;
                        }
                    }

                    auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                        val.GetName().c_str(), flags, aosAllowedDrivers.List(),
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
    return ret;
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
        static_cast<GDALInConstructionAlgorithmArg *>(m_args.back().get()));
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
                      GDALArgDatasetValue *pValue, GDALArgDatasetValueType type)
{
    pValue->SetType(type);
    auto &arg = AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
        this,
        GDALAlgorithmArgDecl(longName, chShortName, helpMessage, GAAT_DATASET),
        pValue));
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
                      GDALArgDatasetValueType)
{
    // FIXME
    // pValue->SetType(type);
    return AddArg(std::make_unique<GDALInConstructionAlgorithmArg>(
        this,
        GDALAlgorithmArgDecl(longName, chShortName, helpMessage,
                             GAAT_DATASET_LIST),
        pValue));
}

/************************************************************************/
/*                 GDALAlgorithm::AddInputDatasetArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddInputDatasetArg(GDALArgDatasetValue *pValue,
                                  GDALArgDatasetValueType type,
                                  bool positionalAndRequired)
{
    auto &arg = AddArg(GDAL_ARG_NAME_INPUT, 'i',
                       CPLSPrintf("Input %s dataset",
                                  GDALArgDatasetValueTypeName(type).c_str()),
                       pValue, type);
    if (positionalAndRequired)
        arg.SetPositional().SetRequired();
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddInputDatasetArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddInputDatasetArg(std::vector<GDALArgDatasetValue> *pValue,
                                  GDALArgDatasetValueType type,
                                  bool positionalAndRequired)
{
    auto &arg = AddArg(GDAL_ARG_NAME_INPUT, 'i',
                       CPLSPrintf("Input %s datasets",
                                  GDALArgDatasetValueTypeName(type).c_str()),
                       pValue, type);
    if (positionalAndRequired)
        arg.SetPositional().SetRequired();
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOutputDatasetArg()                 */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddOutputDatasetArg(GDALArgDatasetValue *pValue,
                                   GDALArgDatasetValueType type,
                                   bool positionalAndRequired)
{
    pValue->SetInputFlags(GADV_NAME);
    pValue->SetOutputFlags(GADV_OBJECT);
    auto &arg = AddArg(GDAL_ARG_NAME_OUTPUT, 'o',
                       CPLSPrintf("Output %s dataset",
                                  GDALArgDatasetValueTypeName(type).c_str()),
                       pValue, type)
                    .SetIsInput(true)
                    .SetIsOutput(true);
    if (positionalAndRequired)
        arg.SetPositional().SetRequired();
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOverwriteArg()                     */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALAlgorithm::AddOverwriteArg(bool *pValue)
{
    return AddArg("overwrite", 0,
                  _("Whether overwriting existing output is allowed"), pValue)
        .SetDefault(false);
}

/************************************************************************/
/*                 GDALAlgorithm::AddUpdateArg()                        */
/************************************************************************/

GDALInConstructionAlgorithmArg &GDALAlgorithm::AddUpdateArg(bool *pValue)
{
    return AddArg(GDAL_ARG_NAME_UPDATE, 0,
                  _("Whether to open existing dataset in update mode"), pValue)
        .SetDefault(false);
}

/************************************************************************/
/*                 GDALAlgorithm::AddOpenOptionsArg()                   */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddOpenOptionsArg(std::vector<std::string> *pValue)
{
    return AddArg("open-option", 0, _("Open options"), pValue)
        .AddAlias("oo")
        .SetMetaVar("KEY=VALUE")
        .SetCategory(GAAC_ADVANCED);
}

/************************************************************************/
/*                            ValidateFormat()                          */
/************************************************************************/

bool GDALAlgorithm::ValidateFormat(const GDALAlgorithmArg &arg) const
{
    if (arg.GetChoices().empty())
    {
        const auto Validate = [this, &arg](const std::string &val)
        {
            auto hDriver = GDALGetDriverByName(val.c_str());
            if (!hDriver)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Invalid value for argument '%s'. Driver '%s' does "
                            "not exist",
                            arg.GetName().c_str(), val.c_str());
                return false;
            }

            const auto caps = arg.GetMetadataItem(GAAMDI_REQUIRED_CAPABILITIES);
            if (caps)
            {
                for (const std::string &cap : *caps)
                {
                    if (!GDALGetMetadataItem(hDriver, cap.c_str(), nullptr))
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
/*                 GDALAlgorithm::AddInputFormatsArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddInputFormatsArg(std::vector<std::string> *pValue)
{
    auto &arg = AddArg("input-format", 0, _("Input formats"), pValue)
                    .AddAlias("if")
                    .SetCategory(GAAC_ADVANCED);
    arg.AddValidationAction([this, &arg]() { return ValidateFormat(arg); });
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOutputFormatArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddOutputFormatArg(std::string *pValue)
{
    auto &arg = AddArg("output-format", 'f', _("Output format"), pValue)
                    .AddAlias("of")
                    .AddAlias("format");
    arg.AddValidationAction([this, &arg]() { return ValidateFormat(arg); });
    return arg;
}

/************************************************************************/
/*                 GDALAlgorithm::AddOutputStringArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddOutputStringArg(std::string *pValue)
{
    return AddArg("output-string", 0,
                  _("Output string, in which the result is placed"), pValue)
        .SetHiddenForCLI()
        .SetIsInput(false)
        .SetIsOutput(true);
}

/************************************************************************/
/*                    GDALAlgorithm::AddLayerNameArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddLayerNameArg(std::string *pValue)
{
    return AddArg("layer", 'l', _("Layer name"), pValue);
}

/************************************************************************/
/*                    GDALAlgorithm::AddLayerNameArg()                  */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddLayerNameArg(std::vector<std::string> *pValue)
{
    return AddArg("layer", 'l', _("Layer name"), pValue);
}

/************************************************************************/
/*                          ValidateKeyValue()                          */
/************************************************************************/

bool GDALAlgorithm::ValidateKeyValue(const GDALAlgorithmArg &arg) const
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
        for (const auto &val : arg.Get<std::vector<std::string>>())
        {
            if (!Validate(val))
                return false;
        }
    }

    return true;
}

/************************************************************************/
/*                 GDALAlgorithm::AddCreationOptionsArg()               */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddCreationOptionsArg(std::vector<std::string> *pValue)
{
    auto &arg = AddArg("creation-option", 0, _("Creation option"), pValue)
                    .AddAlias("co")
                    .SetMetaVar("<KEY>=<VALUE>");
    arg.AddValidationAction([this, &arg]() { return ValidateKeyValue(arg); });
    return arg;
}

/************************************************************************/
/*                GDALAlgorithm::AddLayerCreationOptionsArg()           */
/************************************************************************/

GDALInConstructionAlgorithmArg &
GDALAlgorithm::AddLayerCreationOptionsArg(std::vector<std::string> *pValue)
{
    auto &arg =
        AddArg("layer-creation-option", 0, _("Layer creation option"), pValue)
            .AddAlias("lco")
            .SetMetaVar("<KEY>=<VALUE>");
    arg.AddValidationAction([this, &arg]() { return ValidateKeyValue(arg); });
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
                       helpMessage ? helpMessage
                                   : _("Bounding box as xmin,ymin,xmax,ymax"),
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
/*                       GDALAlgorithm::Run()                           */
/************************************************************************/

bool GDALAlgorithm::Run(GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (m_selectedSubAlg)
        return m_selectedSubAlg->Run(pfnProgress, pProgressData);

    if (m_helpRequested)
    {
        printf("%s", GetUsageForCLI(false).c_str()); /*ok*/
        return true;
    }

    if (m_JSONUsageRequested)
    {
        printf("%s", GetUsageAsJSON().c_str()); /*ok*/
        return true;
    }

    return ValidateArguments() && RunImpl(pfnProgress, pProgressData);
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
        if (arg->IsHiddenForCLI())
            continue;
        std::string opt;
        bool addComma = false;
        if (!arg->GetShortName().empty())
        {
            opt += '-';
            opt += arg->GetShortName();
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
        if (!arg->IsHiddenForCLI() && !arg->IsPositional())
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
            assert(subAlg);
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
                    if (alias == GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR)
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
                osRet += " <";
                osRet += arg->GetMetaVar();
                osRet += '>';
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

            if (arg->GetType() == GAAT_DATASET)
            {
                auto &val = arg->Get<GDALArgDatasetValue>();
                if (val.GetInputFlags() == GADV_NAME &&
                    val.GetOutputFlags() == GADV_OBJECT)
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

            if (arg->GetDisplayHintAboutRepetition())
            {
                if (arg->GetMinCount() > 0 &&
                    arg->GetMinCount() == arg->GetMaxCount())
                {
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
                    if (otherArg->IsHiddenForCLI() || otherArg.get() == arg)
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

    if (!m_helpURL.empty())
    {
        osRet += "\nFor more details, consult ";
        osRet += GetHelpFullURL();
        osRet += '\n';
    }

    return osRet;
}

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
        assert(subAlg);
        if (subAlg->m_displayInJSONUsage)
        {
            CPLJSONDocument oSubDoc;
            oSubDoc.LoadMemory(subAlg->GetUsageAsJSON());
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
                case GAAT_DATASET:
                case GAAT_STRING_LIST:
                case GAAT_INTEGER_LIST:
                case GAAT_REAL_LIST:
                case GAAT_DATASET_LIST:
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unhandled default value for arg %s",
                             arg->GetName().c_str());
                    break;
            }
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

        if (arg->GetType() == GAAT_DATASET)
        {
            const auto &val = arg->Get<GDALArgDatasetValue>();
            {
                CPLJSONArray jAr;
                if (val.GetType() & GDAL_OF_RASTER)
                    jAr.Add("raster");
                if (val.GetType() & GDAL_OF_VECTOR)
                    jAr.Add("vector");
                if (val.GetType() & GDAL_OF_MULTIDIM_RASTER)
                    jAr.Add("muldim_raster");
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
                jArg.Add("input_flags", GetFlags(val.GetInputFlags()));
            }
            if (arg->IsOutput())
            {
                jArg.Add("output_flags", GetFlags(val.GetOutputFlags()));
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
            if (!arg->IsOnlyForCLI() && arg->IsInput() && !arg->IsOutput())
                jArgs.Add(ProcessArg(arg.get()));
        }
        oRoot.Add("input_arguments", jArgs);
    }

    {
        CPLJSONArray jArgs;
        for (const auto &arg : m_args)
        {
            if (!arg->IsOnlyForCLI() && !arg->IsInput() && arg->IsOutput())
                jArgs.Add(ProcessArg(arg.get()));
        }
        oRoot.Add("output_arguments", jArgs);
    }

    {
        CPLJSONArray jArgs;
        for (const auto &arg : m_args)
        {
            if (!arg->IsOnlyForCLI() && arg->IsInput() && arg->IsOutput())
                jArgs.Add(ProcessArg(arg.get()));
        }
        oRoot.Add("input_output_arguments", jArgs);
    }

    return oDoc.SaveAsString();
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
 * @param value value (may be null)
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
/*                    GDALArgDatasetValueGetType()                      */
/************************************************************************/

/** Get which type of dataset is allowed / generated.
 *
 * Binary-or combination of GDAL_OF_RASTER, GDAL_OF_VECTOR and
 * GDAL_OF_MULTIDIM_RASTER.
 *
 * @param hValue Handle to a GDALArgDatasetValue. Must NOT be null.
 * @since 3.11
 */
GDALArgDatasetValueType GDALArgDatasetValueGetType(GDALArgDatasetValueH hValue)
{
    VALIDATE_POINTER1(hValue, __func__, 0);
    return hValue->ptr->GetType();
}

/************************************************************************/
/*                   GDALArgDatasetValueGetInputFlags()                 */
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
 *
 * @param hValue Handle to a GDALArgDatasetValue. Must NOT be null.
 * @return string whose lifetime is bound to hAlg and which must not
 * be freed.
 * @since 3.11
 */
int GDALArgDatasetValueGetInputFlags(GDALArgDatasetValueH hValue)
{
    VALIDATE_POINTER1(hValue, __func__, 0);
    return hValue->ptr->GetInputFlags();
}

/************************************************************************/
/*                  GDALArgDatasetValueGetOutputFlags()                 */
/************************************************************************/

/** Indicates which components among name and dataset are modified,
 * when this argument serves as an output.
 *
 * If the GADV_NAME bit is set, it indicates a dataset name is generated as
 * output (that is the algorithm will generate the name. Rarely used).
 * If the GADV_OBJECT bit is set, it indicates a dataset object is
 * generated as output, and available for use after the algorithm has
 * completed.
 *
 * @param hValue Handle to a GDALArgDatasetValue. Must NOT be null.
 * @return string whose lifetime is bound to hAlg and which must not
 * be freed.
 * @since 3.11
 */
int GDALArgDatasetValueGetOutputFlags(GDALArgDatasetValueH hValue)
{
    VALIDATE_POINTER1(hValue, __func__, 0);
    return hValue->ptr->GetOutputFlags();
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
