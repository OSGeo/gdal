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

//! @cond Doxygen_Suppress

#include "cpl_conv.h"
#include "cpl_string.h"

#include <algorithm>
#include <limits>
#include <memory>

class GDAL_c_expr_node;
class GDAL_c_expr_parse_context;

static void GDAL_c_expr_error(GDAL_c_expr_parse_context *context,
                              const char *msg);

#include "gdal_c_expr.h"

namespace
{
#include "gdal_c_expr_parser.hpp"
} /* end of anonymous namespace */

static int GDAL_c_expr_lex(GDAL_c_expr_node **ppNode,
                           GDAL_c_expr_parse_context *context);

class GDAL_c_expr_parse_context
{
    GDAL_c_expr_parse_context(const GDAL_c_expr_parse_context &) = delete;
    GDAL_c_expr_parse_context &
    operator=(const GDAL_c_expr_parse_context &) = delete;

  public:
    GDAL_c_expr_parse_context() = default;

    int nStartToken = 0;
    const char *pszInput = nullptr;
    const char *pszNext = nullptr;
    const char *pszLastValid = nullptr;

    std::unique_ptr<GDAL_c_expr_node> poRoot{};
};

namespace
{
#include "gdal_c_expr_parser.cpp"
} /* end of anonymous namespace */

/************************************************************************/
/*                         GDAL_c_expr_error()                          */
/************************************************************************/
static void GDAL_c_expr_error(GDAL_c_expr_parse_context *context,
                              const char *msg)
{
    yysymbol_name(YYSYMBOL_YYEMPTY);  // to please MSVC

    CPLString osMsg;
    osMsg.Printf("C Expression Parsing Error: %s. Occurred around :\n", msg);

    int n = static_cast<int>(context->pszLastValid - context->pszInput);

    for (int i = std::max(0, n - 40);
         i < n + 40 && context->pszInput[i] != '\0'; i++)
        osMsg += context->pszInput[i];
    osMsg += "\n";
    for (int i = 0; i < std::min(n, 40); i++)
        osMsg += " ";
    osMsg += "^";

    CPLError(CE_Failure, CPLE_AppDefined, "%s", osMsg.c_str());
}

/************************************************************************/
/*                        GDAL_c_expr_compile()                         */
/************************************************************************/

static int GDAL_c_expr_lex(GDAL_c_expr_node **ppNode,
                           GDAL_c_expr_parse_context *context)
{
    const char *pszInput = context->pszNext;

    *ppNode = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Do we have a start symbol to return?                            */
    /* -------------------------------------------------------------------- */
    if (context->nStartToken != 0)
    {
        int nRet = context->nStartToken;
        context->nStartToken = 0;
        return nRet;
    }

    /* -------------------------------------------------------------------- */
    /*      Skip white space.                                               */
    /* -------------------------------------------------------------------- */
    while (*pszInput == ' ' || *pszInput == '\t' || *pszInput == 10 ||
           *pszInput == 13)
        pszInput++;

    context->pszLastValid = pszInput;

    if (*pszInput == '\0')
    {
        context->pszNext = pszInput;
        return EOF;
    }

    /* -------------------------------------------------------------------- */
    /*      Handle numbers.                                                 */
    /* -------------------------------------------------------------------- */
    else if (*pszInput >= '0' && *pszInput <= '9')
    {
        const char *pszNext = pszInput + 1;

        CPLString osToken;
        osToken += *pszInput;

        // collect non-decimal part of number
        while (*pszNext >= '0' && *pszNext <= '9')
            osToken += *(pszNext++);

        // collect decimal places.
        if (*pszNext == '.')
        {
            osToken += *(pszNext++);
            while (*pszNext >= '0' && *pszNext <= '9')
                osToken += *(pszNext++);
        }

        // collect exponent
        if (*pszNext == 'e' || *pszNext == 'E')
        {
            osToken += *(pszNext++);
            if (*pszNext == '-' || *pszNext == '+')
                osToken += *(pszNext++);
            while (*pszNext >= '0' && *pszNext <= '9')
                osToken += *(pszNext++);
        }

        context->pszNext = pszNext;

        if (strstr(osToken, ".") || strstr(osToken, "e") ||
            strstr(osToken, "E") || osToken.size() >= 20)
        {
            *ppNode = new GDAL_c_expr_node(CPLAtof(osToken));
        }
        else
        {
            *ppNode = new GDAL_c_expr_node(
                static_cast<int64_t>(CPLAtoGIntBig(osToken)));
        }

        return C_EXPR_TOK_NUMBER;
    }

    /* -------------------------------------------------------------------- */
    /*      Handle alpha-numerics.                                          */
    /* -------------------------------------------------------------------- */
    else if (*pszInput == '.' || *pszInput == '_' ||
             isalnum(static_cast<unsigned char>(*pszInput)))
    {
        int nReturn = C_EXPR_TOK_IDENTIFIER;
        const char *pszNext = pszInput + 1;

        CPLString osToken;
        osToken += *pszInput;

        // collect text characters
        while (isalnum(static_cast<unsigned char>(*pszNext)) ||
               *pszNext == '_' || static_cast<unsigned char>(*pszNext) > 127)
            osToken += *(pszNext++);

        context->pszNext = pszNext;

        /* Constants */
        if (EQUAL(osToken, "_pi"))  // muparser specific
        {
            *ppNode = new GDAL_c_expr_node(M_PI);
            return C_EXPR_TOK_NUMBER;
        }
        else if (EQUAL(osToken, "_e"))  // muparser specific
        {
            constexpr double CONSTANT_E = 2.7182818284590452353602874713526624;
            *ppNode = new GDAL_c_expr_node(CONSTANT_E);
            return C_EXPR_TOK_NUMBER;
        }
        else if (EQUAL(osToken, "NAN"))  // muparser specific
        {
            *ppNode =
                new GDAL_c_expr_node(std::numeric_limits<double>::quiet_NaN());
            return C_EXPR_TOK_NUMBER;
        }
        else if (EQUAL(osToken, "NODATA"))  // muparser specific
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_NODATA);
            return C_EXPR_TOK_NUMBER;
        }

        /* Zero-arg functions */
        else if (EQUAL(osToken, "rnd"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_RND);
            return C_EXPR_TOK_FUNCTION_ZERO_ARG;
        }

        /* Single-arg functions */
        else if (EQUAL(osToken, "abs"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_ABS);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "sqrt"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_SQRT);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "cos"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_COS);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "sin"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_SIN);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "tan"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_TAN);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "acos"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_ACOS);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "asin"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_ASIN);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "atan"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_ATAN);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "cosh"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_COSH);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "sinh"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_SINH);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "tanh"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_TANH);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "acosh"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_ACOSH);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "asinh"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_ASINH);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "atanh"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_ATANH);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "exp"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_EXP);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "log") || EQUAL(osToken, "ln"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_LOG);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "log2"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_LOG2);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "log10"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_LOG10);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "isnan"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_ISNAN);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "isnodata"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_ISNODATA);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "sign"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_SIGN);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }
        else if (EQUAL(osToken, "rint"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_RINT);
            return C_EXPR_TOK_FUNCTION_SINGLE_ARG;
        }

        // Two arguments
        else if (EQUAL(osToken, "fmod"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_FMOD);
            return C_EXPR_TOK_FUNCTION_TWO_ARG;
        }

        /* Multiple arg functions */
        else if (EQUAL(osToken, "min"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_MIN);
            return C_EXPR_TOK_FUNCTION_MULTIPLE_ARG;
        }
        else if (EQUAL(osToken, "max"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_MAX);
            return C_EXPR_TOK_FUNCTION_MULTIPLE_ARG;
        }
        else if (EQUAL(osToken, "sum"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_SUM);
            return C_EXPR_TOK_FUNCTION_MULTIPLE_ARG;
        }
        else if (EQUAL(osToken, "avg"))
        {
            *ppNode = new GDAL_c_expr_node(C_EXPR_AVG);
            return C_EXPR_TOK_FUNCTION_MULTIPLE_ARG;
        }

        else
        {
            *ppNode = new GDAL_c_expr_node(osToken);
            nReturn = C_EXPR_TOK_IDENTIFIER;
        }

        return nReturn;
    }

    else if (pszInput[0] == '!' && pszInput[1] == '=')
    {
        context->pszNext = pszInput + 2;
        return C_EXPR_TOK_NE;
    }
    else if (pszInput[0] == '!')
    {
        context->pszNext = pszInput + 1;
        return C_EXPR_TOK_NOT;
    }
    else if (pszInput[0] == '&' && pszInput[1] == '&')
    {
        context->pszNext = pszInput + 2;
        return C_EXPR_TOK_AND;
    }
    else if (pszInput[0] == '&')
    {
        context->pszNext = pszInput + 1;
        return C_EXPR_TOK_BITWISE_AND;
    }
    else if (pszInput[0] == '|' && pszInput[1] == '|')
    {
        context->pszNext = pszInput + 2;
        return C_EXPR_TOK_OR;
    }
    else if (pszInput[0] == '|')
    {
        context->pszNext = pszInput + 1;
        return C_EXPR_TOK_BITWISE_OR;
    }
    else if (pszInput[0] == '?')
    {
        context->pszNext = pszInput + 1;
        return C_EXPR_TOK_TERNARY_THEN;
    }
    else if (pszInput[0] == ':')
    {
        context->pszNext = pszInput + 1;
        return C_EXPR_TOK_TERNARY_ELSE;
    }
    else if (pszInput[0] == '=' && pszInput[1] == '=')
    {
        context->pszNext = pszInput + 2;
        return C_EXPR_TOK_EQ;
    }
    else if (pszInput[0] == '<' && pszInput[1] == '=')
    {
        context->pszNext = pszInput + 2;
        return C_EXPR_TOK_LE;
    }
    else if (pszInput[0] == '<')
    {
        context->pszNext = pszInput + 1;
        return C_EXPR_TOK_LT;
    }
    else if (pszInput[0] == '>' && pszInput[1] == '=')
    {
        context->pszNext = pszInput + 2;
        return C_EXPR_TOK_GE;
    }
    else if (pszInput[0] == '>')
    {
        context->pszNext = pszInput + 1;
        return C_EXPR_TOK_GT;
    }

    /* -------------------------------------------------------------------- */
    /*      Handle special tokens.                                          */
    /* -------------------------------------------------------------------- */
    else
    {
        context->pszNext = pszInput + 1;
        return *pszInput;
    }
}

/************************************************************************/
/*                        GDAL_c_expr_compile()                         */
/************************************************************************/

std::unique_ptr<GDAL_c_expr_node> GDAL_c_expr_compile(const char *expr)
{
    GDAL_c_expr_parse_context context;

    context.pszInput = expr;
    context.pszNext = expr;
    context.pszLastValid = expr;
    context.nStartToken = C_EXPR_TOK_START;

    if (GDAL_c_expr_parse(&context) == 0)
    {
        return std::move(context.poRoot);
    }

    return nullptr;
}

/************************************************************************/
/*                        GDAL_c_expr_node()                            */
/************************************************************************/

GDAL_c_expr_node::GDAL_c_expr_node() = default;

/************************************************************************/
/*                        GDAL_c_expr_node(int)                         */
/************************************************************************/

GDAL_c_expr_node::GDAL_c_expr_node(int nValueIn)
    : field_type(C_EXPR_FIELD_TYPE_INTEGER), int_value(nValueIn)
{
}

/************************************************************************/
/*                      GDAL_c_expr_node(int64_t)                       */
/************************************************************************/

GDAL_c_expr_node::GDAL_c_expr_node(int64_t nValueIn)
    : field_type(C_EXPR_FIELD_TYPE_INTEGER), int_value(nValueIn)
{
}

/************************************************************************/
/*                      GDAL_c_expr_node(double)                        */
/************************************************************************/

GDAL_c_expr_node::GDAL_c_expr_node(double dfValueIn)
    : field_type(C_EXPR_FIELD_TYPE_FLOAT), float_value(dfValueIn)
{
}

/************************************************************************/
/*                       GDAL_c_expr_node(const char*)                  */
/************************************************************************/

GDAL_c_expr_node::GDAL_c_expr_node(const char *pszValueIn,
                                   c_expr_field_type field_type_in)
    : field_type(field_type_in), string_value(pszValueIn ? pszValueIn : "")
{
}

/************************************************************************/
/*                        GDAL_c_expr_node(c_expr_op)                   */
/************************************************************************/

GDAL_c_expr_node::GDAL_c_expr_node(c_expr_op eOpIn)
    : eNodeType(CENT_OPERATION), eOp(eOpIn)
{
}

/************************************************************************/
/*              GDAL_c_expr_node(const GDAL_c_expr_node&)               */
/************************************************************************/

GDAL_c_expr_node::GDAL_c_expr_node(const GDAL_c_expr_node &other)
    : eNodeType(other.eNodeType), field_type(other.field_type), eOp(other.eOp),
      string_value(other.string_value), int_value(other.int_value),
      float_value(other.float_value)
{
    for (const auto &otherSubExpr : other.apoSubExpr)
    {
        apoSubExpr.emplace_back(
            std::make_unique<GDAL_c_expr_node>(*otherSubExpr));
    }
}

/************************************************************************/
/*                          ~GDAL_c_expr_node()                         */
/************************************************************************/

GDAL_c_expr_node::~GDAL_c_expr_node() = default;

/************************************************************************/
/*                         PushSubExpression()                          */
/************************************************************************/

void GDAL_c_expr_node::PushSubExpression(GDAL_c_expr_node *child)

{
    apoSubExpr.push_back(std::unique_ptr<GDAL_c_expr_node>(child));
}

/************************************************************************/
/*                       ReverseSubExpressions()                        */
/************************************************************************/

void GDAL_c_expr_node::ReverseSubExpressions()

{
    std::reverse(apoSubExpr.begin(), apoSubExpr.end());
}

//! @endcond
