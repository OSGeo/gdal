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

#include <map>
#include <muParser.h>

namespace gdal
{

/*! @cond Doxygen_Suppress */
class MuParserExpression::Impl
{
  public:
    explicit Impl(std::string_view osExpression)
        : m_osExpression(std::string(osExpression)), m_oVectors{}, m_oParser{},
          m_adfResults{1}, m_bIsCompiled{false}
    {
    }

    void Register(std::string_view osVariable, double *pdfValue)
    {
        m_oParser.DefineVar(std::string(osVariable), pdfValue);
    }

    CPLErr Compile()
    {
        try
        {
            CPLString tmpExpression(m_osExpression);

            for (const auto &[osVec, osElems] : m_oVectors)
            {
                tmpExpression.replaceAll(osVec, osElems);
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

    CPLString m_osExpression;
    std::map<CPLString, CPLString> m_oVectors;
    mu::Parser m_oParser;
    std::vector<double> m_adfResults;
    bool m_bIsCompiled;
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
    m_pImpl->Register(osVariable, pdfValue);
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
        osElementVarName.Printf("__%s_%d", osVectorVarName.c_str(),
                                static_cast<int>(i));
        RegisterVariable(osElementVarName, padfValues->data() + i);

        if (i > 0)
        {
            osElementsList += ",";
        }
        osElementsList += osElementVarName;
    }

    m_pImpl->m_oVectors[std::string(osVariable)] = osElementsList;
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
