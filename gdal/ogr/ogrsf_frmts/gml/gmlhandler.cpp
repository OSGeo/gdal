/**********************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLHandler class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include <ctype.h>
#include "gmlreaderp.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_hash_set.h"

#ifdef HAVE_XERCES

/* Must be a multiple of 4 */
#define MAX_TOKEN_SIZE  1000

/************************************************************************/
/*                        GMLXercesHandler()                            */
/************************************************************************/

GMLXercesHandler::GMLXercesHandler( GMLReader *poReader ) : GMLHandler(poReader)
{
    m_nEntityCounter = 0;
}

/************************************************************************/
/*                            startElement()                            */
/************************************************************************/

void GMLXercesHandler::startElement(CPL_UNUSED const XMLCh* const uri,
                                    const XMLCh* const localname,
                                    CPL_UNUSED const XMLCh* const qname,
                                    const Attributes& attrs )
{
    char        szElementName[MAX_TOKEN_SIZE];

    m_nEntityCounter = 0;

    /* A XMLCh character can expand to 4 bytes in UTF-8 */
    if (4 * tr_strlen( localname ) >= MAX_TOKEN_SIZE)
    {
        static int bWarnOnce = FALSE;
        XMLCh* tempBuffer = (XMLCh*) CPLMalloc(sizeof(XMLCh) * (MAX_TOKEN_SIZE / 4 + 1));
        memcpy(tempBuffer, localname, sizeof(XMLCh) * (MAX_TOKEN_SIZE / 4));
        tempBuffer[MAX_TOKEN_SIZE / 4] = 0;
        tr_strcpy( szElementName, tempBuffer );
        CPLFree(tempBuffer);
        if (!bWarnOnce)
        {
            bWarnOnce = TRUE;
            CPLError(CE_Warning, CPLE_AppDefined, "A too big element name has been truncated");
        }
    }
    else
        tr_strcpy( szElementName, localname );

    if (GMLHandler::startElement(szElementName, strlen(szElementName), (void*) &attrs) == OGRERR_NOT_ENOUGH_MEMORY)
    {
        throw SAXNotSupportedException("Out of memory");
    }
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
void GMLXercesHandler::endElement(CPL_UNUSED const XMLCh* const uri,
                                  CPL_UNUSED const XMLCh* const localname,
                                  CPL_UNUSED const XMLCh* const qname )
{
    m_nEntityCounter = 0;

    if (GMLHandler::endElement() == OGRERR_NOT_ENOUGH_MEMORY)
    {
        throw SAXNotSupportedException("Out of memory");
    }
}

/************************************************************************/
/*                             characters()                             */
/************************************************************************/

void GMLXercesHandler::characters(const XMLCh* const chars_in,
                                  CPL_UNUSED
#if XERCES_VERSION_MAJOR >= 3
                                  const XMLSize_t length
#else
                                  const unsigned int length
#endif
                                  )

{
    char* utf8String = tr_strdup(chars_in);
    int nLen = strlen(utf8String);
    OGRErr eErr = GMLHandler::dataHandler(utf8String, nLen);
    CPLFree(utf8String);
    if (eErr == OGRERR_NOT_ENOUGH_MEMORY)
    {
        throw SAXNotSupportedException("Out of memory");
    }
}

/************************************************************************/
/*                             fatalError()                             */
/************************************************************************/

void GMLXercesHandler::fatalError( const SAXParseException &exception)

{
    char *pszErrorMessage;

    pszErrorMessage = tr_strdup( exception.getMessage() );
    CPLError( CE_Failure, CPLE_AppDefined, 
              "XML Parsing Error: %s at line %d, column %d\n", 
              pszErrorMessage, (int)exception.getLineNumber(), (int)exception.getColumnNumber() );

    CPLFree( pszErrorMessage );
}

/************************************************************************/
/*                             startEntity()                            */
/************************************************************************/

void GMLXercesHandler::startEntity (CPL_UNUSED const XMLCh *const name)
{
    m_nEntityCounter ++;
    if (m_nEntityCounter > 1000 && !m_poReader->HasStoppedParsing())
    {
        throw SAXNotSupportedException("File probably corrupted (million laugh pattern)");
    }
}

/************************************************************************/
/*                               GetFID()                               */
/************************************************************************/

const char* GMLXercesHandler::GetFID(void* attr)
{
    const Attributes* attrs = (const Attributes*) attr;
    int nFIDIndex;
    XMLCh   anFID[100];

    tr_strcpy( anFID, "fid" );
    nFIDIndex = attrs->getIndex( anFID );
    if( nFIDIndex != -1 )
    {
        char* pszValue = tr_strdup( attrs->getValue( nFIDIndex ) );
        osFID.assign(pszValue);
        CPLFree(pszValue);
        return osFID.c_str();
    }
    else
    {
        tr_strcpy( anFID, "gml:id" );
        nFIDIndex = attrs->getIndex( anFID );
        if( nFIDIndex != -1 )
        {
            char* pszValue = tr_strdup( attrs->getValue( nFIDIndex ) );
            osFID.assign(pszValue);
            CPLFree(pszValue);
            return osFID.c_str();
        }
    }

    osFID.resize(0);
    return NULL;
}

/************************************************************************/
/*                        AddAttributes()                               */
/************************************************************************/

CPLXMLNode* GMLXercesHandler::AddAttributes(CPLXMLNode* psNode, void* attr)
{
    const Attributes* attrs = (const Attributes*) attr;

    CPLXMLNode* psLastChild = NULL;

    for(unsigned int i=0; i < attrs->getLength(); i++)
    {
        char* pszName = tr_strdup(attrs->getQName(i));
        char* pszValue = tr_strdup(attrs->getValue(i));

        CPLXMLNode* psChild = CPLCreateXMLNode(NULL, CXT_Attribute, pszName);
        CPLCreateXMLNode(psChild, CXT_Text, pszValue);

        CPLFree(pszName);
        CPLFree(pszValue);

        if (psLastChild == NULL)
            psNode->psChild = psChild;
        else
            psLastChild->psNext = psChild;
        psLastChild = psChild;
    }

    return psLastChild;
}

/************************************************************************/
/*                    GetAttributeValue()                               */
/************************************************************************/

char* GMLXercesHandler::GetAttributeValue(void* attr, const char* pszAttributeName)
{
    const Attributes* attrs = (const Attributes*) attr;
    for(unsigned int i=0; i < attrs->getLength(); i++)
    {
        char* pszString = tr_strdup(attrs->getQName(i));
        if (strcmp(pszString, pszAttributeName) == 0)
        {
            CPLFree(pszString);
            return tr_strdup(attrs->getValue(i));
        }
        CPLFree(pszString);
    }
    return NULL;
}

/************************************************************************/
/*                    GetAttributeByIdx()                               */
/************************************************************************/

char* GMLXercesHandler::GetAttributeByIdx(void* attr, unsigned int idx, char** ppszKey)
{
    const Attributes* attrs = (const Attributes*) attr;
    if( idx >= attrs->getLength() )
    {
        *ppszKey = NULL;
        return NULL;
    }
    *ppszKey = tr_strdup(attrs->getQName(idx));
    return tr_strdup(attrs->getValue(idx));
}

#endif

#ifdef HAVE_EXPAT

/************************************************************************/
/*                            GMLExpatHandler()                         */
/************************************************************************/

GMLExpatHandler::GMLExpatHandler( GMLReader *poReader, XML_Parser oParser ) : GMLHandler(poReader)

{
    m_oParser = oParser;
    m_bStopParsing = FALSE;
    m_nDataHandlerCounter = 0;
}

/************************************************************************/
/*                           startElementCbk()                          */
/************************************************************************/

void XMLCALL GMLExpatHandler::startElementCbk(void *pUserData, const char *pszName,
                                              const char **ppszAttr)

{
    GMLExpatHandler* pThis = ((GMLExpatHandler*)pUserData);
    if (pThis->m_bStopParsing)
        return;

    const char* pszIter = pszName;
    char ch;
    while((ch = *pszIter) != '\0')
    {
        if (ch == ':')
            pszName = pszIter + 1;
        pszIter ++;
    }

    if (pThis->GMLHandler::startElement(pszName, (int)(pszIter - pszName), ppszAttr) == OGRERR_NOT_ENOUGH_MEMORY)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        pThis->m_bStopParsing = TRUE;
        XML_StopParser(pThis->m_oParser, XML_FALSE);
    }

}

/************************************************************************/
/*                            endElementCbk()                           */
/************************************************************************/
void XMLCALL GMLExpatHandler::endElementCbk(void *pUserData,
                                            CPL_UNUSED const char* pszName )
{
    GMLExpatHandler* pThis = ((GMLExpatHandler*)pUserData);
    if (pThis->m_bStopParsing)
        return;

    if (pThis->GMLHandler::endElement() == OGRERR_NOT_ENOUGH_MEMORY)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        pThis->m_bStopParsing = TRUE;
        XML_StopParser(pThis->m_oParser, XML_FALSE);
    }
}

/************************************************************************/
/*                            dataHandlerCbk()                          */
/************************************************************************/

void XMLCALL GMLExpatHandler::dataHandlerCbk(void *pUserData, const char *data, int nLen)

{
    GMLExpatHandler* pThis = ((GMLExpatHandler*)pUserData);
    if (pThis->m_bStopParsing)
        return;

    pThis->m_nDataHandlerCounter ++;
    /* The size of the buffer that is fetched and that Expat parses is */
    /* PARSER_BUF_SIZE bytes. If the dataHandlerCbk() callback is called */
    /* more than PARSER_BUF_SIZE times, this means that one byte in the */
    /* file expands to more XML text fragments, which is the sign of a */
    /* likely abuse of <!ENTITY> */
    /* Note: the counter is zeroed by ResetDataHandlerCounter() before each */
    /* new XML parsing. */
    if (pThis->m_nDataHandlerCounter >= PARSER_BUF_SIZE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "File probably corrupted (million laugh pattern)");
        pThis->m_bStopParsing = TRUE;
        XML_StopParser(pThis->m_oParser, XML_FALSE);
        return;
    }

    if (pThis->GMLHandler::dataHandler(data, nLen) == OGRERR_NOT_ENOUGH_MEMORY)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        pThis->m_bStopParsing = TRUE;
        XML_StopParser(pThis->m_oParser, XML_FALSE);
        return;
    }
}

/************************************************************************/
/*                               GetFID()                               */
/************************************************************************/

const char* GMLExpatHandler::GetFID(void* attr)
{
    const char** papszIter = (const char** )attr;
    while(*papszIter)
    {
        if (strcmp(*papszIter, "fid") == 0 ||
            strcmp(*papszIter, "gml:id") == 0)
        {
            return papszIter[1];
        }
        papszIter += 2;
    }
    return NULL;
}

/************************************************************************/
/*                        AddAttributes()                               */
/************************************************************************/

CPLXMLNode* GMLExpatHandler::AddAttributes(CPLXMLNode* psNode, void* attr)
{
    const char** papszIter = (const char** )attr;

    CPLXMLNode* psLastChild = NULL;

    while(*papszIter)
    {
        CPLXMLNode* psChild = CPLCreateXMLNode(NULL, CXT_Attribute, papszIter[0]);
        CPLCreateXMLNode(psChild, CXT_Text, papszIter[1]);

        if (psLastChild == NULL)
            psNode->psChild = psChild;
        else
            psLastChild->psNext = psChild;
        psLastChild = psChild;

        papszIter += 2;
    }

    return psLastChild;
}

/************************************************************************/
/*                    GetAttributeValue()                               */
/************************************************************************/

char* GMLExpatHandler::GetAttributeValue(void* attr, const char* pszAttributeName)
{
    const char** papszIter = (const char** )attr;
    while(*papszIter)
    {
        if (strcmp(*papszIter, pszAttributeName) == 0)
        {
            return CPLStrdup(papszIter[1]);
        }
        papszIter += 2;
    }
    return NULL;
}

/************************************************************************/
/*                    GetAttributeByIdx()                               */
/************************************************************************/

/* CAUTION: should be called with increasing idx starting from 0 and */
/* no attempt to read beyond end of list */
char* GMLExpatHandler::GetAttributeByIdx(void* attr, unsigned int idx, char** ppszKey)
{
    const char** papszIter = (const char** )attr;
    if( papszIter[2 * idx] == NULL )
    {
        *ppszKey = NULL;
        return NULL;
    }
    *ppszKey = CPLStrdup(papszIter[2 * idx]);
    return CPLStrdup(papszIter[2 * idx+1]);
}

#endif


static const char* const apszGMLGeometryElements[] =
{
    "BoundingBox", /* ows:BoundingBox */
    "CompositeCurve",
    "CompositeSurface",
    "Curve",
    "GeometryCollection", /* OGR < 1.8.0 bug... */
    "LineString",
    "MultiCurve",
    "MultiGeometry",
    "MultiLineString",
    "MultiPoint",
    "MultiPolygon",
    "MultiSurface",
    "Point",
    "Polygon",
    "PolygonPatch",
    "SimplePolygon", /* GML 3.3 compact encoding */
    "SimpleRectangle", /* GML 3.3 compact encoding */
    "SimpleTriangle", /* GML 3.3 compact encoding */
    "SimpleMultiPoint", /* GML 3.3 compact encoding */
    "Solid",
    "Surface",
    "TopoCurve",
    "TopoSurface"
};

#define GML_GEOMETRY_TYPE_COUNT  \
    (int)(sizeof(apszGMLGeometryElements) / sizeof(apszGMLGeometryElements[0]))

struct _GeometryNamesStruct {
    unsigned long nHash;
    const char   *pszName;
} ;

/************************************************************************/
/*                    GMLHandlerSortGeometryElements()                  */
/************************************************************************/

static int GMLHandlerSortGeometryElements(const void *_pA, const void *_pB)
{
    GeometryNamesStruct* pA = (GeometryNamesStruct*)_pA;
    GeometryNamesStruct* pB = (GeometryNamesStruct*)_pB;
    CPLAssert(pA->nHash != pB->nHash);
    if (pA->nHash < pB->nHash)
        return -1;
    else
        return 1;
}

/************************************************************************/
/*                            GMLHandler()                              */
/************************************************************************/

GMLHandler::GMLHandler( GMLReader *poReader )

{
    m_poReader = poReader;
    m_bInCurField = FALSE;
    m_nCurFieldAlloc = 0;
    m_nCurFieldLen = 0;
    m_pszCurField = NULL;
    m_nAttributeIndex = -1;
    m_nAttributeDepth = 0;

    m_pszGeometry = NULL;
    m_nGeomAlloc = 0;
    m_nGeomLen = 0;
    m_nGeometryDepth = 0;
    m_bAlreadyFoundGeometry = FALSE;
    m_nGeometryPropertyIndex = 0;

    m_nDepthFeature = m_nDepth = 0;
    m_inBoundedByDepth = 0;

    eAppSchemaType = APPSCHEMA_GENERIC;

    m_pszCityGMLGenericAttrName = NULL;
    m_inCityGMLGenericAttrDepth = 0;

    m_bReportHref = FALSE;
    m_pszHref = NULL;
    m_pszUom = NULL;
    m_pszValue = NULL;
    m_pszKieli = NULL;

    pasGeometryNames = (GeometryNamesStruct*)CPLMalloc(
        GML_GEOMETRY_TYPE_COUNT * sizeof(GeometryNamesStruct));
    for(int i=0; i<GML_GEOMETRY_TYPE_COUNT; i++)
    {
        pasGeometryNames[i].pszName = apszGMLGeometryElements[i];
        pasGeometryNames[i].nHash =
                    CPLHashSetHashStr(pasGeometryNames[i].pszName);
    }
    qsort(pasGeometryNames, GML_GEOMETRY_TYPE_COUNT,
          sizeof(GeometryNamesStruct),
          GMLHandlerSortGeometryElements);

    nStackDepth = 0;
    stateStack[0] = STATE_TOP;
}

/************************************************************************/
/*                            ~GMLHandler()                             */
/************************************************************************/

GMLHandler::~GMLHandler()

{
    if (apsXMLNode.size() >= 2 && apsXMLNode[1].psNode != NULL)
        CPLDestroyXMLNode(apsXMLNode[1].psNode);

    CPLFree( m_pszCurField );
    CPLFree( m_pszGeometry );
    CPLFree( m_pszCityGMLGenericAttrName );
    CPLFree( m_pszHref );
    CPLFree( m_pszUom );
    CPLFree( m_pszValue );
    CPLFree( m_pszKieli );
    CPLFree( pasGeometryNames );
}


/************************************************************************/
/*                             startElement()                           */
/************************************************************************/

OGRErr GMLHandler::startElement(const char *pszName, int nLenName, void* attr)
{
    OGRErr eRet;
    switch(stateStack[nStackDepth])
    {
        case STATE_TOP:                 eRet = startElementTop(pszName, nLenName, attr); break;
        case STATE_DEFAULT:             eRet = startElementDefault(pszName, nLenName, attr); break;
        case STATE_FEATURE:             eRet = startElementFeatureAttribute(pszName, nLenName, attr); break;
        case STATE_PROPERTY:            eRet = startElementFeatureAttribute(pszName, nLenName, attr); break;
        case STATE_FEATUREPROPERTY:     eRet = startElementFeatureProperty(pszName, nLenName, attr); break;
        case STATE_GEOMETRY:            eRet = startElementGeometry(pszName, nLenName, attr); break;
        case STATE_IGNORED_FEATURE:     eRet = OGRERR_NONE; break;
        case STATE_BOUNDED_BY:          eRet = startElementBoundedBy(pszName, nLenName, attr); break;
        case STATE_CITYGML_ATTRIBUTE:   eRet = startElementCityGMLGenericAttr(pszName, nLenName, attr); break;
        default:                        eRet = OGRERR_NONE; break;
    }
    m_nDepth++;
    return eRet;
}

/************************************************************************/
/*                              endElement()                            */
/************************************************************************/

OGRErr GMLHandler::endElement()
{
    m_nDepth--;
    switch(stateStack[nStackDepth])
    {
        case STATE_TOP:                 return OGRERR_NONE; break;
        case STATE_DEFAULT:             return endElementDefault(); break;
        case STATE_FEATURE:             return endElementFeature(); break;
        case STATE_PROPERTY:            return endElementAttribute(); break;
        case STATE_FEATUREPROPERTY:     return endElementFeatureProperty(); break;
        case STATE_GEOMETRY:            return endElementGeometry(); break;
        case STATE_IGNORED_FEATURE:     return endElementIgnoredFeature(); break;
        case STATE_BOUNDED_BY:          return endElementBoundedBy(); break;
        case STATE_CITYGML_ATTRIBUTE:   return endElementCityGMLGenericAttr(); break;
        default:                        return OGRERR_NONE; break;
    }
}

/************************************************************************/
/*                              dataHandler()                           */
/************************************************************************/

OGRErr GMLHandler::dataHandler(const char *data, int nLen)
{
    switch(stateStack[nStackDepth])
    {
        case STATE_TOP:                 return OGRERR_NONE; break;
        case STATE_DEFAULT:             return OGRERR_NONE; break;
        case STATE_FEATURE:             return OGRERR_NONE; break;
        case STATE_PROPERTY:            return dataHandlerAttribute(data, nLen); break;
        case STATE_FEATUREPROPERTY:     return OGRERR_NONE; break;
        case STATE_GEOMETRY:            return dataHandlerGeometry(data, nLen); break;
        case STATE_IGNORED_FEATURE:     return OGRERR_NONE; break;
        case STATE_BOUNDED_BY:          return OGRERR_NONE; break;
        case STATE_CITYGML_ATTRIBUTE:   return dataHandlerAttribute(data, nLen); break;
        default:                        return OGRERR_NONE; break;
    }
}

#define PUSH_STATE(val) do { nStackDepth ++; CPLAssert(nStackDepth < STACK_SIZE); stateStack[nStackDepth] = val; } while(0)
#define POP_STATE()     nStackDepth --

/************************************************************************/
/*                       startElementBoundedBy()                        */
/************************************************************************/

OGRErr GMLHandler::startElementBoundedBy(const char *pszName,
                                         CPL_UNUSED int nLenName,
                                         void* attr )
{
    if ( m_nDepth == 2 && strcmp(pszName, "Envelope") == 0 )
    {
        char* pszGlobalSRSName = GetAttributeValue(attr, "srsName");
        m_poReader->SetGlobalSRSName(pszGlobalSRSName);
        CPLFree(pszGlobalSRSName);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       startElementGeometry()                         */
/************************************************************************/

OGRErr GMLHandler::startElementGeometry(const char *pszName, int nLenName, void* attr )
{
    if( nLenName == 9 && strcmp(pszName, "boundedBy") == 0 )
    {
        m_inBoundedByDepth = m_nDepth;

        PUSH_STATE(STATE_BOUNDED_BY);

        return OGRERR_NONE;
    }

    /* Create new XML Element */
    CPLXMLNode* psCurNode = (CPLXMLNode *) CPLCalloc(sizeof(CPLXMLNode),1);
    psCurNode->eType = CXT_Element;
    psCurNode->pszValue = (char*) CPLMalloc( nLenName+1 );
    memcpy(psCurNode->pszValue, pszName, nLenName+1);

    /* Attach element as the last child of its parent */
    NodeLastChild& sNodeLastChild = apsXMLNode[apsXMLNode.size()-1];
    CPLXMLNode* psLastChildParent = sNodeLastChild.psLastChild;

    if (psLastChildParent == NULL)
    {
        CPLXMLNode* psParent = sNodeLastChild.psNode;
        if (psParent)
            psParent->psChild = psCurNode;
    }
    else
    {
        psLastChildParent->psNext = psCurNode;
    }
    sNodeLastChild.psLastChild = psCurNode;

    /* Add attributes to the element */
    CPLXMLNode* psLastChildCurNode = AddAttributes(psCurNode, attr);

    /* Some CityGML lack a srsDimension="3" in posList, such as in */
    /* http://www.citygml.org/fileadmin/count.php?f=fileadmin%2Fcitygml%2Fdocs%2FFrankfurt_Street_Setting_LOD3.zip */
    /* So we have to add it manually */
    if (eAppSchemaType == APPSCHEMA_CITYGML && nLenName == 7 &&
        strcmp(pszName, "posList") == 0 &&
        CPLGetXMLValue(psCurNode, "srsDimension", NULL) == NULL)
    {
        CPLXMLNode* psChild = CPLCreateXMLNode(NULL, CXT_Attribute, "srsDimension");
        CPLCreateXMLNode(psChild, CXT_Text, "3");

        if (psLastChildCurNode == NULL)
            psCurNode->psChild = psChild;
        else
            psLastChildCurNode->psNext = psChild;
        psLastChildCurNode = psChild;
    }

    /* Push the element on the stack */
    NodeLastChild sNewNodeLastChild;
    sNewNodeLastChild.psNode = psCurNode;
    sNewNodeLastChild.psLastChild = psLastChildCurNode;
    apsXMLNode.push_back(sNewNodeLastChild);

    if (m_pszGeometry)
    {
        CPLFree(m_pszGeometry);
        m_pszGeometry = NULL;
        m_nGeomAlloc = 0;
        m_nGeomLen = 0;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                    startElementCityGMLGenericAttr()                  */
/************************************************************************/

OGRErr GMLHandler::startElementCityGMLGenericAttr(const char *pszName,
                                                  CPL_UNUSED int nLenName,
                                                  CPL_UNUSED void* attr )
{
    if( strcmp(pszName, "value") == 0 )
    {
        if(m_pszCurField)
        {
            CPLFree(m_pszCurField);
            m_pszCurField = NULL;
            m_nCurFieldLen = m_nCurFieldAlloc = 0;
        }
        m_bInCurField = TRUE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       DealWithAttributes()                           */
/************************************************************************/

void GMLHandler::DealWithAttributes(const char *pszName, int nLenName, void* attr )
{
    GMLReadState *poState = m_poReader->GetState();
    GMLFeatureClass *poClass = poState->m_poFeature->GetClass();

    for(unsigned int idx=0; TRUE ;idx++)
    {
        char* pszAttrKey = NULL;
        char* pszAttrVal = NULL;

        pszAttrVal = GetAttributeByIdx(attr, idx, &pszAttrKey);
        if( pszAttrVal == NULL )
            break;

        int nAttrIndex = 0;
        const char* pszAttrKeyNoNS = strchr(pszAttrKey, ':');
        if( pszAttrKeyNoNS != NULL )
            pszAttrKeyNoNS ++;

        /* If attribute is referenced by the .gfs */
        if( poClass->IsSchemaLocked() &&
            ( (pszAttrKeyNoNS != NULL &&
               (nAttrIndex =
                 m_poReader->GetAttributeElementIndex( pszName, nLenName, pszAttrKeyNoNS )) != -1) ||
              ((nAttrIndex =
                 m_poReader->GetAttributeElementIndex( pszName, nLenName, pszAttrKey )) != -1 ) ) )
        {
            nAttrIndex = FindRealPropertyByCheckingConditions( nAttrIndex, attr );
            if( nAttrIndex >= 0 )
            {
                m_poReader->SetFeaturePropertyDirectly( NULL, pszAttrVal, nAttrIndex );
                pszAttrVal = NULL;
            }
        }

        /* Hard-coded historic cases */
        else if( strcmp(pszAttrKey, "xlink:href") == 0 )
        {
            if (m_bReportHref)
            {
                if( m_bInCurField )
                {
                    CPLFree(m_pszHref);
                    m_pszHref = pszAttrVal;
                    pszAttrVal = NULL;
                }
                else if( !poClass->IsSchemaLocked() ||
                         (nAttrIndex =
                            m_poReader->GetAttributeElementIndex( CPLSPrintf("%s_href", pszName ),
                                                      nLenName + 5 )) != -1 )
                {
                    poState->PushPath( pszName, nLenName );
                    CPLString osPropNameHref = poState->osPath + "_href";
                    poState->PopPath();
                    m_poReader->SetFeaturePropertyDirectly( osPropNameHref, pszAttrVal, nAttrIndex );
                    pszAttrVal = NULL;
                }
            }
        }
        else if( strcmp(pszAttrKey, "uom") == 0 )
        {
            CPLFree(m_pszUom);
            m_pszUom = pszAttrVal;
            pszAttrVal = NULL;
        }
        else if( strcmp(pszAttrKey, "value") == 0 )
        {
            CPLFree(m_pszValue);
            m_pszValue = pszAttrVal;
            pszAttrVal = NULL;
        }
        else /* Get language in 'kieli' attribute of 'teksti' element */
        if( eAppSchemaType == APPSCHEMA_MTKGML &&
            nLenName == 6 && strcmp(pszName, "teksti") == 0 &&
            strcmp(pszAttrKey, "kieli") == 0 )
        {
            CPLFree(m_pszKieli);
            m_pszKieli = pszAttrVal;
            pszAttrVal = NULL;
        }

        /* Should we report all attributes ? */
        else if( m_poReader->ReportAllAttributes() && !poClass->IsSchemaLocked() )
        {
            poState->PushPath( pszName, nLenName );
            CPLString osPropName = poState->osPath;
            poState->PopPath();

            m_poReader->SetFeaturePropertyDirectly(
                CPLSPrintf("%s@%s", osPropName.c_str(), pszAttrKeyNoNS ? pszAttrKeyNoNS : pszAttrKey),
                pszAttrVal, -1 );
            pszAttrVal = NULL;
        }

        CPLFree(pszAttrKey);
        CPLFree(pszAttrVal);
    }

#if 0
    if( poClass->IsSchemaLocked() )
    {
        poState->PushPath( pszName, nLenName );
        CPLString osPath = poState->osPath;
        poState->PopPath();
        /* Find fields that match an attribute that is missing */
        for(int i=0; i < poClass->GetPropertyCount(); i++ )
        {
            GMLPropertyDefn* poProp = poClass->GetProperty(i);
            const char* pszSrcElement = poProp->GetSrcElement();
            if( poProp->GetType() == OFTStringList &&
                poProp->GetSrcElementLen() > osPath.size() &&
                strncmp(pszSrcElement, osPath, osPath.size()) == 0 &&
                pszSrcElement[osPath.size()] == '@' )
            {
                char* pszAttrVal = GetAttributeValue(attr, pszSrcElement + osPath.size() + 1);
                if( pszAttrVal == NULL )
                {
                    const char* pszCond = poProp->GetCondition();
                    if( pszCond == NULL || IsConditionMatched(pszCond, attr) )
                    {
                        m_poReader->SetFeaturePropertyDirectly( NULL, CPLStrdup(""), i );
                    }
                }
                else
                    CPLFree(pszAttrVal);
            }
        }
    }
#endif
}

/************************************************************************/
/*                        IsConditionMatched()                          */
/************************************************************************/

/* FIXME! 'and' / 'or' operators are evaluated left to right, without */
/* and precedence rules between them ! */

int GMLHandler::IsConditionMatched(const char* pszCondition, void* attr)
{
    if( pszCondition == NULL )
        return TRUE;

    int bSyntaxError = FALSE;
    CPLString osCondAttr, osCondVal;
    const char* pszIter = pszCondition;
    int bOpEqual = TRUE;
    while( *pszIter == ' ' )
        pszIter ++;
    if( *pszIter != '@' )
        bSyntaxError = TRUE;
    else
    {
        pszIter++;
        while( *pszIter != '\0' &&
               *pszIter != ' ' &&
               *pszIter != '!' &&
               *pszIter != '=' )
        {
            osCondAttr += *pszIter;
            pszIter++;
        }
        while( *pszIter == ' ' )
            pszIter ++;

        if( *pszIter == '!' )
        {
            bOpEqual = FALSE;
            pszIter ++;
        }

        if( *pszIter != '=' )
            bSyntaxError = TRUE;
        else
        {
            pszIter ++;
            while( *pszIter == ' ' )
                pszIter ++;
            if( *pszIter != '\'' )
                bSyntaxError = TRUE;
            else
            {
                pszIter ++;
                while( *pszIter != '\0' &&
                       *pszIter != '\'' )
                {
                    osCondVal += *pszIter;
                    pszIter++;
                }
                if( *pszIter != '\'' )
                    bSyntaxError = TRUE;
                else
                {
                    pszIter ++;
                    while( *pszIter == ' ' )
                        pszIter ++;
                }
            }
        }
    }

    if( bSyntaxError )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid condition : %s. Must be of the form @attrname[!]='attrvalue' [and|or other_cond]*. 'and' and 'or' operators cannot be mixed",
                 pszCondition);
        return FALSE;
    }

    char* pszVal = GetAttributeValue(attr, osCondAttr);
    if( pszVal == NULL )
        pszVal = CPLStrdup("");
    int bCondMet = ((bOpEqual && strcmp(pszVal, osCondVal) == 0 ) ||
                    (!bOpEqual && strcmp(pszVal, osCondVal) != 0 ));
    CPLFree(pszVal);
    if( *pszIter == '\0' )
        return bCondMet;

    if( strncmp(pszIter, "and", 3) == 0 )
    {
        pszIter += 3;
        if( !bCondMet )
            return FALSE;
        return IsConditionMatched(pszIter, attr);
    }

    if( strncmp(pszIter, "or", 2) == 0 )
    {
        pszIter += 2;
        if( bCondMet )
            return TRUE;
        return IsConditionMatched(pszIter, attr);
    }

    CPLError(CE_Failure, CPLE_NotSupported,
                "Invalid condition : %s. Must be of the form @attrname[!]='attrvalue' [and|or other_cond]*. 'and' and 'or' operators cannot be mixed",
                pszCondition);
    return FALSE;
}

/************************************************************************/
/*                FindRealPropertyByCheckingConditions()                */
/************************************************************************/

int GMLHandler::FindRealPropertyByCheckingConditions(int nIdx, void* attr)
{
    GMLReadState *poState = m_poReader->GetState();
    GMLFeatureClass *poClass = poState->m_poFeature->GetClass();

    GMLPropertyDefn* poProp = poClass->GetProperty(nIdx);
    const char* pszCond = poProp->GetCondition();
    if( pszCond != NULL && !IsConditionMatched(pszCond, attr) )
    {
        /* try other attributes with same source element, but with different */
        /* condition */
        const char* pszSrcElement = poProp->GetSrcElement();
        nIdx = -1;
        for(int i=m_nAttributeIndex+1; i < poClass->GetPropertyCount(); i++ )
        {
            poProp = poClass->GetProperty(i);
            if( strcmp(poProp->GetSrcElement(), pszSrcElement) == 0 )
            {
                pszCond = poProp->GetCondition();
                if( IsConditionMatched(pszCond, attr) )
                {
                    nIdx = i;
                    break;
                }
            }
        }
    }
    return nIdx;
}

/************************************************************************/
/*                      startElementFeatureAttribute()                  */
/************************************************************************/

OGRErr GMLHandler::startElementFeatureAttribute(const char *pszName, int nLenName, void* attr )
{
    /* Reset flag */
    m_bInCurField = FALSE;

    GMLReadState *poState = m_poReader->GetState();

/* -------------------------------------------------------------------- */
/*      If we are collecting geometry, or if we determine this is a     */
/*      geometry element then append to the geometry info.              */
/* -------------------------------------------------------------------- */
    if( IsGeometryElement( pszName ) )
    {
        int bReadGeometry;

        /* If the <GeometryElementPath> is defined in the .gfs, use it */
        /* to read the appropriate geometry element */
        GMLFeatureClass* poClass = poState->m_poFeature->GetClass();
        m_nGeometryPropertyIndex = 0;
        if( poClass->IsSchemaLocked() &&
            poClass->GetGeometryPropertyCount() == 0 )
        {
            bReadGeometry = FALSE;
        }
        else if( poClass->IsSchemaLocked() &&
                 poClass->GetGeometryPropertyCount() == 1 &&
                 poClass->GetGeometryProperty(0)->GetSrcElement()[0] == '\0' )
        {
            bReadGeometry = TRUE;
        }
        else if( poClass->IsSchemaLocked() &&
                 poClass->GetGeometryPropertyCount() > 0 )
        {
            m_nGeometryPropertyIndex = poClass->GetGeometryPropertyIndexBySrcElement( poState->osPath.c_str() );
            bReadGeometry = (m_nGeometryPropertyIndex >= 0);
        }
        else if( m_poReader->FetchAllGeometries() )
        {
            bReadGeometry = TRUE;
        }
        else if( !poClass->IsSchemaLocked() && m_poReader->IsWFSJointLayer() )
        {
            m_nGeometryPropertyIndex = poClass->GetGeometryPropertyIndexBySrcElement( poState->osPath.c_str() );
            if( m_nGeometryPropertyIndex < 0 )
            {
                const char* pszElement = poState->osPath.c_str();
                CPLString osFieldName;
                /* Strip member| prefix. Should always be true normally */
                if( strncmp(pszElement, "member|", strlen("member|")) == 0 )
                    osFieldName = pszElement + strlen("member|");

                /* Replace layer|property by layer_property */
                size_t iPos = osFieldName.find('|');
                if( iPos != std::string::npos )
                    osFieldName[iPos] = '.';

                poClass->AddGeometryProperty( new GMLGeometryPropertyDefn(
                        osFieldName, poState->osPath.c_str(), wkbUnknown, -1, TRUE ) );
                m_nGeometryPropertyIndex = poClass->GetGeometryPropertyCount();
            }
            bReadGeometry = TRUE;
        }
        else
        {
            /* AIXM special case: for RouteSegment, we only want to read Curve geometries */
            /* not 'start' and 'end' geometries */
            if (eAppSchemaType == APPSCHEMA_AIXM &&
                strcmp(poState->m_poFeature->GetClass()->GetName(), "RouteSegment") == 0)
                bReadGeometry = strcmp( pszName, "Curve") == 0;

            /* For Inspire objects : the "main" geometry is in a <geometry> element */
            else if (m_bAlreadyFoundGeometry)
                bReadGeometry = FALSE;
            else if (strcmp( poState->osPath.c_str(), "geometry") == 0)
            {
                m_bAlreadyFoundGeometry = TRUE;
                bReadGeometry = TRUE;
            }

            else
                bReadGeometry = TRUE;
        }
        if (bReadGeometry)
        {
            m_nGeometryDepth = m_nDepth;

            CPLAssert(apsXMLNode.size() == 0);

            NodeLastChild sNodeLastChild;
            sNodeLastChild.psNode = NULL;
            sNodeLastChild.psLastChild = NULL;
            apsXMLNode.push_back(sNodeLastChild);

            PUSH_STATE(STATE_GEOMETRY);

            return startElementGeometry(pszName, nLenName, attr);
        }
    }


    else if( nLenName == 9 && strcmp(pszName, "boundedBy") == 0 )
    {
        m_inBoundedByDepth = m_nDepth;

        PUSH_STATE(STATE_BOUNDED_BY);

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Is it a CityGML generic attribute ?                             */
/* -------------------------------------------------------------------- */
    else if( eAppSchemaType == APPSCHEMA_CITYGML &&
             m_poReader->IsCityGMLGenericAttributeElement( pszName, attr ) )
    {
        CPLFree(m_pszCityGMLGenericAttrName);
        m_pszCityGMLGenericAttrName = GetAttributeValue(attr, "name");
        m_inCityGMLGenericAttrDepth = m_nDepth;

        PUSH_STATE(STATE_CITYGML_ATTRIBUTE);

        return OGRERR_NONE;
    }
    
    else if( m_poReader->IsWFSJointLayer() && m_nDepth == m_nDepthFeature + 1 )
    {
    }

    else if( m_poReader->IsWFSJointLayer() && m_nDepth == m_nDepthFeature + 2 )
    {
        const char* pszFID = GetFID(attr);
        if( pszFID )
        {
            poState->PushPath( pszName, nLenName );
            CPLString osPropPath= poState->osPath + "@id";
            poState->PopPath();
            m_poReader->SetFeaturePropertyDirectly( osPropPath, CPLStrdup(pszFID), -1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      If it is (or at least potentially is) a simple attribute,       */
/*      then start collecting it.                                       */
/* -------------------------------------------------------------------- */
    else if( (m_nAttributeIndex =
                m_poReader->GetAttributeElementIndex( pszName, nLenName )) != -1 )
    {
        GMLFeatureClass *poClass = poState->m_poFeature->GetClass();
        if( poClass->IsSchemaLocked() &&
            (poClass->GetProperty(m_nAttributeIndex)->GetType() == GMLPT_FeatureProperty ||
             poClass->GetProperty(m_nAttributeIndex)->GetType() == GMLPT_FeaturePropertyList) )
        {
            m_nAttributeDepth = m_nDepth;
            PUSH_STATE(STATE_FEATUREPROPERTY);
        }
        else
        {
            /* Is this a property with a condition on an attribute value ? */
            if( poClass->IsSchemaLocked() )
            {
                m_nAttributeIndex = FindRealPropertyByCheckingConditions( m_nAttributeIndex, attr );
            }

            if( m_nAttributeIndex >= 0 )
            {
                if(m_pszCurField)
                {
                    CPLFree(m_pszCurField);
                    m_pszCurField = NULL;
                    m_nCurFieldLen = m_nCurFieldAlloc = 0;
                }
                m_bInCurField = TRUE;

                DealWithAttributes(pszName, nLenName, attr);

                if (stateStack[nStackDepth] != STATE_PROPERTY)
                {
                    m_nAttributeDepth = m_nDepth;
                    PUSH_STATE(STATE_PROPERTY);
                }
            }
            /*else
            {
                DealWithAttributes(pszName, nLenName, attr);
            }*/
        }

    }
    else
    {
        DealWithAttributes(pszName, nLenName, attr);
    }

    poState->PushPath( pszName, nLenName );

    return OGRERR_NONE;
}

/************************************************************************/
/*                         startElementTop()                            */
/************************************************************************/

OGRErr GMLHandler::startElementTop(const char *pszName,
                                   CPL_UNUSED int nLenName,
                                   void* attr )
{
    if (strcmp(pszName, "CityModel") == 0 )
    {
        eAppSchemaType = APPSCHEMA_CITYGML;
    }
    else if (strcmp(pszName, "AIXMBasicMessage") == 0)
    {
        eAppSchemaType = APPSCHEMA_AIXM;
        m_bReportHref = TRUE;
    }
    else if (strcmp(pszName, "Maastotiedot") == 0)
    {
        eAppSchemaType = APPSCHEMA_MTKGML;

        char *pszSRSName = GetAttributeValue(attr, "srsName");
        m_poReader->SetGlobalSRSName(pszSRSName);
        CPLFree(pszSRSName);

        m_bReportHref = TRUE;

        /* the schemas of MTKGML don't have (string) width, so don't set it */
        m_poReader->SetWidthFlag(FALSE);
    }

    stateStack[0] = STATE_DEFAULT;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        startElementDefault()                         */
/************************************************************************/

OGRErr GMLHandler::startElementDefault(const char *pszName, int nLenName, void* attr )

{
/* -------------------------------------------------------------------- */
/*      Is it a feature?  If so push a whole new state, and return.     */
/* -------------------------------------------------------------------- */
    int nClassIndex;
    const char* pszFilteredClassName;

    if( nLenName == 9 && strcmp(pszName, "boundedBy") == 0 )
    {
        m_inBoundedByDepth = m_nDepth;

        PUSH_STATE(STATE_BOUNDED_BY);

        return OGRERR_NONE;
    }

    else if( m_poReader->ShouldLookForClassAtAnyLevel() &&
        ( pszFilteredClassName = m_poReader->GetFilteredClassName() ) != NULL )
    {
        if( strcmp(pszName, pszFilteredClassName) == 0 )
        {
            m_poReader->PushFeature( pszName, GetFID(attr), m_poReader->GetFilteredClassIndex() );

            m_nDepthFeature = m_nDepth;

            PUSH_STATE(STATE_FEATURE);

            return OGRERR_NONE;
        }
    }

    /* WFS 2.0 GetFeature documents have a wfs:FeatureCollection */
    /* as a wfs:member of the top wfs:FeatureCollection. We don't want this */
    /* wfs:FeatureCollection to be recognized as a feature */
    else if( (!(nLenName == strlen("FeatureCollection") &&
                strcmp(pszName, "FeatureCollection") == 0)) &&
             (nClassIndex = m_poReader->GetFeatureElementIndex( pszName, nLenName, eAppSchemaType )) != -1 )
    {
        m_bAlreadyFoundGeometry = FALSE;

        pszFilteredClassName = m_poReader->GetFilteredClassName();
        if ( pszFilteredClassName != NULL &&
             strcmp(pszName, pszFilteredClassName) != 0 )
        {
            m_nDepthFeature = m_nDepth;

            PUSH_STATE(STATE_IGNORED_FEATURE);

            return OGRERR_NONE;
        }
        else
        {
            if( eAppSchemaType == APPSCHEMA_MTKGML )
            {
                m_poReader->PushFeature( pszName, NULL, nClassIndex );

                char* pszGID = GetAttributeValue(attr, "gid");
                if( pszGID )
                    m_poReader->SetFeaturePropertyDirectly( "gid", pszGID, -1, GMLPT_String );
            }
            else
                m_poReader->PushFeature( pszName, GetFID(attr), nClassIndex );

            m_nDepthFeature = m_nDepth;

            PUSH_STATE(STATE_FEATURE);

            return OGRERR_NONE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Push the element onto the current state's path.                 */
/* -------------------------------------------------------------------- */
    m_poReader->GetState()->PushPath( pszName, nLenName );

    return OGRERR_NONE;
}

/************************************************************************/
/*                      endElementIgnoredFeature()                      */
/************************************************************************/

OGRErr GMLHandler::endElementIgnoredFeature()

{
    if (m_nDepth == m_nDepthFeature)
    {
        POP_STATE();
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                         endElementBoundedBy()                        */
/************************************************************************/
OGRErr GMLHandler::endElementBoundedBy()

{
    if( m_inBoundedByDepth == m_nDepth)
    {
        POP_STATE();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       ParseAIXMElevationPoint()                      */
/************************************************************************/

CPLXMLNode* GMLHandler::ParseAIXMElevationPoint(CPLXMLNode *psGML)
{
    const char* pszElevation =
        CPLGetXMLValue( psGML, "elevation", NULL );
    if (pszElevation)
    {
        m_poReader->SetFeaturePropertyDirectly( "elevation",
                                        CPLStrdup(pszElevation), -1 );
        const char* pszElevationUnit =
            CPLGetXMLValue( psGML, "elevation.uom", NULL );
        if (pszElevationUnit)
        {
            m_poReader->SetFeaturePropertyDirectly( "elevation_uom",
                                            CPLStrdup(pszElevationUnit), -1 );
        }
    }

    const char* pszGeoidUndulation =
        CPLGetXMLValue( psGML, "geoidUndulation", NULL );
    if (pszGeoidUndulation)
    {
        m_poReader->SetFeaturePropertyDirectly( "geoidUndulation",
                                        CPLStrdup(pszGeoidUndulation), -1 );
        const char* pszGeoidUndulationUnit =
            CPLGetXMLValue( psGML, "geoidUndulation.uom", NULL );
        if (pszGeoidUndulationUnit)
        {
            m_poReader->SetFeaturePropertyDirectly( "geoidUndulation_uom",
                                            CPLStrdup(pszGeoidUndulationUnit), -1 );
        }
    }

    const char* pszPos =
                    CPLGetXMLValue( psGML, "pos", NULL );
    const char* pszCoordinates =
                CPLGetXMLValue( psGML, "coordinates", NULL );
    if (pszPos != NULL)
    {
        char* pszGeometry = CPLStrdup(CPLSPrintf(
            "<gml:Point><gml:pos>%s</gml:pos></gml:Point>",
                                                    pszPos));
        CPLDestroyXMLNode(psGML);
        psGML = CPLParseXMLString(pszGeometry);
        CPLFree(pszGeometry);
    }
    else if (pszCoordinates != NULL)
    {
        char* pszGeometry = CPLStrdup(CPLSPrintf(
            "<gml:Point><gml:coordinates>%s</gml:coordinates></gml:Point>",
                                            pszCoordinates));
        CPLDestroyXMLNode(psGML);
        psGML = CPLParseXMLString(pszGeometry);
        CPLFree(pszGeometry);
    }
    else
    {
        CPLDestroyXMLNode(psGML);
        psGML = NULL;
    }

    return psGML;
}

/************************************************************************/
/*                         endElementGeometry()                         */
/************************************************************************/
OGRErr GMLHandler::endElementGeometry()

{
    if (m_nGeomLen)
    {
        CPLXMLNode* psNode = (CPLXMLNode *) CPLCalloc(sizeof(CPLXMLNode),1);
        psNode->eType = CXT_Text;
        psNode->pszValue = m_pszGeometry;

        NodeLastChild& sNodeLastChild = apsXMLNode[apsXMLNode.size()-1];
        CPLXMLNode* psLastChildParent = sNodeLastChild.psLastChild;
        if (psLastChildParent == NULL)
        {
            CPLXMLNode* psParent = sNodeLastChild.psNode;
            if (psParent)
                psParent->psChild = psNode;
        }
        else
            psLastChildParent->psNext = psNode;
        sNodeLastChild.psLastChild = psNode;

        m_pszGeometry = NULL;
        m_nGeomAlloc = 0;
        m_nGeomLen = 0;
    }

    if( m_nDepth == m_nGeometryDepth )
    {
        CPLXMLNode* psInterestNode = apsXMLNode[apsXMLNode.size()-1].psNode;

        /*char* pszXML = CPLSerializeXMLTree(psInterestNode);
        CPLDebug("GML", "geometry = %s", pszXML);
        CPLFree(pszXML);*/

        apsXMLNode.pop_back();

        /* AIXM ElevatedPoint. We want to parse this */
        /* a bit specially because ElevatedPoint is aixm: stuff and */
        /* the srsDimension of the <gml:pos> can be set to TRUE although */
        /* they are only 2 coordinates in practice */
        if ( eAppSchemaType == APPSCHEMA_AIXM && psInterestNode != NULL &&
            strcmp(psInterestNode->pszValue, "ElevatedPoint") == 0 )
        {
            psInterestNode = ParseAIXMElevationPoint(psInterestNode);
        }
        else if ( eAppSchemaType == APPSCHEMA_MTKGML && psInterestNode != NULL )
        {
            if( strcmp(psInterestNode->pszValue, "Murtoviiva") == 0 )
            {
                CPLFree(psInterestNode->pszValue);
                psInterestNode->pszValue = CPLStrdup("gml:LineString");
            }
            else if( strcmp(psInterestNode->pszValue, "Alue") == 0 )
            {
                CPLFree(psInterestNode->pszValue);
                psInterestNode->pszValue = CPLStrdup("gml:Polygon");
            }
            else if( strcmp(psInterestNode->pszValue, "Piste") == 0 )
            {
                CPLFree(psInterestNode->pszValue);
                psInterestNode->pszValue = CPLStrdup("gml:Point");
            }
        }
        else if( psInterestNode != NULL &&
                 strcmp(psInterestNode->pszValue, "BoundingBox") == 0 )
        {
            CPLFree(psInterestNode->pszValue);
            psInterestNode->pszValue = CPLStrdup("Envelope");
            for( CPLXMLNode* psChild = psInterestNode->psChild;
                 psChild;
                 psChild = psChild->psNext )
            {
                if( psChild->eType == CXT_Attribute &&
                    strcmp(psChild->pszValue, "crs") == 0 )
                {
                    CPLFree(psChild->pszValue);
                    psChild->pszValue = CPLStrdup("srsName");
                    break;
                }
            }
        }

        GMLFeature* poGMLFeature = m_poReader->GetState()->m_poFeature;
        if (m_poReader->FetchAllGeometries())
            poGMLFeature->AddGeometry(psInterestNode);
        else
        {
            GMLFeatureClass* poClass = poGMLFeature->GetClass();
            if( poClass->GetGeometryPropertyCount() > 1 )
            {
                poGMLFeature->SetGeometryDirectly(
                    m_nGeometryPropertyIndex, psInterestNode);
            }
            else
            {
                poGMLFeature->SetGeometryDirectly(psInterestNode);
            }
        }

        POP_STATE();
    }

    apsXMLNode.pop_back();

    return OGRERR_NONE;
}

/************************************************************************/
/*                    endElementCityGMLGenericAttr()                    */
/************************************************************************/
OGRErr GMLHandler::endElementCityGMLGenericAttr()

{
    if( m_pszCityGMLGenericAttrName != NULL && m_bInCurField )
    {
        if( m_pszCurField != NULL )
        {
            m_poReader->SetFeaturePropertyDirectly( m_pszCityGMLGenericAttrName,
                                            m_pszCurField, -1 );
        }
        m_pszCurField = NULL;
        m_nCurFieldLen = m_nCurFieldAlloc = 0;
        m_bInCurField = FALSE;
        CPLFree(m_pszCityGMLGenericAttrName);
        m_pszCityGMLGenericAttrName = NULL;
    }

    if( m_inCityGMLGenericAttrDepth == m_nDepth )
    {
        POP_STATE();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        endElementAttribute()                         */
/************************************************************************/
OGRErr GMLHandler::endElementAttribute()

{
    GMLReadState *poState = m_poReader->GetState();

    if (m_bInCurField)
    {
        if (m_pszCurField == NULL)
        {
            if (m_pszValue != NULL)
            {
                m_poReader->SetFeaturePropertyDirectly( poState->osPath.c_str(),
                                                m_pszValue, -1 );
                m_pszValue = NULL;
            }
        }
        else
        {
            m_poReader->SetFeaturePropertyDirectly( poState->osPath.c_str(),
                                            m_pszCurField,
                                            m_nAttributeIndex );
            m_pszCurField = NULL;
        }

        if (m_pszHref != NULL)
        {
            CPLString osPropNameHref = poState->osPath + "_href";
            m_poReader->SetFeaturePropertyDirectly( osPropNameHref, m_pszHref, -1 );
            m_pszHref = NULL;
        }

        if (m_pszUom != NULL)
        {
            CPLString osPropNameUom = poState->osPath + "_uom";
            m_poReader->SetFeaturePropertyDirectly( osPropNameUom, m_pszUom, -1 );
            m_pszUom = NULL;
        }

        if (m_pszKieli != NULL)
        {
            CPLString osPropName = poState->osPath + "_kieli";
            m_poReader->SetFeaturePropertyDirectly( osPropName, m_pszKieli, -1 );
            m_pszKieli = NULL;
        }

        m_nCurFieldLen = m_nCurFieldAlloc = 0;
        m_bInCurField = FALSE;
        m_nAttributeIndex = -1;

        CPLFree( m_pszValue );
        m_pszValue = NULL;
    }

    poState->PopPath();

    if( m_nAttributeDepth == m_nDepth )
    {
        POP_STATE();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                    startElementFeatureProperty()                     */
/************************************************************************/

OGRErr GMLHandler::startElementFeatureProperty(CPL_UNUSED const char *pszName,
                                               CPL_UNUSED int nLenName,
                                               void* attr )
{
    if (m_nDepth == m_nAttributeDepth + 1)
    {
        const char* pszGMLId = GetFID(attr);
        if( pszGMLId != NULL )
        {
            m_poReader->SetFeaturePropertyDirectly( NULL,
                                                    CPLStrdup(CPLSPrintf("#%s", pszGMLId)),
                                                    m_nAttributeIndex );
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                      endElementFeatureProperty()                      */
/************************************************************************/

OGRErr GMLHandler::endElementFeatureProperty()

{
    if (m_nDepth == m_nAttributeDepth)
    {
        GMLReadState *poState = m_poReader->GetState();
        poState->PopPath();

        POP_STATE();
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                          endElementFeature()                         */
/************************************************************************/
OGRErr GMLHandler::endElementFeature()

{
/* -------------------------------------------------------------------- */
/*      If we are collecting a feature, and this element tag matches    */
/*      element name for the class, then we have finished the           */
/*      feature, and we pop the feature read state.                     */
/* -------------------------------------------------------------------- */
    if( m_nDepth == m_nDepthFeature )
    {
        m_poReader->PopState();

        POP_STATE();
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, we just pop the element off the local read states    */
/*      element stack.                                                  */
/* -------------------------------------------------------------------- */
    else
    {
        m_poReader->GetState()->PopPath();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          endElementDefault()                         */
/************************************************************************/
OGRErr GMLHandler::endElementDefault()

{
    if (m_nDepth > 0)
        m_poReader->GetState()->PopPath();

    return OGRERR_NONE;
}

/************************************************************************/
/*                         dataHandlerAttribute()                       */
/************************************************************************/

OGRErr GMLHandler::dataHandlerAttribute(const char *data, int nLen)

{
    int nIter = 0;

    if( m_bInCurField )
    {
        // Ignore white space
        if (m_nCurFieldLen == 0)
        {
            while (nIter < nLen)
            {
                char ch = data[nIter];
                if( !(ch == ' ' || ch == 10 || ch == 13 || ch == '\t') )
                    break;
                nIter ++;
            }
        }

        int nCharsLen = nLen - nIter;

        if (m_nCurFieldLen + nCharsLen + 1 > m_nCurFieldAlloc)
        {
            m_nCurFieldAlloc = m_nCurFieldAlloc * 4 / 3 + nCharsLen + 1;
            char *pszNewCurField = (char *)
                VSIRealloc( m_pszCurField, m_nCurFieldAlloc );
            if (pszNewCurField == NULL)
            {
                return OGRERR_NOT_ENOUGH_MEMORY;
            }
            m_pszCurField = pszNewCurField;
        }
        memcpy( m_pszCurField + m_nCurFieldLen, data + nIter, nCharsLen);
        m_nCurFieldLen += nCharsLen;
        m_pszCurField[m_nCurFieldLen] = '\0';
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         dataHandlerGeometry()                        */
/************************************************************************/

OGRErr GMLHandler::dataHandlerGeometry(const char *data, int nLen)

{
    int nIter = 0;

    // Ignore white space
    if (m_nGeomLen == 0)
    {
        while (nIter < nLen)
        {
            char ch = data[nIter];
            if( !(ch == ' ' || ch == 10 || ch == 13 || ch == '\t') )
                break;
            nIter ++;
        }
    }

    int nCharsLen = nLen - nIter;
    if (nCharsLen)
    {
        if( m_nGeomLen + nCharsLen + 1 > m_nGeomAlloc )
        {
            m_nGeomAlloc = m_nGeomAlloc * 4 / 3 + nCharsLen + 1;
            char* pszNewGeometry = (char *)
                VSIRealloc( m_pszGeometry, m_nGeomAlloc);
            if (pszNewGeometry == NULL)
            {
                return OGRERR_NOT_ENOUGH_MEMORY;
            }
            m_pszGeometry = pszNewGeometry;
        }
        memcpy( m_pszGeometry+m_nGeomLen, data + nIter, nCharsLen);
        m_nGeomLen += nCharsLen;
        m_pszGeometry[m_nGeomLen] = '\0';
    }

    return OGRERR_NONE;
}


/************************************************************************/
/*                         IsGeometryElement()                          */
/************************************************************************/

int GMLHandler::IsGeometryElement( const char *pszElement )

{
    int nFirst = 0;
    int nLast = GML_GEOMETRY_TYPE_COUNT- 1;
    unsigned long nHash = CPLHashSetHashStr(pszElement);
    do
    {
        int nMiddle = (nFirst + nLast) / 2;
        if (nHash == pasGeometryNames[nMiddle].nHash)
            return strcmp(pszElement, pasGeometryNames[nMiddle].pszName) == 0;
        if (nHash < pasGeometryNames[nMiddle].nHash)
            nLast = nMiddle - 1;
        else
            nFirst = nMiddle + 1;
    } while(nFirst <= nLast);

    if (eAppSchemaType == APPSCHEMA_AIXM &&
        strcmp( pszElement, "ElevatedPoint") == 0)
        return TRUE;

    if( eAppSchemaType == APPSCHEMA_MTKGML &&
        ( strcmp( pszElement, "Piste") == 0 ||
          strcmp( pszElement, "Alue") == 0  ||
          strcmp( pszElement, "Murtoviiva") == 0 ) )
        return TRUE;

    return FALSE;
}
