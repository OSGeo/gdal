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
 * Class to support evaluation of a mathematical expression
 */
class MathExpression
{
  public:
    virtual ~MathExpression() = default;

    virtual void RegisterVariable(std::string_view osVariable,
                                  double *pdfLocation) = 0;

    virtual void RegisterVector(std::string_view osVariable,
                                std::vector<double> *padfLocation) = 0;

    virtual CPLErr Compile() = 0;

    virtual CPLErr Evaluate() = 0;

    virtual const std::vector<double> &Results() const = 0;
};

/**
 * Class to support evaluation of an expression using the exprtk library.
 */
class ExprtkExpression : public MathExpression
{
  public:
    explicit ExprtkExpression(std::string_view osExpression);

    virtual ~ExprtkExpression();

    void RegisterVariable(std::string_view osVariable,
                          double *pdfLocation) override;

    void RegisterVector(std::string_view osVariable,
                        std::vector<double> *padfLocation) override;

    CPLErr Compile() override;

    CPLErr Evaluate() override;

    const std::vector<double> &Results() const override;

  private:
    class Impl;

    std::unique_ptr<Impl> m_pImpl;
};

/**
 * Class to support evaluation of an expression using the muparser library.
 */
class MuParserExpression : public MathExpression
{
  public:
    explicit MuParserExpression(std::string_view osExpression);

    virtual ~MuParserExpression();

    void RegisterVariable(std::string_view osVariable,
                          double *pdfLocation) override;

    void RegisterVector(std::string_view osVariable,
                        std::vector<double> *padfLocation) override;

    CPLErr Compile() override;

    CPLErr Evaluate() override;

    const std::vector<double> &Results() const override;

  private:
    class Impl;

    std::unique_ptr<Impl> m_pImpl;
};

}  // namespace gdal
