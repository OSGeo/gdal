/******************************************************************************
 * $Id$
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

#include "cpl_minixml.h"
#include "ogr_geometry.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "cpl_error.h"
#include "cpl_conv.h"

#define SRSDIM_LOC_GEOMETRY (1 << 0)
#define SRSDIM_LOC_POSLIST  (1 << 1)

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

#ifdef notdef
    if( !b3D )
    {
        if( x == (int) x && y == (int) y )
            sprintf( pszTarget, "%d,%d", (int) x, (int) y );
        else if( fabs(x) < 370 && fabs(y) < 370 )
            CPLsprintf( pszTarget, "%.16g,%.16g", x, y );
        else if( fabs(x) > 100000000.0 || fabs(y) > 100000000.0 )
            CPLsprintf( pszTarget, "%.16g,%.16g", x, y );
        else
            CPLsprintf( pszTarget, "%.3f,%.3f", x, y );
    }
    else
    {
        if( x == (int) x && y == (int) y && z == (int) z )
            sprintf( pszTarget, "%d,%d,%d", (int) x, (int) y, (int) z );
        else if( fabs(x) < 370 && fabs(y) < 370 )
            CPLsprintf( pszTarget, "%.16g,%.16g,%.16g", x, y, z );
        else if( fabs(x) > 100000000.0 || fabs(y) > 100000000.0 
                 || fabs(z) > 100000000.0 )
            CPLsprintf( pszTarget, "%.16g,%.16g,%.16g", x, y, z );
        else
            CPLsprintf( pszTarget, "%.3f,%.3f,%.3f", x, y, z );
    }
#endif
}

/************************************************************************/
/*                            _GrowBuffer()                             */
/************************************************************************/

static void _GrowBuffer( size_t nNeeded, char **ppszText, size_t *pnMaxLength )

{
    if( nNeeded+1 >= *pnMaxLength )
    {
        *pnMaxLength = MAX(*pnMaxLength * 2,nNeeded+1);
        *ppszText = (char *) CPLRealloc(*ppszText, *pnMaxLength);
    }
}

/************************************************************************/
/*                            AppendString()                            */
/************************************************************************/

static void AppendString( char **ppszText, size_t *pnLength, size_t *pnMaxLength,
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
    char        szCoordinate[256];
    bool        b3D = wkbHasZ(poLine->getGeometryType()) != FALSE;

    *pnLength += strlen(*ppszText + *pnLength);
    _GrowBuffer( *pnLength + 20, ppszText, pnMaxLength );

    strcat( *ppszText + *pnLength, "<gml:coordinates>" );
    *pnLength += strlen(*ppszText + *pnLength);

    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        MakeGMLCoordinate( szCoordinate, 
                           poLine->getX(iPoint),
                           poLine->getY(iPoint),
                           poLine->getZ(iPoint),
                           b3D );
        _GrowBuffer( *pnLength + strlen(szCoordinate)+1, 
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
    char szAttributes[64] = { 0 };
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
        const char* pszAuthName = NULL;
        const char* pszAuthCode = NULL;
        const char* pszTarget = NULL;

        if (poSRS->IsProjected())
            pszTarget = "PROJCS";
        else
            pszTarget = "GEOGCS";

        pszAuthName = poSRS->GetAuthorityName( pszTarget );
        if( NULL != pszAuthName )
        {
            if( EQUAL( pszAuthName, "EPSG" ) )
            {
                pszAuthCode = poSRS->GetAuthorityCode( pszTarget );
                if( NULL != pszAuthCode && strlen(pszAuthCode) < 10 )
                {
                    snprintf( szAttributes + nAttrsLength,
                              sizeof(szAttributes) - nAttrsLength,
                              " srsName=\"%s:%s\"",
                              pszAuthName, pszAuthCode );

                    nAttrsLength += strlen(szAttributes + nAttrsLength);
                }
            }
        }
    }

    OGRwkbGeometryType eType = poGeometry->getGeometryType();
    OGRwkbGeometryType eFType = wkbFlatten(eType);

/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( eType == wkbPoint )
    {
        char    szCoordinate[256];
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        MakeGMLCoordinate( szCoordinate, 
                           poPoint->getX(), poPoint->getY(), 0.0, false );

        _GrowBuffer( *pnLength + strlen(szCoordinate) + 60 + nAttrsLength,
                     ppszText, pnMaxLength );

        snprintf( *ppszText + *pnLength, *pnMaxLength -  *pnLength, 
                "<gml:Point%s><gml:coordinates>%s</gml:coordinates></gml:Point>",
                 szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( eType == wkbPoint25D )
    {
        char    szCoordinate[256];
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        MakeGMLCoordinate( szCoordinate, 
                           poPoint->getX(), poPoint->getY(), poPoint->getZ(), 
                           true );

        _GrowBuffer( *pnLength + strlen(szCoordinate) + 70 + nAttrsLength,
                     ppszText, pnMaxLength );

        snprintf( *ppszText + *pnLength, *pnMaxLength -  *pnLength, 
                "<gml:Point%s><gml:coordinates>%s</gml:coordinates></gml:Point>",
                 szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbLineString )
    {
        bool bRing = EQUAL(poGeometry->getGeometryName(),"LINEARRING");

        // Buffer for tag name + srsName attribute if set
        const size_t nLineTagLength = 16;
        char* pszLineTagName = NULL;
        const size_t nLineTagNameBufLen = nLineTagLength + nAttrsLength + 1;
        pszLineTagName = (char *) CPLMalloc( nLineTagNameBufLen );

        if( bRing )
        {
            snprintf( pszLineTagName, nLineTagNameBufLen, "<gml:LinearRing%s>", szAttributes );

            AppendString( ppszText, pnLength, pnMaxLength,
                          pszLineTagName );
        }
        else
        {
            snprintf( pszLineTagName, nLineTagNameBufLen, "<gml:LineString%s>", szAttributes );

            AppendString( ppszText, pnLength, pnMaxLength,
                          pszLineTagName );
        }

        // FREE TAG BUFFER
        CPLFree( pszLineTagName );

        AppendCoordinateList( (OGRLineString *) poGeometry, 
                              ppszText, pnLength, pnMaxLength );

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
        OGRPolygon      *poPolygon = (OGRPolygon *) poGeometry;

        // Buffer for polygon tag name + srsName attribute if set
        const size_t nPolyTagLength = 13;
        char* pszPolyTagName = NULL;
        const size_t nPolyTagNameBufLen = nPolyTagLength + nAttrsLength + 1;
        pszPolyTagName = (char *) CPLMalloc( nPolyTagNameBufLen );

        // Compose Polygon tag with or without srsName attribute
        snprintf( pszPolyTagName, nPolyTagNameBufLen, "<gml:Polygon%s>", szAttributes );

        AppendString( ppszText, pnLength, pnMaxLength,
                      pszPolyTagName );

        // FREE TAG BUFFER
        CPLFree( pszPolyTagName );

        // Don't add srsName to polygon rings

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
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeometry;
        int             iMember;
        const char *pszElemClose = NULL;
        const char *pszMemberElem = NULL;

        // Buffer for opening tag + srsName attribute
        char* pszElemOpen = NULL;

        if( eFType == wkbMultiPolygon )
        {
            const size_t nBufLen = 13 + nAttrsLength + 1;
            pszElemOpen = (char *) CPLMalloc( nBufLen );
            snprintf( pszElemOpen, nBufLen, "MultiPolygon%s>", szAttributes );

            pszElemClose = "MultiPolygon>";
            pszMemberElem = "polygonMember>";
        }
        else if( eFType == wkbMultiLineString )
        {
            const size_t nBufLen = 16 + nAttrsLength + 1;
            pszElemOpen = (char *) CPLMalloc( nBufLen );
            snprintf( pszElemOpen, nBufLen, "MultiLineString%s>", szAttributes );

            pszElemClose = "MultiLineString>";
            pszMemberElem = "lineStringMember>";
        }
        else if( eFType == wkbMultiPoint )
        {
            const size_t nBufLen = 11 + nAttrsLength + 1;
            pszElemOpen = (char *) CPLMalloc( nBufLen );
            snprintf( pszElemOpen, nBufLen, "MultiPoint%s>", szAttributes );

            pszElemClose = "MultiPoint>";
            pszMemberElem = "pointMember>";
        }
        else
        {
            const size_t nBufLen = 19 + nAttrsLength + 1;
            pszElemOpen = (char *) CPLMalloc( nBufLen );
            snprintf( pszElemOpen, nBufLen, "MultiGeometry%s>", szAttributes );

            pszElemClose = "MultiGeometry>";
            pszMemberElem = "geometryMember>";
        }

        AppendString( ppszText, pnLength, pnMaxLength, "<gml:" );
        AppendString( ppszText, pnLength, pnMaxLength, pszElemOpen );

        for( iMember = 0; iMember < poGC->getNumGeometries(); iMember++)
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

        // FREE TAG BUFFER
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
/*                                                                      */
/*      Export the envelope of a geometry as a gml:Box.                 */
/************************************************************************/

CPLXMLNode *OGR_G_ExportEnvelopeToGMLTree( OGRGeometryH hGeometry )

{
    CPLXMLNode *psBox, *psCoord;
    OGREnvelope sEnvelope;
    char        szCoordinate[256];
    char       *pszY;

    memset( &sEnvelope, 0, sizeof(sEnvelope) );
    ((OGRGeometry *) hGeometry)->getEnvelope( &sEnvelope );

    if( sEnvelope.MinX == 0 && sEnvelope.MaxX == 0 
        && sEnvelope.MaxX == 0 && sEnvelope.MaxY == 0 )
    {
        /* there is apparently a special way of representing a null box
           geometry ... we should use it here eventually. */

        return NULL;
    }

    psBox = CPLCreateXMLNode( NULL, CXT_Element, "gml:Box" );

/* -------------------------------------------------------------------- */
/*      Add minxy coordinate.                                           */
/* -------------------------------------------------------------------- */
    psCoord = CPLCreateXMLNode( psBox, CXT_Element, "gml:coord" );

    MakeGMLCoordinate( szCoordinate, sEnvelope.MinX, sEnvelope.MinY, 0.0, 
                       false );
    pszY = strstr(szCoordinate,",") + 1;
    pszY[-1] = '\0';

    CPLCreateXMLElementAndValue( psCoord, "gml:X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "gml:Y", pszY );

/* -------------------------------------------------------------------- */
/*      Add maxxy coordinate.                                           */
/* -------------------------------------------------------------------- */
    psCoord = CPLCreateXMLNode( psBox, CXT_Element, "gml:coord" );

    MakeGMLCoordinate( szCoordinate, sEnvelope.MaxX, sEnvelope.MaxY, 0.0, 
                       false );
    pszY = strstr(szCoordinate,",") + 1;
    pszY[-1] = '\0';

    CPLCreateXMLElementAndValue( psCoord, "gml:X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "gml:Y", pszY );

    return psBox;
}


/************************************************************************/
/*                     AppendGML3CoordinateList()                       */
/************************************************************************/

static void AppendGML3CoordinateList( const OGRSimpleCurve *poLine, bool bCoordSwap,
                                      char **ppszText, size_t *pnLength,
                                      size_t *pnMaxLength, int nSRSDimensionLocFlags )

{
    char        szCoordinate[256];
    int         b3D = wkbHasZ(poLine->getGeometryType());

    *pnLength += strlen(*ppszText + *pnLength);
    _GrowBuffer( *pnLength + 40, ppszText, pnMaxLength );

    if (b3D && (nSRSDimensionLocFlags & SRSDIM_LOC_POSLIST) != 0)
        strcat( *ppszText + *pnLength, "<gml:posList srsDimension=\"3\">" );
    else
        strcat( *ppszText + *pnLength, "<gml:posList>" );
    *pnLength += strlen(*ppszText + *pnLength);


    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        if (bCoordSwap)
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
                                    bool bLongSRS,
                                    bool bLineStringAsCurve,
                                    const char* pszGMLId,
                                    int nSRSDimensionLocFlags,
                                    bool bForceLineStringAsLinearRing,
                                    const char* pszNamespaceDecl)

{

/* -------------------------------------------------------------------- */
/*      Check for Spatial Reference System attached to given geometry   */
/* -------------------------------------------------------------------- */

    // Buffer for srsName, xmlns:gml, srsDimension and gml:id attributes (srsName="..." gml:id="...")
    char szAttributes[256];
    size_t nAttrsLength = 0;

    szAttributes[0] = 0;

    const OGRSpatialReference* poSRS = NULL;
    if (poParentSRS)
        poSRS = poParentSRS;
    else
        poSRS = poGeometry->getSpatialReference();

    bool bCoordSwap = false;

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
        const char* pszAuthName = NULL;
        const char* pszAuthCode = NULL;
        const char* pszTarget = NULL;

        if (poSRS->IsProjected())
            pszTarget = "PROJCS";
        else
            pszTarget = "GEOGCS";

        pszAuthName = poSRS->GetAuthorityName( pszTarget );
        if( NULL != pszAuthName )
        {
            if( EQUAL( pszAuthName, "EPSG" ) )
            {
                pszAuthCode = poSRS->GetAuthorityCode( pszTarget );
                if( NULL != pszAuthCode && strlen(pszAuthCode) < 10 )
                {
                    if (bLongSRS && !(((OGRSpatialReference*)poSRS)->EPSGTreatsAsLatLong() ||
                                      ((OGRSpatialReference*)poSRS)->EPSGTreatsAsNorthingEasting()))
                    {
                        OGRSpatialReference oSRS;
                        if (oSRS.importFromEPSGA(atoi(pszAuthCode)) == OGRERR_NONE)
                        {
                            if (oSRS.EPSGTreatsAsLatLong() || oSRS.EPSGTreatsAsNorthingEasting())
                                bCoordSwap = true;
                        }
                    }

                    if (!bIsSubGeometry)
                    {
                        if (bLongSRS)
                        {
                            snprintf( szAttributes + nAttrsLength,
                                      sizeof(szAttributes) - nAttrsLength,
                                      " srsName=\"urn:ogc:def:crs:%s::%s\"",
                                      pszAuthName, pszAuthCode );
                        }
                        else
                        {
                            snprintf( szAttributes + nAttrsLength,
                                      sizeof(szAttributes) - nAttrsLength,
                                      " srsName=\"%s:%s\"",
                                      pszAuthName, pszAuthCode );
                        }

                        nAttrsLength += strlen(szAttributes + nAttrsLength);
                    }
                }
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

    if (pszGMLId != NULL && nAttrsLength + 9 + strlen(pszGMLId) + 1 < sizeof(szAttributes))
    {
        snprintf( szAttributes + nAttrsLength,
                  sizeof(szAttributes) - nAttrsLength,
                  " gml:id=\"%s\"",
                  pszGMLId );
        nAttrsLength += strlen(szAttributes + nAttrsLength);
    }

    OGRwkbGeometryType eType = poGeometry->getGeometryType();
    OGRwkbGeometryType eFType = wkbFlatten(eType);

/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( eType == wkbPoint )
    {
        char    szCoordinate[256];
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        if (bCoordSwap)
            OGRMakeWktCoordinate( szCoordinate,
                           poPoint->getY(), poPoint->getX(), 0.0, 2 );
        else
            OGRMakeWktCoordinate( szCoordinate,
                           poPoint->getX(), poPoint->getY(), 0.0, 2 );

        _GrowBuffer( *pnLength + strlen(szCoordinate) + 60 + nAttrsLength,
                     ppszText, pnMaxLength );

        snprintf( *ppszText + *pnLength, *pnMaxLength -  *pnLength,
                "<gml:Point%s><gml:pos>%s</gml:pos></gml:Point>",
                 szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( eType == wkbPoint25D )
    {
        char    szCoordinate[256];
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        if (bCoordSwap)
            OGRMakeWktCoordinate( szCoordinate,
                           poPoint->getY(), poPoint->getX(), poPoint->getZ(), 3 );
        else
            OGRMakeWktCoordinate( szCoordinate,
                           poPoint->getX(), poPoint->getY(), poPoint->getZ(), 3 );

        _GrowBuffer( *pnLength + strlen(szCoordinate) + 70 + nAttrsLength,
                     ppszText, pnMaxLength );

        snprintf( *ppszText + *pnLength, *pnMaxLength -  *pnLength,
                "<gml:Point%s><gml:pos>%s</gml:pos></gml:Point>",
                 szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( eFType == wkbLineString )
    {
        bool bRing = EQUAL(poGeometry->getGeometryName(),"LINEARRING") ||
                    bForceLineStringAsLinearRing;
        if (!bRing && bLineStringAsCurve)
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                            "<gml:Curve" );
            AppendString( ppszText, pnLength, pnMaxLength,
                            szAttributes );
            AppendString( ppszText, pnLength, pnMaxLength,
                            "><gml:segments><gml:LineStringSegment>" );
            AppendGML3CoordinateList( (OGRLineString *) poGeometry, bCoordSwap,
                                ppszText, pnLength, pnMaxLength, nSRSDimensionLocFlags );
            AppendString( ppszText, pnLength, pnMaxLength,
                            "</gml:LineStringSegment></gml:segments></gml:Curve>" );
        }
        else
        {
            // Buffer for tag name + srsName attribute if set
            const size_t nLineTagLength = 16;
            char* pszLineTagName = NULL;
            const size_t nLineTagNameBufLen = nLineTagLength + nAttrsLength + 1;
            pszLineTagName = (char *) CPLMalloc( nLineTagNameBufLen );

            if( bRing )
            {
                /* LinearRing isn't supposed to have srsName attribute according to GML3 SF-0 */
                AppendString( ppszText, pnLength, pnMaxLength,
                            "<gml:LinearRing>" );
            }
            else
            {
                snprintf( pszLineTagName, nLineTagNameBufLen, "<gml:LineString%s>", szAttributes );

                AppendString( ppszText, pnLength, pnMaxLength,
                            pszLineTagName );
            }

            // FREE TAG BUFFER
            CPLFree( pszLineTagName );

            AppendGML3CoordinateList( (OGRLineString *) poGeometry, bCoordSwap,
                                ppszText, pnLength, pnMaxLength, nSRSDimensionLocFlags );

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
        OGRSimpleCurve* poSC = (OGRSimpleCurve *) poGeometry;
        /* SQL MM has a unique type for arc and circle, GML not */
        if( poSC->getNumPoints() == 3 &&
            poSC->getX(0) == poSC->getX(2) &&
            poSC->getY(0) == poSC->getY(2) )
        {
            double dfMidX = (poSC->getX(0) + poSC->getX(1)) / 2;
            double dfMidY = (poSC->getY(0) + poSC->getY(1)) / 2;
            double dfDirX = (poSC->getX(1) - poSC->getX(0)) / 2;
            double dfDirY = (poSC->getY(1) - poSC->getY(0)) / 2;
            double dfNormX = -dfDirY;
            double dfNormY = dfDirX;
            double dfNewX = dfMidX + dfNormX;
            double dfNewY = dfMidY + dfNormY;
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
            AppendGML3CoordinateList( poLS, bCoordSwap,
                                ppszText, pnLength, pnMaxLength, nSRSDimensionLocFlags );
            AppendString( ppszText, pnLength, pnMaxLength,
                            "</gml:Circle></gml:segments></gml:Curve>" );
            delete poLS;
        }
        else
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                            "><gml:segments><gml:ArcString>" );
            AppendGML3CoordinateList( poSC, bCoordSwap,
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
        AppendString( ppszText, pnLength, pnMaxLength,
                        "<gml:CompositeCurve" );
        AppendString( ppszText, pnLength, pnMaxLength,
                        szAttributes );
        AppendString( ppszText, pnLength, pnMaxLength,">");

        OGRCompoundCurve* poCC = (OGRCompoundCurve*)poGeometry;
        for(int i=0;i<poCC->getNumCurves();i++)
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:curveMember>" );
            if( !OGR2GML3GeometryAppend( poCC->getCurve(i), poSRS, ppszText, pnLength,
                                         pnMaxLength, true, bLongSRS,
                                         bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags, false, NULL) )
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
        OGRCurvePolygon      *poCP = (OGRCurvePolygon *) poGeometry;

        // Buffer for polygon tag name + srsName attribute if set
        const size_t nPolyTagLength = 13;
        char* pszPolyTagName = NULL;
        const size_t nPolyTagNameBufLen = nPolyTagLength + nAttrsLength + 1;
        pszPolyTagName = (char *) CPLMalloc( nPolyTagNameBufLen );

        // Compose Polygon tag with or without srsName attribute
        snprintf( pszPolyTagName, nPolyTagNameBufLen, "<gml:Polygon%s>", szAttributes );

        AppendString( ppszText, pnLength, pnMaxLength,
                      pszPolyTagName );

        // FREE TAG BUFFER
        CPLFree( pszPolyTagName );

        // Don't add srsName to polygon rings

        if( poCP->getExteriorRingCurve() != NULL )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:exterior>" );

            if( !OGR2GML3GeometryAppend( poCP->getExteriorRingCurve(), poSRS,
                                         ppszText, pnLength, pnMaxLength,
                                         true, bLongSRS, bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags, true, NULL) )
            {
                return false;
            }

            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:exterior>" );
        }

        for( int iRing = 0; iRing < poCP->getNumInteriorRings(); iRing++ )
        {
            OGRCurve *poRing = poCP->getInteriorRingCurve(iRing);

            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:interior>" );

            if( !OGR2GML3GeometryAppend( poRing, poSRS, ppszText, pnLength,
                                         pnMaxLength, true, bLongSRS,
                                         bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags, true, NULL) )
                return false;

            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:interior>" );
        }

        AppendString( ppszText, pnLength, pnMaxLength,
                      "</gml:Polygon>" );
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
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeometry;
        int             iMember;
        const char *pszElemClose = NULL;
        const char *pszMemberElem = NULL;

        // Buffer for opening tag + srsName attribute
        char* pszElemOpen = NULL;

        if( eFType == wkbMultiPolygon || eFType == wkbMultiSurface )
        {
            const size_t nBufLen = 13 + nAttrsLength + 1;
            pszElemOpen = (char *) CPLMalloc( nBufLen );
            snprintf( pszElemOpen, nBufLen, "MultiSurface%s>", szAttributes );

            pszElemClose = "MultiSurface>";
            pszMemberElem = "surfaceMember>";
        }
        else if( eFType == wkbMultiLineString || eFType == wkbMultiCurve )
        {
            const size_t nBufLen = 16 + nAttrsLength + 1;
            pszElemOpen = (char *) CPLMalloc( nBufLen );
            snprintf( pszElemOpen, nBufLen, "MultiCurve%s>", szAttributes );

            pszElemClose = "MultiCurve>";
            pszMemberElem = "curveMember>";
        }
        else if( eFType == wkbMultiPoint )
        {
            const size_t nBufLen = 11 + nAttrsLength + 1;
            pszElemOpen = (char *) CPLMalloc( nBufLen );
            snprintf( pszElemOpen, nBufLen, "MultiPoint%s>", szAttributes );

            pszElemClose = "MultiPoint>";
            pszMemberElem = "pointMember>";
        }
        else
        {
            const size_t nBufLen = 19 + nAttrsLength + 1;
            pszElemOpen = (char *) CPLMalloc( nBufLen );
            snprintf( pszElemOpen, nBufLen, "MultiGeometry%s>", szAttributes );

            pszElemClose = "MultiGeometry>";
            pszMemberElem = "geometryMember>";
        }

        AppendString( ppszText, pnLength, pnMaxLength, "<gml:" );
        AppendString( ppszText, pnLength, pnMaxLength, pszElemOpen );

        for( iMember = 0; iMember < poGC->getNumGeometries(); iMember++)
        {
            OGRGeometry *poMember = poGC->getGeometryRef( iMember );

            AppendString( ppszText, pnLength, pnMaxLength, "<gml:" );
            AppendString( ppszText, pnLength, pnMaxLength, pszMemberElem );

            char* pszGMLIdSub = NULL;
            if (pszGMLId != NULL)
                pszGMLIdSub = CPLStrdup(CPLSPrintf("%s.%d", pszGMLId, iMember));

            if( !OGR2GML3GeometryAppend( poMember, poSRS,
                                         ppszText, pnLength, pnMaxLength,
                                         true, bLongSRS, bLineStringAsCurve,
                                         pszGMLIdSub, nSRSDimensionLocFlags, false, NULL ) )
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

        // FREE TAG BUFFER
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
/*                       OGR_G_ExportToGMLTree()                        */
/************************************************************************/

CPLXMLNode *OGR_G_ExportToGMLTree( OGRGeometryH hGeometry )

{
    char        *pszText;
    CPLXMLNode  *psTree;

    pszText = OGR_G_ExportToGML( hGeometry );
    if( pszText == NULL )
        return NULL;

    psTree = CPLParseXMLString( pszText );

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
 * <li> FORMAT=GML3 (GML2 or GML32 also accepted starting with GDAL 2.1). If not set, it will default to GML 2.1.2 output.
 * <li> GML3_LINESTRING_ELEMENT=curve. (Only valid for FORMAT=GML3) To use gml:Curve element for linestrings.
 *     Otherwise gml:LineString will be used .
 * <li> GML3_LONGSRS=YES/NO. (Only valid for FORMAT=GML3) Default to YES. If YES, SRS with EPSG authority will
 *      be written with the "urn:ogc:def:crs:EPSG::" prefix.
 *      In the case, if the SRS is a geographic SRS without explicit AXIS order, but that the same SRS authority code
 *      imported with ImportFromEPSGA() should be treated as lat/long, then the function will take care of coordinate order swapping.
 *      If set to NO, SRS with EPSG authority will be written with the "EPSG:" prefix, even if they are in lat/long order.
 * <li> GMLID=astring. If specified, a gml:id attribute will be written in the top-level geometry element with the provided value.
 *      Required for GML 3.2 compatibility.
 * <li> SRSDIMENSION_LOC=POSLIST/GEOMETRY/GEOMETRY,POSLIST. (Only valid for FORMAT=GML3, GDAL >= 2.0) Default to POSLIST.
 *      For 2.5D geometries, define the location where to attach the srsDimension attribute.
 *      There are diverging implementations. Some put in on the &lt;gml:posList&gt; element, other
 *      on the top geometry element.
 * <li> NAMESPACE_DECL=YES/NO. If set to YES, xmlns:gml="http://www.opengis.net/gml" will be added to the
 *      root node for GML < 3.2 or xmlns:gml="http://www.opengis.net/gml/3.2" for GML 3.2
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
    char        *pszText;
    size_t       nLength = 0, nMaxLength = 1;

    if( hGeometry == NULL )
        return CPLStrdup( "" );

    pszText = (char *) CPLMalloc(nMaxLength);
    pszText[0] = '\0';

    const char* pszFormat = CSLFetchNameValue(papszOptions, "FORMAT");
    bool bNamespaceDecl = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "NAMESPACE_DECL", "NO")) != FALSE;
    if (pszFormat && (EQUAL(pszFormat, "GML3") || EQUAL(pszFormat, "GML32")) )
    {
        const char* pszLineStringElement = CSLFetchNameValue(papszOptions, "GML3_LINESTRING_ELEMENT");
        bool bLineStringAsCurve = (pszLineStringElement && EQUAL(pszLineStringElement, "curve"));
        bool bLongSRS = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "GML3_LONGSRS", "YES")) != FALSE;
        const char* pszGMLId = CSLFetchNameValue(papszOptions, "GMLID");
        if( pszGMLId == NULL && EQUAL(pszFormat, "GML32") )
            CPLError(CE_Warning, CPLE_AppDefined, "FORMAT=GML32 specified but not GMLID set");
        const char* pszSRSDimensionLoc = CSLFetchNameValueDef(papszOptions,"SRSDIMENSION_LOC","POSLIST");
        char** papszSRSDimensionLoc = CSLTokenizeString2(pszSRSDimensionLoc, ",", 0);
        int nSRSDimensionLocFlags = 0;
        for(int i=0; papszSRSDimensionLoc[i] != NULL; i++)
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
        if( !OGR2GML3GeometryAppend( (OGRGeometry *) hGeometry, NULL, &pszText,
                                     &nLength, &nMaxLength, false, bLongSRS,
                                     bLineStringAsCurve, pszGMLId, nSRSDimensionLocFlags, false,
                                     pszNamespaceDecl ))
        {
            CPLFree( pszText );
            return NULL;
        }
        else
            return pszText;
    }

    const char* pszNamespaceDecl = NULL;
    if( bNamespaceDecl )
        pszNamespaceDecl = "http://www.opengis.net/gml";
    if( !OGR2GMLGeometryAppend( (OGRGeometry *) hGeometry, &pszText,
                                &nLength, &nMaxLength, false,
                                pszNamespaceDecl ))
    {
        CPLFree( pszText );
        return NULL;
    }
    else
        return pszText;
}
