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
 ****************************************************************************/
#include "cpl_minixml.h"
#include "ogr_geometry.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "cpl_error.h"
#include "cpl_conv.h"


#define EPSILON 1e-8

/************************************************************************/
/*                        MakeKMLCoordinate()                           */
/************************************************************************/

static void MakeKMLCoordinate( char *pszTarget, 
                               double x, double y, double z, int b3D )

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
            static int bFirstWarning = TRUE;
            if (bFirstWarning)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Latitude %f is invalid. Valid range is [-90,90]. This warning will not be issued any more",
                        y);
                bFirstWarning = FALSE;
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
            static int bFirstWarning = TRUE;
            if (bFirstWarning)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Longitude %f has been modified to fit into range [-180,180]. This warning will not be issued any more",
                        x);
                bFirstWarning = FALSE;
            }
    
            if (x > 180)
                x -= ((int) ((x+180)/360)*360);
            else if (x < -180)
                x += ((int) (180 - x)/360)*360;
        }
    }

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
    char szCoordinate[256]= { 0 };
    int b3D = (poLine->getGeometryType() & wkb25DBit);

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
                                  int *pnMaxLength, char * szAltitudeMode )

{
/* -------------------------------------------------------------------- */
/*      2D Point                                                        */
/* -------------------------------------------------------------------- */
    if( poGeometry->getGeometryType() == wkbPoint )
    {
        char szCoordinate[256] = { 0 };
        OGRPoint* poPoint = static_cast<OGRPoint*>(poGeometry);

        if (poPoint->getCoordinateDimension() == 0)
        {
            _GrowBuffer( *pnLength + 10, 
                     ppszText, pnMaxLength );
            strcat( *ppszText + *pnLength, "<Point/>");
            *pnLength += strlen( *ppszText + *pnLength );
        }
        else
        {
            MakeKMLCoordinate( szCoordinate, 
                            poPoint->getX(), poPoint->getY(), 0.0, FALSE );

            _GrowBuffer( *pnLength + strlen(szCoordinate) + 60, 
                        ppszText, pnMaxLength );

            sprintf( *ppszText + *pnLength, 
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
        OGRPoint *poPoint = static_cast<OGRPoint*>(poGeometry);

        MakeKMLCoordinate( szCoordinate, 
                           poPoint->getX(), poPoint->getY(), poPoint->getZ(), 
                           TRUE );
                           
        if (NULL == szAltitudeMode) 
        { 
            _GrowBuffer( *pnLength + strlen(szCoordinate) + 70, 
                         ppszText, pnMaxLength );

            sprintf( *ppszText + *pnLength,  
                     "<Point><coordinates>%s</coordinates></Point>",
                     szCoordinate );
        }
        else
        { 
            _GrowBuffer( *pnLength + strlen(szCoordinate) 
                         + strlen(szAltitudeMode) + 70, 
                         ppszText, pnMaxLength );

            sprintf( *ppszText + *pnLength,  
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
        int bRing = EQUAL(poGeometry->getGeometryName(),"LINEARRING");

        if( bRing )
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<LinearRing>" );
        else
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<LineString>" );

        if (NULL != szAltitudeMode) 
        { 
            AppendString( ppszText, pnLength, pnMaxLength, szAltitudeMode); 
        }

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
        OGRPolygon* poPolygon = static_cast<OGRPolygon*>(poGeometry);

        AppendString( ppszText, pnLength, pnMaxLength, "<Polygon>" );

        if (NULL != szAltitudeMode) 
        { 
            AppendString( ppszText, pnLength, pnMaxLength, szAltitudeMode); 
        }

        if( poPolygon->getExteriorRing() != NULL )
        {
            AppendString( ppszText, pnLength, pnMaxLength,
                          "<outerBoundaryIs>" );

            if( !OGR2KMLGeometryAppend( poPolygon->getExteriorRing(), 
                                        ppszText, pnLength, pnMaxLength, szAltitudeMode ) )
            {
                return FALSE;
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
                return FALSE;
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
             || wkbFlatten(poGeometry->getGeometryType()) == wkbGeometryCollection )
    {
        OGRGeometryCollection* poGC = NULL;
        poGC = static_cast<OGRGeometryCollection*>(poGeometry);

        AppendString( ppszText, pnLength, pnMaxLength, "<MultiGeometry>" );

        // XXX - mloskot
        //if (NULL != szAltitudeMode) 
        //{ 
        //    AppendString( ppszText, pnLength, pnMaxLength, szAltitudeMode); 
        //}

        for( int iMember = 0; iMember < poGC->getNumGeometries(); iMember++)
        {
            OGRGeometry *poMember = poGC->getGeometryRef( iMember );

            if( !OGR2KMLGeometryAppend( poMember, ppszText, pnLength, pnMaxLength, szAltitudeMode ) )
            {
                return FALSE;
            }
        }

		AppendString( ppszText, pnLength, pnMaxLength, "</MultiGeometry>" );
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                   OGR_G_ExportEnvelopeToKMLTree()                    */
/*                                                                      */
/*      Export the envelope of a geometry as a KML:Box.                 */
/************************************************************************/

CPLXMLNode* OGR_G_ExportEnvelopeToKMLTree( OGRGeometryH hGeometry )
{
    VALIDATE_POINTER1( hGeometry, "OGR_G_ExportEnvelopeToKMLTree", NULL );

    CPLXMLNode* psBox = NULL;
    CPLXMLNode* psCoord = NULL;
    OGREnvelope sEnvelope;
    char szCoordinate[256] = { 0 };
    char* pszY = NULL;

    memset( &sEnvelope, 0, sizeof(sEnvelope) );
    ((OGRGeometry*)(hGeometry))->getEnvelope( &sEnvelope );

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

char *OGR_G_ExportToKML( OGRGeometryH hGeometry, const char *pszAltitudeMode )
{
    char* pszText = NULL;
    int nLength = 0;
    int nMaxLength = 1;
    char szAltitudeMode[128] =  { 0 }; 

    // TODO - mloskot: Shouldn't we use VALIDATE_POINTER1 here?
    if( hGeometry == NULL )
        return CPLStrdup( "" );

    pszText = (char *) CPLMalloc(nMaxLength);
    pszText[0] = '\0';

    if (NULL != pszAltitudeMode) 
 	{ 
        sprintf(szAltitudeMode, "<altitudeMode>%s</altitudeMode>", pszAltitudeMode); 
 	} 
 	else 
 	{ 
 	    szAltitudeMode[0] = 0; 
 	} 

    if( !OGR2KMLGeometryAppend( (OGRGeometry *) hGeometry, &pszText, 
                                &nLength, &nMaxLength, szAltitudeMode ))
    {
        CPLFree( pszText );
        return NULL;
    }
    
    return pszText;
}

/************************************************************************/
/*                       OGR_G_ExportToKMLTree()                        */
/************************************************************************/

CPLXMLNode *OGR_G_ExportToKMLTree( OGRGeometryH hGeometry )
{
    char        *pszText = NULL;
    CPLXMLNode  *psTree = NULL;

    // TODO - mloskot: If passed geometry is null the pszText is non-null,
    // so the condition below is false.
    pszText = OGR_G_ExportToKML( hGeometry, NULL );
    if( pszText == NULL )
        return NULL;

    psTree = CPLParseXMLString( pszText );

    CPLFree( pszText );

    return psTree;
}

