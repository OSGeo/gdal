/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements decoder of shapebin geometry for PGeo
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Paul Ramsey, pramsey at cleverelephant.ca
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011, Paul Ramsey <pramsey at cleverelephant.ca>
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogrpgeogeometry.h"
#include "ogr_p.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include <limits>
//#define DEBUG_VERBOSE

CPL_CVSID("$Id$");

#define SHPP_TRISTRIP   0
#define SHPP_TRIFAN     1
#define SHPP_OUTERRING  2
#define SHPP_INNERRING  3
#define SHPP_FIRSTRING  4
#define SHPP_RING       5
#define SHPP_TRIANGLES  6 /* Multipatch 9.0 specific */


typedef enum
{
    CURVE_ARC_INTERIOR_POINT,
    CURVE_ARC_CENTER_POINT,
    CURVE_BEZIER,
    CURVE_ELLIPSE_BY_CENTER
} CurveType;

typedef struct
{
    int       nStartPointIdx;
    CurveType eType;
    union
    {
        /* Arc defined by an intermediate point */
        struct
        {
            double dfX;
            double dfY;
        } ArcByIntermediatePoint;

        /* Deprecated way of defining circular arc by its center and winding order */
        struct
        {
            double dfX;
            double dfY;
            EMULATED_BOOL bIsCCW;
        } ArcByCenterPoint;

        struct
        {
            double dfX1;
            double dfY1;
            double dfX2;
            double dfY2;
        } Bezier;

        struct
        {
            double dfX;
            double dfY;
            double dfRotationDeg;
            double dfSemiMajor;
            double dfRatioSemiMinor;
            EMULATED_BOOL   bIsMinor;
            EMULATED_BOOL   bIsComplete;
        } EllipseByCenter;
    } u;
} CurveSegment;

static const int EXT_SHAPE_SEGMENT_ARC = 1;
static const int EXT_SHAPE_SEGMENT_BEZIER = 4;
static const int EXT_SHAPE_SEGMENT_ELLIPSE = 5;

static const int EXT_SHAPE_ARC_EMPTY = 0x1;
static const int EXT_SHAPE_ARC_CCW   = 0x8;
static const int EXT_SHAPE_ARC_MINOR = 0x10;
static const int EXT_SHAPE_ARC_LINE =  0x20;
static const int EXT_SHAPE_ARC_POINT = 0x40;
static const int EXT_SHAPE_ARC_IP    = 0x80;

static const int EXT_SHAPE_ELLIPSE_EMPTY       = 0x1;
static const int EXT_SHAPE_ELLIPSE_LINE        = 0x40;
static const int EXT_SHAPE_ELLIPSE_POINT       = 0x80;
static const int EXT_SHAPE_ELLIPSE_CIRCULAR    = 0x100;
static const int EXT_SHAPE_ELLIPSE_CENTER_TO   = 0x200;
static const int EXT_SHAPE_ELLIPSE_CENTER_FROM = 0x400;
static const int EXT_SHAPE_ELLIPSE_CCW         = 0x800;
static const int EXT_SHAPE_ELLIPSE_MINOR       = 0x1000;
static const int EXT_SHAPE_ELLIPSE_COMPLETE    = 0x2000;

/************************************************************************/
/*                  OGRCreateFromMultiPatchPart()                       */
/************************************************************************/

void OGRCreateFromMultiPatchPart(OGRMultiPolygon *poMP,
                                 OGRPolygon*& poLastPoly,
                                 int nPartType,
                                 int nPartPoints,
                                 double* padfX,
                                 double* padfY,
                                 double* padfZ)
{
    nPartType &= 0xf;

    if( nPartType == SHPP_TRISTRIP )
    {
        if( poLastPoly != NULL )
        {
            poMP->addGeometryDirectly( poLastPoly );
            poLastPoly = NULL;
        }

        for( int iBaseVert = 0; iBaseVert < nPartPoints-2; iBaseVert++ )
        {
            OGRPolygon *poPoly = new OGRPolygon();
            OGRLinearRing *poRing = new OGRLinearRing();
            int iSrcVert = iBaseVert;

            poRing->setPoint( 0,
                            padfX[iSrcVert],
                            padfY[iSrcVert],
                            padfZ[iSrcVert] );
            poRing->setPoint( 1,
                            padfX[iSrcVert+1],
                            padfY[iSrcVert+1],
                            padfZ[iSrcVert+1] );

            poRing->setPoint( 2,
                            padfX[iSrcVert+2],
                            padfY[iSrcVert+2],
                            padfZ[iSrcVert+2] );
            poRing->setPoint( 3,
                            padfX[iSrcVert],
                            padfY[iSrcVert],
                            padfZ[iSrcVert] );

            poPoly->addRingDirectly( poRing );
            poMP->addGeometryDirectly( poPoly );
        }
    }
    else if( nPartType == SHPP_TRIFAN )
    {
        if( poLastPoly != NULL )
        {
            poMP->addGeometryDirectly( poLastPoly );
            poLastPoly = NULL;
        }

        for( int iBaseVert = 0; iBaseVert < nPartPoints-2; iBaseVert++ )
        {
            OGRPolygon *poPoly = new OGRPolygon();
            OGRLinearRing *poRing = new OGRLinearRing();
            int iSrcVert = iBaseVert;

            poRing->setPoint( 0,
                            padfX[0],
                            padfY[0],
                            padfZ[0] );
            poRing->setPoint( 1,
                            padfX[iSrcVert+1],
                            padfY[iSrcVert+1],
                            padfZ[iSrcVert+1] );

            poRing->setPoint( 2,
                            padfX[iSrcVert+2],
                            padfY[iSrcVert+2],
                            padfZ[iSrcVert+2] );
            poRing->setPoint( 3,
                            padfX[0],
                            padfY[0],
                            padfZ[0] );

            poPoly->addRingDirectly( poRing );
            poMP->addGeometryDirectly( poPoly );
        }
    }
    else if( nPartType == SHPP_OUTERRING
            || nPartType == SHPP_INNERRING
            || nPartType == SHPP_FIRSTRING
            || nPartType == SHPP_RING )
    {
        if( poLastPoly != NULL
            && (nPartType == SHPP_OUTERRING
                || nPartType == SHPP_FIRSTRING) )
        {
            poMP->addGeometryDirectly( poLastPoly );
            poLastPoly = NULL;
        }

        if( poLastPoly == NULL )
            poLastPoly = new OGRPolygon();

        OGRLinearRing *poRing = new OGRLinearRing;

        poRing->setPoints( nPartPoints,
                            padfX,
                            padfY,
                            padfZ );

        poRing->closeRings();

        poLastPoly->addRingDirectly( poRing );
    }
    else if ( nPartType == SHPP_TRIANGLES )
    {
        if( poLastPoly != NULL )
        {
            poMP->addGeometryDirectly( poLastPoly );
            poLastPoly = NULL;
        }

        for( int iBaseVert = 0; iBaseVert < nPartPoints-2; iBaseVert+=3 )
        {
            OGRPolygon *poPoly = new OGRPolygon();
            OGRLinearRing *poRing = new OGRLinearRing();
            int iSrcVert = iBaseVert;

            poRing->setPoint( 0,
                            padfX[iSrcVert],
                            padfY[iSrcVert],
                            padfZ[iSrcVert] );
            poRing->setPoint( 1,
                            padfX[iSrcVert+1],
                            padfY[iSrcVert+1],
                            padfZ[iSrcVert+1] );

            poRing->setPoint( 2,
                            padfX[iSrcVert+2],
                            padfY[iSrcVert+2],
                            padfZ[iSrcVert+2] );
            poRing->setPoint( 3,
                            padfX[iSrcVert],
                            padfY[iSrcVert],
                            padfZ[iSrcVert] );

            poPoly->addRingDirectly( poRing );
            poMP->addGeometryDirectly( poPoly );
        }
    }
    else
        CPLDebug( "OGR", "Unrecognized parttype %d, ignored.",
                  nPartType );
}

/************************************************************************/
/*                     OGRCreateFromMultiPatch()                        */
/*                                                                      */
/*      Translate a multipatch representation to an OGR geometry        */
/*      Mostly copied from shape2ogr.cpp                                */
/************************************************************************/

static OGRGeometry* OGRCreateFromMultiPatch(int nParts,
                                            GInt32* panPartStart,
                                            GInt32* panPartType,
                                            int nPoints,
                                            double* padfX,
                                            double* padfY,
                                            double* padfZ)
{
    OGRMultiPolygon *poMP = new OGRMultiPolygon();
    int iPart;
    OGRPolygon *poLastPoly = NULL;

    for( iPart = 0; iPart < nParts; iPart++ )
    {
        int nPartPoints, nPartStart;

        // Figure out details about this part's vertex list.
        if( panPartStart == NULL )
        {
            nPartPoints = nPoints;
            nPartStart = 0;
        }
        else
        {

            if( iPart == nParts - 1 )
                nPartPoints =
                    nPoints - panPartStart[iPart];
            else
                nPartPoints = panPartStart[iPart+1]
                    - panPartStart[iPart];
            nPartStart = panPartStart[iPart];
        }

        OGRCreateFromMultiPatchPart(poMP,
                                    poLastPoly,
                                    panPartType[iPart],
                                    nPartPoints,
                                    padfX + nPartStart,
                                    padfY + nPartStart,
                                    padfZ + nPartStart);
    }

    if( poLastPoly != NULL )
    {
        poMP->addGeometryDirectly( poLastPoly );
        poLastPoly = NULL;
    }

    return poMP;
}


/************************************************************************/
/*                      OGRWriteToShapeBin()                            */
/*                                                                      */
/*      Translate OGR geometry to a shapefile binary representation     */
/************************************************************************/

OGRErr OGRWriteToShapeBin( OGRGeometry *poGeom,
                           GByte **ppabyShape,
                           int *pnBytes )
{
    int nShpSize = 4; /* All types start with integer type number */
    int nShpZSize = 0; /* Z gets tacked onto the end */
    GUInt32 nPoints = 0;
    GUInt32 nParts = 0;

/* -------------------------------------------------------------------- */
/*      Null or Empty input maps to SHPT_NULL.                          */
/* -------------------------------------------------------------------- */
    if ( ! poGeom || poGeom->IsEmpty() )
    {
        *ppabyShape = (GByte*)VSI_MALLOC_VERBOSE(nShpSize);
        if( *ppabyShape == NULL )
            return OGRERR_FAILURE;
        GUInt32 zero = SHPT_NULL;
        memcpy(*ppabyShape, &zero, nShpSize);
        *pnBytes = nShpSize;
        return OGRERR_NONE;
    }

    OGRwkbGeometryType nOGRType = wkbFlatten(poGeom->getGeometryType());
    const bool b3d = wkbHasZ(poGeom->getGeometryType());
    const bool bHasM = wkbHasM(poGeom->getGeometryType());
    const int nCoordDims = poGeom->CoordinateDimension();

/* -------------------------------------------------------------------- */
/*      Calculate the shape buffer size                                 */
/* -------------------------------------------------------------------- */
    if ( nOGRType == wkbPoint )
    {
        nShpSize += 8 * nCoordDims;
    }
    else if ( nOGRType == wkbLineString )
    {
        OGRLineString *poLine = (OGRLineString*)poGeom;
        nPoints = poLine->getNumPoints();
        nParts = 1;
        nShpSize += 16 * nCoordDims; /* xy(z)(m) box */
        nShpSize += 4; /* nparts */
        nShpSize += 4; /* npoints */
        nShpSize += 4; /* parts[1] */
        nShpSize += 8 * nCoordDims * nPoints; /* points */
        nShpZSize = 16 + 8 * nPoints;
    }
    else if ( nOGRType == wkbPolygon )
    {
        poGeom->closeRings();
        OGRPolygon *poPoly = (OGRPolygon*)poGeom;
        nParts = poPoly->getNumInteriorRings() + 1;
        for ( GUInt32 i = 0; i < nParts; i++ )
        {
            OGRLinearRing *poRing = i == 0
                ? poPoly->getExteriorRing()
                : poPoly->getInteriorRing(i-1);
            nPoints += poRing->getNumPoints();
        }
        nShpSize += 16 * nCoordDims; /* xy(z)(m) box */
        nShpSize += 4; /* nparts */
        nShpSize += 4; /* npoints */
        nShpSize += 4 * nParts; /* parts[nparts] */
        nShpSize += 8 * nCoordDims * nPoints; /* points */
        nShpZSize = 16 + 8 * nPoints;
    }
    else if ( nOGRType == wkbMultiPoint )
    {
        OGRMultiPoint *poMPoint = (OGRMultiPoint*)poGeom;
        for ( int i = 0; i < poMPoint->getNumGeometries(); i++ )
        {
            OGRPoint *poPoint = (OGRPoint*)(poMPoint->getGeometryRef(i));
            if ( poPoint->IsEmpty() )
                continue;
            nPoints++;
        }
        nShpSize += 16 * nCoordDims; /* xy(z)(m) box */
        nShpSize += 4; /* npoints */
        nShpSize += 8 * nCoordDims * nPoints; /* points */
        nShpZSize = 16 + 8 * nPoints;
    }
    else if ( nOGRType == wkbMultiLineString )
    {
        OGRMultiLineString *poMLine = (OGRMultiLineString*)poGeom;
        for ( int i = 0; i < poMLine->getNumGeometries(); i++ )
        {
            OGRLineString *poLine = (OGRLineString*)(poMLine->getGeometryRef(i));
            /* Skip empties */
            if ( poLine->IsEmpty() )
                continue;
            nParts++;
            nPoints += poLine->getNumPoints();
        }
        nShpSize += 16 * nCoordDims; /* xy(z)(m) box */
        nShpSize += 4; /* nparts */
        nShpSize += 4; /* npoints */
        nShpSize += 4 * nParts; /* parts[nparts] */
        nShpSize += 8 * nCoordDims * nPoints ; /* points */
        nShpZSize = 16 + 8 * nPoints;
    }
    else if ( nOGRType == wkbMultiPolygon )
    {
        poGeom->closeRings();
        OGRMultiPolygon *poMPoly = (OGRMultiPolygon*)poGeom;
        for( int j = 0; j < poMPoly->getNumGeometries(); j++ )
        {
            OGRPolygon *poPoly = (OGRPolygon*)(poMPoly->getGeometryRef(j));
            int nRings = poPoly->getNumInteriorRings() + 1;

            /* Skip empties */
            if ( poPoly->IsEmpty() )
                continue;

            nParts += nRings;
            for ( int i = 0; i < nRings; i++ )
            {
                OGRLinearRing *poRing = i == 0
                    ? poPoly->getExteriorRing()
                    : poPoly->getInteriorRing(i-1);
                nPoints += poRing->getNumPoints();
            }
        }
        nShpSize += 16 * nCoordDims; /* xy(z)(m) box */
        nShpSize += 4; /* nparts */
        nShpSize += 4; /* npoints */
        nShpSize += 4 * nParts; /* parts[nparts] */
        nShpSize += 8 * nCoordDims * nPoints ; /* points */
        nShpZSize = 16 + 8 * nPoints;
    }
    else
    {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

//#define WRITE_ARC_HACK
#ifdef WRITE_ARC_HACK
    int nShpSizeBeforeCurve = nShpSize;
    nShpSize += 4 + 4 + 4 + 20;
#endif
    /* Allocate our shape buffer */
    *ppabyShape = (GByte*)VSI_MALLOC_VERBOSE(nShpSize);
    if ( ! *ppabyShape )
        return OGRERR_FAILURE;

#ifdef WRITE_ARC_HACK
    /* To be used with :
id,WKT
1,"LINESTRING (1 0,0 1)"
2,"LINESTRING (5 1,6 0)"
3,"LINESTRING (10 1,11 0)"
4,"LINESTRING (16 0,15 1)"
5,"LINESTRING (21 0,20 1)"
6,"LINESTRING (31 0,30 2)" <-- not constant radius
    */

    GUInt32 nTmp = 1;
    memcpy((*ppabyShape) + nShpSizeBeforeCurve, &nTmp, 4);
    nTmp = 0;
    memcpy((*ppabyShape) + nShpSizeBeforeCurve + 4, &nTmp, 4);
    nTmp = EXT_SHAPE_SEGMENT_ARC;
    memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8, &nTmp, 4);
    static int nCounter = 0;
    nCounter ++;
    if( nCounter == 1 )
    {
        double dfVal = 0;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4, &dfVal, 8);
        dfVal = 0;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 8, &dfVal, 8);
        nTmp = 0;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 16, &nTmp, 4);
    }
    else if( nCounter == 2 )
    {
        double dfVal = 5;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4, &dfVal, 8);
        dfVal = 0;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 8, &dfVal, 8);
        nTmp = EXT_SHAPE_ARC_MINOR;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 16, &nTmp, 4);
    }
    else if( nCounter == 3 )
    {
        double dfVal = 10;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4, &dfVal, 8);
        dfVal = 0;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 8, &dfVal, 8);
        nTmp = EXT_SHAPE_ARC_CCW;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 16, &nTmp, 4);
    }
    else if( nCounter == 4 )
    {
        double dfVal = 15;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4, &dfVal, 8);
        dfVal = 0;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 8, &dfVal, 8);
        nTmp = EXT_SHAPE_ARC_CCW | EXT_SHAPE_ARC_MINOR;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 16, &nTmp, 4);
    }
    else if( nCounter == 5 )
    {
        double dfVal = 20;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4, &dfVal, 8);
        dfVal = 0;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 8, &dfVal, 8);
        nTmp = EXT_SHAPE_ARC_MINOR; // Inconsistent with SP and EP. Only the CCW/not CCW is taken into account by ArcGIS
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 16, &nTmp, 4);
    }
    else if( nCounter == 6 )
    {
        double dfVal = 30; // Radius inconsistent with SP and EP
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4, &dfVal, 8);
        dfVal = 0;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 8, &dfVal, 8);
        nTmp = EXT_SHAPE_ARC_MINOR;
        memcpy((*ppabyShape) + nShpSizeBeforeCurve + 8 + 4 + 16, &nTmp, 4);
    }
#endif

    /* Fill in the output size. */
    *pnBytes = nShpSize;

    /* Set up write pointers */
    unsigned char *pabyPtr = *ppabyShape;
    unsigned char *pabyPtrZ = NULL;
    unsigned char *pabyPtrM = NULL;
    if( bHasM )
        pabyPtrM = pabyPtr + nShpSize - nShpZSize;
    if ( b3d )
    {
        if( bHasM )
            pabyPtrZ = pabyPtrM - nShpZSize;
        else
            pabyPtrZ = pabyPtr + nShpSize - nShpZSize;
    }

/* -------------------------------------------------------------------- */
/*      Write in the Shape type number now                              */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = SHPT_NULL;

    switch(nOGRType)
    {
        case wkbPoint:
        {
            nGType = (b3d && bHasM) ? SHPT_POINTZM :
                     (b3d)          ? SHPT_POINTZ :
                     (bHasM)        ? SHPT_POINTM :
                                      SHPT_POINT;
            break;
        }
        case wkbMultiPoint:
        {
            nGType = (b3d && bHasM) ? SHPT_MULTIPOINTZM :
                     (b3d)          ? SHPT_MULTIPOINTZ :
                     (bHasM)        ? SHPT_MULTIPOINTM :
                                      SHPT_MULTIPOINT;
            break;
        }
        case wkbLineString:
        case wkbMultiLineString:
        {
            nGType = (b3d && bHasM) ? SHPT_ARCZM :
                     (b3d)          ? SHPT_ARCZ :
                     (bHasM)        ? SHPT_ARCM :
                                      SHPT_ARC;
            break;
        }
        case wkbPolygon:
        case wkbMultiPolygon:
        {
            nGType = (b3d && bHasM) ? SHPT_POLYGONZM :
                     (b3d)          ? SHPT_POLYGONZ :
                     (bHasM)        ? SHPT_POLYGONM :
                                      SHPT_POLYGON;
            break;
        }
        default:
        {
            return OGRERR_UNSUPPORTED_OPERATION;
        }
    }
    /* Write in the type number and advance the pointer */
#ifdef WRITE_ARC_HACK
    nGType = SHPT_GENERALPOLYLINE | 0x20000000;
#endif

    nGType = CPL_LSBWORD32( nGType );
    memcpy( pabyPtr, &nGType, 4 );
    pabyPtr += 4;


/* -------------------------------------------------------------------- */
/*      POINT and POINTZ                                                */
/* -------------------------------------------------------------------- */
    if ( nOGRType == wkbPoint )
    {
        OGRPoint *poPoint = (OGRPoint*)poGeom;
        double x = poPoint->getX();
        double y = poPoint->getY();

        /* Copy in the raw data. */
        memcpy( pabyPtr, &x, 8 );
        memcpy( pabyPtr+8, &y, 8 );
        if( b3d )
        {
            double z = poPoint->getZ();
            memcpy( pabyPtr+8+8, &z, 8 );
        }
        if( bHasM )
        {
            double m = poPoint->getM();
            memcpy( pabyPtr+8+((b3d) ? 16 : 8), &m, 8 );
        }

        /* Swap if needed. Shape doubles always LSB */
        if( OGR_SWAP( wkbNDR ) )
        {
            CPL_SWAPDOUBLE( pabyPtr );
            CPL_SWAPDOUBLE( pabyPtr+8 );
            if( b3d )
                CPL_SWAPDOUBLE( pabyPtr+8+8 );
            if( bHasM )
                CPL_SWAPDOUBLE( pabyPtr+8+((b3d) ? 16 : 8) );
        }

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      All the non-POINT types require an envelope next                */
/* -------------------------------------------------------------------- */
    OGREnvelope3D envelope;
    poGeom->getEnvelope(&envelope);
    memcpy( pabyPtr, &(envelope.MinX), 8 );
    memcpy( pabyPtr+8, &(envelope.MinY), 8 );
    memcpy( pabyPtr+8+8, &(envelope.MaxX), 8 );
    memcpy( pabyPtr+8+8+8, &(envelope.MaxY), 8 );

    /* Swap box if needed. Shape doubles are always LSB */
    if( OGR_SWAP( wkbNDR ) )
    {
        for ( int i = 0; i < 4; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    pabyPtr += 32;

    /* Write in the Z bounds at the end of the XY buffer */
    if ( b3d )
    {
        memcpy( pabyPtrZ, &(envelope.MinZ), 8 );
        memcpy( pabyPtrZ+8, &(envelope.MaxZ), 8 );

        /* Swap Z bounds if necessary */
        if( OGR_SWAP( wkbNDR ) )
        {
            for ( int i = 0; i < 2; i++ )
                CPL_SWAPDOUBLE( pabyPtrZ + 8*i );
        }
        pabyPtrZ += 16;
    }

    /* Reserve space for the M bounds at the end of the XY buffer */
    GByte* pabyPtrMBounds = NULL;
    double dfMinM = std::numeric_limits<double>::max();
    double dfMaxM = -dfMinM;
    if ( bHasM )
    {
        pabyPtrMBounds = pabyPtrM;
        pabyPtrM += 16;
    }

/* -------------------------------------------------------------------- */
/*      LINESTRING and LINESTRINGZ                                      */
/* -------------------------------------------------------------------- */
    if ( nOGRType == wkbLineString )
    {
        const OGRLineString *poLine = (OGRLineString*)poGeom;

        /* Write in the nparts (1) */
        GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
        memcpy( pabyPtr, &nPartsLsb, 4 );
        pabyPtr += 4;

        /* Write in the npoints */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

        /* Write in the part index (0) */
        GUInt32 nPartIndex = 0;
        memcpy( pabyPtr, &nPartIndex, 4 );
        pabyPtr += 4;

        /* Write in the point data */
        poLine->getPoints((OGRRawPoint*)pabyPtr, (double*)pabyPtrZ);
        if( bHasM )
        {
            for( GUInt32 k = 0; k < nPoints; k++ )
            {
                double dfM = poLine->getM(k);
                memcpy( pabyPtrM + 8*k, &dfM, 8);
                if( dfM < dfMinM ) dfMinM = dfM;
                if( dfM > dfMaxM ) dfMaxM = dfM;
            }
        }


        /* Swap if necessary */
        if( OGR_SWAP( wkbNDR ) )
        {
            for( GUInt32 k = 0; k < nPoints; k++ )
            {
                CPL_SWAPDOUBLE( pabyPtr + 16*k );
                CPL_SWAPDOUBLE( pabyPtr + 16*k + 8 );
                if( b3d )
                    CPL_SWAPDOUBLE( pabyPtrZ + 8*k );
                if( bHasM )
                    CPL_SWAPDOUBLE( pabyPtrM + 8*k );
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      POLYGON and POLYGONZ                                            */
/* -------------------------------------------------------------------- */
    else if ( nOGRType == wkbPolygon )
    {
        OGRPolygon *poPoly = (OGRPolygon*)poGeom;

        /* Write in the part count */
        GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
        memcpy( pabyPtr, &nPartsLsb, 4 );
        pabyPtr += 4;

        /* Write in the total point count */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

/* -------------------------------------------------------------------- */
/*      Now we have to visit each ring and write an index number into   */
/*      the parts list, and the coordinates into the points list.       */
/*      to do it in one pass, we will use three write pointers.         */
/*      pabyPtr writes the part indexes                                 */
/*      pabyPoints writes the xy coordinates                            */
/*      pabyPtrZ writes the z coordinates                               */
/* -------------------------------------------------------------------- */

        /* Just past the partindex[nparts] array */
        unsigned char* pabyPoints = pabyPtr + 4*nParts;

        int nPointIndexCount = 0;

        for( GUInt32 i = 0; i < nParts; i++ )
        {
            /* Check our Ring and condition it */
            OGRLinearRing *poRing = NULL;
            if ( i == 0 )
            {
                poRing = poPoly->getExteriorRing();
                /* Outer ring must be clockwise */
                if ( ! poRing->isClockwise() )
                    poRing->reverseWindingOrder();
            }
            else
            {
                poRing = poPoly->getInteriorRing(i-1);
                /* Inner rings should be anti-clockwise */
                if ( poRing->isClockwise() )
                    poRing->reverseWindingOrder();
            }

            int nRingNumPoints = poRing->getNumPoints();

#ifndef WRITE_ARC_HACK
            /* Cannot write un-closed rings to shape */
            if( nRingNumPoints <= 2 || ! poRing->get_IsClosed() )
                return OGRERR_FAILURE;
#endif

            /* Write in the part index */
            GUInt32 nPartIndex = CPL_LSBWORD32( nPointIndexCount );
            memcpy( pabyPtr, &nPartIndex, 4 );

            /* Write in the point data */
            poRing->getPoints((OGRRawPoint*)pabyPoints, (double*)pabyPtrZ);
            if( bHasM )
            {
                for( int k = 0; k < nRingNumPoints; k++ )
                {
                    double dfM = poRing->getM(k);
                    memcpy( pabyPtrM + 8*k, &dfM, 8);
                    if( dfM < dfMinM ) dfMinM = dfM;
                    if( dfM > dfMaxM ) dfMaxM = dfM;
                }
            }

            /* Swap if necessary */
            if( OGR_SWAP( wkbNDR ) )
            {
                for( int k = 0; k < nRingNumPoints; k++ )
                {
                    CPL_SWAPDOUBLE( pabyPoints + 16*k );
                    CPL_SWAPDOUBLE( pabyPoints + 16*k + 8 );
                    if( b3d )
                        CPL_SWAPDOUBLE( pabyPtrZ + 8*k );
                    if( bHasM )
                        CPL_SWAPDOUBLE( pabyPtrM + 8*k );
                }
            }

            nPointIndexCount += nRingNumPoints;
            /* Advance the write pointers */
            pabyPtr += 4;
            pabyPoints += 16 * nRingNumPoints;
            if ( b3d )
                pabyPtrZ += 8 * nRingNumPoints;
            if ( bHasM )
                pabyPtrM += 8 * nRingNumPoints;
        }
    }
/* -------------------------------------------------------------------- */
/*      MULTIPOINT and MULTIPOINTZ                                      */
/* -------------------------------------------------------------------- */
    else if ( nOGRType == wkbMultiPoint )
    {
        OGRMultiPoint *poMPoint = (OGRMultiPoint*)poGeom;

        /* Write in the total point count */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

/* -------------------------------------------------------------------- */
/*      Now we have to visit each point write it into the points list   */
/*      We will use two write pointers.                                 */
/*      pabyPtr writes the xy coordinates                               */
/*      pabyPtrZ writes the z coordinates                               */
/* -------------------------------------------------------------------- */

        for( GUInt32 i = 0; i < nPoints; i++ )
        {
            const OGRPoint *poPt = (OGRPoint*)(poMPoint->getGeometryRef(i));

            /* Skip empties */
            if ( poPt->IsEmpty() )
                continue;

            /* Write the coordinates */
            double x = poPt->getX();
            double y = poPt->getY();
            memcpy(pabyPtr, &x, 8);
            memcpy(pabyPtr+8, &y, 8);
            if ( b3d )
            {
                double z = poPt->getZ();
                memcpy(pabyPtrZ, &z, 8);
            }
            if ( bHasM )
            {
                double dfM = poPt->getM();
                memcpy(pabyPtrM, &dfM, 8);
                if( dfM < dfMinM ) dfMinM = dfM;
                if( dfM > dfMaxM ) dfMaxM = dfM;
            }

            /* Swap if necessary */
            if( OGR_SWAP( wkbNDR ) )
            {
                CPL_SWAPDOUBLE( pabyPtr );
                CPL_SWAPDOUBLE( pabyPtr + 8 );
                if( b3d )
                    CPL_SWAPDOUBLE( pabyPtrZ );
                if( bHasM )
                    CPL_SWAPDOUBLE( pabyPtrM );
            }

            /* Advance the write pointers */
            pabyPtr += 16;
            if ( b3d )
                pabyPtrZ += 8;
            if ( bHasM )
                pabyPtrM += 8;
        }
    }

/* -------------------------------------------------------------------- */
/*      MULTILINESTRING and MULTILINESTRINGZ                            */
/* -------------------------------------------------------------------- */
    else if ( nOGRType == wkbMultiLineString )
    {
        OGRMultiLineString *poMLine = (OGRMultiLineString*)poGeom;

        /* Write in the part count */
        GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
        memcpy( pabyPtr, &nPartsLsb, 4 );
        pabyPtr += 4;

        /* Write in the total point count */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

        /* Just past the partindex[nparts] array */
        unsigned char* pabyPoints = pabyPtr + 4*nParts;

        int nPointIndexCount = 0;

        for( GUInt32 i = 0; i < nParts; i++ )
        {
            const OGRLineString *poLine = (OGRLineString*)(poMLine->getGeometryRef(i));

            /* Skip empties */
            if ( poLine->IsEmpty() )
                continue;

            int nLineNumPoints = poLine->getNumPoints();

            /* Write in the part index */
            GUInt32 nPartIndex = CPL_LSBWORD32( nPointIndexCount );
            memcpy( pabyPtr, &nPartIndex, 4 );

            /* Write in the point data */
            poLine->getPoints((OGRRawPoint*)pabyPoints, (double*)pabyPtrZ);
            if( bHasM )
            {
                for( int k = 0; k < nLineNumPoints; k++ )
                {
                    double dfM = poLine->getM(k);
                    memcpy( pabyPtrM + 8*k, &dfM, 8);
                    if( dfM < dfMinM ) dfMinM = dfM;
                    if( dfM > dfMaxM ) dfMaxM = dfM;
                }
            }

            /* Swap if necessary */
            if( OGR_SWAP( wkbNDR ) )
            {
                for( int k = 0; k < nLineNumPoints; k++ )
                {
                    CPL_SWAPDOUBLE( pabyPoints + 16*k );
                    CPL_SWAPDOUBLE( pabyPoints + 16*k + 8 );
                    if( b3d )
                        CPL_SWAPDOUBLE( pabyPtrZ + 8*k );
                    if( bHasM )
                        CPL_SWAPDOUBLE( pabyPtrM + 8*k );
                }
            }

            nPointIndexCount += nLineNumPoints;

            /* Advance the write pointers */
            pabyPtr += 4;
            pabyPoints += 16 * nLineNumPoints;
            if ( b3d )
                pabyPtrZ += 8 * nLineNumPoints;
            if ( bHasM )
                pabyPtrM += 8 * nLineNumPoints;
        }
    }
/* -------------------------------------------------------------------- */
/*      MULTIPOLYGON and MULTIPOLYGONZ                                  */
/* -------------------------------------------------------------------- */
    else /* if ( nOGRType == wkbMultiPolygon ) */
    {
        OGRMultiPolygon *poMPoly = (OGRMultiPolygon*)poGeom;

        /* Write in the part count */
        GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
        memcpy( pabyPtr, &nPartsLsb, 4 );
        pabyPtr += 4;

        /* Write in the total point count */
        GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
        memcpy( pabyPtr, &nPointsLsb, 4 );
        pabyPtr += 4;

/* -------------------------------------------------------------------- */
/*      Now we have to visit each ring and write an index number into   */
/*      the parts list, and the coordinates into the points list.       */
/*      to do it in one pass, we will use three write pointers.         */
/*      pabyPtr writes the part indexes                                 */
/*      pabyPoints writes the xy coordinates                            */
/*      pabyPtrZ writes the z coordinates                               */
/* -------------------------------------------------------------------- */

        /* Just past the partindex[nparts] array */
        unsigned char* pabyPoints = pabyPtr + 4*nParts;

        int nPointIndexCount = 0;

        for( int i = 0; i < poMPoly->getNumGeometries(); i++ )
        {
            OGRPolygon *poPoly = (OGRPolygon*)(poMPoly->getGeometryRef(i));

            /* Skip empties */
            if ( poPoly->IsEmpty() )
                continue;

            int nRings = 1 + poPoly->getNumInteriorRings();

            for( int j = 0; j < nRings; j++ )
            {
                /* Check our Ring and condition it */
                OGRLinearRing *poRing = NULL;
                if ( j == 0 )
                {
                    poRing = poPoly->getExteriorRing();
                    /* Outer ring must be clockwise */
                    if ( ! poRing->isClockwise() )
                        poRing->reverseWindingOrder();
                }
                else
                {
                    poRing = poPoly->getInteriorRing(j-1);
                    /* Inner rings should be anti-clockwise */
                    if ( poRing->isClockwise() )
                        poRing->reverseWindingOrder();
                }

                int nRingNumPoints = poRing->getNumPoints();

                /* Cannot write closed rings to shape */
                if( nRingNumPoints <= 2 || ! poRing->get_IsClosed() )
                    return OGRERR_FAILURE;

                /* Write in the part index */
                GUInt32 nPartIndex = CPL_LSBWORD32( nPointIndexCount );
                memcpy( pabyPtr, &nPartIndex, 4 );

                /* Write in the point data */
                poRing->getPoints((OGRRawPoint*)pabyPoints, (double*)pabyPtrZ);
                if( bHasM )
                {
                    for( int k = 0; k < nRingNumPoints; k++ )
                    {
                        double dfM = poRing->getM(k);
                        memcpy( pabyPtrM + 8*k, &dfM, 8);
                        if( dfM < dfMinM ) dfMinM = dfM;
                        if( dfM > dfMaxM ) dfMaxM = dfM;
                    }
                }

                /* Swap if necessary */
                if( OGR_SWAP( wkbNDR ) )
                {
                    for( int k = 0; k < nRingNumPoints; k++ )
                    {
                        CPL_SWAPDOUBLE( pabyPoints + 16*k );
                        CPL_SWAPDOUBLE( pabyPoints + 16*k + 8 );
                        if( b3d )
                            CPL_SWAPDOUBLE( pabyPtrZ + 8*k );
                        if( bHasM )
                            CPL_SWAPDOUBLE( pabyPtrM + 8*k );
                    }
                }

                nPointIndexCount += nRingNumPoints;
                /* Advance the write pointers */
                pabyPtr += 4;
                pabyPoints += 16 * nRingNumPoints;
                if ( b3d )
                    pabyPtrZ += 8 * nRingNumPoints;
                if ( bHasM )
                    pabyPtrM += 8 * nRingNumPoints;
            }
        }
    }

    if ( bHasM )
    {
        if( dfMinM > dfMaxM )
        {
            dfMinM = 0.0;
            dfMaxM = 0.0;
        }
        memcpy( pabyPtrMBounds, &(dfMinM), 8 );
        memcpy( pabyPtrMBounds+8, &(dfMaxM), 8 );

        /* Swap M bounds if necessary */
        if( OGR_SWAP( wkbNDR ) )
        {
            for ( int i = 0; i < 2; i++ )
                CPL_SWAPDOUBLE( pabyPtrMBounds + 8*i );
        }
    }

    return OGRERR_NONE;
}


/************************************************************************/
/*                   OGRWriteMultiPatchToShapeBin()                     */
/************************************************************************/

OGRErr OGRWriteMultiPatchToShapeBin( OGRGeometry *poGeom,
                                     GByte **ppabyShape,
                                     int *pnBytes )
{
    if( wkbFlatten(poGeom->getGeometryType()) != wkbMultiPolygon )
        return OGRERR_UNSUPPORTED_OPERATION;

    poGeom->closeRings();
    OGRMultiPolygon *poMPoly = (OGRMultiPolygon*)poGeom;
    int nParts = 0;
    int* panPartStart = NULL;
    int* panPartType = NULL;
    int nPoints = 0;
    OGRRawPoint* poPoints = NULL;
    double* padfZ = NULL;
    int nBeginLastPart = 0;

    for( int j = 0; j < poMPoly->getNumGeometries(); j++ )
    {
        OGRPolygon *poPoly = (OGRPolygon*)(poMPoly->getGeometryRef(j));
        int nRings = poPoly->getNumInteriorRings() + 1;

        /* Skip empties */
        if ( poPoly->IsEmpty() )
            continue;

        OGRLinearRing *poRing = poPoly->getExteriorRing();
        if( nRings == 1 && poRing->getNumPoints() == 4 )
        {
            if( nParts > 0 && poPoints != NULL &&
                ((panPartType[nParts-1] == SHPP_TRIANGLES && nPoints - panPartStart[nParts-1] == 3) ||
                 panPartType[nParts-1] == SHPP_TRIFAN) &&
                poRing->getX(0) == poPoints[nBeginLastPart].x &&
                poRing->getY(0) == poPoints[nBeginLastPart].y &&
                poRing->getZ(0) == padfZ[nBeginLastPart] &&
                poRing->getX(1) == poPoints[nPoints-1].x &&
                poRing->getY(1) == poPoints[nPoints-1].y &&
                poRing->getZ(1) == padfZ[nPoints-1] )
            {
                panPartType[nParts-1] = SHPP_TRIFAN;

                poPoints = (OGRRawPoint*)CPLRealloc(poPoints,
                                            (nPoints + 1) * sizeof(OGRRawPoint));
                padfZ = (double*)CPLRealloc(padfZ, (nPoints + 1) * sizeof(double));
                poPoints[nPoints].x = poRing->getX(2);
                poPoints[nPoints].y = poRing->getY(2);
                padfZ[nPoints] = poRing->getZ(2);
                nPoints ++;
            }
            else if( nParts > 0 && poPoints != NULL &&
                ((panPartType[nParts-1] == SHPP_TRIANGLES && nPoints - panPartStart[nParts-1] == 3) ||
                 panPartType[nParts-1] == SHPP_TRISTRIP) &&
                poRing->getX(0) == poPoints[nPoints-2].x &&
                poRing->getY(0) == poPoints[nPoints-2].y &&
                poRing->getZ(0) == padfZ[nPoints-2] &&
                poRing->getX(1) == poPoints[nPoints-1].x &&
                poRing->getY(1) == poPoints[nPoints-1].y &&
                poRing->getZ(1) == padfZ[nPoints-1] )
            {
                panPartType[nParts-1] = SHPP_TRISTRIP;

                poPoints = (OGRRawPoint*)CPLRealloc(poPoints,
                                            (nPoints + 1) * sizeof(OGRRawPoint));
                padfZ = (double*)CPLRealloc(padfZ, (nPoints + 1) * sizeof(double));
                poPoints[nPoints].x = poRing->getX(2);
                poPoints[nPoints].y = poRing->getY(2);
                padfZ[nPoints] = poRing->getZ(2);
                nPoints ++;
            }
            else
            {
                if( nParts == 0 || panPartType[nParts-1] != SHPP_TRIANGLES )
                {
                    nBeginLastPart = nPoints;

                    panPartStart = (int*)CPLRealloc(panPartStart, (nParts + 1) * sizeof(int));
                    panPartType = (int*)CPLRealloc(panPartType, (nParts + 1) * sizeof(int));
                    panPartStart[nParts] = nPoints;
                    panPartType[nParts] = SHPP_TRIANGLES;
                    nParts ++;
                }

                poPoints = (OGRRawPoint*)CPLRealloc(poPoints,
                                        (nPoints + 3) * sizeof(OGRRawPoint));
                padfZ = (double*)CPLRealloc(padfZ, (nPoints + 3) * sizeof(double));
                for(int i=0;i<3;i++)
                {
                    poPoints[nPoints+i].x = poRing->getX(i);
                    poPoints[nPoints+i].y = poRing->getY(i);
                    padfZ[nPoints+i] = poRing->getZ(i);
                }
                nPoints += 3;
            }
        }
        else
        {
            panPartStart = (int*)CPLRealloc(panPartStart, (nParts + nRings) * sizeof(int));
            panPartType = (int*)CPLRealloc(panPartType, (nParts + nRings) * sizeof(int));

            for ( int i = 0; i < nRings; i++ )
            {
                panPartStart[nParts + i] = nPoints;
                if ( i == 0 )
                {
                    poRing = poPoly->getExteriorRing();
                    panPartType[nParts + i] = SHPP_OUTERRING;
                }
                else
                {
                    poRing = poPoly->getInteriorRing(i-1);
                    panPartType[nParts + i] = SHPP_INNERRING;
                }
                poPoints = (OGRRawPoint*)CPLRealloc(poPoints,
                        (nPoints + poRing->getNumPoints()) * sizeof(OGRRawPoint));
                padfZ = (double*)CPLRealloc(padfZ,
                        (nPoints + poRing->getNumPoints()) * sizeof(double));
                for( int k = 0; k < poRing->getNumPoints(); k++ )
                {
                    poPoints[nPoints+k].x = poRing->getX(k);
                    poPoints[nPoints+k].y = poRing->getY(k);
                    padfZ[nPoints+k] = poRing->getZ(k);
                }
                nPoints += poRing->getNumPoints();
            }

            nParts += nRings;
        }
    }

    int nShpSize = 4; /* All types start with integer type number */
    nShpSize += 16 * 2; /* xy bbox */
    nShpSize += 4; /* nparts */
    nShpSize += 4; /* npoints */
    nShpSize += 4 * nParts; /* panPartStart[nparts] */
    nShpSize += 4 * nParts; /* panPartType[nparts] */
    nShpSize += 8 * 2 * nPoints; /* xy points */
    nShpSize += 16; /* z bbox */
    nShpSize += 8 * nPoints; /* z points */

    *pnBytes = nShpSize;
    *ppabyShape = (GByte*) CPLMalloc(nShpSize);

    GByte* pabyPtr = *ppabyShape;

    /* Write in the type number and advance the pointer */
    GUInt32 nGType = CPL_LSBWORD32( SHPT_MULTIPATCH );
    memcpy( pabyPtr, &nGType, 4 );
    pabyPtr += 4;

    OGREnvelope3D envelope;
    poGeom->getEnvelope(&envelope);
    memcpy( pabyPtr, &(envelope.MinX), 8 );
    memcpy( pabyPtr+8, &(envelope.MinY), 8 );
    memcpy( pabyPtr+8+8, &(envelope.MaxX), 8 );
    memcpy( pabyPtr+8+8+8, &(envelope.MaxY), 8 );

    /* Swap box if needed. Shape doubles are always LSB */
    if( OGR_SWAP( wkbNDR ) )
    {
        for ( int i = 0; i < 4; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    pabyPtr += 32;

    /* Write in the part count */
    GUInt32 nPartsLsb = CPL_LSBWORD32( nParts );
    memcpy( pabyPtr, &nPartsLsb, 4 );
    pabyPtr += 4;

    /* Write in the total point count */
    GUInt32 nPointsLsb = CPL_LSBWORD32( nPoints );
    memcpy( pabyPtr, &nPointsLsb, 4 );
    pabyPtr += 4;

    for( int i = 0; i < nParts; i ++ )
    {
        int nPartStart = CPL_LSBWORD32(panPartStart[i]);
        memcpy( pabyPtr, &nPartStart, 4 );
        pabyPtr += 4;
    }
    for( int i = 0; i < nParts; i ++ )
    {
        int nPartType = CPL_LSBWORD32(panPartType[i]);
        memcpy( pabyPtr, &nPartType, 4 );
        pabyPtr += 4;
    }

    if( poPoints != NULL )
        memcpy(pabyPtr, poPoints, 2 * 8 * nPoints);

    /* Swap box if needed. Shape doubles are always LSB */
    if( OGR_SWAP( wkbNDR ) )
    {
        for( int i = 0; i < 2 * nPoints; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    pabyPtr += 2 * 8 * nPoints;

    memcpy( pabyPtr, &(envelope.MinZ), 8 );
    memcpy( pabyPtr+8, &(envelope.MaxZ), 8 );
    if( OGR_SWAP( wkbNDR ) )
    {
        for( int i = 0; i < 2; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    pabyPtr += 16;

    if( padfZ != NULL )
        memcpy(pabyPtr, padfZ, 8 * nPoints);
    /* Swap box if needed. Shape doubles are always LSB */
    if( OGR_SWAP( wkbNDR ) )
    {
        for( int i = 0; i < nPoints; i++ )
            CPL_SWAPDOUBLE( pabyPtr + 8*i );
    }
    //pabyPtr +=  8 * nPoints;

    CPLFree(panPartStart);
    CPLFree(panPartType);
    CPLFree(poPoints);
    CPLFree(padfZ);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetAngleOnEllipse()                        */
/************************************************************************/

/* Return the angle in deg [0,360] of dfArcX,dfArcY regarding the */
/* ellipse semi-major axis */
static double GetAngleOnEllipse( double dfPointOnArcX,
                                 double dfPointOnArcY,
                                 double dfCenterX,
                                 double dfCenterY,
                                 double dfRotationDeg /* ellipse rotation*/,
                                 double dfSemiMajor,
                                 double dfSemiMinor )
{
    /*  Let's invert the following equation where cosA,sinA are unknown
        dfPointOnArcX-dfCenterX = cosA*M*cosRot + sinA*m*sinRot
        dfPointOnArcY-dfCenterY = -cosA*M*sinRot + sinA*m*cosRot
    */
    const double dfRotationRadians = dfRotationDeg * M_PI / 180.0;
    const double dfCosRot = cos(dfRotationRadians);
    const double dfSinRot = sin(dfRotationRadians);
    const double dfDeltaX = dfPointOnArcX - dfCenterX;
    const double dfDeltaY = dfPointOnArcY - dfCenterY;
    const double dfCosA = ( dfCosRot * dfDeltaX - dfSinRot * dfDeltaY ) / dfSemiMajor;
    const double dfSinA = ( dfSinRot * dfDeltaX + dfCosRot * dfDeltaY ) / dfSemiMinor;
    // We could check that dfCosA^2 + dfSinA^2 ~= 1 to verify that the point
    // is on the ellipse
    const double dfAngle = atan2( dfSinA, dfCosA ) / M_PI * 180;
    if( dfAngle < -180 )
        return dfAngle + 360;
    return dfAngle;
}

/************************************************************************/
/*                    OGRShapeCreateCompoundCurve()                     */
/************************************************************************/

static OGRCurve* OGRShapeCreateCompoundCurve( int nPartStartIdx,
                                             int nPartPoints,
                                             const CurveSegment* pasCurves,
                                             int nCurves,
                                             int nFirstCurveIdx,
                                             /*const*/ double* padfX,
                                             /*const*/ double* padfY,
                                             /*const*/ double* padfZ,
                                             /*const*/ double* padfM,
                                             int* pnLastCurveIdx )
{
    OGRCompoundCurve* poCC = new OGRCompoundCurve();
    int nLastPointIdx = nPartStartIdx;
    bool bHasCircularArcs = false;
    int i = nFirstCurveIdx;  // Used after for.
    for( ; i < nCurves; i ++ )
    {
        const int nStartPointIdx = pasCurves[i].nStartPointIdx;

        if( nStartPointIdx < nPartStartIdx )
        {
            // Shouldn't happen normally, but who knows...
            continue;
        }

        // For a multi-part geometry, stop when the start index of the curve
        // exceeds the last point index of the part
        if( nStartPointIdx >= nPartStartIdx + nPartPoints )
        {
            if( pnLastCurveIdx )
                *pnLastCurveIdx = i;
            break;
        }

        // Add linear segments between end of last curve portion (or beginning
        // of the part) and start of current curve
        if( nStartPointIdx > nLastPointIdx )
        {
            OGRLineString *poLine = new OGRLineString();
            poLine->setPoints( nStartPointIdx - nLastPointIdx + 1,
                                padfX + nLastPointIdx,
                                padfY + nLastPointIdx,
                                (padfZ != NULL) ? padfZ + nLastPointIdx : NULL,
                                (padfM != NULL) ? padfM + nLastPointIdx : NULL );
            poCC->addCurveDirectly(poLine);
        }

        if( pasCurves[i].eType == CURVE_ARC_INTERIOR_POINT &&
            nStartPointIdx+1 < nPartStartIdx + nPartPoints )
        {
            OGRPoint p1( padfX[nStartPointIdx], padfY[nStartPointIdx],
                         (padfZ != NULL) ? padfZ[nStartPointIdx] : 0.0,
                         (padfM != NULL) ? padfM[nStartPointIdx] : 0.0 );
            OGRPoint p2( pasCurves[i].u.ArcByIntermediatePoint.dfX,
                         pasCurves[i].u.ArcByIntermediatePoint.dfY,
                         (padfZ != NULL) ?  padfZ[nStartPointIdx] : 0.0 );
            OGRPoint p3( padfX[nStartPointIdx+1], padfY[nStartPointIdx+1],
                         (padfZ != NULL) ? padfZ[nStartPointIdx+1] : 0.0,
                         (padfM != NULL) ? padfM[nStartPointIdx+1] : 0.0 );

            // Some software (like QGIS, see https://hub.qgis.org/issues/15116)
            // do not like 3-point circles, so use a 5 point variant.
            if( p1.getX() == p3.getX() && p1.getY() == p3.getY() )
            {
                if( p1.getX() != p2.getX() || p1.getY() != p2.getY() )
                {
                    bHasCircularArcs = true;
                    OGRCircularString* poCS = new OGRCircularString();
                    double dfCenterX = (p1.getX() + p2.getX()) / 2;
                    double dfCenterY = (p1.getY() + p2.getY()) / 2;
                    poCS->addPoint( &p1 );
                    OGRPoint pInterm1( dfCenterX - ( p2.getY() - dfCenterY ),
                                       dfCenterY + ( p1.getX() - dfCenterX ),
                                       (padfZ != NULL) ?  padfZ[nStartPointIdx] : 0.0 );
                    poCS->addPoint( &pInterm1 );
                    poCS->addPoint( &p2 );
                    OGRPoint pInterm2( dfCenterX + ( p2.getY() - dfCenterY ),
                                       dfCenterY - ( p1.getX() - dfCenterX ),
                                       (padfZ != NULL) ?  padfZ[nStartPointIdx] : 0.0 );
                    poCS->addPoint( &pInterm2 );
                    poCS->addPoint( &p3 );
                    poCS->set3D( padfZ != NULL );
                    poCS->setMeasured( padfM != NULL );
                    poCC->addCurveDirectly(poCS);
                }
            }
            else
            {
                bHasCircularArcs = true;
                OGRCircularString* poCS = new OGRCircularString();
                poCS->addPoint( &p1 );
                poCS->addPoint( &p2 );
                poCS->addPoint( &p3 );
                poCS->set3D( padfZ != NULL );
                poCS->setMeasured( padfM != NULL );
                poCC->addCurveDirectly(poCS);
            }
        }

        else if( pasCurves[i].eType == CURVE_ARC_CENTER_POINT &&
            nStartPointIdx+1 < nPartStartIdx + nPartPoints )
        {
            const double dfCenterX = pasCurves[i].u.ArcByCenterPoint.dfX;
            const double dfCenterY = pasCurves[i].u.ArcByCenterPoint.dfY;
            double dfDeltaY = padfY[nStartPointIdx] - dfCenterY;
            double dfDeltaX = padfX[nStartPointIdx] - dfCenterX;
            double dfAngleStart = atan2(dfDeltaY, dfDeltaX);
            dfDeltaY = padfY[nStartPointIdx+1] - dfCenterY;
            dfDeltaX = padfX[nStartPointIdx+1] - dfCenterX;
            double dfAngleEnd = atan2(dfDeltaY, dfDeltaX);
            // Note: this definition from center and 2 points may be not a circle...
            double dfRadius = sqrt( dfDeltaX * dfDeltaX + dfDeltaY * dfDeltaY );
            if( pasCurves[i].u.ArcByCenterPoint.bIsCCW )
            {
                if( dfAngleStart >= dfAngleEnd )
                    dfAngleEnd += 2 * M_PI;
            }
            else
            {
                if( dfAngleStart <= dfAngleEnd )
                    dfAngleEnd -= 2 * M_PI;
            }
            const double dfMidAngle = (dfAngleStart + dfAngleEnd) / 2;
            OGRPoint p1( padfX[nStartPointIdx], padfY[nStartPointIdx],
                         (padfZ != NULL) ? padfZ[nStartPointIdx] : 0.0,
                         (padfM != NULL) ? padfM[nStartPointIdx] : 0.0 );
            OGRPoint p2( dfCenterX + dfRadius * cos(dfMidAngle),
                         dfCenterY + dfRadius * sin(dfMidAngle),
                         (padfZ != NULL) ?  padfZ[nStartPointIdx] : 0.0 );
            OGRPoint p3( padfX[nStartPointIdx+1], padfY[nStartPointIdx+1],
                         (padfZ != NULL) ? padfZ[nStartPointIdx+1] : 0.0,
                         (padfM != NULL) ? padfM[nStartPointIdx+1] : 0.0 );

            bHasCircularArcs = true;
            OGRCircularString* poCS = new OGRCircularString();
            poCS->addPoint( &p1 );
            poCS->addPoint( &p2 );
            poCS->addPoint( &p3 );
            poCS->set3D( padfZ != NULL );
            poCS->setMeasured( padfM != NULL );
            poCC->addCurveDirectly(poCS);
        }

        else if( pasCurves[i].eType == CURVE_BEZIER &&
                  nStartPointIdx+1 < nPartStartIdx + nPartPoints )
        {
            const int nSteps = 100;
            OGRLineString *poLine = new OGRLineString();
            poLine->setNumPoints(nSteps + 1);
            const double dfX0 = padfX[nStartPointIdx];
            const double dfY0 = padfY[nStartPointIdx];
            const double dfX1 = pasCurves[i].u.Bezier.dfX1;
            const double dfY1 = pasCurves[i].u.Bezier.dfY1;
            const double dfX2 = pasCurves[i].u.Bezier.dfX2;
            const double dfY2 = pasCurves[i].u.Bezier.dfY2;
            const double dfX3 = padfX[nStartPointIdx+1];
            const double dfY3 = padfY[nStartPointIdx+1];
            poLine->setPoint(0, dfX0, dfY0,
                             (padfZ != NULL) ? padfZ[nStartPointIdx] : 0.0,
                             (padfM != NULL) ? padfM[nStartPointIdx] : 0.0);
            for(int j=1;j<nSteps; j++)
            {
                const double t = static_cast<double>(j) / nSteps;
                // Third-order Bezier interpolation
                poLine->setPoint(j,
                                 (1-t)*(1-t)*(1-t)*dfX0 + 3*(1-t)*(1-t)*t*dfX1 +
                                 3*(1-t)*t*t*dfX2 + t*t*t*dfX3,
                                 (1-t)*(1-t)*(1-t)*dfY0 + 3*(1-t)*(1-t)*t*dfY1 +
                                 3*(1-t)*t*t*dfY2 + t*t*t*dfY3);
            }
            poLine->setPoint(nSteps, dfX3, dfY3,
                             (padfZ != NULL) ? padfZ[nStartPointIdx+1] : 0.0,
                             (padfM != NULL) ? padfM[nStartPointIdx+1] : 0.0);
            poLine->set3D( padfZ != NULL );
            poLine->setMeasured( padfM != NULL );
            poCC->addCurveDirectly(poLine);
        }

        else if( pasCurves[i].eType == CURVE_ELLIPSE_BY_CENTER &&
                 nStartPointIdx+1 < nPartStartIdx + nPartPoints )
        {
            const double dfSemiMinor =
              pasCurves[i].u.EllipseByCenter.dfSemiMajor *
              pasCurves[i].u.EllipseByCenter.dfRatioSemiMinor;
            // different sign conventions between ext shape
            // (trigonometric, CCW) and approximateArcAngles (CW)
            const double dfRotationDeg = - pasCurves[i].u.EllipseByCenter.dfRotationDeg;
            const double dfAngleStart = GetAngleOnEllipse(
                    padfX[nStartPointIdx],
                    padfY[nStartPointIdx],
                    pasCurves[i].u.EllipseByCenter.dfX,
                    pasCurves[i].u.EllipseByCenter.dfY,
                    dfRotationDeg,
                    pasCurves[i].u.EllipseByCenter.dfSemiMajor,
                    dfSemiMinor);
            const double dfAngleEnd = GetAngleOnEllipse(
                    padfX[nStartPointIdx+1],
                    padfY[nStartPointIdx+1],
                    pasCurves[i].u.EllipseByCenter.dfX,
                    pasCurves[i].u.EllipseByCenter.dfY,
                    dfRotationDeg,
                    pasCurves[i].u.EllipseByCenter.dfSemiMajor,
                    dfSemiMinor);
            //CPLDebug("OGR", "Start angle=%f, End angle=%f", dfAngleStart, dfAngleEnd);
            /* approximateArcAngles() use CW */
            double dfAngleStartForApprox = -dfAngleStart;
            double dfAngleEndForApprox = -dfAngleEnd;
            if( pasCurves[i].u.EllipseByCenter.bIsComplete )
            {
                dfAngleEndForApprox = dfAngleStartForApprox + 360;
            }
            else if( pasCurves[i].u.EllipseByCenter.bIsMinor )
            {
                if( dfAngleEndForApprox > dfAngleStartForApprox + 180 )
                {
                    dfAngleEndForApprox -= 360;
                }
                else if( dfAngleEndForApprox < dfAngleStartForApprox - 180 )
                {
                    dfAngleEndForApprox += 360;
                }
            }
            else
            {
                if( dfAngleEndForApprox > dfAngleStartForApprox &&
                    dfAngleEndForApprox < dfAngleStartForApprox + 180 )
                {
                    dfAngleEndForApprox -= 360;
                }
                else if( dfAngleEndForApprox < dfAngleStartForApprox &&
                         dfAngleEndForApprox > dfAngleStartForApprox - 180 )
                {
                    dfAngleEndForApprox += 360;
                }
            }
            OGRLineString* poLine = reinterpret_cast<OGRLineString*>(
              OGRGeometryFactory::approximateArcAngles(
                  pasCurves[i].u.EllipseByCenter.dfX,
                  pasCurves[i].u.EllipseByCenter.dfY,
                  (padfZ != NULL) ? padfZ[nStartPointIdx] : 0.0,
                  pasCurves[i].u.EllipseByCenter.dfSemiMajor,
                  dfSemiMinor,
                  dfRotationDeg,
                  dfAngleStartForApprox,
                  dfAngleEndForApprox, 0 ) );
             if( poLine->getNumPoints() >= 2 )
             {
                poLine->setPoint(0,
                                 padfX[nStartPointIdx],
                                 padfY[nStartPointIdx],
                                 (padfZ != NULL) ? padfZ[nStartPointIdx] : 0.0,
                                 (padfM != NULL) ? padfM[nStartPointIdx] : 0.0);
                poLine->setPoint(poLine->getNumPoints()-1,
                                 padfX[nStartPointIdx+1],
                                 padfY[nStartPointIdx+1],
                                 (padfZ != NULL) ? padfZ[nStartPointIdx+1] : 0.0,
                                 (padfM != NULL) ? padfM[nStartPointIdx+1] : 0.0);
             }
             poLine->set3D( padfZ != NULL );
             poLine->setMeasured( padfM != NULL );
             poCC->addCurveDirectly(poLine);
        }

        // Should not happen normally.
        else if( nStartPointIdx+1 < nPartStartIdx + nPartPoints )
        {
            OGRLineString *poLine = new OGRLineString();
            poLine->setPoints( 2,
                                padfX + nStartPointIdx,
                                padfY + nStartPointIdx,
                                (padfZ != NULL) ? padfZ + nStartPointIdx : NULL,
                                (padfM != NULL) ? padfM + nStartPointIdx : NULL );
            poCC->addCurveDirectly(poLine);
        }
        nLastPointIdx = nStartPointIdx + 1;
    }
    if( i == nCurves )
    {
        if( pnLastCurveIdx )
            *pnLastCurveIdx = i;
    }

    // Add terminating linear segments
    if( nLastPointIdx < nPartStartIdx+nPartPoints-1 )
    {
        OGRLineString *poLine = new OGRLineString();
        poLine->setPoints( nPartStartIdx+nPartPoints-1 - nLastPointIdx + 1,
                            padfX + nLastPointIdx,
                            padfY + nLastPointIdx,
                            (padfZ != NULL) ? padfZ + nLastPointIdx : NULL,
                            (padfM != NULL) ? padfM + nLastPointIdx : NULL );
        poCC->addCurveDirectly(poLine);
    }

    if( !bHasCircularArcs )
        return reinterpret_cast<OGRCurve*> (OGR_G_ForceTo(
                            reinterpret_cast<OGRGeometryH>(poCC), wkbLineString, NULL ));
    else
        return poCC;
}

/************************************************************************/
/*                      OGRCreateFromShapeBin()                         */
/*                                                                      */
/*      Translate shapefile binary representation to an OGR             */
/*      geometry.                                                       */
/************************************************************************/

OGRErr OGRCreateFromShapeBin( GByte *pabyShape,
                              OGRGeometry **ppoGeom,
                              int nBytes )

{
    *ppoGeom = NULL;

    if( nBytes < 4 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Shape buffer size (%d) too small",
                 nBytes);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*  Detect zlib compressed shapes and uncompress buffer if necessary    */
/*  NOTE: this seems to be an undocumented feature, even in the         */
/*  extended_shapefile_format.pdf found in the FileGDB API documentation*/
/* -------------------------------------------------------------------- */
    if( nBytes >= 14 &&
        pabyShape[12] == 0x78 && pabyShape[13] == 0xDA /* zlib marker */)
    {
        GInt32 nUncompressedSize, nCompressedSize;
        memcpy( &nUncompressedSize, pabyShape + 4, 4 );
        memcpy( &nCompressedSize, pabyShape + 8, 4 );
        CPL_LSBPTR32( &nUncompressedSize );
        CPL_LSBPTR32( &nCompressedSize );
        if (nCompressedSize + 12 == nBytes &&
            nUncompressedSize > 0)
        {
            GByte* pabyUncompressedBuffer = (GByte*)VSI_MALLOC_VERBOSE(nUncompressedSize);
            if (pabyUncompressedBuffer == NULL)
            {
                return OGRERR_FAILURE;
            }

            size_t nRealUncompressedSize = 0;
            if( CPLZLibInflate( pabyShape + 12, nCompressedSize,
                                pabyUncompressedBuffer, nUncompressedSize,
                                &nRealUncompressedSize ) == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "CPLZLibInflate() failed");
                VSIFree(pabyUncompressedBuffer);
                return OGRERR_FAILURE;
            }

            OGRErr eErr = OGRCreateFromShapeBin(pabyUncompressedBuffer,
                                                ppoGeom,
                                                static_cast<int>(nRealUncompressedSize));

            VSIFree(pabyUncompressedBuffer);

            return eErr;
        }
    }

    int nSHPType = pabyShape[0];

/* -------------------------------------------------------------------- */
/*      Return a NULL geometry when SHPT_NULL is encountered.           */
/*      Watch out, null return does not mean "bad data" it means        */
/*      "no geometry here". Watch the OGRErr for the error status       */
/* -------------------------------------------------------------------- */
    if ( nSHPType == SHPT_NULL )
    {
      *ppoGeom = NULL;
      return OGRERR_NONE;
    }

//    CPLDebug( "PGeo",
//              "Shape type read from PGeo data is nSHPType = %d",
//              nSHPType );

    const bool bIsExtended = ( nSHPType >= SHPT_GENERALPOLYLINE && nSHPType <= SHPT_GENERALMULTIPATCH );

    const bool bHasZ = (
                   nSHPType == SHPT_POINTZ
                || nSHPType == SHPT_POINTZM
                || nSHPType == SHPT_MULTIPOINTZ
                || nSHPType == SHPT_MULTIPOINTZM
                || nSHPType == SHPT_POLYGONZ
                || nSHPType == SHPT_POLYGONZM
                || nSHPType == SHPT_ARCZ
                || nSHPType == SHPT_ARCZM
                || nSHPType == SHPT_MULTIPATCH
                || nSHPType == SHPT_MULTIPATCHM
                || (bIsExtended && (pabyShape[3] & 0x80) != 0 ) );

    const bool bHasM = (
                   nSHPType == SHPT_POINTM
                || nSHPType == SHPT_POINTZM
                || nSHPType == SHPT_MULTIPOINTM
                || nSHPType == SHPT_MULTIPOINTZM
                || nSHPType == SHPT_POLYGONM
                || nSHPType == SHPT_POLYGONZM
                || nSHPType == SHPT_ARCM
                || nSHPType == SHPT_ARCZM
                || nSHPType == SHPT_MULTIPATCHM
                || (bIsExtended && (pabyShape[3] & 0x40) != 0 ) );

    const bool bHasCurves = (bIsExtended && (pabyShape[3] & 0x20) != 0 );

    switch( nSHPType )
    {
      case SHPT_GENERALPOLYLINE:
        nSHPType = SHPT_ARC;
        break;
      case SHPT_GENERALPOLYGON:
        nSHPType = SHPT_POLYGON;
        break;
      case SHPT_GENERALPOINT:
        nSHPType = SHPT_POINT;
        break;
      case SHPT_GENERALMULTIPOINT:
        nSHPType = SHPT_MULTIPOINT;
        break;
      case SHPT_GENERALMULTIPATCH:
        nSHPType = SHPT_MULTIPATCH;
    }

/* ==================================================================== */
/*  Extract vertices for a Polygon or Arc.              */
/* ==================================================================== */
    if(    nSHPType == SHPT_ARC
        || nSHPType == SHPT_ARCZ
        || nSHPType == SHPT_ARCM
        || nSHPType == SHPT_ARCZM
        || nSHPType == SHPT_POLYGON
        || nSHPType == SHPT_POLYGONZ
        || nSHPType == SHPT_POLYGONM
        || nSHPType == SHPT_POLYGONZM
        || nSHPType == SHPT_MULTIPATCH
        || nSHPType == SHPT_MULTIPATCHM)
    {
        GInt32         nPoints, nParts;
        int            nOffset;
        GInt32         *panPartStart;
        GInt32         *panPartType = NULL;

        if (nBytes < 44)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupted Shape : nBytes=%d, nSHPType=%d", nBytes, nSHPType);
            return OGRERR_FAILURE;
        }

/* -------------------------------------------------------------------- */
/*      Extract part/point count, and build vertex and part arrays      */
/*      to proper size.                                                 */
/* -------------------------------------------------------------------- */
        memcpy( &nPoints, pabyShape + 40, 4 );
        memcpy( &nParts, pabyShape + 36, 4 );

        CPL_LSBPTR32( &nPoints );
        CPL_LSBPTR32( &nParts );

        if (nPoints < 0 || nParts < 0 ||
            nPoints > 50 * 1000 * 1000 || nParts > 10 * 1000 * 1000)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Corrupted Shape : nPoints=%d, nParts=%d.",
                     nPoints, nParts);
            return OGRERR_FAILURE;
        }

        int bIsMultiPatch = ( nSHPType == SHPT_MULTIPATCH || nSHPType == SHPT_MULTIPATCHM );

        /* With the previous checks on nPoints and nParts, */
        /* we should not overflow here and after */
        /* since 50 M * (16 + 8 + 8) = 1 600 MB */
        int nRequiredSize = 44 + 4 * nParts + 16 * nPoints;
        if ( bHasZ )
        {
            nRequiredSize += 16 + 8 * nPoints;
        }
        if ( bHasM )
        {
            nRequiredSize += 16 + 8 * nPoints;
        }
        if( bIsMultiPatch )
        {
            nRequiredSize += 4 * nParts;
        }
        if (nRequiredSize > nBytes)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupted Shape : nPoints=%d, nParts=%d, nBytes=%d, nSHPType=%d, nRequiredSize=%d",
                     nPoints, nParts, nBytes, nSHPType, nRequiredSize);
            return OGRERR_FAILURE;
        }

        panPartStart = (GInt32 *) VSI_MALLOC2_VERBOSE(nParts,sizeof(GInt32));
        if (nParts != 0 && panPartStart == NULL)
        {
            return OGRERR_FAILURE;
        }

/* -------------------------------------------------------------------- */
/*      Copy out the part array from the record.                        */
/* -------------------------------------------------------------------- */
        memcpy( panPartStart, pabyShape + 44, 4 * nParts );
        for( int i = 0; i < nParts; i++ )
        {
            CPL_LSBPTR32( panPartStart + i );

            /* We check that the offset is inside the vertex array */
            if (panPartStart[i] < 0 ||
                panPartStart[i] >= nPoints)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Corrupted Shape : panPartStart[%d] = %d, nPoints = %d",
                        i, panPartStart[i], nPoints);
                CPLFree(panPartStart);
                return OGRERR_FAILURE;
            }
            if (i > 0 && panPartStart[i] <= panPartStart[i-1])
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Corrupted Shape : panPartStart[%d] = %d, panPartStart[%d] = %d",
                        i, panPartStart[i], i - 1, panPartStart[i - 1]);
                CPLFree(panPartStart);
                return OGRERR_FAILURE;
            }
        }

        nOffset = 44 + 4*nParts;

/* -------------------------------------------------------------------- */
/*      If this is a multipatch, we will also have parts types.         */
/* -------------------------------------------------------------------- */
        if( bIsMultiPatch )
        {
            panPartType = (GInt32 *) VSI_MALLOC2_VERBOSE(nParts,sizeof(GInt32));
            if (panPartType == NULL)
            {
                CPLFree(panPartStart);
                return OGRERR_FAILURE;
            }

            memcpy( panPartType, pabyShape + nOffset, 4*nParts );
            for( int i = 0; i < nParts; i++ )
            {
                CPL_LSBPTR32( panPartType + i );
            }
            nOffset += 4*nParts;
        }

/* -------------------------------------------------------------------- */
/*      Copy out the vertices from the record.                          */
/* -------------------------------------------------------------------- */
        double *padfX = (double *) VSI_MALLOC_VERBOSE(sizeof(double)*nPoints);
        double *padfY = (double *) VSI_MALLOC_VERBOSE(sizeof(double)*nPoints);
        double *padfZ = (double *) VSI_CALLOC_VERBOSE(sizeof(double),nPoints);
        double *padfM = (double *) (bHasM ? VSI_CALLOC_VERBOSE(sizeof(double),nPoints) : NULL);
        if ( nPoints != 0 && (padfX == NULL || padfY == NULL || padfZ == NULL || (bHasM && padfM == NULL)) )
        {
            CPLFree( panPartStart );
            CPLFree( panPartType );
            CPLFree( padfX );
            CPLFree( padfY );
            CPLFree( padfZ );
            CPLFree( padfM );
            return OGRERR_FAILURE;
        }

        for( int i = 0; i < nPoints; i++ )
        {
            memcpy(padfX + i, pabyShape + nOffset + i * 16, 8 );
            memcpy(padfY + i, pabyShape + nOffset + i * 16 + 8, 8 );
            CPL_LSBPTR64( padfX + i );
            CPL_LSBPTR64( padfY + i );
        }

        nOffset += 16*nPoints;

/* -------------------------------------------------------------------- */
/*      If we have a Z coordinate, collect that now.                    */
/* -------------------------------------------------------------------- */
        if( bHasZ )
        {
            for( int i = 0; i < nPoints; i++ )
            {
                memcpy( padfZ + i, pabyShape + nOffset + 16 + i*8, 8 );
                CPL_LSBPTR64( padfZ + i );
            }

            nOffset += 16 + 8*nPoints;
        }

/* -------------------------------------------------------------------- */
/*      If we have a M coordinate, collect that now.                    */
/* -------------------------------------------------------------------- */
        if( bHasM )
        {
            for( int i = 0; i < nPoints; i++ )
            {
                memcpy( padfM + i, pabyShape + nOffset + 16 + i*8, 8 );
                CPL_LSBPTR64( padfM + i );
            }

            nOffset += 16 + 8*nPoints;
        }

/* -------------------------------------------------------------------- */
/*      If we have curves, collect them now.                            */
/* -------------------------------------------------------------------- */
        int nCurves = 0;
        CurveSegment* pasCurves = NULL;
        if( bHasCurves && nOffset + 4 <= nBytes )
        {
            memcpy( &nCurves, pabyShape + nOffset, 4 );
            CPL_LSBPTR32(&nCurves);
            nOffset += 4;
#ifdef DEBUG_VERBOSE
            CPLDebug("OGR", "nCurves = %d", nCurves);
#endif
            if( nCurves < 0 || nCurves > (nBytes - nOffset) / (8 + 20) )
            {
                CPLDebug("OGR", "Invalid nCurves = %d", nCurves);
                nCurves = 0;
            }
            pasCurves = (CurveSegment *) VSI_MALLOC2_VERBOSE(sizeof(CurveSegment), nCurves);
            if( pasCurves == NULL )
            {
                nCurves = 0;
            }
            int iCurve = 0;
            for( int i = 0; i < nCurves; i++ )
            {
                if( nOffset + 8 > nBytes )
                {
                    CPLDebug("OGR", "Not enough bytes");
                    break;
                }
                int nStartPointIdx;
                memcpy( &nStartPointIdx, pabyShape + nOffset, 4 );
                CPL_LSBPTR32(&nStartPointIdx);
                nOffset += 4;
                int nSegmentType;
                memcpy( &nSegmentType, pabyShape + nOffset, 4 );
                CPL_LSBPTR32(&nSegmentType);
                nOffset += 4;
#ifdef DEBUG_VERBOSE
                CPLDebug("OGR", "[%d] nStartPointIdx = %d, segmentType = %d", i, nSegmentType, nSegmentType);
#endif
                if( nStartPointIdx < 0 || nStartPointIdx >= nPoints ||
                    (iCurve > 0 && nStartPointIdx <= pasCurves[iCurve-1].nStartPointIdx) )
                {
                    CPLDebug("OGR", "Invalid nStartPointIdx = %d", nStartPointIdx);
                    break;
                }
                pasCurves[iCurve].nStartPointIdx = nStartPointIdx;
                if( nSegmentType == EXT_SHAPE_SEGMENT_ARC )
                {
                    if( nOffset + 20 > nBytes )
                    {
                        CPLDebug("OGR", "Not enough bytes");
                        break;
                    }
                    double dfVal1, dfVal2;
                    memcpy( &dfVal1, pabyShape + nOffset + 0, 8 );
                    CPL_LSBPTR64(&dfVal1);
                    memcpy( &dfVal2, pabyShape + nOffset + 8, 8 );
                    CPL_LSBPTR64(&dfVal2);
                    int nBits;
                    memcpy( &nBits, pabyShape + nOffset + 16, 4 );
                    CPL_LSBPTR32(&nBits);

                    (void)EXT_SHAPE_ARC_MINOR;
#ifdef DEBUG_VERBOSE
                    CPLDebug("OGR", "Arc: ");
                    CPLDebug("OGR", " dfVal1 = %f, dfVal2 = %f, nBits=%X", dfVal1, dfVal2, nBits);
                    if( nBits & EXT_SHAPE_ARC_EMPTY ) CPLDebug("OGR", "  IsEmpty");
                    if( nBits & EXT_SHAPE_ARC_CCW ) CPLDebug("OGR", "  IsCCW");
                    if( nBits & EXT_SHAPE_ARC_MINOR ) CPLDebug("OGR", " IsMinor");
                    if( nBits & EXT_SHAPE_ARC_LINE ) CPLDebug("OGR", "  IsLine");
                    if( nBits & EXT_SHAPE_ARC_POINT ) CPLDebug("OGR", "  IsPoint");
                    if( nBits & EXT_SHAPE_ARC_IP ) CPLDebug("OGR", "  DefinedIP");
#endif
                    if( (nBits & EXT_SHAPE_ARC_IP) != 0 )
                    {
                        pasCurves[iCurve].eType = CURVE_ARC_INTERIOR_POINT;
                        pasCurves[iCurve].u.ArcByIntermediatePoint.dfX = dfVal1;
                        pasCurves[iCurve].u.ArcByIntermediatePoint.dfY = dfVal2;
                        iCurve++;
                    }
                    else if( (nBits & EXT_SHAPE_ARC_EMPTY) == 0 &&
                             (nBits & EXT_SHAPE_ARC_LINE) == 0 &&
                             (nBits & EXT_SHAPE_ARC_POINT) == 0 )
                    {
                        // This is the old deprecated way
                        pasCurves[iCurve].eType = CURVE_ARC_CENTER_POINT;
                        pasCurves[iCurve].u.ArcByCenterPoint.dfX = dfVal1;
                        pasCurves[iCurve].u.ArcByCenterPoint.dfY = dfVal2;
                        pasCurves[iCurve].u.ArcByCenterPoint.bIsCCW = ( nBits & EXT_SHAPE_ARC_CCW ) != 0;
                        iCurve++;
                    }
                    nOffset += 16 + 4;
                }
                else if( nSegmentType == EXT_SHAPE_SEGMENT_BEZIER )
                {
                    if( nOffset + 32 > nBytes )
                    {
                        CPLDebug("OGR", "Not enough bytes");
                        break;
                    }
                    double dfX1, dfY1;
                    memcpy( &dfX1, pabyShape + nOffset + 0, 8 );
                    CPL_LSBPTR64(&dfX1);
                    memcpy( &dfY1, pabyShape + nOffset + 8, 8 );
                    CPL_LSBPTR64(&dfY1);
                    double dfX2, dfY2;
                    memcpy( &dfX2, pabyShape + nOffset + 16, 8 );
                    CPL_LSBPTR64(&dfX2);
                    memcpy( &dfY2, pabyShape + nOffset + 24, 8 );
                    CPL_LSBPTR64(&dfY2);
#ifdef DEBUG_VERBOSE
                    CPLDebug("OGR", "Bezier:");
                    CPLDebug("OGR", "  dfX1 = %f, dfY1 = %f", dfX1, dfY1);
                    CPLDebug("OGR", "  dfX2 = %f, dfY2 = %f", dfX2, dfY2);
#endif
                    pasCurves[iCurve].eType = CURVE_BEZIER;
                    pasCurves[iCurve].u.Bezier.dfX1 = dfX1;
                    pasCurves[iCurve].u.Bezier.dfY1 = dfY1;
                    pasCurves[iCurve].u.Bezier.dfX2 = dfX2;
                    pasCurves[iCurve].u.Bezier.dfY2 = dfY2;
                    iCurve++;
                    nOffset += 32;
                }
                else if( nSegmentType == EXT_SHAPE_SEGMENT_ELLIPSE )
                {
                    if( nOffset + 44 > nBytes )
                    {
                        CPLDebug("OGR", "Not enough bytes");
                        break;
                    }
                    double dfVS0;
                    memcpy( &dfVS0, pabyShape + nOffset, 8 );
                    nOffset += 8;
                    CPL_LSBPTR64(&dfVS0);

                    double dfVS1;
                    memcpy( &dfVS1, pabyShape + nOffset, 8 );
                    nOffset += 8;
                    CPL_LSBPTR64(&dfVS1);

                    double dfRotationOrFromV;
                    memcpy( &dfRotationOrFromV, pabyShape + nOffset, 8 );
                    nOffset += 8;
                    CPL_LSBPTR64(&dfRotationOrFromV);

                    double dfSemiMajor;
                    memcpy( &dfSemiMajor, pabyShape + nOffset, 8 );
                    nOffset += 8;
                    CPL_LSBPTR64(&dfSemiMajor);

                    double dfMinorMajorRatioOrDeltaV;
                    memcpy( &dfMinorMajorRatioOrDeltaV, pabyShape + nOffset, 8 );
                    nOffset += 8;
                    CPL_LSBPTR64(&dfMinorMajorRatioOrDeltaV);

                    int nBits;
                    memcpy( &nBits, pabyShape + nOffset, 4 );
                    CPL_LSBPTR32(&nBits);
                    nOffset += 4;

                    (void)EXT_SHAPE_ELLIPSE_EMPTY;
                    (void)EXT_SHAPE_ELLIPSE_LINE;
                    (void)EXT_SHAPE_ELLIPSE_POINT;
                    (void)EXT_SHAPE_ELLIPSE_CIRCULAR;
                    (void)EXT_SHAPE_ELLIPSE_CCW;
                    (void)EXT_SHAPE_ELLIPSE_MINOR;
                    (void)EXT_SHAPE_ELLIPSE_COMPLETE;
#ifdef DEBUG_VERBOSE
                    CPLDebug("OGR", "Ellipse:");
                    CPLDebug("OGR", "  dfVS0 = %f", dfVS0);
                    CPLDebug("OGR", "  dfVS1 = %f", dfVS1);
                    CPLDebug("OGR", "  dfRotationOrFromV = %f", dfRotationOrFromV);
                    CPLDebug("OGR", "  dfSemiMajor = %f", dfSemiMajor);
                    CPLDebug("OGR", "  dfMinorMajorRatioOrDeltaV = %f", dfMinorMajorRatioOrDeltaV);
                    CPLDebug("OGR", "  nBits=%X", nBits);

                    if (nBits & EXT_SHAPE_ELLIPSE_EMPTY) CPLDebug("OGR", "   IsEmpty");
                    if (nBits & EXT_SHAPE_ELLIPSE_LINE) CPLDebug("OGR", "   IsLine");
                    if (nBits & EXT_SHAPE_ELLIPSE_POINT) CPLDebug("OGR", "   IsPoint");
                    if (nBits & EXT_SHAPE_ELLIPSE_CIRCULAR) CPLDebug("OGR", "   IsCircular");
                    if (nBits & EXT_SHAPE_ELLIPSE_CENTER_TO) CPLDebug("OGR", "   CenterTo");
                    if (nBits & EXT_SHAPE_ELLIPSE_CENTER_FROM) CPLDebug("OGR", "   CenterFrom");
                    if (nBits & EXT_SHAPE_ELLIPSE_CCW) CPLDebug("OGR", "   IsCCW");
                    if (nBits & EXT_SHAPE_ELLIPSE_MINOR) CPLDebug("OGR", "   IsMinor");
                    if (nBits & EXT_SHAPE_ELLIPSE_COMPLETE) CPLDebug("OGR", "   IsComplete");
#endif

                    if( (nBits & EXT_SHAPE_ELLIPSE_CENTER_TO) == 0 &&
                        (nBits & EXT_SHAPE_ELLIPSE_CENTER_FROM) == 0 )
                    {
                        pasCurves[iCurve].eType = CURVE_ELLIPSE_BY_CENTER;
                        pasCurves[iCurve].u.EllipseByCenter.dfX = dfVS0;
                        pasCurves[iCurve].u.EllipseByCenter.dfY = dfVS1;
                        pasCurves[iCurve].u.EllipseByCenter.dfRotationDeg = dfRotationOrFromV / M_PI * 180;
                        pasCurves[iCurve].u.EllipseByCenter.dfSemiMajor = dfSemiMajor;
                        pasCurves[iCurve].u.EllipseByCenter.dfRatioSemiMinor = dfMinorMajorRatioOrDeltaV;
                        pasCurves[iCurve].u.EllipseByCenter.bIsMinor = ((nBits & EXT_SHAPE_ELLIPSE_MINOR) != 0);
                        pasCurves[iCurve].u.EllipseByCenter.bIsComplete = ((nBits & EXT_SHAPE_ELLIPSE_COMPLETE) != 0);
                        iCurve++;
                    }

                }
                else
                {
                    CPLDebug("OGR", "unhandled segmentType = %d", nSegmentType);
                }
            }

            nCurves = iCurve;
        }

/* -------------------------------------------------------------------- */
/*      Build corresponding OGR objects.                                */
/* -------------------------------------------------------------------- */
        if(    nSHPType == SHPT_ARC
            || nSHPType == SHPT_ARCZ
            || nSHPType == SHPT_ARCM
            || nSHPType == SHPT_ARCZM )
        {
/* -------------------------------------------------------------------- */
/*      Arc - As LineString                                             */
/* -------------------------------------------------------------------- */
            if( nParts == 1 )
            {
                if( nCurves > 0 )
                {
                    *ppoGeom = OGRShapeCreateCompoundCurve(
                      0, nPoints, pasCurves, nCurves, 0,
                      padfX, padfY, (bHasZ) ? padfZ : NULL, padfM, NULL );
                }
                else
                {
                    OGRLineString *poLine = new OGRLineString();
                    *ppoGeom = poLine;

                    poLine->setPoints( nPoints, padfX, padfY, padfZ, padfM );
                }
            }

/* -------------------------------------------------------------------- */
/*      Arc - As MultiLineString                                        */
/* -------------------------------------------------------------------- */
            else
            {
                if( nCurves > 0 )
                {
                    OGRMultiCurve *poMulti = new OGRMultiCurve;
                    *ppoGeom = poMulti;

                    int iCurveIdx = 0;
                    for( int i = 0; i < nParts; i++ )
                    {
                        int nVerticesInThisPart;

                        if( i == nParts-1 )
                            nVerticesInThisPart = nPoints - panPartStart[i];
                        else
                            nVerticesInThisPart =
                                panPartStart[i+1] - panPartStart[i];

                        poMulti->addGeometryDirectly( OGRShapeCreateCompoundCurve(
                                panPartStart[i], nVerticesInThisPart,
                                pasCurves, nCurves, iCurveIdx,
                                padfX, padfY, (bHasZ) ? padfZ : NULL, padfM, &iCurveIdx ) );
                    }
                }
                else
                {
                    OGRMultiLineString *poMulti = new OGRMultiLineString;
                    *ppoGeom = poMulti;

                    for( int i = 0; i < nParts; i++ )
                    {
                        OGRLineString *poLine = new OGRLineString;
                        int nVerticesInThisPart;

                        if( i == nParts-1 )
                            nVerticesInThisPart = nPoints - panPartStart[i];
                        else
                            nVerticesInThisPart =
                                panPartStart[i+1] - panPartStart[i];

                        poLine->setPoints( nVerticesInThisPart,
                                          padfX + panPartStart[i],
                                          padfY + panPartStart[i],
                                          padfZ + panPartStart[i],
                                          (padfM != NULL) ? padfM + panPartStart[i] : NULL );

                        poMulti->addGeometryDirectly( poLine );
                    }
                }
            }
        } /* ARC */

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
        else if(    nSHPType == SHPT_POLYGON
                 || nSHPType == SHPT_POLYGONZ
                 || nSHPType == SHPT_POLYGONM
                 || nSHPType == SHPT_POLYGONZM )
        {
            if( nCurves > 0 && nParts != 0)
            {
                if (nParts == 1)
                {
                    OGRCurvePolygon *poOGRPoly = new OGRCurvePolygon;
                    *ppoGeom = poOGRPoly;
                    int nVerticesInThisPart = nPoints - panPartStart[0];

                    OGRCurve* poRing = OGRShapeCreateCompoundCurve(
                      panPartStart[0], nVerticesInThisPart, pasCurves, nCurves, 0,
                      padfX, padfY, (bHasZ) ? padfZ : NULL, padfM, NULL );
                    if( poOGRPoly->addRingDirectly( poRing ) != OGRERR_NONE )
                    {
                        delete poRing;
                        delete poOGRPoly;
                        *ppoGeom = NULL;
                    }
                }
                else
                {
                    OGRGeometry *poOGR = NULL;
                    OGRCurvePolygon** tabPolygons = new OGRCurvePolygon*[nParts];

                    int iCurveIdx = 0;
                    for( int i = 0; i < nParts; i++ )
                    {
                        tabPolygons[i] = new OGRCurvePolygon();
                        int nVerticesInThisPart;

                        if( i == nParts-1 )
                            nVerticesInThisPart = nPoints - panPartStart[i];
                        else
                            nVerticesInThisPart =
                                panPartStart[i+1] - panPartStart[i];


                        OGRCurve* poRing = OGRShapeCreateCompoundCurve(
                            panPartStart[i], nVerticesInThisPart,
                            pasCurves, nCurves, iCurveIdx,
                            padfX, padfY, (bHasZ) ? padfZ : NULL, padfM, &iCurveIdx );
                        if( tabPolygons[i]->addRingDirectly( poRing ) != OGRERR_NONE )
                        {
                            delete poRing;
                            for( ; i >=0 ; --i )
                                delete tabPolygons[i];
                            delete[] tabPolygons;
                            tabPolygons = NULL;
                            *ppoGeom = NULL;
                            break;
                        }
                    }

                    if( tabPolygons != NULL )
                    {
                        int isValidGeometry;
                        const char* papszOptions[] = { "METHOD=ONLY_CCW", NULL };
                        poOGR = OGRGeometryFactory::organizePolygons(
                            (OGRGeometry**)tabPolygons, nParts, &isValidGeometry, papszOptions );

                        if (!isValidGeometry)
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                    "Geometry of polygon cannot be translated to Simple Geometry. "
                                    "All polygons will be contained in a multipolygon.\n");
                        }

                        *ppoGeom = poOGR;
                        delete[] tabPolygons;
                    }
                }
            }
            else if (nParts != 0)
            {
                if (nParts == 1)
                {
                    OGRPolygon *poOGRPoly = new OGRPolygon;
                    *ppoGeom = poOGRPoly;
                    OGRLinearRing *poRing = new OGRLinearRing;
                    int nVerticesInThisPart = nPoints - panPartStart[0];

                    poRing->setPoints( nVerticesInThisPart,
                                       padfX + panPartStart[0],
                                       padfY + panPartStart[0],
                                       padfZ + panPartStart[0],
                                       (padfM != NULL) ? padfM + panPartStart[0] : NULL );

                    if( poOGRPoly->addRingDirectly( poRing ) != OGRERR_NONE )
                    {
                        delete poRing;
                        delete poOGRPoly;
                        *ppoGeom = NULL;
                    }
                }
                else
                {
                    OGRGeometry *poOGR = NULL;
                    OGRPolygon** tabPolygons = new OGRPolygon*[nParts];

                    for( int i = 0; i < nParts; i++ )
                    {
                        tabPolygons[i] = new OGRPolygon();
                        OGRLinearRing *poRing = new OGRLinearRing;
                        int nVerticesInThisPart;

                        if( i == nParts-1 )
                            nVerticesInThisPart = nPoints - panPartStart[i];
                        else
                            nVerticesInThisPart =
                                panPartStart[i+1] - panPartStart[i];

                        poRing->setPoints( nVerticesInThisPart,
                                           padfX + panPartStart[i],
                                           padfY + panPartStart[i],
                                           padfZ + panPartStart[i],
                                           (padfM != NULL) ? padfM + panPartStart[i] : NULL );
                        if( tabPolygons[i]->addRingDirectly(poRing) != OGRERR_NONE )
                        {
                            delete poRing;
                            for( ; i >=0 ; --i )
                                delete tabPolygons[i];
                            delete[] tabPolygons;
                            tabPolygons = NULL;
                            *ppoGeom = NULL;
                            break;
                        }
                    }

                    if( tabPolygons != NULL )
                    {
                        int isValidGeometry;
                        const char* papszOptions[] = { "METHOD=ONLY_CCW", NULL };
                        poOGR = OGRGeometryFactory::organizePolygons(
                            (OGRGeometry**)tabPolygons, nParts, &isValidGeometry, papszOptions );

                        if (!isValidGeometry)
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                    "Geometry of polygon cannot be translated to Simple Geometry. "
                                    "All polygons will be contained in a multipolygon.\n");
                        }

                        *ppoGeom = poOGR;
                        delete[] tabPolygons;
                    }
                }
            }
        } /* polygon */

/* -------------------------------------------------------------------- */
/*      Multipatch                                                      */
/* -------------------------------------------------------------------- */
        else if( bIsMultiPatch )
        {
            *ppoGeom = OGRCreateFromMultiPatch( nParts,
                                                panPartStart,
                                                panPartType,
                                                nPoints,
                                                padfX,
                                                padfY,
                                                padfZ );
        }

        CPLFree( panPartStart );
        CPLFree( panPartType );
        CPLFree( padfX );
        CPLFree( padfY );
        CPLFree( padfZ );
        CPLFree( padfM );
        CPLFree( pasCurves );

        if (*ppoGeom != NULL)
        {
            if( !bHasZ )
                (*ppoGeom)->set3D(FALSE);
        }

        return (*ppoGeom != NULL) ? OGRERR_NONE : OGRERR_FAILURE;
    }

/* ==================================================================== */
/*  Extract vertices for a MultiPoint.                  */
/* ==================================================================== */
    else if(    nSHPType == SHPT_MULTIPOINT
             || nSHPType == SHPT_MULTIPOINTM
             || nSHPType == SHPT_MULTIPOINTZ
             || nSHPType == SHPT_MULTIPOINTZM )
    {
      GInt32 nPoints;
      GInt32 nOffsetZ;
      GInt32 nOffsetM = 0;

      memcpy( &nPoints, pabyShape + 36, 4 );
      CPL_LSBPTR32( &nPoints );

      if (nPoints < 0 || nPoints > 50 * 1000 * 1000 )
      {
          CPLError(CE_Failure, CPLE_AppDefined, "Corrupted Shape : nPoints=%d.",
                   nPoints);
          return OGRERR_FAILURE;
      }

      nOffsetZ = 40 + 2*8*nPoints + 2*8;
      if( bHasM )
          nOffsetM = (bHasZ) ? nOffsetZ + 2*8 * 8*nPoints : nOffsetZ;

      OGRMultiPoint *poMultiPt = new OGRMultiPoint;
      *ppoGeom = poMultiPt;

      for( int i = 0; i < nPoints; i++ )
      {
          double x, y;
          OGRPoint *poPt = new OGRPoint;

          /* Copy X */
          memcpy(&x, pabyShape + 40 + i*16, 8);
          CPL_LSBPTR64(&x);
          poPt->setX(x);

          /* Copy Y */
          memcpy(&y, pabyShape + 40 + i*16 + 8, 8);
          CPL_LSBPTR64(&y);
          poPt->setY(y);

          /* Copy Z */
          if ( bHasZ )
          {
            double z;
            memcpy(&z, pabyShape + nOffsetZ + i*8, 8);
            CPL_LSBPTR64(&z);
            poPt->setZ(z);
          }

          /* Copy M */
          if ( bHasM )
          {
            double m;
            memcpy(&m, pabyShape + nOffsetM + i*8, 8);
            CPL_LSBPTR64(&m);
            poPt->setM(m);
          }

          poMultiPt->addGeometryDirectly( poPt );
      }

      poMultiPt->set3D( bHasZ );
      poMultiPt->setMeasured( bHasM );

      return OGRERR_NONE;
    }

/* ==================================================================== */
/*      Extract vertices for a point.                                   */
/* ==================================================================== */
    else if(    nSHPType == SHPT_POINT
             || nSHPType == SHPT_POINTM
             || nSHPType == SHPT_POINTZ
             || nSHPType == SHPT_POINTZM )
    {
        /* int nOffset; */
        double  dfX, dfY, dfZ = 0, dfM = 0;

        if (nBytes < 4 + 8 + 8 + ((bHasZ) ? 8 : 0) + ((bHasM) ? 8 : 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupted Shape : nBytes=%d, nSHPType=%d", nBytes, nSHPType);
            return OGRERR_FAILURE;
        }

        memcpy( &dfX, pabyShape + 4, 8 );
        memcpy( &dfY, pabyShape + 4 + 8, 8 );

        CPL_LSBPTR64( &dfX );
        CPL_LSBPTR64( &dfY );
        /* nOffset = 20 + 8; */

        if( bHasZ )
        {
            memcpy( &dfZ, pabyShape + 4 + 16, 8 );
            CPL_LSBPTR64( &dfZ );
        }

        if( bHasM )
        {
            memcpy( &dfM, pabyShape + 4 + 16 + ((bHasZ) ? 8 : 0), 8 );
            CPL_LSBPTR64( &dfM );
        }

        if( bHasZ && bHasM )
            *ppoGeom = new OGRPoint( dfX, dfY, dfZ, dfM );
        else if( bHasZ )
            *ppoGeom = new OGRPoint( dfX, dfY, dfZ );
        else if( bHasM )
        {
            OGRPoint* poPoint = new OGRPoint( dfX, dfY );
            poPoint->setM(dfM);
            *ppoGeom = poPoint;
        }
        else
            *ppoGeom = new OGRPoint( dfX, dfY );

        return OGRERR_NONE;
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "Unsupported geometry type: %d",
              nSHPType );

    return OGRERR_FAILURE;
}
