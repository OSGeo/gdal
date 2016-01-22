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
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "ods_formula.h"

#ifndef PI
#define PI  3.14159265358979323846
#endif

/************************************************************************/
/*                          ods_formula_node()                          */
/************************************************************************/

ods_formula_node::ods_formula_node()

{
    Initialize();
}

/************************************************************************/
/*                         ods_formula_node(int)                        */
/************************************************************************/

ods_formula_node::ods_formula_node( int nValueIn )

{
    Initialize();

    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = nValueIn;
}

/************************************************************************/
/*                      ods_formula_node(double)                        */
/************************************************************************/

ods_formula_node::ods_formula_node( double dfValueIn )

{
    Initialize();

    field_type = ODS_FIELD_TYPE_FLOAT;
    float_value = dfValueIn;
}

/************************************************************************/
/*                       ods_formula_node(const char*)                  */
/************************************************************************/

ods_formula_node::ods_formula_node( const char *pszValueIn,
                                    ods_formula_field_type field_type_in )

{
    Initialize();

    field_type = field_type_in;
    string_value = CPLStrdup( pszValueIn ? pszValueIn : "" );
}

/************************************************************************/
/*                        ods_formula_node(ods_formula_op)              */
/************************************************************************/

ods_formula_node::ods_formula_node( ods_formula_op eOpIn )

{
    Initialize();

    eNodeType = SNT_OPERATION;

    eOp = eOpIn;
}

/************************************************************************/
/*              ods_formula_node(const ods_formula_node&)               */
/************************************************************************/

ods_formula_node::ods_formula_node( const ods_formula_node& other )
{
    eNodeType = other.eNodeType;
    eOp = other.eOp;
    field_type = other.field_type;
    int_value = other.int_value;
    float_value = other.float_value;
    string_value = other.string_value ? CPLStrdup(other.string_value) : NULL;
    nSubExprCount = other.nSubExprCount;
    if (nSubExprCount)
    {
        papoSubExpr = (ods_formula_node **)
            CPLMalloc( sizeof(void*) * nSubExprCount );
        for(int i=0;i<nSubExprCount;i++)
        {
            papoSubExpr[i] = new ods_formula_node( *(other.papoSubExpr[i]) );
        }
    }
    else
        papoSubExpr = NULL;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void ods_formula_node::Initialize()

{
    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_EMPTY;
    int_value = 0;
    float_value = 0;
    string_value = NULL;
    papoSubExpr = NULL;
    nSubExprCount = 0;
}

/************************************************************************/
/*                          ~ods_formula_node()                         */
/************************************************************************/

ods_formula_node::~ods_formula_node()

{
    CPLFree( string_value );
    FreeSubExpr();
}

/************************************************************************/
/*                         PushSubExpression()                          */
/************************************************************************/

void ods_formula_node::PushSubExpression( ods_formula_node *child )

{
    nSubExprCount++;
    papoSubExpr = (ods_formula_node **)
        CPLRealloc( papoSubExpr, sizeof(void*) * nSubExprCount );

    papoSubExpr[nSubExprCount-1] = child;
}

/************************************************************************/
/*                       ReverseSubExpressions()                        */
/************************************************************************/

void ods_formula_node::ReverseSubExpressions()

{
    int i;
    for( i = 0; i < nSubExprCount / 2; i++ )
    {
        ods_formula_node *temp;

        temp = papoSubExpr[i];
        papoSubExpr[i] = papoSubExpr[nSubExprCount - i - 1];
        papoSubExpr[nSubExprCount - i - 1] = temp;
    }
}

/************************************************************************/
/*                        GetOperatorName()                             */
/************************************************************************/

static const char* ODSGetOperatorName( ods_formula_op eOp )
{
    switch (eOp)
    {
        case ODS_OR : return "OR";
        case ODS_AND : return "AND";
        case ODS_NOT : return "NOT";
        case ODS_IF : return "IF";

        case ODS_PI : return "PI";

        //case ODS_T : return "T";
        case ODS_LEN : return "LEN";
        case ODS_LEFT : return "LEFT";
        case ODS_RIGHT : return "RIGHT";
        case ODS_MID : return "MID";

        case ODS_SUM : return "SUM";
        case ODS_AVERAGE : return "AVERAGE";
        case ODS_MIN : return "MIN";
        case ODS_MAX : return "MAX";
        case ODS_COUNT : return "COUNT";
        case ODS_COUNTA : return "COUNTA";

        case ODS_EQ : return "=";
        case ODS_NE : return "<>";
        case ODS_GE : return ">=";
        case ODS_LE : return "<=";
        case ODS_LT : return "<";
        case ODS_GT : return ">";

        case ODS_ADD : return "+";
        case ODS_SUBTRACT : return "-";
        case ODS_MULTIPLY : return "*";
        case ODS_DIVIDE : return "/";
        case ODS_MODULUS : return "MOD";
        case ODS_CONCAT : return "&";

        case ODS_LIST : return "*list*";
        case ODS_CELL : return "*cell*";
        case ODS_CELL_RANGE : return "*cell_range*";
        default:
        {
            const SingleOpStruct* psSingleOp = ODSGetSingleOpEntry(eOp);
            if (psSingleOp != NULL)
                return psSingleOp->pszName;
            return "*unknown*";
        }
    }
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void ods_formula_node::Dump( FILE * fp, int depth )

{
    char        spaces[60];
    int         i;

    for( i = 0; i < depth*2 && i < (int) sizeof(spaces) - 1; i++ )
        spaces[i] = ' ';
    spaces[i] = '\0';

    if( eNodeType == SNT_CONSTANT )
    {
        if( field_type == ODS_FIELD_TYPE_INTEGER )
            fprintf( fp, "%s  %d\n", spaces, int_value );
        else if( field_type == ODS_FIELD_TYPE_FLOAT )
            fprintf( fp, "%s  %.15g\n", spaces, float_value );
        else
            fprintf( fp, "%s  \"%s\"\n", spaces, string_value );
        return;
    }

    CPLAssert( eNodeType == SNT_OPERATION );

    fprintf( fp, "%s%s\n", spaces, ODSGetOperatorName(eOp) );

    for( i = 0; i < nSubExprCount; i++ )
        papoSubExpr[i]->Dump( fp, depth+1 );
}

/************************************************************************/
/*                             FreeSubExpr()                            */
/************************************************************************/

void  ods_formula_node::FreeSubExpr()
{
    int i;
    for( i = 0; i < nSubExprCount; i++ )
        delete papoSubExpr[i];
    CPLFree( papoSubExpr );

    nSubExprCount = 0;
    papoSubExpr = NULL;
}

/************************************************************************/
/*                              Evaluate()                              */
/************************************************************************/

int ods_formula_node::Evaluate(IODSCellEvaluator* poEvaluator)
{
    if (eNodeType == SNT_CONSTANT)
        return TRUE;

    CPLAssert( eNodeType == SNT_OPERATION );

    switch (eOp)
    {
        case ODS_OR: return EvaluateOR(poEvaluator);
        case ODS_AND: return EvaluateAND(poEvaluator);
        case ODS_NOT: return EvaluateNOT(poEvaluator);
        case ODS_IF: return EvaluateIF(poEvaluator);

        case ODS_PI:
            eNodeType = SNT_CONSTANT;
            field_type = ODS_FIELD_TYPE_FLOAT;
            float_value = PI;
            return TRUE;

        case ODS_LEN : return EvaluateLEN(poEvaluator);
        case ODS_LEFT : return EvaluateLEFT(poEvaluator);
        case ODS_RIGHT : return EvaluateRIGHT(poEvaluator);
        case ODS_MID : return EvaluateMID(poEvaluator);

        case ODS_SUM:
        case ODS_AVERAGE:
        case ODS_MIN:
        case ODS_MAX:
        case ODS_COUNT:
        case ODS_COUNTA:
            return EvaluateListArgOp(poEvaluator);

        case ODS_ABS:
        case ODS_SQRT:
        case ODS_COS:
        case ODS_SIN:
        case ODS_TAN:
        case ODS_ACOS:
        case ODS_ASIN:
        case ODS_ATAN:
        case ODS_EXP:
        case ODS_LN:
        case ODS_LOG:
            return EvaluateSingleArgOp(poEvaluator);


        case ODS_EQ: return EvaluateEQ(poEvaluator);
        case ODS_NE: return EvaluateNE(poEvaluator);
        case ODS_LE: return EvaluateLE(poEvaluator);
        case ODS_GE: return EvaluateGE(poEvaluator);
        case ODS_LT: return EvaluateLT(poEvaluator);
        case ODS_GT: return EvaluateGT(poEvaluator);

        case ODS_ADD:
        case ODS_SUBTRACT:
        case ODS_MULTIPLY:
        case ODS_DIVIDE:
        case ODS_MODULUS:
            return EvaluateBinaryArithmetic(poEvaluator);

        case ODS_CONCAT: return EvaluateCONCAT(poEvaluator);

        case ODS_CELL: return EvaluateCELL(poEvaluator);

        default:
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unhandled case in Evaluate() for %s",
                     ODSGetOperatorName(eOp));
            return FALSE;
        }
    }
}

/************************************************************************/
/*                             EvaluateOR()                             */
/************************************************************************/

int ods_formula_node::EvaluateOR(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_OR );

    CPLAssert(nSubExprCount == 1);
    CPLAssert(papoSubExpr[0]->eNodeType == SNT_OPERATION );
    CPLAssert(papoSubExpr[0]->eOp == ODS_LIST );
    int bVal = FALSE;
    for(int i = 0; i < papoSubExpr[0]->nSubExprCount; i++)
    {
        if (!(papoSubExpr[0]->papoSubExpr[i]->Evaluate(poEvaluator)))
            return FALSE;
        CPLAssert(papoSubExpr[0]->papoSubExpr[i]->eNodeType == SNT_CONSTANT );
        if (papoSubExpr[0]->papoSubExpr[i]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal |= (papoSubExpr[0]->papoSubExpr[i]->int_value != 0);
        }
        else if (papoSubExpr[0]->papoSubExpr[i]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal |= (papoSubExpr[0]->papoSubExpr[i]->float_value != 0);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Bad argument type for %s", ODSGetOperatorName(eOp));
            return FALSE;
        }
    }

    FreeSubExpr();

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = bVal;

    return TRUE;
}

/************************************************************************/
/*                            EvaluateAND()                             */
/************************************************************************/

int ods_formula_node::EvaluateAND(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_AND );

    CPLAssert(nSubExprCount == 1);
    CPLAssert(papoSubExpr[0]->eNodeType == SNT_OPERATION );
    CPLAssert(papoSubExpr[0]->eOp == ODS_LIST );
    int bVal = TRUE;
    for(int i = 0; i < papoSubExpr[0]->nSubExprCount; i++)
    {
        if (!(papoSubExpr[0]->papoSubExpr[i]->Evaluate(poEvaluator)))
            return FALSE;
        CPLAssert(papoSubExpr[0]->papoSubExpr[i]->eNodeType == SNT_CONSTANT );
        if (papoSubExpr[0]->papoSubExpr[i]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal &= (papoSubExpr[0]->papoSubExpr[i]->int_value != 0);
        }
        else if (papoSubExpr[0]->papoSubExpr[i]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal &= (papoSubExpr[0]->papoSubExpr[i]->float_value != 0);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Bad argument type for %s", ODSGetOperatorName(eOp));
            return FALSE;
        }
    }

    FreeSubExpr();

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = bVal;

    return TRUE;
}

/************************************************************************/
/*                            EvaluateNOT()                             */
/************************************************************************/

int ods_formula_node::EvaluateNOT(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_NOT );

    CPLAssert(nSubExprCount == 1);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );

    int bVal = FALSE;
    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_INTEGER)
    {
        bVal = !(papoSubExpr[0]->int_value != 0);
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_FLOAT)
    {
        bVal = !(papoSubExpr[0]->float_value != 0);
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Bad argument type for %s", ODSGetOperatorName(eOp));
        return FALSE;
    }

    FreeSubExpr();

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = bVal;

    return TRUE;
}

/************************************************************************/
/*                            EvaluateIF()                              */
/************************************************************************/

int ods_formula_node::EvaluateIF(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_IF );

    CPLAssert(nSubExprCount == 2 || nSubExprCount == 3);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;
    if (nSubExprCount == 3 && !(papoSubExpr[2]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );
    if (nSubExprCount == 3)
    {
        CPLAssert(papoSubExpr[2]->eNodeType == SNT_CONSTANT );
    }
    int bCond = FALSE;
    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_INTEGER)
    {
        bCond = (papoSubExpr[0]->int_value != 0);
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_FLOAT)
    {
        bCond = (papoSubExpr[0]->float_value != 0);
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Bad argument type for %s", ODSGetOperatorName(eOp));
        return FALSE;
    }

    if (bCond)
    {
        eNodeType = SNT_CONSTANT;
        field_type = papoSubExpr[1]->field_type;
        if (field_type == ODS_FIELD_TYPE_INTEGER)
            int_value = papoSubExpr[1]->int_value;
        else if (field_type == ODS_FIELD_TYPE_FLOAT)
            float_value = papoSubExpr[1]->float_value;
        else if (field_type == ODS_FIELD_TYPE_STRING)
        {
            string_value = papoSubExpr[1]->string_value;
            papoSubExpr[1]->string_value = NULL;
        }
    }
    else if (nSubExprCount == 3)
    {
        eNodeType = SNT_CONSTANT;
        field_type = papoSubExpr[2]->field_type;
        if (field_type == ODS_FIELD_TYPE_INTEGER)
            int_value = papoSubExpr[2]->int_value;
        else if (field_type == ODS_FIELD_TYPE_FLOAT)
            float_value = papoSubExpr[2]->float_value;
        else if (field_type == ODS_FIELD_TYPE_STRING)
        {
            string_value = papoSubExpr[2]->string_value;
            papoSubExpr[2]->string_value = NULL;
        }
    }
    else
    {
        eNodeType = SNT_CONSTANT;
        field_type = ODS_FIELD_TYPE_INTEGER;
        int_value = FALSE;
    }

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                            EvaluateEQ()                              */
/************************************************************************/

int ods_formula_node::EvaluateEQ(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_EQ );

    CPLAssert(nSubExprCount == 2);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );

    int bVal = FALSE;
    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_INTEGER)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->int_value == papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->int_value == papoSubExpr[1]->float_value);
        }
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_FLOAT)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->float_value == papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->float_value == papoSubExpr[1]->float_value);
        }
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_STRING)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_STRING)
        {
            bVal = (strcmp(papoSubExpr[0]->string_value,
                           papoSubExpr[1]->string_value) == 0);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Bad argument type for %s", ODSGetOperatorName(eOp));
        return FALSE;
    }

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = bVal;

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                           EvaluateNE()                               */
/************************************************************************/

int ods_formula_node::EvaluateNE(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_NE );

    eOp = ODS_EQ;
    if (!EvaluateEQ(poEvaluator))
        return FALSE;

    int_value = !int_value;
    return TRUE;
}

/************************************************************************/
/*                              GetCase()                               */
/************************************************************************/

typedef enum
{
    CASE_LOWER,
    CASE_UPPER,
    CASE_UNKNOWN,
} CaseType;

static CaseType GetCase(const char* pszStr)
{
    int bInit = TRUE;
    char ch;
    CaseType eCase = CASE_UNKNOWN;
    while((ch = *(pszStr++)) != '\0')
    {
        if (bInit)
        {
            if (ch >= 'a' && ch <= 'z')
                eCase = CASE_LOWER;
            else if (ch >= 'A' && ch <= 'Z')
                eCase = CASE_UPPER;
            else
                return CASE_UNKNOWN;
        }
        else if (ch >= 'a' && ch <= 'z' && eCase == CASE_LOWER)
            ;
        else if (ch >= 'A' && ch <= 'Z' && eCase == CASE_UPPER)
            ;
        else
            return CASE_UNKNOWN;
    }
    return eCase;
}

/************************************************************************/
/*                            EvaluateLE()                              */
/************************************************************************/

int ods_formula_node::EvaluateLE(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_LE );

    CPLAssert(nSubExprCount == 2);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );

    int bVal = FALSE;
    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_INTEGER)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->int_value <= papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->int_value <= papoSubExpr[1]->float_value);
        }
        else
            bVal = TRUE;
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_FLOAT)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->float_value <= papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->float_value <= papoSubExpr[1]->float_value);
        }
        else
            bVal = TRUE;
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_STRING)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_STRING)
        {
            if (GetCase(papoSubExpr[0]->string_value) ==
                GetCase(papoSubExpr[1]->string_value))
                bVal = (strcmp(papoSubExpr[0]->string_value,
                               papoSubExpr[1]->string_value) <= 0);
            else
                bVal = (strcasecmp(papoSubExpr[0]->string_value,
                                   papoSubExpr[1]->string_value) <= 0);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Bad argument type for %s", ODSGetOperatorName(eOp));
        return FALSE;
    }

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = bVal;

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                            EvaluateGE()                              */
/************************************************************************/

int ods_formula_node::EvaluateGE(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_GE );

    CPLAssert(nSubExprCount == 2);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );

    int bVal = FALSE;
    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_INTEGER)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->int_value >= papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->int_value >= papoSubExpr[1]->float_value);
        }
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_FLOAT)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->float_value >= papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->float_value >= papoSubExpr[1]->float_value);
        }
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_STRING)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_STRING)
        {
            if (GetCase(papoSubExpr[0]->string_value) ==
                GetCase(papoSubExpr[1]->string_value))
                bVal = (strcmp(papoSubExpr[0]->string_value,
                               papoSubExpr[1]->string_value) >= 0);
            else
                bVal = (strcasecmp(papoSubExpr[0]->string_value,
                                   papoSubExpr[1]->string_value) >= 0);
        }
        else
            bVal = TRUE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Bad argument type for %s", ODSGetOperatorName(eOp));
        return FALSE;
    }

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = bVal;

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                            EvaluateLT()                              */
/************************************************************************/

int ods_formula_node::EvaluateLT(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_LT );

    CPLAssert(nSubExprCount == 2);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );

    int bVal = FALSE;
    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_INTEGER)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->int_value < papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->int_value < papoSubExpr[1]->float_value);
        }
        else
            bVal = TRUE;
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_FLOAT)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->float_value < papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->float_value < papoSubExpr[1]->float_value);
        }
        else
            bVal = TRUE;
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_STRING)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_STRING)
        {
            if (GetCase(papoSubExpr[0]->string_value) ==
                GetCase(papoSubExpr[1]->string_value))
                bVal = (strcmp(papoSubExpr[0]->string_value,
                               papoSubExpr[1]->string_value) < 0);
            else
                bVal = (strcasecmp(papoSubExpr[0]->string_value,
                                   papoSubExpr[1]->string_value) < 0);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Bad argument type for %s", ODSGetOperatorName(eOp));
        return FALSE;
    }

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = bVal;

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                            EvaluateGT()                              */
/************************************************************************/

int ods_formula_node::EvaluateGT(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_GT );

    CPLAssert(nSubExprCount == 2);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );

    int bVal = FALSE;
    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_INTEGER)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->int_value > papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->int_value > papoSubExpr[1]->float_value);
        }
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_FLOAT)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            bVal = (papoSubExpr[0]->float_value > papoSubExpr[1]->int_value);
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            bVal = (papoSubExpr[0]->float_value > papoSubExpr[1]->float_value);
        }
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_STRING)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_STRING)
        {
            if (GetCase(papoSubExpr[0]->string_value) ==
                GetCase(papoSubExpr[1]->string_value))
                bVal = (strcmp(papoSubExpr[0]->string_value,
                               papoSubExpr[1]->string_value) > 0);
            else
                bVal = (strcasecmp(papoSubExpr[0]->string_value,
                                   papoSubExpr[1]->string_value) > 0);
        }
        else
            bVal = TRUE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Bad argument type for %s", ODSGetOperatorName(eOp));
        return FALSE;
    }

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = bVal;

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                         EvaluateSingleArgOp()                        */
/************************************************************************/

int ods_formula_node::EvaluateSingleArgOp(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );

    const SingleOpStruct* psSingleOp = ODSGetSingleOpEntry(eOp);
    CPLAssert(psSingleOp);

    CPLAssert(nSubExprCount == 1);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    double dfVal = 0;

    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_INTEGER)
    {
        dfVal = psSingleOp->pfnEval(papoSubExpr[0]->int_value);
    }
    else if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_FLOAT)
    {
        dfVal = psSingleOp->pfnEval(papoSubExpr[0]->float_value);
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Bad argument type for %s",
                 psSingleOp->pszName);
        return FALSE;
    }

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_FLOAT;
    float_value = dfVal;

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                       EvaluateBinaryArithmetic()                     */
/************************************************************************/

int ods_formula_node::EvaluateBinaryArithmetic(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp >= ODS_ADD && eOp<= ODS_MODULUS );

    CPLAssert(nSubExprCount == 2);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );

    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_INTEGER)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            int nVal;
            switch (eOp)
            {
                case ODS_ADD      : nVal = (papoSubExpr[0]->int_value + papoSubExpr[1]->int_value); break;
                case ODS_SUBTRACT : nVal = (papoSubExpr[0]->int_value - papoSubExpr[1]->int_value); break;
                case ODS_MULTIPLY : nVal = (papoSubExpr[0]->int_value * papoSubExpr[1]->int_value); break;
                case ODS_DIVIDE   :
                    if (papoSubExpr[1]->int_value != 0)
                        nVal = (papoSubExpr[0]->int_value / papoSubExpr[1]->int_value);
                    else
                        return FALSE;
                    break;
                case ODS_MODULUS  :
                    if (papoSubExpr[1]->int_value != 0)
                        nVal = (papoSubExpr[0]->int_value % papoSubExpr[1]->int_value);
                    else
                        return FALSE;
                    break;
                default: nVal = 0; CPLAssert(0);
            }

            eNodeType = SNT_CONSTANT;
            field_type = ODS_FIELD_TYPE_INTEGER;
            int_value = nVal;

            FreeSubExpr();

            return TRUE;
        }
        else if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            papoSubExpr[0]->field_type = ODS_FIELD_TYPE_FLOAT;
            papoSubExpr[0]->float_value = papoSubExpr[0]->int_value;
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Bad argument type for %s", ODSGetOperatorName(eOp));
            return FALSE;
        }
    }

    if (papoSubExpr[0]->field_type == ODS_FIELD_TYPE_FLOAT)
    {
        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_INTEGER)
        {
            papoSubExpr[1]->field_type = ODS_FIELD_TYPE_FLOAT;
            papoSubExpr[1]->float_value = papoSubExpr[1]->int_value;
        }

        if (papoSubExpr[1]->field_type == ODS_FIELD_TYPE_FLOAT)
        {
            float dfVal;
            switch (eOp)
            {
                case ODS_ADD      : dfVal = (papoSubExpr[0]->float_value + papoSubExpr[1]->float_value); break;
                case ODS_SUBTRACT : dfVal = (papoSubExpr[0]->float_value - papoSubExpr[1]->float_value); break;
                case ODS_MULTIPLY : dfVal = (papoSubExpr[0]->float_value * papoSubExpr[1]->float_value); break;
                case ODS_DIVIDE   :
                    if (papoSubExpr[1]->float_value != 0)
                        dfVal = (papoSubExpr[0]->float_value / papoSubExpr[1]->float_value);
                    else
                        return FALSE;
                    break;
                case ODS_MODULUS  :
                    if (papoSubExpr[1]->float_value != 0)
                        dfVal = fmod(papoSubExpr[0]->float_value, papoSubExpr[1]->float_value);
                    else
                        return FALSE;
                    break;
                default: dfVal = 0.0; CPLAssert(0);
            }

            eNodeType = SNT_CONSTANT;
            field_type = ODS_FIELD_TYPE_FLOAT;
            float_value = dfVal;

            FreeSubExpr();

            return TRUE;
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Bad argument type for %s", ODSGetOperatorName(eOp));
            return FALSE;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Bad argument type for %s", ODSGetOperatorName(eOp));
        return FALSE;
    }
}


/************************************************************************/
/*                         TransformToString()                          */
/************************************************************************/

std::string ods_formula_node::TransformToString() const
{
    char szTmp[128];
    if (field_type == ODS_FIELD_TYPE_INTEGER)
    {
        snprintf(szTmp, sizeof(szTmp), "%d", int_value);
        return szTmp;
    }
    else if (field_type == ODS_FIELD_TYPE_FLOAT)
    {
        snprintf(szTmp, sizeof(szTmp), "%.16g", float_value);
        return szTmp;
    }
    else if (field_type == ODS_FIELD_TYPE_STRING)
    {
        return string_value;
    }
    else
    {
        return "";
    }
}

/************************************************************************/
/*                           EvaluateCONCAT()                           */
/************************************************************************/

int ods_formula_node::EvaluateCONCAT(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_CONCAT );

    CPLAssert(nSubExprCount == 2);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );

    std::string osLeft(papoSubExpr[0]->TransformToString());
    std::string osRight(papoSubExpr[1]->TransformToString());

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_STRING;
    string_value = CPLStrdup((osLeft + osRight).c_str());

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                             GetRowCol()                              */
/************************************************************************/

static int GetRowCol(const char* pszCell, int& nRow, int& nCol)
{
    int i;

    if (pszCell[0] != '.')
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid cell %s", pszCell);
        return FALSE;
    }

    nCol = 0;
    for(i=1; pszCell[i]>='A' && pszCell[i]<='Z';i++)
    {
        nCol = nCol * 26 + (pszCell[i] - 'A');
    }
    nRow = atoi(pszCell + i) - 1;

    return TRUE;
}

/************************************************************************/
/*                         EvaluateListArgOp()                          */
/************************************************************************/

int ods_formula_node::EvaluateListArgOp(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp >= ODS_SUM && eOp <= ODS_COUNTA );

    CPLAssert(nSubExprCount == 1);
    CPLAssert(papoSubExpr[0]->eNodeType == SNT_OPERATION );
    CPLAssert(papoSubExpr[0]->eOp == ODS_LIST );

    std::vector<double> adfVal;
    int i;

    int nCount = 0, nCountA = 0;

    for(i=0;i<papoSubExpr[0]->nSubExprCount;i++)
    {
        if (papoSubExpr[0]->papoSubExpr[i]->eNodeType == SNT_OPERATION &&
            papoSubExpr[0]->papoSubExpr[i]->eOp == ODS_CELL_RANGE)
        {
            CPLAssert (papoSubExpr[0]->papoSubExpr[i]->nSubExprCount == 2);
            CPLAssert (papoSubExpr[0]->papoSubExpr[i]->papoSubExpr[0]->eNodeType == SNT_CONSTANT);
            CPLAssert (papoSubExpr[0]->papoSubExpr[i]->papoSubExpr[0]->field_type == ODS_FIELD_TYPE_STRING);
            CPLAssert (papoSubExpr[0]->papoSubExpr[i]->papoSubExpr[1]->eNodeType == SNT_CONSTANT);
            CPLAssert (papoSubExpr[0]->papoSubExpr[i]->papoSubExpr[1]->field_type == ODS_FIELD_TYPE_STRING);

            if (poEvaluator == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "No cell evaluator provided");
                return FALSE;
            }

            const char* psz1 = papoSubExpr[0]->papoSubExpr[i]->papoSubExpr[0]->string_value;
            const char* psz2 = papoSubExpr[0]->papoSubExpr[i]->papoSubExpr[1]->string_value;
            int nRow1 = 0,nCol1 = 0;
            if (!GetRowCol(psz1, nRow1, nCol1))
                return FALSE;
            int nRow2 = 0,nCol2 = 0;
            if (!GetRowCol(psz2, nRow2, nCol2))
                return FALSE;

            std::vector<ods_formula_node> aoOutValues;
            if (poEvaluator->EvaluateRange(nRow1, nCol1, nRow2, nCol2, aoOutValues))
            {
                for(size_t j = 0; j < aoOutValues.size(); j++)
                {
                    if (aoOutValues[j].eNodeType == SNT_CONSTANT)
                    {
                        if (aoOutValues[j].field_type == ODS_FIELD_TYPE_INTEGER)
                        {
                            adfVal.push_back(aoOutValues[j].int_value);
                            nCount ++;
                            nCountA ++;
                        }
                        else if (aoOutValues[j].field_type == ODS_FIELD_TYPE_FLOAT)
                        {
                            adfVal.push_back(aoOutValues[j].float_value);
                            nCount ++;
                            nCountA ++;
                        }
                        else if (aoOutValues[j].field_type == ODS_FIELD_TYPE_STRING)
                        {
                            nCountA ++;
                        }
                    }
                }
            }
        }
        else
        {
            if (!(papoSubExpr[0]->papoSubExpr[i]->Evaluate(poEvaluator)))
                return FALSE;

            CPLAssert (papoSubExpr[0]->papoSubExpr[i]->eNodeType == SNT_CONSTANT );
            if (papoSubExpr[0]->papoSubExpr[i]->field_type == ODS_FIELD_TYPE_INTEGER)
            {
                adfVal.push_back(papoSubExpr[0]->papoSubExpr[i]->int_value);
                nCount ++;
                nCountA ++;
            }
            else if (papoSubExpr[0]->papoSubExpr[i]->field_type == ODS_FIELD_TYPE_FLOAT)
            {
                adfVal.push_back(papoSubExpr[0]->papoSubExpr[i]->float_value);
                nCount ++;
                nCountA ++;
            }
            else if (eOp == ODS_COUNT || eOp == ODS_COUNTA)
            {
                if (papoSubExpr[0]->papoSubExpr[i]->field_type == ODS_FIELD_TYPE_STRING)
                    nCountA ++;
            }
            else
            {

                CPLError(CE_Failure, CPLE_NotSupported,
                         "Bad argument type for %s", ODSGetOperatorName(eOp));
                return FALSE;
            }
        }
    }

    if (eOp == ODS_COUNT)
    {
        eNodeType = SNT_CONSTANT;
        field_type = ODS_FIELD_TYPE_INTEGER;
        int_value = nCount;

        FreeSubExpr();
        return TRUE;
    }

    if (eOp == ODS_COUNTA)
    {
        eNodeType = SNT_CONSTANT;
        field_type = ODS_FIELD_TYPE_INTEGER;
        int_value = nCountA;

        FreeSubExpr();
        return TRUE;
    }

    double dfVal = 0;

    switch(eOp)
    {
        case ODS_SUM:
        {
            for(i=0;i<(int)adfVal.size();i++)
            {
                dfVal += adfVal[i];
            }
            break;
        }

        case ODS_AVERAGE:
        {
            for(i=0;i<(int)adfVal.size();i++)
            {
                dfVal += adfVal[i];
            }
            dfVal /= adfVal.size();
            break;
        }

        case ODS_MIN:
        {
            dfVal = (adfVal.size() == 0) ? 0 :adfVal[0];
            for(i=1;i<(int)adfVal.size();i++)
            {
                if (adfVal[i] < dfVal) dfVal = adfVal[i];
            }
            break;
        }

        case ODS_MAX:
        {
            dfVal = (adfVal.size() == 0) ? 0 :adfVal[0];
            for(i=1;i<(int)adfVal.size();i++)
            {
                if (adfVal[i] > dfVal) dfVal = adfVal[i];
            }
            break;
        }

        default:
            break;
    }

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_FLOAT;
    float_value = dfVal;

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                           EvaluateCELL()                             */
/************************************************************************/

int ods_formula_node::EvaluateCELL(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );
    CPLAssert( eOp == ODS_CELL );

    CPLAssert(nSubExprCount == 1);
    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[0]->field_type == ODS_FIELD_TYPE_STRING );

    if (poEvaluator == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No cell evaluator provided");
        return FALSE;
    }

    int nRow = 0,nCol = 0;
    if (!GetRowCol(papoSubExpr[0]->string_value, nRow, nCol))
        return FALSE;

    std::vector<ods_formula_node> aoOutValues;
    if (poEvaluator->EvaluateRange(nRow, nCol, nRow, nCol, aoOutValues) &&
        aoOutValues.size() == 1)
    {
        if (aoOutValues[0].eNodeType == SNT_CONSTANT)
        {
            FreeSubExpr();

            eNodeType = aoOutValues[0].eNodeType;
            field_type = aoOutValues[0].field_type;
            int_value = aoOutValues[0].int_value;
            float_value = aoOutValues[0].float_value;
            string_value = aoOutValues[0].string_value ? CPLStrdup(aoOutValues[0].string_value) : NULL;

            return TRUE;
        }
    }

    return FALSE;
}

/************************************************************************/
/*                           EvaluateLEN()                              */
/************************************************************************/

int ods_formula_node::EvaluateLEN(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );

    CPLAssert(nSubExprCount == 1);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );

    std::string osVal = papoSubExpr[0]->TransformToString();

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_INTEGER;
    int_value = strlen(osVal.c_str()); // FIXME : UTF8 support

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                           EvaluateLEFT()                             */
/************************************************************************/

int ods_formula_node::EvaluateLEFT(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );

    CPLAssert(nSubExprCount == 2);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );

    std::string osVal = papoSubExpr[0]->TransformToString();

    if (papoSubExpr[1]->field_type != ODS_FIELD_TYPE_INTEGER)
        return FALSE;

    // FIXME : UTF8 support
    int nVal = papoSubExpr[1]->int_value;
    if (nVal < 0)
        return FALSE;

    osVal = osVal.substr(0,nVal);

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_STRING;
    string_value = CPLStrdup(osVal.c_str());

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                           EvaluateRIGHT()                            */
/************************************************************************/

int ods_formula_node::EvaluateRIGHT(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );

    CPLAssert(nSubExprCount == 2);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );

    std::string osVal = papoSubExpr[0]->TransformToString();

    if (papoSubExpr[1]->field_type != ODS_FIELD_TYPE_INTEGER)
        return FALSE;

    // FIXME : UTF8 support
    size_t nLen = osVal.size();
    int nVal = papoSubExpr[1]->int_value;
    if (nVal < 0)
        return FALSE;

    if (nLen > (size_t) nVal)
        osVal = osVal.substr(nLen-nVal);

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_STRING;
    string_value = CPLStrdup(osVal.c_str());

    FreeSubExpr();

    return TRUE;
}

/************************************************************************/
/*                           EvaluateMID()                             */
/************************************************************************/

int ods_formula_node::EvaluateMID(IODSCellEvaluator* poEvaluator)
{
    CPLAssert( eNodeType == SNT_OPERATION );

    CPLAssert(nSubExprCount == 3);
    if (!(papoSubExpr[0]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[1]->Evaluate(poEvaluator)))
        return FALSE;
    if (!(papoSubExpr[2]->Evaluate(poEvaluator)))
        return FALSE;

    CPLAssert(papoSubExpr[0]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[1]->eNodeType == SNT_CONSTANT );
    CPLAssert(papoSubExpr[2]->eNodeType == SNT_CONSTANT );

    std::string osVal = papoSubExpr[0]->TransformToString();

    if (papoSubExpr[1]->field_type != ODS_FIELD_TYPE_INTEGER)
        return FALSE;

    if (papoSubExpr[2]->field_type != ODS_FIELD_TYPE_INTEGER)
        return FALSE;

    // FIXME : UTF8 support
    size_t nLen = osVal.size();
    int nStart = papoSubExpr[1]->int_value;
    int nExtractLen = papoSubExpr[2]->int_value;
    if (nStart <= 0)
        return FALSE;
    if (nExtractLen < 0)
        return FALSE;

    if ((size_t)nStart <= nLen)
    {
        if (nStart-1 + nExtractLen >= (int)nLen)
            osVal = osVal.substr(nStart - 1);
        else
            osVal = osVal.substr(nStart - 1, nExtractLen);
    }
    else
        osVal = "";

    eNodeType = SNT_CONSTANT;
    field_type = ODS_FIELD_TYPE_STRING;
    string_value = CPLStrdup(osVal.c_str());

    FreeSubExpr();

    return TRUE;
}
