/**********************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLHandler class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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

#if HAVE_XERCES == 1

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

void GMLXercesHandler::startElement(const XMLCh* const    uri,
                                    const XMLCh* const    localname,
                                    const XMLCh* const    qname,
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

    if (GMLHandler::startElement(szElementName, (void*) &attrs) == CE_Failure)
    {
        throw SAXNotSupportedException("Out of memory");
    }
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
void GMLXercesHandler::endElement(const   XMLCh* const    uri,
                                  const   XMLCh* const    localname,
                                  const   XMLCh* const    qname )

{
    char        szElementName[MAX_TOKEN_SIZE];

    m_nEntityCounter = 0;

    /* A XMLCh character can expand to 4 bytes in UTF-8 */
    if (4 * tr_strlen( localname ) >= MAX_TOKEN_SIZE)
    {
        XMLCh* tempBuffer = (XMLCh*) CPLMalloc(sizeof(XMLCh) * (MAX_TOKEN_SIZE / 4 + 1));
        memcpy(tempBuffer, localname, sizeof(XMLCh) * (MAX_TOKEN_SIZE / 4));
        tempBuffer[MAX_TOKEN_SIZE / 4] = 0;
        tr_strcpy( szElementName, tempBuffer );
        CPLFree(tempBuffer);
    }
    else
        tr_strcpy( szElementName, localname );

    if (GMLHandler::endElement(szElementName) == CE_Failure)
    {
        throw SAXNotSupportedException("Out of memory");
    }
}

#if XERCES_VERSION_MAJOR >= 3
/************************************************************************/
/*                             characters() (xerces 3 version)          */
/************************************************************************/

void GMLXercesHandler::characters(const XMLCh* const chars_in,
                                  const XMLSize_t length )
{
    char* utf8String = tr_strdup(chars_in);
    int nLen = strlen(utf8String);
    OGRErr eErr = GMLHandler::dataHandler(utf8String, nLen);
    CPLFree(utf8String);
    if (eErr == CE_Failure)
    {
        throw SAXNotSupportedException("Out of memory");
    }
}

#else
/************************************************************************/
/*                             characters() (xerces 2 version)          */
/************************************************************************/

void GMLXercesHandler::characters(const XMLCh* const chars_in,
                                  const unsigned int length )

{
    char* utf8String = tr_strdup(chars_in);
    int nLen = strlen(utf8String);
    OGRErr eErr = GMLHandler::dataHandler(utf8String, nLen);
    CPLFree(utf8String);
    if (eErr == CE_Failure)
    {
        throw SAXNotSupportedException("Out of memory");
    }
}
#endif

/************************************************************************/
/*                             fatalError()                             */
/************************************************************************/

void GMLXercesHandler::fatalError( const SAXParseException &exception)

{
    char *pszErrorMessage;

    pszErrorMessage = tr_strdup( exception.getMessage() );
    CPLError( CE_Failure, CPLE_AppDefined, 
              "XML Parsing Error: %s\n", 
              pszErrorMessage );

    CPLFree( pszErrorMessage );
}

/************************************************************************/
/*                             startEntity()                            */
/************************************************************************/

void GMLXercesHandler::startEntity (const XMLCh *const name)
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

char* GMLXercesHandler::GetFID(void* attr)
{
    const Attributes* attrs = (const Attributes*) attr;
    int nFIDIndex;
    XMLCh   anFID[100];

    tr_strcpy( anFID, "fid" );
    nFIDIndex = attrs->getIndex( anFID );
    if( nFIDIndex != -1 )
        return tr_strdup( attrs->getValue( nFIDIndex ) );

    return NULL;
}

/************************************************************************/
/*                        GetAttributes()                               */
/************************************************************************/

char* GMLXercesHandler::GetAttributes(void* attr)
{
    const Attributes* attrs = (const Attributes*) attr;
    CPLString osRes;
    char *pszString;

    for(unsigned int i=0; i < attrs->getLength(); i++)
    {
        osRes += " ";
        pszString = tr_strdup(attrs->getQName(i));
        osRes += pszString;
        CPLFree( pszString );
        osRes += "=\"";
        pszString = tr_strdup(attrs->getValue(i));
        osRes += pszString;
        CPLFree( pszString );
        osRes += "\"";
    }
    return CPLStrdup(osRes);
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

#else


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
/*                            startElement()                            */
/************************************************************************/

OGRErr GMLExpatHandler::startElement(const char *pszName, void* attr )

{
    if (m_bStopParsing)
        return CE_Failure;

    const char* pszColon = strchr(pszName, ':');
    if (pszColon)
        pszName = pszColon + 1;

    if (GMLHandler::startElement(pszName, attr) == CE_Failure)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        m_bStopParsing = TRUE;
        XML_StopParser(m_oParser, XML_FALSE);
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
OGRErr GMLExpatHandler::endElement(const char* pszName )

{
    if (m_bStopParsing)
        return CE_Failure;

    const char* pszColon = strchr(pszName, ':');
    if (pszColon)
        pszName = pszColon + 1;

    if (GMLHandler::endElement(pszName) == CE_Failure)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        m_bStopParsing = TRUE;
        XML_StopParser(m_oParser, XML_FALSE);
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             characters()                             */
/************************************************************************/

OGRErr GMLExpatHandler::dataHandler(const char *data, int nLen)

{
    if (m_bStopParsing)
        return CE_Failure;

    m_nDataHandlerCounter ++;
    if (m_nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "File probably corrupted (million laugh pattern)");
        m_bStopParsing = TRUE;
        XML_StopParser(m_oParser, XML_FALSE);
        return CE_Failure;
    }

    if (GMLHandler::dataHandler(data, nLen) == CE_Failure)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        m_bStopParsing = TRUE;
        XML_StopParser(m_oParser, XML_FALSE);
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                               GetFID()                               */
/************************************************************************/

char* GMLExpatHandler::GetFID(void* attr)
{
    const char** papszIter = (const char** )attr;
    while(*papszIter)
    {
        if (strcmp(*papszIter, "fid") == 0)
        {
            return CPLStrdup(papszIter[1]);
        }
        
        papszIter += 2;
    }
    return NULL;
}

/************************************************************************/
/*                        GetAttributes()                               */
/************************************************************************/

char* GMLExpatHandler::GetAttributes(void* attr)
{
    const char** papszIter = (const char** )attr;
    CPLString osRes;
    while(*papszIter)
    {
        osRes += " ";
        osRes += *papszIter;
        osRes += "=\"";
        osRes += papszIter[1];
        osRes += "\"";

        papszIter += 2;
    }
    return CPLStrdup( osRes );
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

#endif



/************************************************************************/
/*                            GMLHandler()                              */
/************************************************************************/

GMLHandler::GMLHandler( GMLReader *poReader )

{
    m_poReader = poReader;
    m_pszCurField = NULL;
    m_pszGeometry = NULL;
    m_nGeomAlloc = m_nGeomLen = 0;
    m_nDepthFeature = m_nDepth = 0;
    m_bInBoundedBy = FALSE;
    m_inBoundedByDepth = 0;
    m_bInCityGMLGenericAttr = FALSE;
    m_pszCityGMLGenericAttrName = NULL;
    m_inCityGMLGenericAttrDepth = 0;
    m_bIsCityGML = FALSE;
    m_bReportHref = FALSE;
    m_pszHref = NULL;
}

/************************************************************************/
/*                            ~GMLHandler()                             */
/************************************************************************/

GMLHandler::~GMLHandler()

{
    CPLFree( m_pszCurField );
    CPLFree( m_pszGeometry );
    CPLFree( m_pszCityGMLGenericAttrName );
    CPLFree( m_pszHref );
}


/************************************************************************/
/*                            startElement()                            */
/************************************************************************/

OGRErr GMLHandler::startElement(const char *pszName, void* attr )

{
    GMLReadState *poState = m_poReader->GetState();

    if ( m_nDepth == 0 )
    {
        if (strcmp(pszName, "CityModel") == 0 )
            m_bIsCityGML = TRUE;
        else if (strcmp(pszName, "AIXMBasicMessage") == 0)
            m_bReportHref = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      If we are in the midst of collecting a feature attribute        */
/*      value, then this must be a complex attribute which we don't     */
/*      try to collect for now, so just terminate the field             */
/*      collection.                                                     */
/* -------------------------------------------------------------------- */
    if( m_pszCurField != NULL )
    {
        CPLFree( m_pszCurField );
        m_pszCurField = NULL;
    }

    if ( m_bInCityGMLGenericAttr )
    {
        if( strcmp(pszName, "value") == 0 )
        {
            CPLFree( m_pszCurField );
            m_pszCurField = CPLStrdup("");
        }
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting geometry, or if we determine this is a     */
/*      geometry element then append to the geometry info.              */
/* -------------------------------------------------------------------- */
    else if( m_pszGeometry != NULL 
        || IsGeometryElement( pszName )
        /* AIXM ElevatedPoint */ || strcmp( pszName, "ElevatedPoint") == 0 )
    {
        /* should save attributes too! */

        int bReadGeometry;

        if( m_pszGeometry == NULL )
        {
            /* If the <GeometryElementPath> is defined in the .gfs, use it */
            /* to read the appropriate geometry element */
            const char* pszGeometryElement = (poState->m_poFeature) ?
                    poState->m_poFeature->GetClass()->GetGeometryElement() : NULL;
            if (pszGeometryElement != NULL)
                bReadGeometry = strcmp(poState->m_pszPath, pszGeometryElement) == 0;
            else
                bReadGeometry = TRUE;
            if (bReadGeometry)
                m_nGeometryDepth = poState->m_nPathLength;
        }
        else
            bReadGeometry = TRUE;

        if (bReadGeometry)
        {
            char* pszAttributes = GetAttributes(attr);
            size_t nLNLenBytes = strlen(pszName);

            /* Some CityGML lack a srsDimension="3" in posList, such as in */
            /* http://www.citygml.org/fileadmin/count.php?f=fileadmin%2Fcitygml%2Fdocs%2FFrankfurt_Street_Setting_LOD3.zip */
            /* So we have to add it manually */
            if (m_bIsCityGML && strcmp(pszName, "posList") == 0 &&
                strstr(pszAttributes, "srsDimension") == NULL)
            {
                CPLFree(pszAttributes);
                pszAttributes = CPLStrdup(" srsDimension=\"3\"");
            }

            if( m_nGeomLen + nLNLenBytes + 4 + strlen( pszAttributes ) >
                m_nGeomAlloc )
            {
                m_nGeomAlloc = (size_t) (m_nGeomAlloc * 1.3 + nLNLenBytes + 1000 +
                                    strlen( pszAttributes ));
                char* pszNewGeometry = (char *)
                    VSIRealloc( m_pszGeometry, m_nGeomAlloc);
                if (pszNewGeometry == NULL)
                {
                    CPLFree(pszAttributes);
                    return CE_Failure;
                }
                m_pszGeometry = pszNewGeometry;
            }

            strcpy( m_pszGeometry+m_nGeomLen++, "<" );
            strcpy( m_pszGeometry+m_nGeomLen, pszName );
            m_nGeomLen += nLNLenBytes;
            /* saving attributes */
            strcat( m_pszGeometry + m_nGeomLen, pszAttributes );
            m_nGeomLen += strlen( pszAttributes );
            CPLFree(pszAttributes);
            strcat( m_pszGeometry + (m_nGeomLen++), ">" );
        }
    }

    else if( m_bInBoundedBy)
    {
        ;
    }

/* -------------------------------------------------------------------- */
/*      Is it a feature?  If so push a whole new state, and return.     */
/* -------------------------------------------------------------------- */
    else if( m_nDepthFeature == 0 &&
             m_poReader->IsFeatureElement( pszName ) )
    {
        char* pszFID = GetFID(attr);

        m_poReader->PushFeature( pszName, pszFID);

        CPLFree(pszFID);

        m_nDepthFeature = m_nDepth;
        m_nDepth ++;

        return CE_None;
    }

    else if( strcmp(pszName, "boundedBy") == 0 )
    {
        m_bInBoundedBy = TRUE;
        m_inBoundedByDepth = m_nDepth;
    }

/* -------------------------------------------------------------------- */
/*      Is it a CityGML generic attribute ?                             */
/* -------------------------------------------------------------------- */
    else if( m_poReader->IsCityGMLGenericAttributeElement( pszName, attr ) )
    {
        m_bInCityGMLGenericAttr = TRUE;
        CPLFree(m_pszCityGMLGenericAttrName);
        m_pszCityGMLGenericAttrName = GetAttributeValue(attr, "name");
        m_inCityGMLGenericAttrDepth = m_nDepth;
    }

/* -------------------------------------------------------------------- */
/*      If it is (or at least potentially is) a simple attribute,       */
/*      then start collecting it.                                       */
/* -------------------------------------------------------------------- */
    else if( m_poReader->IsAttributeElement( pszName ) )
    {
        CPLFree( m_pszCurField );
        m_pszCurField = CPLStrdup("");
        if (m_bReportHref)
        {
            CPLFree(m_pszHref);
            m_pszHref = GetAttributeValue(attr, "xlink:href");
        }
    }

/* -------------------------------------------------------------------- */
/*      Push the element onto the current state's path.                 */
/* -------------------------------------------------------------------- */
    poState->PushPath( pszName );

    m_nDepth ++;

    return CE_None;
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
OGRErr GMLHandler::endElement(const char* pszName )

{
    m_nDepth --;

    GMLReadState *poState = m_poReader->GetState();

    if( m_bInBoundedBy && strcmp(pszName, "boundedBy") == 0 &&
        m_inBoundedByDepth == m_nDepth)
    {
        m_bInBoundedBy = FALSE;
    }

    else if( m_bInCityGMLGenericAttr )
    {
        if( m_pszCityGMLGenericAttrName != NULL && m_pszCurField != NULL )
        {
            CPLAssert( poState->m_poFeature != NULL );

            m_poReader->SetFeatureProperty( m_pszCityGMLGenericAttrName, m_pszCurField );
            CPLFree( m_pszCurField );
            m_pszCurField = NULL;
            CPLFree(m_pszCityGMLGenericAttrName);
            m_pszCityGMLGenericAttrName = NULL;
        }

        if( m_inCityGMLGenericAttrDepth == m_nDepth )
        {
            m_bInCityGMLGenericAttr = FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Is this closing off an attribute value?  We assume so if        */
/*      we are collecting an attribute value and got to this point.     */
/*      We don't bother validating that the closing tag matches the     */
/*      opening tag.                                                    */
/* -------------------------------------------------------------------- */
    else if( m_pszCurField != NULL )
    {
        CPLAssert( poState->m_poFeature != NULL );

        if ( m_pszHref != NULL && (m_pszCurField == NULL || EQUAL(m_pszCurField, "")))
        {
            CPLString osPropNameHref = CPLSPrintf("%s_href", poState->m_pszPath);
            m_poReader->SetFeatureProperty( osPropNameHref, m_pszHref );
        }
        else
        {
            m_poReader->SetFeatureProperty( poState->m_pszPath, m_pszCurField );

            if (m_pszHref != NULL)
            {
                CPLString osPropNameHref = CPLSPrintf("%s_href", poState->m_pszPath);
                m_poReader->SetFeatureProperty( osPropNameHref, m_pszHref );
            }
        }
        CPLFree( m_pszCurField );
        m_pszCurField = NULL;
        CPLFree( m_pszHref );
        m_pszHref = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting Geometry than store it, and consider if    */
/*      this is the end of the geometry.                                */
/* -------------------------------------------------------------------- */
    if( m_pszGeometry != NULL )
    {
        /* should save attributes too! */

        size_t nLNLenBytes = strlen(pszName);

        if( m_nGeomLen + nLNLenBytes + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (size_t) (m_nGeomAlloc * 1.3 + nLNLenBytes + 1000);
            char* pszNewGeometry = (char *)
                VSIRealloc( m_pszGeometry, m_nGeomAlloc);
            if (pszNewGeometry == NULL)
            {
                return CE_Failure;
            }
            m_pszGeometry = pszNewGeometry;
        }

        strcat( m_pszGeometry+m_nGeomLen, "</" );
        strcpy( m_pszGeometry+m_nGeomLen+2, pszName );
        strcat( m_pszGeometry+m_nGeomLen+nLNLenBytes+2, ">" );
        m_nGeomLen += nLNLenBytes + 3;

        if( poState->m_nPathLength == m_nGeometryDepth+1 )
        {
            if( poState->m_poFeature != NULL )
            {
                /* AIXM ElevatedPoint. We want to parse this */
                /* a bit specially because ElevatedPoint is aixm: stuff and */
                /* the srsDimension of the <gml:pos> can be set to TRUE although */
                /* they are only 2 coordinates in practice */
                if ( strcmp(pszName, "ElevatedPoint") == 0 )
                {
                    CPLXMLNode *psGML = CPLParseXMLString( m_pszGeometry );
                    if (psGML)
                    {
                        const char* pszElevation =
                            CPLGetXMLValue( psGML, "elevation", NULL );
                        if (pszElevation)
                        {
                            m_poReader->SetFeatureProperty( "elevation",
                                                            pszElevation );
                                const char* pszElevationUnit =
                            CPLGetXMLValue( psGML, "elevation.uom", NULL );
                            if (pszElevationUnit)
                            {
                                m_poReader->SetFeatureProperty( "elevation_uom",
                                                             pszElevationUnit );
                            }
                        }

                        const char* pszPos =
                                        CPLGetXMLValue( psGML, "pos", NULL );
                        const char* pszCoordinates =
                                  CPLGetXMLValue( psGML, "coordinates", NULL );
                        if (pszPos != NULL)
                        {
                            char* pszNewGeometry = CPLStrdup(CPLSPrintf(
                                "<gml:Point><gml:pos>%s</gml:pos></gml:Point>",
                                                                      pszPos));
                            CPLFree(m_pszGeometry);
                            m_pszGeometry = pszNewGeometry;
                        }
                        else if (pszCoordinates != NULL)
                        {
                            char* pszNewGeometry = CPLStrdup(CPLSPrintf(
                                "<gml:Point><gml:coordinates>%s</gml:coordinates></gml:Point>",
                                                              pszCoordinates));
                            CPLFree(m_pszGeometry);
                            m_pszGeometry = pszNewGeometry;
                        }
                        else
                        {
                            CPLFree(m_pszGeometry);
                            m_pszGeometry = NULL;
                        }
                        CPLDestroyXMLNode( psGML );
                    }
                    else
                    {
                        CPLFree(m_pszGeometry);
                        m_pszGeometry = NULL;
                    }
                }
                if (m_pszGeometry)
                {
                    if (m_poReader->FetchAllGeometries())
                        poState->m_poFeature->AddGeometry( m_pszGeometry );
                    else
                        poState->m_poFeature->SetGeometryDirectly( m_pszGeometry );
                }
            }
            else
                CPLFree( m_pszGeometry );

            m_pszGeometry = NULL;
            m_nGeomAlloc = m_nGeomLen = 0;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting a feature, and this element tag matches    */
/*      element name for the class, then we have finished the           */
/*      feature, and we pop the feature read state.                     */
/* -------------------------------------------------------------------- */
    if( m_nDepth == m_nDepthFeature && poState->m_poFeature != NULL
        && strcmp(pszName,
                 poState->m_poFeature->GetClass()->GetElementName()) == 0 )
    {
        m_nDepthFeature = 0;
        m_poReader->PopState();
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, we just pop the element off the local read states    */
/*      element stack.                                                  */
/* -------------------------------------------------------------------- */
    else
    {
        if( strcmp(pszName,poState->GetLastComponent()) == 0 )
            poState->PopPath();
        else
        {
            CPLAssert( FALSE );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             characters()                             */
/************************************************************************/

OGRErr GMLHandler::dataHandler(const char *data, int nLen)

{
    int nIter = 0;

    if( m_pszCurField != NULL )
    {
        int     nCurFieldLength = strlen(m_pszCurField);

        // Ignore white space
        if (nCurFieldLength == 0)
        {
            while (nIter < nLen &&
                   ( data[nIter] == ' ' || data[nIter] == 10 || data[nIter]== 13 || data[nIter] == '\t') )
                nIter ++;
        }

        size_t nCharsLen = nLen - nIter;

        char *pszNewCurField = (char *)
            VSIRealloc( m_pszCurField,
                        nCurFieldLength+ nCharsLen +1 );
        if (pszNewCurField == NULL)
        {
            return CE_Failure;
        }
        m_pszCurField = pszNewCurField;
        memcpy( m_pszCurField + nCurFieldLength, data + nIter, nCharsLen);
        nCurFieldLength += nCharsLen;

        m_pszCurField[nCurFieldLength] = '\0';
    }
    else if( m_pszGeometry != NULL )
    {
        // Ignore white space
        if (m_nGeomLen == 0)
        {
            while (nIter < nLen &&
                   ( data[nIter] == ' ' || data[nIter] == 10 || data[nIter]== 13 || data[nIter] == '\t') )
                nIter ++;
        }

        size_t nCharsLen = nLen - nIter;

        if( m_nGeomLen + nCharsLen + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (size_t) (m_nGeomAlloc * 1.3 + nCharsLen + 1000);
            char* pszNewGeometry = (char *)
                VSIRealloc( m_pszGeometry, m_nGeomAlloc);
            if (pszNewGeometry == NULL)
            {
                return CE_Failure;
            }
            m_pszGeometry = pszNewGeometry;
        }

        memcpy( m_pszGeometry+m_nGeomLen, data + nIter, nCharsLen);
        m_nGeomLen += nCharsLen;
        m_pszGeometry[m_nGeomLen] = '\0';
    }

    return CE_None;
}


/************************************************************************/
/*                         IsGeometryElement()                          */
/************************************************************************/

int GMLHandler::IsGeometryElement( const char *pszElement )

{
    return strcmp(pszElement,"Polygon") == 0
        || strcmp(pszElement,"MultiPolygon") == 0
        || strcmp(pszElement,"MultiPoint") == 0
        || strcmp(pszElement,"MultiLineString") == 0
        || strcmp(pszElement,"MultiSurface") == 0
        || strcmp(pszElement,"MultiGeometry") == 0
        || strcmp(pszElement,"GeometryCollection") == 0 /* OGR < 1.8.0 bug... */
        || strcmp(pszElement,"Point") == 0
        || strcmp(pszElement,"Curve") == 0
        || strcmp(pszElement,"MultiCurve") == 0
        || strcmp(pszElement,"TopoCurve") == 0
        || strcmp(pszElement,"Surface") == 0
        || strcmp(pszElement,"TopoSurface") == 0
        || strcmp(pszElement,"PolygonPatch") == 0
        || strcmp(pszElement,"LineString") == 0
        || strcmp(pszElement,"CompositeSurface") == 0
        || strcmp(pszElement,"Solid") == 0;
}
