/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCircularString geometry class.
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2010, 2014, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <assert.h>
#include <vector>

CPL_CVSID("$Id");

/************************************************************************/
/*                         OGRCircularString()                          */
/************************************************************************/

/**
 * \brief Create an empty circular string.
 */

OGRCircularString::OGRCircularString()

{
}

/************************************************************************/
/*                        ~OGRCircularString()                          */
/************************************************************************/

OGRCircularString::~OGRCircularString()

{
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRCircularString::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbCircularStringZ;
    else
        return wkbCircularString;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRCircularString::getGeometryName() const

{
    return "CIRCULARSTRING";
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRCircularString::importFromWkb( unsigned char * pabyData,
                                         int nSize,
                                         OGRwkbVariant eWkbVariant )

{
    OGRErr eErr = OGRSimpleCurve::importFromWkb(pabyData, nSize, eWkbVariant);
    if (eErr == OGRERR_NONE)
    {
        if (!IsValidFast())
        {
            empty();
            return OGRERR_CORRUPT_DATA;
        }
    }
    return eErr;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr  OGRCircularString::exportToWkb( OGRwkbByteOrder eByteOrder,
                                        unsigned char * pabyData,
                                        OGRwkbVariant eWkbVariant  ) const

{
    if (!IsValidFast())
    {
        return OGRERR_FAILURE;
    }

    if( eWkbVariant == wkbVariantOldOgc ) /* does not make sense for new geometries, so patch it */
        eWkbVariant = wkbVariantIso;
    return OGRSimpleCurve::exportToWkb(eByteOrder, pabyData, eWkbVariant);
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.  Currently this is     */
/*      `CIRCULARSTRING [Z] ( x y [z], x y [z], ...)',                  */
/************************************************************************/

OGRErr OGRCircularString::importFromWkt( char ** ppszInput )

{
    OGRErr eErr = OGRSimpleCurve::importFromWkt(ppszInput);
    if (eErr == OGRERR_NONE)
    {
        if (!IsValidFast())
        {
            empty();
            return OGRERR_CORRUPT_DATA;
        }
    }
    return eErr;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

OGRErr OGRCircularString::exportToWkt( char ** ppszDstText,
                                       CPL_UNUSED OGRwkbVariant eWkbVariant ) const

{
    if (!IsValidFast())
    {
        return OGRERR_FAILURE;
    }

    return OGRSimpleCurve::exportToWkt(ppszDstText, wkbVariantIso);
}

/************************************************************************/
/*                             get_Length()                             */
/*                                                                      */
/*      For now we return a simple euclidian 2D distance.               */
/************************************************************************/

double OGRCircularString::get_Length() const

{
    double dfLength = 0.0;
    double R, cx, cy, alpha0, alpha1, alpha2;
    int i;
    for(i=0;i<nPointCount-2;i+=2)
    {
        double x0 = paoPoints[i].x, y0 = paoPoints[i].y,
               x1 = paoPoints[i+1].x, y1 = paoPoints[i+1].y,
               x2 = paoPoints[i+2].x, y2 = paoPoints[i+2].y;
        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                            R, cx, cy, alpha0, alpha1, alpha2) )
        {
            dfLength += fabs(alpha2 - alpha0) * R;
        }
        else
        {
            dfLength += sqrt((x2-x0)*(x2-x0)+(y2-y0)*(y2-y0));
        }
    }
    return dfLength;
}

/************************************************************************/
/*                       ExtendEnvelopeWithCircular()                   */
/************************************************************************/

void OGRCircularString::ExtendEnvelopeWithCircular( OGREnvelope * psEnvelope ) const
{
    if( !IsValidFast() || nPointCount == 0 )
        return;

    /* Loop through circular portions and determine if they include some */
    /* extremities of the circle */
    for(int i=0;i<nPointCount-2;i+=2)
    {
        double x0 = paoPoints[i].x, y0 = paoPoints[i].y,
               x1 = paoPoints[i+1].x, y1 = paoPoints[i+1].y,
               x2 = paoPoints[i+2].x, y2 = paoPoints[i+2].y;
        double R, cx, cy, alpha0, alpha1, alpha2;
        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                            R, cx, cy, alpha0, alpha1, alpha2))
        {
            int quadrantStart = (int)floor(alpha0 / (M_PI / 2));
            int quadrantEnd  = (int)floor(alpha2 / (M_PI / 2));
            if( quadrantStart > quadrantEnd )
            {
                int tmp = quadrantStart;
                quadrantStart = quadrantEnd;
                quadrantEnd = tmp;
            }
            /* Transition trough quadrants in counter-clock wise direction */
            for( i=quadrantStart+1; i<=quadrantEnd; i++)
            {
                switch( ((i+8)%4) )
                {
                    case 0:
                        psEnvelope->MaxX = MAX(psEnvelope->MaxX, cx + R);
                        break;
                    case 1:
                        psEnvelope->MaxY = MAX(psEnvelope->MaxY, cy + R);
                        break;
                    case 2:
                        psEnvelope->MinX = MIN(psEnvelope->MinX, cx - R);
                        break;
                    case 3:
                        psEnvelope->MinY = MIN(psEnvelope->MaxY, cy - R);
                        break;
                    default:
                        CPLAssert(FALSE);
                        break;
                }
            }
        }
    }
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCircularString::getEnvelope( OGREnvelope * psEnvelope ) const

{
    OGRSimpleCurve::getEnvelope(psEnvelope);
    ExtendEnvelopeWithCircular(psEnvelope);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCircularString::getEnvelope( OGREnvelope3D * psEnvelope ) const

{
    OGRSimpleCurve::getEnvelope(psEnvelope);
    ExtendEnvelopeWithCircular(psEnvelope);
}

/************************************************************************/
/*                     OGRCircularString::segmentize()                  */
/************************************************************************/

void OGRCircularString::segmentize( double dfMaxLength )
{
    if( !IsValidFast() || nPointCount == 0 )
        return;

    /* So as to make sure that the same line followed in both directions */
    /* result in the same segmentized line */
    if ( paoPoints[0].x < paoPoints[nPointCount - 1].x ||
         (paoPoints[0].x == paoPoints[nPointCount - 1].x &&
          paoPoints[0].y < paoPoints[nPointCount - 1].y) )
    {
        reversePoints();
        segmentize(dfMaxLength);
        reversePoints();
    }

    std::vector<OGRRawPoint> aoRawPoint;
    std::vector<double> adfZ;
    for(int i=0;i<nPointCount-2;i+=2)
    {
        double x0 = paoPoints[i].x, y0 = paoPoints[i].y,
               x1 = paoPoints[i+1].x, y1 = paoPoints[i+1].y,
               x2 = paoPoints[i+2].x, y2 = paoPoints[i+2].y;
        double R, cx, cy, alpha0, alpha1, alpha2;

        aoRawPoint.push_back(OGRRawPoint(x0,y0));
        if( padfZ )
            adfZ.push_back(padfZ[i]);

        /* We have strong constraints on the number of intermediate points */
        /* we can add */

        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                            R, cx, cy, alpha0, alpha1, alpha2) )
        {
            /* It is an arc circle */
            double dfSegmentLength1 = fabs(alpha1 - alpha0) * R;
            double dfSegmentLength2 = fabs(alpha2 - alpha1) * R;
            if( dfSegmentLength1 > dfMaxLength || dfSegmentLength2 > dfMaxLength )
            {
                int nIntermediatePoints = 1 + 2 * (int)floor(dfSegmentLength1 / dfMaxLength / 2);
                double dfStep = (alpha1 - alpha0) / (nIntermediatePoints + 1);
                for(int j=1;j<=nIntermediatePoints;j++)
                {
                    double alpha = alpha0 + dfStep * j;
                    double x = cx + R * cos(alpha), y = cy + R * sin(alpha);
                    aoRawPoint.push_back(OGRRawPoint(x,y));
                    if( padfZ )
                    {
                        double z = padfZ[i] + (padfZ[i+1] - padfZ[i]) * (alpha - alpha0) / (alpha1 - alpha0);
                        adfZ.push_back(z);
                    }
                }
            }
            aoRawPoint.push_back(OGRRawPoint(x1,y1));
            if( padfZ )
                adfZ.push_back(padfZ[i+1]);

            if( dfSegmentLength1 > dfMaxLength || dfSegmentLength2 > dfMaxLength )
            {
                int nIntermediatePoints = 1 + 2 * (int)floor(dfSegmentLength2 / dfMaxLength / 2);
                double dfStep = (alpha2 - alpha1) / (nIntermediatePoints + 1);
                for(int j=1;j<=nIntermediatePoints;j++)
                {
                    double alpha = alpha1 + dfStep * j;
                    double x = cx + R * cos(alpha), y = cy + R * sin(alpha);
                    aoRawPoint.push_back(OGRRawPoint(x,y));
                    if( padfZ )
                    {
                        double z = padfZ[i+1] + (padfZ[i+2] - padfZ[i+1]) * (alpha - alpha1) / (alpha2 - alpha1);
                        adfZ.push_back(z);
                    }
                }
            }
        }
        else
        {
            /* It is a straight line */
            double dfSegmentLength1 = sqrt((x1-x0)*(x1-x0)+(y1-y0)*(y1-y0));
            double dfSegmentLength2 = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
            if( dfSegmentLength1 > dfMaxLength || dfSegmentLength2 > dfMaxLength )
            {
                int nIntermediatePoints = 1 + 2 * (int)ceil(dfSegmentLength1 / dfMaxLength / 2);
                for(int j=1;j<=nIntermediatePoints;j++)
                {
                    aoRawPoint.push_back(OGRRawPoint(
                            x0 + j * (x1-x0) / (nIntermediatePoints + 1),
                            y0 + j * (y1-y0) / (nIntermediatePoints + 1)));
                    if( padfZ )
                        adfZ.push_back(padfZ[i] + j * (padfZ[i+1]-padfZ[i]) / (nIntermediatePoints + 1));
                }
            }

            aoRawPoint.push_back(OGRRawPoint(x1,y1));
            if( padfZ )
                adfZ.push_back(padfZ[i+1]);

            if( dfSegmentLength1 > dfMaxLength || dfSegmentLength2 > dfMaxLength )
            {
                int nIntermediatePoints = 1 + 2 * (int)ceil(dfSegmentLength2 / dfMaxLength / 2);
                for(int j=1;j<=nIntermediatePoints;j++)
                {
                    aoRawPoint.push_back(OGRRawPoint(
                            x1 + j * (x2-x1) / (nIntermediatePoints + 1),
                            y1 + j * (y2-y1) / (nIntermediatePoints + 1)));
                    if( padfZ )
                        adfZ.push_back(padfZ[i+1] + j * (padfZ[i+2]-padfZ[i+1]) / (nIntermediatePoints + 1));
                }
            }
        }
    }
    aoRawPoint.push_back(paoPoints[nPointCount-1]);
    if( padfZ )
        adfZ.push_back(padfZ[nPointCount-1]);

    CPLAssert(aoRawPoint.size() == 0 || (aoRawPoint.size() >= 3 && (aoRawPoint.size() % 2) == 1));
    if( padfZ )
        CPLAssert(adfZ.size() == aoRawPoint.size());

    /* Is there actually something to modify ? */
    if( nPointCount < (int)aoRawPoint.size() )
    {
        nPointCount = (int)aoRawPoint.size();
        paoPoints = (OGRRawPoint *)
                OGRRealloc(paoPoints, sizeof(OGRRawPoint) * nPointCount);
        memcpy(paoPoints, aoRawPoint.data(), sizeof(OGRRawPoint) * nPointCount);
        if( padfZ )
        {
            padfZ = (double*) OGRRealloc(padfZ, sizeof(double) * aoRawPoint.size());
            memcpy(padfZ, adfZ.data(), sizeof(double) * nPointCount);
        }
    }
}

/************************************************************************/
/*                               Value()                                */
/*                                                                      */
/*      Get an interpolated point at some distance along the curve.     */
/************************************************************************/

void OGRCircularString::Value( double dfDistance, OGRPoint * poPoint ) const

{
    double      dfLength = 0;
    int         i;

    if( dfDistance < 0 )
    {
        StartPoint( poPoint );
        return;
    }
    
    for(i=0;i<nPointCount-2;i+=2)
    {
        double x0 = paoPoints[i].x, y0 = paoPoints[i].y,
               x1 = paoPoints[i+1].x, y1 = paoPoints[i+1].y,
               x2 = paoPoints[i+2].x, y2 = paoPoints[i+2].y;
        double R, cx, cy, alpha0, alpha1, alpha2;

        /* We have strong constraints on the number of intermediate points */
        /* we can add */

        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                            R, cx, cy, alpha0, alpha1, alpha2) )
        {
            /* It is an arc circle */
            double dfSegLength = fabs(alpha2 - alpha0) * R;
            if (dfSegLength > 0)
            {
                if( (dfLength <= dfDistance) && ((dfLength + dfSegLength) >= 
                                                dfDistance) )
                {
                    double      dfRatio;

                    dfRatio = (dfDistance - dfLength) / dfSegLength;
                    
                    double alpha = alpha0 * (1 - dfRatio) + alpha2 * dfRatio;
                    double x = cx + R * cos(alpha), y = cy + R * sin(alpha);

                    poPoint->setX( x );
                    poPoint->setY( y );

                    if( getCoordinateDimension() == 3 )
                        poPoint->setZ( padfZ[i] * (1 - dfRatio)
                                    + padfZ[i+2] * dfRatio );
                    
                    return;
                }

                dfLength += dfSegLength;
            }
        }
        else
        {
            /* It is a straight line */
            double dfSegLength = sqrt((x2-x0)*(x2-x0)+(y2-y0)*(y2-y0));
            if (dfSegLength > 0)
            {
                if( (dfLength <= dfDistance) && ((dfLength + dfSegLength) >= 
                                                dfDistance) )
                {
                    double      dfRatio;

                    dfRatio = (dfDistance - dfLength) / dfSegLength;

                    poPoint->setX( paoPoints[i].x * (1 - dfRatio)
                                + paoPoints[i+2].x * dfRatio );
                    poPoint->setY( paoPoints[i].y * (1 - dfRatio)
                                + paoPoints[i+2].y * dfRatio );

                    if( getCoordinateDimension() == 3 )
                        poPoint->setZ( padfZ[i] * (1 - dfRatio)
                                    + padfZ[i+2] * dfRatio );
                    
                    return;
                }

                dfLength += dfSegLength;
            }
        }
    }

    EndPoint( poPoint );
}

/************************************************************************/
/*                          CurveToLine()                               */
/************************************************************************/

OGRLineString* OGRCircularString::CurveToLine(double dfMaxAngleStepSizeDegrees,
                                              const char* const* papszOptions) const
{
    OGRLineString* poLine = new OGRLineString();
    poLine->assignSpatialReference(getSpatialReference());

    int bHasZ = (getCoordinateDimension() == 3);
    int i;
    for(i=0;i<nPointCount-2;i+=2)
    {
        OGRLineString* poArc = OGRGeometryFactory::curveToLineString(
                paoPoints[i].x, paoPoints[i].y, padfZ ? padfZ[i] : 0.0,
                paoPoints[i+1].x, paoPoints[i+1].y, padfZ ? padfZ[i+1] : 0.0,
                paoPoints[i+2].x, paoPoints[i+2].y, padfZ ? padfZ[i+2] : 0.0,
                bHasZ,
                dfMaxAngleStepSizeDegrees,
                papszOptions);
        poLine->addSubLineString(poArc, (i == 0) ? 0 : 1);
        delete poArc;
    }
    return poLine;
}

/************************************************************************/
/*                        IsValidFast()                                 */
/************************************************************************/

OGRBoolean OGRCircularString::IsValidFast(  ) const

{
    if (nPointCount == 1 || nPointCount == 2 ||
        (nPointCount >= 3 && (nPointCount % 2) == 0))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Bad number of points in circular string : %d", nPointCount);
        return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                            IsValid()                                 */
/************************************************************************/

OGRBoolean OGRCircularString::IsValid(  ) const

{
    return IsValidFast() && OGRGeometry::IsValid();
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRCircularString::hasCurveGeometry(CPL_UNUSED int bLookForNonLinear) const
{
    return TRUE;
}

/************************************************************************/
/*                         getLinearGeometry()                        */
/************************************************************************/

OGRGeometry* OGRCircularString::getLinearGeometry(double dfMaxAngleStepSizeDegrees,
                                                    const char* const* papszOptions) const
{
    return CurveToLine(dfMaxAngleStepSizeDegrees, papszOptions);
}

/************************************************************************/
/*                     GetCasterToLineString()                          */
/************************************************************************/

OGRCurveCasterToLineString OGRCircularString::GetCasterToLineString() const {
    return (OGRCurveCasterToLineString) OGRGeometry::CastToError;
}

/************************************************************************/
/*                        GetCasterToLinearRing()                       */
/************************************************************************/

OGRCurveCasterToLinearRing OGRCircularString::GetCasterToLinearRing() const {
    return (OGRCurveCasterToLinearRing) OGRGeometry::CastToError;
}

/************************************************************************/
/*                            IsFullCircle()                            */
/************************************************************************/

int OGRCircularString::IsFullCircle( double& cx, double& cy, double& square_R ) const
{
    if( getNumPoints() == 3 && get_IsClosed() )
    {
        double x0 = getX(0);
        double y0 = getY(0);
        double x1 = getX(1);
        double y1 = getY(1);
        cx = (x0 + x1) / 2;
        cy = (y0 + y1) / 2;
        square_R = (x1-cx)*(x1-cx)+(y1-cy)*(y1-cy);
        return TRUE;
    }
    /* Full circle defined by 2 arcs ? */
    else if( getNumPoints() == 5 && get_IsClosed() )
    {
        double R_1, cx_1, cy_1, alpha0_1, alpha1_1, alpha2_1;
        double R_2, cx_2, cy_2, alpha0_2, alpha1_2, alpha2_2;
        if( OGRGeometryFactory::GetCurveParmeters(
                getX(0), getY(0),
                getX(1), getY(1),
                getX(2), getY(2),
                R_1, cx_1, cy_1, alpha0_1, alpha1_1, alpha2_1) &&
            OGRGeometryFactory::GetCurveParmeters(
                getX(2), getY(2),
                getX(3), getY(3),
                getX(4), getY(4),
                R_2, cx_2, cy_2, alpha0_2, alpha1_2, alpha2_2) &&
            fabs(R_1-R_2) < 1e-10 &&
            fabs(cx_1-cx_2) < 1e-10 &&
            fabs(cy_1-cy_2) < 1e-10 &&
            (alpha2_1 - alpha0_1) * (alpha2_2 - alpha0_2) > 0 )
        {
            cx = cx_1;
            cy = cy_1;
            square_R = R_1 * R_1;
            return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                       get_AreaOfCurveSegments()                      */
/************************************************************************/

double OGRCircularString::get_AreaOfCurveSegments() const
{
    double dfArea = 0;
    for( int i=0; i < getNumPoints() - 2; i += 2 )
    {
        double x0 = getX(i), y0 = getY(i),
               x1 = getX(i+1), y1 = getY(i+1),
               x2 = getX(i+2), y2 = getY(i+2);
        double R, cx, cy, alpha0, alpha1, alpha2;
        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                            R, cx, cy, alpha0, alpha1, alpha2))
        {
            double delta_alpha01 = alpha1 - alpha0; /* should be <= PI in absolute value */
            double delta_alpha12 = alpha2 - alpha1; /* same */
            /* This is my maths, but wikipedia confirms it... */
            /* Cf http://en.wikipedia.org/wiki/Circular_segment */
            dfArea += 0.5 * R * R * fabs( delta_alpha01 - sin(delta_alpha01) +
                                          delta_alpha12 - sin(delta_alpha12) );
        }
    }
    return dfArea;
}

/************************************************************************/
/*                           get_Area()                                 */
/************************************************************************/

double OGRCircularString::get_Area() const
{
    double cx, cy, square_R;

    if( IsEmpty() || !get_IsClosed() )
        return 0;

    if( IsFullCircle(cx, cy, square_R) )
    {
        return M_PI * square_R;
    }

    /* Optimization for convex rings */
    if( IsConvex() )
    {
        /* Compute area of shape without the circular segments */
        double dfArea = get_LinearArea();

        /* Add the area of the spherical segments */
        dfArea += get_AreaOfCurveSegments();

        return dfArea;
    }
    else
    {
        OGRLineString* poLS = CurveToLine();
        double dfArea = poLS->get_Area();
        delete poLS;

        return dfArea;
    }
}

/************************************************************************/
/*                           ContainsPoint()                            */
/************************************************************************/

int OGRCircularString::ContainsPoint( const OGRPoint* p ) const
{
    double cx, cy, square_R;
    if( IsFullCircle(cx, cy, square_R) )
    {
        double square_dist = (p->getX()- cx)*(p->getX()- cx)+
                             (p->getY()- cy)*(p->getY()- cy);
        return square_dist <= square_R;
    }
    return -1;
}
