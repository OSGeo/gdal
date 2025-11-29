/******************************************************************************
 *
 * Component: GDAL
 * Purpose:  Simplified C expression parser
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (C) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_C_EXPR_H
#define GDAL_C_EXPR_H

//! @cond Doxygen_Suppress

#include "cpl_port.h"

#include <memory>
#include <string>
#include <vector>

typedef enum
{
    // Logic expressions
    C_EXPR_OR,
    C_EXPR_AND,
    C_EXPR_NOT,
    C_EXPR_TERNARY,

    // Functions with one argument
    C_EXPR_ABS,
    C_EXPR_SQRT,
    C_EXPR_COS,
    C_EXPR_SIN,
    C_EXPR_TAN,
    C_EXPR_ACOS,
    C_EXPR_ASIN,
    C_EXPR_ATAN,
    C_EXPR_COSH,
    C_EXPR_SINH,
    C_EXPR_TANH,
    C_EXPR_ACOSH,
    C_EXPR_ASINH,
    C_EXPR_ATANH,
    C_EXPR_EXP,
    C_EXPR_LOG,
    C_EXPR_LOG2,
    C_EXPR_LOG10,
    C_EXPR_ISNAN,
    C_EXPR_FMOD,
    C_EXPR_RINT,

    // Comparison functions
    C_EXPR_EQ,
    C_EXPR_NE,
    C_EXPR_LE,
    C_EXPR_GE,
    C_EXPR_LT,
    C_EXPR_GT,

    // Arithmetic functions
    C_EXPR_ADD,
    C_EXPR_SUBTRACT,
    C_EXPR_MULTIPLY,
    C_EXPR_DIVIDE,
    C_EXPR_MODULUS,

    // Bit operation
    C_EXPR_BITWISE_AND,
    C_EXPR_BITWISE_OR,

    // Muparser specific
    C_EXPR_RND,
    C_EXPR_SIGN,
    C_EXPR_POWER,
    C_EXPR_MIN,
    C_EXPR_MAX,
    C_EXPR_SUM,
    C_EXPR_AVG,

    // Muparser-GDAL specific
    C_EXPR_NODATA,
    C_EXPR_ISNODATA,

    C_EXPR_LIST,  // used internally only for multiple arguments function

    C_EXPR_INVALID
} c_expr_op;

typedef enum
{
    C_EXPR_FIELD_TYPE_INTEGER,
    C_EXPR_FIELD_TYPE_FLOAT,
    C_EXPR_FIELD_TYPE_IDENTIFIER,
    C_EXPR_FIELD_TYPE_EMPTY
} c_expr_field_type;

typedef enum
{
    CENT_CONSTANT,
    CENT_OPERATION
} c_expr_node_type;

class GDAL_c_expr_node
{
  public:
    GDAL_c_expr_node();

    explicit GDAL_c_expr_node(const char *, c_expr_field_type field_type_in =
                                                C_EXPR_FIELD_TYPE_IDENTIFIER);
    explicit GDAL_c_expr_node(int);
    explicit GDAL_c_expr_node(int64_t);
    explicit GDAL_c_expr_node(double);
    explicit GDAL_c_expr_node(c_expr_op);

    GDAL_c_expr_node(const GDAL_c_expr_node &other);

    ~GDAL_c_expr_node();

    c_expr_node_type eNodeType = CENT_CONSTANT;
    c_expr_field_type field_type = C_EXPR_FIELD_TYPE_EMPTY;

    /* only for CENT_OPERATION */
    void PushSubExpression(GDAL_c_expr_node *);
    void ReverseSubExpressions();
    c_expr_op eOp = C_EXPR_INVALID;
    std::vector<std::unique_ptr<GDAL_c_expr_node>> apoSubExpr{};

    /* only for CENT_CONSTANT */
    std::string string_value{};
    int64_t int_value = 0;
    double float_value = 0;

  private:
    GDAL_c_expr_node &operator=(const GDAL_c_expr_node &) = delete;
};

std::unique_ptr<GDAL_c_expr_node> CPL_DLL GDAL_c_expr_compile(const char *expr);

//! @endcond

#endif
