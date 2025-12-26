/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDALAlgorithm C++ API
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_ALGORITHM_CPP_INCLUDED
#define GDAL_ALGORITHM_CPP_INCLUDED

#include "gdalalgorithm_c.h"

/************************************************************************/
/************************************************************************/
/*                      GDAL Algorithm C++ API                          */
/************************************************************************/
/************************************************************************/

// This header requires C++17
// _MSC_VER >= 1920 : Visual Studio >= 2019
#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS) &&                 \
    (defined(DOXYGEN_SKIP) || __cplusplus >= 201703L || _MSC_VER >= 1920)

#include "cpl_error.h"

#include <limits>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

class GDALDataset;
class OGRSpatialReference;

/** Common argument category */
constexpr const char *GAAC_COMMON = "Common";

/** Base argument category */
constexpr const char *GAAC_BASE = "Base";

/** Advanced argument category */
constexpr const char *GAAC_ADVANCED = "Advanced";

/** Esoteric argument category */
constexpr const char *GAAC_ESOTERIC = "Esoteric";

/** Argument metadata item that applies to the "input-format" and
 * "output-format" argument */
constexpr const char *GAAMDI_REQUIRED_CAPABILITIES = "required_capabilities";

/** Argument metadata item that applies to "output-format" argument */
constexpr const char *GAAMDI_VRT_COMPATIBLE = "vrt_compatible";

/** Argument metadata item that applies to "output-format" argument */
constexpr const char *GAAMDI_ALLOWED_FORMATS = "allowed_formats";

/** Argument metadata item that applies to "output-format" argument */
constexpr const char *GAAMDI_EXCLUDED_FORMATS = "excluded_formats";

/** Argument metadata item that applies to "output-format" argument */
constexpr const char *GAAMDI_EXTRA_FORMATS = "extra_formats";

/** Name of the argument for an input dataset. */
constexpr const char *GDAL_ARG_NAME_INPUT = "input";

/** Name of the argument for the input format. */
constexpr const char *GDAL_ARG_NAME_INPUT_FORMAT = "input-format";

/** Name of the argument for the input layer. */
constexpr const char *GDAL_ARG_NAME_INPUT_LAYER = "input-layer";

/** Name of the argument for an open option. */
constexpr const char *GDAL_ARG_NAME_OPEN_OPTION = "open-option";

/** Name of the argument for an output dataset. */
constexpr const char *GDAL_ARG_NAME_OUTPUT = "output";

/** Name of the argument for an output string. */
constexpr const char *GDAL_ARG_NAME_OUTPUT_STRING = "output-string";

/** Name of the boolean argument to request outtputing directly on stdout. */
constexpr const char *GDAL_ARG_NAME_STDOUT = "stdout";

/** Name of the argument for an output format. */
constexpr const char *GDAL_ARG_NAME_OUTPUT_FORMAT = "output-format";

/** Name of the argument for the output layer. */
constexpr const char *GDAL_ARG_NAME_OUTPUT_LAYER = "output-layer";

/** Name of the argument for a creation option. */
constexpr const char *GDAL_ARG_NAME_CREATION_OPTION = "creation-option";

/** Name of the argument for a layer creation option. */
constexpr const char *GDAL_ARG_NAME_LAYER_CREATION_OPTION =
    "layer-creation-option";

/** Name of the argument for update. */
constexpr const char *GDAL_ARG_NAME_UPDATE = "update";

/** Name of the argument for overwriting a dataset. */
constexpr const char *GDAL_ARG_NAME_OVERWRITE = "overwrite";

/** Name of the argument for overwriting a layer. */
constexpr const char *GDAL_ARG_NAME_OVERWRITE_LAYER = "overwrite-layer";

/** Name of the argument for append. */
constexpr const char *GDAL_ARG_NAME_APPEND = "append";

/** Name of the argument for read-only. */
constexpr const char *GDAL_ARG_NAME_READ_ONLY = "read-only";

/** Name of the argument for number of threads (string). */
constexpr const char *GDAL_ARG_NAME_NUM_THREADS = "num-threads";

/** Name of the argument for number of threads (integer). */
constexpr const char *GDAL_ARG_NAME_NUM_THREADS_INT_HIDDEN =
    "num-threads-int-hidden";

/** Name of the argument for quiet mode. */
constexpr const char *GDAL_ARG_NAME_QUIET = "quiet";

/** Driver must expose GDAL_DCAP_RASTER or GDAL_DCAP_MULTIDIM_RASTER.
 * This is a potential value of GetMetadataItem(GAAMDI_REQUIRED_CAPABILITIES)
 */
constexpr const char *GDAL_ALG_DCAP_RASTER_OR_MULTIDIM_RASTER =
    "raster-or-multidim-raster";

/************************************************************************/
/*                           GDALArgDatasetValue                        */
/************************************************************************/

/** Return the string representation of GDALArgDatasetType */
std::string CPL_DLL GDALAlgorithmArgDatasetTypeName(GDALArgDatasetType);

class GDALAlgorithmArg;

/** Value for an argument that points to a GDALDataset.
 *
 * This is the value of arguments of type GAAT_DATASET or GAAT_DATASET_LIST.
 */
class CPL_DLL GDALArgDatasetValue final
{
  public:
    /** Default (empty) constructor */
    GDALArgDatasetValue() = default;

    /** Constructor by dataset name. */
    explicit GDALArgDatasetValue(const std::string &name)
        : m_name(name), m_nameSet(true)
    {
    }

    /** Constructor by dataset instance, increasing its reference counter */
    explicit GDALArgDatasetValue(GDALDataset *poDS);

    /** Move constructor */
    GDALArgDatasetValue(GDALArgDatasetValue &&other);

    /** Destructor. Decrease m_poDS reference count, and destroy it if no
     * longer referenced. */
    ~GDALArgDatasetValue();

    /** Dereference the dataset object and close it if no longer referenced.
     * Return an error if an error occurred during dataset closing. */
    bool Close();

    /** Move-assignment operator */
    GDALArgDatasetValue &operator=(GDALArgDatasetValue &&other);

    /** Get the GDALDataset* instance (may be null), and increase its reference
     * count if not null. Once done with the dataset, the caller should call
     * GDALDataset::Release().
     */
    GDALDataset *GetDatasetIncreaseRefCount();

    /** Get a GDALDataset* instance (may be null). This does not modify the
     * reference counter, hence the lifetime of the returned object is not
     * guaranteed to exceed the one of this instance.
     */
    GDALDataset *GetDatasetRef()
    {
        return m_poDS;
    }

    /** Get a GDALDataset* instance (may be null). This does not modify the
     * reference counter, hence the lifetime of the returned object is not
     * guaranteed to exceed the one of this instance.
     */
    const GDALDataset *GetDatasetRef() const
    {
        return m_poDS;
    }

    /** Borrow the GDALDataset* instance (may be null), leaving its reference
     * counter unchanged.
     */
    GDALDataset *BorrowDataset()
    {
        GDALDataset *ret = m_poDS;
        m_poDS = nullptr;
        return ret;
    }

    /** Borrow the GDALDataset* instance from another GDALArgDatasetValue,
     * leaving its reference counter unchanged.
     */
    void BorrowDatasetFrom(GDALArgDatasetValue &other)
    {
        Close();
        m_poDS = other.BorrowDataset();
        m_name = other.m_name;
    }

    /** Get dataset name */
    const std::string &GetName() const
    {
        return m_name;
    }

    /** Return whether a dataset name has been set */
    bool IsNameSet() const
    {
        return m_nameSet;
    }

    /** Set dataset name */
    void Set(const std::string &name);

    /** Transfer dataset to this instance (does not affect its reference
     * counter). */
    void Set(std::unique_ptr<GDALDataset> poDS);

    /** Set dataset object, increasing its reference counter. */
    void Set(GDALDataset *poDS);

    /** Set from other value, increasing the reference counter of the
     * GDALDataset object.
     */
    void SetFrom(const GDALArgDatasetValue &other);

    /** Set that the dataset has been opened by the algorithm */
    void SetDatasetOpenedByAlgorithm()
    {
        m_openedByAlgorithm = true;
    }

    /** Whether the dataset has been opened by the algorithm */
    bool HasDatasetBeenOpenedByAlgorithm() const
    {
        return m_openedByAlgorithm;
    }

  protected:
    friend class GDALAlgorithm;

    /** Set the argument that owns us. */
    void SetOwnerArgument(GDALAlgorithmArg *arg)
    {
        CPLAssert(!m_ownerArg);
        m_ownerArg = arg;
    }

  private:
    /** The owner argument (may be nullptr for freestanding objects) */
    GDALAlgorithmArg *m_ownerArg = nullptr;

    /** Dataset object. */
    GDALDataset *m_poDS = nullptr;

    /** Dataset name */
    std::string m_name{};

    /** Whether a dataset name (possibly empty for a MEM dataset...) has been set */
    bool m_nameSet = false;

    /** Whether the dataset has been opened by the algorithm */
    bool m_openedByAlgorithm = false;

    GDALArgDatasetValue(const GDALArgDatasetValue &) = delete;
    GDALArgDatasetValue &operator=(const GDALArgDatasetValue &) = delete;
};

/************************************************************************/
/*                           GDALAlgorithmArgDecl                       */
/************************************************************************/

/** Argument declaration.
 *
 * It does not hold its value.
 */
class CPL_DLL GDALAlgorithmArgDecl final
{
  public:
    /** Special value for the SetMaxCount() / GetMaxCount() to indicate
      * unlimited number of values. */
    static constexpr int UNBOUNDED = std::numeric_limits<int>::max();

    /** Constructor.
     *
     * @param longName Long name. Must be 2 characters at least. Must not start
     *                 with dash.
     * @param chShortName 1-letter short name, or NUL character
     * @param description Description.
     * @param type Type of the argument.
     */
    GDALAlgorithmArgDecl(const std::string &longName, char chShortName,
                         const std::string &description,
                         GDALAlgorithmArgType type);

    /** Declare an alias. Must be 2 characters at least. */
    GDALAlgorithmArgDecl &AddAlias(const std::string &alias)
    {
        m_aliases.push_back(alias);
        return *this;
    }

    /** Declare a shortname alias.*/
    GDALAlgorithmArgDecl &AddShortNameAlias(char shortNameAlias)
    {
        m_shortNameAliases.push_back(shortNameAlias);
        return *this;
    }

    /** Declare an hidden alias (i.e. not exposed in usage).
     * Must be 2 characters at least. */
    GDALAlgorithmArgDecl &AddHiddenAlias(const std::string &alias)
    {
        m_hiddenAliases.push_back(alias);
        return *this;
    }

    /** Declare that the argument is positional. Typically input / output files
     */
    GDALAlgorithmArgDecl &SetPositional()
    {
        m_positional = true;
        return *this;
    }

    /** Declare that the argument is required. Default is no
     */
    GDALAlgorithmArgDecl &SetRequired()
    {
        m_required = true;
        return *this;
    }

    /** Declare the "meta-var" hint.
     * By default, the meta-var value is the long name of the argument in
     * upper case.
     */
    GDALAlgorithmArgDecl &SetMetaVar(const std::string &metaVar)
    {
        m_metaVar = metaVar;
        return *this;
    }

    /** Declare the argument category: GAAC_COMMON, GAAC_BASE, GAAC_ADVANCED,
     * GAAC_ESOTERIC or a custom category.
     */
    GDALAlgorithmArgDecl &SetCategory(const std::string &category)
    {
        m_category = category;
        return *this;
    }

    /** Declare a default value for the argument.
     */
    template <class T> GDALAlgorithmArgDecl &SetDefault(const T &value)
    {
        m_hasDefaultValue = true;
        try
        {
            switch (m_type)
            {
                case GAAT_BOOLEAN:
                {
                    if constexpr (std::is_same_v<T, bool>)
                    {
                        m_defaultValue = value;
                        return *this;
                    }
                    break;
                }

                case GAAT_STRING:
                {
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        m_defaultValue = value;
                        return *this;
                    }
                    break;
                }

                case GAAT_INTEGER:
                {
                    if constexpr (std::is_same_v<T, int>)
                    {
                        m_defaultValue = value;
                        return *this;
                    }
                    break;
                }

                case GAAT_REAL:
                {
                    if constexpr (std::is_assignable_v<double &, T>)
                    {
                        m_defaultValue = static_cast<double>(value);
                        return *this;
                    }
                    break;
                }

                case GAAT_STRING_LIST:
                {
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        m_defaultValue = std::vector<std::string>{value};
                        return *this;
                    }
                    else if constexpr (std::is_same_v<T,
                                                      std::vector<std::string>>)
                    {
                        m_defaultValue = value;
                        return *this;
                    }
                    break;
                }

                case GAAT_INTEGER_LIST:
                {
                    if constexpr (std::is_same_v<T, int>)
                    {
                        m_defaultValue = std::vector<int>{value};
                        return *this;
                    }
                    else if constexpr (std::is_same_v<T, std::vector<int>>)
                    {
                        m_defaultValue = value;
                        return *this;
                    }
                    break;
                }

                case GAAT_REAL_LIST:
                {
                    if constexpr (std::is_assignable_v<double &, T>)
                    {
                        m_defaultValue =
                            std::vector<double>{static_cast<double>(value)};
                        return *this;
                    }
                    else if constexpr (std::is_same_v<T, std::vector<double>>)
                    {
                        m_defaultValue = value;
                        return *this;
                    }
                    break;
                }

                case GAAT_DATASET:
                case GAAT_DATASET_LIST:
                    break;
            }
        }
        catch (const std::bad_variant_access &)
        {
            // should not happen
            // fallthrough
        }
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Argument %s: SetDefault(): unexpected type for value",
                 GetName().c_str());
        return *this;
    }

    /** Declare a default value for the argument.
     */
    GDALAlgorithmArgDecl &SetDefault(const char *value)
    {
        return SetDefault(std::string(value));
    }

    /** Declare the minimum number of values for the argument. Defaults to 0.
     * Only applies to list type of arguments.
     * Setting it to non-zero does *not* make the argument required. It just
     * sets the minimum number of values when it is specified. To also make
     * it required, use SetRequired().
     */
    GDALAlgorithmArgDecl &SetMinCount(int count);

    /** Declare the maximum number of values for the argument.
     * Defaults to 1 for scalar types, and UNBOUNDED for list types.
     * Only applies to list type of arguments.
     */
    GDALAlgorithmArgDecl &SetMaxCount(int count);

    /** Declare whether in \--help message one should display hints about the
     * minimum/maximum number of values. Defaults to true.
     */
    GDALAlgorithmArgDecl &SetDisplayHintAboutRepetition(bool displayHint)
    {
        m_displayHintAboutRepetition = displayHint;
        return *this;
    }

    /** Declares whether, for list type of arguments, several values, comma
     * separated, may be specified. That is "--foo=bar,baz".
     * The default is true.
     */
    GDALAlgorithmArgDecl &SetPackedValuesAllowed(bool allowed)
    {
        m_packedValuesAllowed = allowed;
        return *this;
    }

    /** Declares whether, for list type of arguments, the argument may be
     * repeated. That is "--foo=bar --foo=baz".
     * The default is true.
     */
    GDALAlgorithmArgDecl &SetRepeatedArgAllowed(bool allowed)
    {
        m_repeatedArgAllowed = allowed;
        return *this;
    }

    //! @cond Doxygen_Suppress
    GDALAlgorithmArgDecl &SetChoices()
    {
        return *this;
    }

    //! @endcond

    /** Declares the allowed values (as strings) for the argument.
     * Only honored for GAAT_STRING and GAAT_STRING_LIST types.
     */
    template <
        typename T, typename... U,
        typename std::enable_if<!std::is_same_v<T, std::vector<std::string> &>,
                                bool>::type = true>
    GDALAlgorithmArgDecl &SetChoices(T &&first, U &&...rest)
    {
        m_choices.push_back(std::forward<T>(first));
        SetChoices(std::forward<U>(rest)...);
        return *this;
    }

    /** Declares the allowed values (as strings) for the argument.
     * Only honored for GAAT_STRING and GAAT_STRING_LIST types.
     */
    GDALAlgorithmArgDecl &SetChoices(const std::vector<std::string> &choices)
    {
        m_choices = choices;
        return *this;
    }

    /** Set the minimum (included) value allowed.
     *
     * Only taken into account on GAAT_INTEGER, GAAT_INTEGER_LIST,
     * GAAT_REAL and GAAT_REAL_LIST arguments.
     */
    GDALAlgorithmArgDecl &SetMinValueIncluded(double min)
    {
        m_minVal = min;
        m_minValIsIncluded = true;
        return *this;
    }

    /** Set the minimum (excluded) value allowed.
     *
     * Only taken into account on GAAT_INTEGER, GAAT_INTEGER_LIST,
     * GAAT_REAL and GAAT_REAL_LIST arguments.
     */
    GDALAlgorithmArgDecl &SetMinValueExcluded(double min)
    {
        m_minVal = min;
        m_minValIsIncluded = false;
        return *this;
    }

    /** Set the maximum (included) value allowed. */
    GDALAlgorithmArgDecl &SetMaxValueIncluded(double max)
    {
        m_maxVal = max;
        m_maxValIsIncluded = true;
        return *this;
    }

    /** Set the maximum (excluded) value allowed. */
    GDALAlgorithmArgDecl &SetMaxValueExcluded(double max)
    {
        m_maxVal = max;
        m_maxValIsIncluded = false;
        return *this;
    }

    /** Sets the minimum number of characters (for arguments of type
     * GAAT_STRING and GAAT_STRING_LIST)
     */
    GDALAlgorithmArgDecl &SetMinCharCount(int count)
    {
        m_minCharCount = count;
        return *this;
    }

    //! @cond Doxygen_Suppress
    GDALAlgorithmArgDecl &SetHiddenChoices()
    {
        return *this;
    }

    //! @endcond

    /** Declares the, hidden, allowed values (as strings) for the argument.
     * Only honored for GAAT_STRING and GAAT_STRING_LIST types.
     */
    template <typename T, typename... U>
    GDALAlgorithmArgDecl &SetHiddenChoices(T &&first, U &&...rest)
    {
        m_hiddenChoices.push_back(std::forward<T>(first));
        SetHiddenChoices(std::forward<U>(rest)...);
        return *this;
    }

    /** Declare that the argument must not be mentioned in CLI usage.
     * For example, "output-value" for "gdal raster info", which is only
     * meant when the algorithm is used from a non-CLI context.
     */
    GDALAlgorithmArgDecl &SetHiddenForCLI(bool hiddenForCLI = true)
    {
        m_hiddenForCLI = hiddenForCLI;
        return *this;
    }

    /** Declare that the argument is hidden in the context of an API use.
     * Said otherwise, if it is only for CLI usage.
     * For example "--help" */
    GDALAlgorithmArgDecl &SetHiddenForAPI(bool hiddenForAPI = true)
    {
        m_hiddenForAPI = hiddenForAPI;
        return *this;
    }

    /** Declare that the argument is hidden. Default is no.
     * This is equivalent to calling SetHiddenForCLI() and SetHiddenForAPI()
     */
    GDALAlgorithmArgDecl &SetHidden()
    {
        m_hiddenForCLI = true;
        m_hiddenForAPI = true;
        return *this;
    }

    /** Indicate whether the value of the argument is read-only during the
     * execution of the algorithm. Default is true.
     */
    GDALAlgorithmArgDecl &SetIsInput(bool isInput = true)
    {
        m_isInput = isInput;
        return *this;
    }

    /** Indicate whether (at least part of) the value of the argument is set
     * during the execution of the algorithm.
     * For example, "output-value" for "gdal raster info"
     * Default is false.
     * An argument may return both IsInput() and IsOutput() as true.
     * For example the "gdal raster convert" algorithm consumes the dataset
     * name of its "output" argument, and sets the dataset object during its
     * execution.
     */
    GDALAlgorithmArgDecl &SetIsOutput(bool isOutput = true)
    {
        m_isOutput = isOutput;
        return *this;
    }

    /** Set the name of the mutual exclusion group to which this argument
     * belongs to. At most one argument in a group can be specified.
     */
    GDALAlgorithmArgDecl &SetMutualExclusionGroup(const std::string &group)
    {
        m_mutualExclusionGroup = group;
        return *this;
    }

    /** Set user-defined metadata item.
     */
    GDALAlgorithmArgDecl &
    AddMetadataItem(const std::string &name,
                    const std::vector<std::string> &values)
    {
        m_metadata[name] = values;
        return *this;
    }

    /** Set that this (string) argument accepts the \@filename syntax to
     * mean that the content of the specified file should be used as the
     * value of the argument.
     */
    GDALAlgorithmArgDecl &SetReadFromFileAtSyntaxAllowed()
    {
        m_readFromFileAtSyntaxAllowed = true;
        return *this;
    }

    /** Sets that SQL comments must be removed from a (string) argument.
     */
    GDALAlgorithmArgDecl &SetRemoveSQLCommentsEnabled()
    {
        m_removeSQLComments = true;
        return *this;
    }

    /** Sets whether the dataset should be opened automatically by
     * GDALAlgorithm. Only applies to GAAT_DATASET and GAAT_DATASET_LIST.
     */
    GDALAlgorithmArgDecl &SetAutoOpenDataset(bool autoOpen)
    {
        m_autoOpenDataset = autoOpen;
        return *this;
    }

    /** Declares that this argument has been created on-the-fly from user-provided
     * argument.
     */
    GDALAlgorithmArgDecl &SetUserProvided()
    {
        m_userProvided = true;
        return *this;
    }

    /** Return the (long) name */
    inline const std::string &GetName() const
    {
        return m_longName;
    }

    /** Return the short name, or empty string if there is none */
    inline const std::string &GetShortName() const
    {
        return m_shortName;
    }

    /** Return the aliases (potentially none) */
    inline const std::vector<std::string> &GetAliases() const
    {
        return m_aliases;
    }

    /** Return the shortname aliases (potentially none) */
    inline const std::vector<char> &GetShortNameAliases() const
    {
        return m_shortNameAliases;
    }

    /** Return the description */
    inline const std::string &GetDescription() const
    {
        return m_description;
    }

    /** Return the "meta-var" hint.
     * By default, the meta-var value is the long name of the argument in
     * upper case.
     */
    inline const std::string &GetMetaVar() const
    {
        return m_metaVar;
    }

    /** Return the argument category: GAAC_COMMON, GAAC_BASE, GAAC_ADVANCED,
     * GAAC_ESOTERIC or a custom category.
     */
    inline const std::string &GetCategory() const
    {
        return m_category;
    }

    /** Return the type */
    inline GDALAlgorithmArgType GetType() const
    {
        return m_type;
    }

    /** Return the allowed values (as strings) for the argument.
     * Only honored for GAAT_STRING and GAAT_STRING_LIST types.
     */
    inline const std::vector<std::string> &GetChoices() const
    {
        return m_choices;
    }

    /** Return the allowed hidden values (as strings) for the argument.
     * Only honored for GAAT_STRING and GAAT_STRING_LIST types.
     */
    inline const std::vector<std::string> &GetHiddenChoices() const
    {
        return m_hiddenChoices;
    }

    /** Return the minimum value and whether it is included. */
    inline std::pair<double, bool> GetMinValue() const
    {
        return {m_minVal, m_minValIsIncluded};
    }

    /** Return the maximum value and whether it is included. */
    inline std::pair<double, bool> GetMaxValue() const
    {
        return {m_maxVal, m_maxValIsIncluded};
    }

    /** Return the minimum number of characters (for arguments of type
     * GAAT_STRING and GAAT_STRING_LIST)
     */
    inline int GetMinCharCount() const
    {
        return m_minCharCount;
    }

    /** Return whether the argument is required. Defaults to false.
     */
    inline bool IsRequired() const
    {
        return m_required;
    }

    /** Return the minimum number of values for the argument. Defaults to 0.
     * Only applies to list type of arguments.
     */
    inline int GetMinCount() const
    {
        return m_minCount;
    }

    /** Return the maximum number of values for the argument.
     * Defaults to 1 for scalar types, and UNBOUNDED for list types.
     * Only applies to list type of arguments.
     */
    inline int GetMaxCount() const
    {
        return m_maxCount;
    }

    /** Returns whether in \--help message one should display hints about the
     * minimum/maximum number of values. Defaults to true.
     */
    inline bool GetDisplayHintAboutRepetition() const
    {
        return m_displayHintAboutRepetition;
    }

    /** Return whether, for list type of arguments, several values, space
     * separated, may be specified. That is "--foo=bar,baz".
     * The default is true.
     */
    inline bool GetPackedValuesAllowed() const
    {
        return m_packedValuesAllowed;
    }

    /** Return whether, for list type of arguments, the argument may be
     * repeated. That is "--foo=bar --foo=baz".
     * The default is true.
     */
    inline bool GetRepeatedArgAllowed() const
    {
        return m_repeatedArgAllowed;
    }

    /** Return if the argument is a positional one. */
    inline bool IsPositional() const
    {
        return m_positional;
    }

    /** Return if the argument has a declared default value. */
    inline bool HasDefaultValue() const
    {
        return m_hasDefaultValue;
    }

    /** Return whether the argument is hidden.
     */
    inline bool IsHidden() const
    {
        return m_hiddenForCLI && m_hiddenForAPI;
    }

    /** Return whether the argument must not be mentioned in CLI usage.
     * For example, "output-value" for "gdal raster info", which is only
     * meant when the algorithm is used from a non-CLI context.
     */
    inline bool IsHiddenForCLI() const
    {
        return m_hiddenForCLI;
    }

    /** Return whether the argument is only for CLI usage.
     * For example "--help"
     * This is an alias for IsHiddenForAPI()
     */
    inline bool IsOnlyForCLI() const
        CPL_WARN_DEPRECATED("Use IsHiddenForAPI() instead")
    {
        return m_hiddenForAPI;
    }

    /** Return whether the argument is hidden for API usage
     * For example "--help" */
    inline bool IsHiddenForAPI() const
    {
        return m_hiddenForAPI;
    }

    /** Indicate whether the value of the argument is read-only during the
     * execution of the algorithm. Default is true.
     */
    inline bool IsInput() const
    {
        return m_isInput;
    }

    /** Return whether (at least part of) the value of the argument is set
     * during the execution of the algorithm.
     * For example, "output-value" for "gdal raster info"
     * Default is false.
     * An argument may return both IsInput() and IsOutput() as true.
     * For example the "gdal raster convert" algorithm consumes the dataset
     * name of its "output" argument, and sets the dataset object during its
     * execution.
     */
    inline bool IsOutput() const
    {
        return m_isOutput;
    }

    /** Return the name of the mutual exclusion group to which this argument
     * belongs to, or empty string if it does not belong to any exclusion
     * group.
     */
    inline const std::string &GetMutualExclusionGroup() const
    {
        return m_mutualExclusionGroup;
    }

    /** Return if this (string) argument accepts the \@filename syntax to
     * mean that the content of the specified file should be used as the
     * value of the argument.
     */
    inline bool IsReadFromFileAtSyntaxAllowed() const
    {
        return m_readFromFileAtSyntaxAllowed;
    }

    /** Returns whether SQL comments must be removed from a (string) argument.
     */
    bool IsRemoveSQLCommentsEnabled() const
    {
        return m_removeSQLComments;
    }

    /** Returns whether the dataset should be opened automatically by
     * GDALAlgorithm. Only applies to GAAT_DATASET and GAAT_DATASET_LIST.
     */
    bool AutoOpenDataset() const
    {
        return m_autoOpenDataset;
    }

    /** Returns whether the argument has been user-provided.
     */
    bool IsUserProvided() const
    {
        return m_userProvided;
    }

    /** Get user-defined metadata. */
    inline const std::map<std::string, std::vector<std::string>>
    GetMetadata() const
    {
        return m_metadata;
    }

    /** Get user-defined metadata by item name. */
    inline const std::vector<std::string> *
    GetMetadataItem(const std::string &name) const
    {
        const auto iter = m_metadata.find(name);
        return iter == m_metadata.end() ? nullptr : &(iter->second);
    }

    /** Return the default value of the argument.
     * Must be called with T consistent of the type of the algorithm, and only
     * if HasDefaultValue() is true.
     * Valid T types are:
     * - bool for GAAT_BOOLEAN
     * - int for GAAT_INTEGER
     * - double for GAAT_REAL
     * - std::string for GAAT_STRING
     * - GDALArgDatasetValue for GAAT_DATASET
     * - std::vector<int> for GAAT_INTEGER_LIST
     * - std::vector<double for GAAT_REAL_LIST
     * - std::vector<std::string> for GAAT_STRING_LIST
     * - std::vector<GDALArgDatasetValue> for GAAT_DATASET_LIST
     */
    template <class T> inline const T &GetDefault() const
    {
        return std::get<T>(m_defaultValue);
    }

    /** Get which type of dataset is allowed / generated.
     * Binary-or combination of GDAL_OF_RASTER, GDAL_OF_VECTOR and
     * GDAL_OF_MULTIDIM_RASTER, possibly combined with GDAL_OF_UPDATE.
     * Only applies to arguments of type GAAT_DATASET or GAAT_DATASET_LIST.
     */
    GDALArgDatasetType GetDatasetType() const
    {
        return m_datasetType;
    }

    /** Set which type of dataset is allowed / generated.
     * Binary-or combination of GDAL_OF_RASTER, GDAL_OF_VECTOR and
     * GDAL_OF_MULTIDIM_RASTER.
     * Only applies to arguments of type GAAT_DATASET or GAAT_DATASET_LIST.
     */
    void SetDatasetType(GDALArgDatasetType type)
    {
        m_datasetType = type;
    }

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
     */
    int GetDatasetInputFlags() const
    {
        return m_datasetInputFlags;
    }

    /** Indicates which components among name and dataset are modified,
     * when this argument serves as an output.
     *
     * If the GADV_NAME bit is set, it indicates a dataset name is generated as
     * output (that is the algorithm will generate the name. Rarely used).
     * If the GADV_OBJECT bit is set, it indicates a dataset object is
     * generated as output, and available for use after the algorithm has
     * completed.
     * Only applies to arguments of type GAAT_DATASET or GAAT_DATASET_LIST.
     */
    int GetDatasetOutputFlags() const
    {
        return m_datasetOutputFlags;
    }

    /** Set which components among name and dataset are accepted as
     * input, when this argument serves as an input.
     * Only applies to arguments of type GAAT_DATASET or GAAT_DATASET_LIST.
     */
    void SetDatasetInputFlags(int flags)
    {
        m_datasetInputFlags = flags;
    }

    /** Set which components among name and dataset are modified when this
     * argument serves as an output.
     * Only applies to arguments of type GAAT_DATASET or GAAT_DATASET_LIST.
     */
    void SetDatasetOutputFlags(int flags)
    {
        m_datasetOutputFlags = flags;
    }

  private:
    const std::string m_longName;
    const std::string m_shortName;
    const std::string m_description;
    const GDALAlgorithmArgType m_type;
    std::string m_category = GAAC_BASE;
    std::string m_metaVar{};
    std::string m_mutualExclusionGroup{};
    int m_minCount = 0;
    int m_maxCount = 0;
    bool m_required = false;
    bool m_positional = false;
    bool m_hasDefaultValue = false;
    bool m_hiddenForCLI = false;
    bool m_hiddenForAPI = false;
    bool m_isInput = true;
    bool m_isOutput = false;
    bool m_packedValuesAllowed = true;
    bool m_repeatedArgAllowed = true;
    bool m_displayHintAboutRepetition = true;
    bool m_readFromFileAtSyntaxAllowed = false;
    bool m_removeSQLComments = false;
    bool m_autoOpenDataset = true;
    bool m_userProvided = false;
    std::map<std::string, std::vector<std::string>> m_metadata{};
    std::vector<std::string> m_aliases{};
    std::vector<std::string> m_hiddenAliases{};
    std::vector<char> m_shortNameAliases{};
    std::vector<std::string> m_choices{};
    std::vector<std::string> m_hiddenChoices{};
    std::variant<bool, std::string, int, double, std::vector<std::string>,
                 std::vector<int>, std::vector<double>>
        m_defaultValue{};
    double m_minVal = std::numeric_limits<double>::quiet_NaN();
    double m_maxVal = std::numeric_limits<double>::quiet_NaN();
    bool m_minValIsIncluded = false;
    bool m_maxValIsIncluded = false;
    int m_minCharCount = 0;
    GDALArgDatasetType m_datasetType =
        GDAL_OF_RASTER | GDAL_OF_VECTOR | GDAL_OF_MULTIDIM_RASTER;

    /** Which components among name and dataset are accepted as
     * input, when this argument serves as an input.
     */
    int m_datasetInputFlags = GADV_NAME | GADV_OBJECT;

    /** Which components among name and dataset are generated as
     * output, when this argument serves as an output.
     */
    int m_datasetOutputFlags = GADV_OBJECT;
};

/************************************************************************/
/*                           GDALAlgorithmArg                           */
/************************************************************************/

class GDALAlgorithm;

/** Argument of an algorithm.
 */
class CPL_DLL GDALAlgorithmArg /* non-final */
{
  public:
    /** Constructor */
    template <class T>
    GDALAlgorithmArg(const GDALAlgorithmArgDecl &decl, T *pValue)
        : m_decl(decl), m_value(pValue)
    {
        if constexpr (!std::is_same_v<T, GDALArgDatasetValue> &&
                      !std::is_same_v<T, std::vector<GDALArgDatasetValue>>)
        {
            if (decl.HasDefaultValue())
            {
                try
                {
                    *std::get<T *>(m_value) = decl.GetDefault<T>();
                }
                catch (const std::bad_variant_access &e)
                {
                    // I don't think that can happen, but Coverity Scan thinks
                    // so
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "*std::get<T *>(m_value) = decl.GetDefault<T>() "
                             "failed: %s",
                             e.what());
                }
            }
        }
    }

    /** Destructor */
    virtual ~GDALAlgorithmArg();

    /** Return the argument declaration. */
    const GDALAlgorithmArgDecl &GetDeclaration() const
    {
        return m_decl;
    }

    /** Alias for GDALAlgorithmArgDecl::GetName() */
    inline const std::string &GetName() const
    {
        return m_decl.GetName();
    }

    /** Alias for GDALAlgorithmArgDecl::GetShortName() */
    inline const std::string &GetShortName() const
    {
        return m_decl.GetShortName();
    }

    /** Alias for GDALAlgorithmArgDecl::GetAliases() */
    inline const std::vector<std::string> &GetAliases() const
    {
        return m_decl.GetAliases();
    }

    /** Alias for GDALAlgorithmArgDecl::GetShortNameAliases() */
    inline const std::vector<char> &GetShortNameAliases() const
    {
        return m_decl.GetShortNameAliases();
    }

    /** Alias for GDALAlgorithmArgDecl::GetDescription() */
    inline const std::string &GetDescription() const
    {
        return m_decl.GetDescription();
    }

    /** Alias for GDALAlgorithmArgDecl::GetMetaVar() */
    inline const std::string &GetMetaVar() const
    {
        return m_decl.GetMetaVar();
    }

    /** Alias for GDALAlgorithmArgDecl::GetType() */
    inline GDALAlgorithmArgType GetType() const
    {
        return m_decl.GetType();
    }

    /** Alias for GDALAlgorithmArgDecl::GetCategory() */
    inline const std::string &GetCategory() const
    {
        return m_decl.GetCategory();
    }

    /** Alias for GDALAlgorithmArgDecl::IsRequired() */
    inline bool IsRequired() const
    {
        return m_decl.IsRequired();
    }

    /** Alias for GDALAlgorithmArgDecl::GetMinCount() */
    inline int GetMinCount() const
    {
        return m_decl.GetMinCount();
    }

    /** Alias for GDALAlgorithmArgDecl::GetMaxCount() */
    inline int GetMaxCount() const
    {
        return m_decl.GetMaxCount();
    }

    /** Alias for GDALAlgorithmArgDecl::GetDisplayHintAboutRepetition() */
    inline bool GetDisplayHintAboutRepetition() const
    {
        return m_decl.GetDisplayHintAboutRepetition();
    }

    /** Alias for GDALAlgorithmArgDecl::GetPackedValuesAllowed() */
    inline bool GetPackedValuesAllowed() const
    {
        return m_decl.GetPackedValuesAllowed();
    }

    /** Alias for GDALAlgorithmArgDecl::GetRepeatedArgAllowed() */
    inline bool GetRepeatedArgAllowed() const
    {
        return m_decl.GetRepeatedArgAllowed();
    }

    /** Alias for GDALAlgorithmArgDecl::IsPositional() */
    inline bool IsPositional() const
    {
        return m_decl.IsPositional();
    }

    /** Alias for GDALAlgorithmArgDecl::GetChoices() */
    inline const std::vector<std::string> &GetChoices() const
    {
        return m_decl.GetChoices();
    }

    /** Alias for GDALAlgorithmArgDecl::GetHiddenChoices() */
    inline const std::vector<std::string> &GetHiddenChoices() const
    {
        return m_decl.GetHiddenChoices();
    }

    /** Return auto completion choices, if a auto completion function has been
     * registered.
     */
    inline std::vector<std::string>
    GetAutoCompleteChoices(const std::string &currentValue) const
    {
        if (m_autoCompleteFunction)
            return m_autoCompleteFunction(currentValue);
        return {};
    }

    /** Alias for GDALAlgorithmArgDecl::GetMinValue() */
    inline std::pair<double, bool> GetMinValue() const
    {
        return m_decl.GetMinValue();
    }

    /** Alias for GDALAlgorithmArgDecl::GetMaxValue() */
    inline std::pair<double, bool> GetMaxValue() const
    {
        return m_decl.GetMaxValue();
    }

    /** Alias for GDALAlgorithmArgDecl::GetMinCharCount() */
    inline int GetMinCharCount() const
    {
        return m_decl.GetMinCharCount();
    }

    /** Return whether the argument value has been explicitly set with Set() */
    inline bool IsExplicitlySet() const
    {
        return m_explicitlySet;
    }

    /** Alias for GDALAlgorithmArgDecl::HasDefaultValue() */
    inline bool HasDefaultValue() const
    {
        return m_decl.HasDefaultValue();
    }

    /** Alias for GDALAlgorithmArgDecl::IsHidden() */
    inline bool IsHidden() const
    {
        return m_decl.IsHidden();
    }

    /** Alias for GDALAlgorithmArgDecl::IsHiddenForCLI() */
    inline bool IsHiddenForCLI() const
    {
        return m_decl.IsHiddenForCLI();
    }

    /** Alias for GDALAlgorithmArgDecl::IsOnlyForCLI() */
    inline bool IsOnlyForCLI() const
        CPL_WARN_DEPRECATED("Use IsHiddenForAPI() instead")
    {
        return m_decl.IsHiddenForAPI();
    }

    /** Alias for GDALAlgorithmArgDecl::IsHiddenForAPI() */
    inline bool IsHiddenForAPI() const
    {
        return m_decl.IsHiddenForAPI();
    }

    /** Alias for GDALAlgorithmArgDecl::IsInput() */
    inline bool IsInput() const
    {
        return m_decl.IsInput();
    }

    /** Alias for GDALAlgorithmArgDecl::IsOutput() */
    inline bool IsOutput() const
    {
        return m_decl.IsOutput();
    }

    /** Alias for GDALAlgorithmArgDecl::IsReadFromFileAtSyntaxAllowed() */
    inline bool IsReadFromFileAtSyntaxAllowed() const
    {
        return m_decl.IsReadFromFileAtSyntaxAllowed();
    }

    /** Alias for GDALAlgorithmArgDecl::IsRemoveSQLCommentsEnabled() */
    inline bool IsRemoveSQLCommentsEnabled() const
    {
        return m_decl.IsRemoveSQLCommentsEnabled();
    }

    /** Alias for GDALAlgorithmArgDecl::GetMutualExclusionGroup() */
    inline const std::string &GetMutualExclusionGroup() const
    {
        return m_decl.GetMutualExclusionGroup();
    }

    /** Alias for GDALAlgorithmArgDecl::GetMetadata() */
    inline const std::map<std::string, std::vector<std::string>>
    GetMetadata() const
    {
        return m_decl.GetMetadata();
    }

    /** Alias for GDALAlgorithmArgDecl::GetMetadataItem() */
    inline const std::vector<std::string> *
    GetMetadataItem(const std::string &name) const
    {
        return m_decl.GetMetadataItem(name);
    }

    /** Alias for GDALAlgorithmArgDecl::GetDefault() */
    template <class T> inline const T &GetDefault() const
    {
        return m_decl.GetDefault<T>();
    }

    /** Alias for GDALAlgorithmArgDecl::AutoOpenDataset() */
    inline bool AutoOpenDataset() const
    {
        return m_decl.AutoOpenDataset();
    }

    /** Alias for GDALAlgorithmArgDecl::IsUserProvided() */
    inline bool IsUserProvided() const
    {
        return m_decl.IsUserProvided();
    }

    /** Alias for GDALAlgorithmArgDecl::GetDatasetType() */
    inline GDALArgDatasetType GetDatasetType() const
    {
        return m_decl.GetDatasetType();
    }

    /** Alias for GDALAlgorithmArgDecl::GetDatasetInputFlags() */
    inline int GetDatasetInputFlags() const
    {
        return m_decl.GetDatasetInputFlags();
    }

    /** Alias for GDALAlgorithmArgDecl::GetDatasetOutputFlags() */
    inline int GetDatasetOutputFlags() const
    {
        return m_decl.GetDatasetOutputFlags();
    }

    /** Return the value of the argument, which is by decreasing order of priority:
     * - the value set through Set().
     * - the default value set through SetDefault().
     * - the initial value of the C++ variable to which this argument is bound to.
     *
     * Must be called with T consistent of the type of the algorithm:
     * - bool for GAAT_BOOLEAN
     * - int for GAAT_INTEGER
     * - double for GAAT_REAL
     * - std::string for GAAT_STRING
     * - GDALArgDatasetValue for GAAT_DATASET
     * - std::vector<int> for GAAT_INTEGER_LIST
     * - std::vector<double for GAAT_REAL_LIST
     * - std::vector<std::string> for GAAT_STRING_LIST
     * - std::vector<GDALArgDatasetValue> for GAAT_DATASET_LIST
     */
    template <class T> inline T &Get()
    {
        return *(std::get<T *>(m_value));
    }

    /** Return the value of the argument, which is by decreasing order of priority:
     * - the value set through Set().
     * - the default value set through SetDefault().
     * - the initial value of the C++ variable to which this argument is bound to.
     *
     * Must be called with T consistent of the type of the algorithm:
     * - bool for GAAT_BOOLEAN
     * - int for GAAT_INTEGER
     * - double for GAAT_REAL
     * - std::string for GAAT_STRING
     * - GDALArgDatasetValue for GAAT_DATASET
     * - std::vector<int> for GAAT_INTEGER_LIST
     * - std::vector<double for GAAT_REAL_LIST
     * - std::vector<std::string> for GAAT_STRING_LIST
     * - std::vector<GDALArgDatasetValue> for GAAT_DATASET_LIST
     */
    template <class T> inline const T &Get() const
    {
        return *(std::get<T *>(m_value));
    }

    /** Set the value for a GAAT_BOOLEAN argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(bool value);

    /** Set the value for a GAAT_STRING argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(const std::string &value);

    /** Set the value for a GAAT_STRING argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(const char *value)
    {
        return Set(std::string(value ? value : ""));
    }

    /** Set the value for a GAAT_STRING argument from a GDALDataType
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(GDALDataType dt)
    {
        return Set(GDALGetDataTypeName(dt));
    }

    /** Set the value for a GAAT_STRING argument (representing a CRS)
     * from a OGRSpatialReference
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(const OGRSpatialReference &);

    /** Set the value for a GAAT_INTEGER (or GAAT_REAL) argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(int value);

    /** Set the value for a GAAT_REAL argument */
    bool Set(double value);

    /** Set the value for a GAAT_DATASET argument, increasing ds' reference
     * counter if ds is not null.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(GDALDataset *ds);

    /** Set the value for a GAAT_DATASET argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(std::unique_ptr<GDALDataset> ds);

    /** Set the value for a GAAT_DATASET argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool SetDatasetName(const std::string &name);

    /** Set the value for a GAAT_DATASET argument.
     * It references the dataset pointed by other.m_poDS.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool SetFrom(const GDALArgDatasetValue &other);

    /** Set the value for a GAAT_STRING_LIST argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(const std::vector<std::string> &value);

    /** Set the value for a GAAT_INTEGER_LIST argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(const std::vector<int> &value);

    /** Set the value for a GAAT_REAL_LIST argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(const std::vector<double> &value);

    /** Set the value for a GAAT_DATASET_LIST argument.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool Set(std::vector<GDALArgDatasetValue> &&value);

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(bool value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(int value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(double value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(const std::string &value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(const char *value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(GDALDataType value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(const OGRSpatialReference &value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(const std::vector<int> &value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(const std::vector<double> &value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(const std::vector<std::string> &value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    inline GDALAlgorithmArg &operator=(GDALDataset *value)
    {
        Set(value);
        return *this;
    }

    /** Set the value of the argument. */
    GDALAlgorithmArg &operator=(std::unique_ptr<GDALDataset> value);

    /** Set the value for another argument.
     * For GAAT_DATASET, it will reference the dataset pointed by other.m_poDS.
     * It cannot be called several times for a given argument.
     * Validation checks and other actions are run.
     * Return true if success.
     */
    bool SetFrom(const GDALAlgorithmArg &other);

    /** Advanced method used to make "gdal info" and "gdal raster|vector info"
     * to avoid re-opening an already opened dataset */
    void SetSkipIfAlreadySet(bool skip = true)
    {
        m_skipIfAlreadySet = skip;
    }

    /** Advanced method used to make "gdal info" and "gdal raster|vector info"
     * to avoid re-opening an already opened dataset */
    bool SkipIfAlreadySet() const
    {
        return m_skipIfAlreadySet;
    }

    /** Serialize this argument and its value.
     * May return false if the argument is not explicitly set or if a dataset
     * is passed by value.
     */
    bool Serialize(std::string &serializedArg, bool absolutePath = false) const;

    /** Return an escaped string for argument serialization */
    static std::string GetEscapedString(const std::string &s);

    //! @cond Doxygen_Suppress
    void NotifyValueSet()
    {
        m_explicitlySet = true;
    }

    //! @endcond

  protected:
    friend class GDALAlgorithm;
    /** Argument declaration */
    GDALAlgorithmArgDecl m_decl;
    /** Pointer to the value */
    std::variant<bool *, std::string *, int *, double *, GDALArgDatasetValue *,
                 std::vector<std::string> *, std::vector<int> *,
                 std::vector<double> *, std::vector<GDALArgDatasetValue> *>
        m_value{};
    /** Actions */
    std::vector<std::function<void()>> m_actions{};
    /** Validation actions */
    std::vector<std::function<bool()>> m_validationActions{};
    /** Autocompletion function */
    std::function<std::vector<std::string>(const std::string &)>
        m_autoCompleteFunction{};
    /** Algorithm that may own this argument. */
    GDALAlgorithm *m_owner = nullptr;

  private:
    bool m_skipIfAlreadySet = false;
    bool m_explicitlySet = false;

    template <class T> bool SetInternal(const T &value)
    {
        m_explicitlySet = true;
        *std::get<T *>(m_value) = value;
        return RunAllActions();
    }

    bool ProcessString(std::string &value) const;

    bool RunAllActions();
    void RunActions();
    bool RunValidationActions();
    std::string ValidateChoice(const std::string &value) const;
    bool ValidateIntRange(int val) const;
    bool ValidateRealRange(double val) const;

    void ReportError(CPLErr eErrClass, CPLErrorNum err_no, const char *fmt,
                     ...) const CPL_PRINT_FUNC_FORMAT(4, 5);

    CPL_DISALLOW_COPY_ASSIGN(GDALAlgorithmArg)
};

/************************************************************************/
/*                     GDALInConstructionAlgorithmArg                   */
/************************************************************************/

//! @cond Doxygen_Suppress
namespace test_gdal_algorithm
{
struct test_gdal_algorithm;
}

//! @endcond

/** Technical class used by GDALAlgorithm when constructing argument
 * declarations.
 */
class CPL_DLL GDALInConstructionAlgorithmArg final : public GDALAlgorithmArg
{
    friend struct test_gdal_algorithm::test_gdal_algorithm;

  public:
    /** Constructor */
    template <class T>
    GDALInConstructionAlgorithmArg(GDALAlgorithm *owner,
                                   const GDALAlgorithmArgDecl &decl, T *pValue)
        : GDALAlgorithmArg(decl, pValue)
    {
        m_owner = owner;
    }

    /** Destructor */
    ~GDALInConstructionAlgorithmArg() override;

    /** Add a documented alias for the argument */
    GDALInConstructionAlgorithmArg &AddAlias(const std::string &alias);

    /** Add a non-documented alias for the argument */
    GDALInConstructionAlgorithmArg &AddHiddenAlias(const std::string &alias);

    /** Add a shortname alias for the argument */
    GDALInConstructionAlgorithmArg &AddShortNameAlias(char shortNameAlias);

    /** Alias for GDALAlgorithmArgDecl::SetPositional() */
    GDALInConstructionAlgorithmArg &SetPositional();

    /** Alias for GDALAlgorithmArgDecl::SetRequired() */
    GDALInConstructionAlgorithmArg &SetRequired()
    {
        m_decl.SetRequired();
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetMetaVar() */
    GDALInConstructionAlgorithmArg &SetMetaVar(const std::string &metaVar)
    {
        m_decl.SetMetaVar(metaVar);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetCategory() */
    GDALInConstructionAlgorithmArg &SetCategory(const std::string &category)
    {
        m_decl.SetCategory(category);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetDefault() */
    template <class T>
    GDALInConstructionAlgorithmArg &SetDefault(const T &value)
    {
        m_decl.SetDefault(value);

        if constexpr (!std::is_same_v<T, GDALArgDatasetValue> &&
                      !std::is_same_v<T, std::vector<GDALArgDatasetValue>>)
        {
            try
            {
                switch (m_decl.GetType())
                {
                    case GAAT_BOOLEAN:
                        *std::get<bool *>(m_value) = m_decl.GetDefault<bool>();
                        break;
                    case GAAT_STRING:
                        *std::get<std::string *>(m_value) =
                            m_decl.GetDefault<std::string>();
                        break;
                    case GAAT_INTEGER:
                        *std::get<int *>(m_value) = m_decl.GetDefault<int>();
                        break;
                    case GAAT_REAL:
                        *std::get<double *>(m_value) =
                            m_decl.GetDefault<double>();
                        break;
                    case GAAT_STRING_LIST:
                        *std::get<std::vector<std::string> *>(m_value) =
                            m_decl.GetDefault<std::vector<std::string>>();
                        break;
                    case GAAT_INTEGER_LIST:
                        *std::get<std::vector<int> *>(m_value) =
                            m_decl.GetDefault<std::vector<int>>();
                        break;
                    case GAAT_REAL_LIST:
                        *std::get<std::vector<double> *>(m_value) =
                            m_decl.GetDefault<std::vector<double>>();
                        break;
                    case GAAT_DATASET:
                    case GAAT_DATASET_LIST:
                        break;
                }
            }
            catch (const std::bad_variant_access &)
            {
                // I don't think that can happen, but Coverity Scan thinks so
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Argument %s: SetDefault(): unexpected type for value",
                         GetName().c_str());
            }
        }
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetDefault() */
    GDALInConstructionAlgorithmArg &SetDefault(const char *value)
    {
        return SetDefault(std::string(value));
    }

    /** Alias for GDALAlgorithmArgDecl::SetMinCount() */
    GDALInConstructionAlgorithmArg &SetMinCount(int count)
    {
        m_decl.SetMinCount(count);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetMaxCount() */
    GDALInConstructionAlgorithmArg &SetMaxCount(int count)
    {
        m_decl.SetMaxCount(count);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetDisplayHintAboutRepetition() */
    GDALInConstructionAlgorithmArg &
    SetDisplayHintAboutRepetition(bool displayHint)
    {
        m_decl.SetDisplayHintAboutRepetition(displayHint);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetPackedValuesAllowed() */
    GDALInConstructionAlgorithmArg &SetPackedValuesAllowed(bool allowed)
    {
        m_decl.SetPackedValuesAllowed(allowed);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetRepeatedArgAllowed() */
    GDALInConstructionAlgorithmArg &SetRepeatedArgAllowed(bool allowed)
    {
        m_decl.SetRepeatedArgAllowed(allowed);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetChoices() */
    template <
        typename T, typename... U,
        typename std::enable_if<!std::is_same_v<T, std::vector<std::string> &>,
                                bool>::type = true>
    GDALInConstructionAlgorithmArg &SetChoices(T &&first, U &&...rest)
    {
        m_decl.SetChoices(std::forward<T>(first), std::forward<U>(rest)...);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetChoices() */
    GDALInConstructionAlgorithmArg &
    SetChoices(const std::vector<std::string> &choices)
    {
        m_decl.SetChoices(choices);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetHiddenChoices() */
    template <typename T, typename... U>
    GDALInConstructionAlgorithmArg &SetHiddenChoices(T &&first, U &&...rest)
    {
        m_decl.SetHiddenChoices(std::forward<T>(first),
                                std::forward<U>(rest)...);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetMinValueIncluded() */
    GDALInConstructionAlgorithmArg &SetMinValueIncluded(double min)
    {
        m_decl.SetMinValueIncluded(min);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetMinValueExcluded() */
    GDALInConstructionAlgorithmArg &SetMinValueExcluded(double min)
    {
        m_decl.SetMinValueExcluded(min);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetMaxValueIncluded() */
    GDALInConstructionAlgorithmArg &SetMaxValueIncluded(double max)
    {
        m_decl.SetMaxValueIncluded(max);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetMaxValueExcluded() */
    GDALInConstructionAlgorithmArg &SetMaxValueExcluded(double max)
    {
        m_decl.SetMaxValueExcluded(max);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetMinCharCount() */
    GDALInConstructionAlgorithmArg &SetMinCharCount(int count)
    {
        m_decl.SetMinCharCount(count);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetHidden() */
    GDALInConstructionAlgorithmArg &SetHidden()
    {
        m_decl.SetHidden();
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetHiddenForCLI() */
    GDALInConstructionAlgorithmArg &SetHiddenForCLI(bool hiddenForCLI = true)
    {
        m_decl.SetHiddenForCLI(hiddenForCLI);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetHiddenForAPI() */
    GDALInConstructionAlgorithmArg &SetHiddenForAPI(bool hiddenForAPI = true)
    {
        m_decl.SetHiddenForAPI(hiddenForAPI);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetIsInput() */
    GDALInConstructionAlgorithmArg &SetIsInput(bool isInput = true)
    {
        m_decl.SetIsInput(isInput);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetIsOutput() */
    GDALInConstructionAlgorithmArg &SetIsOutput(bool isOutput = true)
    {
        m_decl.SetIsOutput(isOutput);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetReadFromFileAtSyntaxAllowed() */
    GDALInConstructionAlgorithmArg &SetReadFromFileAtSyntaxAllowed()
    {
        m_decl.SetReadFromFileAtSyntaxAllowed();
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetRemoveSQLCommentsEnabled() */
    GDALInConstructionAlgorithmArg &SetRemoveSQLCommentsEnabled()
    {
        m_decl.SetRemoveSQLCommentsEnabled();
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetAutoOpenDataset() */
    GDALInConstructionAlgorithmArg &SetAutoOpenDataset(bool autoOpen)
    {
        m_decl.SetAutoOpenDataset(autoOpen);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetMutualExclusionGroup() */
    GDALInConstructionAlgorithmArg &
    SetMutualExclusionGroup(const std::string &group)
    {
        m_decl.SetMutualExclusionGroup(group);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::AddMetadataItem() */
    GDALInConstructionAlgorithmArg &
    AddMetadataItem(const std::string &name,
                    const std::vector<std::string> &values)
    {
        m_decl.AddMetadataItem(name, values);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetDatasetType() */
    GDALInConstructionAlgorithmArg &
    SetDatasetType(GDALArgDatasetType datasetType)
    {
        m_decl.SetDatasetType(datasetType);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetDatasetInputFlags() */
    GDALInConstructionAlgorithmArg &SetDatasetInputFlags(int flags)
    {
        m_decl.SetDatasetInputFlags(flags);
        return *this;
    }

    /** Alias for GDALAlgorithmArgDecl::SetDatasetOutputFlags() */
    GDALInConstructionAlgorithmArg &SetDatasetOutputFlags(int flags)
    {
        m_decl.SetDatasetOutputFlags(flags);
        return *this;
    }

    /** Register an action that is executed, once and exactly once, if the
     * argument is explicitly set, at the latest by the ValidateArguments()
     * method. */
    GDALInConstructionAlgorithmArg &AddAction(std::function<void()> f)
    {
        m_actions.push_back(f);
        return *this;
    }

    /** Register an action that is executed, once and exactly once, if the
     * argument is explicitly set, at the latest by the ValidateArguments()
     * method. If the provided function returns false, validation fails.
     * The validation function of a given argument can only check the value of
     * this argument, and cannot assume other arguments have already been set.
     */
    GDALInConstructionAlgorithmArg &AddValidationAction(std::function<bool()> f)
    {
        m_validationActions.push_back(f);
        return *this;
    }

    /** Register a function that will return a list of valid choices for
     * the value of the argument. This is typically used for autocompletion.
     */
    GDALInConstructionAlgorithmArg &SetAutoCompleteFunction(
        std::function<std::vector<std::string>(const std::string &)> f)
    {
        m_autoCompleteFunction = std::move(f);
        return *this;
    }

    /** Register an action to validate that the argument value is a valid
     * CRS definition.
     * @param noneAllowed Set to true to mean that "null" or "none" are allowed
     * to mean to unset CRS.
     * @param specialValues List of other allowed special values.
     */
    GDALInConstructionAlgorithmArg &
    SetIsCRSArg(bool noneAllowed = false,
                const std::vector<std::string> &specialValues =
                    std::vector<std::string>());

    /** Alias for GDALAlgorithmArgDecl::SetUserProvided() */
    GDALInConstructionAlgorithmArg &SetUserProvided()
    {
        m_decl.SetUserProvided();
        return *this;
    }
};

/************************************************************************/
/*                      GDALAlgorithmRegistry                           */
/************************************************************************/

/** Registry of GDAL algorithms.
 */
class CPL_DLL GDALAlgorithmRegistry
{
  public:
    /** Special value to put in m_aliases to separate public alias from
     * hidden aliases */
    static constexpr const char *HIDDEN_ALIAS_SEPARATOR = "==hide==";

    virtual ~GDALAlgorithmRegistry();

    /** Algorithm information */
    class AlgInfo
    {
      public:
        /** Algorithm (short) name */
        std::string m_name{};
        /** Aliases */
        std::vector<std::string> m_aliases{};
#ifdef DOXYGEN_SKIP
        /** Creation function */
        std::function m_creationFunc{};
#else
        /** Creation function */
        std::function<std::unique_ptr<GDALAlgorithm>(void)> m_creationFunc{};
#endif
    };

    /** Register the algorithm of type MyAlgorithm.
     */
    template <class MyAlgorithm> bool Register()
    {
        AlgInfo info;
        info.m_name = MyAlgorithm::NAME;
        info.m_aliases = MyAlgorithm::GetAliasesStatic();
        info.m_creationFunc = []() -> std::unique_ptr<GDALAlgorithm>
        { return std::make_unique<MyAlgorithm>(); };
        return Register(info);
    }

    /** Register an algorithm by its AlgInfo structure.
     */
    bool Register(const AlgInfo &info);

    /** Get the names of registered algorithms.
     *
     * This only returns the main name of each algorithm, not its potential
     * alternate names.
     */
    std::vector<std::string> GetNames() const;

    /** Instantiate an algorithm by its name or one of its alias.
     * @return algorithm, or nullptr
     */
    std::unique_ptr<GDALAlgorithm> Instantiate(const std::string &name) const;

    /** Instantiate an algorithm by its path.
     * @return algorithm, or nullptr
     * @since 3.12
     */
    std::unique_ptr<GDALAlgorithm>
    Instantiate(const std::vector<std::string> &path) const;

    /** Instantiate an algorithm by its path.
     * @return algorithm, or nullptr
     * @since 3.12
     */
    template <typename... V>
    inline std::unique_ptr<GDALAlgorithm> Instantiate(const std::string &first,
                                                      V &&...rest)
    {
        std::vector<std::string> path;
        return InstantiateInternal(path, first, std::forward<V>(rest)...);
    }

    /** Get an algorithm by its name. */
    const AlgInfo *GetInfo(const std::string &name) const
    {
        auto iter = m_mapNameToInfo.find(name);
        return iter != m_mapNameToInfo.end() ? &(iter->second) : nullptr;
    }

    /** Returns true if there are no algorithms registered. */
    bool empty() const
    {
        return m_mapNameToInfo.empty();
    }

  protected:
    /** Instantiate an algorithm by its name or one of its alias. */
    virtual std::unique_ptr<GDALAlgorithm>
    InstantiateTopLevel(const std::string &name) const;

  private:
    std::map<std::string, AlgInfo> m_mapNameToInfo{};
    std::map<std::string, AlgInfo> m_mapAliasToInfo{};
    std::map<std::string, AlgInfo> m_mapHiddenAliasToInfo{};

    std::unique_ptr<GDALAlgorithm>
    InstantiateInternal(std::vector<std::string> &path);

    template <typename... V>
    std::unique_ptr<GDALAlgorithm>
    InstantiateInternal(std::vector<std::string> &path,
                        const std::string &first, V &&...rest)
    {
        path.push_back(first);
        return InstantiateInternal(path, std::forward<V>(rest)...);
    }
};

/************************************************************************/
/*                            GDALAlgorithm                             */
/************************************************************************/

/** GDAL algorithm.
 *
 * An algorithm declares its name, description, help URL.
 * It also defined arguments or (mutual exclusion) sub-algorithms.
 *
 * It can be used from the command line with the ParseCommandLineArguments()
 * method, or users can iterate over the available arguments with the GetArgs()
 * or GetArg() method and fill them programmatically with
 * GDALAlgorithmArg::Set().
 *
 * Execution of the algorithm is done with the Run() method.
 *
 * This is an abstract class. Implementations must sub-class it and implement the
 * RunImpl() method.
 */

/* abstract */ class CPL_DLL GDALAlgorithm
{
    friend struct test_gdal_algorithm::test_gdal_algorithm;

  public:
    virtual ~GDALAlgorithm();

    /** Get the algorithm name */
    const std::string &GetName() const
    {
        return m_name;
    }

    /** Get the algorithm description (a few sentences at most) */
    const std::string &GetDescription() const
    {
        return m_description;
    }

    /** Get the long algorithm description. May be empty. */
    const std::string &GetLongDescription() const
    {
        return m_longDescription;
    }

    /** Get the algorithm help URL. If starting with '/', it is relative to
     * "https://gdal.org".
     */
    const std::string &GetHelpURL() const
    {
        return m_helpURL;
    }

    /** Get the algorithm full URL, resolving relative URLs. */
    const std::string &GetHelpFullURL() const
    {
        return m_helpFullURL;
    }

    /** Returns whether this algorithm is hidden */
    bool IsHidden() const
    {
        return m_hidden;
    }

    /** Returns whether this algorithm has sub-algorithms */
    bool HasSubAlgorithms() const;

    /** Get the names of registered algorithms.
     *
     * This only returns the main name of each algorithm, not its potential
     * alternate names.
     */
    std::vector<std::string> GetSubAlgorithmNames() const;

    /** Instantiate an algorithm by its name (or its alias). */
    std::unique_ptr<GDALAlgorithm>
    InstantiateSubAlgorithm(const std::string &name,
                            bool suggestionAllowed = true) const;

    /** Return the potential arguments of the algorithm. */
    const std::vector<std::unique_ptr<GDALAlgorithmArg>> &GetArgs() const
    {
        return m_args;
    }

    /** Return the potential arguments of the algorithm. */
    std::vector<std::unique_ptr<GDALAlgorithmArg>> &GetArgs()
    {
        return m_args;
    }

    /** Return a likely matching argument using a Damerau-Levenshtein distance */
    std::string GetSuggestionForArgumentName(const std::string &osName) const;

    /** Return an argument from its long name, short name or an alias */
    GDALAlgorithmArg *GetArg(const std::string &osName,
                             bool suggestionAllowed = false)
    {
        return GetArg(osName, suggestionAllowed, /* isConst = */ false);
    }

    /** Return an argument from its long name, short name or an alias */
    GDALAlgorithmArg &operator[](const std::string &osName)
    {
        auto alg = GetArg(osName, false);
        if (!alg)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Argument '%s' does not exist", osName.c_str());
            return m_dummyArg;
        }
        return *alg;
    }

    /** Return an argument from its long name, short name or an alias */
    const GDALAlgorithmArg *GetArg(const std::string &osName,
                                   bool suggestionAllowed = false) const
    {
        return const_cast<GDALAlgorithm *>(this)->GetArg(
            osName, suggestionAllowed, /* isConst = */ true);
    }

    /** Return an argument from its long name, short name or an alias */
    const GDALAlgorithmArg &operator[](const std::string &osName) const
    {
        const auto alg = GetArg(osName, false);
        if (!alg)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Argument '%s' does not exist", osName.c_str());
            return m_dummyArg;
        }
        return *alg;
    }

    /** Set the calling path to this algorithm.
     *
     * For example the main "gdal" CLI will set the path to the name of its
     * binary before calling ParseCommandLineArguments().
     */
    void SetCallPath(const std::vector<std::string> &path)
    {
        m_callPath = path;
    }

    /** Set hint before calling ParseCommandLineArguments() that it must
     * try to be be graceful when possible, e.g. accepting
     * "gdal raster convert in.tif out.tif --co"
     */
    void SetParseForAutoCompletion()
    {
        m_parseForAutoCompletion = true;
    }

    /** Set the reference file paths used to interpret relative paths.
     *
     * This has only effect if called before calling ParseCommandLineArguments().
     */
    void SetReferencePathForRelativePaths(const std::string &referencePath)
    {
        m_referencePath = referencePath;
    }

    /** Return the reference file paths used to interpret relative paths. */
    const std::string &GetReferencePathForRelativePaths() const
    {
        return m_referencePath;
    }

    /** Returns whether this algorithm supports a streamed output dataset. */
    bool SupportsStreamedOutput() const
    {
        return m_supportsStreamedOutput;
    }

    /** Indicates that the algorithm must be run to generate a streamed output
     * dataset. In particular, this must be used as a hint by algorithms to
     * avoid writing files on the filesystem. This is used by the GDALG driver
     * when executing a serialized algorithm command line.
     *
     * This has only effect if called before calling Run().
     */
    void SetExecutionForStreamedOutput()
    {
        m_executionForStreamOutput = true;
    }

    /** Parse a command line argument, which does not include the algorithm
     * name, to set the value of corresponding arguments.
     */
    virtual bool
    ParseCommandLineArguments(const std::vector<std::string> &args);

    /** Validate that all constraints are met.
     *
     * This method may emit several errors if several constraints are not met.
     *
     * This method is automatically executed by ParseCommandLineArguments()
     * and Run(), and thus does generally not need to be explicitly called.
     * Derived classes overriding this method should generally call the base
     * method.
     */
    virtual bool ValidateArguments();

    /** Execute the algorithm, starting with ValidateArguments() and then
     * calling RunImpl().
     */
    bool Run(GDALProgressFunc pfnProgress = nullptr,
             void *pProgressData = nullptr);

    /** Complete any pending actions, and return the final status.
     * This is typically useful for algorithm that generate an output dataset.
     */
    virtual bool Finalize();

    /** Usage options */
    struct UsageOptions
    {
        /** Whether this is a pipeline step */
        bool isPipelineStep;
        /** Maximum width of the names of the options */
        size_t maxOptLen;
        /** Whether this is a pipeline main */
        bool isPipelineMain;

        UsageOptions()
            : isPipelineStep(false), maxOptLen(0), isPipelineMain(false)
        {
        }
    };

    /** Return the usage as a string appropriate for command-line interface
     * \--help output.
     */
    virtual std::string
    GetUsageForCLI(bool shortUsage,
                   const UsageOptions &usageOptions = UsageOptions()) const;

    /** Return the usage of the algorithm as a JSON-serialized string.
     *
     * This can be used to dynamically generate interfaces to algorithms.
     */
    virtual std::string GetUsageAsJSON() const;

    /** Return the actual algorithm that is going to be invoked, when the
     * current algorithm has sub-algorithms.
     *
     * Only valid after ParseCommandLineArguments() has been called.
     */
    GDALAlgorithm &GetActualAlgorithm()
    {
        if (m_selectedSubAlg)
            return m_selectedSubAlg->GetActualAlgorithm();
        return *this;
    }

    /** Whether the \--help flag has been specified. */
    bool IsHelpRequested() const
    {
        return m_helpRequested;
    }

    /** Whether the \--json-usage flag has been specified. */
    bool IsJSONUsageRequested() const
    {
        return m_JSONUsageRequested;
    }

    /** Whether the \--progress flag has been specified. */
    bool IsProgressBarRequested() const
    {
        if (m_selectedSubAlg)
            return m_selectedSubAlg->IsProgressBarRequested();
        return m_progressBarRequested;
    }

    /** Return alias names (generally short) for the current algorithm. */
    const std::vector<std::string> &GetAliases() const
    {
        return m_aliases;
    }

    //! @cond Doxygen_Suppress
    /** Return alias names. This method should be redefined in derived classes
     * that want to define aliases.
     */
    static std::vector<std::string> GetAliasesStatic()
    {
        return {};
    }

    //! @endcond

    /** Used by the "gdal info" special algorithm when it first tries to
     * run "gdal raster info", to inherit from the potential special flags,
     * such as \--help or \--json-usage, that this later algorithm has received.
     */
    bool PropagateSpecialActionTo(GDALAlgorithm *target)
    {
        target->m_calledFromCommandLine = m_calledFromCommandLine;
        target->m_progressBarRequested = m_progressBarRequested;
        target->m_quiet = m_quiet;
        if (m_specialActionRequested)
        {
            target->m_specialActionRequested = m_specialActionRequested;
            target->m_helpRequested = m_helpRequested;
            target->m_helpDocRequested = m_helpDocRequested;
            target->m_JSONUsageRequested = m_JSONUsageRequested;
            return true;
        }
        return false;
    }

    /** Return auto completion suggestions */
    virtual std::vector<std::string>
    GetAutoComplete(std::vector<std::string> &args, bool lastWordIsComplete,
                    bool showAllOptions);

    /** Set whether the algorithm is called from the command line. */
    void SetCalledFromCommandLine()
    {
        m_calledFromCommandLine = true;
    }

    /** Return whether the algorithm is called from the command line. */
    bool IsCalledFromCommandLine() const
    {
        return m_calledFromCommandLine;
    }

    /** Whether the algorithm generates an output string */
    virtual bool HasOutputString() const;

    /** Save command line in a .gdalg.json file.
     * If filename is empty, outString will contain the serialized JSON content.
     */
    static bool SaveGDALG(const std::string &filename, std::string &outString,
                          const std::string &commandLine);

    //! @cond Doxygen_Suppress
    void ReportError(CPLErr eErrClass, CPLErrorNum err_no, const char *fmt,
                     ...) const CPL_PRINT_FUNC_FORMAT(4, 5);
    //! @endcond

  protected:
    friend class GDALInConstructionAlgorithmArg;
    friend class GDALRasterReprojectUtils;

    /** Selected sub-algorithm. Set by ParseCommandLineArguments() when
     * handling over on a sub-algorithm. */
    GDALAlgorithm *m_selectedSubAlg = nullptr;

    /** Call path to the current algorithm. For example, for "gdal convert raster",
     * it is ["gdal", "convert"]
     */
    std::vector<std::string> m_callPath{};

    /** Long description of the algorithm */
    std::string m_longDescription{};

    /** Whether a progress bar is requested (value of \--progress argument) */
    bool m_progressBarRequested = true;

    /** Whether a progress bar is disabled (value of \--quiet argument) */
    bool m_quiet = false;

    /** Whether ValidateArguments() should be skipped during ParseCommandLineArguments() */
    bool m_skipValidationInParseCommandLine = false;

    friend class GDALAlgorithmRegistry;  // to set m_aliases
    /** Algorithm alias names */
    std::vector<std::string> m_aliases{};

    /** Whether this algorithm supports a streamed output dataset. */
    bool m_supportsStreamedOutput = false;

    /** Whether this algorithm is run to generated a streamed output dataset. */
    bool m_executionForStreamOutput = false;

    /** Whether this algorithm should be hidden (but can be instantiate if name known) */
    bool m_hidden = false;

    /** Map a dataset name to its object (used for nested pipelines) */
    std::map<std::string, GDALDataset *> m_oMapDatasetNameToDataset{};

    /** Constructor */
    GDALAlgorithm(const std::string &name, const std::string &description,
                  const std::string &helpURL);

    /** Special processing for an argument of type GAAT_DATASET */
    bool ProcessDatasetArg(GDALAlgorithmArg *arg, GDALAlgorithm *algForOutput);

    /** Register the sub-algorithm of type MyAlgorithm.
     */
    template <class MyAlgorithm> bool RegisterSubAlgorithm()
    {
        return m_subAlgRegistry.Register<MyAlgorithm>();
    }

    /** Register a sub-algoritm by its AlgInfo structure.
     */
    bool RegisterSubAlgorithm(const GDALAlgorithmRegistry::AlgInfo &info)
    {
        return m_subAlgRegistry.Register(info);
    }

    /** Allow arbitrary user arguments using long name syntax (--something) */
    void AllowArbitraryLongNameArgs()
    {
        m_arbitraryLongNameArgsAllowed = true;
    }

    /** Add boolean argument. */
    GDALInConstructionAlgorithmArg &AddArg(const std::string &longName,
                                           char chShortName,
                                           const std::string &helpMessage,
                                           bool *pValue);

    /** Add string argument. */
    GDALInConstructionAlgorithmArg &AddArg(const std::string &longName,
                                           char chShortName,
                                           const std::string &helpMessage,
                                           std::string *pValue);

    /** Add integer argument. */
    GDALInConstructionAlgorithmArg &AddArg(const std::string &longName,
                                           char chShortName,
                                           const std::string &helpMessage,
                                           int *pValue);

    /** Add real argument. */
    GDALInConstructionAlgorithmArg &AddArg(const std::string &longName,
                                           char chShortName,
                                           const std::string &helpMessage,
                                           double *pValue);

    /** Register an auto complete function for a filename argument */
    static void
    SetAutoCompleteFunctionForFilename(GDALInConstructionAlgorithmArg &arg,
                                       GDALArgDatasetType type);

    /** Add dataset argument. */
    GDALInConstructionAlgorithmArg &
    AddArg(const std::string &longName, char chShortName,
           const std::string &helpMessage, GDALArgDatasetValue *pValue,
           GDALArgDatasetType type = GDAL_OF_RASTER | GDAL_OF_VECTOR |
                                     GDAL_OF_MULTIDIM_RASTER);

    /** Add list of string argument. */
    GDALInConstructionAlgorithmArg &AddArg(const std::string &longName,
                                           char chShortName,
                                           const std::string &helpMessage,
                                           std::vector<std::string> *pValue);

    /** Add list of integer argument. */
    GDALInConstructionAlgorithmArg &AddArg(const std::string &longName,
                                           char chShortName,
                                           const std::string &helpMessage,
                                           std::vector<int> *pValue);

    /** Add list of real argument. */
    GDALInConstructionAlgorithmArg &AddArg(const std::string &longName,
                                           char chShortName,
                                           const std::string &helpMessage,
                                           std::vector<double> *pValue);

    /** Add list of dataset argument. */
    GDALInConstructionAlgorithmArg &
    AddArg(const std::string &longName, char chShortName,
           const std::string &helpMessage,
           std::vector<GDALArgDatasetValue> *pValue,
           GDALArgDatasetType type = GDAL_OF_RASTER | GDAL_OF_VECTOR |
                                     GDAL_OF_MULTIDIM_RASTER);

    /** Add input dataset argument. */
    GDALInConstructionAlgorithmArg &AddInputDatasetArg(
        GDALArgDatasetValue *pValue,
        GDALArgDatasetType type = GDAL_OF_RASTER | GDAL_OF_VECTOR |
                                  GDAL_OF_MULTIDIM_RASTER,
        bool positionalAndRequired = true, const char *helpMessage = nullptr);

    /** Add input dataset argument. */
    GDALInConstructionAlgorithmArg &AddInputDatasetArg(
        std::vector<GDALArgDatasetValue> *pValue,
        GDALArgDatasetType type = GDAL_OF_RASTER | GDAL_OF_VECTOR |
                                  GDAL_OF_MULTIDIM_RASTER,
        bool positionalAndRequired = true, const char *helpMessage = nullptr);

    /** Add (input) open option(s) argument. */
    GDALInConstructionAlgorithmArg &
    AddOpenOptionsArg(std::vector<std::string> *pValue,
                      const char *helpMessage = nullptr);

    /** Add output open option(s) argument. */
    GDALInConstructionAlgorithmArg &
    AddOutputOpenOptionsArg(std::vector<std::string> *pValue,
                            const char *helpMessage = nullptr);

    /** Add input format(s) argument. */
    GDALInConstructionAlgorithmArg &
    AddInputFormatsArg(std::vector<std::string> *pValue,
                       const char *helpMessage = nullptr);

    /** Add output dataset argument. */
    GDALInConstructionAlgorithmArg &AddOutputDatasetArg(
        GDALArgDatasetValue *pValue,
        GDALArgDatasetType type = GDAL_OF_RASTER | GDAL_OF_VECTOR |
                                  GDAL_OF_MULTIDIM_RASTER,
        bool positionalAndRequired = true, const char *helpMessage = nullptr);

    /** Add \--overwrite argument. */
    GDALInConstructionAlgorithmArg &
    AddOverwriteArg(bool *pValue, const char *helpMessage = nullptr);

    /** Add \--overwrite-layer argument. */
    GDALInConstructionAlgorithmArg &
    AddOverwriteLayerArg(bool *pValue, const char *helpMessage = nullptr);

    /** Add \--update argument. */
    GDALInConstructionAlgorithmArg &
    AddUpdateArg(bool *pValue, const char *helpMessage = nullptr);

    /** Add \--append argument for a vector layer. */
    GDALInConstructionAlgorithmArg &
    AddAppendLayerArg(bool *pValue, const char *helpMessage = nullptr);

    /** Add (non-CLI) output-string argument. */
    GDALInConstructionAlgorithmArg &
    AddOutputStringArg(std::string *pValue, const char *helpMessage = nullptr);

    /** Add (hidden) stdout argument. */
    GDALInConstructionAlgorithmArg &
    AddStdoutArg(bool *pValue, const char *helpMessage = nullptr);

    /** Add output format argument. */
    GDALInConstructionAlgorithmArg &
    AddOutputFormatArg(std::string *pValue, bool bStreamAllowed = false,
                       bool bGDALGAllowed = false,
                       const char *helpMessage = nullptr);

    /** Add output data type argument. */
    GDALInConstructionAlgorithmArg &
    AddOutputDataTypeArg(std::string *pValue,
                         const char *helpMessage = nullptr);

    /** Add nodata argument. */
    GDALInConstructionAlgorithmArg &
    AddNodataArg(std::string *pValue, bool noneAllowed,
                 const std::string &optionName = "nodata",
                 const char *helpMessage = nullptr);

    /** Add creation option(s) argument. */
    GDALInConstructionAlgorithmArg &
    AddCreationOptionsArg(std::vector<std::string> *pValue,
                          const char *helpMessage = nullptr);

    /** Add layer creation option(s) argument. */
    GDALInConstructionAlgorithmArg &
    AddLayerCreationOptionsArg(std::vector<std::string> *pValue,
                               const char *helpMessage = nullptr);

    /** Add (single) layer name argument. */
    GDALInConstructionAlgorithmArg &
    AddLayerNameArg(std::string *pValue, const char *helpMessage = nullptr);

    /** Add (single) output layer name argument. */
    GDALInConstructionAlgorithmArg &
    AddOutputLayerNameArg(std::string *pValue,
                          const char *helpMessage = nullptr);

    /** Add (potentially multiple) layer name(s) argument. */
    GDALInConstructionAlgorithmArg &
    AddLayerNameArg(std::vector<std::string> *pValue,
                    const char *helpMessage = nullptr);

    /** Add (single) (multidimensional) array name argument. */
    GDALInConstructionAlgorithmArg &
    AddArrayNameArg(std::string *pValue, const char *helpMessage = nullptr);

    /** Add (multiple) (multidimensional) array name argument. */
    GDALInConstructionAlgorithmArg &
    AddArrayNameArg(std::vector<std::string> *pValue,
                    const char *helpMessage = nullptr);

    /** Add a memory size argument(s), The final value is stored in *pValue.
     * pStrValue must be provided as temporary storage, and its initial value
     * (if not empty) is used as the SetDefault() value.
     */
    GDALInConstructionAlgorithmArg &
    AddMemorySizeArg(size_t *pValue, std::string *pStrValue,
                     const std::string &optionName, const char *helpMessage);

    /** Add geometry type argument */
    GDALInConstructionAlgorithmArg &
    AddGeometryTypeArg(std::string *pValue, const char *helpMessage = nullptr);

    /** Register an auto complete function for a layer name argument */
    static void SetAutoCompleteFunctionForLayerName(
        GDALInConstructionAlgorithmArg &layerArg, GDALAlgorithmArg &datasetArg);

    /** Register an auto complete function for a field name argument */
    static void SetAutoCompleteFunctionForFieldName(
        GDALInConstructionAlgorithmArg &fieldArg,
        GDALInConstructionAlgorithmArg &layerNameArg,
        std::vector<GDALArgDatasetValue> &datasetArg);

    /** Add a field name argument */
    GDALInConstructionAlgorithmArg &
    AddFieldNameArg(std::string *pValue, const char *helpMessage = nullptr);

    /** Add a field type (or subtype) argument */
    GDALInConstructionAlgorithmArg &AddFieldTypeSubtypeArg(
        OGRFieldType *pTypeValue, OGRFieldSubType *pSubtypeValue,
        std::string *pStrValue, const std::string &argName = std::string(),
        const char *helpMessage = nullptr);

    /** Add (single) band argument. */
    GDALInConstructionAlgorithmArg &
    AddBandArg(int *pValue, const char *helpMessage = nullptr);

    /** Add (potentially multiple) band argument. */
    GDALInConstructionAlgorithmArg &
    AddBandArg(std::vector<int> *pValue, const char *helpMessage = nullptr);

    /** Add bbox=xmin,ymin,xmax,ymax argument. */
    GDALInConstructionAlgorithmArg &
    AddBBOXArg(std::vector<double> *pValue, const char *helpMessage = nullptr);

    /** Add active layer argument. */
    GDALInConstructionAlgorithmArg &
    AddActiveLayerArg(std::string *pValue, const char *helpMessage = nullptr);

    /** A number of thread argument. The final value is stored in *pValue.
     * pStrValue must be provided as temporary storage, and its initial value
     * (if not empty) is used as the SetDefault() value.
     */
    GDALInConstructionAlgorithmArg &
    AddNumThreadsArg(int *pValue, std::string *pStrValue,
                     const char *helpMessage = nullptr);

    /** Add an argument to ask writing absolute paths. */
    GDALInConstructionAlgorithmArg &
    AddAbsolutePathArg(bool *pValue, const char *helpMessage = nullptr);

    /** Add an argument for pixel function name */
    GDALInConstructionAlgorithmArg &
    AddPixelFunctionNameArg(std::string *pValue,
                            const char *helpMessage = nullptr);

    /** Add an argument for pixel function arguments */
    GDALInConstructionAlgorithmArg &
    AddPixelFunctionArgsArg(std::vector<std::string> *pValue,
                            const char *helpMessage = nullptr);

    /** Add \--quiet (and hidden \--progress) argument. */
    void AddProgressArg();

    /** Register an action that is executed by the ValidateArguments()
     * method. If the provided function returns false, validation fails.
     * Such validation function should typically be used to ensure
     * cross-argument validation. For validation of individual arguments,
     * GDALAlgorithmArg::AddValidationAction should rather be called.
     */
    void AddValidationAction(std::function<bool()> f)
    {
        m_validationActions.push_back(f);
    }

    /** Add KEY=VALUE suggestion from open, creation options */
    static bool AddOptionsSuggestions(const char *pszXML, int datasetType,
                                      const std::string &currentValue,
                                      std::vector<std::string> &oRet);

    /** Validation function to use for key=value type of arguments. */
    bool ParseAndValidateKeyValue(GDALAlgorithmArg &arg);

    /** Method used by GDALRaster|VectorPipelineAlgorithm */
    bool RunPreStepPipelineValidations() const;

    /** Return whether output-format or output arguments express GDALG output */
    bool IsGDALGOutput() const;

    /** Return value for ProcessGDALGOutput */
    enum class ProcessGDALGOutputRet
    {
        /** GDALG output requested and successful. */
        GDALG_OK,
        /** GDALG output requested but an error has occurred. */
        GDALG_ERROR,
        /** GDALG output not requeste. RunImpl() must be run. */
        NOT_GDALG,
    };

    /** Process output to a .gdalg file */
    virtual ProcessGDALGOutputRet ProcessGDALGOutput();

    /** Method executed by Run() when m_executionForStreamOutput is set to
     * ensure the command is safe to execute in a streamed dataset context.
     */
    virtual bool CheckSafeForStreamOutput();

    /** Validate a format argument */
    bool ValidateFormat(const GDALAlgorithmArg &arg, bool bStreamAllowed,
                        bool bGDALGAllowed) const;

    /** Completion function for a format argument */
    static std::vector<std::string>
    FormatAutoCompleteFunction(const GDALAlgorithmArg &arg, bool bStreamAllowed,
                               bool bGDALGAllowed);

    //! @cond Doxygen_Suppress
    void AddAliasFor(GDALInConstructionAlgorithmArg *arg,
                     const std::string &alias);

    void AddShortNameAliasFor(GDALInConstructionAlgorithmArg *arg,
                              char shortNameAlias);

    void SetPositional(GDALInConstructionAlgorithmArg *arg);

    std::vector<std::string>
    OpenOptionCompleteFunction(const std::string &currentValue) const;

    //! @endcond

    /** Whether this argument name is the one of a well-known boolean argument */
    static bool IsKnownOutputRelatedBooleanArgName(std::string_view osName);

    /** Set whether this algorithm should be reported in JSON usage. */
    void SetDisplayInJSONUsage(bool b)
    {
        m_displayInJSONUsage = b;
    }

    /** Method that an algorithm can implement to issue a warning message about
     * its deprecation. This is called at the beginning of the Run() method.
     */
    virtual void WarnIfDeprecated()
    {
    }

    /** Return the list of arguments for CLI usage */
    std::pair<std::vector<std::pair<GDALAlgorithmArg *, std::string>>, size_t>
    GetArgNamesForCLI() const;

    //! @cond Doxygen_Suppress
    std::string GetUsageForCLIEnd() const;
    //! @endcond

  private:
    const std::string m_name{};
    const std::string m_description{};
    const std::string m_helpURL{};
    const std::string m_helpFullURL{};
    bool m_parsedSubStringAlreadyCalled = false;
    bool m_displayInJSONUsage = true;
    bool m_specialActionRequested = false;
    bool m_helpRequested = false;
    bool m_calledFromCommandLine = false;

    // Used by program-output directives in .rst files
    bool m_helpDocRequested = false;

    bool m_JSONUsageRequested = false;
    bool m_parseForAutoCompletion = false;
    std::string m_referencePath{};
    std::vector<std::string> m_dummyConfigOptions{};
    std::vector<std::unique_ptr<GDALAlgorithmArg>> m_args{};
    std::map<std::string, GDALAlgorithmArg *> m_mapLongNameToArg{};
    std::map<std::string, GDALAlgorithmArg *> m_mapShortNameToArg{};
    std::vector<GDALAlgorithmArg *> m_positionalArgs{};
    GDALAlgorithmRegistry m_subAlgRegistry{};
    std::unique_ptr<GDALAlgorithm> m_selectedSubAlgHolder{};
    std::function<std::vector<std::string>(const std::vector<std::string> &)>
        m_autoCompleteFunction{};
    std::vector<std::function<bool()>> m_validationActions{};

    std::string m_dummyVal{};
    GDALAlgorithmArg m_dummyArg{
        GDALAlgorithmArgDecl("dummy", 0, "", GAAT_STRING), &m_dummyVal};

    /** Whether arbitrary user arguments using long name syntax (--something)
     * are allowed.
     */
    bool m_arbitraryLongNameArgsAllowed = false;

    std::vector<std::unique_ptr<std::string>>
        m_arbitraryLongNameArgsValuesStr{};
    std::vector<std::unique_ptr<bool>> m_arbitraryLongNameArgsValuesBool{};

    friend GDALAlgorithmArgH GDALAlgorithmGetArg(GDALAlgorithmH hAlg,
                                                 const char *pszArgName);
    friend GDALAlgorithmArgH
    GDALAlgorithmGetArgNonConst(GDALAlgorithmH hAlg, const char *pszArgName);
    GDALAlgorithmArg *GetArg(const std::string &osName, bool suggestionAllowed,
                             bool isConst);

    GDALInConstructionAlgorithmArg &
    AddArg(std::unique_ptr<GDALInConstructionAlgorithmArg> arg);
    bool ParseArgument(
        GDALAlgorithmArg *arg, const std::string &name,
        const std::string &value,
        std::map<
            GDALAlgorithmArg *,
            std::variant<std::vector<std::string>, std::vector<int>,
                         std::vector<double>, std::vector<GDALArgDatasetValue>>>
            &inConstructionValues);

    bool ValidateBandArg() const;

    virtual bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) = 0;

    /** Extract the last option and its potential value from the provided
     * argument list, and remove them from the list.
     */
    void ExtractLastOptionAndValue(std::vector<std::string> &args,
                                   std::string &option,
                                   std::string &value) const;

    std::vector<std::string> AutoCompleteArrayName() const;

    GDALAlgorithm(const GDALAlgorithm &) = delete;
    GDALAlgorithm &operator=(const GDALAlgorithm &) = delete;
};

//! @cond Doxygen_Suppress
struct GDALAlgorithmHS
{
  private:
    std::unique_ptr<GDALAlgorithm> uniquePtr{};

    GDALAlgorithmHS(const GDALAlgorithmHS &) = delete;
    GDALAlgorithmHS &operator=(const GDALAlgorithmHS &) = delete;

  public:
    GDALAlgorithm *ptr = nullptr;

    GDALAlgorithmHS() = default;

    explicit GDALAlgorithmHS(std::unique_ptr<GDALAlgorithm> alg)
        : uniquePtr(std::move(alg)), ptr(uniquePtr.get())
    {
    }

    static std::unique_ptr<GDALAlgorithmHS> FromRef(GDALAlgorithm &alg)
    {
        auto ret = std::make_unique<GDALAlgorithmHS>();
        ret->ptr = &alg;
        return ret;
    }
};

/************************************************************************/
/*                       GDALContainerAlgorithm                         */
/************************************************************************/

class CPL_DLL GDALContainerAlgorithm : public GDALAlgorithm
{
  public:
    explicit GDALContainerAlgorithm(
        const std::string &name, const std::string &description = std::string(),
        const std::string &helpURL = std::string())
        : GDALAlgorithm(name, description, helpURL)
    {
    }

  protected:
    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

/************************************************************************/
/*                   GDALGlobalAlgorithmRegistry                        */
/************************************************************************/

/** Global registry of GDAL algorithms.
 */
class CPL_DLL GDALGlobalAlgorithmRegistry final : public GDALAlgorithmRegistry
{
  public:
    /** Name of the root "gdal" algorithm. */
    static constexpr const char *ROOT_ALG_NAME = "gdal";

    /** Get the singleton */
    static GDALGlobalAlgorithmRegistry &GetSingleton();

    /** Instantiation function */
    using InstantiateFunc = std::function<std::unique_ptr<GDALAlgorithm>()>;

    /** Declare the algorithm designed by its path (omitting leading path)
     * and provide its instantiation method.
     * For example {"driver", "pdf", "list-layers"}
     *
     * This is typically used by plugins to register extra algorithms.
     */
    void DeclareAlgorithm(const std::vector<std::string> &path,
                          InstantiateFunc instantiateFunc);

    /** Return the direct declared (as per DeclareAlgorithm()) subalgorithms
     * of the given path. */
    std::vector<std::string>
    GetDeclaredSubAlgorithmNames(const std::vector<std::string> &path) const;

    /** Return whether a subalgorithm is declared at the given path. */
    bool HasDeclaredSubAlgorithm(const std::vector<std::string> &path) const;

    /** Instantiate a declared (as per DeclareAlgorithm()) subalgorithm. */
    std::unique_ptr<GDALAlgorithm>
    InstantiateDeclaredSubAlgorithm(const std::vector<std::string> &path) const;

  protected:
    std::unique_ptr<GDALAlgorithm>
    InstantiateTopLevel(const std::string &name) const override;

  private:
    struct Node
    {
        InstantiateFunc instantiateFunc{};
        std::map<std::string, Node> children{};
    };

    Node m_root{};

    GDALGlobalAlgorithmRegistry();
    ~GDALGlobalAlgorithmRegistry() override;

    const Node *GetNodeFromPath(const std::vector<std::string> &path) const;
};

#endif  // #if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS) && (defined(DOXYGEN_SKIP) || __cplusplus >= 201703L || _MSC_VER >= 1920)

#endif  // GDAL_ALGORITHM_CPP_INCLUDED
