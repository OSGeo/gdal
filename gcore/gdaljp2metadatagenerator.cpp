/******************************************************************************
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

#include "cpl_port.h"
#include "gdaljp2metadatagenerator.h"

#include <cstddef>

CPL_CVSID("$Id$")

#ifdef HAVE_LIBXML2

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// For CHECK_ARITY and clang 5
// CHECK_ARITY: check the number of args passed to an XPath function matches.
#undef NULL
#define NULL nullptr

// Simplified version of the macro proposed by libxml2
// The reason is when running against filegdbAPI which includes it own copy
// of libxml2, and the check 'ctxt->valueNr < ctxt->valueFrame + (x)'
// done by libxml2 CHECK_ARITY() thus points to garbage
#undef CHECK_ARITY
#define CHECK_ARITY(x)                                                  \
    if (ctxt == NULL) return;                                           \
    if (nargs != (x))                                                   \
        XP_ERROR(XPATH_INVALID_ARITY);

/************************************************************************/
/*                            GDALGMLJP2Expr                            */
/************************************************************************/

enum class GDALGMLJP2ExprType
{
    GDALGMLJP2Expr_Unknown,
    GDALGMLJP2Expr_XPATH,
    GDALGMLJP2Expr_STRING_LITERAL,
};

class GDALGMLJP2Expr
{
    static void SkipSpaces( const char*& pszStr );

  public:
    GDALGMLJP2ExprType           eType = GDALGMLJP2ExprType::GDALGMLJP2Expr_Unknown;
    CPLString                    osValue{};

    GDALGMLJP2Expr() = default;
    explicit GDALGMLJP2Expr( const char* pszVal ) :
        eType(GDALGMLJP2ExprType::GDALGMLJP2Expr_STRING_LITERAL), osValue(pszVal) {}
    explicit GDALGMLJP2Expr( const CPLString& osVal ) :
        eType(GDALGMLJP2ExprType::GDALGMLJP2Expr_STRING_LITERAL), osValue(osVal) {}
    ~GDALGMLJP2Expr() = default;

    GDALGMLJP2Expr          Evaluate( xmlXPathContextPtr pXPathCtx,
                                      xmlDocPtr pDoc );

    static GDALGMLJP2Expr* Build( const char* pszOriStr,
                                  const char*& pszStr );
    static void ReportError( const char* pszOriStr,
                             const char* pszStr,
                             const char* pszIntroMessage =
                                 "Parsing error at:\n" );
};

/************************************************************************/
/*                         ReportError()                                */
/************************************************************************/

void GDALGMLJP2Expr::ReportError( const char* pszOriStr,
                                  const char* pszStr,
                                  const char* pszIntroMessage )
{
    size_t nDist = static_cast<size_t>(pszStr - pszOriStr);
    if( nDist > 40 )
        nDist = 40;
    CPLString osErrMsg(pszIntroMessage);
    CPLString osInvalidExpr = CPLString(pszStr - nDist).substr(0, nDist + 20);
    for( int i = static_cast<int>(nDist) - 1; i >= 0; --i )
    {
        if( osInvalidExpr[i] == '\n' )
        {
            osInvalidExpr = osInvalidExpr.substr(i+1);
            nDist -= i + 1;
            break;
        }
    }
    for( size_t i = nDist; i < osInvalidExpr.size(); ++i )
    {
        if( osInvalidExpr[i] == '\n' )
        {
            osInvalidExpr.resize(i);
            break;
        }
    }
    osErrMsg += osInvalidExpr;
    osErrMsg += "\n";
    for( size_t i = 0; i < nDist; ++i )
        osErrMsg += " ";
    osErrMsg += "^";
    CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrMsg.c_str());
}

/************************************************************************/
/*                        SkipSpaces()                                  */
/************************************************************************/

void GDALGMLJP2Expr::SkipSpaces( const char*& pszStr )
{
    while( *pszStr == ' ' || *pszStr == '\t' ||
           *pszStr == '\r' || *pszStr ==  '\n' )
        ++pszStr;
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

GDALGMLJP2Expr* GDALGMLJP2Expr::Build( const char* pszOriStr,
                                       const char*& pszStr )
{
    if( STARTS_WITH_CI(pszStr, "{{{") )
    {
        pszStr += strlen("{{{");
        SkipSpaces(pszStr);
        GDALGMLJP2Expr* poExpr = Build(pszOriStr, pszStr);
        if( poExpr == nullptr )
            return nullptr;
        SkipSpaces(pszStr);
        if( !STARTS_WITH_CI(pszStr, "}}}") )
        {
            ReportError(pszOriStr, pszStr);
            delete poExpr;
            return nullptr;
        }
        pszStr += strlen("}}}");
        return poExpr;
    }
    else if( STARTS_WITH_CI(pszStr, "XPATH") )
    {
        pszStr += strlen("XPATH");
        SkipSpaces(pszStr);
        if( *pszStr != '(' )
        {
            ReportError(pszOriStr, pszStr);
            return nullptr;
        }
        ++pszStr;
        SkipSpaces(pszStr);
        CPLString l_osValue;
        int nParenthesisIndent = 0;
        char chLiteralQuote = '\0';
        while( *pszStr )
        {
            if( chLiteralQuote != '\0' )
            {
                if( *pszStr == chLiteralQuote )
                    chLiteralQuote = '\0';
                l_osValue += *pszStr;
                ++pszStr;
            }
            else if( *pszStr == '\'' || *pszStr == '"' )
            {
                chLiteralQuote = *pszStr;
                l_osValue += *pszStr;
                ++pszStr;
            }
            else if( *pszStr == '(' )
            {
                ++nParenthesisIndent;
                l_osValue += *pszStr;
                ++pszStr;
            }
            else if( *pszStr == ')' )
            {
                nParenthesisIndent --;
                if( nParenthesisIndent < 0 )
                {
                    pszStr++;
                    GDALGMLJP2Expr* poExpr = new GDALGMLJP2Expr();
                    poExpr->eType = GDALGMLJP2ExprType::GDALGMLJP2Expr_XPATH;
                    poExpr->osValue = l_osValue;
#if DEBUG_VERBOSE
                    CPLDebug("GMLJP2", "XPath expression '%s'",
                             l_osValue.c_str());
#endif
                    return poExpr;
                }
                l_osValue += *pszStr;
                ++pszStr;
            }
            else
            {
                l_osValue += *pszStr;
                pszStr++;
            }
        }
        ReportError(pszOriStr, pszStr);
        return nullptr;
    }
    else
    {
        ReportError(pszOriStr, pszStr);
        return nullptr;
    }
}

/************************************************************************/
/*                       GDALGMLJP2HexFormatter()                       */
/************************************************************************/

static const char* GDALGMLJP2HexFormatter( GByte nVal )
{
    return CPLSPrintf("%02X", nVal);
}

/************************************************************************/
/*                            Evaluate()                                */
/************************************************************************/

static CPLString GDALGMLJP2EvalExpr( const CPLString& osTemplate,
                                     xmlXPathContextPtr pXPathCtx,
                                     xmlDocPtr pDoc );

GDALGMLJP2Expr GDALGMLJP2Expr::Evaluate(xmlXPathContextPtr pXPathCtx,
                                   xmlDocPtr pDoc)
{
    switch( eType )
    {
        case GDALGMLJP2ExprType::GDALGMLJP2Expr_XPATH:
        {
            xmlXPathObjectPtr pXPathObj = xmlXPathEvalExpression(
                    reinterpret_cast<const xmlChar*>(osValue.c_str()), pXPathCtx);
            if( pXPathObj == nullptr )
                return GDALGMLJP2Expr("");

            // Add result of the evaluation.
            CPLString osXMLRes;
            if( pXPathObj->type == XPATH_STRING )
                osXMLRes = reinterpret_cast<const char*>(pXPathObj->stringval);
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
                    osXMLRes += reinterpret_cast<const char*>(xmlBufferContent(pBuf));
                    xmlBufferFree(pBuf);
                }
            }

            xmlXPathFreeObject(pXPathObj);
            return GDALGMLJP2Expr(osXMLRes);
        }
        default:
            CPLAssert(false);
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
    while( true )
    {
        // Get next expression.
        size_t nStartPos = osTemplate.find("{{{", nPos);
        if( nStartPos == std::string::npos)
        {
            // Add terminating portion of the template.
            osXMLRes += osTemplate.substr(nPos);
            break;
        }

        // Add portion of template before the expression.
        osXMLRes += osTemplate.substr(nPos, nStartPos - nPos);

        const char* pszExpr = osTemplate.c_str() + nStartPos;
        GDALGMLJP2Expr* poExpr = GDALGMLJP2Expr::Build(pszExpr, pszExpr);
        if( poExpr == nullptr )
            break;
        nPos = static_cast<size_t>(pszExpr - osTemplate.c_str());
        osXMLRes += poExpr->Evaluate(pXPathCtx,pDoc).osValue;
        delete poExpr;
    }
    return osXMLRes;
}

/************************************************************************/
/*                      GDALGMLJP2XPathErrorHandler()                   */
/************************************************************************/

static void GDALGMLJP2XPathErrorHandler( void * /* userData */,
                                         xmlErrorPtr error)
{
    if( error->domain == XML_FROM_XPATH &&
        error->str1 != nullptr &&
        error->int1 < static_cast<int>(strlen(error->str1)) )
    {
        GDALGMLJP2Expr::ReportError(error->str1,
                                    error->str1 + error->int1,
                                    "XPath error:\n");
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "An error occurred in libxml2");
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
            if( pNode->ns != nullptr && pNode->ns->prefix != nullptr )
            {
                //printf("%s %s\n",pNode->ns->prefix, pNode->ns->href);
                if(xmlXPathRegisterNs(pXPathCtx, pNode->ns->prefix, pNode->ns->href) != 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Registration of namespace %s failed",
                             reinterpret_cast<const char*>(pNode->ns->prefix));
                }
            }
        }

        GDALGMLJP2RegisterNamespaces(pXPathCtx, pNode->children);
    }
}

/************************************************************************/
/*                         GDALGMLJP2XPathIf()                          */
/************************************************************************/

static void GDALGMLJP2XPathIf(xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlXPathObjectPtr cond_val,then_val,else_val;

    CHECK_ARITY(3);
    else_val = valuePop(ctxt);
    then_val = valuePop(ctxt);
    CAST_TO_BOOLEAN
    cond_val = valuePop(ctxt);

    if( cond_val->boolval )
    {
        xmlXPathFreeObject(else_val);
        valuePush(ctxt, then_val);
    }
    else
    {
        xmlXPathFreeObject(then_val);
        valuePush(ctxt, else_val);
    }
    xmlXPathFreeObject(cond_val);
}

/************************************************************************/
/*                        GDALGMLJP2XPathUUID()                         */
/************************************************************************/

static void GDALGMLJP2XPathUUID(xmlXPathParserContextPtr ctxt, int nargs)
{
    CHECK_ARITY(0);

    CPLString osRet;
    static int nCounter = 0;
    srand(static_cast<unsigned int>(time(nullptr)) + nCounter);
    ++nCounter;
    for( int i=0; i<4; i ++ )
        osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
    osRet += "-";
    osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
    osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
    osRet += "-";
    // Set the version number bits (4 == random).
    osRet += GDALGMLJP2HexFormatter((rand() & 0x0F) | 0x40);
    osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
    osRet += "-";
    // Set the variant bits.
    osRet += GDALGMLJP2HexFormatter((rand() & 0x3F) | 0x80);
    osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
    osRet += "-";
    for( int i = 0; i < 6; ++i )
    {
        // coverity[dont_call]
        osRet += GDALGMLJP2HexFormatter(rand() & 0xFF);
    }

    valuePush(ctxt,
              xmlXPathNewString(
                  reinterpret_cast<const xmlChar *>(osRet.c_str())));
}

#endif  // LIBXML2

/************************************************************************/
/*                      GDALGMLJP2GenerateMetadata()                    */
/************************************************************************/

#ifdef HAVE_LIBXML2
CPLXMLNode* GDALGMLJP2GenerateMetadata(
    const CPLString& osTemplateFile,
    const CPLString& osSourceFile
)
{
    GByte* pabyStr = nullptr;
    if( !VSIIngestFile( nullptr, osTemplateFile, &pabyStr, nullptr, -1 ) )
        return nullptr;
    CPLString osTemplate(reinterpret_cast<char *>(pabyStr));
    CPLFree(pabyStr);

    if( !VSIIngestFile( nullptr, osSourceFile, &pabyStr, nullptr, -1 ) )
        return nullptr;
    CPLString osSource(reinterpret_cast<char *>(pabyStr));
    CPLFree(pabyStr);

    xmlDocPtr pDoc = xmlParseDoc(
        reinterpret_cast<const xmlChar *>(osSource.c_str()));
    if( pDoc == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse %s",
                 osSourceFile.c_str());
        return nullptr;
    }

    xmlXPathContextPtr pXPathCtx = xmlXPathNewContext(pDoc);
    if( pXPathCtx == nullptr )
    {
        xmlFreeDoc(pDoc);
        return nullptr;
    }

    xmlXPathRegisterFunc(pXPathCtx, reinterpret_cast<const xmlChar *>("if"),
                         GDALGMLJP2XPathIf);
    xmlXPathRegisterFunc(pXPathCtx, reinterpret_cast<const xmlChar *>("uuid"),
                         GDALGMLJP2XPathUUID);

    pXPathCtx->error = GDALGMLJP2XPathErrorHandler;

    GDALGMLJP2RegisterNamespaces(pXPathCtx, xmlDocGetRootElement(pDoc));

    CPLString osXMLRes = GDALGMLJP2EvalExpr(osTemplate, pXPathCtx, pDoc);

    xmlXPathFreeContext(pXPathCtx);
    xmlFreeDoc(pDoc);

    return CPLParseXMLString(osXMLRes);
}
#else  // !HAVE_LIBXML2
CPLXMLNode* GDALGMLJP2GenerateMetadata(
    const CPLString&  /* osTemplateFile */,
    const CPLString&  /* osSourceFile */
)
{
    return nullptr;
}
#endif  // HAVE_LIBXML2
