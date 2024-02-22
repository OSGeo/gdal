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

#define NASDebug(fmt, ...)                                                     \
    CPLDebugOnly("NAS", "%s:%d %s " fmt, __FILE__, __LINE__, __FUNCTION__,     \
                 __VA_ARGS__)

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

NASHandler::NASHandler(NASReader *poReader)
    : m_poReader(poReader), m_pszCurField(nullptr), m_pszGeometry(nullptr),
      m_nGeomAlloc(0), m_nGeomLen(0), m_nGeometryDepth(0),
      m_nGeometryPropertyIndex(-1), m_nDepth(0), m_nDepthFeature(0),
      m_bIgnoreFeature(false), m_Locator(nullptr)
{
}

/************************************************************************/
/*                            ~NASHandler()                             */
/************************************************************************/

NASHandler::~NASHandler()

{
    CPLFree(m_pszCurField);
    CPLFree(m_pszGeometry);
}

/************************************************************************/
/*                        GetAttributes()                               */
/************************************************************************/

CPLString NASHandler::GetAttributes(const Attributes *attrs)
{
    CPLString osRes;

    for (unsigned int i = 0; i < attrs->getLength(); i++)
    {
        osRes += " ";
        osRes += transcode(attrs->getQName(i));
        osRes += "=\"";
        osRes += transcode(attrs->getValue(i));
        osRes += "\"";
    }
    return osRes;
}

/************************************************************************/
/*                   setDocumentLocator()                               */
/************************************************************************/

void NASHandler::setDocumentLocator(const Locator *locator)
{
    m_Locator = locator;
    return DefaultHandler::setDocumentLocator(locator);
}

/************************************************************************/
/*                            startElement()                            */
/************************************************************************/

void NASHandler::startElement(const XMLCh *const /* uri */,
                              const XMLCh *const localname,
                              const XMLCh *const /* qname */,
                              const Attributes &attrs)

{
    GMLReadState *poState = m_poReader->GetState();

    transcode(localname, m_osElementName);

    NASDebug("element=%s poState=%s m_nDepth=%d m_nDepthFeature=%d context=%s",
             m_osElementName.c_str(), poState ? "(state)" : "(no state)",
             m_nDepth, m_nDepthFeature, m_osDeleteContext.c_str());

    m_nDepth++;
    if (m_bIgnoreFeature && m_nDepth > m_nDepthFeature)
        return;

    if (m_nDepthFeature == 0)
    {
        if (m_osElementName == "Replace")
        {
            const XMLCh achSafeToIgnore[] = {'s', 'a', 'f', 'e', 'T', 'o', 'I',
                                             'g', 'n', 'o', 'r', 'e', 0};
            int nIndex = attrs.getIndex(achSafeToIgnore);
            if (nIndex != -1)
                transcode(attrs.getValue(nIndex), m_osSafeToIgnore);
            else
                m_osSafeToIgnore = "true";
            m_osReplacingFID = "";

            CPLAssert(m_osDeleteContext == "");
            m_osDeleteContext = m_osElementName;
        }
        else if (m_osElementName == "Update" || m_osElementName == "Delete")
        {
            const XMLCh achTypeNames[] = {'t', 'y', 'p', 'e', 'N',
                                          'a', 'm', 'e', 's', 0};
            const XMLCh achTypeName[] = {'t', 'y', 'p', 'e', 'N',
                                         'a', 'm', 'e', 0};
            int nIndex = attrs.getIndex(achTypeNames);
            if (nIndex == -1)
                nIndex = attrs.getIndex(achTypeName);

            if (nIndex == -1)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "NAS: expected type name missing at %s:%d:%d",
                         m_poReader->GetSourceFileName(),
                         static_cast<int>(m_Locator->getLineNumber()),
                         static_cast<int>(m_Locator->getColumnNumber()));
                return;
            }

            transcode(attrs.getValue(nIndex), m_osTypeName);

            const char *pszTypeName = strchr(m_osTypeName.c_str(), ':');
            pszTypeName = pszTypeName ? pszTypeName + 1 : m_osTypeName.c_str();
            m_osTypeName = pszTypeName;

            CPLAssert(m_osDeleteContext == "");
            m_osDeleteContext = m_osElementName;
        }
        else if (m_osDeleteContext == "Update" &&
                 (m_osElementName == "Name" ||
                  m_osElementName == "ValueReference" ||
                  m_osElementName == "Value"))
        {
            // fetch value
            CPLFree(m_pszCurField);
            m_pszCurField = CPLStrdup("");
        }
        else if (m_osDeleteContext != "" && (m_osElementName == "ResourceId" ||
                                             m_osElementName == "FeatureId"))
        {
            const char *pszFilteredClassName =
                m_poReader->GetFilteredClassName();
            if (!pszFilteredClassName || EQUAL(pszFilteredClassName, "Delete"))
            {
                const XMLCh achRid[] = {'r', 'i', 'd', 0};
                const XMLCh achFid[] = {'f', 'i', 'd', 0};
                if (m_osTypeName == "")
                {
                    CPLError(CE_Failure, CPLE_AssertionFailed,
                             "NAS: type name(s) missing at %s:%d:%d",
                             m_poReader->GetSourceFileName(),
                             static_cast<int>(m_Locator->getLineNumber()),
                             static_cast<int>(m_Locator->getColumnNumber()));
                    return;
                }

                int nIndex = attrs.getIndex(
                    m_osElementName == "ResourceId" ? achRid : achFid);
                if (nIndex == -1)
                {
                    CPLError(CE_Failure, CPLE_AssertionFailed,
                             "NAS: expected feature id missing at %s,%d:%d",
                             m_poReader->GetSourceFileName(),
                             static_cast<int>(m_Locator->getLineNumber()),
                             static_cast<int>(m_Locator->getColumnNumber()));
                    return;
                }

                CPLString osFeatureId;
                transcode(attrs.getValue(nIndex), osFeatureId);

                m_poReader->PushFeature("Delete", attrs);
                m_poReader->SetFeaturePropertyDirectly("typeName",
                                                       CPLStrdup(m_osTypeName));
                m_poReader->SetFeaturePropertyDirectly(
                    "context", CPLStrdup(m_osDeleteContext));
                m_poReader->SetFeaturePropertyDirectly("FeatureId",
                                                       CPLStrdup(osFeatureId));

                if (m_osDeleteContext == "Replace")
                {
                    if (m_osReplacingFID == "")
                    {
                        CPLError(
                            CE_Failure, CPLE_AssertionFailed,
                            "NAS: replacing feature id not set at %s:%d:%d",
                            m_poReader->GetSourceFileName(),
                            static_cast<int>(m_Locator->getLineNumber()),
                            static_cast<int>(m_Locator->getColumnNumber()));
                        return;
                    }

                    m_poReader->SetFeaturePropertyDirectly(
                        "replacedBy", CPLStrdup(m_osReplacingFID));
                    m_poReader->SetFeaturePropertyDirectly(
                        "safeToIgnore", CPLStrdup(m_osSafeToIgnore));
                    m_osReplacingFID = "";
                    m_osSafeToIgnore = "";
                }
                else if (m_osDeleteContext == "Update")
                {
                    m_poReader->SetFeaturePropertyDirectly(
                        "endet", CPLStrdup(m_osUpdateEnds));
                    for (std::list<CPLString>::iterator it =
                             m_UpdateOccasions.begin();
                         it != m_UpdateOccasions.end(); ++it)
                    {
                        m_poReader->SetFeaturePropertyDirectly("anlass",
                                                               CPLStrdup(*it));
                    }

                    m_osUpdateEnds = "";
                    m_UpdateOccasions.clear();
                }

                return;
            }
            else
            {
                // we don't issue Delete features
                m_osDeleteContext = "";
            }
        }
        else if (m_poReader->IsFeatureElement(m_osElementName))
        {
            m_nDepthFeature = m_nDepth - 1;

            // record id of replacing feature
            if (m_osDeleteContext == "Replace")
            {
                const XMLCh achGmlId[] = {'g', 'm', 'l', ':', 'i', 'd', 0};
                int nIndex = attrs.getIndex(achGmlId);
                if (nIndex == -1)
                {
                    CPLError(CE_Failure, CPLE_AssertionFailed,
                             "NAS: id of replacing feature not set at %s:%d:%d",
                             m_poReader->GetSourceFileName(),
                             static_cast<int>(m_Locator->getLineNumber()),
                             static_cast<int>(m_Locator->getColumnNumber()));
                    m_bIgnoreFeature = true;
                    return;
                }

                CPLAssert(m_osReplacingFID == "");
                transcode(attrs.getValue(nIndex), m_osReplacingFID);
            }

            m_osTypeName = m_osElementName;

            const char *pszFilteredClassName =
                m_poReader->GetFilteredClassName();
            m_bIgnoreFeature = pszFilteredClassName &&
                               !EQUAL(m_osElementName, pszFilteredClassName);

            if (!m_bIgnoreFeature)
                m_poReader->PushFeature(m_osElementName, attrs);

            return;
        }
    }
    else if (m_pszGeometry != nullptr || IsGeometryElement(m_osElementName))
    {
        if (m_nGeometryPropertyIndex == -1 && poState->m_poFeature &&
            poState->m_poFeature->GetClass())
        {
            GMLFeatureClass *poClass = poState->m_poFeature->GetClass();
            m_nGeometryPropertyIndex =
                poClass->GetGeometryPropertyIndexBySrcElement(
                    poState->osPath.c_str());
        }

        const int nLNLen = static_cast<int>(m_osElementName.size());
        CPLString osAttributes = GetAttributes(&attrs);

        /* should save attributes too! */

        if (m_pszGeometry == nullptr)
            m_nGeometryDepth = poState->m_nPathLength;

        if (m_pszGeometry == nullptr ||
            m_nGeomLen + nLNLen + 4 + (int)osAttributes.size() > m_nGeomAlloc)
        {
            m_nGeomAlloc =
                (int)(m_nGeomAlloc * 1.3 + nLNLen + osAttributes.size() + 1000);
            m_pszGeometry = (char *)CPLRealloc(m_pszGeometry, m_nGeomAlloc);
        }

        strcpy(m_pszGeometry + m_nGeomLen, "<");
        strcpy(m_pszGeometry + m_nGeomLen + 1, m_osElementName);

        if (!osAttributes.empty())
        {
            strcat(m_pszGeometry + m_nGeomLen, " ");
            strcat(m_pszGeometry + m_nGeomLen, osAttributes);
        }

        strcat(m_pszGeometry + m_nGeomLen, ">");
        m_nGeomLen += static_cast<int>(strlen(m_pszGeometry + m_nGeomLen));
    }
    else if (m_poReader->IsAttributeElement(m_osElementName, attrs))
    {
        m_poReader->DealWithAttributes(
            m_osElementName, static_cast<int>(m_osElementName.length()), attrs);
        CPLFree(m_pszCurField);
        m_pszCurField = CPLStrdup("");
    }

    poState->PushPath(m_osElementName);

    if (poState->osPath.size() > 512)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "NAS: Too long path. Stop parsing at %s:%d:%d",
                 m_poReader->GetSourceFileName(),
                 static_cast<int>(m_Locator->getLineNumber()),
                 static_cast<int>(m_Locator->getColumnNumber()));
        m_poReader->StopParsing();
    }
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
void NASHandler::endElement(const XMLCh *const /* uri */,
                            const XMLCh *const localname,
                            const XMLCh *const /* qname */)

{
    GMLReadState *poState = m_poReader->GetState();

    transcode(localname, m_osElementName);

    NASDebug("element=%s poState=%s m_nDepth=%d m_nDepthFeature=%d context=%s",
             m_osElementName.c_str(), poState ? "(state)" : "(no state)",
             m_nDepth, m_nDepthFeature, m_osDeleteContext.c_str());

    m_nDepth--;
    if (m_bIgnoreFeature && m_nDepth >= m_nDepthFeature)
    {
        if (m_nDepth == m_nDepthFeature)
        {
            m_bIgnoreFeature = false;
            m_nDepthFeature = 0;
        }
        return;
    }

    if (m_osDeleteContext == "Update")
    {
        if (m_osElementName == "Name" || m_osElementName == "ValueReference")
        {
            const char *pszName;
            pszName = strrchr(m_pszCurField, '/');
            pszName = pszName ? pszName + 1 : m_pszCurField;
            pszName = strrchr(pszName, ':');
            pszName = pszName ? pszName + 1 : m_pszCurField;

            CPLAssert(m_osUpdatePropertyName == "");
            m_osUpdatePropertyName = pszName;
            CPLFree(m_pszCurField);
            m_pszCurField = nullptr;

            if (m_osUpdatePropertyName != "endet" &&
                m_osUpdatePropertyName != "anlass")
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "NAS: Unexpected property name %s at %s:%d:%d",
                         m_osUpdatePropertyName.c_str(),
                         m_poReader->GetSourceFileName(),
                         static_cast<int>(m_Locator->getLineNumber()),
                         static_cast<int>(m_Locator->getColumnNumber()));
                m_osUpdatePropertyName = "";
            }
        }
        else if (m_osElementName == "Value")
        {
            CPLAssert(m_osUpdatePropertyName != "");
            if (m_osUpdatePropertyName == "endet")
                m_osUpdateEnds = m_pszCurField;
            else if (m_osUpdatePropertyName == "anlass")
                m_UpdateOccasions.push_back(m_pszCurField);
            m_osUpdatePropertyName = "";
            CPLFree(m_pszCurField);
            m_pszCurField = nullptr;
        }
    }
    else if (m_pszCurField != nullptr && poState->m_poFeature != nullptr)
    {
        m_poReader->SetFeaturePropertyDirectly(poState->osPath.c_str(),
                                               m_pszCurField);
        m_pszCurField = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      If we are collecting Geometry than store it, and consider if    */
    /*      this is the end of the geometry.                                */
    /* -------------------------------------------------------------------- */
    if (m_pszGeometry != nullptr)
    {
        int nLNLen = static_cast<int>(m_osElementName.size());

        /* should save attributes too! */

        if (m_nGeomLen + nLNLen + 4 > m_nGeomAlloc)
        {
            m_nGeomAlloc = (int)(m_nGeomAlloc * 1.3 + nLNLen + 1000);
            m_pszGeometry = (char *)CPLRealloc(m_pszGeometry, m_nGeomAlloc);
        }

        strcat(m_pszGeometry + m_nGeomLen, "</");
        strcpy(m_pszGeometry + m_nGeomLen + 2, m_osElementName);
        strcat(m_pszGeometry + m_nGeomLen + nLNLen + 2, ">");
        m_nGeomLen += static_cast<int>(strlen(m_pszGeometry + m_nGeomLen));

        if (poState->m_nPathLength == m_nGeometryDepth + 1)
        {
            if (poState->m_poFeature != nullptr)
            {
                CPLXMLNode *psNode = CPLParseXMLString(m_pszGeometry);
                if (psNode)
                {
                    /* workaround for common malformed gml:pos with just a
                     * elevation value instead of a full 3D coordinate:
                     *
                     * <gml:Point gml:id="BII2H">
                     *    <gml:pos
                     * srsName="urn:adv:crs:ETRS89_h">41.394</gml:pos>
                     * </gml:Point>
                     *
                     */
                    const char *pszPos =
                        CPLGetXMLValue(psNode, "=Point.pos", nullptr);
                    if (pszPos != nullptr && strstr(pszPos, " ") == nullptr)
                    {
                        CPLSetXMLValue(psNode, "pos",
                                       CPLSPrintf("0 0 %s", pszPos));
                    }

                    if (m_nGeometryPropertyIndex >= 0 &&
                        m_nGeometryPropertyIndex <
                            poState->m_poFeature->GetGeometryCount() &&
                        poState->m_poFeature
                            ->GetGeometryList()[m_nGeometryPropertyIndex])
                    {
                        int iId =
                            poState->m_poFeature->GetClass()->GetPropertyIndex(
                                "gml_id");
                        const GMLProperty *poIdProp =
                            poState->m_poFeature->GetProperty(iId);
#ifdef DEBUG_VERBOSE
                        char *pszOldGeom = CPLSerializeXMLTree(
                            poState->m_poFeature
                                ->GetGeometryList()[m_nGeometryPropertyIndex]);

                        NASDebug(
                            "Overwriting other geometry (%s; replace:%s; "
                            "with:%s) at %s:%d:%d",
                            poIdProp && poIdProp->nSubProperties > 0 &&
                                    poIdProp->papszSubProperties[0]
                                ? poIdProp->papszSubProperties[0]
                                : "(null)",
                            m_pszGeometry, pszOldGeom,
                            m_poReader->GetSourceFileName(),
                            static_cast<int>(m_Locator->getLineNumber()),
                            static_cast<int>(m_Locator->getColumnNumber()));

                        CPLFree(pszOldGeom);
#else
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "NAS: Overwriting other geometry (%s) at %s:%d:%d",
                            poIdProp && poIdProp->nSubProperties > 0 &&
                                    poIdProp->papszSubProperties[0]
                                ? poIdProp->papszSubProperties[0]
                                : "(null)",
                            m_poReader->GetSourceFileName(),
                            static_cast<int>(m_Locator->getLineNumber()),
                            static_cast<int>(m_Locator->getColumnNumber()));
#endif
                    }

                    if (m_nGeometryPropertyIndex >= 0)
                        poState->m_poFeature->SetGeometryDirectly(
                            m_nGeometryPropertyIndex, psNode);

                    // no geometry property or property without element path
                    else if (poState->m_poFeature->GetClass()
                                     ->GetGeometryPropertyCount() == 0 ||
                             (poState->m_poFeature->GetClass()
                                      ->GetGeometryPropertyCount() == 1 &&
                              poState->m_poFeature->GetClass()
                                  ->GetGeometryProperty(0)
                                  ->GetSrcElement() &&
                              *poState->m_poFeature->GetClass()
                                      ->GetGeometryProperty(0)
                                      ->GetSrcElement() == 0))
                        poState->m_poFeature->SetGeometryDirectly(psNode);

                    else
                    {
                        CPLError(
                            CE_Warning, CPLE_AssertionFailed,
                            "NAS: Unexpected geometry skipped (class:%s "
                            "path:%s geom:%s) at %s:%d:%d",
                            poState->m_poFeature->GetClass()->GetName(),
                            poState->osPath.c_str(), m_pszGeometry,
                            m_poReader->GetSourceFileName(),
                            static_cast<int>(m_Locator->getLineNumber()),
                            static_cast<int>(m_Locator->getColumnNumber()));
                        CPLDestroyXMLNode(psNode);
                    }
                }
                else
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "NAS: Invalid geometry skipped at %s:%d:%d",
                             m_poReader->GetSourceFileName(),
                             static_cast<int>(m_Locator->getLineNumber()),
                             static_cast<int>(m_Locator->getColumnNumber()));
            }
            else
                CPLError(CE_Warning, CPLE_AppDefined,
                         "NAS: Skipping geometry without feature at %s:%d:%d",
                         m_poReader->GetSourceFileName(),
                         static_cast<int>(m_Locator->getLineNumber()),
                         static_cast<int>(m_Locator->getColumnNumber()));

            CPLFree(m_pszGeometry);
            m_pszGeometry = nullptr;
            m_nGeomAlloc = m_nGeomLen = 0;
            m_nGeometryPropertyIndex = -1;
        }
    }

    // Finished actual feature or ResourceId/FeatureId of Delete/Replace/Update operation
    if ((m_nDepth == m_nDepthFeature && poState->m_poFeature != nullptr &&
         EQUAL(m_osElementName,
               poState->m_poFeature->GetClass()->GetElementName())) ||
        (m_osDeleteContext != "" &&
         (m_osElementName == "ResourceId" || m_osElementName == "FeatureId")))
    {
        m_nDepthFeature = 0;
        m_poReader->PopState();
    }
    else
        poState->PopPath();

    if (m_osDeleteContext == m_osElementName)
    {
        m_osDeleteContext = "";
    }
}

/************************************************************************/
/*                             characters()                             */
/************************************************************************/

void NASHandler::characters(const XMLCh *const chars, const XMLSize_t length)
{
    if (m_pszCurField != nullptr)
    {
        const int nCurFieldLength = static_cast<int>(strlen(m_pszCurField));

        int nSkipped = 0;
        if (nCurFieldLength == 0)
        {
            // Ignore white space
            while (chars[nSkipped] == ' ' || chars[nSkipped] == 10 ||
                   chars[nSkipped] == 13 || chars[nSkipped] == '\t')
                nSkipped++;
        }

        transcode(chars + nSkipped, m_osCharacters,
                  static_cast<int>(length) - nSkipped);

        m_pszCurField = static_cast<char *>(CPLRealloc(
            m_pszCurField, nCurFieldLength + m_osCharacters.size() + 1));
        memcpy(m_pszCurField + nCurFieldLength, m_osCharacters.c_str(),
               m_osCharacters.size() + 1);
    }

    if (m_pszGeometry != nullptr)
    {
        int nSkipped = 0;
        if (m_nGeomLen == 0)
        {
            // Ignore white space
            while (chars[nSkipped] == ' ' || chars[nSkipped] == 10 ||
                   chars[nSkipped] == 13 || chars[nSkipped] == '\t')
                nSkipped++;
        }

        transcode(chars + nSkipped, m_osCharacters,
                  static_cast<int>(length) - nSkipped);

        const int nCharsLen = static_cast<int>(m_osCharacters.size());

        if (m_nGeomLen + nCharsLen * 4 + 4 > m_nGeomAlloc)
        {
            m_nGeomAlloc = (int)(m_nGeomAlloc * 1.3 + nCharsLen * 4 + 1000);
            m_pszGeometry = (char *)CPLRealloc(m_pszGeometry, m_nGeomAlloc);
        }

        memcpy(m_pszGeometry + m_nGeomLen, m_osCharacters.c_str(),
               m_osCharacters.size() + 1);
        m_nGeomLen += static_cast<int>(strlen(m_pszGeometry + m_nGeomLen));
    }
}

/************************************************************************/
/*                             fatalError()                             */
/************************************************************************/

void NASHandler::fatalError(const SAXParseException &exception)

{
    CPLString osErrMsg;
    transcode(exception.getMessage(), osErrMsg);
    CPLError(CE_Failure, CPLE_AppDefined,
             "NAS: XML Parsing Error: %s at line %d, column %d\n",
             osErrMsg.c_str(), static_cast<int>(exception.getLineNumber()),
             static_cast<int>(exception.getColumnNumber()));
}

/************************************************************************/
/*                         IsGeometryElement()                          */
/************************************************************************/

bool NASHandler::IsGeometryElement(const char *pszElement)

{
    return strcmp(pszElement, "Polygon") == 0 ||
           strcmp(pszElement, "MultiPolygon") == 0 ||
           strcmp(pszElement, "MultiPoint") == 0 ||
           strcmp(pszElement, "MultiLineString") == 0 ||
           strcmp(pszElement, "MultiSurface") == 0 ||
           strcmp(pszElement, "GeometryCollection") == 0 ||
           strcmp(pszElement, "Point") == 0 ||
           strcmp(pszElement, "Curve") == 0 ||
           strcmp(pszElement, "MultiCurve") == 0 ||
           strcmp(pszElement, "CompositeCurve") == 0 ||
           strcmp(pszElement, "Surface") == 0 ||
           strcmp(pszElement, "PolygonPatch") == 0 ||
           strcmp(pszElement, "LineString") == 0;
}

// vim: set sw=4 expandtab ai :
