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

    /**
    * Register a variable to be used in the expression.
    *
    * The value of the variable may be changed during repeated evaluations of
    * the expression, but its location in memory may not.
    *
    * @param osVariable The name of the variable
    * @param pdfLocation The location of the variable's value
    *
    * @since 3.11
    */
    virtual void RegisterVariable(std::string_view osVariable,
                                  double *pdfLocation) = 0;

    /**
     * Register a vector to be used in the expression.
     *
     * The values and size of the vector may be changed during repeated evaluations
     * of the expression, but its location in memory may not.
     *
     * @param osVariable The name of the vector
     * @param padfLocation The location of the vector
     *
     * @since 3.11
     */
    virtual void RegisterVector(std::string_view osVariable,
                                std::vector<double> *padfLocation) = 0;

    /**
     * Compile the expression.
     *
     * If not called explicitly, the expression will be compiled the first time
     * the expression is evaluated.
     *
     * @return CE_None if the expression can be successfully parsed and all
     *                 symbols have been registered, CE_Failure otherwise.
     *
     * @since 3.11
     */
    virtual CPLErr Compile() = 0;

    /**
     * Evaluate the expression.
     *
     * @return CE_None if the expression was successfully evaluated, CE_Failure otherwise.
     *
     * @since 3.11
     */
    virtual CPLErr Evaluate() = 0;

    /**
     * Access the results from the last time the expression was evaluated.
     *
     * The returned vector may be reused on subsequent evaluations of the expression.
     *
     * @return a reference to the vector in which results are stored.
     *
     * @since 3.11
     */
    virtual const std::vector<double> &Results() const = 0;
};

/*! @cond Doxygen_Suppress */

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

/*! @endcond */

}  // namespace gdal
