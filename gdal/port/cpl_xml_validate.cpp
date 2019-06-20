/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement XML validation against XSD schema
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

#ifdef HAVE_LIBXML2
#include <libxml/xmlversion.h>
#if defined(LIBXML_VERSION) && LIBXML_VERSION >= 20622
// We need at least 2.6.20 for xmlSchemaValidateDoc
// and xmlParseDoc to accept a const xmlChar*
// We could workaround it, but likely not worth the effort for now.
// Actually, we need at least 2.6.22, at runtime, to be
// able to parse the OGC GML schemas
#define HAVE_RECENT_LIBXML2

// libxml2 before 2.8.0 had a bug to parse the OGC GML schemas
// We have a workaround for that for versions >= 2.6.20 and < 2.8.0.
#if defined(LIBXML_VERSION) && LIBXML_VERSION < 20800
#define HAS_VALIDATION_BUG
#endif

#else
#warning "Not recent enough libxml2 version"
#endif
#endif

#ifdef HAVE_RECENT_LIBXML2
#include <string.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

#include <libxml/xmlschemas.h>
#include <libxml/parserInternals.h>
#include <libxml/catalog.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "cpl_string.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"

static xmlExternalEntityLoader pfnLibXMLOldExtranerEntityLoader = nullptr;

/************************************************************************/
/*                            CPLFixPath()                              */
/************************************************************************/

// Replace \ by / to make libxml2 happy on Windows and
// replace "a/b/../c" pattern by "a/c".
static void CPLFixPath(char* pszPath)
{
    for( int i = 0; pszPath[i] != '\0'; ++i )
    {
        if( pszPath[i] == '\\' )
            pszPath[i] = '/';
    }

    while( true )
    {
        char* pszSlashDotDot = strstr(pszPath, "/../");
        if( pszSlashDotDot == nullptr || pszSlashDotDot == pszPath )
            return;
        char* pszSlashBefore = pszSlashDotDot - 1;
        while( pszSlashBefore > pszPath && *pszSlashBefore != '/' )
            pszSlashBefore--;
        if( pszSlashBefore == pszPath )
            return;
        memmove(pszSlashBefore + 1, pszSlashDotDot + 4,
                strlen(pszSlashDotDot + 4) + 1);
    }
}

#ifdef HAS_VALIDATION_BUG

/************************************************************************/
/*                  CPLHasLibXMLBugWarningCallback()                    */
/************************************************************************/

static void CPLHasLibXMLBugWarningCallback ( void * /*ctx*/,
                                             const char* /*msg*/, ... )
{}

/************************************************************************/
/*                          CPLHasLibXMLBug()                           */
/************************************************************************/

static bool CPLHasLibXMLBug()
{
    static bool bHasLibXMLBug = false;
    static bool bLibXMLBugChecked = false;
    if( bLibXMLBugChecked )
        return bHasLibXMLBug;

    constexpr char szLibXMLBugTester[] =
        "<schema targetNamespace=\"http://foo\" "
        "xmlns:foo=\"http://foo\" xmlns=\"http://www.w3.org/2001/XMLSchema\">"
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

    xmlSchemaParserCtxtPtr pSchemaParserCtxt =
        xmlSchemaNewMemParserCtxt(szLibXMLBugTester, strlen(szLibXMLBugTester));

    xmlSchemaSetParserErrors(pSchemaParserCtxt,
                             CPLHasLibXMLBugWarningCallback,
                             CPLHasLibXMLBugWarningCallback,
                             nullptr);

    xmlSchemaPtr pSchema = xmlSchemaParse(pSchemaParserCtxt);
    xmlSchemaFreeParserCtxt(pSchemaParserCtxt);

    bHasLibXMLBug = pSchema == nullptr;
    bLibXMLBugChecked = true;

    if( pSchema )
        xmlSchemaFree(pSchema);

    if( bHasLibXMLBug )
    {
        CPLDebug(
            "CPL",
            "LibXML bug found "
            "(cf https://bugzilla.gnome.org/show_bug.cgi?id=630130). "
            "Will try to workaround for GML schemas." );
    }

    return bHasLibXMLBug;
}

#endif

/************************************************************************/
/*                         CPLExtractSubSchema()                        */
/************************************************************************/

static CPLXMLNode* CPLExtractSubSchema( CPLXMLNode* psSubXML,
                                        CPLXMLNode* psMainSchema )
{
    if( psSubXML->eType == CXT_Element &&
        strcmp(psSubXML->pszValue, "?xml") == 0 )
    {
        CPLXMLNode* psNext = psSubXML->psNext;
        psSubXML->psNext = nullptr;
        CPLDestroyXMLNode(psSubXML);
        psSubXML = psNext;
    }

    if( psSubXML != nullptr && psSubXML->eType == CXT_Comment )
    {
        CPLXMLNode* psNext = psSubXML->psNext;
        psSubXML->psNext = nullptr;
        CPLDestroyXMLNode(psSubXML);
        psSubXML = psNext;
    }

    if( psSubXML != nullptr && psSubXML->eType == CXT_Element &&
        (strcmp(psSubXML->pszValue, "schema") == 0 ||
         strcmp(psSubXML->pszValue, "xs:schema") == 0 ||
         strcmp(psSubXML->pszValue, "xsd:schema") == 0) &&
        psSubXML->psNext == nullptr )
    {
        CPLXMLNode* psNext = psSubXML->psChild;
        while( psNext != nullptr && psNext->eType != CXT_Element &&
               psNext->psNext != nullptr && psNext->psNext->eType != CXT_Element )
        {
            // Add xmlns: from subschema to main schema if missing.
            if( psNext->eType == CXT_Attribute &&
                STARTS_WITH(psNext->pszValue, "xmlns:") &&
                CPLGetXMLValue(psMainSchema, psNext->pszValue, nullptr) == nullptr )
            {
                CPLXMLNode* psAttr =
                    CPLCreateXMLNode(nullptr, CXT_Attribute, psNext->pszValue);
                CPLCreateXMLNode(psAttr, CXT_Text, psNext->psChild->pszValue);

                psAttr->psNext = psMainSchema->psChild;
                psMainSchema->psChild = psAttr;
            }
            psNext = psNext->psNext;
        }

        if( psNext != nullptr && psNext->eType != CXT_Element &&
            psNext->psNext != nullptr && psNext->psNext->eType == CXT_Element )
        {
            CPLXMLNode* psNext2 = psNext->psNext;
            psNext->psNext = nullptr;
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

// Return TRUE if the current node must be destroyed.
static bool CPLWorkaroundLibXMLBug( CPLXMLNode* psIter )
{
    if( psIter->eType == CXT_Element &&
        strcmp(psIter->pszValue, "element") == 0 &&
        strcmp(CPLGetXMLValue(psIter, "name", ""), "QuantityExtent") == 0 &&
        strcmp(CPLGetXMLValue(psIter, "type", ""),
               "gml:QuantityExtentType") == 0 )
    {
        CPLXMLNode* psIter2 = psIter->psChild;
        while( psIter2 )
        {
            if( psIter2->eType == CXT_Attribute &&
                strcmp(psIter2->pszValue, "type") == 0 )
            {
                CPLFree(psIter2->psChild->pszValue);
                if( strcmp(CPLGetXMLValue(psIter, "substitutionGroup", ""),
                           "gml:AbstractValue") == 0 )
                     // GML 3.2.1.
                    psIter2->psChild->pszValue =
                        CPLStrdup("gml:MeasureOrNilReasonListType");
                else
                    psIter2->psChild->pszValue =
                        CPLStrdup("gml:MeasureOrNullListType");
            }
            psIter2 = psIter2->psNext;
        }
    }

    else if( psIter->eType == CXT_Element &&
             strcmp(psIter->pszValue, "element") == 0 &&
             strcmp(CPLGetXMLValue(psIter, "name", ""),
                    "CategoryExtent") == 0 &&
             strcmp(CPLGetXMLValue(psIter, "type", ""),
                    "gml:CategoryExtentType") == 0 )
    {
        CPLXMLNode* psIter2 = psIter->psChild;
        while( psIter2 )
        {
            if( psIter2->eType == CXT_Attribute &&
                strcmp(psIter2->pszValue, "type") == 0 )
            {
                CPLFree(psIter2->psChild->pszValue);
                if( strcmp(CPLGetXMLValue(psIter, "substitutionGroup", ""),
                           "gml:AbstractValue") == 0 )
                    // GML 3.2.1
                    psIter2->psChild->pszValue =
                        CPLStrdup("gml:CodeOrNilReasonListType");
                else
                    psIter2->psChild->pszValue =
                        CPLStrdup("gml:CodeOrNullListType");
            }
            psIter2 = psIter2->psNext;
        }
    }

    else if( CPLHasLibXMLBug() && psIter->eType == CXT_Element &&
             strcmp(psIter->pszValue, "complexType") == 0 &&
             (strcmp(CPLGetXMLValue(psIter, "name", ""),
                     "QuantityExtentType") == 0 ||
              strcmp(CPLGetXMLValue(psIter, "name", ""),
                     "CategoryExtentType") == 0) )
    {
        // Destroy this element.
        return true;
    }

    // For GML 3.2.1
    else if( psIter->eType == CXT_Element &&
             strcmp(psIter->pszValue, "complexType") == 0 &&
             strcmp(CPLGetXMLValue(psIter, "name", ""), "VectorType") == 0 )
    {
        CPLXMLNode* psSimpleContent =
            CPLCreateXMLNode(nullptr, CXT_Element, "simpleContent");
        CPLXMLNode* psExtension =
            CPLCreateXMLNode(psSimpleContent, CXT_Element, "extension");
        CPLXMLNode* psExtensionBase =
            CPLCreateXMLNode(psExtension, CXT_Attribute, "base");
        CPLCreateXMLNode(psExtensionBase, CXT_Text, "gml:doubleList");
        CPLXMLNode* psAttributeGroup =
            CPLCreateXMLNode(psExtension, CXT_Element, "attributeGroup");
        CPLXMLNode* psAttributeGroupRef =
            CPLCreateXMLNode(psAttributeGroup, CXT_Attribute, "ref");
        CPLCreateXMLNode(psAttributeGroupRef, CXT_Text,
                         "gml:SRSReferenceGroup");

        CPLXMLNode* psName = CPLCreateXMLNode(nullptr, CXT_Attribute, "name");
        CPLCreateXMLNode(psName, CXT_Text, "VectorType");

        CPLDestroyXMLNode(psIter->psChild);
        psIter->psChild = psName;
        psIter->psChild->psNext = psSimpleContent;
    }

    else if( psIter->eType == CXT_Element &&
             strcmp(psIter->pszValue, "element") == 0 &&
             (strcmp(CPLGetXMLValue(psIter, "name", ""),
                     "domainOfValidity") == 0 ||
              strcmp(CPLGetXMLValue(psIter, "name", ""),
                     "coordinateOperationAccuracy") == 0 ||
              strcmp(CPLGetXMLValue(psIter, "name", ""),
                     "formulaCitation") == 0) )
    {
        CPLXMLNode* psComplexType =
            CPLCreateXMLNode(nullptr, CXT_Element, "complexType");
        CPLXMLNode* psSequence =
            CPLCreateXMLNode(psComplexType, CXT_Element, "sequence");
        CPLXMLNode* psSequenceMinOccurs =
            CPLCreateXMLNode(psSequence, CXT_Attribute, "minOccurs");
        CPLCreateXMLNode(psSequenceMinOccurs, CXT_Text, "0");
        CPLXMLNode* psAny = CPLCreateXMLNode(psSequence, CXT_Element, "any");
        CPLXMLNode* psAnyMinOccurs =
            CPLCreateXMLNode(psAny, CXT_Attribute, "minOccurs");
        CPLCreateXMLNode(psAnyMinOccurs, CXT_Text, "0");
        CPLXMLNode* psAnyProcessContents =
            CPLCreateXMLNode(psAny, CXT_Attribute, " processContents");
        CPLCreateXMLNode(psAnyProcessContents, CXT_Text, "lax");

        CPLXMLNode* psName = CPLCreateXMLNode(nullptr, CXT_Attribute, "name");
        CPLCreateXMLNode(psName, CXT_Text, CPLGetXMLValue(psIter, "name", ""));

        CPLDestroyXMLNode(psIter->psChild);
        psIter->psChild = psName;
        psIter->psChild->psNext = psComplexType;
    }

    return false;
}
#endif

/************************************************************************/
/*                       CPLLoadSchemaStrInternal()                     */
/************************************************************************/

static
CPLXMLNode* CPLLoadSchemaStrInternal( CPLHashSet* hSetSchemas,
                                      const char* pszFile )
{
    if( CPLHashSetLookup(hSetSchemas, pszFile) )
        return nullptr;

    CPLHashSetInsert(hSetSchemas, CPLStrdup(pszFile));

    CPLDebug("CPL", "Parsing %s", pszFile);

    CPLXMLNode* psXML = CPLParseXMLFile(pszFile);
    if( psXML == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot open %s", pszFile);
        return nullptr;
    }

    CPLXMLNode* psSchema = CPLGetXMLNode(psXML, "=schema");
    if( psSchema == nullptr )
    {
        psSchema = CPLGetXMLNode(psXML, "=xs:schema");
    }
    if( psSchema == nullptr )
    {
        psSchema = CPLGetXMLNode(psXML, "=xsd:schema");
    }
    if( psSchema == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find schema node in %s", pszFile);
        CPLDestroyXMLNode(psXML);
        return nullptr;
    }

    CPLXMLNode* psPrev = nullptr;
    CPLXMLNode* psIter = psSchema->psChild;
    while( psIter )
    {
        bool bDestroyCurrentNode = false;

#ifdef HAS_VALIDATION_BUG
        if( CPLHasLibXMLBug() )
            bDestroyCurrentNode = CPLWorkaroundLibXMLBug(psIter);
#endif

        // Load the referenced schemas, and integrate them in the main schema.
        if( psIter->eType == CXT_Element &&
            (strcmp(psIter->pszValue, "include") == 0 ||
             strcmp(psIter->pszValue, "xs:include") == 0||
             strcmp(psIter->pszValue, "xsd:include") == 0) &&
            psIter->psChild != nullptr &&
            psIter->psChild->eType == CXT_Attribute &&
            strcmp(psIter->psChild->pszValue, "schemaLocation") == 0 )
        {
            const char* pszIncludeSchema = psIter->psChild->psChild->pszValue;
            char* pszFullFilename = CPLStrdup(
                CPLFormFilename(CPLGetPath(pszFile), pszIncludeSchema, nullptr) );

            CPLFixPath(pszFullFilename);

            CPLXMLNode* psSubXML = nullptr;

            // If we haven't yet loaded that schema, do it now.
            if( !CPLHashSetLookup(hSetSchemas, pszFullFilename) )
            {
                psSubXML =
                    CPLLoadSchemaStrInternal(hSetSchemas, pszFullFilename);
                if( psSubXML == nullptr )
                {
                    CPLFree(pszFullFilename);
                    CPLDestroyXMLNode(psXML);
                    return nullptr;
                }
            }
            CPLFree(pszFullFilename);
            pszFullFilename = nullptr;

            if( psSubXML )
            {
                CPLXMLNode* psNext = psIter->psNext;

                psSubXML = CPLExtractSubSchema(psSubXML, psSchema);
                if( psSubXML == nullptr )
                {
                    CPLDestroyXMLNode(psXML);
                    return nullptr;
                }

                // Replace <include/> node by the subXML.
                CPLXMLNode* psIter2 = psSubXML;
                while( psIter2->psNext )
                    psIter2 = psIter2->psNext;
                psIter2->psNext = psNext;

                if( psPrev == nullptr )
                    psSchema->psChild = psSubXML;
                else
                    psPrev->psNext = psSubXML;

                psIter->psNext = nullptr;
                CPLDestroyXMLNode(psIter);

                psPrev = psIter2;
                psIter = psNext;
                continue;
            }
            else
            {
                // We have already included that file,
                // so just remove the <include/> node
                bDestroyCurrentNode = true;
            }
        }

        // Patch the schemaLocation of <import/>.
        else if( psIter->eType == CXT_Element &&
                 (strcmp(psIter->pszValue, "import") == 0 ||
                  strcmp(psIter->pszValue, "xs:import") == 0||
                  strcmp(psIter->pszValue, "xsd:import") == 0) )
        {
            CPLXMLNode* psIter2 = psIter->psChild;
            while( psIter2 )
            {
                if( psIter2->eType == CXT_Attribute &&
                    strcmp(psIter2->pszValue, "schemaLocation") == 0 &&
                    psIter2->psChild != nullptr &&
                    !STARTS_WITH(psIter2->psChild->pszValue, "http://") &&
                    !STARTS_WITH(psIter2->psChild->pszValue, "ftp://") &&
                    // If the top file is our warping file, don't alter the path
                    // of the import.
                    strstr(pszFile, "/vsimem/CPLValidateXML_") == nullptr )
                {
                    char* pszFullFilename =
                        CPLStrdup(CPLFormFilename(
                            CPLGetPath(pszFile),
                            psIter2->psChild->pszValue, nullptr ));
                    CPLFixPath(pszFullFilename);
                    CPLFree(psIter2->psChild->pszValue);
                    psIter2->psChild->pszValue = pszFullFilename;
                }
                psIter2 = psIter2->psNext;
            }
        }

        if( bDestroyCurrentNode )
        {
            CPLXMLNode* psNext = psIter->psNext;
            if( psPrev == nullptr )
                psSchema->psChild = psNext;
            else
                psPrev->psNext = psNext;

            psIter->psNext = nullptr;
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
void CPLMoveImportAtBeginning( CPLXMLNode* psXML )
{
    CPLXMLNode* psSchema = CPLGetXMLNode(psXML, "=schema");
    if( psSchema == nullptr )
        psSchema = CPLGetXMLNode(psXML, "=xs:schema");
    if( psSchema == nullptr )
        psSchema = CPLGetXMLNode(psXML, "=xsd:schema");
    if( psSchema == nullptr )
        return;

    CPLXMLNode* psPrev = nullptr;
    CPLXMLNode* psIter = psSchema->psChild;
    while( psIter )
    {
        if( psPrev != nullptr && psIter->eType == CXT_Element &&
            (strcmp(psIter->pszValue, "import") == 0 ||
             strcmp(psIter->pszValue, "xs:import") == 0 ||
             strcmp(psIter->pszValue, "xsd:import") == 0) )
        {
            // Reorder at the beginning.
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
char* CPLLoadSchemaStr( const char* pszXSDFilename )
{
#ifdef HAS_VALIDATION_BUG
    CPLHasLibXMLBug();
#endif

    CPLHashSet* hSetSchemas =
        CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);
    CPLXMLNode* psSchema =
        CPLLoadSchemaStrInternal(hSetSchemas, pszXSDFilename);

    char* pszStr = nullptr;
    if( psSchema )
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

static void CPLLibXMLInputStreamCPLFree( xmlChar* pszBuffer )
{
    CPLFree(pszBuffer);
}

/************************************************************************/
/*                           CPLFindLocalXSD()                          */
/************************************************************************/

static CPLString CPLFindLocalXSD( const char* pszXSDFilename )
{
    CPLString osTmp;
    const char *pszSchemasOpenGIS =
        CPLGetConfigOption("GDAL_OPENGIS_SCHEMAS", nullptr);
    if( pszSchemasOpenGIS != nullptr )
    {
        int nLen = static_cast<int>(strlen(pszSchemasOpenGIS));
        if( nLen > 0 && pszSchemasOpenGIS[nLen-1] == '/' )
        {
            osTmp = pszSchemasOpenGIS;
            osTmp += pszXSDFilename;
        }
        else
        {
            osTmp = pszSchemasOpenGIS;
            osTmp += "/";
            osTmp += pszXSDFilename;
        }
    }
    else if( (pszSchemasOpenGIS = CPLFindFile( "gdal", "SCHEMAS_OPENGIS_NET" ))
             != nullptr )
    {
        osTmp = pszSchemasOpenGIS;
        osTmp += "/";
        osTmp += pszXSDFilename;
    }

    VSIStatBufL sStatBuf;
    if( VSIStatExL(osTmp, &sStatBuf, VSI_STAT_EXISTS_FLAG) == 0 )
        return osTmp;
    return "";
}

/************************************************************************/
/*                      CPLExternalEntityLoader()                       */
/************************************************************************/

constexpr char szXML_XSD[] =
    "<schema xmlns=\"http://www.w3.org/2001/XMLSchema\" "
    "targetNamespace=\"http://www.w3.org/XML/1998/namespace\">"
    "<attribute name=\"lang\">"
    "<simpleType>"
    "<union memberTypes=\"language\">"
    "<simpleType>"
    "<restriction base=\"string\">"
    "<enumeration value=\"\"/>"
    "</restriction>"
    "</simpleType>"
    "</union>"
    "</simpleType>"
    "</attribute>"
    "<attribute name=\"space\">"
    "<simpleType>"
    "<restriction base=\"NCName\">"
    "<enumeration value=\"default\"/>"
    "<enumeration value=\"preserve\"/>"
    "</restriction>"
    "</simpleType>"
    "</attribute>"
    "<attribute name=\"base\" type=\"anyURI\"/>"
    "<attribute name=\"id\" type=\"ID\"/>"
    "<attributeGroup name=\"specialAttrs\">"
    "<attribute ref=\"xml:base\"/>"
    "<attribute ref=\"xml:lang\"/>"
    "<attribute ref=\"xml:space\"/>"
    "<attribute ref=\"xml:id\"/>"
    "</attributeGroup>"
    "</schema>";

// Simplified (and truncated) version of http://www.w3.org/1999/xlink.xsd
// (sufficient for GML schemas).
constexpr char szXLINK_XSD[] =
    "<schema xmlns=\"http://www.w3.org/2001/XMLSchema\" "
    "targetNamespace=\"http://www.w3.org/1999/xlink\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
    "<attribute name=\"type\" type=\"string\"/>"
    "<attribute name=\"href\" type=\"anyURI\"/>"
    "<attribute name=\"role\" type=\"anyURI\"/>"
    "<attribute name=\"arcrole\" type=\"anyURI\"/>"
    "<attribute name=\"title\" type=\"string\"/>"
    "<attribute name=\"show\" type=\"string\"/>"
    "<attribute name=\"actuate\" type=\"string\"/>"
    "<attribute name=\"label\" type=\"NCName\"/>"
    "<attribute name=\"from\" type=\"NCName\"/>"
    "<attribute name=\"to\" type=\"NCName\"/>"
    "<attributeGroup name=\"simpleAttrs\">"
    "<attribute ref=\"xlink:type\" fixed=\"simple\"/>"
    "<attribute ref=\"xlink:href\"/>"
    "<attribute ref=\"xlink:role\"/>"
    "<attribute ref=\"xlink:arcrole\"/>"
    "<attribute ref=\"xlink:title\"/>"
    "<attribute ref=\"xlink:show\"/>"
    "<attribute ref=\"xlink:actuate\"/>"
    "</attributeGroup>"
    "</schema>";

static
xmlParserInputPtr CPLExternalEntityLoader( const char * URL,
                                           const char * ID,
                                           xmlParserCtxtPtr context )
{
#if DEBUG_VERBOSE
    CPLDebug("CPL", "CPLExternalEntityLoader(%s)", URL);
#endif
    // Use libxml2 catalog mechanism to resolve the URL to something else.
    // xmlChar* pszResolved = xmlCatalogResolveSystem((const xmlChar*)URL);
    xmlChar* pszResolved =
        xmlCatalogResolveSystem(reinterpret_cast<const xmlChar *>(URL));
    if( pszResolved == nullptr )
        pszResolved =
            xmlCatalogResolveURI(reinterpret_cast<const xmlChar *>(URL));
    CPLString osURL;
    if( pszResolved )
    {
        CPLDebug( "CPL", "Resolving %s in %s", URL,
                  reinterpret_cast<const char *>(pszResolved) );
        osURL = reinterpret_cast<const char *>(pszResolved);
        URL = osURL.c_str();
        xmlFree(pszResolved);
        pszResolved = nullptr;
    }

    if( STARTS_WITH(URL, "http://") )
    {
        // Make sure to use http://schemas.opengis.net/
        // when gml/2 or gml/3 is detected.
        const char* pszGML = strstr(URL, "gml/2");
        if( pszGML == nullptr )
            pszGML = strstr(URL, "gml/3");
        if( pszGML != nullptr )
        {
            osURL = "http://schemas.opengis.net/";
            osURL += pszGML;
            URL = osURL.c_str();
        }
        else if( strcmp(URL, "http://www.w3.org/2001/xml.xsd") == 0 )
        {
            CPLString osTmp = CPLFindLocalXSD("xml.xsd");
            if( !osTmp.empty() )
            {
                osURL = osTmp;
                URL = osURL.c_str();
            }
            else
            {
                CPLDebug(
                    "CPL",
                    "Resolving %s to local definition",
                    "http://www.w3.org/2001/xml.xsd" );
                return xmlNewStringInputStream(
                    context, reinterpret_cast<const xmlChar*>(szXML_XSD) );
            }
        }
        else if( strcmp(URL, "http://www.w3.org/1999/xlink.xsd") == 0 )
        {
            CPLString osTmp = CPLFindLocalXSD("xlink.xsd");
            if( !osTmp.empty() )
            {
                osURL = osTmp;
                URL = osURL.c_str();
            }
            else
            {
                CPLDebug(
                    "CPL",
                    "Resolving %s to local definition",
                    "http://www.w3.org/1999/xlink.xsd" );
                return xmlNewStringInputStream(
                    context, reinterpret_cast<const xmlChar *>( szXLINK_XSD) );
            }
        }
        else if( !STARTS_WITH(URL, "http://schemas.opengis.net/") )
        {
            CPLDebug("CPL", "Loading %s", URL);
            return pfnLibXMLOldExtranerEntityLoader(URL, ID, context);
        }
    }
    else if( STARTS_WITH(URL, "ftp://") )
    {
        return pfnLibXMLOldExtranerEntityLoader(URL, ID, context);
    }
    else if( STARTS_WITH(URL, "file://") )
    {
        // Parse file:// URI so as to be able to open them with VSI*L API.
        if( STARTS_WITH(URL, "file://localhost/") )
            URL += 16;
        else
            URL += 7;

        if( URL[0] == '/' && URL[1] != '\0' && URL[2] == ':' && URL[3] == '/' )
        {
            // Windows.
            ++URL;
        }
        else if( URL[0] == '/' )
        {
            // Unix.
        }
        else
        {
            return pfnLibXMLOldExtranerEntityLoader(URL, ID, context);
        }
    }

    CPLString osModURL;
    if( STARTS_WITH(URL, "/vsizip/vsicurl/http%3A//") )
    {
        osModURL = "/vsizip/vsicurl/http://";
        osModURL += URL + strlen("/vsizip/vsicurl/http%3A//");
    }
    else if( STARTS_WITH(URL, "/vsicurl/http%3A//") )
    {
        osModURL = "vsicurl/http://";
        osModURL += URL + strlen("/vsicurl/http%3A//");
    }
    else if( STARTS_WITH(URL, "http://schemas.opengis.net/") )
    {
        const char *pszAfterOpenGIS =
                URL + strlen("http://schemas.opengis.net/");

        const char *pszSchemasOpenGIS =
            CPLGetConfigOption("GDAL_OPENGIS_SCHEMAS", nullptr);
        if( pszSchemasOpenGIS != nullptr )
        {
            const int nLen = static_cast<int>(strlen(pszSchemasOpenGIS));
            if( nLen > 0 && pszSchemasOpenGIS[nLen-1] == '/' )
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
        else if( (pszSchemasOpenGIS =
                      CPLFindFile( "gdal", "SCHEMAS_OPENGIS_NET" )) != nullptr )
        {
            osModURL = pszSchemasOpenGIS;
            osModURL += "/";
            osModURL += pszAfterOpenGIS;
        }
        else if( (pszSchemasOpenGIS =
                      CPLFindFile( "gdal", "SCHEMAS_OPENGIS_NET.zip" ))
                 != nullptr )
        {
            osModURL = "/vsizip/";
            osModURL += pszSchemasOpenGIS;
            osModURL += "/";
            osModURL += pszAfterOpenGIS;
        }
        else
        {
            osModURL =
                "/vsizip/vsicurl/"
                "http://schemas.opengis.net/SCHEMAS_OPENGIS_NET.zip/";
            osModURL += pszAfterOpenGIS;
        }
    }
    else
    {
        osModURL = URL;
    }

    xmlChar* pszBuffer =
        reinterpret_cast<xmlChar *>(CPLLoadSchemaStr(osModURL));
    if( pszBuffer == nullptr )
        return nullptr;

    xmlParserInputPtr poInputStream =
        xmlNewStringInputStream(context, pszBuffer);
    if( poInputStream != nullptr )
        poInputStream->free = CPLLibXMLInputStreamCPLFree;
    return poInputStream;
}

/************************************************************************/
/*                    CPLLibXMLWarningErrorCallback()                   */
/************************************************************************/

static void CPLLibXMLWarningErrorCallback ( void * ctx, const char * msg, ... )
{
    va_list varg;
    va_start(varg, msg);

    char *pszStr = reinterpret_cast<char *>(va_arg( varg, char *));

    if( strstr(pszStr, "since this namespace was already imported") == nullptr )
    {
        xmlErrorPtr pErrorPtr = xmlGetLastError();
        const char* pszFilename = static_cast<char *>(ctx);
        char* pszStrDup = CPLStrdup(pszStr);
        int nLen = static_cast<int>(strlen(pszStrDup));
        if( nLen > 0 && pszStrDup[nLen-1] == '\n' )
            pszStrDup[nLen-1] = '\0';
        if( pszFilename != nullptr && pszFilename[0] != '<' )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "libXML: %s:%d: %s",
                     pszFilename, pErrorPtr ? pErrorPtr->line : 0, pszStrDup);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "libXML: %d: %s",
                     pErrorPtr ? pErrorPtr->line : 0, pszStrDup);
        }
        CPLFree(pszStrDup);
    }

    va_end(varg);
}

/************************************************************************/
/*                      CPLLoadContentFromFile()                        */
/************************************************************************/

static
char* CPLLoadContentFromFile( const char* pszFilename )
{
    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if( fp == nullptr )
        return nullptr;
    if( VSIFSeekL(fp, 0, SEEK_END) != 0 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        return nullptr;
    }
    vsi_l_offset nSize = VSIFTellL(fp);
    if( VSIFSeekL(fp, 0, SEEK_SET) != 0 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        return nullptr;
    }
    if( static_cast<vsi_l_offset>(static_cast<int>(nSize)) != nSize ||
        nSize > INT_MAX - 1 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        return nullptr;
    }
    char* pszBuffer =
        static_cast<char *>(VSIMalloc(static_cast<size_t>(nSize) + 1));
    if( pszBuffer == nullptr )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        return nullptr;
    }
    if( static_cast<size_t>(VSIFReadL(pszBuffer, 1,
                                      static_cast<size_t>(nSize), fp))
        != static_cast<size_t>(nSize) )
    {
        VSIFree(pszBuffer);
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        return nullptr;
    }
    pszBuffer[nSize] = '\0';
    CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
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
 * @since GDAL 1.10.0
 */

static
CPLXMLSchemaPtr CPLLoadXMLSchema( const char* pszXSDFilename )
{
    char* pszStr = CPLLoadSchemaStr(pszXSDFilename);
    if( pszStr == nullptr )
        return nullptr;

    xmlExternalEntityLoader pfnLibXMLOldExtranerEntityLoaderLocal = nullptr;
    pfnLibXMLOldExtranerEntityLoaderLocal = xmlGetExternalEntityLoader();
    pfnLibXMLOldExtranerEntityLoader = pfnLibXMLOldExtranerEntityLoaderLocal;
    xmlSetExternalEntityLoader(CPLExternalEntityLoader);

    xmlSchemaParserCtxtPtr pSchemaParserCtxt =
        xmlSchemaNewMemParserCtxt(pszStr, static_cast<int>(strlen(pszStr)));

    xmlSchemaSetParserErrors(pSchemaParserCtxt,
                             CPLLibXMLWarningErrorCallback,
                             CPLLibXMLWarningErrorCallback,
                             nullptr);

    xmlSchemaPtr pSchema = xmlSchemaParse(pSchemaParserCtxt);
    xmlSchemaFreeParserCtxt(pSchemaParserCtxt);

    xmlSetExternalEntityLoader(pfnLibXMLOldExtranerEntityLoaderLocal);

    CPLFree(pszStr);

    return static_cast<CPLXMLSchemaPtr>( pSchema );
}

/************************************************************************/
/*                         CPLFreeXMLSchema()                           */
/************************************************************************/

/**
 * \brief Free a XSD schema.
 *
 * @param pSchema a handle to the parsed XML schema.
 *
 * @since GDAL 1.10.0
 */

static
void CPLFreeXMLSchema( CPLXMLSchemaPtr pSchema )
{
    if( pSchema )
        xmlSchemaFree(static_cast<xmlSchemaPtr>(pSchema));
}

/************************************************************************/
/*                          CPLValidateXML()                            */
/************************************************************************/

/**
 * \brief Validate a XML file against a XML schema.
 *
 * @param pszXMLFilename the filename of the XML file to validate.
 * @param pszXSDFilename the filename of the XSD schema.
 * @param papszOptions unused for now. Set to NULL.
 * @return TRUE if the XML file validates against the XML schema.
 *
 * @since GDAL 1.10.0
 */

int CPLValidateXML( const char* pszXMLFilename,
                    const char* pszXSDFilename,
                    CPL_UNUSED CSLConstList papszOptions )
{
    char szHeader[2048] = {};  // TODO(schwehr): Get this off of the stack.
    CPLString osTmpXSDFilename;

    if( pszXMLFilename[0] == '<' )
    {
        strncpy(szHeader, pszXMLFilename, sizeof(szHeader));
        szHeader[sizeof(szHeader)-1] = '\0';
    }
    else
    {
        VSILFILE* fpXML = VSIFOpenL(pszXMLFilename, "rb");
        if( fpXML == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Cannot open %s", pszXMLFilename );
            return FALSE;
        }
        const vsi_l_offset nRead =
            VSIFReadL(szHeader, 1, sizeof(szHeader) - 1, fpXML);
        szHeader[nRead] = '\0';
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpXML));
    }

    // Workaround following bug:
    //
    // "element FeatureCollection: Schemas validity error : Element
    // '{http://www.opengis.net/wfs}FeatureCollection': No matching global
    // declaration available for the validation root"
    //
    // We create a wrapping XSD that imports the WFS .xsd (and possibly the GML
    // .xsd too) and the application schema.  This is a known libxml2
    // limitation.
    if( strstr(szHeader, "<wfs:FeatureCollection") ||
        (strstr(szHeader, "<FeatureCollection") &&
         strstr(szHeader, "xmlns:wfs=\"http://www.opengis.net/wfs\"")) )
    {
        const char* pszWFSSchemaNamespace = "http://www.opengis.net/wfs";
        const char* pszWFSSchemaLocation = nullptr;
        const char* pszGMLSchemaLocation = nullptr;
        if( strstr(szHeader, "wfs/1.0.0/WFS-basic.xsd") )
        {
            pszWFSSchemaLocation =
                "http://schemas.opengis.net/wfs/1.0.0/WFS-basic.xsd";
        }
        else if( strstr(szHeader, "wfs/1.1.0/wfs.xsd") )
        {
            pszWFSSchemaLocation =
                "http://schemas.opengis.net/wfs/1.1.0/wfs.xsd";
        }
        else if( strstr(szHeader, "wfs/2.0/wfs.xsd") )
        {
            pszWFSSchemaNamespace = "http://www.opengis.net/wfs/2.0";
            pszWFSSchemaLocation = "http://schemas.opengis.net/wfs/2.0/wfs.xsd";
        }

        VSILFILE* fpXSD = VSIFOpenL(pszXSDFilename, "rb");
        if( fpXSD == nullptr )
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                    "Cannot open %s", pszXSDFilename);
            return FALSE;
        }
        const vsi_l_offset nRead =
            VSIFReadL(szHeader, 1, sizeof(szHeader) - 1, fpXSD);
        szHeader[nRead] = '\0';
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpXSD));

        if( strstr(szHeader, "gml/3.1.1") != nullptr &&
            strstr(szHeader, "gml/3.1.1/base/gml.xsd") == nullptr )
        {
            pszGMLSchemaLocation =
                "http://schemas.opengis.net/gml/3.1.1/base/gml.xsd";
        }

        if( pszWFSSchemaLocation != nullptr )
        {
            osTmpXSDFilename = CPLSPrintf(
                "/vsimem/CPLValidateXML_%p_%p.xsd",
                pszXMLFilename, pszXSDFilename );
            char * const pszEscapedXSDFilename =
                CPLEscapeString(pszXSDFilename, -1, CPLES_XML);
            VSILFILE * const fpMEM = VSIFOpenL(osTmpXSDFilename, "wb");
            CPL_IGNORE_RET_VAL(VSIFPrintfL(
                fpMEM,
                "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"));
            CPL_IGNORE_RET_VAL(VSIFPrintfL(
                fpMEM,
                "   <xs:import namespace=\"%s\" schemaLocation=\"%s\"/>\n",
                pszWFSSchemaNamespace, pszWFSSchemaLocation));
            CPL_IGNORE_RET_VAL(VSIFPrintfL(
                fpMEM,
                "   <xs:import namespace=\"ignored\" schemaLocation=\"%s\"/>\n",
                pszEscapedXSDFilename));
            if( pszGMLSchemaLocation )
                CPL_IGNORE_RET_VAL(VSIFPrintfL(
                    fpMEM,
                    "   <xs:import namespace=\"http://www.opengis.net/gml\" "
                    "schemaLocation=\"%s\"/>\n", pszGMLSchemaLocation));
            CPL_IGNORE_RET_VAL(VSIFPrintfL(fpMEM, "</xs:schema>\n"));
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpMEM));
            CPLFree(pszEscapedXSDFilename);
        }
    }

    CPLXMLSchemaPtr pSchema =
        CPLLoadXMLSchema(!osTmpXSDFilename.empty()
                         ? osTmpXSDFilename.c_str()
                         : pszXSDFilename);
    if( !osTmpXSDFilename.empty() )
        VSIUnlink(osTmpXSDFilename);
    if( pSchema == nullptr )
        return FALSE;

    xmlSchemaValidCtxtPtr pSchemaValidCtxt =
        xmlSchemaNewValidCtxt(static_cast<xmlSchemaPtr>(pSchema));

    if( pSchemaValidCtxt == nullptr )
    {
        CPLFreeXMLSchema(pSchema);
        return FALSE;
    }

    xmlSchemaSetValidErrors(pSchemaValidCtxt,
                            CPLLibXMLWarningErrorCallback,
                            CPLLibXMLWarningErrorCallback,
                            const_cast<char *>(pszXMLFilename) );

    bool bValid = false;
    if( pszXMLFilename[0] == '<' )
    {
        xmlDocPtr pDoc =
            xmlParseDoc(reinterpret_cast<const xmlChar *>(pszXMLFilename));
        if( pDoc != nullptr )
        {
            bValid = xmlSchemaValidateDoc(pSchemaValidCtxt, pDoc) == 0;
        }
        xmlFreeDoc(pDoc);
    }
    else if( !STARTS_WITH(pszXMLFilename, "/vsi") )
    {
        bValid =
            xmlSchemaValidateFile(pSchemaValidCtxt, pszXMLFilename, 0) == 0;
    }
    else
    {
        char* pszXML = CPLLoadContentFromFile(pszXMLFilename);
        if( pszXML != nullptr )
        {
            xmlDocPtr pDoc =
                xmlParseDoc(reinterpret_cast<const xmlChar *>(pszXML));
            if( pDoc != nullptr )
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

#else  // HAVE_RECENT_LIBXML2

/************************************************************************/
/*                          CPLValidateXML()                            */
/************************************************************************/

int CPLValidateXML( const char* /* pszXMLFilename */,
                    const char* /* pszXSDFilename */,
                    CSLConstList /* papszOptions */ )
{
    CPLError( CE_Failure, CPLE_NotSupported,
              "%s not implemented due to missing libxml2 support",
              "CPLValidateXML()" );
    return FALSE;
}

#endif  // HAVE_RECENT_LIBXML2
