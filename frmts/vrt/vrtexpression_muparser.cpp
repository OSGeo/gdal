/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of GDALExpressionEvaluator.
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2024, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "vrtexpression.h"
#include "cpl_string.h"

#include <limits>
#include <map>
#include <optional>
#include "muparser_header.h"

namespace gdal
{

/*! @cond Doxygen_Suppress */

static std::optional<std::string> Sanitize(const std::string &osVariable)
{
    // muparser does not allow characters '[' or ']' which we use to emulate
    // vectors. Replace these with a combination of underscores
    auto from = osVariable.find('[');
    if (from != std::string::npos)
    {
        auto to = osVariable.find(']');
        if (to != std::string::npos)
        {
            auto sanitized = std::string("__") + osVariable.substr(0, from) +
                             +"__" +
                             osVariable.substr(from + 1, to - from - 1) + "__";
            return sanitized;
        }
    }

    return std::nullopt;
}

static void ReplaceVariable(std::string &expression,
                            const std::string &variable,
                            const std::string &sanitized)
{
    std::string::size_type seekPos = 0;
    auto pos = expression.find(variable, seekPos);
    while (pos != std::string::npos)
    {
        auto end = pos + variable.size();

        if (pos == 0 ||
            (!std::isalnum(expression[pos - 1]) && expression[pos - 1] != '_'))
        {
            expression =
                expression.substr(0, pos) + sanitized + expression.substr(end);
        }

        seekPos = end;
        pos = expression.find(variable, seekPos);
    }
}

class MuParserExpression::Impl
{
  public:
    explicit Impl(std::string_view osExpression)
        : m_osExpression(std::string(osExpression))
    {
    }

    void Register(std::string_view osVariable, double *pdfValue)
    {
        try
        {
            m_oParser.DefineVar(std::string(osVariable), pdfValue);
        }
        catch (const mu::Parser::exception_type &)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid variable name: %s",
                     std::string(osVariable).c_str());
            m_bCompileFailed = true;
        }
    }

    CPLErr Compile()
    {
        if (m_bCompileFailed)
        {
            return CE_Failure;
        }

        // On some platforms muparser does not seem to parse "nan" as a floating
        // point literal.
        try
        {
            m_oParser.DefineConst("nan",
                                  std::numeric_limits<double>::quiet_NaN());
            m_oParser.DefineConst("NaN",
                                  std::numeric_limits<double>::quiet_NaN());
        }
        catch (const mu::Parser::exception_type &)
        {
        }

        try
        {
            std::string tmpExpression(m_osExpression);

            for (const auto &[osFrom, osTo] : m_oSubstitutions)
            {
                ReplaceVariable(tmpExpression, osFrom, osTo);
            }

            m_oParser.SetExpr(tmpExpression);
        }
        catch (const mu::Parser::exception_type &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.GetMsg().c_str());
            return CE_Failure;
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }

        return CE_None;
    }

    CPLErr Evaluate()
    {
        if (!m_bIsCompiled)
        {
            if (auto eErr = Compile(); eErr != CE_None)
            {
                return eErr;
            }

            m_bIsCompiled = true;
        }

        try
        {
            int nResults;
            const double *dfResults = m_oParser.Eval(nResults);
            m_adfResults.resize(nResults);
            std::copy(dfResults, dfResults + nResults, m_adfResults.begin());
        }
        catch (const mu::Parser::exception_type &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.GetMsg().c_str());
            return CE_Failure;
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }

        return CE_None;
    }

    const CPLString m_osExpression;
    std::map<CPLString, CPLString> m_oSubstitutions{};
    mu::Parser m_oParser{};
    std::vector<double> m_adfResults{1};
    bool m_bIsCompiled = false;
    bool m_bCompileFailed = false;
};

MuParserExpression::MuParserExpression(std::string_view osExpression)
    : m_pImpl{std::make_unique<Impl>(osExpression)}

{
}

MuParserExpression::~MuParserExpression()
{
}

CPLErr MuParserExpression::Compile()
{
    return m_pImpl->Compile();
}

void MuParserExpression::RegisterVariable(std::string_view osVariable,
                                          double *pdfValue)
{
    auto sanitized = Sanitize(std::string(osVariable));
    if (sanitized.has_value())
    {
        m_pImpl->m_oSubstitutions[std::string(osVariable)] = sanitized.value();
    }
    m_pImpl->Register(sanitized.value_or(std::string(osVariable)), pdfValue);
}

void MuParserExpression::RegisterVector(std::string_view osVariable,
                                        std::vector<double> *padfValues)
{
    // muparser does not support vector variables, so we simulate them
    // by creating a scalar variable for each element, and then replacing
    // the name of the vector by a list of its elements before compiling
    // the expression.
    CPLString osElementVarName;
    CPLString osElementsList;
    std::string osVectorVarName(osVariable);

    int nElementVarNameLength = static_cast<int>(
        4 + osVectorVarName.size() + std::log10(padfValues->size()));
    osElementsList.reserve(padfValues->size() *
                           (1 + nElementVarNameLength));  // +1 for commas

    for (std::size_t i = 0; i < padfValues->size(); i++)
    {
        osElementVarName.Printf("%s[%d]", osVectorVarName.c_str(),
                                static_cast<int>(i));
        osElementVarName = Sanitize(osElementVarName).value();
        RegisterVariable(osElementVarName, padfValues->data() + i);

        if (i > 0)
        {
            osElementsList += ",";
        }
        osElementsList += osElementVarName;
    }

    m_pImpl->m_oSubstitutions[std::string(osVariable)] = osElementsList;
}

CPLErr MuParserExpression::Evaluate()
{
    return m_pImpl->Evaluate();
}

const std::vector<double> &MuParserExpression::Results() const
{
    return m_pImpl->m_adfResults;
}

/*! @endcond Doxygen_Suppress */

}  // namespace gdal
