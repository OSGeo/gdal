/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Implementation of OGR -> KML geometries writer.
 * Author:   Christopher Condit, condit@sdsc.edu
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2006/07/27 19:53:01  mloskot
 * Added common file header to KML driver source files.
 *
 *
 */
#include "cpl_minixml.h"
#include "ogr_geometry.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "cpl_error.h"
#include "cpl_conv.h"

/************************************************************************/
/*                        MakeKMLCoordinate()                           */
/************************************************************************/

static void MakeKMLCoordinate( char *pszTarget, 
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
            sprintf( pszTarget, "%.16g,%.16g", x, y );
        else if( fabs(x) > 100000000.0 || fabs(y) > 100000000.0 )
            sprintf( pszTarget, "%.16g,%.16g", x, y );
        else
            sprintf( pszTarget, "%.3f,%.3f", x, y );
    }
    else
    {
        if( x == (int) x && y == (int) y && z == (int) z )
            sprintf( pszTarget, "%d,%d,%d", (int) x, (int) y, (int) z );
        else if( fabs(x) < 370 && fabs(y) < 370 )
            sprintf( pszTarget, "%.16g,%.16g,%.16g", x, y, z );
        else if( fabs(x) > 100000000.0 || fabs(y) > 100000000.0 
                 || fabs(z) > 100000000.0 )
            sprintf( pszTarget, "%.16g,%.16g,%.16g", x, y, z );
        else
            sprintf( pszTarget, "%.3f,%.3f,%.3f", x, y, z );
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

    strcat( *ppszText + *pnLength, "<coordinates>" );
    *pnLength += strlen(*ppszText + *pnLength);

    
    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        MakeKMLCoordinate( szCoordinate, 
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
    strcat( *ppszText + *pnLength, "</coordinates>" );
    *pnLength += strlen(*ppszText + *pnLength);
}

/************************************************************************/
/*                       OGR2KMLGeometryAppend()                        */
/************************************************************************/

static int OGR2KMLGeometryAppend( OGRGeometry *poGeometry, 
                                  char **ppszText, int *pnLength, 
                                  int *pnMaxLength )

{
/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint )
    {
        char    szCoordinate[256];
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        MakeKMLCoordinate( szCoordinate, 
                           poPoint->getX(), poPoint->getY(), 0.0, FALSE );
                           
        _GrowBuffer( *pnLength + strlen(szCoordinate) + 60, 
                     ppszText, pnMaxLength );

        sprintf( *ppszText + *pnLength, 
                "<Point><coordinates>%s</coordinates></Point>",
                 szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPoint25D )
    {
        char    szCoordinate[256];
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        MakeKMLCoordinate( szCoordinate, 
                           poPoint->getX(), poPoint->getY(), poPoint->getZ(), 
                           TRUE );
                           
        _GrowBuffer( *pnLength + strlen(szCoordinate) + 70, 
                     ppszText, pnMaxLength );

        sprintf( *ppszText + *pnLength, 
                "<Point><coordinates>%s</coordinates></Point>",
                 szCoordinate );

        *pnLength += strlen( *ppszText + *pnLength );
    }

/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbLineString 
             || poGeometry->getGeometryType() == wkbLineString25D )
    {
        int bRing = EQUAL(poGeometry->getGeometryName(),"LINEARRING");

        if( bRing )
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<LinearRing>" );
        else
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<LineString>" );

        AppendCoordinateList( (OGRLineString *) poGeometry, 
                              ppszText, pnLength, pnMaxLength );
        
        if( bRing )
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</LinearRing>" );
        else
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</LineString>" );
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPolygon 
             || poGeometry->getGeometryType() == wkbPolygon25D )
    {
        OGRPolygon      *poPolygon = (OGRPolygon *) poGeometry;

        AppendString( ppszText, pnLength, pnMaxLength,
                      "<Polygon>" );

        if( poPolygon->getExteriorRing() != NULL )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<outerBoundaryIs>" );

            if( !OGR2KMLGeometryAppend( poPolygon->getExteriorRing(), 
                                        ppszText, pnLength, pnMaxLength ) )
                return FALSE;
            
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</outerBoundaryIs>" );
        }

        for( int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++ )
        {
            OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            AppendString( ppszText, pnLength, pnMaxLength,
                          "<innerBoundaryIs>" );
            
            if( !OGR2KMLGeometryAppend( poRing, ppszText, pnLength, 
                                        pnMaxLength ) )
                return FALSE;
            
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</innerBoundaryIs>" );
        }

        AppendString( ppszText, pnLength, pnMaxLength,
                      "</Polygon>" );
    }

/* -------------------------------------------------------------------- */
/*      MultiPolygon                                                    */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon 
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString
             || wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint
             || wkbFlatten(poGeometry->getGeometryType()) == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeometry;
        int             iMember;
        const char *pszElem, *pszMemberElem;

        if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon )
        {
            pszElem = "MultiPolygon>";
            pszMemberElem = "polygonMember>";
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiLineString )
        {
            pszElem = "MultiLineString>";
            pszMemberElem = "lineStringMember>";
        }
        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint )
        {
            pszElem = "MultiPoint>";
            pszMemberElem = "pointMember>";
        }
        else
        {
            pszElem = "GeometryCollection>";
            pszMemberElem = "geometryMember>";
        }

        AppendString( ppszText, pnLength, pnMaxLength, "<" );
        AppendString( ppszText, pnLength, pnMaxLength, pszElem );

        for( iMember = 0; iMember < poGC->getNumGeometries(); iMember++)
        {
            OGRGeometry *poMember = poGC->getGeometryRef( iMember );

            AppendString( ppszText, pnLength, pnMaxLength, "<" );
            AppendString( ppszText, pnLength, pnMaxLength, pszMemberElem );
            
            if( !OGR2KMLGeometryAppend( poMember, 
                                        ppszText, pnLength, pnMaxLength ) )
                return FALSE;
            
            AppendString( ppszText, pnLength, pnMaxLength, "</" );
            AppendString( ppszText, pnLength, pnMaxLength, pszMemberElem );
        }

        AppendString( ppszText, pnLength, pnMaxLength, "</" );
        AppendString( ppszText, pnLength, pnMaxLength, pszElem );
    }

    else
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                   OGR_G_ExportEnvelopeToKMLTree()                    */
/*                                                                      */
/*      Export the envelope of a geometry as a KML:Box.                 */
/************************************************************************/

CPLXMLNode *OGR_G_ExportEnvelopeToKMLTree( OGRGeometryH hGeometry )
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

    psBox = CPLCreateXMLNode( NULL, CXT_Element, "Box" );

/* -------------------------------------------------------------------- */
/*      Add minxy coordinate.                                           */
/* -------------------------------------------------------------------- */
    psCoord = CPLCreateXMLNode( psBox, CXT_Element, "coord" );
    
    MakeKMLCoordinate( szCoordinate, sEnvelope.MinX, sEnvelope.MinY, 0.0, 
                       FALSE );
    pszY = strstr(szCoordinate,",") + 1;
    pszY[-1] = '\0';

    CPLCreateXMLElementAndValue( psCoord, "X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "Y", pszY );

/* -------------------------------------------------------------------- */
/*      Add maxxy coordinate.                                           */
/* -------------------------------------------------------------------- */
    psCoord = CPLCreateXMLNode( psBox, CXT_Element, "coord" );
    
    MakeKMLCoordinate( szCoordinate, sEnvelope.MaxX, sEnvelope.MaxY, 0.0, 
                       FALSE );
    pszY = strstr(szCoordinate,",") + 1;
    pszY[-1] = '\0';

    CPLCreateXMLElementAndValue( psCoord, "X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "Y", pszY );

    return psBox;
}

/************************************************************************/
/*                         OGR_G_ExportToKML()                          */
/************************************************************************/
char *OGR_G_ExportToKML( OGRGeometryH hGeometry )
{
    char        *pszText;
    int         nLength = 0, nMaxLength = 1;

    if( hGeometry == NULL )
        return CPLStrdup( "" );

    pszText = (char *) CPLMalloc(nMaxLength);
    pszText[0] = '\0';

    if( !OGR2KMLGeometryAppend( (OGRGeometry *) hGeometry, &pszText, 
                                &nLength, &nMaxLength ))
    {
        CPLFree( pszText );
        return NULL;
    }
    else
        return pszText;
}

/************************************************************************/
/*                       OGR_G_ExportToKMLTree()                        */
/************************************************************************/

CPLXMLNode *OGR_G_ExportToKMLTree( OGRGeometryH hGeometry )
{
    char        *pszText;
    CPLXMLNode  *psTree;

    pszText = OGR_G_ExportToKML( hGeometry );
    if( pszText == NULL )
        return NULL;

    psTree = CPLParseXMLString( pszText );

    CPLFree( pszText );

    return psTree;
}
