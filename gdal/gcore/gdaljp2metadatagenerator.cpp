/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  GDALJP2Metadata: metadata generator
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, European Union Satellite Centre
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

#include <vector>
#include "gdaljp2metadatagenerator.h"

CPL_CVSID("$Id$");

#ifdef HAVE_LIBXML2

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

/************************************************************************/
/*                            GDALGMLJP2Expr                            */
/************************************************************************/

typedef enum 
{
    GDALGMLJP2Expr_Unknown,
    GDALGMLJP2Expr_STRING_LITERAL,
    GDALGMLJP2Expr_NUMERIC_LITERAL,
    GDALGMLJP2Expr_XPATH,
    GDALGMLJP2Expr_ADD,
    GDALGMLJP2Expr_SUB,
    GDALGMLJP2Expr_NOT,
    GDALGMLJP2Expr_AND,
    GDALGMLJP2Expr_OR,
    GDALGMLJP2Expr_EQ,
    GDALGMLJP2Expr_NEQ,
    GDALGMLJP2Expr_LT,
    GDALGMLJP2Expr_LE,
    GDALGMLJP2Expr_GT,
    GDALGMLJP2Expr_GE,
    GDALGMLJP2Expr_IF,
    GDALGMLJP2Expr_CONCAT,
    GDALGMLJP2Expr_EVAL,
    GDALGMLJP2Expr_CAST_TO_STRING,
    GDALGMLJP2Expr_CAST_TO_NUMERIC,
    GDALGMLJP2Expr_SUBSTRING,
    GDALGMLJP2Expr_SUBSTRING_BEFORE,
    GDALGMLJP2Expr_SUBSTRING_AFTER,
    GDALGMLJP2Expr_STRING_LENGTH,
    GDALGMLJP2Expr_UUID
} GDALGMLJP2ExprType;

// {{{IF(EQ(XPATH(), '5'), '', '')}}}

class GDALGMLJP2Expr
{
        static void             SkipSpaces(const char*& pszStr);
        static GDALGMLJP2Expr*  BuildNaryOp(const char* pszOriStr,
                                            const char*& pszStr, int nary);

    public:
        GDALGMLJP2ExprType           eType;
        CPLString                    osValue;
        std::vector<GDALGMLJP2Expr*> apoSubExpr;

                                GDALGMLJP2Expr(): eType(GDALGMLJP2Expr_Unknown) {}
                                GDALGMLJP2Expr(const char* pszVal): eType(GDALGMLJP2Expr_STRING_LITERAL), osValue(pszVal) {}
                                GDALGMLJP2Expr(CPLString osVal): eType(GDALGMLJP2Expr_STRING_LITERAL), osValue(osVal) {}
                                GDALGMLJP2Expr(bool b): eType(GDALGMLJP2Expr_STRING_LITERAL), osValue(b ? "true" : "false") {}
                               ~GDALGMLJP2Expr();

        GDALGMLJP2Expr          Evaluate(xmlXPathContextPtr pXPathCtx,
                                         xmlDocPtr pDoc);

        static GDALGMLJP2Expr*  Build(const char* pszOriStr,
                                      const char*& pszStr);
        static void             ReportError(const char* pszOriStr,
                                            const char* pszStr,
                                            const char* pszIntroMessage = "Parsing error at:\n");
};

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

GDALGMLJP2Expr::~GDALGMLJP2Expr()
{
    for(size_t i=0;i<apoSubExpr.size();i++)
        delete apoSubExpr[i];
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

void GDALGMLJP2Expr::ReportError(const char* pszOriStr,
                                 const char* pszStr,
                                 const char* pszIntroMessage)
{
    size_t nDist = (size_t)(pszStr - pszOriStr);
    if( nDist > 40 )
        nDist = 40;
    CPLString osErrMsg(pszIntroMessage);
    CPLString osInvalidExpr = CPLString(pszStr - nDist).substr(0, nDist + 20);
    for(int i=(int)nDist-1;i>=0;i--)
    {
        if( osInvalidExpr[i] == '\n' )
        {
            osInvalidExpr = osInvalidExpr.substr(i+1);
            nDist -= i+1;
            break;
        }
    }
    for(size_t i=nDist;i<osInvalidExpr.size();i++)
    {
        if( osInvalidExpr[i] == '\n' )
        {
            osInvalidExpr.resize(i);
            break;
        }
    }
    osErrMsg += osInvalidExpr;
    osErrMsg += "\n";
    for(size_t i=0;i<nDist;i++)
        osErrMsg += " ";
    osErrMsg += "^";
    CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrMsg.c_str());
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

void GDALGMLJP2Expr::SkipSpaces(const char*& pszStr)
{
    while( *pszStr == ' ' || *pszStr == '\t' || *pszStr == '\r' || *pszStr ==  '\n' )
        pszStr ++;
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

GDALGMLJP2Expr* GDALGMLJP2Expr::BuildNaryOp(const char* pszOriStr,
                                            const char*& pszStr, int nary)
{
    GDALGMLJP2Expr* poExpr = new GDALGMLJP2Expr();

    SkipSpaces(pszStr);
    if( *pszStr != '(' )
    {
        ReportError(pszOriStr, pszStr);
        delete poExpr;
        return NULL;
    }
    pszStr ++;
    SkipSpaces(pszStr);
    for( int i=0;nary < 0 || i<nary;i++)
    {
        GDALGMLJP2Expr* poSubExpr = Build(pszOriStr, pszStr);
        if( poSubExpr == NULL )
        {
            delete poExpr;
            return NULL;
        }
        SkipSpaces(pszStr);
        poExpr->apoSubExpr.push_back(poSubExpr);
        if( nary < 0 && *pszStr == ')' )
        {
            break;
        }
        else if( nary < 0 || i < nary - 1 )
        {
            if( *pszStr != ',' )
            {
                ReportError(pszOriStr, pszStr);
                delete poExpr;
                return NULL;
            }
            pszStr ++;
            SkipSpaces(pszStr);
        }
    }
    if( *pszStr != ')' )
    {
        ReportError(pszOriStr, pszStr);
        delete poExpr;
        return NULL;
    }
    pszStr ++;
    return poExpr;
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

typedef struct
{
    const char*        pszOp;
    GDALGMLJP2ExprType eType;
    int                nary;
} GDALGMLJP2Operators;

GDALGMLJP2Expr* GDALGMLJP2Expr::Build(const char* pszOriStr,
                                      const char*& pszStr)
{
    if( EQUALN(pszStr, "{{{", strlen("{{{")) )
    {
        pszStr += strlen("{{{");
        SkipSpaces(pszStr);
        GDALGMLJP2Expr* poExpr = Build(pszOriStr, pszStr);
        if( poExpr == NULL )
            return NULL;
        SkipSpaces(pszStr);
        if( !EQUALN(pszStr, "}}}", strlen("}}}")) )
        {
            ReportError(pszOriStr, pszStr);
            delete poExpr;
            return NULL;
        }
        pszStr += strlen("}}}");
        return poExpr;
    }
    else if( EQUALN(pszStr, "XPATH", strlen("XPATH")) )
    {
        pszStr += strlen("XPATH");
        SkipSpaces(pszStr);
        if( *pszStr != '(' )
        {
            ReportError(pszOriStr, pszStr);
            return NULL;
        }
        pszStr ++;
        SkipSpaces(pszStr);
        CPLString osValue;
        int nParenthesisIndent = 0;
        char chLiteralQuote = '\0';
        while( *pszStr )
        {
            if( chLiteralQuote != '\0' )
            {
                if( *pszStr == chLiteralQuote )
                    chLiteralQuote = '\0';
                osValue += *pszStr;
                pszStr++;
            }
            else if( *pszStr == '\'' || *pszStr == '"' )
            {
                chLiteralQuote = *pszStr;
                osValue += *pszStr;
                pszStr++;
            }
            else if( *pszStr == '(' )
            {
                nParenthesisIndent ++;
                osValue += *pszStr;
                pszStr++;
            }
            else if( *pszStr == ')' )
            {
                nParenthesisIndent --;
                if( nParenthesisIndent < 0 )
                {
                    pszStr++;
                    GDALGMLJP2Expr* poExpr = new GDALGMLJP2Expr();
                    poExpr->eType = GDALGMLJP2Expr_XPATH;
                    poExpr->osValue = osValue;
                    //CPLDebug("GMLJP2", "XPath expression '%s'", osValue.c_str());
                    return poExpr;
                }
                osValue += *pszStr;
                pszStr++;
            }
            else
            {
                osValue += *pszStr;
                pszStr++;
            }
        }
        ReportError(pszOriStr, pszStr);
        return NULL;
    }
    else if( pszStr[0] == '\'' )
    {
        pszStr ++;
        CPLString osValue;
        while( *pszStr )
        {
            if( *pszStr == '\\' )
            {
                if( pszStr[1] == '\\' )
                    osValue += "\\";
                else if( pszStr[1] == '\'' )
                    osValue += "\'";
                else
                {
                    ReportError(pszOriStr, pszStr);
                    return NULL;
                }
                pszStr += 2;
            }
            else if( *pszStr == '\'' )
            {
                pszStr ++;
                GDALGMLJP2Expr* poExpr = new GDALGMLJP2Expr();
                poExpr->eType = GDALGMLJP2Expr_STRING_LITERAL;
                poExpr->osValue = osValue;
                return poExpr;
            }
            else
            {
                osValue += *pszStr;
                pszStr ++;
            }
        }
        ReportError(pszOriStr, pszStr);
        return NULL;
    }
    else if( pszStr[0] == '-' || pszStr[0] == '.'||
             (pszStr[0] >= '0' && pszStr[0] <= '9') )
    {
        CPLString osValue;
        while( *pszStr )
        {
            if( *pszStr == ' ' || *pszStr == ')' || *pszStr == ',' || *pszStr == '}' )
            {
                GDALGMLJP2Expr* poExpr = new GDALGMLJP2Expr();
                poExpr->eType = GDALGMLJP2Expr_NUMERIC_LITERAL;
                poExpr->osValue = osValue;
                return poExpr;
            }
            osValue += *pszStr;
            pszStr ++;
        }
        ReportError(pszOriStr, pszStr);
        return NULL;
    }
    else
    {
        static const GDALGMLJP2Operators asOperators[] =
        {
            { "IF", GDALGMLJP2Expr_IF, 3 },
            { "ADD", GDALGMLJP2Expr_ADD, 2 },
            { "AND", GDALGMLJP2Expr_AND, 2 },
            { "OR", GDALGMLJP2Expr_OR, 2 },
            { "NOT", GDALGMLJP2Expr_NOT, 1 },
            { "EQ", GDALGMLJP2Expr_EQ, 2 },
            { "NEQ", GDALGMLJP2Expr_NEQ, 2 },
            { "LT", GDALGMLJP2Expr_LT, 2 },
            { "LE", GDALGMLJP2Expr_LE, 2 },
            { "GT", GDALGMLJP2Expr_GT, 2 },
            { "GE", GDALGMLJP2Expr_GE, 2 },
            { "CONCAT", GDALGMLJP2Expr_CONCAT, -1 },
            { "NUMERIC", GDALGMLJP2Expr_CAST_TO_NUMERIC, 1 },
            { "STRING_LENGTH", GDALGMLJP2Expr_STRING_LENGTH, 1 },
            { "STRING", GDALGMLJP2Expr_CAST_TO_STRING, 1 }, /* must be after */
            { "SUBSTRING_BEFORE", GDALGMLJP2Expr_SUBSTRING_BEFORE, 2 },
            { "SUBSTRING_AFTER", GDALGMLJP2Expr_SUBSTRING_AFTER, 2 },
            { "SUBSTRING", GDALGMLJP2Expr_SUBSTRING, 3 }, /* must be after */
            { "SUB", GDALGMLJP2Expr_SUB, 2 }, /* must be after */
            { "UUID", GDALGMLJP2Expr_UUID, 0},
            { "EVAL", GDALGMLJP2Expr_EVAL, 1}
        };
        for(size_t i=0; i<sizeof(asOperators)/sizeof(asOperators[0]);i++)
        {
            const char* pszOp = asOperators[i].pszOp;
            if( EQUALN(pszStr, pszOp, strlen(pszOp)) )
            {
                pszStr += strlen(pszOp);
                GDALGMLJP2Expr* poExpr = BuildNaryOp(pszOriStr, pszStr, asOperators[i].nary);
                if( poExpr )
                {
                    if( asOperators[i].eType == GDALGMLJP2Expr_NEQ )
                    {
                        poExpr->eType = GDALGMLJP2Expr_EQ;
                        GDALGMLJP2Expr* poTopExpr = new GDALGMLJP2Expr();
                        poTopExpr->eType = GDALGMLJP2Expr_NOT;
                        poTopExpr->apoSubExpr.push_back(poExpr);
                        poExpr = poTopExpr;
                    }
                    else if( asOperators[i].eType == GDALGMLJP2Expr_GT )
                    {
                        poExpr->eType = GDALGMLJP2Expr_LE;
                        GDALGMLJP2Expr* poTopExpr = new GDALGMLJP2Expr();
                        poTopExpr->eType = GDALGMLJP2Expr_NOT;
                        poTopExpr->apoSubExpr.push_back(poExpr);
                        poExpr = poTopExpr;
                    }
                    else if( asOperators[i].eType == GDALGMLJP2Expr_GE )
                    {
                        poExpr->eType = GDALGMLJP2Expr_LT;
                        GDALGMLJP2Expr* poTopExpr = new GDALGMLJP2Expr();
                        poTopExpr->eType = GDALGMLJP2Expr_NOT;
                        poTopExpr->apoSubExpr.push_back(poExpr);
                        poExpr = poTopExpr;
                    }
                    else
                        poExpr->eType = asOperators[i].eType;
                }
                return poExpr;
            }
        }
        ReportError(pszOriStr, pszStr);
        return NULL;
    }
}

/************************************************************************/
/*                       GDALGMLJP2HexFormatter()                       */
/************************************************************************/

static const char* GDALGMLJP2HexFormatter(GByte nVal)
{
    return CPLSPrintf("%02X", nVal);
}

/************************************************************************/
/*                            Evaluate()                                */
/************************************************************************/

static CPLString GDALGMLJP2EvalExpr(const CPLString& osTemplate,
                                    xmlXPathContextPtr pXPathCtx,
                                    xmlDocPtr pDoc);

GDALGMLJP2Expr GDALGMLJP2Expr::Evaluate(xmlXPathContextPtr pXPathCtx,
                                   xmlDocPtr pDoc)
{
    switch(eType)
    {
        case GDALGMLJP2Expr_STRING_LITERAL:
        case GDALGMLJP2Expr_NUMERIC_LITERAL:
            return *this;

        case GDALGMLJP2Expr_XPATH:
        {
            xmlXPathObjectPtr pXPathObj = xmlXPathEvalExpression(
                    (const xmlChar*)osValue.c_str(), pXPathCtx);
            if( pXPathObj == NULL )
                return GDALGMLJP2Expr("");

            // Add result of the evaluation
            CPLString osXMLRes;
            if( pXPathObj->type == XPATH_STRING )
                osXMLRes = (const char*)pXPathObj->stringval;
            else if( pXPathObj->type == XPATH_BOOLEAN )
                osXMLRes = pXPathObj->boolval ? "true" : "false";
            else if( pXPathObj->type == XPATH_NUMBER )
                osXMLRes = CPLSPrintf("%.16g", pXPathObj->floatval);
            else if( pXPathObj->type == XPATH_NODESET )
            {
                xmlNodeSetPtr pNodes = pXPathObj->nodesetval;
                int nNodes = (pNodes) ? pNodes->nodeNr : 0;
                for(int i=0;i<nNodes;i++)
                {
                    xmlNodePtr pCur = pNodes->nodeTab[i];

                    xmlBufferPtr pBuf = xmlBufferCreate();
                    xmlNodeDump(pBuf, pDoc, pCur, 2, 1);
                    osXMLRes += (const char*)xmlBufferContent(pBuf);
                    xmlBufferFree(pBuf);
                }
            }

            xmlXPathFreeObject(pXPathObj);
            return GDALGMLJP2Expr(osXMLRes);
        }

        case GDALGMLJP2Expr_AND:
        {
            return GDALGMLJP2Expr(
                apoSubExpr[0]->Evaluate(pXPathCtx,pDoc).osValue == "true" &&
                apoSubExpr[1]->Evaluate(pXPathCtx,pDoc).osValue == "true" );
        }

        case GDALGMLJP2Expr_OR:
        {
            return GDALGMLJP2Expr(
                apoSubExpr[0]->Evaluate(pXPathCtx,pDoc).osValue == "true" ||
                apoSubExpr[1]->Evaluate(pXPathCtx,pDoc).osValue == "true" );
        }

        case GDALGMLJP2Expr_NOT:
        {
            return GDALGMLJP2Expr(
                apoSubExpr[0]->Evaluate(pXPathCtx,pDoc).osValue != "true");
        }

        case GDALGMLJP2Expr_ADD:
        {
            const GDALGMLJP2Expr& oExpr1 = apoSubExpr[0]->Evaluate(pXPathCtx,pDoc);
            const GDALGMLJP2Expr& oExpr2 = apoSubExpr[1]->Evaluate(pXPathCtx,pDoc);
            GDALGMLJP2Expr oExpr(CPLSPrintf("%.16g", CPLAtof(oExpr1.osValue) + CPLAtof(oExpr2.osValue)));
            oExpr.eType = GDALGMLJP2Expr_NUMERIC_LITERAL;
            return oExpr;
        }

        case GDALGMLJP2Expr_SUB:
        {
            const GDALGMLJP2Expr& oExpr1 = apoSubExpr[0]->Evaluate(pXPathCtx,pDoc);
            const GDALGMLJP2Expr& oExpr2 = apoSubExpr[1]->Evaluate(pXPathCtx,pDoc);
            GDALGMLJP2Expr oExpr(CPLSPrintf("%.16g", CPLAtof(oExpr1.osValue) - CPLAtof(oExpr2.osValue)));
            oExpr.eType = GDALGMLJP2Expr_NUMERIC_LITERAL;
            return oExpr;
        }

        case GDALGMLJP2Expr_EQ:
        {
            const GDALGMLJP2Expr& oExpr1 = apoSubExpr[0]->Evaluate(pXPathCtx,pDoc);
            const GDALGMLJP2Expr& oExpr2 = apoSubExpr[1]->Evaluate(pXPathCtx,pDoc);
            bool bRes;
            if( oExpr1.eType == GDALGMLJP2Expr_NUMERIC_LITERAL &&
                oExpr2.eType == GDALGMLJP2Expr_NUMERIC_LITERAL )
            {
                bRes = ( CPLAtof(oExpr1.osValue) == CPLAtof(oExpr2.osValue) );
            }
            else
            {
                bRes = (oExpr1.osValue == oExpr2.osValue );
            }
            return GDALGMLJP2Expr(bRes);
        }

        case GDALGMLJP2Expr_LT:
        {
            const GDALGMLJP2Expr& oExpr1 = apoSubExpr[0]->Evaluate(pXPathCtx,pDoc);
            const GDALGMLJP2Expr& oExpr2 = apoSubExpr[1]->Evaluate(pXPathCtx,pDoc);
            bool bRes;
            if( oExpr1.eType == GDALGMLJP2Expr_NUMERIC_LITERAL &&
                oExpr2.eType == GDALGMLJP2Expr_NUMERIC_LITERAL )
            {
                bRes = ( CPLAtof(oExpr1.osValue) < CPLAtof(oExpr2.osValue) );
            }
            else
            {
                bRes = (oExpr1.osValue < oExpr2.osValue );
            }
            return GDALGMLJP2Expr(bRes);
        }

        case GDALGMLJP2Expr_LE:
        {
            const GDALGMLJP2Expr& oExpr1 = apoSubExpr[0]->Evaluate(pXPathCtx,pDoc);
            const GDALGMLJP2Expr& oExpr2 = apoSubExpr[1]->Evaluate(pXPathCtx,pDoc);
            bool bRes;
            if( oExpr1.eType == GDALGMLJP2Expr_NUMERIC_LITERAL &&
                oExpr2.eType == GDALGMLJP2Expr_NUMERIC_LITERAL )
            {
                bRes = ( CPLAtof(oExpr1.osValue) <= CPLAtof(oExpr2.osValue) );
            }
            else
            {
                bRes = (oExpr1.osValue <= oExpr2.osValue );
            }
            return GDALGMLJP2Expr(bRes);
        }

        case GDALGMLJP2Expr_IF:
        {
            if( apoSubExpr[0]->Evaluate(pXPathCtx,pDoc).osValue == "true" )
                return apoSubExpr[1]->Evaluate(pXPathCtx,pDoc);
            else
                return apoSubExpr[2]->Evaluate(pXPathCtx,pDoc);
        }

        case GDALGMLJP2Expr_EVAL:
        {
            return GDALGMLJP2Expr(
                GDALGMLJP2EvalExpr(apoSubExpr[0]->Evaluate(pXPathCtx,pDoc).osValue,pXPathCtx,pDoc));
        }

        case GDALGMLJP2Expr_CONCAT:
        {
            CPLString osRet;
            for(size_t i=0;i<apoSubExpr.size();i++)
                osRet += apoSubExpr[i]->Evaluate(pXPathCtx,pDoc).osValue;
            return GDALGMLJP2Expr(osRet);
        }

        case GDALGMLJP2Expr_CAST_TO_NUMERIC:
        {
            GDALGMLJP2Expr oExpr = CPLSPrintf("%.16g", CPLAtof(apoSubExpr[0]->Evaluate(pXPathCtx,pDoc).osValue));
            oExpr.eType = GDALGMLJP2Expr_NUMERIC_LITERAL;
            return oExpr;
        }

        case GDALGMLJP2Expr_CAST_TO_STRING:
        {
            GDALGMLJP2Expr oExpr = apoSubExpr[0]->Evaluate(pXPathCtx,pDoc).osValue;
            oExpr.eType = GDALGMLJP2Expr_STRING_LITERAL;
            return oExpr;
        }
        
        case GDALGMLJP2Expr_UUID:
        {
            CPLString osRet;
            static int nCounter = 0;
            srand((unsigned int)time(NULL) + nCounter);
            nCounter ++;
            for( int i=0; i<4; i ++ )
                osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
            osRet += "-";
            osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
            osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
            osRet += "-";
            osRet += GDALGMLJP2HexFormatter((rand() & 0x0F) | 0x40); // set the version number bits (4 == random)
            osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
            osRet += "-";
            osRet += GDALGMLJP2HexFormatter((rand() & 0x3F) | 0x80); // set the variant bits
            osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
            osRet += "-";
            for( int i=0; i<6; i ++ )
                osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
            return GDALGMLJP2Expr(osRet);
        }
        
        case GDALGMLJP2Expr_STRING_LENGTH:
        {
            GDALGMLJP2Expr oExpr(CPLSPrintf("%d",
                (int)strlen(apoSubExpr[0]->Evaluate(pXPathCtx,pDoc).osValue)));
            oExpr.eType = GDALGMLJP2Expr_NUMERIC_LITERAL;
            return oExpr;
        }

        case GDALGMLJP2Expr_SUBSTRING:
        {
            const GDALGMLJP2Expr& oExpr1 = apoSubExpr[0]->Evaluate(pXPathCtx,pDoc);
            const GDALGMLJP2Expr& oExpr2 = apoSubExpr[1]->Evaluate(pXPathCtx,pDoc);
            const GDALGMLJP2Expr& oExpr3 = apoSubExpr[2]->Evaluate(pXPathCtx,pDoc);
            int nStart = atoi(oExpr2.osValue);
            int nLen = atoi(oExpr3.osValue);
            nStart --; /* from XPath/SQL convention to C convention */
            if( nStart < 0 )
            {
                nLen += nStart;
                nStart = 0;
            }
            if( nLen < 0 )
                return GDALGMLJP2Expr("");
            if( nStart >= (int)oExpr1.osValue.size() )
                return GDALGMLJP2Expr("");
            if( nStart + nLen > (int)oExpr1.osValue.size() )
                nLen = (int)oExpr1.osValue.size() - nStart;
            return GDALGMLJP2Expr(oExpr1.osValue.substr(nStart, nLen));
        }

        case GDALGMLJP2Expr_SUBSTRING_BEFORE:
        {
            const GDALGMLJP2Expr& oExpr1 = apoSubExpr[0]->Evaluate(pXPathCtx,pDoc);
            const GDALGMLJP2Expr& oExpr2 = apoSubExpr[1]->Evaluate(pXPathCtx,pDoc);
            size_t nPos = oExpr1.osValue.find(oExpr2.osValue);
            if( nPos == std::string::npos )
                return GDALGMLJP2Expr("");
            return GDALGMLJP2Expr(oExpr1.osValue.substr(0, nPos));
        }

        case GDALGMLJP2Expr_SUBSTRING_AFTER:
        {
            const GDALGMLJP2Expr& oExpr1 = apoSubExpr[0]->Evaluate(pXPathCtx,pDoc);
            const GDALGMLJP2Expr& oExpr2 = apoSubExpr[1]->Evaluate(pXPathCtx,pDoc);
            size_t nPos = oExpr1.osValue.find(oExpr2.osValue);
            if( nPos == std::string::npos )
                return GDALGMLJP2Expr("");
            return GDALGMLJP2Expr(oExpr1.osValue.substr(nPos + oExpr2.osValue.size()));
        }

        default:
            CPLAssert(FALSE);
            return GDALGMLJP2Expr("");
    }
}

/************************************************************************/
/*                        GDALGMLJP2EvalExpr()                          */
/************************************************************************/

static CPLString GDALGMLJP2EvalExpr(const CPLString& osTemplate,
                                    xmlXPathContextPtr pXPathCtx,
                                    xmlDocPtr pDoc)
{
    CPLString osXMLRes;
    size_t nPos = 0;
    while( TRUE )
    {
        // Get next expression
        size_t nStartPos = osTemplate.find("{{{", nPos);
        if( nStartPos == std::string::npos)
        {
            // Add terminating portion of the template
            osXMLRes += osTemplate.substr(nPos);
            break;
        }

        // Add portion of template before the expression
        osXMLRes += osTemplate.substr(nPos, nStartPos - nPos);
        
        const char* pszExpr = osTemplate.c_str() + nStartPos;
        GDALGMLJP2Expr* poExpr = GDALGMLJP2Expr::Build(pszExpr, pszExpr);
        if( poExpr == NULL )
            break;
        nPos = (size_t)(pszExpr - osTemplate.c_str());
        osXMLRes += poExpr->Evaluate(pXPathCtx,pDoc).osValue;
        delete poExpr;
    }
    return osXMLRes;
}

/************************************************************************/
/*                      GDALGMLJP2XPathErrorHandler()                   */
/************************************************************************/

static void GDALGMLJP2XPathErrorHandler(CPL_UNUSED void * userData, 
                                        xmlErrorPtr error)
{
    if( error->domain == XML_FROM_XPATH &&
        error->str1 != NULL &&
        error->int1 < (int)strlen(error->str1) )
    {
        GDALGMLJP2Expr::ReportError(error->str1,
                                    error->str1 + error->int1,
                                    "XPath error:\n");
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "An error occured in libxml2");
    }
}

/************************************************************************/
/*                    GDALGMLJP2RegisterNamespaces()                    */
/************************************************************************/

static void GDALGMLJP2RegisterNamespaces(xmlXPathContextPtr pXPathCtx,
                                         xmlNode* pNode)
{
    for(; pNode; pNode = pNode->next)
    {
        if( pNode->type == XML_ELEMENT_NODE)
        {
            if( pNode->ns != NULL && pNode->ns->prefix != NULL )
            {
                //printf("%s %s\n",pNode->ns->prefix, pNode->ns->href);
                if(xmlXPathRegisterNs(pXPathCtx, pNode->ns->prefix, pNode->ns->href) != 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Registration of namespace %s failed",
                             (const char*)pNode->ns->prefix);
                }
            }
        }

        GDALGMLJP2RegisterNamespaces(pXPathCtx, pNode->children);
    }
}

#endif /* defined(LIBXML2) */

/************************************************************************/
/*                      GDALGMLJP2GenerateMetadata()                    */
/************************************************************************/

CPLXMLNode* GDALGMLJP2GenerateMetadata(const CPLString& osTemplateFile,
                                       const CPLString& osSourceFile)
{
#ifndef HAVE_LIBXML2
    return NULL;
#else
    GByte* pabyStr = NULL;
    if( !VSIIngestFile( NULL, osTemplateFile, &pabyStr, NULL, -1 ) )
        return NULL;
    CPLString osTemplate((const char*)pabyStr);
    CPLFree(pabyStr);
    
    if( !VSIIngestFile( NULL, osSourceFile, &pabyStr, NULL, -1 ) )
        return NULL;
    CPLString osSource((const char*)pabyStr);
    CPLFree(pabyStr);
    
    xmlDocPtr pDoc = xmlParseDoc((const xmlChar *)osSource.c_str());
    if( pDoc == NULL )
        return NULL;

    xmlXPathContextPtr pXPathCtx = xmlXPathNewContext(pDoc);
    if( pXPathCtx == NULL )
    {
        xmlFreeDoc(pDoc); 
        return NULL;
    }
    pXPathCtx->error = GDALGMLJP2XPathErrorHandler;
    
    GDALGMLJP2RegisterNamespaces(pXPathCtx, xmlDocGetRootElement(pDoc));

    CPLString osXMLRes = GDALGMLJP2EvalExpr(osTemplate, pXPathCtx, pDoc);

    xmlXPathFreeContext(pXPathCtx); 
    xmlFreeDoc(pDoc); 

    return CPLParseXMLString(osXMLRes);
#endif
}
