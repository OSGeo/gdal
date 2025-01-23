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
#include "cpl_string.h"
#include "vrtexpression.h"

#define exprtk_disable_caseinsensitivity
#define exprtk_disable_rtl_io
#define exprtk_disable_rtl_io_file
#define exprtk_disable_rtl_vecops
#define exprtk_disable_string_capabilities

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

#include <exprtk.hpp>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <chrono>
#include <cstdint>
#include <limits>
#include <sstream>
#include <thread>

namespace gdal
{

/*! @cond Doxygen_Suppress */
struct vector_access_check final : public exprtk::vector_access_runtime_check
{
    bool handle_runtime_violation(violation_context &context) override
    {
        auto nElements = (static_cast<std::uint8_t *>(context.end_ptr) -
                          static_cast<std::uint8_t *>(context.base_ptr)) /
                         context.type_size;
        auto nIndexAccessed = (static_cast<std::uint8_t *>(context.access_ptr) -
                               static_cast<std::uint8_t *>(context.base_ptr)) /
                              context.type_size;

        std::ostringstream oss;
        oss << "Attempted to access index " << nIndexAccessed
            << " in a vector of " << nElements
            << " elements when evaluating VRT expression.";
        throw std::runtime_error(oss.str());
    }
};

struct loop_timeout_check final : public exprtk::loop_runtime_check
{
    using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;

    loop_timeout_check() : exprtk::loop_runtime_check()
    {
        double dfMaxLoopIterationSeconds =
            CPLAtofM(CPLGetConfigOption("GDAL_EXPRTK_TIMEOUT_SECONDS", "1"));
        max_duration = std::chrono::microseconds(
            static_cast<size_t>(dfMaxLoopIterationSeconds * 1e6));
    }

    void start_timer()
    {
        timeout_t = std::chrono::steady_clock::now() + max_duration;
    }

    bool check() override
    {

        if (++iterations >= max_iters_per_check)
        {
            if (std::chrono::steady_clock::now() > timeout_t)
            {
                timeout = true;
                return false;
            }

            iterations = 0;
        }

        return true;
    }

    void handle_runtime_violation(const violation_context &) override
    {
        std::ostringstream oss;

        // current version of exprtk does not report the correct
        // violation in case of timeout, so we track the error category
        // ourselves
        if (timeout)
        {
            oss << "Expression evaluation time exceeded maximum of "
                << static_cast<double>(max_duration.count() / 1e6)
                << " seconds. You can increase this threshold by setting the "
                << "GDAL_EXPRTK_TIMEOUT_SECONDS configuration "
                << "option.";
        }
        else
        {
            oss << "Exceeded maximum of " << max_loop_iterations
                << " loop iterations.";
        }

        throw std::runtime_error(oss.str());
    }

    static constexpr size_t max_iters_per_check = 10000;
    size_t iterations = 0;
    time_point_t timeout_t{};
    std::chrono::microseconds max_duration{};
    bool timeout{false};
};

class ExprtkExpression::Impl
{
  public:
    exprtk::expression<double> m_oExpression{};
    exprtk::parser<double> m_oParser{};
    exprtk::symbol_table<double> m_oSymbolTable{};
    std::string m_osExpression{};

    std::vector<std::pair<std::string, double *>> m_aoVariables{};
    std::vector<std::pair<std::string, std::vector<double> *>> m_aoVectors{};
    std::vector<double> m_adfResults{};
    vector_access_check m_oVectorAccessCheck{};
    loop_timeout_check m_oLoopRuntimeCheck{};

    bool m_bIsCompiled{false};

    explicit Impl()
    {
        using settings_t = std::decay_t<decltype(m_oParser.settings())>;

        m_oLoopRuntimeCheck.loop_set = loop_timeout_check::e_all_loops;
        m_oLoopRuntimeCheck.max_loop_iterations = std::numeric_limits<
            decltype(m_oLoopRuntimeCheck.max_loop_iterations)>::max();
        m_oParser.register_vector_access_runtime_check(m_oVectorAccessCheck);
        m_oParser.register_loop_runtime_check(m_oLoopRuntimeCheck);

#ifndef NDEBUG
        // Only used for automated testing of GDAL_EXPRTK_TIMEOUT_SECONDS
        m_oSymbolTable.add_function("sleep", sleep);
#endif

        int nMaxVectorLength = std::atoi(
            CPLGetConfigOption("GDAL_EXPRTK_MAX_VECTOR_LENGTH", "100000"));

        if (nMaxVectorLength > 0)
        {
            m_oParser.settings().set_max_local_vector_size(nMaxVectorLength);
        }

        bool bEnableLoops =
            CPLTestBool(CPLGetConfigOption("GDAL_EXPRTK_ENABLE_LOOPS", "YES"));
        if (!bEnableLoops)
        {
            m_oParser.settings().disable_control_structure(
                settings_t::e_ctrl_for_loop);
            m_oParser.settings().disable_control_structure(
                settings_t::e_ctrl_while_loop);
            m_oParser.settings().disable_control_structure(
                settings_t::e_ctrl_repeat_loop);
        }
    }

    CPLErr compile()
    {
        int nMaxExpressionLength = std::atoi(
            CPLGetConfigOption("GDAL_EXPRTK_MAX_EXPRESSION_LENGTH", "100000"));
        if (m_osExpression.size() >
            static_cast<std::size_t>(nMaxExpressionLength))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Expression length of %d exceeds maximum of %d set by "
                     "GDAL_EXPRTK_MAX_EXPRESSION_LENGTH",
                     static_cast<int>(m_osExpression.size()),
                     nMaxExpressionLength);
            return CE_Failure;
        }

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

    CPLErr evaluate()
    {
        if (!m_bIsCompiled)
        {
            auto eErr = compile();
            if (eErr != CE_None)
            {
                return eErr;
            }
        }

        m_adfResults.clear();
        double value;
        try
        {
            value = m_oExpression.value();  // force evaluation
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }

        m_oLoopRuntimeCheck.start_timer();
        const auto &results = m_oExpression.results();

        // We follow a different method to get the result depending on
        // how the expression was formed. If a "return" statement was
        // used, the result will be accessible via the "result" object.
        // If no "return" statement was used, the result is accessible
        // from the "value" variable (and must not be a vector.)
        if (results.count() == 0)
        {
            m_adfResults.resize(1);
            m_adfResults[0] = value;
        }
        else if (results.count() == 1)
        {

            if (results[0].type == exprtk::type_store<double>::e_scalar)
            {
                m_adfResults.resize(1);
                results.get_scalar(0, m_adfResults[0]);
            }
            else if (results[0].type == exprtk::type_store<double>::e_vector)
            {
                results.get_vector(0, m_adfResults);
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
            m_adfResults.resize(results.count());
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
                    results.get_scalar(i, m_adfResults[i]);
                }
            }
        }

        return CE_None;
    }

  private:
#ifndef NDEBUG
    struct sleep_fn : public exprtk::ifunction<double>
    {
        sleep_fn() : exprtk::ifunction<double>(1)
        {
        }

        using exprtk::ifunction<double>::operator();

        double operator()(const double &seconds) override
        {
            std::this_thread::sleep_for(
                std::chrono::microseconds(static_cast<int>(seconds * 1e6)));
            return 0;
        }
    };

    sleep_fn sleep{};
#endif
};

/**
 * Define an expression to be evaluated using the exprtk library.
 *
 * @param osExpression the expression to evaluate. Refer to exprtk library documentation
 *                     for details of the allowable syntax.
 *
 * @since 3.11
 */
ExprtkExpression::ExprtkExpression(std::string_view osExpression)
    : m_pImpl(std::make_unique<Impl>())
{
    m_pImpl->m_osExpression = osExpression;
}

ExprtkExpression::~ExprtkExpression()
{
}

void ExprtkExpression::RegisterVariable(std::string_view osVariable,
                                        double *pdfValue)
{
    m_pImpl->m_aoVariables.emplace_back(osVariable, pdfValue);
}

void ExprtkExpression::RegisterVector(std::string_view osVariable,
                                      std::vector<double> *padfValue)
{
    m_pImpl->m_aoVectors.emplace_back(osVariable, padfValue);
}

CPLErr ExprtkExpression::Compile()
{
    return m_pImpl->compile();
}

const std::vector<double> &ExprtkExpression::Results() const
{
    return m_pImpl->m_adfResults;
}

CPLErr ExprtkExpression::Evaluate()
{
    return m_pImpl->evaluate();
}

/*! @endcond Doxygen_Suppress */

}  // namespace gdal
