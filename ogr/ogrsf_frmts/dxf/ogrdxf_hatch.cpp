/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Implements translation support for HATCH elements as part
 *           of the OGRDXFLayer class.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "ogr_api.h"

#include "ogrdxf_polyline_smooth.h"

CPL_CVSID("$Id$");

#ifndef PI
#define PI  3.14159265358979323846
#endif

/************************************************************************/
/*                           TranslateHATCH()                           */
/*                                                                      */
/*      We mostly just try to convert hatch objects as polygons or      */
/*      multipolygons representing the hatched area.  It is hard to     */
/*      preserve the actual details of the hatching.                    */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateHATCH()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    CPLString osHatchPattern;
    /* int nFillFlag = 0; */
    OGRGeometryCollection oGC;

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 70:
            /* nFillFlag = atoi(szLineBuf); */
            break;

          case 2:
            osHatchPattern = szLineBuf;
            poFeature->SetField( "Text", osHatchPattern.c_str() );
            break;

          case 91:
          {
              int nBoundaryPathCount = atoi(szLineBuf);
              int iBoundary;

              for( iBoundary = 0; iBoundary < nBoundaryPathCount; iBoundary++ )
              {
                  if (CollectBoundaryPath( &oGC ) != OGRERR_NONE)
                      break;
              }
          }
          break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Try to turn the set of lines into something useful.             */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    OGRGeometry* poFinalGeom = (OGRGeometry *)
        OGRBuildPolygonFromEdges( (OGRGeometryH) &oGC,
                                  TRUE, TRUE, 0.0000001, &eErr );
    if( eErr != OGRERR_NONE )
    {
        delete poFinalGeom;
        OGRMultiLineString* poMLS = new OGRMultiLineString();
        for(int i=0;i<oGC.getNumGeometries();i++)
            poMLS->addGeometry(oGC.getGeometryRef(i));
        poFinalGeom = poMLS;
    }

    ApplyOCSTransformer( poFinalGeom );
    poFeature->SetGeometryDirectly( poFinalGeom );

/* -------------------------------------------------------------------- */
/*      Work out the color for this feature.  For now we just assume    */
/*      solid fill.  We cannot trivially translate the various sorts    */
/*      of hatching.                                                    */
/* -------------------------------------------------------------------- */
    CPLString osLayer = poFeature->GetFieldAsString("Layer");

    int nColor = 256;

    if( oStyleProperties.count("Color") > 0 )
        nColor = atoi(oStyleProperties["Color"]);

    // Use layer color? 
    if( nColor < 1 || nColor > 255 )
    {
        const char *pszValue = poDS->LookupLayerProperty( osLayer, "Color" );
        if( pszValue != NULL )
            nColor = atoi(pszValue);
    }
        
/* -------------------------------------------------------------------- */
/*      Setup the style string.                                         */
/* -------------------------------------------------------------------- */
    if( nColor >= 1 && nColor <= 255 )
    {
        CPLString osStyle;
        const unsigned char *pabyDXFColors = ACGetColorTable();
        
        osStyle.Printf( "BRUSH(fc:#%02x%02x%02x)",
                        pabyDXFColors[nColor*3+0],
                        pabyDXFColors[nColor*3+1],
                        pabyDXFColors[nColor*3+2] );
        
        poFeature->SetStyleString( osStyle );
    }

    return poFeature;
}

/************************************************************************/
/*                        CollectBoundaryPath()                         */
/************************************************************************/

OGRErr OGRDXFLayer::CollectBoundaryPath( OGRGeometryCollection *poGC )

{
    int  nCode;
    char szLineBuf[257];

/* -------------------------------------------------------------------- */
/*      Read the boundary path type.                                    */
/* -------------------------------------------------------------------- */
    nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
    if( nCode != 92 )
        return OGRERR_FAILURE;

    int  nBoundaryPathType = atoi(szLineBuf);

/* ==================================================================== */
/*      Handle polyline loops.                                          */
/* ==================================================================== */
    if( nBoundaryPathType & 0x02 )
        return CollectPolylinePath( poGC );

/* ==================================================================== */
/*      Handle non-polyline loops.                                      */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Read number of edges.                                           */
/* -------------------------------------------------------------------- */
    nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
    if( nCode != 93 )
        return OGRERR_FAILURE;

    int nEdgeCount = atoi(szLineBuf);
    
/* -------------------------------------------------------------------- */
/*      Loop reading edges.                                             */
/* -------------------------------------------------------------------- */
    int iEdge;

    for( iEdge = 0; iEdge < nEdgeCount; iEdge++ )
    {
/* -------------------------------------------------------------------- */
/*      Read the edge type.                                             */
/* -------------------------------------------------------------------- */
#define ET_LINE         1
#define ET_CIRCULAR_ARC 2
#define ET_ELLIPTIC_ARC 3
#define ET_SPLINE       4

        nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
        if( nCode != 72 )
            return OGRERR_FAILURE;

        int nEdgeType = atoi(szLineBuf);
        
/* -------------------------------------------------------------------- */
/*      Process a line edge.                                            */
/* -------------------------------------------------------------------- */
        if( nEdgeType == ET_LINE )
        {
            double dfStartX;
            double dfStartY;
            double dfEndX;
            double dfEndY;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 10 )
                dfStartX = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 20 )
                dfStartY = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 11 )
                dfEndX = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 21 )
                dfEndY = CPLAtof(szLineBuf);
            else
                break;

            OGRLineString *poLS = new OGRLineString();

            poLS->addPoint( dfStartX, dfStartY );
            poLS->addPoint( dfEndX, dfEndY );

            poGC->addGeometryDirectly( poLS );
        }
/* -------------------------------------------------------------------- */
/*      Process a circular arc.                                         */
/* -------------------------------------------------------------------- */
        else if( nEdgeType == ET_CIRCULAR_ARC )
        {
            double dfCenterX;
            double dfCenterY;
            double dfRadius;
            double dfStartAngle;
            double dfEndAngle;
            int    bCounterClockwise = FALSE;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 10 )
                dfCenterX = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 20 )
                dfCenterY = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 40 )
                dfRadius = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 50 )
                dfStartAngle = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 51 )
                dfEndAngle = CPLAtof(szLineBuf);
            else
                break;

            if( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) == 73 )
                bCounterClockwise = atoi(szLineBuf);
            else if (nCode >= 0)
                poDS->UnreadValue();

            if( dfStartAngle > dfEndAngle )
                dfEndAngle += 360.0;
            if( bCounterClockwise )
            {
                dfStartAngle *= -1; 
                dfEndAngle *= -1; 
            }

            OGRGeometry *poArc = OGRGeometryFactory::approximateArcAngles( 
                dfCenterX, dfCenterY, 0.0,
                dfRadius, dfRadius, 0.0,
                dfStartAngle, dfEndAngle, 0.0 );

            poArc->flattenTo2D();

            poGC->addGeometryDirectly( poArc );
        }

/* -------------------------------------------------------------------- */
/*      Process an elliptical arc.                                      */
/* -------------------------------------------------------------------- */
        else if( nEdgeType == ET_ELLIPTIC_ARC )
        {
            double dfCenterX;
            double dfCenterY;
            double dfMajorRadius, dfMinorRadius;
            double dfMajorX, dfMajorY;
            double dfStartAngle;
            double dfEndAngle;
            double dfRotation;
            double dfRatio;
            int    bCounterClockwise = FALSE;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 10 )
                dfCenterX = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 20 )
                dfCenterY = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 11 )
                dfMajorX = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 21 )
                dfMajorY = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 40 )
                dfRatio = CPLAtof(szLineBuf) / 100.0;
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 50 )
                dfStartAngle = CPLAtof(szLineBuf);
            else
                break;

            if( poDS->ReadValue(szLineBuf,sizeof(szLineBuf)) == 51 )
                dfEndAngle = CPLAtof(szLineBuf);
            else
                break;

            if( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) == 73 )
                bCounterClockwise = atoi(szLineBuf);
            else if (nCode >= 0)
                poDS->UnreadValue();

            if( dfStartAngle > dfEndAngle )
                dfEndAngle += 360.0;
            if( bCounterClockwise )
            {
                dfStartAngle *= -1; 
                dfEndAngle *= -1; 
            }

            dfMajorRadius = sqrt( dfMajorX * dfMajorX + dfMajorY * dfMajorY );
            dfMinorRadius = dfMajorRadius * dfRatio;

            dfRotation = -1 * atan2( dfMajorY, dfMajorX ) * 180 / PI;

            OGRGeometry *poArc = OGRGeometryFactory::approximateArcAngles( 
                dfCenterX, dfCenterY, 0.0,
                dfMajorRadius, dfMinorRadius, dfRotation,
                dfStartAngle, dfEndAngle, 0.0 );

            poArc->flattenTo2D();

            poGC->addGeometryDirectly( poArc );
        }
        else
        {
            CPLDebug( "DXF", "Unsupported HATCH boundary line type:%d",
                      nEdgeType );
            return OGRERR_UNSUPPORTED_OPERATION;
        }
    }

/* -------------------------------------------------------------------- */
/*      Skip through source boundary objects if present.                */
/* -------------------------------------------------------------------- */
    nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
    if( nCode != 97 )
    {
        if (nCode < 0)
            return OGRERR_FAILURE;
        poDS->UnreadValue();
    }
    else
    {
        int iObj, nObjCount = atoi(szLineBuf);

        for( iObj = 0; iObj < nObjCount; iObj++ )
        {
            if (poDS->ReadValue( szLineBuf, sizeof(szLineBuf) ) < 0)
                return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        CollectPolylinePath()                         */
/************************************************************************/

OGRErr OGRDXFLayer::CollectPolylinePath( OGRGeometryCollection *poGC )

{
    int  nCode;
    char szLineBuf[257];
    DXFSmoothPolyline oSmoothPolyline;
    double dfBulge = 0.0;
    double dfX = 0.0, dfY = 0.0;
    int bHaveX = FALSE, bHaveY = FALSE;
    int bIsClosed = FALSE;
    int nVertexCount = -1;
    int bHaveBulges = FALSE;

/* -------------------------------------------------------------------- */
/*      Read the boundary path type.                                    */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        if( nVertexCount > 0 && (int) oSmoothPolyline.size() == nVertexCount )
            break;

        switch( nCode )
        {
          case 93:
            nVertexCount = atoi(szLineBuf);
            break;

          case 72:
            bHaveBulges = atoi(szLineBuf);
            break;

          case 73:
            bIsClosed = atoi(szLineBuf);
            break;

          case 10:
            if( bHaveX && bHaveY )
            {
                oSmoothPolyline.AddPoint(dfX, dfY, 0.0, dfBulge);
                dfBulge = 0.0;
                bHaveY = FALSE;
            }
            dfX = CPLAtof(szLineBuf);
            bHaveX = TRUE;
            break;

          case 20:
            if( bHaveX && bHaveY )
            {
                oSmoothPolyline.AddPoint( dfX, dfY, 0.0, dfBulge );
                dfBulge = 0.0;
                bHaveX = bHaveY = FALSE;
            }
            dfY = CPLAtof(szLineBuf);
            bHaveY = TRUE;
            if( bHaveX && bHaveY && !bHaveBulges )
            {
                oSmoothPolyline.AddPoint( dfX, dfY, 0.0, dfBulge );
                dfBulge = 0.0;
                bHaveX = bHaveY = FALSE;
            }
            break;

          case 42:
            dfBulge = CPLAtof(szLineBuf);
            if( bHaveX && bHaveY )
            {
                oSmoothPolyline.AddPoint( dfX, dfY, 0.0, dfBulge );
                dfBulge = 0.0;
                bHaveX = bHaveY = FALSE;
            }
            break;

          default:
            break;
        }
    }

    if( nCode != 10 && nCode != 20 && nCode != 42 && nCode >= 0)
        poDS->UnreadValue();

    if( bHaveX && bHaveY )
        oSmoothPolyline.AddPoint(dfX, dfY, 0.0, dfBulge);

    if( bIsClosed )
        oSmoothPolyline.Close();

    poGC->addGeometryDirectly( oSmoothPolyline.Tesselate() );

/* -------------------------------------------------------------------- */
/*      Skip through source boundary objects if present.                */
/* -------------------------------------------------------------------- */
    nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
    if( nCode != 97 )
    {
        if (nCode < 0)
            return OGRERR_FAILURE;
        poDS->UnreadValue();
    }
    else
    {
        int iObj, nObjCount = atoi(szLineBuf);

        for( iObj = 0; iObj < nObjCount; iObj++ )
        {
            if (poDS->ReadValue( szLineBuf, sizeof(szLineBuf) ) < 0)
                return OGRERR_FAILURE;
        }
    }
    return OGRERR_NONE;
}

    
