/******************************************************************************
 * $Id$
 *
 * Component: ODS formula Engine
 * Purpose: Implementation of the ods_formula_node class used to represent a
 *          node in a ODS expression.
 * Author: Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _ODS_FORMULA_H_INCLUDED_
#define _ODS_FORMULA_H_INCLUDED_

#include "cpl_conv.h"
#include "cpl_string.h"

#include <vector>

#if defined(_WIN32) && !defined(_WIN32_WCE)
#  define strcasecmp stricmp
#elif defined(_WIN32_WCE)
#  define strcasecmp _stricmp
#endif

typedef enum {
    ODS_OR,
    ODS_AND,
    ODS_NOT,
    ODS_IF,

    ODS_PI,

    ODS_SUM,
    ODS_AVERAGE,
    ODS_MIN,
    ODS_MAX,
    ODS_COUNT,
    ODS_COUNTA,

    //ODS_T,
    ODS_LEN,
    ODS_LEFT,
    ODS_RIGHT,
    ODS_MID,

    ODS_ABS,
    ODS_SQRT,
    ODS_COS,
    ODS_SIN,
    ODS_TAN,
    ODS_ACOS,
    ODS_ASIN,
    ODS_ATAN,
    ODS_EXP,
    ODS_LN,
    ODS_LOG,

    ODS_EQ,
    ODS_NE,
    ODS_LE,
    ODS_GE,
    ODS_LT,
    ODS_GT,

    ODS_ADD,
    ODS_SUBTRACT,
    ODS_MULTIPLY,
    ODS_DIVIDE,
    ODS_MODULUS,

    ODS_CONCAT,

    ODS_LIST,

    ODS_CELL,
    ODS_CELL_RANGE,
} ods_formula_op;

typedef enum {
    ODS_FIELD_TYPE_INTEGER,
    ODS_FIELD_TYPE_FLOAT,
    ODS_FIELD_TYPE_STRING,
    ODS_FIELD_TYPE_EMPTY
} ods_formula_field_type;

typedef enum {
    SNT_CONSTANT,
    SNT_OPERATION
} ods_formula_node_type;

class IODSCellEvaluator;

class ods_formula_node {
private:
    void           FreeSubExpr();
    std::string    TransformToString() const;

    int            EvaluateOR(IODSCellEvaluator* poEvaluator);
    int            EvaluateAND(IODSCellEvaluator* poEvaluator);
    int            EvaluateNOT(IODSCellEvaluator* poEvaluator);
    int            EvaluateIF(IODSCellEvaluator* poEvaluator);

    int            EvaluateLEN(IODSCellEvaluator* poEvaluator);
    int            EvaluateLEFT(IODSCellEvaluator* poEvaluator);
    int            EvaluateRIGHT(IODSCellEvaluator* poEvaluator);
    int            EvaluateMID(IODSCellEvaluator* poEvaluator);

    int            EvaluateListArgOp(IODSCellEvaluator* poEvaluator);

    int            EvaluateSingleArgOp(IODSCellEvaluator* poEvaluator);

    int            EvaluateEQ(IODSCellEvaluator* poEvaluator);
    int            EvaluateNE(IODSCellEvaluator* poEvaluator);
    int            EvaluateLE(IODSCellEvaluator* poEvaluator);
    int            EvaluateGE(IODSCellEvaluator* poEvaluator);
    int            EvaluateLT(IODSCellEvaluator* poEvaluator);
    int            EvaluateGT(IODSCellEvaluator* poEvaluator);

    int            EvaluateBinaryArithmetic(IODSCellEvaluator* poEvaluator);

    int            EvaluateCONCAT(IODSCellEvaluator* poEvaluator);

    int            EvaluateCELL(IODSCellEvaluator* poEvaluator);

public:
    ods_formula_node();

    ods_formula_node( const char *, ods_formula_field_type field_type_in = ODS_FIELD_TYPE_STRING );
    ods_formula_node( int );
    ods_formula_node( double );
    ods_formula_node( ods_formula_op );

    ods_formula_node( const ods_formula_node& other );

    ~ods_formula_node();

    void           Initialize();
    void           Dump( FILE *fp, int depth );

    int            Evaluate(IODSCellEvaluator* poEvaluator);

    ods_formula_node_type eNodeType;
    ods_formula_field_type field_type;

    /* only for SNT_OPERATION */
    void        PushSubExpression( ods_formula_node * );
    void        ReverseSubExpressions();
    ods_formula_op eOp;
    int         nSubExprCount;
    ods_formula_node **papoSubExpr;

    /* only for SNT_CONSTANT */
    char        *string_value;
    int         int_value;
    double      float_value;
};

class ods_formula_parse_context {
public:
    ods_formula_parse_context() : nStartToken(0), poRoot(NULL) {}

    int        nStartToken;
    const char *pszInput;
    const char *pszNext;

    ods_formula_node *poRoot;
};

class IODSCellEvaluator
{
public:
    virtual int EvaluateRange(int nRow1, int nCol1, int nRow2, int nCol2,
                              std::vector<ods_formula_node>& aoOutValues) = 0;
    virtual ~IODSCellEvaluator() {}
};

int ods_formulaparse( ods_formula_parse_context *context );
int ods_formulalex( ods_formula_node **ppNode, ods_formula_parse_context *context );
ods_formula_node* ods_formula_compile( const char *expr );

typedef struct
{
    const char      *pszName;
    ods_formula_op   eOp;
    double          (*pfnEval)(double);
} SingleOpStruct;

const SingleOpStruct* ODSGetSingleOpEntry(const char* pszName);
const SingleOpStruct* ODSGetSingleOpEntry(ods_formula_op eOp);

#endif /* def _ODS_FORMULA_H_INCLUDED_ */
