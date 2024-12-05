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

#include <muParser.h>

namespace gdal
{

class MuParserExpression::Impl
{
  public:
    explicit Impl(std::string_view osExpression)
        : m_osExpression(osExpression), m_Parser{}, m_adfResults{1},
          m_bIsCompiled{false}
    {
    }

    void Register(std::string_view osVariable, double *pdfValue)
    {
        m_Parser.DefineVar(std::string(osVariable), pdfValue);
    }

    CPLErr Compile()
    {
        try
        {
            m_Parser.SetExpr(m_osExpression);
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
            m_adfResults[0] = m_Parser.Eval();
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

    std::string m_osExpression;
    mu::Parser m_Parser;
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
    (void)osVariable;
    (void)padfValues;
}

CPLErr MuParserExpression::Evaluate()
{
    return m_pImpl->Evaluate();
}

const std::vector<double> &MuParserExpression::Results() const
{
    return m_pImpl->m_adfResults;
}

}  // namespace gdal
