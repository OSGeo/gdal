/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement XML validation against XSD schema
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault
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

CPL_CVSID("$Id$");

#ifdef HAVE_LIBXML2
#include <libxml/xmlversion.h>
#if defined(LIBXML_VERSION) && LIBXML_VERSION >= 20622
/* We need at least 2.6.20 for xmlSchemaValidateDoc */
/* and xmlParseDoc to accept a const xmlChar* */
/* We could workaround it, but likely not worth the effort for now. */
/* Actually, we need at least 2.6.22, at runtime, to be */
/* able to parse the OGC GML schemas */
#define HAVE_RECENT_LIBXML2

/* libxml2 before 2.8.0 had a bug to parse the OGC GML schemas */
/* We have a workaround for that for versions >= 2.6.20 and < 2.8.0 */
#if defined(LIBXML_VERSION) && LIBXML_VERSION < 20800
#define HAS_VALIDATION_BUG
#endif

#else
#warning "Not recent enough libxml2 version"
#endif
#endif

#ifdef HAVE_RECENT_LIBXML2
#include <string.h>
#include <libxml/xmlschemas.h>
#include <libxml/parserInternals.h>
#include <libxml/catalog.h>

#include "cpl_string.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"

static xmlExternalEntityLoader pfnLibXMLOldExtranerEntityLoader = NULL;

/************************************************************************/
/*                            CPLFixPath()                              */
/************************************************************************/

/* Replace \ by / to make libxml2 happy on Windows and */
/* replace "a/b/../c" pattern by "a/c" */
static void CPLFixPath(char* pszPath)
{
    for(int i=0;pszPath[i] != '\0';i++)
    {
        if (pszPath[i] == '\\')
            pszPath[i] = '/';
    }

    while(TRUE)
    {
        char* pszSlashDotDot = strstr(pszPath, "/../");
        if (pszSlashDotDot == NULL || pszSlashDotDot == pszPath)
            return;
        char* pszSlashBefore = pszSlashDotDot-1;
        while(pszSlashBefore > pszPath && *pszSlashBefore != '/')
            pszSlashBefore --;
        if (pszSlashBefore == pszPath)
            return;
        memmove(pszSlashBefore + 1, pszSlashDotDot + 4,
                strlen(pszSlashDotDot + 4) + 1);
    }
}

#ifdef HAS_VALIDATION_BUG

static int bHasLibXMLBug = -1;

/************************************************************************/
/*                  CPLHasLibXMLBugWarningCallback()                    */
/************************************************************************/

static void CPLHasLibXMLBugWarningCallback (void * ctx, const char * msg, ...)
{
}

/************************************************************************/
/*                          CPLHasLibXMLBug()                           */
/************************************************************************/

static int CPLHasLibXMLBug()
{
    if (bHasLibXMLBug >= 0)
        return bHasLibXMLBug;

    static const char szLibXMLBugTester[] =
    "<schema targetNamespace=\"http://foo\" xmlns:foo=\"http://foo\" xmlns=\"http://www.w3.org/2001/XMLSchema\">"
    "<simpleType name=\"t1\">"
    "<list itemType=\"double\"/>"
    "</simpleType>"
    "<complexType name=\"t2\">"
    "<simpleContent>"
    "<extension base=\"foo:t1\"/>"
    "</simpleContent>"
    "</complexType>"
    "<complexType name=\"t3\">"
    "<simpleContent>"
    "<restriction base=\"foo:t2\">"
    "<length value=\"2\"/>"
    "</restriction>"
    "</simpleContent>"
    "</complexType>"
    "</schema>";

    xmlSchemaParserCtxtPtr pSchemaParserCtxt;
    xmlSchemaPtr pSchema;

    pSchemaParserCtxt = xmlSchemaNewMemParserCtxt(szLibXMLBugTester, strlen(szLibXMLBugTester));

    xmlSchemaSetParserErrors(pSchemaParserCtxt,
                             CPLHasLibXMLBugWarningCallback,
                             CPLHasLibXMLBugWarningCallback,
                             NULL);

    pSchema = xmlSchemaParse(pSchemaParserCtxt);
    xmlSchemaFreeParserCtxt(pSchemaParserCtxt);

    bHasLibXMLBug = (pSchema == NULL);

    if (pSchema)
        xmlSchemaFree(pSchema);

    if (bHasLibXMLBug)
    {
        CPLDebug("CPL",
                 "LibXML bug found (cf https://bugzilla.gnome.org/show_bug.cgi?id=630130). "
                 "Will try to workaround for GML schemas.");
    }

    return bHasLibXMLBug;
}

#endif

/************************************************************************/
/*                         CPLExtractSubSchema()                        */
/************************************************************************/

static CPLXMLNode* CPLExtractSubSchema(CPLXMLNode* psSubXML, CPLXMLNode* psMainSchema)
{
    if (psSubXML->eType == CXT_Element && strcmp(psSubXML->pszValue, "?xml") == 0)
    {
        CPLXMLNode* psNext = psSubXML->psNext;
        psSubXML->psNext = NULL;
        CPLDestroyXMLNode(psSubXML);
        psSubXML = psNext;
    }

    if (psSubXML != NULL && psSubXML->eType == CXT_Comment)
    {
        CPLXMLNode* psNext = psSubXML->psNext;
        psSubXML->psNext = NULL;
        CPLDestroyXMLNode(psSubXML);
        psSubXML = psNext;
    }

    if (psSubXML != NULL && psSubXML->eType == CXT_Element &&
        (strcmp(psSubXML->pszValue, "schema") == 0 ||
         strcmp(psSubXML->pszValue, "xs:schema") == 0 ||
         strcmp(psSubXML->pszValue, "xsd:schema") == 0) &&
        psSubXML->psNext == NULL)
    {
        CPLXMLNode* psNext = psSubXML->psChild;
        while(psNext != NULL && psNext->eType != CXT_Element &&
              psNext->psNext != NULL && psNext->psNext->eType != CXT_Element)
        {
            /* Add xmlns: from subschema to main schema if missing */
            if (psNext->eType == CXT_Attribute &&
                strncmp(psNext->pszValue, "xmlns:", 6) == 0 &&
                CPLGetXMLValue(psMainSchema, psNext->pszValue, NULL) == NULL)
            {
                CPLXMLNode* psAttr = CPLCreateXMLNode(NULL, CXT_Attribute, psNext->pszValue);
                CPLCreateXMLNode(psAttr, CXT_Text, psNext->psChild->pszValue);

                psAttr->psNext = psMainSchema->psChild;
                psMainSchema->psChild = psAttr;
            }
            psNext = psNext->psNext;
        }

        if (psNext != NULL && psNext->eType != CXT_Element &&
            psNext->psNext != NULL && psNext->psNext->eType == CXT_Element)
        {
            CPLXMLNode* psNext2 = psNext->psNext;
            psNext->psNext = NULL;
            CPLDestroyXMLNode(psSubXML);
            psSubXML = psNext2;
        }
    }

    return psSubXML;
}

#ifdef HAS_VALIDATION_BUG
/************************************************************************/
/*                        CPLWorkaroundLibXMLBug()                      */
/************************************************************************/

/* Return TRUE if the current node must be destroyed */
static int CPLWorkaroundLibXMLBug(CPLXMLNode* psIter)
{
    if (psIter->eType == CXT_Element &&
        strcmp(psIter->pszValue, "element") == 0 &&
        strcmp(CPLGetXMLValue(psIter, "name", ""), "QuantityExtent") == 0 &&
        strcmp(CPLGetXMLValue(psIter, "type", ""), "gml:QuantityExtentType") == 0)
    {
        CPLXMLNode* psIter2 = psIter->psChild;
        while(psIter2)
        {
            if (psIter2->eType == CXT_Attribute && strcmp(psIter2->pszValue, "type") == 0)
            {
                CPLFree(psIter2->psChild->pszValue);
                if (strcmp(CPLGetXMLValue(psIter, "substitutionGroup", ""), "gml:AbstractValue") == 0)
                    psIter2->psChild->pszValue = CPLStrdup("gml:MeasureOrNilReasonListType"); /* GML 3.2.1 */
                else
                    psIter2->psChild->pszValue = CPLStrdup("gml:MeasureOrNullListType");
            }
            psIter2 = psIter2->psNext;
        }
    }

    else if (psIter->eType == CXT_Element &&
        strcmp(psIter->pszValue, "element") == 0 &&
        strcmp(CPLGetXMLValue(psIter, "name", ""), "CategoryExtent") == 0 &&
        strcmp(CPLGetXMLValue(psIter, "type", ""), "gml:CategoryExtentType") == 0)
    {
        CPLXMLNode* psIter2 = psIter->psChild;
        while(psIter2)
        {
            if (psIter2->eType == CXT_Attribute && strcmp(psIter2->pszValue, "type") == 0)
            {
                CPLFree(psIter2->psChild->pszValue);
                if (strcmp(CPLGetXMLValue(psIter, "substitutionGroup", ""), "gml:AbstractValue") == 0)
                    psIter2->psChild->pszValue = CPLStrdup("gml:CodeOrNilReasonListType"); /* GML 3.2.1 */
                else
                    psIter2->psChild->pszValue = CPLStrdup("gml:CodeOrNullListType");
            }
            psIter2 = psIter2->psNext;
        }
    }

    else if (bHasLibXMLBug && psIter->eType == CXT_Element &&
             strcmp(psIter->pszValue, "complexType") == 0 &&
             (strcmp(CPLGetXMLValue(psIter, "name", ""), "QuantityExtentType") == 0 ||
              strcmp(CPLGetXMLValue(psIter, "name", ""), "CategoryExtentType") == 0))
    {
        /* Destroy this element */
        return TRUE;
    }

    /* For GML 3.2.1 */
    else if (psIter->eType == CXT_Element &&
             strcmp(psIter->pszValue, "complexType") == 0 &&
             strcmp(CPLGetXMLValue(psIter, "name", ""), "VectorType") == 0)
    {
        CPLXMLNode* psSimpleContent = CPLCreateXMLNode(NULL, CXT_Element, "simpleContent");
        CPLXMLNode* psExtension = CPLCreateXMLNode(psSimpleContent, CXT_Element, "extension");
        CPLXMLNode* psExtensionBase = CPLCreateXMLNode(psExtension, CXT_Attribute, "base");
        CPLCreateXMLNode(psExtensionBase, CXT_Text, "gml:doubleList");
        CPLXMLNode* psAttributeGroup = CPLCreateXMLNode(psExtension, CXT_Element, "attributeGroup");
        CPLXMLNode* psAttributeGroupRef = CPLCreateXMLNode(psAttributeGroup, CXT_Attribute, "ref");
        CPLCreateXMLNode(psAttributeGroupRef, CXT_Text, "gml:SRSReferenceGroup");

        CPLXMLNode* psName = CPLCreateXMLNode(NULL, CXT_Attribute, "name");
        CPLCreateXMLNode(psName, CXT_Text, "VectorType");

        CPLDestroyXMLNode(psIter->psChild);
        psIter->psChild = psName;
        psIter->psChild->psNext = psSimpleContent;
    }

    else if (psIter->eType == CXT_Element &&
             strcmp(psIter->pszValue, "element") == 0 &&
             (strcmp(CPLGetXMLValue(psIter, "name", ""), "domainOfValidity") == 0 ||
              strcmp(CPLGetXMLValue(psIter, "name", ""), "coordinateOperationAccuracy") == 0 ||
              strcmp(CPLGetXMLValue(psIter, "name", ""), "formulaCitation") == 0))
    {
        CPLXMLNode* psComplexType = CPLCreateXMLNode(NULL, CXT_Element, "complexType");
        CPLXMLNode* psSequence = CPLCreateXMLNode(psComplexType, CXT_Element, "sequence");
        CPLXMLNode* psSequenceMinOccurs = CPLCreateXMLNode(psSequence, CXT_Attribute, "minOccurs");
        CPLCreateXMLNode(psSequenceMinOccurs, CXT_Text, "0");
        CPLXMLNode* psAny = CPLCreateXMLNode(psSequence, CXT_Element, "any");
        CPLXMLNode* psAnyMinOccurs = CPLCreateXMLNode(psAny, CXT_Attribute, "minOccurs");
        CPLCreateXMLNode(psAnyMinOccurs, CXT_Text, "0");
        CPLXMLNode* psAnyProcessContents = CPLCreateXMLNode(psAny, CXT_Attribute, " processContents");
        CPLCreateXMLNode(psAnyProcessContents, CXT_Text, "lax");

        CPLXMLNode* psName = CPLCreateXMLNode(NULL, CXT_Attribute, "name");
        CPLCreateXMLNode(psName, CXT_Text, CPLGetXMLValue(psIter, "name", ""));

        CPLDestroyXMLNode(psIter->psChild);
        psIter->psChild = psName;
        psIter->psChild->psNext = psComplexType;
    }

    return FALSE;
}
#endif

/************************************************************************/
/*                       CPLLoadSchemaStrInternal()                     */
/************************************************************************/

static
CPLXMLNode* CPLLoadSchemaStrInternal(CPLHashSet* hSetSchemas,
                                     const char* pszFile)
{
    CPLXMLNode* psXML;
    CPLXMLNode* psSchema;
    CPLXMLNode* psPrev;
    CPLXMLNode* psIter;

    if (CPLHashSetLookup(hSetSchemas, pszFile))
        return NULL;

    CPLHashSetInsert(hSetSchemas, CPLStrdup(pszFile));

    CPLDebug("CPL", "Parsing %s", pszFile);

    psXML = CPLParseXMLFile(pszFile);
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot open %s", pszFile);
        return NULL;
    }

    psSchema = CPLGetXMLNode(psXML, "=schema");
    if (psSchema == NULL)
        psSchema = CPLGetXMLNode(psXML, "=xs:schema");
    if (psSchema == NULL)
        psSchema = CPLGetXMLNode(psXML, "=xsd:schema");
    if (psSchema == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find schema node in %s", pszFile);
        CPLDestroyXMLNode(psXML);
        return NULL;
    }

    psPrev = NULL;
    psIter = psSchema->psChild;
    while(psIter)
    {
        int bDestroyCurrentNode = FALSE;

#ifdef HAS_VALIDATION_BUG
        if (bHasLibXMLBug)
            bDestroyCurrentNode = CPLWorkaroundLibXMLBug(psIter);
#endif

        /* Load the referenced schemas, and integrate them in the main schema */
        if (psIter->eType == CXT_Element &&
            (strcmp(psIter->pszValue, "include") == 0 ||
             strcmp(psIter->pszValue, "xs:include") == 0||
             strcmp(psIter->pszValue, "xsd:include") == 0) &&
            psIter->psChild != NULL &&
            psIter->psChild->eType == CXT_Attribute &&
            strcmp(psIter->psChild->pszValue, "schemaLocation") == 0)
        {
            const char* pszIncludeSchema = psIter->psChild->psChild->pszValue;
            char* pszFullFilename = CPLStrdup(
                CPLFormFilename(CPLGetPath(pszFile), pszIncludeSchema, NULL));

            CPLFixPath(pszFullFilename);

            CPLXMLNode* psSubXML = NULL;

            /* If we haven't yet loaded that schema, do it now */
            if (!CPLHashSetLookup(hSetSchemas, pszFullFilename))
            {
                psSubXML = CPLLoadSchemaStrInternal(hSetSchemas, pszFullFilename);
                if (psSubXML == NULL)
                {
                    CPLFree(pszFullFilename);
                    CPLDestroyXMLNode(psXML);
                    return NULL;
                }
            }
            CPLFree(pszFullFilename);
            pszFullFilename = NULL;

            if (psSubXML)
            {
                CPLXMLNode* psNext = psIter->psNext;

                psSubXML = CPLExtractSubSchema(psSubXML, psSchema);
                if (psSubXML == NULL)
                {
                    CPLDestroyXMLNode(psXML);
                    return NULL;
                }

                /* Replace <include/> node by the subXML */
                CPLXMLNode* psIter2 = psSubXML;
                while(psIter2->psNext)
                    psIter2 = psIter2->psNext;
                psIter2->psNext = psNext;

                if (psPrev == NULL)
                    psSchema->psChild = psSubXML;
                else
                    psPrev->psNext = psSubXML;

                psIter->psNext = NULL;
                CPLDestroyXMLNode(psIter);

                psPrev = psIter2;
                psIter = psNext;
                continue;
            }
            else
            {
                /* We have already included that file, */
                /* so just remove the <include/> node */
                bDestroyCurrentNode = TRUE;
            }
        }

        /* Patch the schemaLocation of <import/> */
        else if (psIter->eType == CXT_Element &&
                    (strcmp(psIter->pszValue, "import") == 0 ||
                     strcmp(psIter->pszValue, "xs:import") == 0||
                     strcmp(psIter->pszValue, "xsd:import") == 0))
        {
            CPLXMLNode* psIter2 = psIter->psChild;
            while(psIter2)
            {
                if (psIter2->eType == CXT_Attribute &&
                    strcmp(psIter2->pszValue, "schemaLocation") == 0 &&
                    psIter2->psChild != NULL &&
                    strncmp(psIter2->psChild->pszValue, "http://", 7) != 0 &&
                    strncmp(psIter2->psChild->pszValue, "ftp://", 6) != 0)
                {
                    char* pszFullFilename = CPLStrdup(CPLFormFilename(
                                      CPLGetPath(pszFile), psIter2->psChild->pszValue, NULL));
                    CPLFixPath(pszFullFilename);
                    CPLFree(psIter2->psChild->pszValue);
                    psIter2->psChild->pszValue = pszFullFilename;
                }
                psIter2 = psIter2->psNext;
            }
        }

        if (bDestroyCurrentNode)
        {
            CPLXMLNode* psNext = psIter->psNext;
            if (psPrev == NULL)
                psSchema->psChild = psNext;
            else
                psPrev->psNext = psNext;

            psIter->psNext = NULL;
            CPLDestroyXMLNode(psIter);

            psIter = psNext;
            continue;
        }

        psPrev = psIter;
        psIter = psIter->psNext;
    }

    return psXML;
}

/************************************************************************/
/*                       CPLMoveImportAtBeginning()                     */
/************************************************************************/

static
void CPLMoveImportAtBeginning(CPLXMLNode* psXML)
{
    CPLXMLNode* psIter;
    CPLXMLNode* psPrev;
    CPLXMLNode* psSchema;

    psSchema = CPLGetXMLNode(psXML, "=schema");
    if (psSchema == NULL)
        psSchema = CPLGetXMLNode(psXML, "=xs:schema");
    if (psSchema == NULL)
        psSchema = CPLGetXMLNode(psXML, "=xsd:schema");
    if (psSchema == NULL)
        return;

    psPrev = NULL;
    psIter = psSchema->psChild;
    while(psIter)
    {
        if (psPrev != NULL && psIter->eType == CXT_Element &&
            (strcmp(psIter->pszValue, "import") == 0 ||
             strcmp(psIter->pszValue, "xs:import") == 0 ||
             strcmp(psIter->pszValue, "xsd:import") == 0))
        {
            /* Reorder at the beginning */
            CPLXMLNode* psNext = psIter->psNext;

            psPrev->psNext = psNext;

            CPLXMLNode* psFirstChild = psSchema->psChild;
            psSchema->psChild = psIter;
            psIter->psNext = psFirstChild;

            psIter = psNext;
            continue;
        }

        psPrev = psIter;
        psIter = psIter->psNext;
    }
}

/************************************************************************/
/*                           CPLLoadSchemaStr()                         */
/************************************************************************/

static
char* CPLLoadSchemaStr(const char* pszXSDFilename)
{
    char* pszStr = NULL;

#ifdef HAS_VALIDATION_BUG
    CPLHasLibXMLBug();
#endif

    CPLHashSet* hSetSchemas =
        CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);
    CPLXMLNode* psSchema =
        CPLLoadSchemaStrInternal(hSetSchemas, pszXSDFilename);
    if (psSchema)
    {
        CPLMoveImportAtBeginning(psSchema);
        pszStr = CPLSerializeXMLTree(psSchema);
        CPLDestroyXMLNode(psSchema);
    }
    CPLHashSetDestroy(hSetSchemas);
    return pszStr;
}

/************************************************************************/
/*                     CPLLibXMLInputStreamCPLFree()                    */
/************************************************************************/

static void CPLLibXMLInputStreamCPLFree(xmlChar* pszBuffer)
{
    CPLFree(pszBuffer);
}

/************************************************************************/
/*                      CPLExternalEntityLoader()                       */
/************************************************************************/

static const char szXML_XSD[] = "<xs:schema targetNamespace=\"http://www.w3.org/XML/1998/namespace\""
" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">"
"<xs:attribute name=\"lang\">"
"<xs:simpleType>"
"<xs:union memberTypes=\"xs:language\">"
"<xs:simpleType>"
"<xs:restriction base=\"xs:string\">"
"<xs:enumeration value=\"\"/>"
"</xs:restriction>"
"</xs:simpleType>"
"</xs:union>"
"</xs:simpleType>"
"</xs:attribute>"
"<xs:attribute name=\"space\">"
"<xs:simpleType>"
"<xs:restriction base=\"xs:NCName\">"
"<xs:enumeration value=\"default\"/>"
"<xs:enumeration value=\"preserve\"/>"
"</xs:restriction>"
"</xs:simpleType>"
"</xs:attribute>"
"<xs:attribute name=\"base\" type=\"xs:anyURI\"/>"
"<xs:attribute name=\"id\" type=\"xs:ID\"/>"
"<xs:attributeGroup name=\"specialAttrs\">"
"<xs:attribute ref=\"xml:base\"/>"
"<xs:attribute ref=\"xml:lang\"/>"
"<xs:attribute ref=\"xml:space\"/>"
"<xs:attribute ref=\"xml:id\"/>"
"</xs:attributeGroup>"
"</xs:schema>";

static
xmlParserInputPtr CPLExternalEntityLoader (const char * URL,
                                           const char * ID,
                                           xmlParserCtxtPtr context)
{
    //CPLDebug("CPL", "CPLExternalEntityLoader(%s)", URL);
    CPLString osURL;
    
    /* Use libxml2 catalog mechanism to resolve the URL to something else */
    xmlChar* pszResolved = xmlCatalogResolveSystem((const xmlChar*)URL);
    if (pszResolved == NULL)
        pszResolved = xmlCatalogResolveURI((const xmlChar*)URL);
    if (pszResolved)
    {
        CPLDebug("CPL", "Resolving %s in %s", URL, (const char*)pszResolved );
        osURL = (const char*)pszResolved;
        URL = osURL.c_str();
        xmlFree(pszResolved);
        pszResolved = NULL;
    }

    if (strncmp(URL, "http://", 7) == 0)
    {
        /* Make sure to use http://schemas.opengis.net/ */
        /* when gml/2 or gml/3 is detected */
        const char* pszGML = strstr(URL, "gml/2");
        if (pszGML == NULL)
            pszGML = strstr(URL, "gml/3");
        if (pszGML != NULL)
        {
            osURL = "http://schemas.opengis.net/";
            osURL += pszGML;
            URL = osURL.c_str();
        }
        else if (strcmp(URL, "http://www.w3.org/2001/xml.xsd") == 0)
        {
            CPLDebug("CPL", "Resolving http://www.w3.org/2001/xml.xsd to local definition");
            return xmlNewStringInputStream(context, (const xmlChar*) szXML_XSD);
        }
        else if (strncmp(URL, "http://schemas.opengis.net/",
                         strlen("http://schemas.opengis.net/")) != 0)
        {
            return pfnLibXMLOldExtranerEntityLoader(URL, ID, context);
        }
    }
    else if (strncmp(URL, "ftp://", 6) == 0)
    {
        return pfnLibXMLOldExtranerEntityLoader(URL, ID, context);
    }
    else if (strncmp(URL, "file://", 7) == 0)
    {
        /* Parse file:// URI so as to be able to open them with VSI*L API */
        if (strncmp(URL, "file://localhost/", 17) == 0)
            URL += 16;
        else
            URL += 7;
        if (URL[0] == '/' && URL[1] != '\0' && URL[2] == ':' && URL[3] == '/') /* Windows */
            URL ++;
        else if (URL[0] == '/') /* Unix */
            ;
        else
            return pfnLibXMLOldExtranerEntityLoader(URL, ID, context);
    }

    CPLString osModURL;
    if (strncmp(URL, "/vsizip/vsicurl/http%3A//",
                strlen("/vsizip/vsicurl/http%3A//")) == 0)
    {
        osModURL = "/vsizip/vsicurl/http://";
        osModURL += URL + strlen("/vsizip/vsicurl/http%3A//");
    }
    else if (strncmp(URL, "/vsicurl/http%3A//",
                     strlen("/vsicurl/http%3A//")) == 0)
    {
        osModURL = "vsicurl/http://";
        osModURL += URL + strlen("/vsicurl/http%3A//");
    }
    else if (strncmp(URL, "http://schemas.opengis.net/",
                     strlen("http://schemas.opengis.net/")) == 0)
    {
        const char *pszAfterOpenGIS =
                URL + strlen("http://schemas.opengis.net/");

        const char *pszSchemasOpenGIS;

        pszSchemasOpenGIS = CPLGetConfigOption("GDAL_OPENGIS_SCHEMAS", NULL);
        if (pszSchemasOpenGIS != NULL)
        {
            int nLen = (int)strlen(pszSchemasOpenGIS);
            if (nLen > 0 && pszSchemasOpenGIS[nLen-1] == '/')
            {
                osModURL = pszSchemasOpenGIS;
                osModURL += pszAfterOpenGIS;
            }
            else
            {
                osModURL = pszSchemasOpenGIS;
                osModURL += "/";
                osModURL += pszAfterOpenGIS;
            }
        }
        else if ((pszSchemasOpenGIS = CPLFindFile( "gdal", "SCHEMAS_OPENGIS_NET" )) != NULL)
        {
            osModURL = pszSchemasOpenGIS;
            osModURL += "/";
            osModURL += pszAfterOpenGIS;
        }
        else if ((pszSchemasOpenGIS = CPLFindFile( "gdal", "SCHEMAS_OPENGIS_NET.zip" )) != NULL)
        {
            osModURL = "/vsizip/";
            osModURL += pszSchemasOpenGIS;
            osModURL += "/";
            osModURL += pszAfterOpenGIS;
        }
        else
        {
            osModURL = "/vsizip/vsicurl/http://schemas.opengis.net/SCHEMAS_OPENGIS_NET.zip/";
            osModURL += pszAfterOpenGIS;
        }
    }
    else
    {
        osModURL = URL;
    }

    xmlChar* pszBuffer = (xmlChar*)CPLLoadSchemaStr(osModURL);
    if (pszBuffer == NULL)
        return NULL;

    xmlParserInputPtr poInputStream = xmlNewStringInputStream(context, pszBuffer);
    if (poInputStream != NULL)
        poInputStream->free = CPLLibXMLInputStreamCPLFree;
    return poInputStream;
}

/************************************************************************/
/*                    CPLLibXMLWarningErrorCallback()                   */
/************************************************************************/

static void CPLLibXMLWarningErrorCallback (void * ctx, const char * msg, ...)
{
    va_list varg;
    char * pszStr;

    va_start(varg, msg);
    pszStr = (char *)va_arg( varg, char *);

    if (strstr(pszStr, "since this namespace was already imported") == NULL)
    {
        xmlErrorPtr pErrorPtr = xmlGetLastError();
        const char* pszFilename = (const char*)ctx;
        char* pszStrDup = CPLStrdup(pszStr);
        int nLen = (int)strlen(pszStrDup);
        if (nLen > 0 && pszStrDup[nLen-1] == '\n')
            pszStrDup[nLen-1] = '\0';
        CPLError(CE_Failure, CPLE_AppDefined, "libXML: %s:%d: %s",
                 pszFilename, pErrorPtr ? pErrorPtr->line : 0, pszStrDup);
        CPLFree(pszStrDup);
    }

    va_end(varg);
}

/************************************************************************/
/*                      CPLLoadContentFromFile()                        */
/************************************************************************/

static
char* CPLLoadContentFromFile(const char* pszFilename)
{
    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return NULL;
    vsi_l_offset nSize;
    VSIFSeekL(fp, 0, SEEK_END);
    nSize = VSIFTellL(fp);
    VSIFSeekL(fp, 0, SEEK_SET);
    if ((vsi_l_offset)(int)nSize != nSize ||
        nSize > INT_MAX - 1 )
    {
        VSIFCloseL(fp);
        return NULL;
    }
    char* pszBuffer = (char*)VSIMalloc(nSize + 1);
    if (pszBuffer == NULL)
    {
        VSIFCloseL(fp);
        return NULL;
    }
    VSIFReadL(pszBuffer, 1, nSize, fp);
    pszBuffer[nSize] = '\0';
    VSIFCloseL(fp);
    return pszBuffer;
}

/************************************************************************/
/*                         CPLLoadXMLSchema()                           */
/************************************************************************/

typedef void* CPLXMLSchemaPtr;

/**
 * \brief Load a XSD schema.
 *
 * The return value should be freed with CPLFreeXMLSchema().
 *
 * @param pszXSDFilename XSD schema to load.
 * @return a handle to the parsed XML schema, or NULL in case of failure.
 *
 * @since GDAL 2.0.0
 */

static
CPLXMLSchemaPtr CPLLoadXMLSchema(const char* pszXSDFilename)
{
    char* pszStr = CPLLoadSchemaStr(pszXSDFilename);
    if (pszStr == NULL)
        return NULL;

    xmlExternalEntityLoader pfnLibXMLOldExtranerEntityLoaderLocal = NULL;
    pfnLibXMLOldExtranerEntityLoaderLocal = xmlGetExternalEntityLoader();
    pfnLibXMLOldExtranerEntityLoader = pfnLibXMLOldExtranerEntityLoaderLocal;
    xmlSetExternalEntityLoader(CPLExternalEntityLoader);

    xmlSchemaParserCtxtPtr pSchemaParserCtxt =
                            xmlSchemaNewMemParserCtxt(pszStr, strlen(pszStr));

    xmlSchemaSetParserErrors(pSchemaParserCtxt,
                             CPLLibXMLWarningErrorCallback,
                             CPLLibXMLWarningErrorCallback,
                             NULL);

    xmlSchemaPtr pSchema = xmlSchemaParse(pSchemaParserCtxt);
    xmlSchemaFreeParserCtxt(pSchemaParserCtxt);

    xmlSetExternalEntityLoader(pfnLibXMLOldExtranerEntityLoaderLocal);

    CPLFree(pszStr);

    return (CPLXMLSchemaPtr) pSchema;
}

/************************************************************************/
/*                         CPLFreeXMLSchema()                           */
/************************************************************************/

/**
 * \brief Free a XSD schema.
 *
 * @param pSchema a handle to the parsed XML schema.
 *
 * @since GDAL 2.0.0
 */

static
void CPLFreeXMLSchema(CPLXMLSchemaPtr pSchema)
{
    if (pSchema)
        xmlSchemaFree((xmlSchemaPtr)pSchema);
}

/************************************************************************/
/*                          CPLValidateXML()                            */
/************************************************************************/

/**
 * \brief Validate a XML file against a XML schema.
 *
 * @param pszXMLFilename the filename of the XML file to validate.
 * @param pszXSDFilename the filename of the XSD schema.
 * @param papszOptions unused for now.
 * @return TRUE if the XML file validates against the XML schema.
 *
 * @since GDAL 2.0.0
 */

int CPLValidateXML(const char* pszXMLFilename,
                   const char* pszXSDFilename,
                   char** papszOptions)
{
    CPLXMLSchemaPtr pSchema = CPLLoadXMLSchema(pszXSDFilename);
    if (pSchema == NULL)
        return FALSE;

    xmlSchemaValidCtxtPtr pSchemaValidCtxt;

    pSchemaValidCtxt = xmlSchemaNewValidCtxt((xmlSchemaPtr)pSchema);

    if (pSchemaValidCtxt == NULL)
    {
        CPLFreeXMLSchema(pSchema);
        return FALSE;
    }

    xmlSchemaSetValidErrors(pSchemaValidCtxt,
                            CPLLibXMLWarningErrorCallback,
                            CPLLibXMLWarningErrorCallback,
                            (void*) pszXMLFilename);

    int bValid = FALSE;
    if (strncmp(pszXMLFilename, "/vsi", 4) != 0)
    {
        bValid =
            xmlSchemaValidateFile(pSchemaValidCtxt, pszXMLFilename, 0) == 0;
    }
    else
    {
        char* pszXML = CPLLoadContentFromFile(pszXMLFilename);
        if (pszXML != NULL)
        {
            xmlDocPtr pDoc = xmlParseDoc((const xmlChar *)pszXML);
            if (pDoc != NULL)
            {
                bValid = xmlSchemaValidateDoc(pSchemaValidCtxt, pDoc) == 0;
            }
            xmlFreeDoc(pDoc);
        }
        CPLFree(pszXML);
    }
    xmlSchemaFreeValidCtxt(pSchemaValidCtxt);
    CPLFreeXMLSchema(pSchema);

    return bValid;
}

#else // HAVE_RECENT_LIBXML2

/************************************************************************/
/*                          CPLValidateXML()                            */
/************************************************************************/

int CPLValidateXML(const char* pszXMLFilename,
                   const char* pszXSDFilename,
                   char** papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "%s not implemented due to missing libxml2 support",
             "CPLValidateXML()");
    return FALSE;
}

#endif // HAVE_RECENT_LIBXML2
