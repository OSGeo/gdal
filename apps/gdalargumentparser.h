/******************************************************************************
 * Project:  GDAL Utilities
 * Purpose:  GDAL argument parser
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef GDALARGUMENTPARSER_H
#define GDALARGUMENTPARSER_H

#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_string.h"

// Rename argparse namespace to a GDAL private one
#define argparse gdal_argparse

// Use our locale-unaware strtod()
#define ARGPARSE_CUSTOM_STRTOD CPLStrtodM

#include "argparse/argparse.hpp"

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

    //! Format an exception as an error message and display the program usage
    void display_error_and_usage(const std::exception &err);

    //! Add -q/--quiet argument, and store its value in *pVar (if pVar not null)
    void add_quiet_argument(bool *pVar);

    //! Add "-if format_name" argument for input format, and store its value into *pvar.
    void add_input_format_argument(CPLStringList *pvar);

    //! Add "-of format_name" argument for output format, and store its value into var.
    void add_output_format_argument(std::string &var);

    //! Add "-co KEY=VALUE" argument for creation options, and store its value into var.
    void add_creation_options_argument(CPLStringList &var);

    //! Add "-mo KEY=VALUE" argument for metadata item options, and store its value into var.
    void add_metadata_item_options_argument(CPLStringList &var);

    //! Add "-oo KEY=VALUE" argument for open options, and store its value into var.
    void add_open_options_argument(CPLStringList &var);

    //! Add "-oo KEY=VALUE" argument for open options, and store its value into *pvar.
    void add_open_options_argument(CPLStringList *pvar);

    //! Add "-ot data_type" argument for output type, and store its value into eDT.
    void add_output_type_argument(GDALDataType &eDT);

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

  private:
    std::map<std::string, ArgumentParser::argument_it>::iterator
    find_argument(const std::string &name);
};

#endif /* GDALARGUMENTPARSER_H */
