/******************************************************************************
 * Project:  GDAL Utilities
 * Purpose:  GDAL argument parser
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALARGUMENTPARSER_H
#define GDALARGUMENTPARSER_H

#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal.h"

// Rename argparse namespace to a GDAL private one
#define argparse gdal_argparse

// Use our locale-unaware strtod()
#define ARGPARSE_CUSTOM_STRTOD CPLStrtodM

#ifdef _MSC_VER
#pragma warning(push)
// unreachable code
#pragma warning(disable : 4702)
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif

#include "argparse/argparse.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

using namespace argparse;

// Place-holder macro using gettext() convention to indicate (future) translatable strings
#ifndef _
#define _(x) (x)
#endif

/** Parse command-line arguments for GDAL utilities.
 *
 * Add helpers over the standard argparse::ArgumentParser class
 *
 * @since GDAL 3.9
 */
class GDALArgumentParser : public ArgumentParser
{
  public:
    //! Constructor
    explicit GDALArgumentParser(const std::string &program_name,
                                bool bForBinary);

    //! Return usage message
    std::string usage() const;

    //! Adds an extra usage hint.
    void add_extra_usage_hint(const std::string &osExtraUsageHint);

    //! Format an exception as an error message and display the program usage
    void display_error_and_usage(const std::exception &err);

    //! Add -q/\--quiet argument, and store its value in *pVar (if pVar not null)
    Argument &add_quiet_argument(bool *pVar);

    //! Add "-if format_name" argument for input format, and store its value into *pvar.
    Argument &add_input_format_argument(CPLStringList *pvar);

    //! Add "-of format_name" argument for output format, and store its value into var.
    Argument &add_output_format_argument(std::string &var);

    //! Add "-co KEY=VALUE" argument for creation options, and store its value into var.
    Argument &add_creation_options_argument(CPLStringList &var);

    //! Add "-mo KEY=VALUE" argument for metadata item options, and store its value into var.
    Argument &add_metadata_item_options_argument(CPLStringList &var);

    //! Add "-oo KEY=VALUE" argument for open options, and store its value into var.
    Argument &add_open_options_argument(CPLStringList &var);

    //! Add "-oo KEY=VALUE" argument for open options, and store its value into *pvar.
    Argument &add_open_options_argument(CPLStringList *pvar);

    //! Add "-ot data_type" argument for output type, and store its value into eDT.
    Argument &add_output_type_argument(GDALDataType &eDT);

    //! Add "-lco NAME=VALUE" argument for layer creation options, and store its value into var.
    Argument &add_layer_creation_options_argument(CPLStringList &var);

    //! Add "-dsco NAME=VALUE" argument for dataset creation options, and store its value into var.
    Argument &add_dataset_creation_options_argument(CPLStringList &var);

    //! Parse command line arguments, without the initial program name.
    void parse_args_without_binary_name(CSLConstList papszArgs);

    //! Parse command line arguments, with the initial program name.
    void parse_args(const CPLStringList &aosArgs);

    //! Return the non positional arguments.
    CPLStringList get_non_positional_arguments(const CPLStringList &aosArgs);

    /**
     * Add an inverted logic (default true, false when set) flag
     * @param name        flag name
     * @param store_into  optional pointer to a bool variable where to store the value
     * @param help        optional help text
     */
    Argument &add_inverted_logic_flag(const std::string &name,
                                      bool *store_into = nullptr,
                                      const std::string &help = "");

    /**
     * Create and add a subparser to the argument parser, keeping ownership
     * @param description   Subparser description
     * @param bForBinary    True if the subparser is for a binary utility, false for a library
     * @return              A pointer to the created subparser
     */
    GDALArgumentParser *add_subparser(const std::string &description,
                                      bool bForBinary);

    /**
     * Get a subparser by name (case insensitive)
     * @param name          Subparser name
     * @return              The subparser or nullptr if not found
     */
    GDALArgumentParser *get_subparser(const std::string &name);

    /**
     * Return true if the argument is used in the command line (also checking subparsers, if any)
     * @param name      Argument name
     * @return          True if the argument is used, false if it is not used.
     * @note            Opposite to the is_used() function this is case insensitive, also checks subparsers and never throws
     */
    bool is_used_globally(const std::string &name);

  private:
    std::map<std::string, ArgumentParser::argument_it>::iterator
    find_argument(const std::string &name);
    std::vector<std::unique_ptr<GDALArgumentParser>> aoSubparsers{};
    std::string m_osExtraUsageHint{};
};

#endif /* GDALARGUMENTPARSER_H */
