/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements translation support for HATCH elements as part
 *           of the OGRDXFLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2017, Alan Thomas <alant@outlook.com.au>
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

#include <algorithm>
#include <cmath>
#include "ogrdxf_polyline_smooth.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           TranslateHATCH()                           */
/*                                                                      */
/*      We mostly just try to convert hatch objects as polygons or      */
/*      multipolygons representing the hatched area.  It is hard to     */
/*      preserve the actual details of the hatching.                    */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateHATCH()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );

    CPLString osHatchPattern;
    double dfElevation = 0.0;  // Z value to be used for EVERY point
    /* int nFillFlag = 0; */
    OGRGeometryCollection oGC;

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 30:
            // Constant elevation.
            dfElevation = CPLAtof( szLineBuf );
            break;

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

              for( int iBoundary = 0;
                   iBoundary < nBoundaryPathCount;
                   iBoundary++ )
              {
                  if (CollectBoundaryPath( &oGC, dfElevation ) != OGRERR_NONE)
                      break;
              }
          }
          break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return nullptr;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Obtain a tolerance value used when building the polygon.        */
/* -------------------------------------------------------------------- */
   double dfTolerance = atof( CPLGetConfigOption( "DXF_HATCH_TOLERANCE", "-1" ) );
   if( dfTolerance < 0 )
   {
       // If the configuration variable isn't set, compute the bounding box
       // and work out a tolerance from that
       OGREnvelope oEnvelope;
       oGC.getEnvelope( &oEnvelope );
       dfTolerance = std::max( oEnvelope.MaxX - oEnvelope.MinX,
           oEnvelope.MaxY - oEnvelope.MinY ) * 1e-7;
   }

/* -------------------------------------------------------------------- */
/*      Try to turn the set of lines into something useful.             */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    OGRGeometry* poFinalGeom = (OGRGeometry *)
        OGRBuildPolygonFromEdges( (OGRGeometryH) &oGC,
                                  TRUE, TRUE, dfTolerance, &eErr );
    if( eErr != OGRERR_NONE )
    {
        delete poFinalGeom;
        OGRMultiLineString* poMLS = new OGRMultiLineString();
        for(int i=0;i<oGC.getNumGeometries();i++)
            poMLS->addGeometry(oGC.getGeometryRef(i));
        poFinalGeom = poMLS;
    }

    poFeature->ApplyOCSTransformer( poFinalGeom );
    poFeature->SetGeometryDirectly( poFinalGeom );

    PrepareBrushStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                        CollectBoundaryPath()                         */
/************************************************************************/

OGRErr OGRDXFLayer::CollectBoundaryPath( OGRGeometryCollection *poGC,
    const double dfElevation )

{
    char szLineBuf[257];

/* -------------------------------------------------------------------- */
/*      Read the boundary path type.                                    */
/* -------------------------------------------------------------------- */
    int nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
    if( nCode != 92 )
    {
        DXF_LAYER_READER_ERROR();
        return OGRERR_FAILURE;
    }

    const int nBoundaryPathType = atoi(szLineBuf);

/* ==================================================================== */
/*      Handle polyline loops.                                          */
/* ==================================================================== */
    if( nBoundaryPathType & 0x02 )
        return CollectPolylinePath( poGC, dfElevation );

/* ==================================================================== */
/*      Handle non-polyline loops.                                      */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Read number of edges.                                           */
/* -------------------------------------------------------------------- */
    nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
    if( nCode != 93 )
    {
        DXF_LAYER_READER_ERROR();
        return OGRERR_FAILURE;
    }

    const int nEdgeCount = atoi(szLineBuf);

/* -------------------------------------------------------------------- */
/*      Loop reading edges.                                             */
/* -------------------------------------------------------------------- */
    for( int iEdge = 0; iEdge < nEdgeCount; iEdge++ )
    {
/* -------------------------------------------------------------------- */
/*      Read the edge type.                                             */
/* -------------------------------------------------------------------- */
        const int ET_LINE = 1;
        const int ET_CIRCULAR_ARC = 2;
        const int ET_ELLIPTIC_ARC = 3;
        const int ET_SPLINE = 4;

        nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
        if( nCode != 72 )
        {
            DXF_LAYER_READER_ERROR();
            return OGRERR_FAILURE;
        }

        int nEdgeType = atoi(szLineBuf);

/* -------------------------------------------------------------------- */
/*      Process a line edge.                                            */
/* -------------------------------------------------------------------- */
        if( nEdgeType == ET_LINE )
        {
            double dfStartX = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) == 10 )
                dfStartX = CPLAtof(szLineBuf);
            else
                break;

            double dfStartY = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) == 20 )
                dfStartY = CPLAtof(szLineBuf);
            else
                break;

            double dfEndX = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) == 11 )
                dfEndX = CPLAtof(szLineBuf);
            else
                break;

            double dfEndY = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) == 21 )
                dfEndY = CPLAtof(szLineBuf);
            else
                break;

            OGRLineString *poLS = new OGRLineString();

            poLS->addPoint( dfStartX, dfStartY, dfElevation );
            poLS->addPoint( dfEndX, dfEndY, dfElevation );

            poGC->addGeometryDirectly( poLS );
        }
/* -------------------------------------------------------------------- */
/*      Process a circular arc.                                         */
/* -------------------------------------------------------------------- */
        else if( nEdgeType == ET_CIRCULAR_ARC )
        {
            double dfCenterX = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 10 )
                dfCenterX = CPLAtof(szLineBuf);
            else
                break;

            double dfCenterY = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 20 )
                dfCenterY = CPLAtof(szLineBuf);
            else
                break;

            double dfRadius = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 40 )
                dfRadius = CPLAtof(szLineBuf);
            else
                break;

            double dfStartAngle = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 50 )
                dfStartAngle = CPLAtof(szLineBuf);
            else
                break;

            double dfEndAngle = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 51 )
                dfEndAngle = CPLAtof(szLineBuf);
            else
                break;

            bool bCounterClockwise = false;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 73 )
                bCounterClockwise = atoi(szLineBuf) != 0;
            else if (nCode >= 0)
                poDS->UnreadValue();
            else
                break;

            if( dfStartAngle > dfEndAngle )
                dfEndAngle += 360.0;
            if( bCounterClockwise )
            {
                dfStartAngle *= -1;
                dfEndAngle *= -1;
            }

            if( fabs(dfEndAngle - dfStartAngle) <= 361.0 )
            {
                OGRGeometry *poArc = OGRGeometryFactory::approximateArcAngles(
                    dfCenterX, dfCenterY, dfElevation,
                    dfRadius, dfRadius, 0.0,
                    dfStartAngle, dfEndAngle, 0.0, poDS->InlineBlocks() );

                // If the input was 2D, we assume we want to keep it that way
                if( dfElevation == 0.0 )
                    poArc->flattenTo2D();

                poGC->addGeometryDirectly( poArc );
            }
            else
            {
                // TODO: emit error ?
            }
        }

/* -------------------------------------------------------------------- */
/*      Process an elliptical arc.                                      */
/* -------------------------------------------------------------------- */
        else if( nEdgeType == ET_ELLIPTIC_ARC )
        {
            double dfCenterX = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 10 )
                dfCenterX = CPLAtof(szLineBuf);
            else
                break;

            double dfCenterY = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 20 )
                dfCenterY = CPLAtof(szLineBuf);
            else
                break;

            double dfMajorX = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 11 )
                dfMajorX = CPLAtof(szLineBuf);
            else
                break;

            double dfMajorY = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 21 )
                dfMajorY = CPLAtof(szLineBuf);
            else
                break;

            double dfRatio = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 40 )
                dfRatio = CPLAtof(szLineBuf);
            if( dfRatio == 0.0 )
                break;

            double dfStartAngle = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) == 50 )
                dfStartAngle = CPLAtof(szLineBuf);
            else
                break;

            double dfEndAngle = 0.0;

            if( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) == 51 )
                dfEndAngle = CPLAtof(szLineBuf);
            else
                break;

            bool bCounterClockwise = false;

            if( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) == 73 )
                bCounterClockwise = atoi(szLineBuf) != 0;
            else if (nCode >= 0)
                poDS->UnreadValue();
            else
                break;

            if( dfStartAngle > dfEndAngle )
                dfEndAngle += 360.0;
            if( bCounterClockwise )
            {
                dfStartAngle *= -1;
                dfEndAngle *= -1;
            }

            const double dfMajorRadius =
                sqrt( dfMajorX * dfMajorX + dfMajorY * dfMajorY );
            const double dfMinorRadius = dfMajorRadius * dfRatio;

            const double dfRotation =
                -1 * atan2( dfMajorY, dfMajorX ) * 180 / M_PI;

            // The start and end angles are stored as circular angles. However,
            // approximateArcAngles is expecting elliptical angles (what AutoCAD
            // calls "parameters"), so let's transform them.
            dfStartAngle = 180.0 * round( dfStartAngle / 180 ) +
                ( fabs( fmod( dfStartAngle, 180 ) ) == 90 ?
                    ( std::signbit( dfStartAngle ) ? 180 : -180 ) :
                    0 ) +
                atan( ( 1.0 / dfRatio ) * tan( dfStartAngle * M_PI / 180 ) ) * 180 / M_PI;
            dfEndAngle = 180.0 * round( dfEndAngle / 180 ) +
                ( fabs( fmod( dfEndAngle, 180 ) ) == 90 ?
                    ( std::signbit( dfEndAngle ) ? 180 : -180 ) :
                    0 ) +
                atan( ( 1.0 / dfRatio ) * tan( dfEndAngle * M_PI / 180 ) ) * 180 / M_PI;

            if( fabs(dfEndAngle - dfStartAngle) <= 361.0 )
            {
                OGRGeometry *poArc = OGRGeometryFactory::approximateArcAngles(
                    dfCenterX, dfCenterY, dfElevation,
                    dfMajorRadius, dfMinorRadius, dfRotation,
                    dfStartAngle, dfEndAngle, 0.0, poDS->InlineBlocks() );

                // If the input was 2D, we assume we want to keep it that way
                if( dfElevation == 0.0 )
                    poArc->flattenTo2D();

                poGC->addGeometryDirectly( poArc );
            }
            else
            {
                // TODO: emit error ?
            }
        }

/* -------------------------------------------------------------------- */
/*      Process an elliptical arc.                                      */
/* -------------------------------------------------------------------- */
        else if( nEdgeType == ET_SPLINE )
        {
            int nDegree = 3;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 94 )
                nDegree = atoi(szLineBuf);
            else
                break;

            // Skip a few things we don't care about
            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) != 73 )
                break;
            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) != 74 )
                break;

            int nKnots = 0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 95 )
                nKnots = atoi(szLineBuf);
            else
                break;

            int nControlPoints = 0;

            if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 96 )
                nControlPoints = atoi(szLineBuf);
            else
                break;

            std::vector<double> adfKnots( 1, 0.0 );

            nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf));
            if( nCode != 40 )
                break;

            while( nCode == 40 )
            {
                adfKnots.push_back( CPLAtof(szLineBuf) );
                nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf));
            }

            std::vector<double> adfControlPoints( 1, 0.0 );
            std::vector<double> adfWeights( 1, 0.0 );

            if( nCode != 10 )
                break;

            while( nCode == 10 )
            {
                adfControlPoints.push_back( CPLAtof(szLineBuf) );

                if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 20 )
                {
                    adfControlPoints.push_back( CPLAtof(szLineBuf) );
                }
                else
                    break;

                adfControlPoints.push_back( 0.0 ); // Z coordinate

                // 42 (weights) are optional
                if( (nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) == 42 )
                {
                    adfWeights.push_back( CPLAtof(szLineBuf) );
                    nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf));
                }
            }

            // Skip past the number of fit points
            if( nCode != 97 )
                break;

            // Eat the rest of this section, if present, until the next
            // boundary segment (72) or the conclusion of the boundary data (97)
            nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf));
            while( nCode > 0 && nCode != 72 && nCode != 97 )
                nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf));
            if( nCode > 0 )
                poDS->UnreadValue();

            auto poLS = InsertSplineWithChecks( nDegree,
                adfControlPoints, nControlPoints, adfKnots, nKnots,
                adfWeights );

            if( !poLS )
            {
                DXF_LAYER_READER_ERROR();
                return OGRERR_FAILURE;
            }

            poGC->addGeometryDirectly( poLS.release() );
        }

        else
        {
            CPLDebug( "DXF", "Unsupported HATCH boundary line type:%d",
                      nEdgeType );
            return OGRERR_UNSUPPORTED_OPERATION;
        }
    }

    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        return OGRERR_FAILURE;
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

OGRErr OGRDXFLayer::CollectPolylinePath( OGRGeometryCollection *poGC,
    const double dfElevation )

{
    int nCode = 0;
    char szLineBuf[257];
    DXFSmoothPolyline oSmoothPolyline;
    double dfBulge = 0.0;
    double dfX = 0.0;
    double dfY = 0.0;
    bool bHaveX = false;
    bool bHaveY = false;
    bool bIsClosed = false;
    int nVertexCount = -1;
    bool bHaveBulges = false;

    if( dfElevation != 0 )
        oSmoothPolyline.setCoordinateDimension(3);

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
            bHaveBulges = CPL_TO_BOOL(atoi(szLineBuf));
            break;

          case 73:
            bIsClosed = CPL_TO_BOOL(atoi(szLineBuf));
            break;

          case 10:
            if( bHaveX && bHaveY )
            {
                oSmoothPolyline.AddPoint(dfX, dfY, dfElevation, dfBulge);
                dfBulge = 0.0;
                bHaveY = false;
            }
            dfX = CPLAtof(szLineBuf);
            bHaveX = true;
            break;

          case 20:
            if( bHaveX && bHaveY )
            {
                oSmoothPolyline.AddPoint( dfX, dfY, dfElevation, dfBulge );
                dfBulge = 0.0;
                bHaveX = false;
            }
            dfY = CPLAtof(szLineBuf);
            bHaveY = true;
            if( bHaveX /* && bHaveY */ && !bHaveBulges )
            {
                oSmoothPolyline.AddPoint( dfX, dfY, dfElevation, dfBulge );
                dfBulge = 0.0;
                bHaveX = false;
                bHaveY = false;
            }
            break;

          case 42:
            dfBulge = CPLAtof(szLineBuf);
            if( bHaveX && bHaveY )
            {
                oSmoothPolyline.AddPoint( dfX, dfY, dfElevation, dfBulge );
                dfBulge = 0.0;
                bHaveX = false;
                bHaveY = false;
            }
            break;

          default:
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        return OGRERR_FAILURE;
    }

    if( nCode != 10 && nCode != 20 && nCode != 42 )
        poDS->UnreadValue();

    if( bHaveX && bHaveY )
        oSmoothPolyline.AddPoint(dfX, dfY, dfElevation, dfBulge);

    if( bIsClosed )
        oSmoothPolyline.Close();

    if(oSmoothPolyline.IsEmpty())
    {
        return OGRERR_FAILURE;
    }

    // Only process polylines with at least 2 vertices
    if( nVertexCount >= 2 )
    {
        oSmoothPolyline.SetUseMaxGapWhenTessellatingArcs( poDS->InlineBlocks() );
        poGC->addGeometryDirectly( oSmoothPolyline.Tessellate() );
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
