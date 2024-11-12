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

#include "cpl_conv.h"
#include "cpl_error.h"
#include "vrtexpression.h"

#define exprtk_disable_caseinsensitivity
#define exprtk_disable_rtl_io
#define exprtk_disable_rtl_io_file
#define exprtk_disable_rtl_vecops
#define exprtk_disable_string_capabilities
#include <exprtk.hpp>

class GDALExpressionEvaluator::Impl
{
  public:
    exprtk::expression<double> m_oExpression{};
    exprtk::parser<double> m_oParser{};
    exprtk::symbol_table<double> m_oSymbolTable{};
    std::string m_osExpression{};

    std::vector<std::pair<std::string, double *>> m_aoVariables{};
    std::vector<std::pair<std::string, std::vector<double> *>> m_aoVectors{};
    std::vector<double> m_adfResults{};

    bool m_bIsCompiled{false};

    CPLErr compile()
    {
        for (const auto &[osVariable, pdfValueLoc] : m_aoVariables)
        {
            m_oSymbolTable.add_variable(osVariable, *pdfValueLoc);
        }

        for (const auto &[osVariable, padfVectorLoc] : m_aoVectors)
        {
            m_oSymbolTable.add_vector(osVariable, *padfVectorLoc);
        }

        m_oExpression.register_symbol_table(m_oSymbolTable);
        bool bSuccess = m_oParser.compile(m_osExpression, m_oExpression);

        if (!bSuccess)
        {
            for (size_t i = 0; i < m_oParser.error_count(); i++)
            {
                const auto &oError = m_oParser.get_error(i);

                CPLError(CE_Warning, CPLE_AppDefined,
                         "Position: %02d "
                         "Type: [%s] "
                         "Message: %s\n",
                         static_cast<int>(oError.token.position),
                         exprtk::parser_error::to_str(oError.mode).c_str(),
                         oError.diagnostic.c_str());
            }

            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to parse expression.");
            return CE_Failure;
        }

        m_bIsCompiled = true;

        return CE_None;
    }
};

GDALExpressionEvaluator::GDALExpressionEvaluator(std::string_view osExpression)
    : m_pImpl(std::make_unique<Impl>())
{
    m_pImpl->m_osExpression = osExpression;
}

GDALExpressionEvaluator::~GDALExpressionEvaluator()
{
}

void GDALExpressionEvaluator::RegisterVariable(std::string_view osVariable,
                                               double *pdfValue)
{
    m_pImpl->m_aoVariables.emplace_back(osVariable, pdfValue);
}

void GDALExpressionEvaluator::RegisterVector(std::string_view osVariable,
                                             std::vector<double> *padfValue)
{
    m_pImpl->m_aoVectors.emplace_back(osVariable, padfValue);
}

CPLErr GDALExpressionEvaluator::Compile()
{
    return m_pImpl->compile();
}

const std::vector<double> &GDALExpressionEvaluator::Results() const
{
    return m_pImpl->m_adfResults;
}

CPLErr GDALExpressionEvaluator::Evaluate()
{
    if (!m_pImpl->m_bIsCompiled)
    {
        auto eErr = m_pImpl->compile();
        if (eErr != CE_None)
        {
            return eErr;
        }
    }

    m_pImpl->m_adfResults.clear();
    double value = m_pImpl->m_oExpression.value();  // force evaluation

    const auto &results = m_pImpl->m_oExpression.results();

    // We follow a different method to get the result depending on
    // how the expression was formed. If a "return" statement was
    // used, the result will be accessible via the "result" object.
    // If no "return" statement was used, the result is accessible
    // from the "value" variable (and must not be a vector.)
    if (results.count() == 0)
    {
        m_pImpl->m_adfResults.resize(1);
        m_pImpl->m_adfResults[0] = value;
    }
    else if (results.count() == 1)
    {

        if (results[0].type == exprtk::type_store<double>::e_scalar)
        {
            m_pImpl->m_adfResults.resize(1);
            results.get_scalar(0, m_pImpl->m_adfResults[0]);
        }
        else if (results[0].type == exprtk::type_store<double>::e_vector)
        {
            results.get_vector(0, m_pImpl->m_adfResults);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Expression returned an unexpected type.");
            return CE_Failure;
        }
    }
    else
    {
        m_pImpl->m_adfResults.resize(results.count());
        for (size_t i = 0; i < results.count(); i++)
        {
            if (results[i].type != exprtk::type_store<double>::e_scalar)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Expression must return a vector or a list of "
                         "scalars.");
                return CE_Failure;
            }
            else
            {
                results.get_scalar(i, m_pImpl->m_adfResults[i]);
            }
        }
    }

    return CE_None;
}
