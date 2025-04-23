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
    set_usage_max_line_width(80);
    set_usage_break_on_mutex();
    add_usage_newline();

    if (bForBinary)
    {
        add_argument("-h", "--help")
            .flag()
            .action(
                [this](const auto &)
                {
                    std::cout << usage() << std::endl << std::endl;
                    std::cout << _("Note: ") << m_parser_path
                              << _(" --long-usage for full help.") << std::endl;
                    std::exit(0);
                })
            .help(_("Shows short help message and exits."));

        // Used by program-output directives in .rst files
        add_argument("--help-doc")
            .flag()
            .hidden()
            .action(
                [this](const auto &)
                {
                    std::cout << usage() << std::endl;
                    std::exit(0);
                })
            .help(_("Display help message for use by documentation."));

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
                [this](const auto &)
                {
                    printf("%s was compiled against GDAL %s and "
                           "is running against GDAL %s\n",
                           m_program_name.c_str(), GDAL_RELEASE_NAME,
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
/*                                usage()                               */
/************************************************************************/

std::string GDALArgumentParser::usage() const
{
    std::string ret(ArgumentParser::usage());
    if (!m_osExtraUsageHint.empty())
    {
        ret += '\n';
        ret += '\n';
        ret += m_osExtraUsageHint;
    }
    return ret;
}

/************************************************************************/
/*                          add_extra_usage_hint()                      */
/************************************************************************/

void GDALArgumentParser::add_extra_usage_hint(
    const std::string &osExtraUsageHint)
{
    m_osExtraUsageHint = osExtraUsageHint;
}

/************************************************************************/
/*                         add_quiet_argument()                         */
/************************************************************************/

Argument &GDALArgumentParser::add_quiet_argument(bool *pVar)
{
    auto &arg =
        this->add_argument("-q", "--quiet")
            .flag()
            .help(
                _("Quiet mode. No progress message is emitted on the standard "
                  "output."));
    if (pVar)
        arg.store_into(*pVar);

    return arg;
}

/************************************************************************/
/*                      add_input_format_argument()                     */
/************************************************************************/

Argument &GDALArgumentParser::add_input_format_argument(CPLStringList *pvar)
{
    return add_argument("-if")
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

Argument &GDALArgumentParser::add_output_format_argument(std::string &var)
{
    auto &arg = add_argument("-of")
                    .metavar("<output_format>")
                    .store_into(var)
                    .help(_("Output format."));
    add_hidden_alias_for(arg, "-f");
    return arg;
}

/************************************************************************/
/*                     add_creation_options_argument()                  */
/************************************************************************/

Argument &GDALArgumentParser::add_creation_options_argument(CPLStringList &var)
{
    return add_argument("-co")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([&var](const std::string &s) { var.AddString(s.c_str()); })
        .help(_("Creation option(s)."));
}

/************************************************************************/
/*                   add_metadata_item_options_argument()               */
/************************************************************************/

Argument &
GDALArgumentParser::add_metadata_item_options_argument(CPLStringList &var)
{
    return add_argument("-mo")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([&var](const std::string &s) { var.AddString(s.c_str()); })
        .help(_("Metadata item option(s)."));
}

/************************************************************************/
/*                       add_open_options_argument()                    */
/************************************************************************/

Argument &GDALArgumentParser::add_open_options_argument(CPLStringList &var)
{
    return add_open_options_argument(&var);
}

/************************************************************************/
/*                       add_open_options_argument()                    */
/************************************************************************/

Argument &GDALArgumentParser::add_open_options_argument(CPLStringList *pvar)
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

    return arg;
}

/************************************************************************/
/*                       add_output_type_argument()                     */
/************************************************************************/

Argument &GDALArgumentParser::add_output_type_argument(GDALDataType &eDT)
{
    return add_argument("-ot")
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

Argument &
GDALArgumentParser::add_layer_creation_options_argument(CPLStringList &var)
{
    return add_argument("-lco")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([&var](const std::string &s) { var.AddString(s.c_str()); })
        .help(_("Layer creation options (format specific)."));
}

Argument &
GDALArgumentParser::add_dataset_creation_options_argument(CPLStringList &var)
{
    return add_argument("-dsco")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([&var](const std::string &s) { var.AddString(s.c_str()); })
        .help(_("Dataset creation options (format specific)."));
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

GDALArgumentParser *
GDALArgumentParser::add_subparser(const std::string &description,
                                  bool bForBinary)
{
    auto parser = std::make_unique<GDALArgumentParser>(description, bForBinary);
    ArgumentParser::add_subparser(*parser.get());
    aoSubparsers.emplace_back(std::move(parser));
    return aoSubparsers.back().get();
}

GDALArgumentParser *GDALArgumentParser::get_subparser(const std::string &name)
{
    auto it = std::find_if(
        aoSubparsers.begin(), aoSubparsers.end(),
        [&name](const auto &parser)
        { return EQUAL(name.c_str(), parser->m_program_name.c_str()); });
    return it != aoSubparsers.end() ? it->get() : nullptr;
}

bool GDALArgumentParser::is_used_globally(const std::string &name)
{
    try
    {
        return ArgumentParser::is_used(name);
    }
    catch (std::logic_error &)
    {
        // ignore
    }

    // Check if it is used by a subparser
    // loop through subparsers
    for (const auto &subparser : aoSubparsers)
    {
        // convert subparser name to lower case
        std::string subparser_name = subparser->m_program_name;
        std::transform(subparser_name.begin(), subparser_name.end(),
                       subparser_name.begin(),
                       [](int c) -> char
                       { return static_cast<char>(::tolower(c)); });
        if (m_subparser_used.find(subparser_name) != m_subparser_used.end())
        {
            if (subparser->is_used_globally(name))
            {
                return true;
            }
        }
    }

    return false;
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
                // Check sub-parsers
                auto subparser = get_subparser(current_argument);
                if (subparser)
                {

                    // build list of remaining args
                    const auto unprocessed_arguments =
                        CPLStringList(std::vector<std::string>(it, end));

                    // invoke subparser
                    m_is_parsed = true;
                    // convert to lower case
                    std::string current_argument_lower = current_argument;
                    std::transform(current_argument_lower.begin(),
                                   current_argument_lower.end(),
                                   current_argument_lower.begin(),
                                   [](int c) -> char
                                   { return static_cast<char>(::tolower(c)); });
                    m_subparser_used[current_argument_lower] = true;
                    return subparser->parse_args(unprocessed_arguments);
                }

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
