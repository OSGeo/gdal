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

#include "gdal_version_full/gdal_version.h"

#include "gdal.h"
#include "gdalargumentparser.h"
#include "commonutils.h"

#include <algorithm>

/************************************************************************/
/*                         GDALArgumentParser()                         */
/************************************************************************/

GDALArgumentParser::GDALArgumentParser(const std::string &program_name,
                                       bool bForBinary)
    : ArgumentParser(program_name, "", default_arguments::none)
{
    set_usage_max_line_width(120);
    set_usage_break_on_mutex();
    add_usage_newline();

    if (bForBinary)
    {
        add_argument("-h", "--help")
            .flag()
            .action(
                [this, program_name](const auto &)
                {
                    std::cout << usage() << std::endl << std::endl;
                    std::cout << _("Note: ") << program_name
                              << _(" --long-usage for full help.") << std::endl;
                    std::exit(0);
                })
            .help(_("Shows short help message and exits."));

        add_argument("--long-usage")
            .flag()
            .action(
                [this](const auto & /*unused*/)
                {
                    std::cout << *this;
                    std::exit(0);
                })
            .help(_("Shows long help message and exits."));

        add_argument("--help-general")
            .flag()
            .help(_("Report detailed help on general options."));

        add_argument("--utility_version")
            .flag()
            .hidden()
            .action(
                [program_name](const auto &)
                {
                    printf("%s was compiled against GDAL %s and "
                           "is running against GDAL %s\n",
                           program_name.c_str(), GDAL_RELEASE_NAME,
                           GDALVersionInfo("RELEASE_NAME"));
                    std::exit(0);
                })
            .help(_("Shows compile-time and run-time GDAL version."));

        add_usage_newline();
    }
}

/************************************************************************/
/*                      display_error_and_usage()                       */
/************************************************************************/

void GDALArgumentParser::display_error_and_usage(const std::exception &err)
{
    std::cerr << _("Error: ") << err.what() << std::endl;
    std::cerr << usage() << std::endl << std::endl;
    std::cout << _("Note: ") << m_program_name
              << _(" --long-usage for full help.") << std::endl;
}

/************************************************************************/
/*                         add_quiet_argument()                         */
/************************************************************************/

void GDALArgumentParser::add_quiet_argument(bool *pVar)
{
    auto &arg =
        this->add_argument("-q", "--quiet")
            .flag()
            .help(
                _("Quiet mode. No progress message is emitted on the standard "
                  "output."));
    if (pVar)
        arg.store_into(*pVar);
}

/************************************************************************/
/*                      add_input_format_argument()                     */
/************************************************************************/

void GDALArgumentParser::add_input_format_argument(CPLStringList *pvar)
{
    add_argument("-if")
        .append()
        .metavar("<format>")
        .action(
            [pvar](const std::string &s)
            {
                if (pvar)
                {
                    if (GDALGetDriverByName(s.c_str()) == nullptr)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "%s is not a recognized driver", s.c_str());
                    }
                    pvar->AddString(s.c_str());
                }
            })
        .help(
            _("Format/driver name(s) to be attempted to open the input file."));
}

/************************************************************************/
/*                      add_output_format_argument()                    */
/************************************************************************/

void GDALArgumentParser::add_output_format_argument(std::string &var)
{
    auto &arg = add_argument("-of")
                    .metavar("<output_format>")
                    .store_into(var)
                    .help(_("Output format."));
    add_hidden_alias_for(arg, "-f");
}

/************************************************************************/
/*                     add_creation_options_argument()                  */
/************************************************************************/

void GDALArgumentParser::add_creation_options_argument(CPLStringList &var)
{
    add_argument("-co")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([&var](const std::string &s) { var.AddString(s.c_str()); })
        .help(_("Creation option(s)."));
}

/************************************************************************/
/*                   add_metadata_item_options_argument()               */
/************************************************************************/

void GDALArgumentParser::add_metadata_item_options_argument(CPLStringList &var)
{
    add_argument("-mo")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([&var](const std::string &s) { var.AddString(s.c_str()); })
        .help(_("Metadata item option(s)."));
}

/************************************************************************/
/*                       add_open_options_argument()                    */
/************************************************************************/

void GDALArgumentParser::add_open_options_argument(CPLStringList &var)
{
    add_open_options_argument(&var);
}

/************************************************************************/
/*                       add_open_options_argument()                    */
/************************************************************************/

void GDALArgumentParser::add_open_options_argument(CPLStringList *pvar)
{
    auto &arg = add_argument("-oo")
                    .metavar("<NAME>=<VALUE>")
                    .append()
                    .help(_("Open option(s) for input dataset."));
    if (pvar)
    {
        arg.action([pvar](const std::string &s)
                   { pvar->AddString(s.c_str()); });
    }
}

/************************************************************************/
/*                       add_output_type_argument()                     */
/************************************************************************/

void GDALArgumentParser::add_output_type_argument(GDALDataType &eDT)
{
    add_argument("-ot")
        .metavar("Byte|Int8|[U]Int{16|32|64}|CInt{16|32}|[C]Float{32|64}")
        .action(
            [&eDT](const std::string &s)
            {
                eDT = GDALGetDataTypeByName(s.c_str());
                if (eDT == GDT_Unknown)
                {
                    throw std::invalid_argument(
                        std::string("Unknown output pixel type: ").append(s));
                }
            })
        .help(_("Output data type."));
}

/************************************************************************/
/*                     parse_args_without_binary_name()                 */
/************************************************************************/

void GDALArgumentParser::parse_args_without_binary_name(CSLConstList papszArgs)
{
    CPLStringList aosArgs;
    aosArgs.AddString(m_program_name.c_str());
    for (CSLConstList papszIter = papszArgs; papszIter && *papszIter;
         ++papszIter)
        aosArgs.AddString(*papszIter);
    parse_args(aosArgs);
}

/************************************************************************/
/*                           find_argument()                            */
/************************************************************************/

std::map<std::string, ArgumentParser::argument_it>::iterator
GDALArgumentParser::find_argument(const std::string &name)
{
    auto arg_map_it = m_argument_map.find(name);
    if (arg_map_it == m_argument_map.end())
    {
        // Attempt case insensitive lookup
        arg_map_it =
            std::find_if(m_argument_map.begin(), m_argument_map.end(),
                         [&name](const auto &oArg)
                         { return EQUAL(name.c_str(), oArg.first.c_str()); });
    }
    return arg_map_it;
}

/************************************************************************/
/*                    get_non_positional_arguments()                    */
/************************************************************************/

CPLStringList
GDALArgumentParser::get_non_positional_arguments(const CPLStringList &aosArgs)
{
    CPLStringList args;

    // Simplified logic borrowed from ArgumentParser::parse_args_internal()
    // that make sure that positional arguments are moved after optional ones,
    // as this is what ArgumentParser::parse_args() only supports.
    // This doesn't support advanced settings, such as sub-parsers or compound
    // argument
    std::vector<std::string> raw_arguments{m_program_name};
    raw_arguments.insert(raw_arguments.end(), aosArgs.List(),
                         aosArgs.List() + aosArgs.size());
    auto arguments = preprocess_arguments(raw_arguments);
    auto end = std::end(arguments);
    auto positional_argument_it = std::begin(m_positional_arguments);
    for (auto it = std::next(std::begin(arguments)); it != end;)
    {
        const auto &current_argument = *it;
        if (Argument::is_positional(current_argument, m_prefix_chars))
        {
            if (positional_argument_it != std::end(m_positional_arguments))
            {
                auto argument = positional_argument_it++;
                auto next_it =
                    argument->consume(it, end, "", /* dry_run = */ true);
                it = next_it;
                continue;
            }
            else
            {
                if (m_positional_arguments.empty())
                {
                    throw std::runtime_error(
                        "Zero positional arguments expected");
                }
                else
                {
                    throw std::runtime_error(
                        "Maximum number of positional arguments "
                        "exceeded, failed to parse '" +
                        current_argument + "'");
                }
            }
        }

        auto arg_map_it = find_argument(current_argument);
        if (arg_map_it != m_argument_map.end())
        {
            auto argument = arg_map_it->second;
            auto next_it = argument->consume(
                std::next(it), end, arg_map_it->first, /* dry_run = */ true);
            // Add official argument name (correcting possible case)
            args.AddString(arg_map_it->first.c_str());
            ++it;
            // Add its values
            for (; it != next_it; ++it)
            {
                args.AddString(it->c_str());
            }
            it = next_it;
        }
        else
        {
            throw std::runtime_error("Unknown argument: " + current_argument);
        }
    }

    return args;
}

Argument &GDALArgumentParser::add_inverted_logic_flag(const std::string &name,
                                                      bool *store_into,
                                                      const std::string &help)
{
    return add_argument(name)
        .default_value(true)
        .implicit_value(false)
        .action(
            [store_into](const auto &)
            {
                if (store_into)
                    *store_into = false;
            })
        .help(help);
}

/************************************************************************/
/*                           parse_args()                               */
/************************************************************************/

void GDALArgumentParser::parse_args(const CPLStringList &aosArgs)
{
    std::vector<std::string> reorderedArgs;
    std::vector<std::string> positionalArgs;

    // ArgumentParser::parse_args() expects the first argument to be the
    // binary name
    if (!aosArgs.empty())
    {
        reorderedArgs.push_back(aosArgs[0]);
    }

    // Simplified logic borrowed from ArgumentParser::parse_args_internal()
    // that make sure that positional arguments are moved after optional ones,
    // as this is what ArgumentParser::parse_args() only supports.
    // This doesn't support advanced settings, such as sub-parsers or compound
    // argument
    std::vector<std::string> raw_arguments{aosArgs.List(),
                                           aosArgs.List() + aosArgs.size()};
    auto arguments = preprocess_arguments(raw_arguments);
    auto end = std::end(arguments);
    auto positional_argument_it = std::begin(m_positional_arguments);
    for (auto it = std::next(std::begin(arguments)); it != end;)
    {
        const auto &current_argument = *it;
        if (Argument::is_positional(current_argument, m_prefix_chars))
        {
            if (positional_argument_it != std::end(m_positional_arguments))
            {
                auto argument = positional_argument_it++;
                auto next_it =
                    argument->consume(it, end, "", /* dry_run = */ true);
                for (; it != next_it; ++it)
                {
                    if (!Argument::is_positional(*it, m_prefix_chars))
                    {
                        next_it = it;
                        break;
                    }
                    positionalArgs.push_back(*it);
                }
                it = next_it;
                continue;
            }
            else
            {
                if (m_positional_arguments.empty())
                {
                    throw std::runtime_error(
                        "Zero positional arguments expected");
                }
                else
                {
                    throw std::runtime_error(
                        "Maximum number of positional arguments "
                        "exceeded, failed to parse '" +
                        current_argument + "'");
                }
            }
        }

        auto arg_map_it = find_argument(current_argument);
        if (arg_map_it != m_argument_map.end())
        {
            auto argument = arg_map_it->second;
            auto next_it = argument->consume(
                std::next(it), end, arg_map_it->first, /* dry_run = */ true);
            // Add official argument name (correcting possible case)
            reorderedArgs.push_back(arg_map_it->first);
            ++it;
            // Add its values
            for (; it != next_it; ++it)
            {
                reorderedArgs.push_back(*it);
            }
            it = next_it;
        }
        else
        {
            throw std::runtime_error("Unknown argument: " + current_argument);
        }
    }

    reorderedArgs.insert(reorderedArgs.end(), positionalArgs.begin(),
                         positionalArgs.end());

    ArgumentParser::parse_args(reorderedArgs);
}
