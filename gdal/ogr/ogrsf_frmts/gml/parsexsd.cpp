/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLParseXSD()
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "parsexsd.h"

#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_http.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "ogr_core.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                              StripNS()                               */
/*                                                                      */
/*      Return potentially shortened form of string with namespace      */
/*      stripped off if there is one.  Returns pointer into             */
/*      original string.                                                */
/************************************************************************/
static const char *StripNS(const char *pszFullValue)

{
    const char *pszColon = strstr(pszFullValue, ":");
    if( pszColon == nullptr )
        return pszFullValue;
    else
        return pszColon + 1;
}

/************************************************************************/
/*                   GetSimpleTypeProperties()                          */
/************************************************************************/

static
bool GetSimpleTypeProperties(CPLXMLNode *psTypeNode,
                             GMLPropertyType *pGMLType,
                             int *pnWidth,
                             int *pnPrecision)
{
    const char *pszBase =
        StripNS(CPLGetXMLValue(psTypeNode, "restriction.base", ""));

    if( EQUAL(pszBase, "decimal") )
    {
        *pGMLType = GMLPT_Real;
        const char *pszWidth =
            CPLGetXMLValue(psTypeNode, "restriction.totalDigits.value", "0");
        const char *pszPrecision =
            CPLGetXMLValue(psTypeNode, "restriction.fractionDigits.value", "0");
        *pnWidth = atoi(pszWidth);
        *pnPrecision = atoi(pszPrecision);
        return true;
    }

     else if( EQUAL(pszBase, "float") )
    {
        *pGMLType = GMLPT_Float;
        return true;
    }

    else if( EQUAL(pszBase, "double") )
    {
        *pGMLType = GMLPT_Real;
        return true;
    }

    else if( EQUAL(pszBase, "integer") )
    {
        *pGMLType = GMLPT_Integer;
        const char *pszWidth =
            CPLGetXMLValue(psTypeNode, "restriction.totalDigits.value", "0");
        *pnWidth = atoi(pszWidth);
        return true;
    }

    else if( EQUAL(pszBase, "long") )
    {
        *pGMLType = GMLPT_Integer64;
        const char *pszWidth =
            CPLGetXMLValue(psTypeNode, "restriction.totalDigits.value", "0");
        *pnWidth = atoi(pszWidth);
        return true;
    }

    else if( EQUAL(pszBase, "unsignedLong") )
    {
        // Optimistically map to signed integer...
        *pGMLType = GMLPT_Integer64;
        const char *pszWidth =
            CPLGetXMLValue(psTypeNode, "restriction.totalDigits.value", "0");
        *pnWidth = atoi(pszWidth);
        return true;
    }

    else if( EQUAL(pszBase, "string") )
    {
        *pGMLType = GMLPT_String;
        const char *pszWidth =
            CPLGetXMLValue(psTypeNode, "restriction.maxLength.value", "0");
        *pnWidth = atoi(pszWidth);
        return true;
    }

    else if( EQUAL(pszBase, "date")  )
    {
        *pGMLType = GMLPT_Date;
        return true;
    }

    else if( EQUAL(pszBase, "time")  )
    {
        *pGMLType = GMLPT_Time;
        return true;
    }

    else if( EQUAL(pszBase, "dateTime") )
    {
        *pGMLType = GMLPT_DateTime;
        return true;
    }

    else if( EQUAL(pszBase, "boolean") )
    {
        *pGMLType = GMLPT_Boolean;
        return true;
    }

    else if( EQUAL(pszBase, "short") )
    {
        *pGMLType = GMLPT_Short;
        return true;
    }

    return false;
}

/************************************************************************/
/*                      LookForSimpleType()                             */
/************************************************************************/

static
bool LookForSimpleType(CPLXMLNode *psSchemaNode,
                      const char* pszStrippedNSType,
                      GMLPropertyType *pGMLType,
                      int *pnWidth,
                      int *pnPrecision)
{
    CPLXMLNode *psThis = psSchemaNode->psChild;
    for( ; psThis != nullptr; psThis = psThis->psNext )
    {
        if( psThis->eType == CXT_Element &&
            EQUAL(psThis->pszValue, "simpleType") &&
            EQUAL(CPLGetXMLValue(psThis, "name", ""), pszStrippedNSType) )
        {
            break;
        }
    }
    if( psThis == nullptr )
        return false;

    return GetSimpleTypeProperties(psThis, pGMLType, pnWidth, pnPrecision);
}

/************************************************************************/
/*                      GetSingleChildElement()                         */
/************************************************************************/

/* Returns the child element whose name is pszExpectedValue only if */
/* there is only one child that is an element. */
static
CPLXMLNode *GetSingleChildElement(CPLXMLNode *psNode,
                                  const char *pszExpectedValue)
{
    if( psNode == nullptr )
        return nullptr;

    CPLXMLNode *psIter = psNode->psChild;
    if( psIter == nullptr )
        return nullptr;

    CPLXMLNode *psChild = nullptr;
    while( psIter != nullptr )
    {
        if( psIter->eType == CXT_Element )
        {
            if( psChild != nullptr )
                return nullptr;
            if( pszExpectedValue != nullptr &&
                strcmp(psIter->pszValue, pszExpectedValue) != 0 )
                return nullptr;
            psChild = psIter;
        }
        psIter = psIter->psNext;
    }
    return psChild;
}

/************************************************************************/
/*                      CheckMinMaxOccursCardinality()                  */
/************************************************************************/

static int CheckMinMaxOccursCardinality(CPLXMLNode *psNode)
{
    const char *pszMinOccurs = CPLGetXMLValue(psNode, "minOccurs", nullptr);
    const char *pszMaxOccurs = CPLGetXMLValue(psNode, "maxOccurs", nullptr);
    return (pszMinOccurs == nullptr || EQUAL(pszMinOccurs, "0") ||
            EQUAL(pszMinOccurs, "1")) &&
           (pszMaxOccurs == nullptr || EQUAL(pszMaxOccurs, "1"));
}

/************************************************************************/
/*                     GetListTypeFromSingleType()                      */
/************************************************************************/

static GMLPropertyType GetListTypeFromSingleType(GMLPropertyType eType)
{
    if( eType == GMLPT_String )
        return GMLPT_StringList;
    if( eType == GMLPT_Integer || eType == GMLPT_Short )
        return GMLPT_IntegerList;
    if( eType == GMLPT_Integer64 )
        return GMLPT_Integer64List;
    if( eType == GMLPT_Real || eType == GMLPT_Float )
        return GMLPT_RealList;
    if( eType == GMLPT_Boolean )
        return GMLPT_BooleanList;
    if( eType == GMLPT_FeatureProperty )
        return GMLPT_FeaturePropertyList;
    return eType;
}

/************************************************************************/
/*                      ParseFeatureType()                              */
/************************************************************************/

typedef struct
{
    const char *pszName;
    OGRwkbGeometryType eType;
} AssocNameType;

static const AssocNameType apsPropertyTypes[] =
{
    {"GeometryPropertyType", wkbUnknown},
    {"PointPropertyType", wkbPoint},
    {"LineStringPropertyType", wkbLineString},
    {"CurvePropertyType", wkbCompoundCurve},
    {"PolygonPropertyType", wkbPolygon},
    {"SurfacePropertyType", wkbCurvePolygon},
    {"MultiPointPropertyType", wkbMultiPoint},
    {"MultiLineStringPropertyType", wkbMultiLineString},
    {"MultiCurvePropertyType", wkbMultiCurve},
    {"MultiPolygonPropertyType", wkbMultiPolygon},
    {"MultiSurfacePropertyType", wkbMultiSurface},
    {"MultiGeometryPropertyType", wkbGeometryCollection},
    {"GeometryAssociationType", wkbUnknown},
    {nullptr, wkbUnknown},
};

/* Found in FME .xsd  (e.g. <element ref="gml:curveProperty" minOccurs="0"/>) */
static const AssocNameType apsRefTypes[] =
{
    {"pointProperty", wkbPoint},
    {"curveProperty", wkbLineString}, // Should we promote to wkbCompoundCurve?
    {"surfaceProperty", wkbPolygon},  // Should we promote to wkbCurvePolygon?
    {"multiPointProperty", wkbMultiPoint},
    {"multiCurveProperty", wkbMultiLineString},
    // Should we promote to wkbMultiSurface?
    {"multiSurfaceProperty", wkbMultiPolygon},
    {nullptr, wkbUnknown},
};

static
GMLFeatureClass *GMLParseFeatureType(CPLXMLNode *psSchemaNode,
                                     const char *pszName,
                                     CPLXMLNode *psThis);

static
GMLFeatureClass *GMLParseFeatureType(CPLXMLNode *psSchemaNode,
                                const char *pszName,
                                const char *pszType)
{
    CPLXMLNode *psThis = psSchemaNode->psChild;
    for( ; psThis != nullptr; psThis = psThis->psNext )
    {
        if( psThis->eType == CXT_Element &&
            EQUAL(psThis->pszValue, "complexType") &&
            EQUAL(CPLGetXMLValue(psThis, "name", ""), pszType) )
        {
            break;
        }
    }
    if( psThis == nullptr )
        return nullptr;

    return GMLParseFeatureType(psSchemaNode, pszName, psThis);
}

static
GMLFeatureClass *GMLParseFeatureType(CPLXMLNode *psSchemaNode,
                                     const char *pszName,
                                     CPLXMLNode *psComplexType)
{

/* -------------------------------------------------------------------- */
/*      Grab the sequence of extensions greatgrandchild.                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psAttrSeq =
        CPLGetXMLNode(psComplexType, "complexContent.extension.sequence");

    if( psAttrSeq == nullptr )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      We are pretty sure this going to be a valid Feature class       */
/*      now, so create it.                                              */
/* -------------------------------------------------------------------- */
    GMLFeatureClass *poClass = new GMLFeatureClass(pszName);

/* -------------------------------------------------------------------- */
/*      Loop over each of the attribute elements being defined for      */
/*      this feature class.                                             */
/* -------------------------------------------------------------------- */
    int nAttributeIndex = 0;

    bool bGotUnrecognizedType = false;

    CPLXMLNode *psAttrDef = psAttrSeq->psChild;
    for( ; psAttrDef != nullptr; psAttrDef = psAttrDef->psNext )
    {
        if( strcmp(psAttrDef->pszValue, "group") == 0 )
        {
            /* Too complex schema for us. Aborts parsing */
            delete poClass;
            return nullptr;
        }

        /* Parse stuff like:
        <xs:choice>
            <xs:element ref="gml:polygonProperty"/>
            <xs:element ref="gml:multiPolygonProperty"/>
        </xs:choice>
        as found in https://downloadagiv.blob.core.windows.net/overstromingsgebieden-en-oeverzones/2014_01/Overstromingsgebieden_en_oeverzones_2014_01_GML.zip
        */
        if( strcmp(psAttrDef->pszValue, "choice") == 0 )
        {
            CPLXMLNode *psChild = psAttrDef->psChild;
            bool bPolygon = false;
            bool bMultiPolygon = false;
            for( ; psChild; psChild = psChild->psNext )
            {
                if( psChild->eType != CXT_Element )
                    continue;
                if( strcmp(psChild->pszValue, "element") == 0 )
                {
                    const char *pszRef = CPLGetXMLValue(psChild, "ref", nullptr);
                    if( pszRef != nullptr )
                    {
                        if( strcmp(pszRef, "gml:polygonProperty") == 0 )
                        {
                            bPolygon = true;
                        }
                        else if( strcmp(pszRef, "gml:multiPolygonProperty") == 0 )
                        {
                            bMultiPolygon = true;
                        }
                        else
                        {
                            delete poClass;
                            return nullptr;
                        }
                    }
                    else
                    {
                        delete poClass;
                        return nullptr;
                    }
                }
            }
            if( bPolygon && bMultiPolygon )
            {
                poClass->AddGeometryProperty(new GMLGeometryPropertyDefn(
                    "", "", wkbMultiPolygon, nAttributeIndex, true));

                nAttributeIndex++;
            }
            continue;
        }

        if( !EQUAL(psAttrDef->pszValue, "element") )
            continue;

        // MapServer WFS writes element type as an attribute of element
        // not as a simpleType definition.
        const char *pszType = CPLGetXMLValue(psAttrDef, "type", nullptr);
        const char *pszElementName = CPLGetXMLValue(psAttrDef, "name", nullptr);
        bool bNullable =
            EQUAL(CPLGetXMLValue(psAttrDef, "minOccurs", "1"), "0");
        const char *pszMaxOccurs = CPLGetXMLValue(psAttrDef, "maxOccurs", nullptr);
        if (pszType != nullptr)
        {
            const char *pszStrippedNSType = StripNS(pszType);
            int nWidth = 0;
            int nPrecision = 0;

            GMLPropertyType gmlType = GMLPT_Untyped;
            if (EQUAL(pszStrippedNSType, "string") ||
                EQUAL(pszStrippedNSType, "Character"))
                gmlType = GMLPT_String;
            else if (EQUAL(pszStrippedNSType, "date"))
                gmlType = GMLPT_Date;
            else if (EQUAL(pszStrippedNSType, "time"))
                gmlType = GMLPT_Time;
            else if (EQUAL(pszStrippedNSType, "dateTime"))
                gmlType = GMLPT_DateTime;
            else if (EQUAL(pszStrippedNSType, "real") ||
                     EQUAL(pszStrippedNSType, "double") ||
                     EQUAL(pszStrippedNSType, "decimal"))
                gmlType = GMLPT_Real;
            else if (EQUAL(pszStrippedNSType, "float") )
                gmlType = GMLPT_Float;
            else if (EQUAL(pszStrippedNSType, "int") ||
                     EQUAL(pszStrippedNSType, "integer"))
                gmlType = GMLPT_Integer;
            else if (EQUAL(pszStrippedNSType, "long"))
                gmlType = GMLPT_Integer64;
            else if (EQUAL(pszStrippedNSType, "unsignedLong"))
            {
                // Optimistically map to signed integer
                gmlType = GMLPT_Integer64;
            }
            else if (EQUAL(pszStrippedNSType, "short") )
                gmlType = GMLPT_Short;
            else if (EQUAL(pszStrippedNSType, "boolean") )
                gmlType = GMLPT_Boolean;
            // TODO: Would be nice to have a binary type.
            else if (EQUAL(pszStrippedNSType, "hexBinary"))
                gmlType = GMLPT_String;
            else if (strcmp(pszType, "gml:FeaturePropertyType") == 0 )
            {
                gmlType = GMLPT_FeatureProperty;
            }
            else if (STARTS_WITH(pszType, "gml:"))
            {
                const AssocNameType *psIter = apsPropertyTypes;
                while(psIter->pszName)
                {
                    if (strncmp(pszType + 4, psIter->pszName,
                                strlen(psIter->pszName)) == 0)
                    {
                        OGRwkbGeometryType eType = psIter->eType;
                        std::string osSRSName;

                        // Look if there's a comment restricting to subclasses.
                        const CPLXMLNode* psIter2 = psAttrDef->psNext;
                        while( psIter2 != nullptr )
                        {
                            if( psIter2->eType == CXT_Comment )
                            {
                                if( strstr(psIter2->pszValue,
                                           "restricted to Polygon") )
                                    eType = wkbPolygon;
                                else if( strstr(psIter2->pszValue,
                                                "restricted to LineString") )
                                    eType = wkbLineString;
                                else if( strstr(psIter2->pszValue,
                                                "restricted to MultiPolygon") )
                                    eType = wkbMultiPolygon;
                                else if( strstr(psIter2->pszValue,
                                                "restricted to MultiLineString") )
                                    eType = wkbMultiLineString;
                                else
                                {
                                    const char* pszSRSName = strstr(psIter2->pszValue, "srsName=\"");
                                    if( pszSRSName )
                                    {
                                        osSRSName = pszSRSName + strlen("srsName=\"");
                                        const auto nPos = osSRSName.find('"');
                                        if( nPos != std::string::npos )
                                            osSRSName.resize(nPos);
                                        else
                                            osSRSName.clear();
                                    }
                                }
                            }

                            psIter2 = psIter2->psNext;
                        }

                        GMLGeometryPropertyDefn* poDefn =
                            new GMLGeometryPropertyDefn(
                                pszElementName, pszElementName, eType,
                                nAttributeIndex, bNullable);
                        poDefn->SetSRSName(osSRSName);

                        if( poClass->AddGeometryProperty(poDefn) < 0 )
                            delete poDefn;
                        else
                            nAttributeIndex++;

                        break;
                    }

                    psIter++;
                }

                if (psIter->pszName == nullptr)
                {
                    // Can be a non geometry gml type.
                    // Too complex schema for us. Aborts parsing.
                    delete poClass;
                    return nullptr;
                }

                if (poClass->GetGeometryPropertyCount() == 0)
                    bGotUnrecognizedType = true;

                continue;
            }

            /* Integraph stuff */
            else if (strcmp(pszType, "G:Point_MultiPointPropertyType") == 0 ||
                     strcmp(pszType, "gmgml:Point_MultiPointPropertyType") == 0)
            {
                GMLGeometryPropertyDefn* poDefn =
                    new GMLGeometryPropertyDefn(
                        pszElementName, pszElementName, wkbMultiPoint,
                        nAttributeIndex, bNullable);

                if( poClass->AddGeometryProperty(poDefn) < 0 )
                    delete poDefn;
                else
                    nAttributeIndex++;

                continue;
            }
            else if (strcmp(pszType,
                            "G:LineString_MultiLineStringPropertyType") == 0 ||
                     strcmp(pszType,
                            "gmgml:LineString_MultiLineStringPropertyType") == 0)
            {
                GMLGeometryPropertyDefn* poDefn =
                    new GMLGeometryPropertyDefn(
                        pszElementName, pszElementName, wkbMultiLineString,
                        nAttributeIndex, bNullable);

                if( poClass->AddGeometryProperty(poDefn) < 0 )
                    delete poDefn;
                else
                    nAttributeIndex++;

                continue;
            }
            else if (strcmp(pszType,
                            "G:Polygon_MultiPolygonPropertyType") == 0 ||
                     strcmp(pszType,
                            "gmgml:Polygon_MultiPolygonPropertyType") == 0 ||
                     strcmp(pszType,
                            "gmgml:Polygon_Surface_MultiSurface_CompositeSurfacePropertyType") == 0)
            {
                GMLGeometryPropertyDefn* poDefn =
                    new GMLGeometryPropertyDefn(
                        pszElementName, pszElementName, wkbMultiPolygon,
                        nAttributeIndex, bNullable);

                if( poClass->AddGeometryProperty(poDefn) < 0 )
                    delete poDefn;
                else
                    nAttributeIndex++;

                continue;
            }

            // ERDAS Apollo stufflike in
            // http://apollo.erdas.com/erdas-apollo/vector/WORLDWIDE?SERVICE=WFS&VERSION=1.0.0&REQUEST=DescribeFeatureType&TYPENAME=wfs:cntry98)
            else if (strcmp(pszType, "wfs:MixedPolygonPropertyType") == 0)
            {
                GMLGeometryPropertyDefn* poDefn =
                    new GMLGeometryPropertyDefn(
                        pszElementName, pszElementName, wkbMultiPolygon,
                        nAttributeIndex, bNullable);

                if( poClass->AddGeometryProperty(poDefn) < 0 )
                    delete poDefn;
                else
                    nAttributeIndex++;

                continue;
            }

            else
            {
                gmlType = GMLPT_Untyped;
                if ( !LookForSimpleType(psSchemaNode, pszStrippedNSType,
                                        &gmlType, &nWidth, &nPrecision) )
                {
                    // Too complex schema for us. Aborts parsing.
                    delete poClass;
                    return nullptr;
                }
            }

            if (pszElementName == nullptr)
                pszElementName = "unnamed";
            const char *pszPropertyName = pszElementName;
            if( gmlType == GMLPT_FeatureProperty )
            {
                pszPropertyName = CPLSPrintf("%s_href", pszElementName);
            }
            GMLPropertyDefn *poProp =
                new GMLPropertyDefn(pszPropertyName, pszElementName);

            if( pszMaxOccurs != nullptr && strcmp(pszMaxOccurs, "1") != 0 )
                gmlType = GetListTypeFromSingleType(gmlType);

            poProp->SetType(gmlType);
            poProp->SetWidth(nWidth);
            poProp->SetPrecision(nPrecision);
            poProp->SetNullable(bNullable);

            if (poClass->AddProperty( poProp ) < 0)
                delete poProp;
            else
                nAttributeIndex++;

            continue;
        }

        // For now we skip geometries.  Fixup later.
        CPLXMLNode *psSimpleType = CPLGetXMLNode(psAttrDef, "simpleType");
        if( psSimpleType == nullptr )
        {
            const char *pszRef = CPLGetXMLValue(psAttrDef, "ref", nullptr);

            // FME .xsd
            if (pszRef != nullptr && STARTS_WITH(pszRef, "gml:"))
            {
                const AssocNameType *psIter = apsRefTypes;
                while(psIter->pszName)
                {
                    if (strncmp(pszRef + 4, psIter->pszName,
                                strlen(psIter->pszName)) == 0)
                    {
                        if (poClass->GetGeometryPropertyCount() > 0)
                        {
                            OGRwkbGeometryType eNewType = psIter->eType;
                            OGRwkbGeometryType eOldType =
                                (OGRwkbGeometryType)poClass
                                    ->GetGeometryProperty(0)
                                    ->GetType();

                            if ((eNewType == wkbMultiPoint &&
                                 eOldType == wkbPoint) ||
                                (eNewType == wkbMultiLineString &&
                                 eOldType == wkbLineString) ||
                                (eNewType == wkbMultiPolygon &&
                                 eOldType == wkbPolygon))
                            {
                                poClass->GetGeometryProperty(0)->SetType(
                                    eNewType);
                            }
                            else
                            {
                                CPLDebug(
                                    "GML",
                                    "Geometry field already found ! "
                                    "Ignoring the following ones");
                            }
                        }
                        else
                        {
                            GMLGeometryPropertyDefn* poDefn =
                                new GMLGeometryPropertyDefn(
                                    pszElementName, pszElementName,
                                    psIter->eType, nAttributeIndex, true);

                            if( poClass->AddGeometryProperty(poDefn) < 0 )
                                delete poDefn;
                            else
                                nAttributeIndex++;
                        }

                        break;
                    }

                    psIter++;
                }

                if (psIter->pszName == nullptr)
                {
                    // Can be a non geometry gml type .
                    // Too complex schema for us. Aborts parsing.
                    delete poClass;
                    return nullptr;
                }

                if (poClass->GetGeometryPropertyCount() == 0)
                    bGotUnrecognizedType = true;

                continue;
            }

            /* Parse stuff like the following found in http://199.29.1.81:8181/miwfs/GetFeature.ashx?REQUEST=GetFeature&MAXFEATURES=1&SERVICE=WFS&VERSION=1.0.0&TYPENAME=miwfs:World :
            <xs:element name="Obj" minOccurs="0" maxOccurs="1">
                <xs:complexType>
                    <xs:sequence>
                        <xs:element ref="gml:_Geometry"/>
                    </xs:sequence>
                </xs:complexType>
            </xs:element>
            */
            CPLXMLNode *l_psComplexType =
                GetSingleChildElement(psAttrDef, "complexType");
            CPLXMLNode *psComplexTypeSequence =
                GetSingleChildElement(l_psComplexType, "sequence");
            CPLXMLNode *psComplexTypeSequenceElement =
                GetSingleChildElement(psComplexTypeSequence, "element");

            if( pszElementName != nullptr &&
                CheckMinMaxOccursCardinality(psAttrDef) &&
                psComplexTypeSequenceElement != nullptr &&
                CheckMinMaxOccursCardinality(psComplexTypeSequence) &&
                strcmp(CPLGetXMLValue(psComplexTypeSequenceElement, "ref", ""),
                       "gml:_Geometry") == 0 )
            {
                GMLGeometryPropertyDefn* poDefn =
                    new GMLGeometryPropertyDefn(
                        pszElementName, pszElementName, wkbUnknown, nAttributeIndex,
                        bNullable);

                if( poClass->AddGeometryProperty(poDefn) < 0 )
                    delete poDefn;
                else
                    nAttributeIndex++;

                continue;
            }
            else
            {
                // Too complex schema for us. Aborts parsing.
                delete poClass;
                return nullptr;
            }
        }

        if (pszElementName == nullptr)
            pszElementName = "unnamed";
        GMLPropertyDefn *poProp =
            new GMLPropertyDefn(pszElementName, pszElementName);

        GMLPropertyType eType = GMLPT_Untyped;
        int nWidth = 0;
        int nPrecision = 0;
        GetSimpleTypeProperties(psSimpleType, &eType, &nWidth, &nPrecision);

        if( pszMaxOccurs != nullptr && strcmp(pszMaxOccurs, "1") != 0 )
            eType = GetListTypeFromSingleType(eType);

        poProp->SetType(eType);
        poProp->SetWidth(nWidth);
        poProp->SetPrecision(nPrecision);
        poProp->SetNullable(bNullable);

        if (poClass->AddProperty(poProp) < 0)
            delete poProp;
        else
            nAttributeIndex++;
    }

    // If we have found an unknown types, let's be on the side of caution and
    // create a geometry field.
    if( poClass->GetGeometryPropertyCount() == 0 &&
        bGotUnrecognizedType )
    {
        poClass->AddGeometryProperty(
            new GMLGeometryPropertyDefn("", "", wkbUnknown, -1, true));
    }

/* -------------------------------------------------------------------- */
/*      Class complete, add to reader class list.                       */
/* -------------------------------------------------------------------- */
    poClass->SetSchemaLocked(true);

    return poClass;
}

/************************************************************************/
/*                         GMLParseXMLFile()                            */
/************************************************************************/

static CPLXMLNode *GMLParseXMLFile(const char *pszFilename)
{
    if( STARTS_WITH(pszFilename, "http://") ||
        STARTS_WITH(pszFilename, "https://") )
    {
        CPLXMLNode *psRet = nullptr;
        CPLHTTPResult *psResult = CPLHTTPFetch(pszFilename, nullptr);
        if( psResult != nullptr )
        {
            if( psResult->pabyData != nullptr )
            {
                psRet = CPLParseXMLString((const char *)psResult->pabyData);
            }
            CPLHTTPDestroyResult(psResult);
        }
        return psRet;
    }
    else
    {
        return CPLParseXMLFile(pszFilename);
    }
}

/************************************************************************/
/*                       CPLGetFirstChildNode()                         */
/************************************************************************/

static CPLXMLNode *CPLGetFirstChildNode(CPLXMLNode *psNode)
{
    if( psNode == nullptr )
        return nullptr;
    CPLXMLNode *psIter = psNode->psChild;
    while( psIter != nullptr )
    {
        if( psIter->eType == CXT_Element )
            return psIter;
        psIter = psIter->psNext;
    }
    return nullptr;
}

/************************************************************************/
/*                          CPLGetLastNode()                            */
/************************************************************************/

static CPLXMLNode *CPLGetLastNode(CPLXMLNode *psNode)
{
    CPLXMLNode *psIter = psNode;
    while( psIter->psNext != nullptr )
        psIter = psIter->psNext;
    return psIter;
}

/************************************************************************/
/*                       CPLXMLSchemaResolveInclude()                   */
/************************************************************************/

static
void CPLXMLSchemaResolveInclude( const char *pszMainSchemaLocation,
                                 CPLXMLNode *psSchemaNode )
{
    std::set<CPLString> osAlreadyIncluded;

    bool bTryAgain;
    do
    {
        CPLXMLNode *psLast = nullptr;
        bTryAgain = false;

        CPLXMLNode *psThis = psSchemaNode->psChild;
        for( ; psThis != nullptr; psThis = psThis->psNext )
        {
            if( psThis->eType == CXT_Element &&
                EQUAL(psThis->pszValue, "include") )
            {
                const char *pszSchemaLocation =
                    CPLGetXMLValue(psThis, "schemaLocation", nullptr);
                if( pszSchemaLocation != nullptr &&
                    osAlreadyIncluded.count( pszSchemaLocation) == 0 )
                {
                    osAlreadyIncluded.insert( pszSchemaLocation );

                    if( !STARTS_WITH(pszSchemaLocation, "http://") &&
                        !STARTS_WITH(pszSchemaLocation, "https://") &&
                        CPLIsFilenameRelative(pszSchemaLocation) )
                    {
                        pszSchemaLocation =
                            CPLFormFilename(CPLGetPath(pszMainSchemaLocation),
                                            pszSchemaLocation, nullptr);
                    }

                    CPLXMLNode *psIncludedXSDTree =
                                GMLParseXMLFile( pszSchemaLocation );
                    if( psIncludedXSDTree != nullptr )
                    {
                        CPLStripXMLNamespace(psIncludedXSDTree, nullptr, TRUE);
                        CPLXMLNode *psIncludedSchemaNode =
                            CPLGetXMLNode(psIncludedXSDTree, "=schema");
                        if( psIncludedSchemaNode != nullptr )
                        {
                            // Substitute de <include> node by its content.
                            CPLXMLNode *psFirstChildElement =
                                CPLGetFirstChildNode(psIncludedSchemaNode);
                            if( psFirstChildElement != nullptr )
                            {
                                CPLXMLNode *psCopy =
                                    CPLCloneXMLTree(psFirstChildElement);
                                if( psLast != nullptr )
                                    psLast->psNext = psCopy;
                                else
                                    psSchemaNode->psChild = psCopy;
                                CPLXMLNode *psNext = psThis->psNext;
                                psThis->psNext = nullptr;
                                CPLDestroyXMLNode(psThis);
                                psThis = CPLGetLastNode(psCopy);
                                psThis->psNext = psNext;

                                // In case the included schema also contains
                                // includes.
                                bTryAgain = true;
                            }
                        }
                        CPLDestroyXMLNode(psIncludedXSDTree);
                    }
                }
            }

            psLast = psThis;
        }
    } while( bTryAgain );

    const char *pszSchemaOutputName =
        CPLGetConfigOption("GML_SCHEMA_OUTPUT_NAME", nullptr);
    if( pszSchemaOutputName != nullptr )
    {
        CPLSerializeXMLTreeToFile(psSchemaNode, pszSchemaOutputName);
    }
}

/************************************************************************/
/*                       GetUniqueConstraints()                         */
/************************************************************************/

static std::set<std::pair<std::string, std::string>>
                                GetUniqueConstraints(const CPLXMLNode* psNode)
{
    /* Parse
        <xs:unique name="uniqueConstraintpolyeas_id">
            <xs:selector xpath="ogr:featureMember/ogr:poly"/>
            <xs:field xpath="ogr:eas_id"/>
        </xs:unique>
    */
    std::set<std::pair<std::string, std::string>> oSet;
    for( const auto* psIter= psNode->psChild; psIter != nullptr; psIter = psIter->psNext )
    {
        if( psIter->eType == CXT_Element &&
            EQUAL(psIter->pszValue, "unique") )
        {
            const char* pszSelector = CPLGetXMLValue(psIter, "selector.xpath", nullptr);
            const char* pszField = CPLGetXMLValue(psIter, "field.xpath", nullptr);
            if( pszSelector && pszField && pszField[0] != '@' )
            {
                const char* pszSlash = strchr(pszSelector, '/');
                if( pszSlash )
                {
                    oSet.insert(std::pair<std::string,std::string>(
                        StripNS(pszSlash+1), StripNS(pszField)));
                }
            }
        }
    }
    return oSet;
}

/************************************************************************/
/*                          GMLParseXSD()                               */
/************************************************************************/

bool GMLParseXSD( const char *pszFile,
                 std::vector<GMLFeatureClass*> &aosClasses,
                 bool &bFullyUnderstood)

{
    bFullyUnderstood = false;

    if( pszFile == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      Load the raw XML file.                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psXSDTree = GMLParseXMLFile(pszFile);

    if( psXSDTree == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      Strip off any namespace qualifiers.                             */
/* -------------------------------------------------------------------- */
    CPLStripXMLNamespace( psXSDTree, nullptr, TRUE );

/* -------------------------------------------------------------------- */
/*      Find <schema> root element.                                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psSchemaNode = CPLGetXMLNode(psXSDTree, "=schema");
    if( psSchemaNode == nullptr )
    {
        CPLDestroyXMLNode( psXSDTree );
        return false;
    }

/* ==================================================================== */
/*      Process each include directive.                                 */
/* ==================================================================== */
    CPLXMLSchemaResolveInclude(pszFile, psSchemaNode);

    // CPLSerializeXMLTreeToFile(psSchemaNode, "/vsistdout/");

    bFullyUnderstood = true;

/* ==================================================================== */
/*      Process each feature class definition.                          */
/* ==================================================================== */
    CPLXMLNode *psThis = psSchemaNode->psChild;

    std::set<std::pair<std::string, std::string>> oSetUniqueConstraints;

    for( ; psThis != nullptr; psThis = psThis->psNext )
    {
/* -------------------------------------------------------------------- */
/*      Check for <xs:element> node.                                    */
/* -------------------------------------------------------------------- */
        if( psThis->eType != CXT_Element
            || !EQUAL(psThis->pszValue, "element") )
            continue;

/* -------------------------------------------------------------------- */
/*      Get name                                                        */
/* -------------------------------------------------------------------- */
        const char *pszName = CPLGetXMLValue(psThis, "name", nullptr);
        if( pszName == nullptr )
        {
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Check the substitution group.                                   */
/* -------------------------------------------------------------------- */
        const char *pszSubGroup =
            StripNS(CPLGetXMLValue(psThis, "substitutionGroup", ""));

        if( EQUAL(pszName, "FeatureCollection") &&
            (EQUAL(pszSubGroup, "_FeatureCollection") ||
             EQUAL(pszSubGroup, "_GML") ||
             EQUAL(pszSubGroup, "AbstractFeature")) )
        {
            oSetUniqueConstraints = GetUniqueConstraints(psThis);
            continue;
        }

        // AbstractFeature used by GML 3.2.
        if( !EQUAL(pszSubGroup, "_Feature") &&
            !EQUAL(pszSubGroup, "AbstractFeature") )
        {
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Get type and verify relationship with name.                     */
/* -------------------------------------------------------------------- */
        const char *pszType = CPLGetXMLValue(psThis, "type", nullptr);
        if (pszType == nullptr)
        {
            CPLXMLNode *psComplexType = CPLGetXMLNode(psThis, "complexType");
            if (psComplexType)
            {
                GMLFeatureClass *poClass =
                    GMLParseFeatureType(psSchemaNode, pszName, psComplexType);
                if (poClass)
                    aosClasses.push_back(poClass);
                else
                    bFullyUnderstood = false;
            }
            continue;
        }
        if( strstr(pszType, ":") != nullptr )
            pszType = strstr(pszType, ":") + 1;
        if( EQUAL(pszType, pszName) )
        {
            // A few WFS servers return a type name which is the element name
            // without any _Type or Type suffix
            // e.g.:
            // http://apollo.erdas.com/erdas-apollo/vector/Cherokee?SERVICE=WFS&VERSION=1.0.0&REQUEST=DescribeFeatureType&TYPENAME=iwfs:Air */

            // TODO(schwehr): What was supposed to go here?
        }

        // <element name="RekisteriyksikonPalstanTietoja" type="ktjkiiwfs:PalstanTietojaType" substitutionGroup="gml:_Feature" />
        else if( strlen(pszType) > 4 &&
                 strcmp(pszType + strlen(pszType) - 4, "Type") == 0 &&
                 strlen(pszName) > strlen(pszType) - 4 &&
                 strncmp(pszName + strlen(pszName) - (strlen(pszType) - 4),
                         pszType,
                         strlen(pszType) - 4) == 0 )        {
        }

        else if( !EQUALN(pszType, pszName, strlen(pszName))
            || !(EQUAL(pszType + strlen(pszName), "_Type") ||
                    EQUAL(pszType + strlen(pszName), "Type") ||
                    EQUAL(pszType + strlen(pszName), "FeatureType")) )
        {
            continue;
        }

        // CanVec .xsd contains weird types that are not used in the related
        // GML.
        if (STARTS_WITH(pszName, "XyZz") ||
            STARTS_WITH(pszName, "XyZ1") ||
            STARTS_WITH(pszName, "XyZ2"))
            continue;

        GMLFeatureClass *poClass =
            GMLParseFeatureType(psSchemaNode, pszName, pszType);
        if (poClass)
            aosClasses.push_back(poClass);
        else
            bFullyUnderstood = false;
    }

    CPLDestroyXMLNode(psXSDTree);

    // Attach unique constraints to fields
    for( const auto& typeFieldPair: oSetUniqueConstraints )
    {
        for( const auto* poClass: aosClasses )
        {
            if( poClass->GetName() == typeFieldPair.first )
            {
                auto poProperty = poClass->GetProperty(typeFieldPair.second.c_str());
                if( poProperty )
                {
                    poProperty->SetUnique(true);
                }
                break;
            }
        }
    }

    return !aosClasses.empty();
}
