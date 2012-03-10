/**********************************************************************
 * $Id$
 *
 * Project:  NAS Reader
 * Purpose:  Implementation of NASHandler class.
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
#include "nasreaderp.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#define MAX_TOKEN_SIZE  1000

/************************************************************************/
/*                             NASHandler()                             */
/************************************************************************/

NASHandler::NASHandler( NASReader *poReader )

{
    m_poReader = poReader;
    m_pszCurField = NULL;
    m_pszGeometry = NULL;
    m_nGeomAlloc = m_nGeomLen = 0;
    m_nDepthFeature = m_nDepth = 0;
    m_bIgnoreFeature = FALSE;
}

/************************************************************************/
/*                            ~NASHandler()                             */
/************************************************************************/

NASHandler::~NASHandler()

{
    CPLFree( m_pszCurField );
    CPLFree( m_pszGeometry );
}

/************************************************************************/
/*                        GetAttributes()                               */
/************************************************************************/

CPLString NASHandler::GetAttributes(const Attributes* attrs)
{
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
    return osRes;
}


/************************************************************************/
/*                            startElement()                            */
/************************************************************************/

void NASHandler::startElement(const XMLCh* const    uri,
                              const XMLCh* const    localname,
                              const XMLCh* const    qname,
                              const Attributes& attrs )

{
    char        szElementName[MAX_TOKEN_SIZE];
    GMLReadState *poState = m_poReader->GetState();
    const char *pszLast = NULL;

    tr_strcpy( szElementName, localname );

    if (m_bIgnoreFeature && m_nDepth >= m_nDepthFeature)
    {
        m_nDepth ++;
        return;
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("NAS",
              "%*sstartElement %s m_bIgnoreFeature:%d depth:%d depthFeature:%d featureClass:%s",
              m_nDepth, "", szElementName,
              m_bIgnoreFeature, m_nDepth, m_nDepthFeature,
              poState->m_poFeature ? poState->m_poFeature->GetClass()->GetElementName() : "(no feature)"
            );
#endif

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

/* -------------------------------------------------------------------- */
/*      If we are collecting geometry, or if we determine this is a     */
/*      geometry element then append to the geometry info.              */
/* -------------------------------------------------------------------- */
    if( m_pszGeometry != NULL 
        || IsGeometryElement( szElementName ) )
    {
        int nLNLen = tr_strlen( localname );
        CPLString osAttributes = GetAttributes( &attrs );

        /* should save attributes too! */

        if( m_pszGeometry == NULL )
            m_nGeometryDepth = poState->m_nPathLength;
        
        if( m_nGeomLen + nLNLen + 4 + (int)osAttributes.size() > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nLNLen + osAttributes.size() + 1000);
            m_pszGeometry = (char *) 
                CPLRealloc( m_pszGeometry, m_nGeomAlloc);
        }

        strcpy( m_pszGeometry+m_nGeomLen, "<" );
        tr_strcpy( m_pszGeometry+m_nGeomLen+1, localname );
        
        if( osAttributes.size() > 0 )
        {
            strcat( m_pszGeometry+m_nGeomLen, " " );
            strcat( m_pszGeometry+m_nGeomLen, osAttributes );
        }

        strcat( m_pszGeometry+m_nGeomLen, ">" );
        m_nGeomLen += strlen(m_pszGeometry+m_nGeomLen);
    }
    
/* -------------------------------------------------------------------- */
/*      Is this the ogc:Filter element in a wfs:Delete or               */
/*      wfsext:Replace operation?  If so we translate it as a           */
/*      specialized sort of feature.                                    */
/* -------------------------------------------------------------------- */
    else if( EQUAL(szElementName,"Filter") 
             && (pszLast = m_poReader->GetState()->GetLastComponent()) != NULL
             && (EQUAL(pszLast,"Delete") || EQUAL(pszLast,"Replace")) )
    {
        const char* pszFilteredClassName = m_poReader->GetFilteredClassName();
        if ( pszFilteredClassName != NULL &&
             strcmp("Delete", pszFilteredClassName) != 0 )
        {
            m_bIgnoreFeature = TRUE;
            m_nDepthFeature = m_nDepth;
            m_nDepth ++;

            return;
        }

        m_bIgnoreFeature = FALSE;

        m_poReader->PushFeature( "Delete", attrs );

        m_nDepthFeature = m_nDepth;
        m_nDepth ++;

        CPLAssert( m_osLastTypeName != "" );
        m_poReader->SetFeaturePropertyDirectly( "typeName", CPLStrdup(m_osLastTypeName) );
        m_poReader->SetFeaturePropertyDirectly( "context", CPLStrdup(pszLast) );

        if( EQUAL( pszLast, "Replace" ) )
        {
            CPLAssert( m_osLastReplacingFID != "" );
            CPLAssert( m_osLastSafeToIgnore != "" );
            m_poReader->SetFeaturePropertyDirectly( "replacedBy", CPLStrdup(m_osLastReplacingFID) );
            m_poReader->SetFeaturePropertyDirectly( "safeToIgnore", CPLStrdup(m_osLastSafeToIgnore) );
        }

        return;
    }

/* -------------------------------------------------------------------- */
/*      Is it a feature?  If so push a whole new state, and return.     */
/* -------------------------------------------------------------------- */
    else if( m_poReader->IsFeatureElement( szElementName ) )
    {
        m_osLastTypeName = szElementName;

        const char* pszFilteredClassName = m_poReader->GetFilteredClassName();

        pszLast = m_poReader->GetState()->GetLastComponent();
        if( pszLast != NULL && EQUAL(pszLast,"Replace") )
        {
            int nIndex;
            XMLCh  Name[100];

            tr_strcpy( Name, "gml:id" );
            nIndex = attrs.getIndex( Name );

            CPLAssert( nIndex!=-1 );
            CPLAssert( m_osLastReplacingFID=="" );

            // Capture "gml:id" attribute as part of the property value -
            // primarily this is for the wfsext:Replace operation's attribute.
            char *pszReplacingFID = tr_strdup( attrs.getValue( nIndex ) );
            m_osLastReplacingFID = pszReplacingFID;
            CPLFree( pszReplacingFID );

#ifdef DEBUG_VERBOSE
	        CPLDebug("NAS", "%*s### Replace typeName=%s replacedBy=%s", m_nDepth, "", m_osLastTypeName.c_str(), m_osLastReplacingFID.c_str() );
#endif
        }

        if ( pszFilteredClassName != NULL &&
             strcmp(szElementName, pszFilteredClassName) != 0 )
        {
            m_bIgnoreFeature = TRUE;
            m_nDepthFeature = m_nDepth;
            m_nDepth ++;

            return;
        }

        m_bIgnoreFeature = FALSE;

        m_poReader->PushFeature( szElementName, attrs );

        m_nDepthFeature = m_nDepth;
        m_nDepth ++;

        return;
    }

/* -------------------------------------------------------------------- */
/*      If it is the wfs:Delete element, then remember the typeName     */
/*      attribute so we can assign it to the feature that will be       */
/*      produced when we process the Filter element.                    */
/* -------------------------------------------------------------------- */
    else if( EQUAL(szElementName,"Delete") )
    {
        int nIndex;
        XMLCh  Name[100];

        tr_strcpy( Name, "typeName" );
        nIndex = attrs.getIndex( Name );
        
        if( nIndex != -1 )
        {
            char *pszTypeName = tr_strdup( attrs.getValue( nIndex ) );
            m_osLastTypeName = pszTypeName;
            CPLFree( pszTypeName );
        }

        m_osLastSafeToIgnore = "";
        m_osLastReplacingFID = "";
    }

/* -------------------------------------------------------------------- */
/*      If it is the wfsext:Replace element, then remember the          */
/*      safeToIgnore attribute so we can assign it to the feature       */
/*      that will be produced when we process the Filter element.       */
/* -------------------------------------------------------------------- */
    else if( EQUAL(szElementName,"Replace") )
    {
        int nIndex;
        XMLCh  Name[100];

        tr_strcpy( Name, "safeToIgnore" );
        nIndex = attrs.getIndex( Name );

        if( nIndex != -1 )
        {
            char *pszSafeToIgnore = tr_strdup( attrs.getValue( nIndex ) );
            m_osLastSafeToIgnore = pszSafeToIgnore;
            CPLFree( pszSafeToIgnore );
        }
        else
        {
	    CPLError( CE_Warning, CPLE_AppDefined, "NAS: safeToIgnore attribute missing" );
            m_osLastSafeToIgnore = "false";
        }

        m_osLastReplacingFID = "";
    }

/* -------------------------------------------------------------------- */
/*      If it is (or at least potentially is) a simple attribute,       */
/*      then start collecting it.                                       */
/* -------------------------------------------------------------------- */
    else if( m_poReader->IsAttributeElement( szElementName ) )
    {
        CPLFree( m_pszCurField );
        m_pszCurField = CPLStrdup("");

        // Capture href as OB property.
        m_poReader->CheckForRelations( szElementName, attrs );

        // Capture "fid" attribute as part of the property value - 
        // primarily this is for wfs:Delete operation's FeatureId attribute.
        if( EQUAL(szElementName,"FeatureId") )
            m_poReader->CheckForFID( attrs, &m_pszCurField );
    }

/* -------------------------------------------------------------------- */
/*      Push the element onto the current state's path.                 */
/* -------------------------------------------------------------------- */
    poState->PushPath( szElementName );

    m_nDepth ++;
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
void NASHandler::endElement(const   XMLCh* const    uri,
                            const   XMLCh* const    localname,
                            const   XMLCh* const    qname )

{
    char        szElementName[MAX_TOKEN_SIZE];
    GMLReadState *poState = m_poReader->GetState();

    tr_strcpy( szElementName, localname );


    m_nDepth --;

    if (m_bIgnoreFeature && m_nDepth >= m_nDepthFeature)
    {
        if (m_nDepth == m_nDepthFeature)
        {
             m_bIgnoreFeature = FALSE;
             m_nDepthFeature = 0;
        }
        return;
    }

#ifdef DEBUG_VERBOSE
   CPLDebug("NAS",
              "%*sendElement %s m_bIgnoreFeature:%d depth:%d depthFeature:%d featureClass:%s",
              m_nDepth, "", szElementName,
              m_bIgnoreFeature, m_nDepth, m_nDepthFeature,
              poState->m_poFeature ? poState->m_poFeature->GetClass()->GetElementName() : "(no feature)"
            );
#endif

/* -------------------------------------------------------------------- */
/*      Is this closing off an attribute value?  We assume so if        */
/*      we are collecting an attribute value and got to this point.     */
/*      We don't bother validating that the closing tag matches the     */
/*      opening tag.                                                    */
/* -------------------------------------------------------------------- */
    if( m_pszCurField != NULL )
    {
        CPLAssert( poState->m_poFeature != NULL );
        
        m_poReader->SetFeaturePropertyDirectly( poState->osPath.c_str(), m_pszCurField );
        m_pszCurField = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting Geometry than store it, and consider if    */
/*      this is the end of the geometry.                                */
/* -------------------------------------------------------------------- */
    if( m_pszGeometry != NULL )
    {
        int nLNLen = tr_strlen( localname );

        /* should save attributes too! */

        if( m_nGeomLen + nLNLen + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nLNLen + 1000);
            m_pszGeometry = (char *) 
                CPLRealloc( m_pszGeometry, m_nGeomAlloc);
        }

        strcat( m_pszGeometry+m_nGeomLen, "</" );
        tr_strcpy( m_pszGeometry+m_nGeomLen+2, localname );
        strcat( m_pszGeometry+m_nGeomLen+nLNLen+2, ">" );
        m_nGeomLen += strlen(m_pszGeometry+m_nGeomLen);

        if( poState->m_nPathLength == m_nGeometryDepth+1 )
        {
            if( poState->m_poFeature != NULL )
            {
                CPLXMLNode* psNode = CPLParseXMLString(m_pszGeometry);
                if (psNode)
                {
                    /* workaround common malformed gml:pos with just a
                     * elevation value instead of a full 3D coordinate:
                     *
                     * <gml:Point gml:id="BII2H">
                     *    <gml:pos srsName="urn:adv:crs:ETRS89_h">41.394</gml:pos>
                     * </gml:Point>
                     *
                     */
                    const char *pszPos;
                    if( (pszPos = CPLGetXMLValue( psNode, "=Point.pos", NULL ) ) != NULL
                        && strstr(pszPos, " ") == NULL )
                    {
                        CPLSetXMLValue( psNode, "pos", CPLSPrintf("0 0 %s", pszPos) );
                    }

                    poState->m_poFeature->SetGeometryDirectly( psNode );
                }
            }

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
        && EQUAL(szElementName,
                 poState->m_poFeature->GetClass()->GetElementName()) )
    {
        m_nDepthFeature = 0;
        m_poReader->PopState();
    }

/* -------------------------------------------------------------------- */
/*      Ends of a wfs:Delete should be triggered on the close of the    */
/*      <Filter> element.                                               */
/* -------------------------------------------------------------------- */
    else if( m_nDepth == m_nDepthFeature 
             && poState->m_poFeature != NULL
             && EQUAL(szElementName,"Filter") 
             && EQUAL(poState->m_poFeature->GetClass()->GetElementName(),
                      "Delete") )
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
        if( EQUAL(szElementName,poState->GetLastComponent()) )
            poState->PopPath();
        else
        {
            CPLAssert( FALSE );
        }
    }
}

/************************************************************************/
/*                             characters()                             */
/************************************************************************/

#if XERCES_VERSION_MAJOR >= 3
void NASHandler::characters( const XMLCh *const chars_in,
                             const XMLSize_t length )
#else
void NASHandler::characters(const XMLCh* const chars_in,
                            const unsigned int length )
#endif

{
    const XMLCh *chars = chars_in;

    if( m_pszCurField != NULL )
    {
        int     nCurFieldLength = strlen(m_pszCurField);

        if (nCurFieldLength == 0)
        {
            // Ignore white space
            while( *chars == ' ' || *chars == 10 || *chars == 13 || *chars == '\t')
                chars++;
        }

        char *pszTranslated = tr_strdup(chars);
        
        if( m_pszCurField == NULL )
        {
            m_pszCurField = pszTranslated;
            nCurFieldLength = strlen(m_pszCurField);
        }
        else
        {
            m_pszCurField = (char *) 
                CPLRealloc( m_pszCurField, 
                            nCurFieldLength+strlen(pszTranslated)+1 );
            strcpy( m_pszCurField + nCurFieldLength, pszTranslated );
            CPLFree( pszTranslated );
        }
    }
    else if( m_pszGeometry != NULL )
    {
        if (m_nGeomLen == 0)
        {
            // Ignore white space
            while( *chars == ' ' || *chars == 10 || *chars == 13 || *chars == '\t')
                chars++;
        }
        
        int nCharsLen = tr_strlen(chars);

        if( m_nGeomLen + nCharsLen*4 + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nCharsLen*4 + 1000);
            m_pszGeometry = (char *) 
                CPLRealloc( m_pszGeometry, m_nGeomAlloc);
        }

        tr_strcpy( m_pszGeometry+m_nGeomLen, chars );
        m_nGeomLen += strlen(m_pszGeometry+m_nGeomLen);
    }
}

/************************************************************************/
/*                             fatalError()                             */
/************************************************************************/

void NASHandler::fatalError( const SAXParseException &exception)

{
    char *pszErrorMessage;

    pszErrorMessage = tr_strdup( exception.getMessage() );
    CPLError( CE_Failure, CPLE_AppDefined, 
              "XML Parsing Error: %s\n", 
              pszErrorMessage );

    CPLFree( pszErrorMessage );
}

/************************************************************************/
/*                         IsGeometryElement()                          */
/************************************************************************/

int NASHandler::IsGeometryElement( const char *pszElement )

{
    return strcmp(pszElement,"Polygon") == 0
        || strcmp(pszElement,"MultiPolygon") == 0 
        || strcmp(pszElement,"MultiPoint") == 0 
        || strcmp(pszElement,"MultiLineString") == 0 
        || strcmp(pszElement,"MultiSurface") == 0 
        || strcmp(pszElement,"GeometryCollection") == 0
        || strcmp(pszElement,"Point") == 0 
        || strcmp(pszElement,"Curve") == 0 
        || strcmp(pszElement,"Surface") == 0 
        || strcmp(pszElement,"PolygonPatch") == 0 
        || strcmp(pszElement,"LineString") == 0;
}
