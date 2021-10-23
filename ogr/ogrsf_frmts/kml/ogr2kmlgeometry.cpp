/******************************************************************************
 *
 * Project:  KML Driver
 * Purpose:  Implementation of OGR -> KML geometries writer.
 * Author:   Christopher Condit, condit@sdsc.edu
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
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
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_api.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

constexpr double EPSILON = 1e-8;

/************************************************************************/
/*                        MakeKMLCoordinate()                           */
/************************************************************************/

static void MakeKMLCoordinate( char *pszTarget, size_t nTargetLen,
                               double x, double y, double z, bool b3D )

{
    if (y < -90 || y > 90)
    {
        if (y > 90 && y < 90 + EPSILON)
        {
            y = 90;
        }
        else if (y > -90 - EPSILON  && y < -90)
        {
            y = -90;
        }
        else
        {
            static bool bFirstWarning = true;
            if( bFirstWarning )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Latitude %f is invalid. Valid range is [-90,90]. "
                          "This warning will not be issued any more",
                          y );
                bFirstWarning = false;
            }
        }
    }

    if (x < -180 || x > 180)
    {
        if (x > 180 && x < 180 + EPSILON)
        {
            x = 180;
        }
        else if (x > -180 - EPSILON  && x < -180)
        {
            x = -180;
        }
        else
        {
            static bool bFirstWarning = true;
            if( bFirstWarning )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Longitude %f has been modified to fit into "
                          "range [-180,180]. This warning will not be "
                          "issued any more",
                          x );
                bFirstWarning = false;
            }

            // Trash drastically non-sensical values.
            if( x > 1.0e6 || x < -1.0e6 || CPLIsNan(x) )
            {
                static bool bFirstWarning2 = true;
                if( bFirstWarning2 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Longitude %lf is unreasonable.  Setting to 0."
                             "This warning will not be issued any more", x);
                    bFirstWarning2 = false;
                }
                x = 0.0;
            }

            if (x > 180)
                x -= (static_cast<int>((x+180)/360)*360);
            else if (x < -180)
                x += (static_cast<int>(180 - x)/360)*360;
        }
    }

    OGRMakeWktCoordinate( pszTarget, x, y, z, b3D ? 3 : 2 );
    while( *pszTarget != '\0' )
    {
        if( *pszTarget == ' ' )
            *pszTarget = ',';
        pszTarget++;
        nTargetLen --;
    }

#if 0
    if( !b3D )
    {
        if( x == static_cast<int>(x) && y == static_cast<int>(y) )
            snprintf( pszTarget, nTargetLen, "%d,%d",
                      static_cast<int>(x), static_cast<int>(y) );
        else if( fabs(x) < 370 && fabs(y) < 370 )
            CPLsnprintf( pszTarget, nTargetLen, "%.16g,%.16g", x, y );
        else if( fabs(x) > 100000000.0 || fabs(y) > 100000000.0 )
            CPLsnprintf( pszTarget, nTargetLen, "%.16g,%.16g", x, y );
        else
            CPLsnprintf( pszTarget, nTargetLen, "%.3f,%.3f", x, y );
    }
    else
    {
        if( x == static_cast<int>(x) &&
            y == static_cast<int>(y) &&
            z == static_cast<int>(z) )
            snprintf( pszTarget, nTargetLen, "%d,%d,%d",
                      static_cast<int>(x), static_cast<int>(y),
                      static_cast<int>(z) );
        else if( fabs(x) < 370 && fabs(y) < 370 )
            CPLsnprintf( pszTarget, nTargetLen, "%.16g,%.16g,%.16g", x, y, z );
        else if( fabs(x) > 100000000.0 || fabs(y) > 100000000.0
                 || fabs(z) > 100000000.0 )
            CPLsnprintf( pszTarget, nTargetLen, "%.16g,%.16g,%.16g", x, y, z );
        else
            CPLsnprintf( pszTarget, nTargetLen, "%.3f,%.3f,%.3f", x, y, z );
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
        *pnMaxLength = std::max(*pnMaxLength * 2, nNeeded + 1);
        *ppszText = static_cast<char *>( CPLRealloc(*ppszText, *pnMaxLength) );
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
    char szCoordinate[256]= { 0 };
    const bool b3D = CPL_TO_BOOL(wkbHasZ(poLine->getGeometryType()));

    *pnLength += strlen(*ppszText + *pnLength);
    _GrowBuffer( *pnLength + 20, ppszText, pnMaxLength );

    strcat( *ppszText + *pnLength, "<coordinates>" );
    *pnLength += strlen(*ppszText + *pnLength);

    for( int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++ )
    {
        MakeKMLCoordinate( szCoordinate, sizeof(szCoordinate),
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

static bool OGR2KMLGeometryAppend( OGRGeometry *poGeometry,
                                   char **ppszText, size_t *pnLength,
                                   size_t *pnMaxLength, char *szAltitudeMode )

{
/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint )
    {
        OGRPoint* poPoint = poGeometry->toPoint();

        if (poPoint->getCoordinateDimension() == 0)
        {
            _GrowBuffer( *pnLength + 10,
                     ppszText, pnMaxLength );
            strcat( *ppszText + *pnLength, "<Point/>");
            *pnLength += strlen( *ppszText + *pnLength );
        }
        else
        {
            char szCoordinate[256] = { 0 };
            MakeKMLCoordinate( szCoordinate, sizeof(szCoordinate),
                               poPoint->getX(), poPoint->getY(), 0.0, false );

            _GrowBuffer( *pnLength + strlen(szCoordinate) + 60,
                         ppszText, pnMaxLength );

            snprintf( *ppszText + *pnLength, *pnMaxLength - *pnLength,
                      "<Point><coordinates>%s</coordinates></Point>",
                      szCoordinate );

            *pnLength += strlen( *ppszText + *pnLength );
        }
    }
/* -------------------------------------------------------------------- */
/*      3D Point                                                        */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbPoint25D )
    {
        char szCoordinate[256] = { 0 };
        OGRPoint *poPoint = poGeometry->toPoint();

        MakeKMLCoordinate( szCoordinate, sizeof(szCoordinate),
                           poPoint->getX(), poPoint->getY(), poPoint->getZ(),
                           true );

        if (nullptr == szAltitudeMode)
        {
            _GrowBuffer( *pnLength + strlen(szCoordinate) + 70,
                         ppszText, pnMaxLength );

            snprintf( *ppszText + *pnLength, *pnMaxLength - *pnLength,
                      "<Point><coordinates>%s</coordinates></Point>",
                      szCoordinate );
        }
        else
        {
            _GrowBuffer( *pnLength + strlen(szCoordinate)
                         + strlen(szAltitudeMode) + 70,
                         ppszText, pnMaxLength );

            snprintf( *ppszText + *pnLength, *pnMaxLength - *pnLength,
                      "<Point>%s<coordinates>%s</coordinates></Point>",
                      szAltitudeMode, szCoordinate );
        }

        *pnLength += strlen( *ppszText + *pnLength );
    }
/* -------------------------------------------------------------------- */
/*      LineString and LinearRing                                       */
/* -------------------------------------------------------------------- */
    else if( poGeometry->getGeometryType() == wkbLineString
             || poGeometry->getGeometryType() == wkbLineString25D )
    {
        const bool bRing = EQUAL(poGeometry->getGeometryName(),"LINEARRING");

        if( bRing )
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<LinearRing>" );
        else
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<LineString>" );

        if (nullptr != szAltitudeMode)
        {
            AppendString( ppszText, pnLength, pnMaxLength, szAltitudeMode);
        }

        AppendCoordinateList( poGeometry->toLineString(),
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
        OGRPolygon* poPolygon = poGeometry->toPolygon();

        AppendString( ppszText, pnLength, pnMaxLength, "<Polygon>" );

        if (nullptr != szAltitudeMode)
        {
            AppendString( ppszText, pnLength, pnMaxLength, szAltitudeMode);
        }

        if( poPolygon->getExteriorRing() != nullptr )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<outerBoundaryIs>" );

            if( !OGR2KMLGeometryAppend( poPolygon->getExteriorRing(),
                                        ppszText, pnLength, pnMaxLength,
                                        szAltitudeMode ) )
            {
                return false;
            }
            AppendString( ppszText, pnLength, pnMaxLength,
                          "</outerBoundaryIs>" );
        }

        for( int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++ )
        {
            OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            AppendString( ppszText, pnLength, pnMaxLength,
                          "<innerBoundaryIs>" );

            if( !OGR2KMLGeometryAppend( poRing, ppszText, pnLength,
                                        pnMaxLength, szAltitudeMode ) )
            {
                return false;
            }
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
             || wkbFlatten(poGeometry->getGeometryType()) ==
                wkbGeometryCollection )
    {
        OGRGeometryCollection* poGC = poGeometry->toGeometryCollection();

        AppendString( ppszText, pnLength, pnMaxLength, "<MultiGeometry>" );

        // XXX - mloskot
        //if (NULL != szAltitudeMode)
        //{
        //    AppendString( ppszText, pnLength, pnMaxLength, szAltitudeMode);
        //}

        for( auto&& poMember: poGC )
        {
            if( !OGR2KMLGeometryAppend( poMember, ppszText, pnLength,
                                        pnMaxLength, szAltitudeMode ) )
            {
                return false;
            }
        }

        AppendString( ppszText, pnLength, pnMaxLength, "</MultiGeometry>" );
    }
    else
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                   OGR_G_ExportEnvelopeToKMLTree()                    */
/*                                                                      */
/*      Export the envelope of a geometry as a KML:Box.                 */
/************************************************************************/

#if 0
CPLXMLNode* OGR_G_ExportEnvelopeToKMLTree( OGRGeometryH hGeometry )
{
    VALIDATE_POINTER1( hGeometry, "OGR_G_ExportEnvelopeToKMLTree", NULL );

    OGREnvelope sEnvelope;

    memset( &sEnvelope, 0, sizeof(sEnvelope) );
    ((OGRGeometry*)(hGeometry))->getEnvelope( &sEnvelope );

    if( sEnvelope.MinX == 0 && sEnvelope.MaxX == 0
        && sEnvelope.MaxX == 0 && sEnvelope.MaxY == 0 )
    {
        /* There is apparently a special way of representing a null box
           geometry ... we should use it here eventually. */

        return NULL;
    }

    CPLXMLNode* psBox = CPLCreateXMLNode( NULL, CXT_Element, "Box" );

/* -------------------------------------------------------------------- */
/*      Add minxy coordinate.                                           */
/* -------------------------------------------------------------------- */
    CPLXMLNode* psCoord = CPLCreateXMLNode( psBox, CXT_Element, "coord" );

    char szCoordinate[256] = { 0 };
    MakeKMLCoordinate( szCoordinate, sEnvelope.MinX, sEnvelope.MinY, 0.0,
                       false );
    char* pszY = strstr(szCoordinate,",") + 1;
    pszY[-1] = '\0';

    CPLCreateXMLElementAndValue( psCoord, "X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "Y", pszY );

/* -------------------------------------------------------------------- */
/*      Add maxxy coordinate.                                           */
/* -------------------------------------------------------------------- */
    psCoord = CPLCreateXMLNode( psBox, CXT_Element, "coord" );

    MakeKMLCoordinate( szCoordinate, sEnvelope.MaxX, sEnvelope.MaxY, 0.0,
                       false );
    pszY = strstr(szCoordinate,",") + 1;
    pszY[-1] = '\0';

    CPLCreateXMLElementAndValue( psCoord, "X", szCoordinate );
    CPLCreateXMLElementAndValue( psCoord, "Y", pszY );

    return psBox;
}
#endif

/************************************************************************/
/*                         OGR_G_ExportToKML()                          */
/************************************************************************/

/**
 * \brief Convert a geometry into KML format.
 *
 * The returned string should be freed with CPLFree() when no longer required.
 *
 * This method is the same as the C++ method OGRGeometry::exportToKML().
 *
 * @param hGeometry handle to the geometry.
 * @param pszAltitudeMode value to write in altitudeMode element, or NULL.
 * @return A KML fragment or NULL in case of error.
 */

char *OGR_G_ExportToKML( OGRGeometryH hGeometry, const char *pszAltitudeMode )
{
    char szAltitudeMode[128];

    // TODO - mloskot: Should we use VALIDATE_POINTER1 here?
    if( hGeometry == nullptr )
        return CPLStrdup( "" );

    size_t nMaxLength = 1;
    char* pszText = static_cast<char *>(CPLMalloc(nMaxLength));
    pszText[0] = '\0';

    if (nullptr != pszAltitudeMode && strlen(pszAltitudeMode) < 128 - (29 + 1))
    {
        snprintf( szAltitudeMode, sizeof(szAltitudeMode),
                  "<altitudeMode>%s</altitudeMode>", pszAltitudeMode);
    }
    else
    {
        szAltitudeMode[0] = 0;
    }

    size_t nLength = 0;
    if( !OGR2KMLGeometryAppend(
        reinterpret_cast<OGRGeometry *>(hGeometry), &pszText,
        &nLength, &nMaxLength, szAltitudeMode ) )
    {
        CPLFree( pszText );
        return nullptr;
    }

    return pszText;
}

/************************************************************************/
/*                       OGR_G_ExportToKMLTree()                        */
/************************************************************************/

#if 0
CPLXMLNode *OGR_G_ExportToKMLTree( OGRGeometryH hGeometry )
{
    // TODO - mloskot: If passed geometry is null the pszText is non-null,
    // so the condition below is false.
    char *pszText = OGR_G_ExportToKML( hGeometry, NULL );
    if( pszText == NULL )
        return NULL;

    CPLXMLNode *psTree = CPLParseXMLString( pszText );

    CPLFree( pszText );

    return psTree;
}
#endif
