/******************************************************************************
 * $Id$
 *
 * Project:  WFS Translator
 * Purpose:  Implements OGR SQL into OGC Filter translation.
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
    char*     pszVal;
    Expr*     expr1;
    Expr*     expr2;
};

/************************************************************************/
/*                      WFS_ExprGetPriority()                           */
/************************************************************************/

static int WFS_ExprGetPriority(const Expr* expr)
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

/************************************************************************/
/*                          WFS_ExprFree()                              */
/************************************************************************/

static void WFS_ExprFree(Expr* expr)
{
    if (expr == NULL) return;
    if (expr->expr1)
        WFS_ExprFree(expr->expr1);
    if (expr->expr2)
        WFS_ExprFree(expr->expr2);
    CPLFree(expr->pszVal);
    CPLFree(expr);
}

/************************************************************************/
/*                        WFS_ExprFreeList()                            */
/************************************************************************/

static void WFS_ExprFreeList(CPLList* psExprList)
{
    CPLList* psIterList = psExprList;
    while(psIterList)
    {
        WFS_ExprFree((Expr*)psIterList->pData);
        psIterList = psIterList->psNext;
    }
    CPLListDestroy(psExprList);
}

/************************************************************************/
/*                    WFS_ExprBuildVarName()                            */
/************************************************************************/

static Expr* WFS_ExprBuildVarName(const char* pszVal)
{
    Expr* expr = (Expr*)CPLCalloc(1, sizeof(Expr));
    expr->eType = TOKEN_VAR_NAME;
    expr->pszVal = CPLStrdup(pszVal);
    return expr;
}

/************************************************************************/
/*                      WFS_ExprBuildValue()                            */
/************************************************************************/

static Expr* WFS_ExprBuildValue(const char* pszVal)
{
    Expr* expr = (Expr*)CPLCalloc(1, sizeof(Expr));
    expr->eType = TOKEN_LITERAL;
    expr->pszVal = CPLStrdup(pszVal);
    return expr;
}

/************************************************************************/
/*                    WFS_ExprBuildOperator()                           */
/************************************************************************/

static Expr* WFS_ExprBuildOperator(TokenType eType)
{
    Expr* expr = (Expr*)CPLCalloc(1, sizeof(Expr));
    expr->eType = eType;
    return expr;
}

/************************************************************************/
/*                     WFS_ExprBuildBinary()                            */
/************************************************************************/

static Expr* WFS_ExprBuildBinary(TokenType eType, Expr* expr1, Expr* expr2)
{
    Expr* expr = (Expr*)CPLCalloc(1, sizeof(Expr));
    expr->eType = eType;
    expr->expr1 = expr1;
    expr->expr2 = expr2;
    return expr;
}

#ifdef notdef

/************************************************************************/
/*                          WFS_ExprDump()                              */
/************************************************************************/

static void WFS_ExprDump(FILE* fp, const Expr* expr)
{
    switch(expr->eType)
    {
        case TOKEN_VAR_NAME:
        case TOKEN_LITERAL:
            fprintf(fp, "%s", expr->pszVal);
            break;

        case TOKEN_NOT:
            fprintf(fp, "NOT (");
            WFS_ExprDump(fp, expr->expr1);
            fprintf(fp, ")");
            break;

        default:
            fprintf(fp, "(");
            WFS_ExprDump(fp, expr->expr1);
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
            WFS_ExprDump(fp, expr->expr2);
            fprintf(fp, ")");
            break;
    }
}
#endif

/************************************************************************/
/*                WFS_ExprDumpGmlObjectIdFilter()                       */
/************************************************************************/

static int WFS_ExprDumpGmlObjectIdFilter(CPLString& osFilter,
                                         const Expr* expr,
                                         int bUseFeatureId,
                                         int bGmlObjectIdNeedsGMLPrefix,
                                         int nVersion)
{
    if (expr->eType == TOKEN_EQUAL &&
        expr->expr1->eType == TOKEN_VAR_NAME &&
        EQUAL(expr->expr1->pszVal, "gml_id") &&
        expr->expr2->eType == TOKEN_LITERAL)
    {
        if (bUseFeatureId)
            osFilter += "<FeatureId fid=\"";
        else if (nVersion >= 200)
            osFilter += "<ResourceId rid=\"";
        else if (!bGmlObjectIdNeedsGMLPrefix)
            osFilter += "<GmlObjectId id=\"";
        else
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
        return WFS_ExprDumpGmlObjectIdFilter(osFilter, expr->expr1,
                                             bUseFeatureId, bGmlObjectIdNeedsGMLPrefix, nVersion) &&
               WFS_ExprDumpGmlObjectIdFilter(osFilter, expr->expr2,
                                             bUseFeatureId, bGmlObjectIdNeedsGMLPrefix, nVersion);
    }
    return FALSE;
}

/************************************************************************/
/*                     WFS_ExprDumpAsOGCFilter()                        */
/************************************************************************/

typedef struct
{
    int nVersion;
    int bPropertyIsNotEqualToSupported;
    int bOutNeedsNullCheck;
    OGRFeatureDefn* poFDefn;
} ExprDumpFilterOptions;

static int WFS_ExprDumpAsOGCFilter(CPLString& osFilter,
                                   const Expr* expr,
                                   int bExpectBinary,
                                   ExprDumpFilterOptions* psOptions)
{
    switch(expr->eType)
    {
        case TOKEN_VAR_NAME:
        {
            if (bExpectBinary)
                return FALSE;

            /* Special fields not understood by server */
            if (EQUAL(expr->pszVal, "gml_id") ||
                EQUAL(expr->pszVal, "FID") ||
                EQUAL(expr->pszVal, "OGR_GEOMETRY") ||
                EQUAL(expr->pszVal, "OGR_GEOM_WKT") ||
                EQUAL(expr->pszVal, "OGR_GEOM_AREA") ||
                EQUAL(expr->pszVal, "OGR_STYLE"))
            {
                CPLDebug("WFS", "Attribute refers to a OGR special field. Cannot use server-side filtering");
                return FALSE;
            }

            const char* pszFieldname;
            CPLString osVal;
            if (expr->pszVal[0] == '\'' || expr->pszVal[0] == '"')
            {
                osVal = expr->pszVal + 1;
                osVal.resize(osVal.size() - 1);
                pszFieldname = osVal.c_str();
            }
            else
                pszFieldname = expr->pszVal;

            if (psOptions->poFDefn->GetFieldIndex(pszFieldname) == -1)
            {
                CPLDebug("WFS", "Field '%s' unknown. Cannot use server-side filtering",
                         pszFieldname);
                return FALSE;
            }

            if (psOptions->nVersion >= 200)
                osFilter += "<ValueReference>";
            else
                osFilter += "<PropertyName>";
            char* pszFieldnameXML = CPLEscapeString(pszFieldname, -1, CPLES_XML);
            osFilter += pszFieldnameXML;
            CPLFree(pszFieldnameXML);
            if (psOptions->nVersion >= 200)
                osFilter += "</ValueReference>";
            else
                osFilter += "</PropertyName>";
            break;
        }

        case TOKEN_LITERAL:
        {
            if (bExpectBinary)
                return FALSE;

            const char* pszLiteral;
            CPLString osVal;
            if (expr->pszVal[0] == '\'' || expr->pszVal[0] == '"')
            {
                osVal = expr->pszVal + 1;
                osVal.resize(osVal.size() - 1);
                pszLiteral = osVal.c_str();
            }
            else
                pszLiteral = expr->pszVal;

            osFilter += "<Literal>";
            char* pszLiteralXML = CPLEscapeString(pszLiteral, -1, CPLES_XML);
            osFilter += pszLiteralXML;
            CPLFree(pszLiteralXML);
            osFilter += "</Literal>";

            break;
        }

        case TOKEN_NOT:
            osFilter += "<Not>";
            if (!WFS_ExprDumpAsOGCFilter(osFilter, expr->expr1, TRUE, psOptions))
                return FALSE;
            osFilter += "</Not>";
            break;

        case TOKEN_LIKE:
        {
            CPLString osVal;
            char ch;
            char firstCh = 0;
            int i;
            if (psOptions->nVersion == 100)
                osFilter += "<PropertyIsLike wildCard='*' singleChar='_' escape='!'>";
            else
                osFilter += "<PropertyIsLike wildCard='*' singleChar='_' escapeChar='!'>";
            if (!WFS_ExprDumpAsOGCFilter(osFilter, expr->expr1, FALSE, psOptions))
                return FALSE;
            if (expr->expr2->eType != TOKEN_LITERAL)
                return FALSE;
            osFilter += "<Literal>";

            /* Escape value according to above special characters */
            /* For URL compatibility reason, we remap the OGR SQL '%' wildchard into '*' */
            i = 0;
            ch = expr->expr2->pszVal[i];
            if (ch == '\'' || ch == '"')
            {
                firstCh = ch;
                i ++;
            }
            for(;(ch = expr->expr2->pszVal[i]) != '\0';i++)
            {
                if (ch == '%')
                    osVal += "*";
                else if (ch == '!')
                    osVal += "!!";
                else if (ch == '*')
                    osVal += "!*";
                else if (ch == firstCh && expr->expr2->pszVal[i + 1] == 0)
                    break;
                else
                {
                    char ach[2];
                    ach[0] = ch;
                    ach[1] = 0;
                    osVal += ach;
                }
            }
            osFilter += osVal;
            osFilter += "</Literal>";
            osFilter += "</PropertyIsLike>";
            break;
        }

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
                if (!WFS_ExprDumpAsOGCFilter(osFilter, expr->expr1, FALSE, psOptions))
                    return FALSE;
                osFilter += "</PropertyIsNull>";
                psOptions->bOutNeedsNullCheck = TRUE;
                break;
            }
            if (expr->eType == TOKEN_NOT_EQUAL && expr->expr2->eType == TOKEN_LITERAL &&
                EQUAL(expr->expr2->pszVal, "NULL"))
            {
                osFilter += "<Not><PropertyIsNull>";
                if (!WFS_ExprDumpAsOGCFilter(osFilter, expr->expr1, FALSE, psOptions))
                    return FALSE;
                osFilter += "</PropertyIsNull></Not>";
                psOptions->bOutNeedsNullCheck = TRUE;
                break;
            }
            TokenType eType = expr->eType;
            int bAddClosingNot = FALSE;
            if (!psOptions->bPropertyIsNotEqualToSupported && eType == TOKEN_NOT_EQUAL)
            {
                osFilter += "<Not>";
                eType = TOKEN_EQUAL;
                bAddClosingNot = TRUE;
            }

            const char* pszName = NULL;
            switch(eType)
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
            if (!WFS_ExprDumpAsOGCFilter(osFilter, expr->expr1, FALSE, psOptions))
                return FALSE;
            if (!WFS_ExprDumpAsOGCFilter(osFilter, expr->expr2, FALSE, psOptions))
                return FALSE;
            osFilter += "</";
            osFilter += pszName;
            osFilter += ">";
            if (bAddClosingNot)
                osFilter += "</Not>";
            break;
        }

        case TOKEN_AND:
        case TOKEN_OR:
        {
            const char* pszName = (expr->eType == TOKEN_AND) ? "And" : "Or";
            osFilter += "<";
            osFilter += pszName;
            osFilter += ">";
            if (!WFS_ExprDumpAsOGCFilter(osFilter, expr->expr1, TRUE, psOptions))
                return FALSE;
            if (!WFS_ExprDumpAsOGCFilter(osFilter, expr->expr2, TRUE, psOptions))
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

/************************************************************************/
/*                      WFS_ExprBuildInternal()                         */
/************************************************************************/

typedef struct
{
    int bExpectVarName;
    int bExpectComparisonOperator;
    int bExpectLogicalOperator;
    int bExpectValue;
    int nParenthesisLevel;
} ExprBuildContext;

static Expr* WFS_ExprBuildInternal(char*** ppapszTokens,
                                   ExprBuildContext* psBuildContext)
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
    while(TRUE)
    {
        pszToken = *papszTokens;
        if (pszToken == NULL)
            break;
        papszTokens ++;

        if (EQUAL(pszToken, "("))
        {
            char** papszSub = papszTokens;
            psBuildContext->nParenthesisLevel ++;
            Expr* expr = WFS_ExprBuildInternal(&papszSub, psBuildContext);
            psBuildContext->nParenthesisLevel --;
            if (expr == NULL)
                goto invalid_expr;
            PUSH_VAL(expr);
            papszTokens = papszSub;
            if (*papszTokens == NULL)
                break;

            continue;
        }
        else if (EQUAL(pszToken, ")"))
        {
            if (psBuildContext->nParenthesisLevel > 0)
                break;
            else
                goto invalid_expr;
        }

        if (psBuildContext->bExpectVarName)
        {
            if (EQUAL(pszToken, "NOT"))
                op = WFS_ExprBuildOperator(TOKEN_NOT);
            else
            {
                PUSH_VAL(WFS_ExprBuildVarName(pszToken));
                psBuildContext->bExpectVarName = FALSE;
                psBuildContext->bExpectComparisonOperator = TRUE;
            }
        }
        else if (psBuildContext->bExpectComparisonOperator)
        {
            psBuildContext->bExpectComparisonOperator = FALSE;
            psBuildContext->bExpectValue = TRUE;
            if (EQUAL(pszToken, "IS"))
            {
                if (*papszTokens != NULL && EQUAL(*papszTokens, "NOT"))
                {
                    op = WFS_ExprBuildOperator(TOKEN_NOT_EQUAL);
                    papszTokens ++;
                }
                else
                    op = WFS_ExprBuildOperator(TOKEN_EQUAL);
            }
            else if (EQUAL(pszToken, "="))
                op = WFS_ExprBuildOperator(TOKEN_EQUAL);
            else if (EQUAL(pszToken, "LIKE") || EQUAL(pszToken, "ILIKE"))
                op = WFS_ExprBuildOperator(TOKEN_LIKE);
            else if (EQUAL(pszToken, "!=") || EQUAL(pszToken, "<>"))
                op = WFS_ExprBuildOperator(TOKEN_NOT_EQUAL);
            else if (EQUAL(pszToken, "<"))
                op = WFS_ExprBuildOperator(TOKEN_LESSER);
            else if (EQUAL(pszToken, "<="))
                op = WFS_ExprBuildOperator(TOKEN_LESSER_OR_EQUAL);
            else if (EQUAL(pszToken, ">"))
                op = WFS_ExprBuildOperator(TOKEN_GREATER);
            else if (EQUAL(pszToken, ">="))
                op = WFS_ExprBuildOperator(TOKEN_GREATER_OR_EQUAL);
            else
                goto invalid_expr;
        }
        else if (psBuildContext->bExpectLogicalOperator)
        {
            psBuildContext->bExpectLogicalOperator = FALSE;
            psBuildContext->bExpectVarName = TRUE;
            if (EQUAL(pszToken, "AND"))
                op = WFS_ExprBuildOperator(TOKEN_AND);
            else if (EQUAL(pszToken, "OR"))
                op = WFS_ExprBuildOperator(TOKEN_OR);
            else if (EQUAL(pszToken, "NOT"))
                op = WFS_ExprBuildOperator(TOKEN_NOT);
            else
                goto invalid_expr;
        }
        else if (psBuildContext->bExpectValue)
        {
            PUSH_VAL(WFS_ExprBuildValue(pszToken));
            psBuildContext->bExpectValue = FALSE;
            psBuildContext->bExpectLogicalOperator = TRUE;
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
                    (WFS_ExprGetPriority(op) <= WFS_ExprGetPriority(prevOp)))
                {
                    if (prevOp->eType != TOKEN_NOT)
                    {
                        POP_VAL(val2);
                        if (val2 == NULL) goto invalid_expr;
                    }
                    POP_VAL(val1);
                    if (val1 == NULL) goto invalid_expr;

                    PUSH_VAL(WFS_ExprBuildBinary(prevOp->eType, val1, val2));
                    POP_OP(prevOp);
                    WFS_ExprFree(prevOp);
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
        PUSH_VAL(WFS_ExprBuildBinary(op->eType, val1, val2));
        val1 = val2 = NULL;

        WFS_ExprFree(op);
        op = NULL;
    }

    POP_VAL(expr);
    return expr;

invalid_expr:
    WFS_ExprFree(op);
    WFS_ExprFree(val1);
    WFS_ExprFree(val2);
    WFS_ExprFreeList(psValExprList);
    WFS_ExprFreeList(psOpExprList);

    return NULL;
}

/************************************************************************/
/*                         WFS_ExprTokenize()                           */
/************************************************************************/

static char** WFS_ExprTokenize(const char* pszFilter)
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
/*               WFS_TurnSQLFilterToOGCFilter()                         */
/************************************************************************/

CPLString WFS_TurnSQLFilterToOGCFilter( const char * pszFilter,
                                        OGRFeatureDefn* poFDefn,
                                        int nVersion,
                                        int bPropertyIsNotEqualToSupported,
                                        int bUseFeatureId,
                                        int bGmlObjectIdNeedsGMLPrefix,
                                        int* pbOutNeedsNullCheck )
{
    char** papszTokens = WFS_ExprTokenize(pszFilter);

    if (papszTokens == NULL)
        return "";

    char** papszTokens2 = papszTokens;

    ExprBuildContext sBuildContext;
    sBuildContext.bExpectVarName = TRUE;
    sBuildContext.bExpectComparisonOperator = FALSE;
    sBuildContext.bExpectLogicalOperator = FALSE;
    sBuildContext.bExpectValue = FALSE;
    sBuildContext.nParenthesisLevel = 0;
    Expr* expr = WFS_ExprBuildInternal(&papszTokens2, &sBuildContext);
    CSLDestroy(papszTokens);

    if (expr == NULL)
        return "";

    CPLString osFilter;
    /* If the filter is only made of querying one or several gml_id */
    /* (with OR operator), we turn this to <GmlObjectId> list */
    if (!WFS_ExprDumpGmlObjectIdFilter(osFilter, expr, bUseFeatureId,
                                       bGmlObjectIdNeedsGMLPrefix, nVersion))
    {
        ExprDumpFilterOptions sOptions;
        sOptions.nVersion = nVersion;
        sOptions.bPropertyIsNotEqualToSupported = bPropertyIsNotEqualToSupported;
        sOptions.bOutNeedsNullCheck = FALSE;
        sOptions.poFDefn = poFDefn;
        osFilter = "";
        if (!WFS_ExprDumpAsOGCFilter(osFilter, expr, TRUE, &sOptions))
            osFilter = "";
        *pbOutNeedsNullCheck = sOptions.bOutNeedsNullCheck;
    }

    WFS_ExprFree(expr);

    return osFilter;
}
