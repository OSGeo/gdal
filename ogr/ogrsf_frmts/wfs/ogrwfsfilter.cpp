/******************************************************************************
 * $Id$
 *
 * Project:  WFS Translator
 * Purpose:  Implements OGRWFSLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_wfs.h"

#include "cpl_list.h"

CPL_CVSID("$Id$");

typedef enum
{
    TOKEN_GREATER_OR_EQUAL,
    TOKEN_GREATER,
    TOKEN_LESSER_OR_EQUAL,
    TOKEN_LESSER,
    TOKEN_LIKE,
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_NOT,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_VAR_NAME,
    TOKEN_LITERAL
} TokenType;

typedef struct _Expr Expr;

struct _Expr
{
    TokenType eType;
    char*  pszVal;
    Expr* expr1;
    Expr* expr2;
};

static int ExprGetPriority(const Expr* expr)
{
    if (expr->eType == TOKEN_NOT)
        return 9;
    else if (expr->eType == TOKEN_GREATER_OR_EQUAL ||
             expr->eType == TOKEN_GREATER ||
             expr->eType == TOKEN_LESSER_OR_EQUAL ||
             expr->eType == TOKEN_LESSER)
        return 6;
    else if (expr->eType == TOKEN_EQUAL ||
             expr->eType == TOKEN_LIKE ||
             expr->eType == TOKEN_NOT_EQUAL)
        return 5;
    else if (expr->eType == TOKEN_AND)
        return 4;
    else if (expr->eType == TOKEN_OR)
        return 3;
    else
        return 0;
}

static void ExprFree(Expr* expr)
{
    if (expr == NULL) return;
    if (expr->expr1)
        ExprFree(expr->expr1);
    if (expr->expr2)
        ExprFree(expr->expr2);
    CPLFree(expr->pszVal);
    CPLFree(expr);
}

static void ExprFreeList(CPLList* psExprList)
{
    CPLList* psIterList = psExprList;
    while(psIterList)
    {
        ExprFree((Expr*)psIterList->pData);
        psIterList = psIterList->psNext;
    }
    CPLListDestroy(psExprList);
}

static Expr* ExprBuildVarName(const char* pszVal)
{
    Expr* expr = (Expr*)CPLCalloc(1, sizeof(Expr));
    expr->eType = TOKEN_VAR_NAME;
    expr->pszVal = CPLStrdup(pszVal);
    return expr;
}

static Expr* ExprBuildValue(const char* pszVal)
{
    Expr* expr = (Expr*)CPLCalloc(1, sizeof(Expr));
    expr->eType = TOKEN_LITERAL;
    expr->pszVal = CPLStrdup(pszVal);
    return expr;
}

static Expr* ExprBuildOperator(TokenType eType)
{
    Expr* expr = (Expr*)CPLCalloc(1, sizeof(Expr));
    expr->eType = eType;
    return expr;
}

static Expr* ExprBuildBinary(TokenType eType, Expr* expr1, Expr* expr2)
{
    Expr* expr = (Expr*)CPLCalloc(1, sizeof(Expr));
    expr->eType = eType;
    expr->expr1 = expr1;
    expr->expr2 = expr2;
    return expr;
}

#ifdef notdef
static void ExprDump(FILE* fp, const Expr* expr)
{
    switch(expr->eType)
    {
        case TOKEN_VAR_NAME:
        case TOKEN_LITERAL:
            fprintf(fp, "%s", expr->pszVal);
            break;

        case TOKEN_NOT:
            fprintf(fp, "NOT (");
            ExprDump(fp, expr->expr1);
            fprintf(fp, ")");
            break;

        default:
            fprintf(fp, "(");
            ExprDump(fp, expr->expr1);
            switch(expr->eType)
            {
                case TOKEN_EQUAL:           fprintf(fp, " = "); break;
                case TOKEN_LIKE:            fprintf(fp, " LIKE "); break;
                case TOKEN_NOT_EQUAL:       fprintf(fp, " != "); break;
                case TOKEN_LESSER_OR_EQUAL: fprintf(fp, " <= "); break;
                case TOKEN_LESSER:          fprintf(fp, " < "); break;
                case TOKEN_GREATER_OR_EQUAL:fprintf(fp, " >= "); break;
                case TOKEN_GREATER:         fprintf(fp, " > "); break;
                case TOKEN_AND:             fprintf(fp, " AND "); break;
                case TOKEN_OR:              fprintf(fp, " OR "); break;
                default: break;
            }
            ExprDump(fp, expr->expr2);
            fprintf(fp, ")");
            break;
    }
}
#endif

static int ExprDumpGmlObjectIdFilter(CPLString& osFilter,
                                     const Expr* expr)
{
    if (expr->eType == TOKEN_EQUAL &&
        expr->expr1->eType == TOKEN_VAR_NAME &&
        EQUAL(expr->expr1->pszVal, "gml_id") &&
        expr->expr2->eType == TOKEN_LITERAL)
    {
        osFilter += "<GmlObjectId gml:id=\"";
        if (expr->expr2->pszVal[0] == '\'' || expr->expr2->pszVal[0] == '"')
        {
            CPLString osVal(expr->expr2->pszVal + 1);
            osVal.resize(osVal.size() - 1);
            osFilter += osVal;
        }
        else
            osFilter += expr->expr2->pszVal;
        osFilter += "\"/>";
        return TRUE;
    }
    else if (expr->eType == TOKEN_OR)
    {
        return ExprDumpGmlObjectIdFilter(osFilter, expr->expr1) &&
               ExprDumpGmlObjectIdFilter(osFilter, expr->expr2);
    }
    return FALSE;
}

static int ExprDumpFilter(CPLString& osFilter,
                          const Expr* expr,
                          int bExpectBinary,
                          int* pbNeedsNullCheck)
{
    switch(expr->eType)
    {
        case TOKEN_VAR_NAME:
            if (bExpectBinary || EQUAL(expr->pszVal, "gml_id"))
                return FALSE;
            osFilter += "<PropertyName>";
            osFilter += expr->pszVal;
            osFilter += "</PropertyName>";
            break;

        case TOKEN_LITERAL:
            if (bExpectBinary)
                return FALSE;
            osFilter += "<Literal>";
            if (expr->pszVal[0] == '\'' || expr->pszVal[0] == '"')
            {
                CPLString osVal(expr->pszVal + 1);
                osVal.resize(osVal.size() - 1);
                osFilter += osVal;
            }
            else
                osFilter += expr->pszVal;
            osFilter += "</Literal>";
            break;

        case TOKEN_NOT:
            osFilter += "<Not>";
            if (!ExprDumpFilter(osFilter, expr->expr1, TRUE, pbNeedsNullCheck))
                return FALSE;
            osFilter += "</Not>";
            break;

        case TOKEN_LIKE:
            osFilter += "<PropertyIsLike wildcard='%' singleChar='_' escape='!'>";
            if (!ExprDumpFilter(osFilter, expr->expr1, FALSE, pbNeedsNullCheck))
                return FALSE;
            if (!ExprDumpFilter(osFilter, expr->expr2, FALSE, pbNeedsNullCheck))
                return FALSE;
            osFilter += "</PropertyIsLike>";
            break;

        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_LESSER_OR_EQUAL:
        case TOKEN_LESSER:
        case TOKEN_GREATER_OR_EQUAL:
        case TOKEN_GREATER:
        {
            if (expr->eType == TOKEN_EQUAL && expr->expr2->eType == TOKEN_LITERAL &&
                EQUAL(expr->expr2->pszVal, "NULL"))
            {
                osFilter += "<PropertyIsNull>";
                if (!ExprDumpFilter(osFilter, expr->expr1, FALSE, pbNeedsNullCheck))
                    return FALSE;
                osFilter += "</PropertyIsNull>";
                *pbNeedsNullCheck = TRUE;
                break;
            }
            if (expr->eType == TOKEN_NOT_EQUAL && expr->expr2->eType == TOKEN_LITERAL &&
                EQUAL(expr->expr2->pszVal, "NULL"))
            {
                osFilter += "<Not><PropertyIsNull>";
                if (!ExprDumpFilter(osFilter, expr->expr1, FALSE, pbNeedsNullCheck))
                    return FALSE;
                osFilter += "</PropertyIsNull></Not>";
                *pbNeedsNullCheck = TRUE;
                break;
            }

            const char* pszName = NULL;
            switch(expr->eType)
            {
                case TOKEN_EQUAL:           pszName = "PropertyIsEqualTo"; break;
                case TOKEN_NOT_EQUAL:       pszName = "PropertyIsNotEqualTo"; break;
                case TOKEN_LESSER_OR_EQUAL: pszName = "PropertyIsLessThanOrEqualTo"; break;
                case TOKEN_LESSER:          pszName = "PropertyIsLessThan"; break;
                case TOKEN_GREATER_OR_EQUAL:pszName = "PropertyIsGreaterThanOrEqualTo"; break;
                case TOKEN_GREATER:         pszName = "PropertyIsGreaterThan"; break;
                default: break;
            }
            osFilter += "<";
            osFilter += pszName;
            osFilter += ">";
            if (!ExprDumpFilter(osFilter, expr->expr1, FALSE, pbNeedsNullCheck))
                return FALSE;
            if (!ExprDumpFilter(osFilter, expr->expr2, FALSE, pbNeedsNullCheck))
                return FALSE;
            osFilter += "</";
            osFilter += pszName;
            osFilter += ">";
            break;
        }

        case TOKEN_AND:
        case TOKEN_OR:
        {
            const char* pszName = (expr->eType == TOKEN_AND) ? "And" : "Or";
            osFilter += "<";
            osFilter += pszName;
            osFilter += ">";
            if (!ExprDumpFilter(osFilter, expr->expr1, TRUE, pbNeedsNullCheck))
                return FALSE;
            if (!ExprDumpFilter(osFilter, expr->expr2, TRUE, pbNeedsNullCheck))
                return FALSE;
            osFilter += "</";
            osFilter += pszName;
            osFilter += ">";

            break;
        }

        default:
            return FALSE;
    }

    return TRUE;
}

static Expr* ExprBuildInternal(char*** ppapszTokens,
                               int bExpectClosingParenthesis)
{
    Expr* expr = NULL;
    Expr* op = NULL;
    Expr* val1 = NULL;
    Expr* val2 = NULL;
    CPLList* psValExprList = NULL;
    CPLList* psOpExprList = NULL;
    char** papszTokens = *ppapszTokens;
    char* pszToken = NULL;

#define PEEK_OP(my_op) my_op = (Expr*)CPLListGetData(psOpExprList)
#define PUSH_OP(my_op) psOpExprList = CPLListInsert(psOpExprList, my_op, 0)
#define POP_OP(my_op) do { my_op = (Expr*)CPLListGetData(psOpExprList);  \
                           if (my_op != NULL) { \
                                CPLList* psNext = psOpExprList->psNext; \
                                CPLFree(psOpExprList); \
                                psOpExprList = psNext; \
                           } \
                      } while(0)
#define PUSH_VAL(my_val) psValExprList = CPLListInsert(psValExprList, my_val, 0)
#define POP_VAL(my_val) do { my_val = (Expr*)CPLListGetData(psValExprList); \
                           if (my_val != NULL) { \
                                CPLList* psNext = psValExprList->psNext; \
                                CPLFree(psValExprList); \
                                psValExprList = psNext; \
                           } \
                      } while(0)

    int bExpectVarName = TRUE;
    int bExpectComparisonOperator = FALSE;
    int bExpectLogicalOperator = FALSE;
    int bExpectValue = FALSE;

    while(TRUE)
    {
        pszToken = *papszTokens;
        if (pszToken == NULL)
            break;
        papszTokens ++;

        if (EQUAL(pszToken, "("))
        {
            char** papszSub = papszTokens;
            Expr* expr = ExprBuildInternal(&papszSub, TRUE);
            if (expr == NULL)
                goto invalid_expr;
            PUSH_VAL(expr);
            papszTokens = papszSub;
            if (*papszTokens == NULL)
                break;

            bExpectVarName = FALSE;
            bExpectComparisonOperator = FALSE;
            bExpectLogicalOperator = TRUE;
            bExpectValue = FALSE;

            continue;
        }
        else if (EQUAL(pszToken, ")"))
        {
            if (bExpectClosingParenthesis)
                break;
            else
                goto invalid_expr;
        }

        if (bExpectVarName)
        {
            if (EQUAL(pszToken, "NOT"))
                op = ExprBuildOperator(TOKEN_NOT);
            else
            {
                PUSH_VAL(ExprBuildVarName(pszToken));
                bExpectVarName = FALSE;
                bExpectComparisonOperator = TRUE;
            }
        }
        else if (bExpectComparisonOperator)
        {
            bExpectComparisonOperator = FALSE;
            bExpectValue = TRUE;
            if (EQUAL(pszToken, "IS"))
            {
                if (*papszTokens != NULL && EQUAL(*papszTokens, "NOT"))
                {
                    op = ExprBuildOperator(TOKEN_NOT_EQUAL);
                    papszTokens ++;
                }
                else
                    op = ExprBuildOperator(TOKEN_EQUAL);
            }
            else if (EQUAL(pszToken, "="))
                op = ExprBuildOperator(TOKEN_EQUAL);
            else if (EQUAL(pszToken, "LIKE") || EQUAL(pszToken, "ILIKE"))
                op = ExprBuildOperator(TOKEN_LIKE);
            else if (EQUAL(pszToken, "!=") || EQUAL(pszToken, "<>"))
                op = ExprBuildOperator(TOKEN_NOT_EQUAL);
            else if (EQUAL(pszToken, "<"))
                op = ExprBuildOperator(TOKEN_LESSER);
            else if (EQUAL(pszToken, "<="))
                op = ExprBuildOperator(TOKEN_LESSER_OR_EQUAL);
            else if (EQUAL(pszToken, ">"))
                op = ExprBuildOperator(TOKEN_GREATER);
            else if (EQUAL(pszToken, ">="))
                op = ExprBuildOperator(TOKEN_GREATER_OR_EQUAL);
            else
                goto invalid_expr;
        }
        else if (bExpectLogicalOperator)
        {
            bExpectLogicalOperator = FALSE;
            bExpectVarName = TRUE;
            if (EQUAL(pszToken, "AND"))
                op = ExprBuildOperator(TOKEN_AND);
            else if (EQUAL(pszToken, "OR"))
                op = ExprBuildOperator(TOKEN_OR);
            else if (EQUAL(pszToken, "NOT"))
                op = ExprBuildOperator(TOKEN_NOT);
            else
                goto invalid_expr;
        }
        else if (bExpectValue)
        {
            PUSH_VAL(ExprBuildValue(pszToken));
            bExpectValue = FALSE;
            bExpectLogicalOperator = TRUE;
        }
        else
            goto invalid_expr;

        if (op != NULL)
        {
            Expr* prevOp;

            while(TRUE)
            {
                PEEK_OP(prevOp);

                if (prevOp != NULL &&
                    (ExprGetPriority(op) <= ExprGetPriority(prevOp)))
                {
                    if (prevOp->eType != TOKEN_NOT)
                    {
                        POP_VAL(val2);
                        if (val2 == NULL) goto invalid_expr;
                    }
                    POP_VAL(val1);
                    if (val1 == NULL) goto invalid_expr;

                    PUSH_VAL(ExprBuildBinary(prevOp->eType, val1, val2));
                    POP_OP(prevOp);
                    ExprFree(prevOp);
                    val1 = val2 = NULL;
                }
                else
                    break;
            }

            PUSH_OP(op);
            op = NULL;
        }

    }

    *ppapszTokens = papszTokens;

    while(TRUE)
    {
        POP_OP(op);
        if (op == NULL)
            break;
        if (op->eType != TOKEN_NOT)
        {
            POP_VAL(val2);
            if (val2 == NULL) goto invalid_expr;
        }
        POP_VAL(val1);
        if (val1 == NULL) goto invalid_expr;
        PUSH_VAL(ExprBuildBinary(op->eType, val1, val2));
        val1 = val2 = NULL;

        ExprFree(op);
        op = NULL;
    }

    POP_VAL(expr);
    return expr;

invalid_expr:
    ExprFree(op);
    ExprFree(val1);
    ExprFree(val2);
    ExprFreeList(psValExprList);
    ExprFreeList(psOpExprList);

    return NULL;
}

static char** Tokenize(const char* pszFilter)
{
    const char* pszIter = pszFilter;
    CPLString osToken;
    char** papszTokens = NULL;
    int bLastCharWasSep = TRUE;
    char prevCh = 0;
    char ch;
    int bInQuote = FALSE;
    char chQuoteChar = 0;

    if (pszFilter == NULL)
        return NULL;

    while((ch = *pszIter) != '\0')
    {
        if (bInQuote)
        {
            osToken += ch;
            if (ch == chQuoteChar)
            {
                papszTokens = CSLAddString(papszTokens, osToken.c_str());
                osToken = "";
                bInQuote = FALSE;
            }

            prevCh = ch;
            pszIter ++;
            continue;
        }

        if (ch == ' ')
        {
            if (!bLastCharWasSep)
            {
                if (osToken.size())
                    papszTokens = CSLAddString(papszTokens, osToken.c_str());
                osToken = "";
            }
            bLastCharWasSep = TRUE;
        }
        else if (ch == '(' || ch == ')' )
        {
            if (osToken.size())
                papszTokens = CSLAddString(papszTokens, osToken.c_str());
            char ach[2];
            ach[0] = ch;
            ach[1] = 0;
            papszTokens = CSLAddString(papszTokens, ach);
            osToken = "";
        }
        else if (ch == '<' || ch == '>' || ch == '!')
        {
            if (ch == '>' && prevCh == '<')
            {
                osToken += ch;
                papszTokens = CSLAddString(papszTokens, osToken.c_str());
                osToken = "";
            }
            else
            {
                if (osToken.size())
                    papszTokens = CSLAddString(papszTokens, osToken.c_str());
                char ach[2];
                ach[0] = ch;
                ach[1] = 0;
                osToken = ach;
            }
        }
        else if (ch == '=')
        {
            if (prevCh == '<' || prevCh == '>' || prevCh == '!')
                osToken += ch;
            else if (prevCh == '=')
                ;
            else
            {
                if (osToken.size())
                    papszTokens = CSLAddString(papszTokens, osToken.c_str());
                osToken = "=";
            }
        }
        else if (ch == '\'' || ch == '"')
        {
            if (osToken.size())
                papszTokens = CSLAddString(papszTokens, osToken.c_str());
            osToken = "'";
            bInQuote = TRUE;
            chQuoteChar = ch;
        }
        else
        {
            if (prevCh == '<' || prevCh == '>' || prevCh == '!' || prevCh == '=')
            {
                if (osToken.size())
                    papszTokens = CSLAddString(papszTokens, osToken.c_str());
                osToken = "";
            }
            osToken += ch;
        }

        bLastCharWasSep = (ch == ' ');

        prevCh = ch;
        pszIter ++;
    }
    if (osToken.size())
        papszTokens = CSLAddString(papszTokens, osToken.c_str());

    if (bInQuote)
    {
        CSLDestroy(papszTokens);
        papszTokens = NULL;
    }

    return papszTokens;
}

/************************************************************************/
/*                  TurnSQLFilterToWFSFilter()                          */
/************************************************************************/

CPLString TurnSQLFilterToWFSFilter( const char * pszFilter, int* pbNeedsNullCheck )
{
    char** papszTokens = Tokenize(pszFilter);

    if (papszTokens == NULL)
        return "";

    char** papszTokens2 = papszTokens;
    Expr* expr = ExprBuildInternal(&papszTokens2, FALSE);
    CSLDestroy(papszTokens);

    if (expr == NULL)
        return "";

    CPLString osFilter;
    if (!ExprDumpGmlObjectIdFilter(osFilter, expr))
    {
        osFilter = "";
        if (!ExprDumpFilter(osFilter, expr, TRUE, pbNeedsNullCheck))
            osFilter = "";
    }

    ExprFree(expr);

    return osFilter;
}
