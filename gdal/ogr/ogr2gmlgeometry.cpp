/******************************************************************************
 *
 * Project:  GML Translator
 * Purpose:  Code to translate OGRGeometry to GML string representation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 *****************************************************************************
 *
 * Independent Security Audit 2003/04/17 Andrey Kiselev:
 *   Completed audit of this module. All functions may be used without buffer
 *   overflows and stack corruptions if caller could be trusted.
 *
 * Security Audit 2003/03/28 warmerda:
 *   Completed security audit.  I believe that this module may be safely used
 *   to generate GML from arbitrary but well formed OGRGeomety objects that
 *   come from a potentially hostile source, but through a trusted OGR importer
 *   without compromising the system.
 *
 */

#include "cpl_port.h"
#include "ogr_api.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

static const int SRSDIM_LOC_GEOMETRY = 1 << 0;
static const int SRSDIM_LOC_POSLIST = 1 << 1;

typedef enum
{
    SRSNAME_SHORT,
    SRSNAME_OGC_URN,
    SRSNAME_OGC_URL
} GMLSRSNameFormat;

/************************************************************************/
/*                        MakeGMLCoordinate()                           */
/************************************************************************/

static void MakeGMLCoordinate( char *pszTarget,
                               double x, double y, double z, bool b3D )

{
    OGRMakeWktCoordinate( pszTarget, x, y, z, b3D ? 3 : 2 );
    while( *pszTarget != '\0' )
    {
        if( *pszTarget == ' ' )
            *pszTarget = ',';
        pszTarget++;
    }
}

/************************************************************************/
/*                            _GrowBuffer()                             */
/************************************************************************/

static void _GrowBuffer( size_t nNeeded, char **ppszText, size_t *pnMaxLength )

{
    if( nNeeded + 1 >= *pnMaxLength )
    {
        *pnMaxLength = std::max(*pnMaxLength * 2, nNeeded + 1);
        *ppszText = static_cast<char *>(CPLRealloc(*ppszText, *pnMaxLength));
    }
}

/************************************************************************/
/*                            AppendString()                            */
/************************************************************************/

static void AppendString( char **ppszText, size_t *pnLength,
                          size_t *pnMaxLength,
                          const char *pszTextToAppend )

{
    _GrowBuffer( *pnLength + strlen(pszTextToAppend) + 1,
                 ppszText, pnMaxLength );

    strcat( *ppszText + *pnLength, pszTextToAppend );
    *pnLength += strlen( *ppszText + *pnLength );
}

/************************************************************************/
/*                        AppendCoordinateList()                        */
/************************************************************************/

static void AppendCoordinateList( OGRLineString *poLine,
                                  char **ppszText, size_t *pnLength,
                                  size_t *pnMaxLength )

{
    const bool b3D = wkbHasZ(poLine->getGeometryType()) != FALSE;

    *pnLength += strlen(*ppszText + *pnLength);
    _GrowBuffer( *pnLength + 20, ppszText, pnMaxLength );

    strcat( *ppszText + *pnLength, "<gml:coordinates>" );
    *pnLength += strlen(*ppszText + *pnLength);

    char szCoordinate[256] = {};
    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        MakeGMLCoordinate( szCoordinate,
                           poLine->getX(iPoint),
                           poLine->getY(iPoint),
                           poLine->getZ(iPoint),
                           b3D );
        _GrowBuffer( *pnLength + strlen(szCoordinate) + 1,
                     ppszText, pnMaxLength );

        if( iPoint != 0 )
            strcat( *ppszText + *pnLength, " " );

        strcat( *ppszText + *pnLength, szCoordinate );
        *pnLength += strlen(*ppszText + *pnLength);
    }

    _GrowBuffer( *pnLength + 20, ppszText, pnMaxLength );
    strcat( *ppszText + *pnLength, "</gml:coordinates>" );
    *pnLength += strlen(*ppszText + *pnLength);
}

/************************************************************************/
/*                       OGR2GMLGeometryAppend()                        */
/************************************************************************/

static bool OGR2GMLGeometryAppend( OGRGeometry *poGeometry,
                                   char **ppszText, size_t *pnLength,
                                   size_t *pnMaxLength,
                                   bool bIsSubGeometry,
                                   const char* pszNamespaceDecl )

{
/* -------------------------------------------------------------------- */
/*      Check for Spatial Reference System attached to given geometry   */
/* -------------------------------------------------------------------- */

    // Buffer for xmlns:gml and srsName attributes (srsName="...")
    char szAttributes[128] = {};
    size_t nAttrsLength = 0;

    szAttributes[0] = 0;

    const OGRSpatialReference* poSRS = NULL;
    poSRS = poGeometry->getSpatialReference();

    if( pszNamespaceDecl != NULL )
    {
        snprintf( szAttributes + nAttrsLength,
                  sizeof(szAttributes) - nAttrsLength,
                  " xmlns:gml=\"%s\"",
                  pszNamespaceDecl );
        nAttrsLength += strlen(szAttributes + nAttrsLength);
        pszNamespaceDecl = NULL;
    }

    if( NULL != poSRS && !bIsSubGeometry )
    {
        const char* pszTarget = poSRS->IsProjected() ? "PROJCS" : "GEOGCS";
        const char* pszAuthName = poSRS->GetAuthorityName( pszTarget );
        const char* pszAuthCode = poSRS->GetAuthorityCode( pszTarget );
        if( NULL != pszAuthName && strlen(pszAuthName) < 10 &&
            NULL != pszAuthCode && strlen(pszAuthCode) < 10 )
        {
            snprintf( szAttributes + nAttrsLength,
                        sizeof(szAttributes) - nAttrsLength,
                        " srsName=\"%s:%s\"",
                        pszAuthName, pszAuthCode );

            nAttrsLength += strlen(szAttributes + nAttrsLength);
        }
    }

    OGRwkbGeometryType eType = poGeometry->getGeometryType();
    OGRwkbGeometryType eFType = wkbFlatten(eType);

/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( eType == wkbPoint )
    {
        OGRPoint *poPoint = dynamic_cast<OGRPoint *>(poGeometry);
        if( poPoint == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRPoint.");
        }

        char szCoordinate[256] = {};
        MakeGMLCoordinate( szCoordinate,
                           poPoint->getX(), poPoint->getY(), 0.0, false );

        _GrowBuffer( *pnLength + strlen(szCoordinate) + 60 + nAttrsLength,
                     ppszText, pnMaxLength );

        snprintf(
            *ppszText + *pnLength, *pnMaxLength - *pnLength,
            "<gml:Point%s><gml:coordinates>%s</gml:coordinates></gml:Point>",
            szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( eType == wkbPoint25D )
    {
        OGRPoint *poPoint = dynamic_cast<OGRPoint *>(poGeometry);
        if( poPoint == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRPoint.");
        }

        char szCoordinate[256] = {};
        MakeGMLCoordinate( szCoordinate,
                           poPoint->getX(), poPoint->getY(), poPoint->getZ(),
                           true );

        _GrowBuffer( *pnLength + strlen(szCoordinate) + 70 + nAttrsLength,
                     ppszText, pnMaxLength );

        snprintf(
            *ppszText + *pnLength, *pnMaxLength - *pnLength,
            "<gml:Point%s><gml:coordinates>%s</gml:coordinates></gml:Point>",
            szAttributes, szCoordinate);

        *pnLength += strlen( *ppszText + *pnLength );
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbLineString )
    {
        bool bRing = EQUAL(poGeometry->getGeometryName(), "LINEARRING");

        // Buffer for tag name + srsName attribute if set
        const size_t nLineTagLength = 16;
        const size_t nLineTagNameBufLen = nLineTagLength + nAttrsLength + 1;
        char* pszLineTagName =
            static_cast<char *>(CPLMalloc(nLineTagNameBufLen));

        if( bRing )
        {
            snprintf( pszLineTagName, nLineTagNameBufLen,
                      "<gml:LinearRing%s>", szAttributes );

            AppendString( ppszText, pnLength, pnMaxLength,
                          pszLineTagName );
        }
        else
        {
            snprintf( pszLineTagName, nLineTagNameBufLen,
                      "<gml:LineString%s>", szAttributes );

            AppendString( ppszText, pnLength, pnMaxLength,
                          pszLineTagName );
        }

        // Free tag buffer.
        CPLFree( pszLineTagName );

        OGRLineString *poLineString = dynamic_cast<OGRLineString *>(poGeometry);
        if( poLineString == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRLineString.");
        }

        AppendCoordinateList( poLineString, ppszText, pnLength, pnMaxLength );

        if( bRing )
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:LinearRing>" );
        else
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:LineString>" );
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbPolygon )
    {
        OGRPolygon *poPolygon = dynamic_cast<OGRPolygon *>(poGeometry);
        if( poPolygon == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRPolygon.");
        }

        // Buffer for polygon tag name + srsName attribute if set.
        const size_t nPolyTagLength = 13;
        const size_t nPolyTagNameBufLen = nPolyTagLength + nAttrsLength + 1;
        char* pszPolyTagName =
            static_cast<char *>(CPLMalloc(nPolyTagNameBufLen));

        // Compose Polygon tag with or without srsName attribute.
        snprintf( pszPolyTagName, nPolyTagNameBufLen,
                  "<gml:Polygon%s>", szAttributes );

        AppendString( ppszText, pnLength, pnMaxLength,
                      pszPolyTagName );

        // Free tag buffer.
        CPLFree( pszPolyTagName );

        // Don't add srsName to polygon rings.
        if( poPolygon->getExteriorRing() != NULL )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:outerBoundaryIs>" );

            if( !OGR2GMLGeometryAppend( poPolygon->getExteriorRing(),
                                        ppszText, pnLength, pnMaxLength,
                                        true, NULL ) )
            {
                return false;
            }

            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:outerBoundaryIs>" );
        }

        for( int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++ )
        {
            OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:innerBoundaryIs>" );

            if( !OGR2GMLGeometryAppend( poRing, ppszText, pnLength,
                                        pnMaxLength, true, NULL ) )
                return false;

            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:innerBoundaryIs>" );
        }

        AppendString( ppszText, pnLength, pnMaxLength,
                      "</gml:Polygon>" );
    }

/* -------------------------------------------------------------------- */
/*      MultiPolygon, MultiLineString, MultiPoint, MultiGeometry        */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbMultiPolygon
             || eFType == wkbMultiLineString
             || eFType == wkbMultiPoint
             || eFType == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC =
            dynamic_cast<OGRGeometryCollection *>(poGeometry);
        if( poGC == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRGeometryCollection.");
        }
        const char *pszElemClose = NULL;
        const char *pszMemberElem = NULL;

        // Buffer for opening tag + srsName attribute.
        char* pszElemOpen = NULL;

        if( eFType == wkbMultiPolygon )
        {
            const size_t nBufLen = 13 + nAttrsLength + 1;
            pszElemOpen = static_cast<char *>(CPLMalloc( nBufLen ));
            snprintf( pszElemOpen, nBufLen, "MultiPolygon%s>", szAttributes );

            pszElemClose = "MultiPolygon>";
            pszMemberElem = "polygonMember>";
        }
        else if( eFType == wkbMultiLineString )
        {
            const size_t nBufLen = 16 + nAttrsLength + 1;
            pszElemOpen = static_cast<char *>(CPLMalloc( nBufLen ));
            snprintf( pszElemOpen, nBufLen,
                      "MultiLineString%s>", szAttributes );

            pszElemClose = "MultiLineString>";
            pszMemberElem = "lineStringMember>";
        }
        else if( eFType == wkbMultiPoint )
        {
            const size_t nBufLen = 11 + nAttrsLength + 1;
            pszElemOpen = static_cast<char *>(CPLMalloc( nBufLen ));
            snprintf( pszElemOpen, nBufLen, "MultiPoint%s>", szAttributes );

            pszElemClose = "MultiPoint>";
            pszMemberElem = "pointMember>";
        }
        else
        {
            const size_t nBufLen = 19 + nAttrsLength + 1;
            pszElemOpen = static_cast<char *>(CPLMalloc( nBufLen ));
            snprintf( pszElemOpen, nBufLen, "MultiGeometry%s>", szAttributes );

            pszElemClose = "MultiGeometry>";
            pszMemberElem = "geometryMember>";
        }

        AppendString( ppszText, pnLength, pnMaxLength, "<gml:" );
        AppendString( ppszText, pnLength, pnMaxLength, pszElemOpen );

        for( int iMember = 0; iMember < poGC->getNumGeometries(); iMember++)
        {
            OGRGeometry *poMember = poGC->getGeometryRef( iMember );

            AppendString( ppszText, pnLength, pnMaxLength, "<gml:" );
            AppendString( ppszText, pnLength, pnMaxLength, pszMemberElem );

            if( !OGR2GMLGeometryAppend( poMember,
                                        ppszText, pnLength, pnMaxLength,
                                        true, NULL ) )
            {
                return false;
            }

            AppendString( ppszText, pnLength, pnMaxLength, "</gml:" );
            AppendString( ppszText, pnLength, pnMaxLength, pszMemberElem );
        }

        AppendString( ppszText, pnLength, pnMaxLength, "</gml:" );
        AppendString( ppszText, pnLength, pnMaxLength, pszElemClose );

        // Free tag buffer.
        CPLFree( pszElemOpen );
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported geometry type %s",
                 OGRGeometryTypeToName(eType));
        return false;
    }

    return true;
}

/************************************************************************/
/*                   OGR_G_ExportEnvelopeToGMLTree()                    */
/************************************************************************/

/** Export the envelope of a geometry as a gml:Box. */
CPLXMLNode *OGR_G_ExportEnvelopeToGMLTree( OGRGeometryH hGeometry )

{
    OGREnvelope sEnvelope;

    reinterpret_cast<OGRGeometry *>(hGeometry)->getEnvelope( &sEnvelope );

    if( !sEnvelope.IsInit() )
    {
        // TODO: There is apparently a special way of representing a null box
        // geometry. Should use it here eventually.
        return NULL;
    }

    CPLXMLNode *psBox = CPLCreateXMLNode( NULL, CXT_Element, "gml:Box" );

/* -------------------------------------------------------------------- */
/*      Add minxy coordinate.                                           */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psCoord = CPLCreateXMLNode( psBox, CXT_Element, "gml:coord" );

    char szCoordinate[256] = {};
    MakeGMLCoordinate( szCoordinate, sEnvelope.MinX, sEnvelope.MinY, 0.0,
                       false );
    char *pszY = strstr(szCoordinate, ",");
    // There must be more after the comma or we have an internal consistency
    // bug in MakeGMLCoordinate.
    if( pszY == NULL || strlen(pszY) < 2)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "MakeGMLCoordinate failed." );
        return NULL;
    }
    *pszY = '\0';
    pszY++;

    CPLCreateXMLElementAndValue( psCoord, "gml:X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "gml:Y", pszY );

/* -------------------------------------------------------------------- */
/*      Add maxxy coordinate.                                           */
/* -------------------------------------------------------------------- */
    psCoord = CPLCreateXMLNode( psBox, CXT_Element, "gml:coord" );

    MakeGMLCoordinate( szCoordinate, sEnvelope.MaxX, sEnvelope.MaxY, 0.0,
                       false );
    pszY = strstr(szCoordinate, ",") + 1;
    pszY[-1] = '\0';

    CPLCreateXMLElementAndValue( psCoord, "gml:X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "gml:Y", pszY );

    return psBox;
}

/************************************************************************/
/*                     AppendGML3CoordinateList()                       */
/************************************************************************/

static void AppendGML3CoordinateList( const OGRSimpleCurve *poLine,
                                      bool bCoordSwap,
                                      char **ppszText, size_t *pnLength,
                                      size_t *pnMaxLength,
                                      int nSRSDimensionLocFlags )

{
    bool b3D = wkbHasZ(poLine->getGeometryType());

    *pnLength += strlen(*ppszText + *pnLength);
    _GrowBuffer( *pnLength + 40, ppszText, pnMaxLength );

    if( b3D && (nSRSDimensionLocFlags & SRSDIM_LOC_POSLIST) != 0 )
        strcat( *ppszText + *pnLength, "<gml:posList srsDimension=\"3\">" );
    else
        strcat( *ppszText + *pnLength, "<gml:posList>" );
    *pnLength += strlen(*ppszText + *pnLength);

    char szCoordinate[256] = {};

    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        if( bCoordSwap )
            OGRMakeWktCoordinate( szCoordinate,
                                  poLine->getY(iPoint),
                                  poLine->getX(iPoint),
                                  poLine->getZ(iPoint),
                                  b3D ? 3 : 2 );
        else
            OGRMakeWktCoordinate( szCoordinate,
                                  poLine->getX(iPoint),
                                  poLine->getY(iPoint),
                                  poLine->getZ(iPoint),
                                  b3D ? 3 : 2 );
        _GrowBuffer( *pnLength + strlen(szCoordinate)+1,
                     ppszText, pnMaxLength );

        if( iPoint != 0 )
            strcat( *ppszText + *pnLength, " " );

        strcat( *ppszText + *pnLength, szCoordinate );
        *pnLength += strlen(*ppszText + *pnLength);
    }

    _GrowBuffer( *pnLength + 20, ppszText, pnMaxLength );
    strcat( *ppszText + *pnLength, "</gml:posList>" );
    *pnLength += strlen(*ppszText + *pnLength);
}

/************************************************************************/
/*                      OGR2GML3GeometryAppend()                        */
/************************************************************************/

static bool OGR2GML3GeometryAppend( const OGRGeometry *poGeometry,
                                    const OGRSpatialReference* poParentSRS,
                                    char **ppszText, size_t *pnLength,
                                    size_t *pnMaxLength,
                                    bool bIsSubGeometry,
                                    GMLSRSNameFormat eSRSNameFormat,
                                    bool bCoordSwap,
                                    bool bLineStringAsCurve,
                                    const char* pszGMLId,
                                    int nSRSDimensionLocFlags,
                                    bool bForceLineStringAsLinearRing,
                                    const char* pszNamespaceDecl,
                                    const char* pszOverriddenElementName )

{

/* -------------------------------------------------------------------- */
/*      Check for Spatial Reference System attached to given geometry   */
/* -------------------------------------------------------------------- */

    // Buffer for srsName, xmlns:gml, srsDimension and gml:id attributes
    // (srsName="..." gml:id="...").

    const OGRSpatialReference* poSRS =
         poParentSRS ? poParentSRS : poGeometry->getSpatialReference();

    char szAttributes[256] = {};
    size_t nAttrsLength = 0;

    if( pszNamespaceDecl != NULL )
    {
        snprintf( szAttributes + nAttrsLength,
                  sizeof(szAttributes) - nAttrsLength,
                  " xmlns:gml=\"%s\"",
                  pszNamespaceDecl );
        pszNamespaceDecl = NULL;
        nAttrsLength += strlen(szAttributes + nAttrsLength);
    }

    if( NULL != poSRS )
    {
        const char* pszTarget = poSRS->IsProjected() ? "PROJCS" : "GEOGCS";
        const char* pszAuthName = poSRS->GetAuthorityName( pszTarget );
        const char* pszAuthCode = poSRS->GetAuthorityCode( pszTarget );
        if( NULL != pszAuthName && strlen(pszAuthName) < 10 &&
            NULL != pszAuthCode && strlen(pszAuthCode) < 10 )
        {
            if( !bIsSubGeometry )
            {
                if (eSRSNameFormat == SRSNAME_OGC_URN)
                {
                    snprintf( szAttributes + nAttrsLength,
                              sizeof(szAttributes) - nAttrsLength,
                              " srsName=\"urn:ogc:def:crs:%s::%s\"",
                              pszAuthName, pszAuthCode );
                }
                else if (eSRSNameFormat == SRSNAME_SHORT)
                {
                    snprintf( szAttributes + nAttrsLength,
                              sizeof(szAttributes) - nAttrsLength,
                              " srsName=\"%s:%s\"",
                              pszAuthName, pszAuthCode );
                }
                else if (eSRSNameFormat == SRSNAME_OGC_URL)
                {
                    snprintf(
                        szAttributes + nAttrsLength,
                        sizeof(szAttributes) - nAttrsLength,
                        " srsName=\"http://www.opengis.net/def/crs/%s/0/%s\"",
                        pszAuthName, pszAuthCode );
                }
                nAttrsLength += strlen(szAttributes + nAttrsLength);
            }
        }
    }

    if( (nSRSDimensionLocFlags & SRSDIM_LOC_GEOMETRY) != 0 &&
        wkbHasZ(poGeometry->getGeometryType()) )
    {
        snprintf( szAttributes + nAttrsLength,
                  sizeof(szAttributes) - nAttrsLength,
                  " srsDimension=\"3\"" );
        nAttrsLength += strlen(szAttributes + nAttrsLength);

        nSRSDimensionLocFlags &= ~SRSDIM_LOC_GEOMETRY;
    }

    if( pszGMLId != NULL &&
        nAttrsLength + 9 + strlen(pszGMLId) + 1 < sizeof(szAttributes) )
    {
        snprintf( szAttributes + nAttrsLength,
                  sizeof(szAttributes) - nAttrsLength,
                  " gml:id=\"%s\"",
                  pszGMLId );
        nAttrsLength += strlen(szAttributes + nAttrsLength);
    }

    const OGRwkbGeometryType eType = poGeometry->getGeometryType();
    const OGRwkbGeometryType eFType = wkbFlatten(eType);

/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( eType == wkbPoint )
    {
        const OGRPoint *poPoint = dynamic_cast<const OGRPoint *>(poGeometry);
        if( poPoint == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRPoint.");
        }

        char szCoordinate[256] = {};
        if( bCoordSwap )
            OGRMakeWktCoordinate( szCoordinate,
                                  poPoint->getY(), poPoint->getX(), 0.0, 2 );
        else
            OGRMakeWktCoordinate( szCoordinate,
                                  poPoint->getX(), poPoint->getY(), 0.0, 2 );

        _GrowBuffer( *pnLength + strlen(szCoordinate) + 60 + nAttrsLength,
                     ppszText, pnMaxLength );

        snprintf( *ppszText + *pnLength, *pnMaxLength - *pnLength,
                  "<gml:Point%s><gml:pos>%s</gml:pos></gml:Point>",
                  szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( eType == wkbPoint25D )
    {
        const OGRPoint *poPoint = dynamic_cast<const OGRPoint *>(poGeometry);
        if( poPoint == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRPoint.");
            return false;
        }

        char szCoordinate[256] = {};
        if( bCoordSwap )
            OGRMakeWktCoordinate(
                szCoordinate,
                poPoint->getY(), poPoint->getX(), poPoint->getZ(), 3 );
        else
            OGRMakeWktCoordinate(
                szCoordinate,
                poPoint->getX(), poPoint->getY(), poPoint->getZ(), 3 );

        _GrowBuffer( *pnLength + strlen(szCoordinate) + 70 + nAttrsLength,
                     ppszText, pnMaxLength );

        snprintf( *ppszText + *pnLength, *pnMaxLength - *pnLength,
                  "<gml:Point%s><gml:pos>%s</gml:pos></gml:Point>",
                  szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbLineString )
    {
        const bool bRing =
            EQUAL(poGeometry->getGeometryName(), "LINEARRING") ||
            bForceLineStringAsLinearRing;
        if( !bRing && bLineStringAsCurve )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                            "<gml:Curve" );
            AppendString( ppszText, pnLength, pnMaxLength,
                            szAttributes );
            AppendString( ppszText, pnLength, pnMaxLength,
                            "><gml:segments><gml:LineStringSegment>" );
            const OGRLineString *poLineString =
                dynamic_cast<const OGRLineString *>(poGeometry);
            if( poLineString == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRLineString.");
            }
            AppendGML3CoordinateList(
                poLineString, bCoordSwap,
                ppszText, pnLength, pnMaxLength, nSRSDimensionLocFlags );
            AppendString(ppszText, pnLength, pnMaxLength,
                         "</gml:LineStringSegment></gml:segments></gml:Curve>");
        }
        else
        {
            // Buffer for tag name + srsName attribute if set.
            const size_t nLineTagLength = 16;
            const size_t nLineTagNameBufLen = nLineTagLength + nAttrsLength + 1;
            char* pszLineTagName =
                static_cast<char *>(CPLMalloc(nLineTagNameBufLen));

            if( bRing )
            {
                // LinearRing isn't supposed to have srsName attribute according
                // to GML3 SF-0.
                AppendString( ppszText, pnLength, pnMaxLength,
                              "<gml:LinearRing>" );
            }
            else
            {
                snprintf( pszLineTagName, nLineTagNameBufLen,
                          "<gml:LineString%s>", szAttributes );

                AppendString( ppszText, pnLength, pnMaxLength,
                              pszLineTagName );
            }

            // Free tag buffer.
            CPLFree( pszLineTagName );

            const OGRLineString *poLineString =
                dynamic_cast<const OGRLineString *>(poGeometry);
            if( poLineString == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRLineString.");
                return false;
            }

            AppendGML3CoordinateList( poLineString, bCoordSwap,
                                      ppszText, pnLength, pnMaxLength,
                                      nSRSDimensionLocFlags );

            if( bRing )
                AppendString( ppszText, pnLength, pnMaxLength,
                              "</gml:LinearRing>" );
            else
                AppendString( ppszText, pnLength, pnMaxLength,
                              "</gml:LineString>" );
        }
    }

/* -------------------------------------------------------------------- */
/*      ArcString or Circle                                             */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbCircularString )
    {
        AppendString( ppszText, pnLength, pnMaxLength,
                        "<gml:Curve" );
        AppendString( ppszText, pnLength, pnMaxLength,
                        szAttributes );
        const OGRSimpleCurve* poSC =
            dynamic_cast<const OGRSimpleCurve *>(poGeometry);
        if( poSC == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRSimpleCurve.");
            return false;
        }
        // SQL MM has a unique type for arc and circle, GML does not.
        if( poSC->getNumPoints() == 3 &&
            poSC->getX(0) == poSC->getX(2) &&
            poSC->getY(0) == poSC->getY(2) )
        {
            const double dfMidX = (poSC->getX(0) + poSC->getX(1)) / 2.0;
            const double dfMidY = (poSC->getY(0) + poSC->getY(1)) / 2.0;
            const double dfDirX = (poSC->getX(1) - poSC->getX(0)) / 2.0;
            const double dfDirY = (poSC->getY(1) - poSC->getY(0)) / 2.0;
            const double dfNormX = -dfDirY;
            const double dfNormY = dfDirX;
            const double dfNewX = dfMidX + dfNormX;
            const double dfNewY = dfMidY + dfNormY;
            OGRLineString* poLS = new OGRLineString();
            OGRPoint p;
            poSC->getPoint(0, &p);
            poLS->addPoint(&p);
            poSC->getPoint(1, &p);
            if( poSC->getCoordinateDimension() == 3 )
                poLS->addPoint(dfNewX, dfNewY, p.getZ());
            else
                poLS->addPoint(dfNewX, dfNewY);
            poLS->addPoint(&p);
            AppendString( ppszText, pnLength, pnMaxLength,
                          "><gml:segments><gml:Circle>" );
            AppendGML3CoordinateList(
                poLS, bCoordSwap,
                ppszText, pnLength, pnMaxLength, nSRSDimensionLocFlags );
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:Circle></gml:segments></gml:Curve>" );
            delete poLS;
        }
        else
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "><gml:segments><gml:ArcString>" );
            AppendGML3CoordinateList(
                poSC, bCoordSwap,
                ppszText, pnLength, pnMaxLength, nSRSDimensionLocFlags );
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:ArcString></gml:segments></gml:Curve>" );
        }
    }

/* -------------------------------------------------------------------- */
/*      CompositeCurve                                                  */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbCompoundCurve )
    {
        AppendString( ppszText, pnLength, pnMaxLength, "<gml:CompositeCurve" );
        AppendString( ppszText, pnLength, pnMaxLength, szAttributes );
        AppendString( ppszText, pnLength, pnMaxLength, ">");

        const OGRCompoundCurve* poCC =
            dynamic_cast<const OGRCompoundCurve *>(poGeometry);
        if( poCC == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRCompoundCurve.");
            return false;
        }
        for( int i = 0; i < poCC->getNumCurves(); i++ )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:curveMember>" );
            if( !OGR2GML3GeometryAppend( poCC->getCurve(i), poSRS,
                                         ppszText, pnLength,
                                         pnMaxLength, true, eSRSNameFormat,
                                         bCoordSwap,
                                         bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags,
                                         false, NULL, NULL) )
                return false;
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:curveMember>" );
        }
        AppendString( ppszText, pnLength, pnMaxLength,
                      "</gml:CompositeCurve>" );
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbPolygon || eFType == wkbCurvePolygon )
    {
        const OGRCurvePolygon *poCP =
            dynamic_cast<const OGRCurvePolygon *>(poGeometry);
        if( poCP == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRCurvePolygon.");
            return false;
        }

        // Buffer for polygon tag name + srsName attribute if set.
        const char* pszElemName = pszOverriddenElementName ?
                                        pszOverriddenElementName : "Polygon";
        const size_t nPolyTagLength = 7 + strlen(pszElemName);
        const size_t nPolyTagNameBufLen = nPolyTagLength + nAttrsLength + 1;
        char* pszPolyTagName =
            static_cast<char *>(CPLMalloc(nPolyTagNameBufLen));

        // Compose Polygon tag with or without srsName attribute.
        snprintf( pszPolyTagName, nPolyTagNameBufLen,
                  "<gml:%s%s>",
                  pszElemName,
                  szAttributes );

        AppendString( ppszText, pnLength, pnMaxLength,
                      pszPolyTagName );

        // Free tag buffer.
        CPLFree( pszPolyTagName );

        // Don't add srsName to polygon rings.

        if( poCP->getExteriorRingCurve() != NULL )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:exterior>" );

            if( !OGR2GML3GeometryAppend( poCP->getExteriorRingCurve(), poSRS,
                                         ppszText, pnLength, pnMaxLength,
                                         true, eSRSNameFormat, bCoordSwap,
                                         bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags,
                                         true, NULL, NULL) )
            {
                return false;
            }

            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:exterior>" );
        }

        for( int iRing = 0; iRing < poCP->getNumInteriorRings(); iRing++ )
        {
            const OGRCurve *poRing = poCP->getInteriorRingCurve(iRing);

            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:interior>" );

            if( !OGR2GML3GeometryAppend( poRing, poSRS, ppszText, pnLength,
                                         pnMaxLength, true, eSRSNameFormat,
                                         bCoordSwap,
                                         bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags,
                                         true, NULL, NULL) )
                return false;

            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:interior>" );
        }

        AppendString( ppszText, pnLength, pnMaxLength, "</gml:" );
        AppendString( ppszText, pnLength, pnMaxLength, pszElemName );
        AppendString( ppszText, pnLength, pnMaxLength, ">" );
    }

/* -------------------------------------------------------------------- */
/*     Triangle                                                         */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbTriangle )
    {
        const OGRTriangle *poTri =
                                dynamic_cast<const OGRTriangle *>(poGeometry);
        if( poTri == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRTriangle.");
            return false;
        }

        AppendString( ppszText, pnLength, pnMaxLength, "<gml:Triangle>" );

        if( poTri->getExteriorRingCurve() != NULL )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:exterior>" );

            if( !OGR2GML3GeometryAppend( poTri->getExteriorRingCurve(), poSRS,
                                         ppszText, pnLength,
                                         pnMaxLength, true, eSRSNameFormat,
                                         bCoordSwap,
                                         bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags,
                                         true, NULL, NULL) )
            {
                return false;
            }

            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:exterior>" );
        }

        AppendString( ppszText, pnLength, pnMaxLength,
                      "</gml:Triangle>" );
    }

/* -------------------------------------------------------------------- */
/*      MultiSurface, MultiCurve, MultiPoint, MultiGeometry             */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbMultiPolygon
             || eFType == wkbMultiSurface
             || eFType == wkbMultiLineString
             || eFType == wkbMultiCurve
             || eFType == wkbMultiPoint
             || eFType == wkbGeometryCollection )
    {
        const OGRGeometryCollection *poGC =
            dynamic_cast<const OGRGeometryCollection *>(poGeometry);
        if( poGC == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRGeometryCollection.");
            return false;
        }
        const char *pszElemClose = NULL;
        const char *pszMemberElem = NULL;

        // Buffer for opening tag + srsName attribute.
        char* pszElemOpen = NULL;

        if( eFType == wkbMultiPolygon || eFType == wkbMultiSurface )
        {
            const size_t nBufLen = 13 + nAttrsLength + 1;
            pszElemOpen = static_cast<char *>(CPLMalloc(nBufLen));
            snprintf( pszElemOpen, nBufLen, "MultiSurface%s>", szAttributes );

            pszElemClose = "MultiSurface>";
            pszMemberElem = "surfaceMember>";
        }
        else if( eFType == wkbMultiLineString || eFType == wkbMultiCurve )
        {
            const size_t nBufLen = 16 + nAttrsLength + 1;
            pszElemOpen = static_cast<char *>(CPLMalloc(nBufLen));
            snprintf( pszElemOpen, nBufLen, "MultiCurve%s>", szAttributes );

            pszElemClose = "MultiCurve>";
            pszMemberElem = "curveMember>";
        }
        else if( eFType == wkbMultiPoint )
        {
            const size_t nBufLen = 11 + nAttrsLength + 1;
            pszElemOpen = static_cast<char *>(CPLMalloc(nBufLen));
            snprintf( pszElemOpen, nBufLen, "MultiPoint%s>", szAttributes );

            pszElemClose = "MultiPoint>";
            pszMemberElem = "pointMember>";
        }
        else
        {
            const size_t nBufLen = 19 + nAttrsLength + 1;
            pszElemOpen = static_cast<char *>(CPLMalloc(nBufLen));
            snprintf( pszElemOpen, nBufLen, "MultiGeometry%s>", szAttributes );

            pszElemClose = "MultiGeometry>";
            pszMemberElem = "geometryMember>";
        }

        AppendString( ppszText, pnLength, pnMaxLength, "<gml:" );
        AppendString( ppszText, pnLength, pnMaxLength, pszElemOpen );

        for( int iMember = 0; iMember < poGC->getNumGeometries(); iMember++ )
        {
            const OGRGeometry *poMember = poGC->getGeometryRef( iMember );

            AppendString( ppszText, pnLength, pnMaxLength, "<gml:" );
            AppendString( ppszText, pnLength, pnMaxLength, pszMemberElem );

            char* pszGMLIdSub = NULL;
            if( pszGMLId != NULL )
                pszGMLIdSub = CPLStrdup(CPLSPrintf("%s.%d", pszGMLId, iMember));

            if( !OGR2GML3GeometryAppend( poMember, poSRS,
                                         ppszText, pnLength, pnMaxLength,
                                         true, eSRSNameFormat, bCoordSwap,
                                         bLineStringAsCurve,
                                         pszGMLIdSub, nSRSDimensionLocFlags,
                                         false, NULL, NULL ) )
            {
                CPLFree(pszGMLIdSub);
                return false;
            }

            CPLFree(pszGMLIdSub);

            AppendString( ppszText, pnLength, pnMaxLength, "</gml:" );
            AppendString( ppszText, pnLength, pnMaxLength, pszMemberElem );
        }

        AppendString( ppszText, pnLength, pnMaxLength, "</gml:" );
        AppendString( ppszText, pnLength, pnMaxLength, pszElemClose );

        // Free tag buffer.
        CPLFree( pszElemOpen );
    }

/* -------------------------------------------------------------------- */
/*      Polyhedral Surface                                              */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbPolyhedralSurface)
    {
        // The patches enclosed in a single <gml:polygonPatches> tag need to be co-planar.
        // TODO - enforce the condition within this implementation
        const OGRPolyhedralSurface *poPS =
            dynamic_cast<const OGRPolyhedralSurface *>(poGeometry);
        if( poPS == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRPolyhedralSurface.");
            return false;
        }

        AppendString( ppszText, pnLength, pnMaxLength, "<gml:PolyhedralSurface" );
        AppendString( ppszText, pnLength, pnMaxLength, szAttributes );
        AppendString( ppszText, pnLength, pnMaxLength, "><gml:polygonPatches>" );

        for( int iMember = 0; iMember < poPS->getNumGeometries(); iMember++)
        {
            const OGRGeometry *poMember = poPS->getGeometryRef( iMember );
            char* pszGMLIdSub = NULL;
            if (pszGMLId != NULL)
                pszGMLIdSub = CPLStrdup(CPLSPrintf("%s.%d", pszGMLId, iMember));

            if( !OGR2GML3GeometryAppend( poMember, poSRS,
                                         ppszText, pnLength,
                                         pnMaxLength, true, eSRSNameFormat,
                                         bCoordSwap,
                                         bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags,
                                         false, NULL, "PolygonPatch" ) )
            {
                CPLFree(pszGMLIdSub);
                return false;
            }

            CPLFree(pszGMLIdSub);
        }

        AppendString( ppszText, pnLength, pnMaxLength,
                      "</gml:polygonPatches>" );
        AppendString( ppszText, pnLength, pnMaxLength,
                      "</gml:PolyhedralSurface>" );
    }

/* -------------------------------------------------------------------- */
/*      TIN                                                             */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbTIN)
    {
        // OGR uses the following hierarchy for TriangulatedSurface -

        // <gml:TriangulatedSurface>
        //     <gml:patches>
        //         <gml:Triangle>
        //             <gml:exterior>
        //                 <gml:LinearRing>
        //                     <gml:posList srsDimension=...>...</gml:posList>
        //                 </gml:LinearRing>
        //             </gml:exterior>
        //         </gml:Triangle>
        //     </gml:patches>
        // </gml:TriangulatedSurface>

        // <gml:trianglePatches> is deprecated, so write feature is not enabled for <gml:trianglePatches>
        const OGRTriangulatedSurface *poTIN =
            dynamic_cast<const OGRTriangulatedSurface *>(poGeometry);
        if( poTIN == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRTriangulatedSurface.");
            return false;
        }

        AppendString( ppszText, pnLength, pnMaxLength, "<gml:TriangulatedSurface" );
        AppendString( ppszText, pnLength, pnMaxLength, szAttributes );
        AppendString( ppszText, pnLength, pnMaxLength, "><gml:patches>" );

        for( int iMember = 0; iMember < poTIN->getNumGeometries(); iMember++)
        {
            const OGRGeometry *poMember = poTIN->getGeometryRef( iMember );

            char* pszGMLIdSub = NULL;
            if (pszGMLId != NULL)
                pszGMLIdSub = CPLStrdup(CPLSPrintf("%s.%d", pszGMLId, iMember));

            if( !OGR2GML3GeometryAppend( poMember, poSRS,
                                         ppszText, pnLength,
                                         pnMaxLength, true, eSRSNameFormat,
                                         bCoordSwap,
                                         bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags,
                                         false, NULL, NULL) )
            {
                CPLFree(pszGMLIdSub);
                return false;
            }

            CPLFree(pszGMLIdSub);
        }

        AppendString( ppszText, pnLength, pnMaxLength, "</gml:patches>" );
        AppendString( ppszText, pnLength, pnMaxLength,
                      "</gml:TriangulatedSurface>" );
    }

    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported geometry type %s",
                 OGRGeometryTypeToName(eType));
        return false;
    }

    return true;
}

/************************************************************************/
/*                       OGR_G_ExportToGMLTree()                        */
/************************************************************************/

/** Convert a geometry into GML format. */
CPLXMLNode *OGR_G_ExportToGMLTree( OGRGeometryH hGeometry )

{
    char *pszText = OGR_G_ExportToGML( hGeometry );
    if( pszText == NULL )
        return NULL;

    CPLXMLNode *psTree = CPLParseXMLString( pszText );

    CPLFree( pszText );

    return psTree;
}

/************************************************************************/
/*                         OGR_G_ExportToGML()                          */
/************************************************************************/

/**
 * \brief Convert a geometry into GML format.
 *
 * The GML geometry is expressed directly in terms of GML basic data
 * types assuming the this is available in the gml namespace.  The returned
 * string should be freed with CPLFree() when no longer required.
 *
 * This method is the same as the C++ method OGRGeometry::exportToGML().
 *
 * @param hGeometry handle to the geometry.
 * @return A GML fragment or NULL in case of error.
 */

char *OGR_G_ExportToGML( OGRGeometryH hGeometry )

{
    return OGR_G_ExportToGMLEx(hGeometry, NULL);
}

/************************************************************************/
/*                        OGR_G_ExportToGMLEx()                         */
/************************************************************************/

/**
 * \brief Convert a geometry into GML format.
 *
 * The GML geometry is expressed directly in terms of GML basic data
 * types assuming the this is available in the gml namespace.  The returned
 * string should be freed with CPLFree() when no longer required.
 *
 * The supported options are :
 * <ul>
 * <li> FORMAT=GML2/GML3/GML32 (GML2 or GML32 added in GDAL 2.1).
 *      If not set, it will default to GML 2.1.2 output.
 * <li> GML3_LINESTRING_ELEMENT=curve. (Only valid for FORMAT=GML3)
 *      To use gml:Curve element for linestrings.
 *      Otherwise gml:LineString will be used .
 * <li> GML3_LONGSRS=YES/NO. (Only valid for FORMAT=GML3, deprecated by
 *      SRSNAME_FORMAT in GDAL &gt;=2.2). Defaults to YES.
 *      If YES, SRS with EPSG authority will be written with the
 *      "urn:ogc:def:crs:EPSG::" prefix.
 *      In the case the SRS is a SRS without explicit AXIS order, but that the
 *      same SRS authority code
 *      imported with ImportFromEPSGA() should be treated as lat/long or
 *      northing/easting, then the function will take care of coordinate order
 *      swapping.
 *      If set to NO, SRS with EPSG authority will be written with the "EPSG:"
 *      prefix, even if they are in lat/long order.
 * <li> SRSNAME_FORMAT=SHORT/OGC_URN/OGC_URL (Only valid for FORMAT=GML3, added
 *      in GDAL 2.2). Defaults to OGC_URN.  If SHORT, then srsName will be in
 *      the form AUTHORITY_NAME:AUTHORITY_CODE If OGC_URN, then srsName will be
 *      in the form urn:ogc:def:crs:AUTHORITY_NAME::AUTHORITY_CODE If OGC_URL,
 *      then srsName will be in the form
 *      http://www.opengis.net/def/crs/AUTHORITY_NAME/0/AUTHORITY_CODE For
 *      OGC_URN and OGC_URL, in the case the SRS is a SRS without explicit AXIS
 *      order, but that the same SRS authority code imported with
 *      ImportFromEPSGA() should be treated as lat/long or northing/easting,
 *      then the function will take care of coordinate order swapping.
 * <li> GMLID=astring. If specified, a gml:id attribute will be written in the
 *      top-level geometry element with the provided value.
 *      Required for GML 3.2 compatibility.
 * <li> SRSDIMENSION_LOC=POSLIST/GEOMETRY/GEOMETRY,POSLIST. (Only valid for
 *      FORMAT=GML3/GML32, GDAL >= 2.0) Default to POSLIST.
 *      For 2.5D geometries, define the location where to attach the
 *      srsDimension attribute.
 *      There are diverging implementations. Some put in on the
 *      &lt;gml:posList&gt; element, other on the top geometry element.

 * <li> NAMESPACE_DECL=YES/NO. If set to YES,
 *      xmlns:gml="http://www.opengis.net/gml" will be added to the root node
 *      for GML < 3.2 or xmlns:gml="http://www.opengis.net/gml/3.2" for GML 3.2
 * </ul>
 *
 * Note that curve geometries like CIRCULARSTRING, COMPOUNDCURVE, CURVEPOLYGON,
 * MULTICURVE or MULTISURFACE are not supported in GML 2.
 *
 * This method is the same as the C++ method OGRGeometry::exportToGML().
 *
 * @param hGeometry handle to the geometry.
 * @param papszOptions NULL-terminated list of options.
 * @return A GML fragment or NULL in case of error.
 *
 * @since OGR 1.8.0
 */

char *OGR_G_ExportToGMLEx( OGRGeometryH hGeometry, char** papszOptions )

{
    if( hGeometry == NULL )
        return CPLStrdup( "" );

    // Do not use hGeometry after here.
    OGRGeometry* poGeometry = reinterpret_cast<OGRGeometry*>(hGeometry);

    size_t nLength = 0;
    size_t nMaxLength = 1;

    char *pszText = static_cast<char *>(CPLMalloc(nMaxLength));
    pszText[0] = '\0';

    const char* pszFormat = CSLFetchNameValue(papszOptions, "FORMAT");
    const bool bNamespaceDecl =
        CPLTestBool(CSLFetchNameValueDef(papszOptions,
                                         "NAMESPACE_DECL", "NO")) != FALSE;
    if( pszFormat && (EQUAL(pszFormat, "GML3") || EQUAL(pszFormat, "GML32")) )
    {
        const char* pszLineStringElement =
            CSLFetchNameValue(papszOptions, "GML3_LINESTRING_ELEMENT");
        const bool bLineStringAsCurve =
            pszLineStringElement && EQUAL(pszLineStringElement, "curve");
        const char* pszLongSRS =
            CSLFetchNameValue(papszOptions, "GML3_LONGSRS");
        const char* pszSRSNameFormat =
            CSLFetchNameValue(papszOptions, "SRSNAME_FORMAT");
        GMLSRSNameFormat eSRSNameFormat = SRSNAME_OGC_URN;
        if( pszSRSNameFormat )
        {
            if( pszLongSRS )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Both GML3_LONGSRS and SRSNAME_FORMAT specified. "
                         "Ignoring GML3_LONGSRS");
            }
            if( EQUAL(pszSRSNameFormat, "SHORT") )
                eSRSNameFormat = SRSNAME_SHORT;
            else if( EQUAL(pszSRSNameFormat, "OGC_URN") )
                eSRSNameFormat = SRSNAME_OGC_URN;
            else if( EQUAL(pszSRSNameFormat, "OGC_URL") )
                eSRSNameFormat = SRSNAME_OGC_URL;
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Invalid value for SRSNAME_FORMAT. "
                         "Using SRSNAME_OGC_URN");
            }
        }
        else if( pszLongSRS && !CPLTestBool(pszLongSRS) )
            eSRSNameFormat = SRSNAME_SHORT;

        const char* pszGMLId = CSLFetchNameValue(papszOptions, "GMLID");
        if( pszGMLId == NULL && EQUAL(pszFormat, "GML32") )
            CPLError(CE_Warning, CPLE_AppDefined,
                     "FORMAT=GML32 specified but not GMLID set");
        const char* pszSRSDimensionLoc =
                CSLFetchNameValueDef(papszOptions, "SRSDIMENSION_LOC",
                                     "POSLIST");
        char** papszSRSDimensionLoc =
            CSLTokenizeString2(pszSRSDimensionLoc, ",", 0);
        int nSRSDimensionLocFlags = 0;
        for( int i = 0; papszSRSDimensionLoc[i] != NULL; i++ )
        {
            if( EQUAL(papszSRSDimensionLoc[i], "POSLIST") )
                nSRSDimensionLocFlags |= SRSDIM_LOC_POSLIST;
            else if( EQUAL(papszSRSDimensionLoc[i], "GEOMETRY") )
                nSRSDimensionLocFlags |= SRSDIM_LOC_GEOMETRY;
            else
                CPLDebug("OGR", "Unrecognized location for srsDimension : %s",
                         papszSRSDimensionLoc[i]);
        }
        CSLDestroy(papszSRSDimensionLoc);
        const char* pszNamespaceDecl = NULL;
        if( bNamespaceDecl && EQUAL(pszFormat, "GML32") )
            pszNamespaceDecl = "http://www.opengis.net/gml/3.2";
        else if( bNamespaceDecl )
            pszNamespaceDecl = "http://www.opengis.net/gml";

        bool bCoordSwap = false;
        const char* pszCoordSwap =
            CSLFetchNameValue(papszOptions, "COORD_SWAP");
        if( pszCoordSwap )
        {
            bCoordSwap = CPLTestBool(pszCoordSwap);
        }
        else
        {
            const OGRSpatialReference* poSRS =
                poGeometry->getSpatialReference();
            if( poSRS != NULL )
            {
                const char* pszTarget =
                    poSRS->IsProjected() ? "PROJCS" : "GEOGCS";
                const char* pszAuthName = poSRS->GetAuthorityName( pszTarget );
                const char* pszAuthCode = poSRS->GetAuthorityCode( pszTarget );
                if( NULL != pszAuthName && NULL != pszAuthCode &&
                    EQUAL( pszAuthName, "EPSG" ) &&
                    eSRSNameFormat != SRSNAME_SHORT &&
                    !(((OGRSpatialReference*)poSRS)->EPSGTreatsAsLatLong() ||
                    ((OGRSpatialReference*)poSRS)->
                          EPSGTreatsAsNorthingEasting()))
                {
                    OGRSpatialReference oSRS;
                    if (oSRS.importFromEPSGA(atoi(pszAuthCode)) == OGRERR_NONE)
                    {
                        if (oSRS.EPSGTreatsAsLatLong() ||
                            oSRS.EPSGTreatsAsNorthingEasting())
                            bCoordSwap = true;
                    }
                }
            }
        }

        if( !OGR2GML3GeometryAppend( poGeometry, NULL, &pszText,
                                     &nLength, &nMaxLength, false,
                                     eSRSNameFormat,
                                     bCoordSwap,
                                     bLineStringAsCurve, pszGMLId,
                                     nSRSDimensionLocFlags, false,
                                     pszNamespaceDecl, NULL ))
        {
            CPLFree( pszText );
            return NULL;
        }

        return pszText;
    }

    const char* pszNamespaceDecl = NULL;
    if( bNamespaceDecl )
        pszNamespaceDecl = "http://www.opengis.net/gml";
    if( !OGR2GMLGeometryAppend(
            poGeometry, &pszText,
            &nLength, &nMaxLength, false,
            pszNamespaceDecl) )
    {
        CPLFree( pszText );
        return NULL;
    }

    return pszText;
}
