/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLParseXSD()
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "parsexsd.h"
#include "cpl_error.h"
#include "cpl_conv.h"
#include "ogr_core.h"
#include "cpl_string.h"
#include "cpl_http.h"
#include <set>

/************************************************************************/
/*                              StripNS()                               */
/*                                                                      */
/*      Return potentially shortened form of string with namespace      */
/*      stripped off if there is one.  Returns pointer into             */
/*      original string.                                                */
/************************************************************************/

const char *StripNS( const char *pszFullValue )

{
    const char *pszColon = strstr( pszFullValue, ":" );
    if( pszColon == NULL )
        return pszFullValue;
    else
        return pszColon + 1;
}

/************************************************************************/
/*                   GetSimpleTypeProperties()                          */
/************************************************************************/

static
int GetSimpleTypeProperties(CPLXMLNode *psTypeNode,
                            GMLPropertyType *pGMLType,
                            int *pnWidth,
                            int *pnPrecision)
{
    const char *pszBase =
            StripNS( CPLGetXMLValue( psTypeNode,
                                        "restriction.base", "" ));

    if( EQUAL(pszBase,"decimal") )
    {
        *pGMLType = GMLPT_Real;
        const char *pszWidth =
            CPLGetXMLValue( psTypeNode,
                        "restriction.totalDigits.value", "0" );
        const char *pszPrecision =
            CPLGetXMLValue( psTypeNode,
                        "restriction.fractionDigits.value", "0" );
        *pnWidth = atoi(pszWidth);
        *pnPrecision = atoi(pszPrecision);
        return TRUE;
    }
    
     else if( EQUAL(pszBase,"float") )
    {
        *pGMLType = GMLPT_Float;
        return TRUE;
    }

    else if( EQUAL(pszBase,"double") )
    {
        *pGMLType = GMLPT_Real;
        return TRUE;
    }

    else if( EQUAL(pszBase,"integer") )
    {
        *pGMLType = GMLPT_Integer;
        const char *pszWidth =
            CPLGetXMLValue( psTypeNode,
                        "restriction.totalDigits.value", "0" );
        *pnWidth = atoi(pszWidth);
        return TRUE;
    }

    else if( EQUAL(pszBase,"long") )
    {
        *pGMLType = GMLPT_Integer64;
        const char *pszWidth =
            CPLGetXMLValue( psTypeNode,
                        "restriction.totalDigits.value", "0" );
        *pnWidth = atoi(pszWidth);
        return TRUE;
    }

    else if( EQUAL(pszBase,"long") )
    {
        *pGMLType = GMLPT_Integer64;
        const char *pszWidth =
            CPLGetXMLValue( psTypeNode,
                        "restriction.totalDigits.value", "0" );
        *pnWidth = atoi(pszWidth);
        return TRUE;
    }

    else if( EQUAL(pszBase,"string") )
    {
        *pGMLType = GMLPT_String;
        const char *pszWidth =
            CPLGetXMLValue( psTypeNode,
                        "restriction.maxLength.value", "0" );
        *pnWidth = atoi(pszWidth);
        return TRUE;
    }

    /* TODO: Would be nice to have a proper date type */
    else if( EQUAL(pszBase,"date") ||
             EQUAL(pszBase,"dateTime") )
    {
        *pGMLType = GMLPT_String;
        return TRUE;
    }

    else if( EQUAL(pszBase,"boolean") )
    {
        *pGMLType = GMLPT_Boolean;
        return TRUE;
    }

    else if( EQUAL(pszBase,"short") )
    {
        *pGMLType = GMLPT_Short;
        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                      LookForSimpleType()                             */
/************************************************************************/

static
int LookForSimpleType(CPLXMLNode *psSchemaNode,
                      const char* pszStrippedNSType,
                      GMLPropertyType *pGMLType,
                      int *pnWidth,
                      int *pnPrecision)
{
    CPLXMLNode *psThis;
    for( psThis = psSchemaNode->psChild;
         psThis != NULL; psThis = psThis->psNext )
    {
        if( psThis->eType == CXT_Element
           && EQUAL(psThis->pszValue,"simpleType")
           && EQUAL(CPLGetXMLValue(psThis,"name",""),pszStrippedNSType) )
        {
            break;
        }
    }
    if (psThis == NULL)
        return FALSE;

    return GetSimpleTypeProperties(psThis, pGMLType, pnWidth, pnPrecision);
}

/************************************************************************/
/*                      GetSingleChildElement()                         */
/************************************************************************/

/* Returns the child element whose name is pszExpectedValue only if */
/* there is only one child that is an element. */
static
CPLXMLNode* GetSingleChildElement(CPLXMLNode* psNode, const char* pszExpectedValue)
{
    CPLXMLNode* psChild = NULL;
    CPLXMLNode* psIter;

    if( psNode == NULL )
        return NULL;

    psIter = psNode->psChild;
    if( psIter == NULL )
        return NULL;
    while( psIter != NULL )
    {
        if( psIter->eType == CXT_Element )
        {
            if( psChild != NULL )
                return NULL;
            if( pszExpectedValue != NULL &&
                strcmp(psIter->pszValue, pszExpectedValue) != 0 )
                return NULL;
            psChild = psIter;
        }
        psIter = psIter->psNext;
    }
    return psChild;
}

/************************************************************************/
/*                      CheckMinMaxOccursCardinality()                  */
/************************************************************************/

static int CheckMinMaxOccursCardinality(CPLXMLNode* psNode)
{
    const char* pszMinOccurs = CPLGetXMLValue( psNode, "minOccurs", NULL );
    const char* pszMaxOccurs = CPLGetXMLValue( psNode, "maxOccurs", NULL );
    return (pszMinOccurs == NULL || EQUAL(pszMinOccurs, "0") ||
            EQUAL(pszMinOccurs, "1")) &&
           (pszMaxOccurs == NULL || EQUAL(pszMaxOccurs, "1"));
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
    const char* pszName;
    OGRwkbGeometryType eType;
} AssocNameType;

static const AssocNameType apsPropertyTypes [] =
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
    {NULL, wkbUnknown},
};

/* Found in FME .xsd  (e.g. <element ref="gml:curveProperty" minOccurs="0"/>) */
static const AssocNameType apsRefTypes [] =
{
    {"pointProperty", wkbPoint},
    {"curveProperty", wkbLineString}, /* should we promote to wkbCompoundCurve ? */
    {"surfaceProperty", wkbPolygon},  /* should we promote to wkbCurvePolygon ? */
    {"multiPointProperty", wkbMultiPoint},
    {"multiCurveProperty", wkbMultiLineString},
    {"multiSurfaceProperty", wkbMultiPolygon}, /* should we promote to wkbMultiSurface ? */
    {NULL, wkbUnknown},
};

static
GMLFeatureClass* GMLParseFeatureType(CPLXMLNode *psSchemaNode,
                                     const char* pszName,
                                     CPLXMLNode *psThis);

static
GMLFeatureClass* GMLParseFeatureType(CPLXMLNode *psSchemaNode,
                                const char* pszName,
                                const char *pszType)
{
    CPLXMLNode *psThis;
    for( psThis = psSchemaNode->psChild;
         psThis != NULL; psThis = psThis->psNext )
    {
        if( psThis->eType == CXT_Element
           && EQUAL(psThis->pszValue,"complexType")
           && EQUAL(CPLGetXMLValue(psThis,"name",""),pszType) )
        {
            break;
        }
    }
    if (psThis == NULL)
        return NULL;

    return GMLParseFeatureType(psSchemaNode, pszName, psThis);
}


static
GMLFeatureClass* GMLParseFeatureType(CPLXMLNode *psSchemaNode,
                                     const char* pszName,
                                     CPLXMLNode *psComplexType)
{

/* -------------------------------------------------------------------- */
/*      Grab the sequence of extensions greatgrandchild.                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psAttrSeq =
        CPLGetXMLNode( psComplexType,
                        "complexContent.extension.sequence" );

    if( psAttrSeq == NULL )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      We are pretty sure this going to be a valid Feature class       */
/*      now, so create it.                                              */
/* -------------------------------------------------------------------- */
    GMLFeatureClass *poClass = new GMLFeatureClass( pszName );

/* -------------------------------------------------------------------- */
/*      Loop over each of the attribute elements being defined for      */
/*      this feature class.                                             */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psAttrDef;
    int nAttributeIndex = 0;

    int bGotUnrecognizedType = FALSE;

    for( psAttrDef = psAttrSeq->psChild;
            psAttrDef != NULL;
            psAttrDef = psAttrDef->psNext )
    {
        if( strcmp(psAttrDef->pszValue,"group") == 0 )
        {
            /* Too complex schema for us. Aborts parsing */
            delete poClass;
            return NULL;
        }
        
        /* Parse stuff like :
        <xs:choice>
            <xs:element ref="gml:polygonProperty"/>
            <xs:element ref="gml:multiPolygonProperty"/>
        </xs:choice>
        as found in https://downloadagiv.blob.core.windows.net/overstromingsgebieden-en-oeverzones/2014_01/Overstromingsgebieden_en_oeverzones_2014_01_GML.zip
        */
        if( strcmp(psAttrDef->pszValue,"choice") == 0 )
        {
            CPLXMLNode* psChild = psAttrDef->psChild;
            int bPolygon = FALSE;
            int bMultiPolygon = FALSE;
            for( ; psChild; psChild = psChild->psNext )
            {
                if( psChild->eType != CXT_Element )
                    continue;
                if( strcmp(psChild->pszValue,"element") == 0 )
                {
                    const char* pszRef = CPLGetXMLValue( psChild, "ref", NULL );
                    if( pszRef != NULL )
                    {
                        if( strcmp(pszRef, "gml:polygonProperty") == 0 )
                            bPolygon = TRUE;
                        else if( strcmp(pszRef, "gml:multiPolygonProperty") == 0 )
                            bMultiPolygon = TRUE;
                        else
                        {
                            delete poClass;
                            return NULL;
                        }
                    }
                    else
                    {
                        delete poClass;
                        return NULL;
                    }
                }
            }
            if( bPolygon && bMultiPolygon )
            {
                poClass->AddGeometryProperty( new GMLGeometryPropertyDefn(
                    "", "", wkbMultiPolygon, nAttributeIndex, TRUE ) );

                nAttributeIndex ++;
            }
            continue;
        }

        if( !EQUAL(psAttrDef->pszValue,"element") )
            continue;

        /* MapServer WFS writes element type as an attribute of element */
        /* not as a simpleType definition */
        const char* pszType = CPLGetXMLValue( psAttrDef, "type", NULL );
        const char* pszElementName = CPLGetXMLValue( psAttrDef, "name", NULL );
        int bNullable = EQUAL(CPLGetXMLValue( psAttrDef, "minOccurs", "1" ), "0");
        const char* pszMaxOccurs = CPLGetXMLValue( psAttrDef, "maxOccurs", NULL );
        if (pszType != NULL)
        {
            const char* pszStrippedNSType = StripNS(pszType);
            int nWidth = 0, nPrecision = 0;

            GMLPropertyType gmlType = GMLPT_Untyped;
            if (EQUAL(pszStrippedNSType, "string") ||
                EQUAL(pszStrippedNSType, "Character"))
                gmlType = GMLPT_String;
            /* TODO: Would be nice to have a proper date type */
            else if (EQUAL(pszStrippedNSType, "date") ||
                     EQUAL(pszStrippedNSType, "dateTime"))
                gmlType = GMLPT_String;
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
            else if (EQUAL(pszStrippedNSType, "short") )
                gmlType = GMLPT_Short;
            else if (EQUAL(pszStrippedNSType, "boolean") )
                gmlType = GMLPT_Boolean;
            else if (strcmp(pszType, "gml:FeaturePropertyType") == 0 )
            {
                gmlType = GMLPT_FeatureProperty;
            }
            else if (strncmp(pszType, "gml:", 4) == 0)
            {
                const AssocNameType* psIter = apsPropertyTypes;
                while(psIter->pszName)
                {
                    if (strncmp(pszType + 4, psIter->pszName, strlen(psIter->pszName)) == 0)
                    {
                        OGRwkbGeometryType eType = psIter->eType;

                        /* Look if there's a comment restricting to subclasses */
                        if( psAttrDef->psNext != NULL && psAttrDef->psNext->eType == CXT_Comment )
                        {
                            if( strstr(psAttrDef->psNext->pszValue, "restricted to Polygon") )
                                eType = wkbPolygon;
                            else if( strstr(psAttrDef->psNext->pszValue, "restricted to LineString") )
                                eType = wkbLineString;
                            else if( strstr(psAttrDef->psNext->pszValue, "restricted to MultiPolygon") )
                                eType = wkbMultiPolygon;
                            else if( strstr(psAttrDef->psNext->pszValue, "restricted to MultiLineString") )
                                eType = wkbMultiLineString;
                        }

                        poClass->AddGeometryProperty( new GMLGeometryPropertyDefn(
                            pszElementName, pszElementName, eType, nAttributeIndex, bNullable ) );

                        nAttributeIndex ++;

                        break;
                    }

                    psIter ++;
                }

                if (psIter->pszName == NULL)
                {
                    /* Can be a non geometry gml type */
                    /* Too complex schema for us. Aborts parsing */
                    delete poClass;
                    return NULL;
                }

                if (poClass->GetGeometryPropertyCount() == 0)
                    bGotUnrecognizedType = TRUE;

                continue;
            }

            /* Integraph stuff */
            else if (strcmp(pszType, "G:Point_MultiPointPropertyType") == 0 ||
                     strcmp(pszType, "gmgml:Point_MultiPointPropertyType") == 0)
            {
                poClass->AddGeometryProperty( new GMLGeometryPropertyDefn(
                    pszElementName, pszElementName, wkbMultiPoint, nAttributeIndex,
                    bNullable ) );

                nAttributeIndex ++;
                continue;
            }
            else if (strcmp(pszType, "G:LineString_MultiLineStringPropertyType") == 0 ||
                     strcmp(pszType, "gmgml:LineString_MultiLineStringPropertyType") == 0)
            {
                poClass->AddGeometryProperty( new GMLGeometryPropertyDefn(
                    pszElementName, pszElementName, wkbMultiLineString, nAttributeIndex,
                    bNullable ) );

                nAttributeIndex ++;
                continue;
            }
            else if (strcmp(pszType, "G:Polygon_MultiPolygonPropertyType") == 0 ||
                     strcmp(pszType, "gmgml:Polygon_MultiPolygonPropertyType") == 0 ||
                     strcmp(pszType, "gmgml:Polygon_Surface_MultiSurface_CompositeSurfacePropertyType") == 0)
            {
                poClass->AddGeometryProperty( new GMLGeometryPropertyDefn(
                    pszElementName, pszElementName, wkbMultiPolygon, nAttributeIndex,
                    bNullable ) );

                nAttributeIndex ++;
                continue;
            }

            /* ERDAS Apollo stuff (like in http://apollo.erdas.com/erdas-apollo/vector/WORLDWIDE?SERVICE=WFS&VERSION=1.0.0&REQUEST=DescribeFeatureType&TYPENAME=wfs:cntry98) */
            else if (strcmp(pszType, "wfs:MixedPolygonPropertyType") == 0)
            {
                poClass->AddGeometryProperty( new GMLGeometryPropertyDefn(
                    pszElementName, pszElementName, wkbMultiPolygon, nAttributeIndex,
                    bNullable) );

                nAttributeIndex ++;
                continue;
            }

            else
            {
                gmlType = GMLPT_Untyped;
                if ( ! LookForSimpleType(psSchemaNode, pszStrippedNSType,
                                         &gmlType, &nWidth, &nPrecision) )
                {
                    /* Too complex schema for us. Aborts parsing */
                    delete poClass;
                    return NULL;
                }
            }

            if (pszElementName == NULL)
                pszElementName = "unnamed";
            const char* pszPropertyName = pszElementName;
            if( gmlType == GMLPT_FeatureProperty )
            {
                pszPropertyName = CPLSPrintf("%s_href", pszElementName);
            }
            GMLPropertyDefn *poProp = new GMLPropertyDefn(
                pszPropertyName, pszElementName );

            if( pszMaxOccurs != NULL && strcmp(pszMaxOccurs, "1") != 0 )
                gmlType = GetListTypeFromSingleType(gmlType);

            poProp->SetType( gmlType );
            poProp->SetWidth( nWidth );
            poProp->SetPrecision( nPrecision );
            poProp->SetNullable( bNullable );

            if (poClass->AddProperty( poProp ) < 0)
                delete poProp;
            else
                nAttributeIndex ++;

            continue;
        }

        // For now we skip geometries .. fixup later.
        CPLXMLNode* psSimpleType = CPLGetXMLNode( psAttrDef, "simpleType" );
        if( psSimpleType == NULL )
        {
            const char* pszRef = CPLGetXMLValue( psAttrDef, "ref", NULL );

            /* FME .xsd */
            if (pszRef != NULL && strncmp(pszRef, "gml:", 4) == 0)
            {
                const AssocNameType* psIter = apsRefTypes;
                while(psIter->pszName)
                {
                    if (strncmp(pszRef + 4, psIter->pszName, strlen(psIter->pszName)) == 0)
                    {
                        if (poClass->GetGeometryPropertyCount() > 0)
                        {
                            OGRwkbGeometryType eNewType = psIter->eType;
                            OGRwkbGeometryType eOldType = (OGRwkbGeometryType)poClass->GetGeometryProperty(0)->GetType();
                            if ((eNewType == wkbMultiPoint && eOldType == wkbPoint) ||
                                (eNewType == wkbMultiLineString && eOldType == wkbLineString) ||
                                (eNewType == wkbMultiPolygon && eOldType == wkbPolygon))
                            {
                                poClass->GetGeometryProperty(0)->SetType(eNewType);
                            }
                            else
                            {
                                CPLDebug("GML", "Geometry field already found ! Ignoring the following ones");
                            }
                        }
                        else
                        {
                            poClass->AddGeometryProperty( new GMLGeometryPropertyDefn(
                                pszElementName, pszElementName, psIter->eType, nAttributeIndex, TRUE ) );

                            nAttributeIndex ++;
                        }

                        break;
                    }

                    psIter ++;
                }

                if (psIter->pszName == NULL)
                {
                    /* Can be a non geometry gml type */
                    /* Too complex schema for us. Aborts parsing */
                    delete poClass;
                    return NULL;
                }

                if (poClass->GetGeometryPropertyCount() == 0)
                    bGotUnrecognizedType = TRUE;

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
            CPLXMLNode* psComplexType = GetSingleChildElement( psAttrDef, "complexType" );
            CPLXMLNode* psComplexTypeSequence = GetSingleChildElement( psComplexType, "sequence" );
            CPLXMLNode* psComplexTypeSequenceElement = GetSingleChildElement( psComplexTypeSequence, "element" );

            if( pszElementName != NULL &&
                CheckMinMaxOccursCardinality(psAttrDef) &&
                psComplexTypeSequenceElement != NULL &&
                CheckMinMaxOccursCardinality(psComplexTypeSequence) &&
                strcmp(CPLGetXMLValue( psComplexTypeSequenceElement, "ref", "" ), "gml:_Geometry") == 0 )
            {
                poClass->AddGeometryProperty( new GMLGeometryPropertyDefn(
                    pszElementName, pszElementName, wkbUnknown, nAttributeIndex, bNullable ) );

                nAttributeIndex ++;

                continue;
            }
            else
            {
                /* Too complex schema for us. Aborts parsing */
                delete poClass;
                return NULL;
            }
        }

        if (pszElementName == NULL)
            pszElementName = "unnamed";
        GMLPropertyDefn *poProp = new GMLPropertyDefn(
            pszElementName, pszElementName );

        GMLPropertyType eType = GMLPT_Untyped;
        int nWidth = 0, nPrecision = 0;
        GetSimpleTypeProperties(psSimpleType, &eType, &nWidth, &nPrecision);

        if( pszMaxOccurs != NULL && strcmp(pszMaxOccurs, "1") != 0 )
            eType = GetListTypeFromSingleType(eType);

        poProp->SetType( eType );
        poProp->SetWidth( nWidth );
        poProp->SetPrecision( nPrecision );
        poProp->SetNullable( bNullable );

        if (poClass->AddProperty( poProp ) < 0)
            delete poProp;
        else
            nAttributeIndex ++;
    }

    /* If we have found an unknown types, let's be on the side of caution and */
    /* create a geometry field */
    if( poClass->GetGeometryPropertyCount() == 0 &&
        bGotUnrecognizedType )
    {
        poClass->AddGeometryProperty( new GMLGeometryPropertyDefn( "", "", wkbUnknown, -1, TRUE ) );
    }

/* -------------------------------------------------------------------- */
/*      Class complete, add to reader class list.                       */
/* -------------------------------------------------------------------- */
    poClass->SetSchemaLocked( TRUE );

    return poClass;
}

/************************************************************************/
/*                         GMLParseXMLFile()                            */
/************************************************************************/

static
CPLXMLNode* GMLParseXMLFile(const char* pszFilename)
{
    if( strncmp(pszFilename, "http://", 7) == 0 ||
        strncmp(pszFilename, "https://", 8) == 0 )
    {
        CPLXMLNode* psRet = NULL;
        CPLHTTPResult* psResult = CPLHTTPFetch( pszFilename, NULL );
        if( psResult != NULL )
        {
            if( psResult->pabyData != NULL )
            {
                psRet = CPLParseXMLString( (const char*) psResult->pabyData );
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

static
CPLXMLNode* CPLGetFirstChildNode( CPLXMLNode* psNode )
{
    if( psNode == NULL )
        return NULL;
    CPLXMLNode* psIter = psNode->psChild;
    while( psIter != NULL )
    {
        if( psIter->eType == CXT_Element )
            return psIter;
        psIter = psIter->psNext;
    }
    return NULL;
}

/************************************************************************/
/*                          CPLGetLastNode()                            */
/************************************************************************/

static
CPLXMLNode* CPLGetLastNode( CPLXMLNode* psNode )
{
    if( psNode == NULL )
        return NULL;
    CPLXMLNode* psIter = psNode;
    while( psIter->psNext != NULL )
        psIter = psIter->psNext;
    return psIter;
}

/************************************************************************/
/*                       CPLXMLSchemaResolveInclude()                   */
/************************************************************************/

static
void CPLXMLSchemaResolveInclude( const char* pszMainSchemaLocation,
                                 CPLXMLNode *psSchemaNode )
{
    std::set<CPLString> osAlreadyIncluded;

    int bTryAgain;
    do
    {
        CPLXMLNode *psThis;
        CPLXMLNode *psLast = NULL;
        bTryAgain = FALSE;

        for( psThis = psSchemaNode->psChild; 
            psThis != NULL; psThis = psThis->psNext )
        {
            if( psThis->eType == CXT_Element &&
                EQUAL(psThis->pszValue,"include") )
            {
                const char* pszSchemaLocation = 
                        CPLGetXMLValue(psThis, "schemaLocation", NULL);
                if( pszSchemaLocation != NULL &&
                    osAlreadyIncluded.count( pszSchemaLocation) == 0 )
                {
                    osAlreadyIncluded.insert( pszSchemaLocation );

                    if( strncmp(pszSchemaLocation, "http://", 7) != 0 &&
                        strncmp(pszSchemaLocation, "https://", 8) != 0 &&
                        CPLIsFilenameRelative(pszSchemaLocation ) )
                    {
                        pszSchemaLocation = CPLFormFilename(
                            CPLGetPath(pszMainSchemaLocation), pszSchemaLocation, NULL );
                    }

                    CPLXMLNode *psIncludedXSDTree =
                                GMLParseXMLFile( pszSchemaLocation );
                    if( psIncludedXSDTree != NULL )
                    {
                        CPLStripXMLNamespace( psIncludedXSDTree, NULL, TRUE );
                        CPLXMLNode *psIncludedSchemaNode =
                                CPLGetXMLNode( psIncludedXSDTree, "=schema" );
                        if( psIncludedSchemaNode != NULL )
                        {
                            /* Substitute de <include> node by its content */
                            CPLXMLNode* psFirstChildElement =
                                CPLGetFirstChildNode(psIncludedSchemaNode);
                            if( psFirstChildElement != NULL )
                            {
                                CPLXMLNode* psCopy = CPLCloneXMLTree(psFirstChildElement);
                                if( psLast != NULL )
                                    psLast->psNext = psCopy;
                                else
                                    psSchemaNode->psChild = psCopy;
                                CPLXMLNode* psNext = psThis->psNext;
                                psThis->psNext = NULL;
                                CPLDestroyXMLNode(psThis);
                                psThis = CPLGetLastNode(psCopy);
                                psThis->psNext = psNext;

                                /* In case the included schema also contains */
                                /* includes */
                                bTryAgain = TRUE;
                            }

                        }
                        CPLDestroyXMLNode( psIncludedXSDTree );
                    }
                }
            }

            psLast = psThis;
        }
    } while( bTryAgain );

    const char* pszSchemaOutputName =
        CPLGetConfigOption("GML_SCHEMA_OUTPUT_NAME", NULL);
    if( pszSchemaOutputName != NULL )
    {
        CPLSerializeXMLTreeToFile( psSchemaNode, pszSchemaOutputName );
    }
}

/************************************************************************/
/*                          GMLParseXSD()                               */
/************************************************************************/

int GMLParseXSD( const char *pszFile,
                 std::vector<GMLFeatureClass*> & aosClasses)

{
    if( pszFile == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Load the raw XML file.                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psXSDTree = GMLParseXMLFile( pszFile );
    
    if( psXSDTree == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Strip off any namespace qualifiers.                             */
/* -------------------------------------------------------------------- */
    CPLStripXMLNamespace( psXSDTree, NULL, TRUE );

/* -------------------------------------------------------------------- */
/*      Find <schema> root element.                                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psSchemaNode = CPLGetXMLNode( psXSDTree, "=schema" );
    if( psSchemaNode == NULL )
    {
        CPLDestroyXMLNode( psXSDTree );
        return FALSE;
    }

/* ==================================================================== */
/*      Process each include directive.                                 */
/* ==================================================================== */
    CPLXMLSchemaResolveInclude( pszFile, psSchemaNode );

    //CPLSerializeXMLTreeToFile(psSchemaNode, "/vsistdout/");

/* ==================================================================== */
/*      Process each feature class definition.                          */
/* ==================================================================== */
    CPLXMLNode *psThis;

    for( psThis = psSchemaNode->psChild; 
         psThis != NULL; psThis = psThis->psNext )
    {
/* -------------------------------------------------------------------- */
/*      Check for <xs:element> node.                                    */
/* -------------------------------------------------------------------- */
        if( psThis->eType != CXT_Element 
            || !EQUAL(psThis->pszValue,"element") )
            continue;

/* -------------------------------------------------------------------- */
/*      Check the substitution group.                                   */
/* -------------------------------------------------------------------- */
        const char *pszSubGroup = 
            StripNS(CPLGetXMLValue(psThis,"substitutionGroup",""));

        // Old OGR produced elements for the feature collection.
        if( EQUAL(pszSubGroup, "_FeatureCollection") )
            continue;

        if( !EQUAL(pszSubGroup, "_Feature") &&
            !EQUAL(pszSubGroup, "AbstractFeature") /* AbstractFeature used by GML 3.2 */ )
        {
            continue;
        }
        
/* -------------------------------------------------------------------- */
/*      Get name                                                        */
/* -------------------------------------------------------------------- */
        const char *pszName;

        pszName = CPLGetXMLValue( psThis, "name", NULL );
        if( pszName == NULL )
        {
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Get type and verify relationship with name.                     */
/* -------------------------------------------------------------------- */
        const char *pszType;

        pszType = CPLGetXMLValue( psThis, "type", NULL );
        if (pszType == NULL)
        {
            CPLXMLNode *psComplexType = CPLGetXMLNode( psThis, "complexType" );
            if (psComplexType)
            {
                GMLFeatureClass* poClass =
                        GMLParseFeatureType(psSchemaNode, pszName, psComplexType);
                if (poClass)
                    aosClasses.push_back(poClass);
            }
            continue;
        }
        if( strstr( pszType, ":" ) != NULL )
            pszType = strstr( pszType, ":" ) + 1;
        if( EQUAL(pszType, pszName) )
        {
            /* A few WFS servers return a type name which is the element name */
            /* without any _Type or Type suffix */
            /* e.g. : http://apollo.erdas.com/erdas-apollo/vector/Cherokee?SERVICE=WFS&VERSION=1.0.0&REQUEST=DescribeFeatureType&TYPENAME=iwfs:Air */
        }

        /* <element name="RekisteriyksikonPalstanTietoja" type="ktjkiiwfs:PalstanTietojaType" substitutionGroup="gml:_Feature" /> */
        else if(  strlen(pszType) > 4 &&
                  strcmp(pszType + strlen(pszType) - 4, "Type") == 0 &&
                  strlen(pszName) > strlen(pszType) - 4 &&
                  strncmp(pszName + strlen(pszName) - (strlen(pszType) - 4),
                          pszType,
                          strlen(pszType) - 4) == 0 )
        {
        }

        else if( !EQUALN(pszType,pszName,strlen(pszName))
            || !(EQUAL(pszType+strlen(pszName),"_Type") ||
                    EQUAL(pszType+strlen(pszName),"Type")) )
        {
            continue;
        }

        /* CanVec .xsd contains weird types that are not used in the related GML */
        if (strncmp(pszName, "XyZz", 4) == 0 ||
            strncmp(pszName, "XyZ1", 4) == 0 ||
            strncmp(pszName, "XyZ2", 4) == 0)
            continue;

        GMLFeatureClass* poClass =
                GMLParseFeatureType(psSchemaNode, pszName, pszType);
        if (poClass)
            aosClasses.push_back(poClass);
    }

    CPLDestroyXMLNode( psXSDTree );

    if( aosClasses.size() > 0 )
    {
        return TRUE;
    }
    else
        return FALSE;
}
