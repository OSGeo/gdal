/**********************************************************************
 *
 * Project:  NAS Reader
 * Purpose:  Implementation of NASHandler class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_xerces.h"

CPL_CVSID("$Id$")

/*
  Update modes:

GID<7
    <wfs:Transaction version="1.0.0" service="WFS">
        <wfs:Delete typeName="AX_BesondereFlurstuecksgrenze">
            <ogc:Filter>
                <ogc:FeatureId fid="DENW18AL0000nANA20120117T130819Z" />
            </ogc:Filter>
        </wfs:Delete>
        <wfsext:Replace vendorId="AdV" safeToIgnore="false">
            <AP_PTO gml:id="DENW18AL0000pewY20131011T071138Z">
                [...]
            </AP_PTO>
            <ogc:Filter>
                <ogc:FeatureId fid="DENW18AL0000pewY20120117T143330Z" />
            </ogc:Filter>
        </wfsext:Replace>
        <wfs:Update typeName="AX_KommunalesGebiet">
            <wfs:Property>
                <wfs:Name>adv:lebenszeitintervall/adv:AA_Lebenszeitintervall/adv:endet</wfs:Name>
                <wfs:Value>2012-08-14T12:32:30Z</wfs:Value>
            </wfs:Property>
            <wfs:Property>
                <wfs:Name>adv:anlass</wfs:Name>
                <wfs:Value>000000</wfs:Value>
            </wfs:Property>
            <wfs:Property>
                <wfs:Name>adv:anlass</wfs:Name>
                <wfs:Value>010102</wfs:Value>
            </wfs:Property>
            <ogc:Filter>
                <ogc:FeatureId fid="DENW11AL000062WD20111016T122010Z" />
            </ogc:Filter>
        </wfs:Update>
    </wfs:Transaction>

GID>=7
    <wfs:Transaction>
        <wfs:Insert>
            <AX_Flurstueck gml:id="DEBY0000F0000001">
                …
            </AX_Flurstueck>
            <AX_Gebaeude gml:id="DEBY0000G0000001">
                …
            </AX_Gebaeude>
        </wfs:Insert>
        <wfs:Replace>
            <AX_Flurstueck gml:id="DEBY0000F0000002">
                …
            </AX_Flurstueck>
            <fes:Filter>
                <fes:ResourceId rid="DEBY0000F000000220010101T000000Z"/>
            </fes:Filter>
        </wfs:Replace>
        <wfs:Delete typeNames=“AX_Buchungsstelle”>
            <fes:Filter>
                <fes:ResourceId rid="DEBY0000B000000320010101T000000Z"/>
                <fes:ResourceId rid="DEBY0000B000000420010101T000000Z"/>
                …
            </fes:Filter>
        </wfs:Delete>
        <wfs:Update typeNames="adv:AX_Flurstueck">
            <wfs:Property>
                <wfs:ValueReference>adv:lebenszeitintervall/adv:AA_Lebenszeitintervall/adv:endet</wfs:ValueReference>
                    <wfs:Value>2007-11-13T12:00:00Z</wfs:Value>
                </wfs:Property>
            <wfs:Property>
            <wfs:ValueReference>adv:anlass</wfs:ValueReference>
                 <wfs:Value>000000</wfs:Value>
            </wfs:Property>
            <wfs:Property>
                 <wfs:ValueReference>adv:anlass</wfs:ValueReference>
                 <wfs:Value>010102</wfs:Value>
            </wfs:Property>
            <wfs:Filter>
                 <fes:ResourceId rid="DEBY123412345678"/>
            </wfs:Filter>
        </wfs:Update>
    </wfs:Transaction>
*/

/************************************************************************/
/*                             NASHandler()                             */
/************************************************************************/

NASHandler::NASHandler( NASReader *poReader ) :
    m_poReader(poReader),
    m_pszCurField(nullptr),
    m_pszGeometry(nullptr),
    m_nGeomAlloc(0),
    m_nGeomLen(0),
    m_nGeometryDepth(0),
    m_nGeometryPropertyIndex(-1),
    m_nDepth(0),
    m_nDepthFeature(0),
    m_bIgnoreFeature(false),
    m_bInUpdate(false),
    m_bInUpdateProperty(false),
    m_nUpdateOrDeleteDepth(0),
    m_nUpdatePropertyDepth(0),
    m_nNameOrValueDepth(0)
{}

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

    for(unsigned int i=0; i < attrs->getLength(); i++)
    {
        osRes += " ";
        osRes += transcode(attrs->getQName(i), m_osAttrName);
        osRes += "=\"";
        osRes += transcode(attrs->getValue(i), m_osAttrValue);
        osRes += "\"";
    }
    return osRes;
}

/************************************************************************/
/*                            startElement()                            */
/************************************************************************/

void NASHandler::startElement( const XMLCh* const /* uri */,
                               const XMLCh* const localname,
                               const XMLCh* const /* qname */,
                               const Attributes& attrs )

{
    GMLReadState *poState = m_poReader->GetState();

    transcode( localname, m_osElementName );
#ifdef DEBUG_TRACE_ELEMENTS
    for(int k=0;k<m_nDepth;k++)
        printf(" "); /*ok*/
    printf(">%s\n", m_osElementName.c_str()); /*ok*/
#endif

    if ( m_bIgnoreFeature && m_nDepth >= m_nDepthFeature )
    {
        m_nDepth ++;
        return;
    }

#ifdef DEBUG_VERBOSE
    CPLDebug( "NAS",
              "[%d] startElement %s m_bIgnoreFeature:%d m_nDepth:%d "
              "m_nDepthFeature:%d featureClass:%s lastComponent:%s",
              m_nDepth, m_osElementName.c_str(),
              m_bIgnoreFeature, m_nDepth, m_nDepthFeature,
              poState->m_poFeature ? poState->m_poFeature->
                  GetClass()->GetElementName() : "(no feature)",
              m_poReader->GetState()->GetLastComponent()
            );
#endif

/* -------------------------------------------------------------------- */
/*      If we are in the midst of collecting a feature attribute        */
/*      value, then this must be a complex attribute which we don't     */
/*      try to collect for now, so just terminate the field             */
/*      collection.                                                     */
/* -------------------------------------------------------------------- */
    if( m_pszCurField != nullptr )
    {
        CPLFree( m_pszCurField );
        m_pszCurField = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting geometry, or if we determine this is a     */
/*      geometry element then append to the geometry info.              */
/* -------------------------------------------------------------------- */
    const char *pszLast = nullptr;

    if( m_pszGeometry != nullptr
        || IsGeometryElement( m_osElementName ) )
    {
        if( m_nGeometryPropertyIndex == -1 &&
            poState->m_poFeature &&
            poState->m_poFeature->GetClass() )
        {
          GMLFeatureClass* poClass = poState->m_poFeature->GetClass();
          m_nGeometryPropertyIndex = poClass->GetGeometryPropertyIndexBySrcElement( poState->osPath.c_str() );
        }

        const int nLNLen = static_cast<int>(m_osElementName.size());
        CPLString osAttributes = GetAttributes( &attrs );

        /* should save attributes too! */

        if( m_pszGeometry == nullptr )
            m_nGeometryDepth = poState->m_nPathLength;

        if( m_pszGeometry == nullptr ||
            m_nGeomLen + nLNLen + 4 + (int)osAttributes.size() > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nLNLen + osAttributes.size() + 1000);
            m_pszGeometry = (char *)
                CPLRealloc( m_pszGeometry, m_nGeomAlloc);
        }

        strcpy( m_pszGeometry+m_nGeomLen, "<" );
        strcpy( m_pszGeometry+m_nGeomLen+1, m_osElementName );

        if( !osAttributes.empty() )
        {
            strcat( m_pszGeometry+m_nGeomLen, " " );
            strcat( m_pszGeometry+m_nGeomLen, osAttributes );
        }

        strcat( m_pszGeometry+m_nGeomLen, ">" );
        m_nGeomLen += static_cast<int>(strlen(m_pszGeometry+m_nGeomLen));
    }

/* -------------------------------------------------------------------- */
/*      Is this the ogc:Filter element in a update operation            */
/*      (wfs:Delete, wfsext:Replace or wfs:Update)?                     */
/*      specialized sort of feature.                                    */
/*      Issue a "Delete" feature for each ResourceId                    */
/* -------------------------------------------------------------------- */
    else if( m_nDepthFeature == 0
             && (m_osElementName == "Filter" || m_osElementName == "ResourceId")
             && (pszLast = m_poReader->GetState()->GetLastComponent()) != nullptr
             && (EQUAL(pszLast,"Delete") || EQUAL(pszLast,"Replace") ||
                 EQUAL(pszLast,"Update")) )
    {
        const char* pszFilteredClassName = m_poReader->GetFilteredClassName();
        if ( pszFilteredClassName != nullptr &&
             strcmp("Delete", pszFilteredClassName) != 0 )
        {
            m_bIgnoreFeature = true;
            m_nDepthFeature = m_nDepth;
            m_nDepth ++;

            return;
        }

        if( m_osLastTypeName == "" )
        {
            CPLError( CE_Failure, CPLE_AssertionFailed,
                      "m_osLastTypeName == \"\"");

            m_bIgnoreFeature = true;
            m_nDepthFeature = m_nDepth;
            m_nDepth ++;
            return;
        }

        if( EQUAL( pszLast, "Replace" )  &&
            ( m_osLastReplacingFID == "" || m_osLastSafeToIgnore == "" ) )
        {
            CPLError( CE_Failure, CPLE_AssertionFailed,
                      "m_osLastReplacingFID == \"\" || "
                      "m_osLastSafeToIgnore == \"\"" );

            m_bIgnoreFeature = true;
            m_nDepthFeature = m_nDepth;
            m_nDepth ++;
            return;
        }

        if( EQUAL( pszLast, "Update" ) && m_osLastEnded == "" )
        {
            CPLError( CE_Failure, CPLE_AssertionFailed,
                      "m_osLastEnded == \"\"" );

            m_bIgnoreFeature = true;
            m_nDepthFeature = m_nDepth;
            m_nDepth ++;
            return;
        }

        m_bIgnoreFeature = false;

        m_poReader->PushFeature( "Delete", attrs );

        m_nDepthFeature = m_nDepth;
        m_nDepth ++;

        m_poReader->SetFeaturePropertyDirectly(
            "typeName", CPLStrdup(m_osLastTypeName) );
        m_poReader->SetFeaturePropertyDirectly(
            "context", CPLStrdup(pszLast) );

        if( EQUAL( pszLast, "Delete" )
            && m_osElementName == "ResourceId" )
        {
            char *rid = CPLStrdup("");

            m_poReader->CheckForRID( attrs, &rid);

            m_poReader->SetFeaturePropertyDirectly(
                    "FeatureId", rid );
        }

        if( EQUAL( pszLast, "Replace" ) )
        {
            // CPLAssert( m_osLastReplacingFID != "" );
            // CPLAssert( m_osLastSafeToIgnore != "" );
            m_poReader->SetFeaturePropertyDirectly(
                "replacedBy", CPLStrdup(m_osLastReplacingFID) );
            m_poReader->SetFeaturePropertyDirectly(
                "safeToIgnore", CPLStrdup(m_osLastSafeToIgnore) );
        }
        else if( EQUAL( pszLast, "Update" ) )
        {
            m_poReader->SetFeaturePropertyDirectly(
                "endet", CPLStrdup(m_osLastEnded) );

            for( std::list<CPLString>::iterator it = m_LastOccasions.begin();
                 it != m_LastOccasions.end();
                 ++it )
            {
                m_poReader->SetFeaturePropertyDirectly(
                    "anlass", CPLStrdup(*it) );
            }

            m_osLastEnded = "";
            m_LastOccasions.clear();
        }

        return;
    }

/* -------------------------------------------------------------------- */
/*      Is it a feature?  If so push a whole new state, and return.     */
/* -------------------------------------------------------------------- */
    else if( !m_bInUpdateProperty && m_nDepthFeature == 0 &&
             m_poReader->IsFeatureElement( m_osElementName ) )
    {
        m_osLastTypeName = m_osElementName;

        const char* pszFilteredClassName = m_poReader->GetFilteredClassName();

        pszLast = m_poReader->GetState()->GetLastComponent();
        if( pszLast != nullptr && EQUAL(pszLast,"Replace") )
        {
            const XMLCh achFID[] = { 'g', 'm', 'l', ':', 'i', 'd', '\0' };
            int nIndex = attrs.getIndex( achFID );

            if( nIndex == -1 || m_osLastReplacingFID !="" )
            {
                CPLError( CE_Failure, CPLE_AssertionFailed,
                          "nIndex == -1 || m_osLastReplacingFID !=\"\"" );

                m_bIgnoreFeature = true;
                m_nDepthFeature = m_nDepth;
                m_nDepth ++;

                return;
            }

            // Capture "gml:id" attribute as part of the property value -
            // primarily this is for the wfsext:Replace operation's attribute.
            transcode( attrs.getValue( nIndex ), m_osLastReplacingFID );

#ifdef DEBUG_VERBOSE
            CPLDebug( "NAS", "[%d] ### Replace typeName=%s replacedBy=%s",
                      m_nDepth, m_osLastTypeName.c_str(),
                      m_osLastReplacingFID.c_str() );
#endif
        }

        if ( pszFilteredClassName != nullptr &&
             m_osElementName != pszFilteredClassName )
        {
            m_bIgnoreFeature = true;
            m_nDepthFeature = m_nDepth;
            m_nDepth ++;

            return;
        }

        m_bIgnoreFeature = false;

        m_poReader->PushFeature( m_osElementName, attrs );

        m_nDepthFeature = m_nDepth;
        m_nDepth ++;

        return;
    }

/* -------------------------------------------------------------------- */
/*      If it is the wfs:Delete or wfs:Update element, then remember    */
/*      the typeName attribute so we can assign it to the feature that  */
/*      will be produced when we process the Filter element.            */
/* -------------------------------------------------------------------- */
    else if( m_nUpdateOrDeleteDepth == 0 &&
             (m_osElementName == "Delete" || m_osElementName == "Update") )
    {
        const XMLCh Name0[] = { 't', 'y', 'p', 'e', 'N', 'a', 'm', 'e', '\0' };

        int nIndex0 = attrs.getIndex( Name0 );
        if( nIndex0 != -1 )
        {
            transcode( attrs.getValue( nIndex0 ), m_osLastTypeName );
        }
        else
        {
            const XMLCh Name1[] = { 't', 'y', 'p', 'e', 'N', 'a', 'm', 'e', 's', '\0' };

            int nIndex1 = attrs.getIndex( Name1 );
            if( nIndex1 != -1 )
            {
                transcode( attrs.getValue( nIndex1 ), m_osLastTypeName );
            }
        }

        m_osLastSafeToIgnore = "";
        m_osLastReplacingFID = "";

        if( m_osElementName == "Update" )
        {
            m_bInUpdate = true;
        }
        m_nUpdateOrDeleteDepth = m_nDepth;
    }

    else if ( m_nUpdatePropertyDepth  == 0 &&
              m_bInUpdate && m_osElementName == "Property" )
    {
        m_bInUpdateProperty = true;
        m_nUpdatePropertyDepth = m_nDepth;
    }

    else if ( m_nNameOrValueDepth == 0 &&
              m_bInUpdateProperty && ( m_osElementName == "Name" ||
                                       m_osElementName == "Value" ||
                                       m_osElementName == "ValueReference" ) )
    {
        // collect attribute name or value
        CPLFree( m_pszCurField );
        m_pszCurField = CPLStrdup("");
        m_nNameOrValueDepth = m_nDepth;
    }

/* -------------------------------------------------------------------- */
/*      If it is the wfsext:Replace element, then remember the          */
/*      safeToIgnore attribute so we can assign it to the feature       */
/*      that will be produced when we process the Filter element.       */
/* -------------------------------------------------------------------- */
    else if( m_osElementName ==  "Replace" )
    {
        const XMLCh Name[] = { 's', 'a', 'f', 'e', 'T', 'o', 'I', 'g', 'n', 'o', 'r', 'e', '\0' };

        int nIndex = attrs.getIndex( Name );

        if( nIndex != -1 )
        {
            transcode( attrs.getValue( nIndex ), m_osLastSafeToIgnore );
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "NAS: safeToIgnore attribute missing" );
            m_osLastSafeToIgnore = "false";
        }

        m_osLastReplacingFID = "";
    }

/* -------------------------------------------------------------------- */
/*      If it is (or at least potentially is) a simple attribute,       */
/*      then start collecting it.                                       */
/* -------------------------------------------------------------------- */
    else if( m_poReader->IsAttributeElement( m_osElementName ) )
    {
        CPLFree( m_pszCurField );
        m_pszCurField = CPLStrdup("");

        // Capture href as OB property.
        m_poReader->CheckForRelations( m_osElementName, attrs, &m_pszCurField );

        // Capture "fid" attribute as part of the property value -
        // primarily this is for wfs:Delete operation's FeatureId attribute.
        if( m_osElementName == "FeatureId" )
            m_poReader->CheckForFID( attrs, &m_pszCurField );
        else if( m_osElementName == "ResourceId" )
            m_poReader->CheckForRID( attrs, &m_pszCurField );
    }

/* -------------------------------------------------------------------- */
/*      Push the element onto the current state's path.                 */
/* -------------------------------------------------------------------- */
    poState->PushPath( m_osElementName );

    m_nDepth ++;

    if( poState->osPath.size() > 512 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too long path. Stop parsing");
        m_poReader->StopParsing();
    }
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
void NASHandler::endElement( const XMLCh* const /* uri */ ,
                             const XMLCh* const localname,
                             const XMLCh* const /* qname */)

{
    GMLReadState *poState = m_poReader->GetState();

    transcode( localname, m_osElementName );

    m_nDepth --;
#ifdef DEBUG_TRACE_ELEMENTS
    for(int k=0;k<m_nDepth;k++)
        printf(" "); /*ok*/
    printf("<%s\n", m_osElementName.c_str()); /*ok*/
#endif

    if (m_bIgnoreFeature && m_nDepth >= m_nDepthFeature)
    {
        if (m_nDepth == m_nDepthFeature)
        {
            m_bIgnoreFeature = false;
            m_nDepthFeature = 0;
        }
        return;
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("NAS",
              "[%d] endElement %s m_bIgnoreFeature:%d m_nDepth:%d m_nDepthFeature:%d featureClass:%s",
              m_nDepth, m_osElementName.c_str(),
              m_bIgnoreFeature, m_nDepth, m_nDepthFeature,
              poState->m_poFeature ? poState->m_poFeature->GetClass()->GetElementName() : "(no feature)"
            );
#endif

   if( m_bInUpdateProperty )
   {
       if( m_nDepth == m_nNameOrValueDepth &&
           (m_osElementName == "Name" || m_osElementName == "ValueReference") )
       {
           m_osLastPropertyName = m_pszCurField ? m_pszCurField : "";
           CPLFree(m_pszCurField);
           m_pszCurField = nullptr;
           m_nNameOrValueDepth = 0;
       }
       else if( m_osElementName == "Value" && m_nDepth == m_nNameOrValueDepth )
       {
           m_osLastPropertyValue = m_pszCurField ? m_pszCurField : "";
           CPLFree(m_pszCurField);
           m_pszCurField = nullptr;
           m_nNameOrValueDepth = 0;
       }
       else if( m_nDepth == m_nUpdatePropertyDepth && m_osElementName == "Property" )
       {
           if( EQUAL( m_osLastPropertyName, "adv:lebenszeitintervall/adv:AA_Lebenszeitintervall/adv:endet" ) ||
               EQUAL( m_osLastPropertyName, "lebenszeitintervall/AA_Lebenszeitintervall/endet" ) )
           {
               CPLAssert( m_osLastPropertyValue != "" );
               m_osLastEnded = m_osLastPropertyValue;
           }
           else if( EQUAL( m_osLastPropertyName, "adv:anlass" ) ||
                    EQUAL( m_osLastPropertyName, "anlass" ) )
           {
               CPLAssert( m_osLastPropertyValue != "" );
               m_LastOccasions.push_back( m_osLastPropertyValue );
           }
           else
           {
               CPLError( CE_Warning, CPLE_AppDefined,
                         "NAS: Expected property name or value instead of %s",
                         m_osLastPropertyName.c_str() );
           }

           m_osLastPropertyName = "";
           m_osLastPropertyValue = "";
           m_bInUpdateProperty = false;
           m_nUpdatePropertyDepth = 0;
       }

       poState->PopPath();

       return;
   }

   if( m_nUpdateOrDeleteDepth > 0 &&
             (m_osElementName == "Delete" || m_osElementName == "Update") )
   {
        if ( m_bInUpdate && m_osElementName == "Update" )
        {
            m_bInUpdate = false;
        }
        m_nUpdateOrDeleteDepth = 0;
   }

/* -------------------------------------------------------------------- */
/*      Is this closing off an attribute value?  We assume so if        */
/*      we are collecting an attribute value and got to this point.     */
/*      We don't bother validating that the closing tag matches the     */
/*      opening tag.                                                    */
/* -------------------------------------------------------------------- */
    if( m_pszCurField != nullptr )
    {
        CPLAssert( poState->m_poFeature != nullptr );

        // keep using "featureid" for GID 7
        const char *pszPath;
        if( EQUAL( poState->m_poFeature->GetClass()->GetElementName(), "Delete" )
            && poState->osPath == "ResourceId" )
            pszPath = "FeatureId";
        else
            pszPath = poState->osPath.c_str();

        m_poReader->SetFeaturePropertyDirectly( pszPath, m_pszCurField );
        m_pszCurField = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting Geometry than store it, and consider if    */
/*      this is the end of the geometry.                                */
/* -------------------------------------------------------------------- */
    if( m_pszGeometry != nullptr )
    {
        int nLNLen = static_cast<int>(m_osElementName.size());

        /* should save attributes too! */

        if( m_nGeomLen + nLNLen + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nLNLen + 1000);
            m_pszGeometry = (char *)
                CPLRealloc( m_pszGeometry, m_nGeomAlloc);
        }

        strcat( m_pszGeometry+m_nGeomLen, "</" );
        strcpy( m_pszGeometry+m_nGeomLen+2, m_osElementName );
        strcat( m_pszGeometry+m_nGeomLen+nLNLen+2, ">" );
        m_nGeomLen += static_cast<int>(strlen(m_pszGeometry+m_nGeomLen));

        if( poState->m_nPathLength == m_nGeometryDepth+1 )
        {
            if( poState->m_poFeature != nullptr )
            {
                CPLXMLNode* psNode = CPLParseXMLString(m_pszGeometry);
                if (psNode)
                {
                    /* workaround for common malformed gml:pos with just a
                     * elevation value instead of a full 3D coordinate:
                     *
                     * <gml:Point gml:id="BII2H">
                     *    <gml:pos srsName="urn:adv:crs:ETRS89_h">41.394</gml:pos>
                     * </gml:Point>
                     *
                     */
                    const char *pszPos =
                        CPLGetXMLValue( psNode, "=Point.pos", nullptr );
                    if( pszPos != nullptr && strstr(pszPos, " ") == nullptr )
                    {
                        CPLSetXMLValue( psNode, "pos", CPLSPrintf("0 0 %s", pszPos) );
                    }

                    if ( m_nGeometryPropertyIndex >= 0 &&
                         m_nGeometryPropertyIndex < poState->m_poFeature->GetGeometryCount() &&
                         poState->m_poFeature->GetGeometryList()[m_nGeometryPropertyIndex] )
                    {
                        int iId = poState->m_poFeature->GetClass()->GetPropertyIndex( "gml_id" );
                        const GMLProperty *poIdProp = poState->m_poFeature->GetProperty(iId);
#ifdef DEBUG_VERBOSE
                        char *pszOldGeom = CPLSerializeXMLTree( poState->m_poFeature->GetGeometryList()[m_nGeometryPropertyIndex] );

                        CPLDebug("NAS", "Overwriting other geometry (%s; replace:%s; with:%s)",
                                 poIdProp && poIdProp->nSubProperties>0 && poIdProp->papszSubProperties[0] ? poIdProp->papszSubProperties[0] : "(null)",
                                 m_pszGeometry,
                                 pszOldGeom
                                );

                        CPLFree( pszOldGeom );
#else
                        CPLError( CE_Warning, CPLE_AppDefined, "NAS: Overwriting other geometry (%s)",
                                 poIdProp && poIdProp->nSubProperties>0 && poIdProp->papszSubProperties[0] ? poIdProp->papszSubProperties[0] : "(null)" );
#endif
                    }

                    if( m_nGeometryPropertyIndex >= 0 )
                        poState->m_poFeature->SetGeometryDirectly( m_nGeometryPropertyIndex, psNode );

                    // no geometry property or property without element path
                    else if( poState->m_poFeature->GetClass()->GetGeometryPropertyCount() == 0 ||
                             ( poState->m_poFeature->GetClass()->GetGeometryPropertyCount() == 1 &&
                               poState->m_poFeature->GetClass()->GetGeometryProperty(0)->GetSrcElement() &&
                               *poState->m_poFeature->GetClass()->GetGeometryProperty(0)->GetSrcElement() == 0 ) )
                        poState->m_poFeature->SetGeometryDirectly( psNode );

                    else
                    {
                        CPLError( CE_Warning, CPLE_AppDefined, "NAS: Unexpected geometry skipped (class:%s path:%s geom:%s)",
                                  poState->m_poFeature->GetClass()->GetName(),
                                  poState->osPath.c_str(),
                                  m_pszGeometry );
                        CPLDestroyXMLNode( psNode );
                    }
                }
                else
                    CPLError( CE_Warning, CPLE_AppDefined, "NAS: Invalid geometry skipped" );
            }
            else
                CPLError( CE_Warning, CPLE_AppDefined, "NAS: Skipping geometry without feature" );

            CPLFree( m_pszGeometry );
            m_pszGeometry = nullptr;
            m_nGeomAlloc = m_nGeomLen = 0;
            m_nGeometryPropertyIndex = -1;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting a feature, and this element tag matches    */
/*      element name for the class, then we have finished the           */
/*      feature, and we pop the feature read state.                     */
/* -------------------------------------------------------------------- */
    const char *pszLast = nullptr;

    if( m_nDepth == m_nDepthFeature && poState->m_poFeature != nullptr
        && m_osElementName ==
                 poState->m_poFeature->GetClass()->GetElementName() )
    {
        m_nDepthFeature = 0;
        m_poReader->PopState();
    }

/* -------------------------------------------------------------------- */
/*      Ends of a wfs:Delete or wfs:Update should be triggered on the   */
/*      close of the <Filter> element.                                  */
/* -------------------------------------------------------------------- */
    else if( m_nDepth == m_nDepthFeature
             && poState->m_poFeature != nullptr
             && m_osElementName == "Filter"
             && (pszLast=poState->m_poFeature->GetClass()->GetElementName())
                != nullptr
             && ( EQUAL(pszLast, "Delete") || EQUAL(pszLast, "Update") ) )
    {
        m_nDepthFeature = 0;
        m_poReader->PopState();
    }

    else if( m_nDepth >= m_nDepthFeature
             && poState->m_poFeature != nullptr
             && m_osElementName == "ResourceId"
             && (pszLast=poState->m_poFeature->GetClass()->GetElementName())
                != nullptr
             && EQUAL(pszLast, "Delete") )
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
        if( m_osElementName == poState->GetLastComponent() )
            poState->PopPath();
        else
        {
            CPLAssert( false );
        }
    }
}

/************************************************************************/
/*                             characters()                             */
/************************************************************************/

void NASHandler::characters( const XMLCh *const chars,
                             const XMLSize_t length )
{
    if( m_pszCurField != nullptr )
    {
        const int nCurFieldLength = static_cast<int>(strlen(m_pszCurField));

        int nSkipped = 0;
        if (nCurFieldLength == 0)
        {
            // Ignore white space
            while( chars[nSkipped] == ' ' || chars[nSkipped] == 10 || chars[nSkipped] == 13 ||
                   chars[nSkipped] == '\t')
                nSkipped++;
        }

        transcode( chars + nSkipped, m_osCharacters,
                   static_cast<int>(length) - nSkipped );

        if( m_pszCurField == nullptr )
        {
            m_pszCurField = CPLStrdup(m_osCharacters);
        }
        else
        {
            m_pszCurField = static_cast<char *>(
                CPLRealloc( m_pszCurField,
                            nCurFieldLength+m_osCharacters.size()+1 ) );
            memcpy( m_pszCurField + nCurFieldLength, m_osCharacters.c_str(),
                    m_osCharacters.size() + 1 );
        }
    }
    else if( m_pszGeometry != nullptr )
    {
        int nSkipped = 0;
        if (m_nGeomLen == 0)
        {
            // Ignore white space
            while( chars[nSkipped] == ' ' || chars[nSkipped] == 10 || chars[nSkipped] == 13 ||
                   chars[nSkipped] == '\t')
                nSkipped++;
        }

        transcode( chars + nSkipped, m_osCharacters,
                   static_cast<int>(length) - nSkipped );

        const int nCharsLen = static_cast<int>(m_osCharacters.size());

        if( m_nGeomLen + nCharsLen*4 + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nCharsLen*4 + 1000);
            m_pszGeometry = (char *)
                CPLRealloc( m_pszGeometry, m_nGeomAlloc);
        }

        memcpy( m_pszGeometry+m_nGeomLen, m_osCharacters.c_str(),
                m_osCharacters.size() + 1 );
        m_nGeomLen += static_cast<int>(strlen(m_pszGeometry+m_nGeomLen));
    }
}

/************************************************************************/
/*                             fatalError()                             */
/************************************************************************/

void NASHandler::fatalError( const SAXParseException &exception)

{
    CPLString osErrMsg;
    transcode( exception.getMessage(), osErrMsg );
    CPLError( CE_Failure, CPLE_AppDefined,
              "XML Parsing Error: %s at line %d, column %d\n",
              osErrMsg.c_str(),
              static_cast<int>(exception.getLineNumber()),
              static_cast<int>(exception.getColumnNumber()) );
}

/************************************************************************/
/*                         IsGeometryElement()                          */
/************************************************************************/

bool NASHandler::IsGeometryElement( const char *pszElement )

{
    return strcmp(pszElement,"Polygon") == 0
        || strcmp(pszElement,"MultiPolygon") == 0
        || strcmp(pszElement,"MultiPoint") == 0
        || strcmp(pszElement,"MultiLineString") == 0
        || strcmp(pszElement,"MultiSurface") == 0
        || strcmp(pszElement,"GeometryCollection") == 0
        || strcmp(pszElement,"Point") == 0
        || strcmp(pszElement,"Curve") == 0
        || strcmp(pszElement,"MultiCurve") == 0
        || strcmp(pszElement,"CompositeCurve") == 0
        || strcmp(pszElement,"Surface") == 0
        || strcmp(pszElement,"PolygonPatch") == 0
        || strcmp(pszElement,"LineString") == 0;
}

// vim: set sw=4 expandtab :
