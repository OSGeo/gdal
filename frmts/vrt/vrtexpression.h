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

#pragma once

#include "cpl_error.h"

#include <string_view>
#include <vector>

namespace gdal
{

/**
 * Class to support evaluation of an expression using the exprtk library.
 */
class ExprtkExpression
{
  public:
    explicit ExprtkExpression(std::string_view osExpression);

    ~ExprtkExpression();

    void RegisterVariable(std::string_view osVariable, double *pdfLocation);

    void RegisterVector(std::string_view osVariable,
                        std::vector<double> *padfLocation);

    CPLErr Compile();

    CPLErr Evaluate();

    const std::vector<double> &Results() const;

  private:
    class Impl;

    std::unique_ptr<Impl> m_pImpl;
};

}  // namespace gdal
