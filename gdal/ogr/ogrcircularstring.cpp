/******************************************************************************
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

#include "cpl_port.h"
#include "ogr_geometry.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <vector>

#include "cpl_error.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

static inline double dist(double x0, double y0, double x1, double y1)
{
    return std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
}

/************************************************************************/
/*                         OGRCircularString()                          */
/************************************************************************/

/**
 * \brief Create an empty circular string.
 */

OGRCircularString::OGRCircularString() {}

/************************************************************************/
/*              OGRCircularString( const OGRCircularString& )           */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCircularString::OGRCircularString( const OGRCircularString& other ) :
    OGRSimpleCurve( other )
{}

/************************************************************************/
/*                        ~OGRCircularString()                          */
/************************************************************************/

OGRCircularString::~OGRCircularString() {}

/************************************************************************/
/*                  operator=( const OGRCircularString& )               */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCircularString&
OGRCircularString::operator=( const OGRCircularString& other )
{
    if( this != &other)
    {
        OGRSimpleCurve::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRCircularString::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbCircularStringZM;
    else if( flags & OGR_G_MEASURED )
        return wkbCircularStringM;
    else if( flags & OGR_G_3D )
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

OGRErr OGRCircularString::importFromWkb( const unsigned char * pabyData,
                                         int nSize,
                                         OGRwkbVariant eWkbVariant,
                                         int& nBytesConsumedOut )

{
    OGRErr eErr = OGRSimpleCurve::importFromWkb(pabyData, nSize,
                                                eWkbVariant,
                                                nBytesConsumedOut);
    if( eErr == OGRERR_NONE )
    {
        if( !IsValidFast() )
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

OGRErr OGRCircularString::exportToWkb( OGRwkbByteOrder eByteOrder,
                                       unsigned char * pabyData,
                                       OGRwkbVariant eWkbVariant ) const

{
    if( !IsValidFast() )
    {
        return OGRERR_FAILURE;
    }

    // Does not make sense for new geometries, so patch it.
    if( eWkbVariant == wkbVariantOldOgc )
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
    const OGRErr eErr = OGRSimpleCurve::importFromWkt(ppszInput);
    if( eErr == OGRERR_NONE )
    {
        if( !IsValidFast() )
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

OGRErr
OGRCircularString::exportToWkt( char ** ppszDstText,
                                CPL_UNUSED OGRwkbVariant eWkbVariant ) const

{
    if( !IsValidFast() )
    {
        return OGRERR_FAILURE;
    }

    return OGRSimpleCurve::exportToWkt(ppszDstText, wkbVariantIso);
}

/************************************************************************/
/*                             get_Length()                             */
/*                                                                      */
/*      For now we return a simple euclidean 2D distance.               */
/************************************************************************/

double OGRCircularString::get_Length() const

{
    double dfLength = 0.0;
    for( int i = 0; i < nPointCount-2; i += 2 )
    {
        const double x0 = paoPoints[i].x;
        const double y0 = paoPoints[i].y;
        const double x1 = paoPoints[i+1].x;
        const double y1 = paoPoints[i+1].y;
        const double x2 = paoPoints[i+2].x;
        const double y2 = paoPoints[i+2].y;
        double R = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        double alpha0 = 0.0;
        double alpha1 = 0.0;
        double alpha2 = 0.0;
        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                                  R, cx, cy,
                                                  alpha0, alpha1, alpha2) )
        {
            dfLength += fabs(alpha2 - alpha0) * R;
        }
        else
        {
            dfLength += dist(x0, y0, x2, y2);
        }
    }
    return dfLength;
}

/************************************************************************/
/*                       ExtendEnvelopeWithCircular()                   */
/************************************************************************/

void OGRCircularString::ExtendEnvelopeWithCircular(
    OGREnvelope * psEnvelope ) const
{
    if( !IsValidFast() || nPointCount == 0 )
        return;

    // Loop through circular portions and determine if they include some
    // extremities of the circle.
    for( int i = 0; i < nPointCount - 2; i += 2 )
    {
        double x0 = paoPoints[i].x;
        const double y0 = paoPoints[i].y;
        const double x1 = paoPoints[i+1].x;
        const double y1 = paoPoints[i+1].y;
        const double x2 = paoPoints[i+2].x;
        const double y2 = paoPoints[i+2].y;
        double R = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        double alpha0 = 0.0;
        double alpha1 = 0.0;
        double alpha2 = 0.0;
        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                                  R, cx, cy,
                                                  alpha0, alpha1, alpha2))
        {
            int quadrantStart =
                static_cast<int>(std::floor(alpha0 / (M_PI / 2)));
            int quadrantEnd = static_cast<int>(std::floor(alpha2 / (M_PI / 2)));
            if( quadrantStart > quadrantEnd )
            {
                std::swap(quadrantStart, quadrantEnd);
            }
            // Transition trough quadrants in counter-clock wise direction.
            for( int j = quadrantStart + 1; j <= quadrantEnd; ++j )
            {
                switch( (j + 8) % 4 )
                {
                    case 0:
                        psEnvelope->MaxX = std::max(psEnvelope->MaxX, cx + R);
                        break;
                    case 1:
                        psEnvelope->MaxY = std::max(psEnvelope->MaxY, cy + R);
                        break;
                    case 2:
                        psEnvelope->MinX = std::min(psEnvelope->MinX, cx - R);
                        break;
                    case 3:
                        psEnvelope->MinY = std::min(psEnvelope->MaxY, cy - R);
                        break;
                    default:
                        CPLAssert(false);
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

    // So as to make sure that the same line followed in both directions
    // result in the same segmentized line.
    if( paoPoints[0].x < paoPoints[nPointCount - 1].x ||
        (paoPoints[0].x == paoPoints[nPointCount - 1].x &&
         paoPoints[0].y < paoPoints[nPointCount - 1].y) )
    {
        reversePoints();
        segmentize(dfMaxLength);
        reversePoints();
    }

    std::vector<OGRRawPoint> aoRawPoint;
    std::vector<double> adfZ;
    for( int i = 0; i < nPointCount - 2; i += 2 )
    {
        const double x0 = paoPoints[i].x;
        const double y0 = paoPoints[i].y;
        const double x1 = paoPoints[i+1].x;
        const double y1 = paoPoints[i+1].y;
        const double x2 = paoPoints[i+2].x;
        const double y2 = paoPoints[i+2].y;
        double R = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        double alpha0 = 0.0;
        double alpha1 = 0.0;
        double alpha2 = 0.0;

        aoRawPoint.push_back(OGRRawPoint(x0, y0));
        if( padfZ )
            adfZ.push_back(padfZ[i]);

        // We have strong constraints on the number of intermediate points
        // we can add.

        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                                  R, cx, cy,
                                                  alpha0, alpha1, alpha2) )
        {
            // It is an arc circle.
            const double dfSegmentLength1 = fabs(alpha1 - alpha0) * R;
            const double dfSegmentLength2 = fabs(alpha2 - alpha1) * R;
            if( dfSegmentLength1 > dfMaxLength ||
                dfSegmentLength2 > dfMaxLength )
            {
                const int nIntermediatePoints =
                    1 +
                    2 * static_cast<int>(std::floor(dfSegmentLength1
                                                    / dfMaxLength / 2.0));
                const double dfStep =
                    (alpha1 - alpha0) / (nIntermediatePoints + 1);
                for( int j = 1; j <= nIntermediatePoints; ++j )
                {
                    double alpha = alpha0 + dfStep * j;
                    const double x = cx + R * cos(alpha);
                    const double y = cy + R * sin(alpha);
                    aoRawPoint.push_back(OGRRawPoint(x, y));
                    if( padfZ )
                    {
                        const double z =
                            padfZ[i] +
                            (padfZ[i+1] - padfZ[i]) * (alpha - alpha0) /
                            (alpha1 - alpha0);
                        adfZ.push_back(z);
                    }
                }
            }
            aoRawPoint.push_back(OGRRawPoint(x1, y1));
            if( padfZ )
                adfZ.push_back(padfZ[i+1]);

            if( dfSegmentLength1 > dfMaxLength ||
                dfSegmentLength2 > dfMaxLength )
            {
                int nIntermediatePoints =
                    1 +
                    2 * static_cast<int>(std::floor(dfSegmentLength2
                                                    / dfMaxLength / 2.0));
                const double dfStep =
                    (alpha2 - alpha1) / (nIntermediatePoints + 1);
                for( int j = 1; j <= nIntermediatePoints; ++j )
                {
                    const double alpha = alpha1 + dfStep * j;
                    const double x = cx + R * cos(alpha);
                    const double y = cy + R * sin(alpha);
                    aoRawPoint.push_back(OGRRawPoint(x, y));
                    if( padfZ )
                    {
                        const double z =
                            padfZ[i+1] +
                            (padfZ[i+2] - padfZ[i+1]) *
                            (alpha - alpha1) / (alpha2 - alpha1);
                        adfZ.push_back(z);
                    }
                }
            }
        }
        else
        {
            // It is a straight line.
            const double dfSegmentLength1 = dist(x0, y0, x1, y1);
            const double dfSegmentLength2 = dist(x1, y1, x2, y2);
            if( dfSegmentLength1 > dfMaxLength ||
                dfSegmentLength2 > dfMaxLength )
            {
                int nIntermediatePoints =
                    1 + 2 * static_cast<int>(ceil(dfSegmentLength1 /
                                                  dfMaxLength / 2.0));
                for( int j = 1; j <= nIntermediatePoints; ++j )
                {
                    aoRawPoint.push_back(OGRRawPoint(
                            x0 + j * (x1-x0) / (nIntermediatePoints + 1),
                            y0 + j * (y1-y0) / (nIntermediatePoints + 1)));
                    if( padfZ )
                        adfZ.push_back(padfZ[i] + j * (padfZ[i+1]-padfZ[i]) /
                                       (nIntermediatePoints + 1));
                }
            }

            aoRawPoint.push_back(OGRRawPoint(x1, y1));
            if( padfZ )
                adfZ.push_back(padfZ[i+1]);

            if( dfSegmentLength1 > dfMaxLength ||
                dfSegmentLength2 > dfMaxLength )
            {
                const int nIntermediatePoints =
                    1 + 2 * static_cast<int>(ceil(dfSegmentLength2 /
                                                  dfMaxLength / 2.0));
                for( int j = 1; j <= nIntermediatePoints; ++j )
                {
                    aoRawPoint.push_back(OGRRawPoint(
                            x1 + j * (x2-x1) / (nIntermediatePoints + 1),
                            y1 + j * (y2-y1) / (nIntermediatePoints + 1)));
                    if( padfZ )
                        adfZ.push_back(padfZ[i+1] + j * (padfZ[i+2]-padfZ[i+1])
                                       / (nIntermediatePoints + 1));
                }
            }
        }
    }
    aoRawPoint.push_back(paoPoints[nPointCount-1]);
    if( padfZ )
        adfZ.push_back(padfZ[nPointCount-1]);

    CPLAssert(aoRawPoint.empty() ||
              (aoRawPoint.size() >= 3 && (aoRawPoint.size() % 2) == 1));
    if( padfZ )
    {
        CPLAssert(adfZ.size() == aoRawPoint.size());
    }

    // Is there actually something to modify?
    if( nPointCount < static_cast<int>(aoRawPoint.size()) )
    {
        nPointCount = static_cast<int>(aoRawPoint.size());
        paoPoints = static_cast<OGRRawPoint *>(
                CPLRealloc(paoPoints, sizeof(OGRRawPoint) * nPointCount));
        memcpy(paoPoints, &aoRawPoint[0], sizeof(OGRRawPoint) * nPointCount);
        if( padfZ )
        {
            padfZ = static_cast<double *>(
                CPLRealloc(padfZ, sizeof(double) * aoRawPoint.size()));
            memcpy(padfZ, &adfZ[0], sizeof(double) * nPointCount);
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
    if( dfDistance < 0 )
    {
        StartPoint( poPoint );
        return;
    }

    double dfLength = 0;

    for( int i = 0; i < nPointCount - 2; i += 2 )
    {
        const double x0 = paoPoints[i].x;
        const double y0 = paoPoints[i].y;
        const double x1 = paoPoints[i+1].x;
        const double y1 = paoPoints[i+1].y;
        const double x2 = paoPoints[i+2].x;
        const double y2 = paoPoints[i+2].y;
        double R = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        double alpha0 = 0.0;
        double alpha1 = 0.0;
        double alpha2 = 0.0;

        // We have strong constraints on the number of intermediate points
        // we can add.

        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                                  R, cx, cy,
                                                  alpha0, alpha1, alpha2) )
        {
            // It is an arc circle.
            const double dfSegLength = fabs(alpha2 - alpha0) * R;
            if( dfSegLength > 0 )
            {
                if( (dfLength <= dfDistance) && ((dfLength + dfSegLength) >=
                                                dfDistance) )
                {
                    const double dfRatio =
                        (dfDistance - dfLength) / dfSegLength;

                    const double alpha =
                        alpha0 * (1 - dfRatio) + alpha2 * dfRatio;
                    const double x = cx + R * cos(alpha);
                    const double y = cy + R * sin(alpha);

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
            // It is a straight line.
            const double dfSegLength = dist(x0, y0, x2, y2);
            if( dfSegLength > 0 )
            {
                if( (dfLength <= dfDistance) && ((dfLength + dfSegLength) >=
                                                dfDistance) )
                {
                    const double dfRatio =
                        (dfDistance - dfLength) / dfSegLength;

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

OGRLineString* OGRCircularString::CurveToLine(
    double dfMaxAngleStepSizeDegrees, const char* const* papszOptions ) const
{
    OGRLineString* poLine = new OGRLineString();
    poLine->assignSpatialReference(getSpatialReference());

    const bool bHasZ = getCoordinateDimension() == 3;
    for( int i = 0; i < nPointCount - 2; i += 2 )
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

OGRBoolean OGRCircularString::IsValidFast() const

{
    if( nPointCount == 1 || nPointCount == 2 ||
        (nPointCount >= 3 && (nPointCount % 2) == 0) )
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

OGRBoolean OGRCircularString::IsValid() const

{
    return IsValidFast() && OGRGeometry::IsValid();
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean
OGRCircularString::hasCurveGeometry( int /* bLookForNonLinear */ ) const
{
    return TRUE;
}

/************************************************************************/
/*                         getLinearGeometry()                        */
/************************************************************************/

OGRGeometry*
OGRCircularString::getLinearGeometry( double dfMaxAngleStepSizeDegrees,
                                      const char* const* papszOptions) const
{
    return CurveToLine(dfMaxAngleStepSizeDegrees, papszOptions);
}

//! @cond Doxygen_Suppress
/************************************************************************/
/*                     GetCasterToLineString()                          */
/************************************************************************/

static OGRLineString* CasterToLineString(OGRCurve* poGeom)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "%s found. Conversion impossible", poGeom->getGeometryName());
    delete poGeom;
    return NULL;
}

OGRCurveCasterToLineString OGRCircularString::GetCasterToLineString() const {
    return ::CasterToLineString;
}

/************************************************************************/
/*                        GetCasterToLinearRing()                       */
/************************************************************************/

static OGRLinearRing* CasterToLinearRing(OGRCurve* poGeom)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "%s found. Conversion impossible", poGeom->getGeometryName());
    delete poGeom;
    return NULL;
}

OGRCurveCasterToLinearRing OGRCircularString::GetCasterToLinearRing() const {
    return ::CasterToLinearRing;
}
//! @endcond

/************************************************************************/
/*                            IsFullCircle()                            */
/************************************************************************/

int OGRCircularString::IsFullCircle( double& cx, double& cy,
                                     double& square_R ) const
{
    if( getNumPoints() == 3 && get_IsClosed() )
    {
        const double x0 = getX(0);
        const double y0 = getY(0);
        const double x1 = getX(1);
        const double y1 = getY(1);
        cx = (x0 + x1) / 2;
        cy = (y0 + y1) / 2;
        square_R = (x1 - cx) * (x1 - cx) + (y1 - cy) * (y1 - cy);
        return TRUE;
    }
    // Full circle defined by 2 arcs?
    else if( getNumPoints() == 5 && get_IsClosed() )
    {
        double R_1 = 0.0;
        double cx_1 = 0.0;
        double cy_1 = 0.0;
        double alpha0_1 = 0.0;
        double alpha1_1 = 0.0;
        double alpha2_1 = 0.0;
        double R_2 = 0.0;
        double cx_2 = 0.0;
        double cy_2 = 0.0;
        double alpha0_2 = 0.0;
        double alpha1_2 = 0.0;
        double alpha2_2 = 0.0;
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

//! @cond Doxygen_Suppress
double OGRCircularString::get_AreaOfCurveSegments() const
{
    double dfArea = 0.0;
    for( int i = 0; i < getNumPoints() - 2; i += 2 )
    {
        const double x0 = getX(i);
        const double y0 = getY(i);
        const double x1 = getX(i+1);
        const double y1 = getY(i+1);
        const double x2 = getX(i+2);
        const double y2 = getY(i+2);
        double R = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        double alpha0 = 0.0;
        double alpha1 = 0.0;
        double alpha2 = 0.0;
        if( OGRGeometryFactory::GetCurveParmeters(x0, y0, x1, y1, x2, y2,
                                                  R, cx, cy,
                                                  alpha0, alpha1, alpha2))
        {
            // Should be <= PI in absolute value.
            const double delta_alpha01 = alpha1 - alpha0;
            const double delta_alpha12 = alpha2 - alpha1; // Same.
            // http://en.wikipedia.org/wiki/Circular_segment
            dfArea += 0.5 * R * R * fabs( delta_alpha01 - sin(delta_alpha01) +
                                          delta_alpha12 - sin(delta_alpha12) );
        }
    }
    return dfArea;
}
//! @endcond

/************************************************************************/
/*                           get_Area()                                 */
/************************************************************************/

double OGRCircularString::get_Area() const
{
    if( IsEmpty() || !get_IsClosed() )
        return 0;

    double cx = 0.0;
    double cy = 0.0;
    double square_R = 0.0;

    if( IsFullCircle(cx, cy, square_R) )
    {
        return M_PI * square_R;
    }

    // Optimization for convex rings.
    if( IsConvex() )
    {
        // Compute area of shape without the circular segments.
        double dfArea = get_LinearArea();

        // Add the area of the spherical segments.
        dfArea += get_AreaOfCurveSegments();

        return dfArea;
    }

    OGRLineString* poLS = CurveToLine();
    const double dfArea = poLS->get_Area();
    delete poLS;

    return dfArea;
}

/************************************************************************/
/*                           ContainsPoint()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
int OGRCircularString::ContainsPoint( const OGRPoint* p ) const
{
    double cx = 0.0;
    double cy = 0.0;
    double square_R = 0.0;
    if( IsFullCircle(cx, cy, square_R) )
    {
        const double square_dist =
            (p->getX() - cx) * (p->getX() - cx) +
            (p->getY() - cy) * (p->getY() - cy);
        return square_dist <= square_R;
    }
    return -1;
}
//! @endcond
