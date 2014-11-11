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
                               double x, double y, double z, int b3D )

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

static void _GrowBuffer( int nNeeded, char **ppszText, int *pnMaxLength )

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

static void AppendString( char **ppszText, int *pnLength, int *pnMaxLength,
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
                                  char **ppszText, int *pnLength, 
                                  int *pnMaxLength )

{
    char        szCoordinate[256];
    int         b3D = (poLine->getGeometryType() & wkb25DBit);

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

static int OGR2GMLGeometryAppend( OGRGeometry *poGeometry, 
                                  char **ppszText, int *pnLength, 
                                  int *pnMaxLength,
                                  int bIsSubGeometry )

{

/* -------------------------------------------------------------------- */
/*      Check for Spatial Reference System attached to given geometry   */
/* -------------------------------------------------------------------- */

    // Buffer for srsName attribute (srsName="...")
    char szAttributes[30] = { 0 };
    int nAttrsLength = 0;

    const OGRSpatialReference* poSRS = NULL;
    poSRS = poGeometry->getSpatialReference();

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
                    sprintf( szAttributes, " srsName=\"%s:%s\"",
                            pszAuthName, pszAuthCode );

                    nAttrsLength = strlen(szAttributes);
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint )
    {
        char    szCoordinate[256];
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        MakeGMLCoordinate( szCoordinate, 
                           poPoint->getX(), poPoint->getY(), 0.0, FALSE );

        _GrowBuffer( *pnLength + strlen(szCoordinate) + 60 + nAttrsLength,
                     ppszText, pnMaxLength );

        sprintf( *ppszText + *pnLength, 
                "<gml:Point%s><gml:coordinates>%s</gml:coordinates></gml:Point>",
                 szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPoint25D )
    {
        char    szCoordinate[256];
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        MakeGMLCoordinate( szCoordinate, 
                           poPoint->getX(), poPoint->getY(), poPoint->getZ(), 
                           TRUE );
                           
        _GrowBuffer( *pnLength + strlen(szCoordinate) + 70 + nAttrsLength,
                     ppszText, pnMaxLength );

        sprintf( *ppszText + *pnLength, 
                "<gml:Point%s><gml:coordinates>%s</gml:coordinates></gml:Point>",
                 szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbLineString 
             || poGeometry->getGeometryType() == wkbLineString25D )
    {
        int bRing = EQUAL(poGeometry->getGeometryName(),"LINEARRING");

        // Buffer for tag name + srsName attribute if set
        const size_t nLineTagLength = 16;
        char* pszLineTagName = NULL;
        pszLineTagName = (char *) CPLMalloc( nLineTagLength + nAttrsLength + 1 );

        if( bRing )
        {
            sprintf( pszLineTagName, "<gml:LinearRing%s>", szAttributes );

            AppendString( ppszText, pnLength, pnMaxLength,
                          pszLineTagName );
        }
        else
        {
            sprintf( pszLineTagName, "<gml:LineString%s>", szAttributes );

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
    else if( poGeometry->getGeometryType() == wkbPolygon 
             || poGeometry->getGeometryType() == wkbPolygon25D )
    {
        OGRPolygon      *poPolygon = (OGRPolygon *) poGeometry;

        // Buffer for polygon tag name + srsName attribute if set
        const size_t nPolyTagLength = 13;
        char* pszPolyTagName = NULL;
        pszPolyTagName = (char *) CPLMalloc( nPolyTagLength + nAttrsLength + 1 );

        // Compose Polygon tag with or without srsName attribute
        sprintf( pszPolyTagName, "<gml:Polygon%s>", szAttributes );

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
                                        TRUE ) )
            {
                return FALSE;
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
                                        pnMaxLength, TRUE ) )
                return FALSE;
            
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:innerBoundaryIs>" );
        }

        AppendString( ppszText, pnLength, pnMaxLength,
                      "</gml:Polygon>" );
    }

/* -------------------------------------------------------------------- */
/*      MultiPolygon, MultiLineString, MultiPoint, MultiGeometry        */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon 
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint
             || wkbFlatten(poGeometry->getGeometryType()) == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeometry;
        int             iMember;
        const char *pszElemClose = NULL;
        const char *pszMemberElem = NULL;

        // Buffer for opening tag + srsName attribute
        char* pszElemOpen = NULL;

        if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon )
        {
            pszElemOpen = (char *) CPLMalloc( 13 + nAttrsLength + 1 );
            sprintf( pszElemOpen, "MultiPolygon%s>", szAttributes );

            pszElemClose = "MultiPolygon>";
            pszMemberElem = "polygonMember>";
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString )
        {
            pszElemOpen = (char *) CPLMalloc( 16 + nAttrsLength + 1 );
            sprintf( pszElemOpen, "MultiLineString%s>", szAttributes );

            pszElemClose = "MultiLineString>";
            pszMemberElem = "lineStringMember>";
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint )
        {
            pszElemOpen = (char *) CPLMalloc( 11 + nAttrsLength + 1 );
            sprintf( pszElemOpen, "MultiPoint%s>", szAttributes );

            pszElemClose = "MultiPoint>";
            pszMemberElem = "pointMember>";
        }
        else
        {
            pszElemOpen = (char *) CPLMalloc( 19 + nAttrsLength + 1 );
            sprintf( pszElemOpen, "MultiGeometry%s>", szAttributes );

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
                                        TRUE ) )
            {
                return FALSE;
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
        return FALSE;
    }

    return TRUE;
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
                       FALSE );
    pszY = strstr(szCoordinate,",") + 1;
    pszY[-1] = '\0';

    CPLCreateXMLElementAndValue( psCoord, "gml:X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "gml:Y", pszY );

/* -------------------------------------------------------------------- */
/*      Add maxxy coordinate.                                           */
/* -------------------------------------------------------------------- */
    psCoord = CPLCreateXMLNode( psBox, CXT_Element, "gml:coord" );
    
    MakeGMLCoordinate( szCoordinate, sEnvelope.MaxX, sEnvelope.MaxY, 0.0, 
                       FALSE );
    pszY = strstr(szCoordinate,",") + 1;
    pszY[-1] = '\0';

    CPLCreateXMLElementAndValue( psCoord, "gml:X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "gml:Y", pszY );

    return psBox;
}


/************************************************************************/
/*                     AppendGML3CoordinateList()                       */
/************************************************************************/

static void AppendGML3CoordinateList( OGRLineString *poLine, int bCoordSwap,
                                      char **ppszText, int *pnLength,
                                      int *pnMaxLength, int nSRSDimensionLocFlags )

{
    char        szCoordinate[256];
    int         b3D = (poLine->getGeometryType() & wkb25DBit);

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

static int OGR2GML3GeometryAppend( OGRGeometry *poGeometry,
                                   const OGRSpatialReference* poParentSRS,
                                   char **ppszText, int *pnLength,
                                   int *pnMaxLength,
                                   int bIsSubGeometry,
                                   int bLongSRS,
                                   int bLineStringAsCurve,
                                   const char* pszGMLId,
                                   int nSRSDimensionLocFlags )

{

/* -------------------------------------------------------------------- */
/*      Check for Spatial Reference System attached to given geometry   */
/* -------------------------------------------------------------------- */

    // Buffer for srsName, srsDimension and gml:id attributes (srsName="..." gml:id="...")
    char szAttributes[256];
    int nAttrsLength = 0;

    szAttributes[0] = 0;

    const OGRSpatialReference* poSRS = NULL;
    if (poParentSRS)
        poSRS = poParentSRS;
    else
        poParentSRS = poSRS = poGeometry->getSpatialReference();

    int bCoordSwap = FALSE;

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
                                bCoordSwap = TRUE;
                        }
                    }

                    if (!bIsSubGeometry)
                    {
                        if (bLongSRS)
                        {
                            snprintf( szAttributes, sizeof(szAttributes),
                                      " srsName=\"urn:ogc:def:crs:%s::%s\"",
                                      pszAuthName, pszAuthCode );
                        }
                        else
                        {
                            snprintf( szAttributes, sizeof(szAttributes),
                                      " srsName=\"%s:%s\"",
                                      pszAuthName, pszAuthCode );
                        }

                        nAttrsLength = strlen(szAttributes);
                    }
                }
            }
        }
    }

    if( (nSRSDimensionLocFlags & SRSDIM_LOC_GEOMETRY) != 0 &&
        (poGeometry->getGeometryType() & wkb25DBit) != 0 )
    {
        strcat(szAttributes, " srsDimension=\"3\"");
        nAttrsLength = strlen(szAttributes);

        nSRSDimensionLocFlags &= ~SRSDIM_LOC_GEOMETRY;
    }

    if (pszGMLId != NULL && nAttrsLength + 9 + strlen(pszGMLId) + 1 < sizeof(szAttributes))
    {
        strcat(szAttributes, " gml:id=\"");
        strcat(szAttributes, pszGMLId);
        strcat(szAttributes, "\"");
        nAttrsLength = strlen(szAttributes);
    }

/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint )
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

        sprintf( *ppszText + *pnLength,
                "<gml:Point%s><gml:pos>%s</gml:pos></gml:Point>",
                 szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPoint25D )
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

        sprintf( *ppszText + *pnLength,
                "<gml:Point%s><gml:pos>%s</gml:pos></gml:Point>",
                 szAttributes, szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbLineString
             || poGeometry->getGeometryType() == wkbLineString25D )
    {
        int bRing = EQUAL(poGeometry->getGeometryName(),"LINEARRING");
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
            pszLineTagName = (char *) CPLMalloc( nLineTagLength + nAttrsLength + 1 );

            if( bRing )
            {
                /* LinearRing isn't supposed to have srsName attribute according to GML3 SF-0 */
                AppendString( ppszText, pnLength, pnMaxLength,
                            "<gml:LinearRing>" );
            }
            else
            {
                sprintf( pszLineTagName, "<gml:LineString%s>", szAttributes );

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
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPolygon
             || poGeometry->getGeometryType() == wkbPolygon25D )
    {
        OGRPolygon      *poPolygon = (OGRPolygon *) poGeometry;

        // Buffer for polygon tag name + srsName attribute if set
        const size_t nPolyTagLength = 13;
        char* pszPolyTagName = NULL;
        pszPolyTagName = (char *) CPLMalloc( nPolyTagLength + nAttrsLength + 1 );

        // Compose Polygon tag with or without srsName attribute
        sprintf( pszPolyTagName, "<gml:Polygon%s>", szAttributes );

        AppendString( ppszText, pnLength, pnMaxLength,
                      pszPolyTagName );

        // FREE TAG BUFFER
        CPLFree( pszPolyTagName );

        // Don't add srsName to polygon rings

        if( poPolygon->getExteriorRing() != NULL )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:exterior>" );

            if( !OGR2GML3GeometryAppend( poPolygon->getExteriorRing(), poSRS,
                                         ppszText, pnLength, pnMaxLength,
                                         TRUE, bLongSRS, bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags) )
            {
                return FALSE;
            }

            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:exterior>" );
        }

        for( int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++ )
        {
            OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            AppendString( ppszText, pnLength, pnMaxLength,
                          "<gml:interior>" );

            if( !OGR2GML3GeometryAppend( poRing, poSRS, ppszText, pnLength,
                                         pnMaxLength, TRUE, bLongSRS,
                                         bLineStringAsCurve,
                                         NULL, nSRSDimensionLocFlags) )
                return FALSE;

            AppendString( ppszText, pnLength, pnMaxLength,
                          "</gml:interior>" );
        }

        AppendString( ppszText, pnLength, pnMaxLength,
                      "</gml:Polygon>" );
    }

/* -------------------------------------------------------------------- */
/*      MultiPolygon, MultiLineString, MultiPoint, MultiGeometry        */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint
             || wkbFlatten(poGeometry->getGeometryType()) == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeometry;
        int             iMember;
        const char *pszElemClose = NULL;
        const char *pszMemberElem = NULL;

        // Buffer for opening tag + srsName attribute
        char* pszElemOpen = NULL;

        if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon )
        {
            pszElemOpen = (char *) CPLMalloc( 13 + nAttrsLength + 1 );
            sprintf( pszElemOpen, "MultiSurface%s>", szAttributes );

            pszElemClose = "MultiSurface>";
            pszMemberElem = "surfaceMember>";
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString )
        {
            pszElemOpen = (char *) CPLMalloc( 16 + nAttrsLength + 1 );
            sprintf( pszElemOpen, "MultiCurve%s>", szAttributes );

            pszElemClose = "MultiCurve>";
            pszMemberElem = "curveMember>";
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint )
        {
            pszElemOpen = (char *) CPLMalloc( 11 + nAttrsLength + 1 );
            sprintf( pszElemOpen, "MultiPoint%s>", szAttributes );

            pszElemClose = "MultiPoint>";
            pszMemberElem = "pointMember>";
        }
        else
        {
            pszElemOpen = (char *) CPLMalloc( 19 + nAttrsLength + 1 );
            sprintf( pszElemOpen, "MultiGeometry%s>", szAttributes );

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
                                         TRUE, bLongSRS, bLineStringAsCurve,
                                         pszGMLIdSub, nSRSDimensionLocFlags ) )
            {
                CPLFree(pszGMLIdSub);
                return FALSE;
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
        return FALSE;
    }

    return TRUE;
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
 * <li> FORMAT=GML3. Otherwise it will default to GML 2.1.2 output.
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
 * </ul>
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
    int         nLength = 0, nMaxLength = 1;

    if( hGeometry == NULL )
        return CPLStrdup( "" );

    pszText = (char *) CPLMalloc(nMaxLength);
    pszText[0] = '\0';

    const char* pszFormat = CSLFetchNameValue(papszOptions, "FORMAT");
    if (pszFormat && EQUAL(pszFormat, "GML3"))
    {
        const char* pszLineStringElement = CSLFetchNameValue(papszOptions, "GML3_LINESTRING_ELEMENT");
        int bLineStringAsCurve = (pszLineStringElement && EQUAL(pszLineStringElement, "curve"));
        int bLongSRS = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "GML3_LONGSRS", "YES"));
        const char* pszGMLId = CSLFetchNameValue(papszOptions, "GMLID");
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
        if( !OGR2GML3GeometryAppend( (OGRGeometry *) hGeometry, NULL, &pszText,
                                    &nLength, &nMaxLength, FALSE, bLongSRS,
                                     bLineStringAsCurve, pszGMLId, nSRSDimensionLocFlags ))
        {
            CPLFree( pszText );
            return NULL;
        }
        else
            return pszText;
    }

    if( !OGR2GMLGeometryAppend( (OGRGeometry *) hGeometry, &pszText,
                                &nLength, &nMaxLength, FALSE ))
    {
        CPLFree( pszText );
        return NULL;
    }
    else
        return pszText;
}
