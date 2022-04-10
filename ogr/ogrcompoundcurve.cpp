/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCompoundCurve geometry class.
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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
#include <cstddef>

#include "cpl_error.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRCompoundCurve()                           */
/************************************************************************/

/**
 * \brief Create an empty compound curve.
 */

OGRCompoundCurve::OGRCompoundCurve() = default;

/************************************************************************/
/*             OGRCompoundCurve( const OGRCompoundCurve& )              */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCompoundCurve::OGRCompoundCurve( const OGRCompoundCurve& ) = default;

/************************************************************************/
/*                         ~OGRCompoundCurve()                          */
/************************************************************************/

OGRCompoundCurve::~OGRCompoundCurve() = default;

/************************************************************************/
/*                 operator=( const OGRCompoundCurve&)                  */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCompoundCurve& OGRCompoundCurve::operator=( const OGRCompoundCurve& other )
{
    if( this != &other)
    {
        OGRCurve::operator=( other );

        oCC = other.oCC;
    }
    return *this;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRCompoundCurve *OGRCompoundCurve::clone() const

{
    return new (std::nothrow) OGRCompoundCurve(*this);
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRCompoundCurve::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbCompoundCurveZM;
    else if( flags & OGR_G_MEASURED )
        return wkbCompoundCurveM;
    else if( flags & OGR_G_3D )
        return wkbCompoundCurveZ;
    else
        return wkbCompoundCurve;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRCompoundCurve::getGeometryName() const

{
    return "COMPOUNDCURVE";
}

/************************************************************************/
/*                              WkbSize()                               */
/************************************************************************/
size_t OGRCompoundCurve::WkbSize() const
{
    return oCC.WkbSize();
}

/************************************************************************/
/*                       addCurveDirectlyFromWkt()                      */
/************************************************************************/

OGRErr OGRCompoundCurve::addCurveDirectlyFromWkb( OGRGeometry* poSelf,
                                                  OGRCurve* poCurve )
{
    OGRCompoundCurve* poCC = poSelf->toCompoundCurve();
    return poCC->addCurveDirectlyInternal( poCurve, 1e-14, FALSE );
}

/************************************************************************/
/*                           importFromWkb()                            */
/************************************************************************/

OGRErr OGRCompoundCurve::importFromWkb( const unsigned char * pabyData,
                                        size_t nSize,
                                        OGRwkbVariant eWkbVariant,
                                        size_t& nBytesConsumedOut )
{
    OGRwkbByteOrder eByteOrder = wkbNDR;
    size_t nDataOffset = 0;
    // coverity[tainted_data]
    OGRErr eErr = oCC.importPreambleFromWkb(this, pabyData, nSize, nDataOffset,
                                             eByteOrder, 9, eWkbVariant);
    if( eErr != OGRERR_NONE )
        return eErr;

    eErr =  oCC.importBodyFromWkb(this, pabyData + nDataOffset, nSize,
                                 false,  // bAcceptCompoundCurve
                                 addCurveDirectlyFromWkb,
                                 eWkbVariant,
                                 nBytesConsumedOut);
    if( eErr == OGRERR_NONE )
        nBytesConsumedOut += nDataOffset;
    return eErr;
}

/************************************************************************/
/*                            exportToWkb()                             */
/************************************************************************/
OGRErr OGRCompoundCurve::exportToWkb( OGRwkbByteOrder eByteOrder,
                                      unsigned char * pabyData,
                                      OGRwkbVariant eWkbVariant ) const
{
    // Does not make sense for new geometries, so patch it.
    if( eWkbVariant == wkbVariantOldOgc )
        eWkbVariant = wkbVariantIso;
    return oCC.exportToWkb(this, eByteOrder, pabyData, eWkbVariant);
}

/************************************************************************/
/*                       addCurveDirectlyFromWkt()                      */
/************************************************************************/

OGRErr OGRCompoundCurve::addCurveDirectlyFromWkt( OGRGeometry* poSelf,
                                                  OGRCurve* poCurve )
{
    return poSelf->toCompoundCurve()->addCurveDirectly(poCurve);
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

OGRErr OGRCompoundCurve::importFromWkt( const char ** ppszInput )
{
    return importCurveCollectionFromWkt( ppszInput,
                                         FALSE, // bAllowEmptyComponent
                                         TRUE, // bAllowLineString
                                         TRUE, // bAllowCurve
                                         FALSE, // bAllowCompoundCurve
                                         addCurveDirectlyFromWkt );
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/
std::string OGRCompoundCurve::exportToWkt(const OGRWktOptions& opts,
                                          OGRErr *err) const
{
    return oCC.exportToWkt(this, opts, err);
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRCompoundCurve::empty()
{
    oCC.empty(this);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCompoundCurve::getEnvelope( OGREnvelope * psEnvelope ) const
{
    oCC.getEnvelope(psEnvelope);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCompoundCurve::getEnvelope( OGREnvelope3D * psEnvelope ) const
{
    oCC.getEnvelope(psEnvelope);
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRCompoundCurve::IsEmpty() const
{
    return oCC.IsEmpty();
}

/************************************************************************/
/*                             get_Length()                             */
/*                                                                      */
/*      For now we return a simple euclidean 2D distance.               */
/************************************************************************/

double OGRCompoundCurve::get_Length() const
{
    double dfLength = 0.0;
    for( int iGeom = 0; iGeom < oCC.nCurveCount; iGeom++ )
        dfLength += oCC.papoCurves[iGeom]->get_Length();
    return dfLength;
}

/************************************************************************/
/*                             StartPoint()                             */
/************************************************************************/

void OGRCompoundCurve::StartPoint(OGRPoint *p) const
{
    CPLAssert(oCC.nCurveCount > 0);
    oCC.papoCurves[0]->StartPoint(p);
}

/************************************************************************/
/*                              EndPoint()                              */
/************************************************************************/

void OGRCompoundCurve::EndPoint(OGRPoint *p) const
{
    CPLAssert(oCC.nCurveCount > 0);
    oCC.papoCurves[oCC.nCurveCount-1]->EndPoint(p);
}

/************************************************************************/
/*                               Value()                                */
/************************************************************************/

void OGRCompoundCurve::Value( double dfDistance, OGRPoint *poPoint ) const
{

    if( dfDistance < 0 )
    {
        StartPoint( poPoint );
        return;
    }

    double dfLength = 0.0;
    for( int iGeom = 0; iGeom < oCC.nCurveCount; iGeom++ )
    {
        const double dfSegLength = oCC.papoCurves[iGeom]->get_Length();
        if( dfSegLength > 0 )
        {
            if( (dfLength <= dfDistance) && ((dfLength + dfSegLength) >=
                                             dfDistance) )
            {
                oCC.papoCurves[iGeom]->Value(dfDistance - dfLength, poPoint);

                return;
            }

            dfLength += dfSegLength;
        }
    }

    EndPoint( poPoint );
}

/************************************************************************/
/*                         CurveToLineInternal()                        */
/************************************************************************/

OGRLineString *
OGRCompoundCurve::CurveToLineInternal( double dfMaxAngleStepSizeDegrees,
                                       const char* const* papszOptions,
                                       int bIsLinearRing ) const
{
    OGRLineString* const poLine = bIsLinearRing
        ? new OGRLinearRing()
        : new OGRLineString();
    poLine->assignSpatialReference(getSpatialReference());
    for( int iGeom = 0; iGeom < oCC.nCurveCount; iGeom++ )
    {
        OGRLineString* poSubLS =
            oCC.papoCurves[iGeom]->CurveToLine(dfMaxAngleStepSizeDegrees,
                                               papszOptions);
        poLine->addSubLineString(poSubLS, (iGeom == 0) ? 0 : 1);
        delete poSubLS;
    }
    return poLine;
}

/************************************************************************/
/*                          CurveToLine()                               */
/************************************************************************/

OGRLineString *
OGRCompoundCurve::CurveToLine( double dfMaxAngleStepSizeDegrees,
                               const char* const* papszOptions ) const
{
    return CurveToLineInternal(dfMaxAngleStepSizeDegrees, papszOptions, FALSE);
}

/************************************************************************/
/*                               Equals()                                */
/************************************************************************/

OGRBoolean OGRCompoundCurve::Equals( const OGRGeometry *poOther ) const
{
    if( poOther == this )
        return TRUE;

    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    return oCC.Equals(&(poOther->toCompoundCurve()->oCC));
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRCompoundCurve::setCoordinateDimension( int nNewDimension )
{
    oCC.setCoordinateDimension( this, nNewDimension );
}

void OGRCompoundCurve::set3D( OGRBoolean bIs3D )
{
    oCC.set3D(this, bIs3D);
}

void OGRCompoundCurve::setMeasured( OGRBoolean bIsMeasured )
{
    oCC.setMeasured(this, bIsMeasured);
}

/************************************************************************/
/*                       assignSpatialReference()                       */
/************************************************************************/

void OGRCompoundCurve::assignSpatialReference( OGRSpatialReference * poSR )
{
    oCC.assignSpatialReference(this, poSR);
}

/************************************************************************/
/*                          getNumCurves()                              */
/************************************************************************/

/**
 * \brief Return the number of curves.
 *
 * Note that the number of curves making this compound curve.
 *
 * Relates to the ISO SQL/MM ST_NumCurves() function.
 *
 * @return number of curves.
 */

int OGRCompoundCurve::getNumCurves() const
{
    return oCC.nCurveCount;
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

/**
 * \brief Fetch reference to indicated internal ring.
 *
 * Note that the returned curve pointer is to an internal data object of the
 * OGRCompoundCurve.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use the
 * OGRGeometry::clone() method to make a separate copy within the application.
 *
 * Relates to the ISO SQL/MM ST_CurveN() function.
 *
 * @param iRing curve index from 0 to getNumCurves() - 1.
 *
 * @return pointer to curve.  May be NULL.
 */

OGRCurve *OGRCompoundCurve::getCurve( int iRing )
{
    return oCC.getCurve(iRing);
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

/**
 * \brief Fetch reference to indicated internal ring.
 *
 * Note that the returned curve pointer is to an internal data object of the
 * OGRCompoundCurve.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use the
 * OGRGeometry::clone() method to make a separate copy within the application.
 *
 * Relates to the ISO SQL/MM ST_CurveN() function.
 *
 * @param iCurve curve index from 0 to getNumCurves() - 1.
 *
 * @return pointer to curve.  May be NULL.
 */

const OGRCurve *OGRCompoundCurve::getCurve( int iCurve ) const
{
    return oCC.getCurve(iCurve);
}

/************************************************************************/
/*                           stealCurve()                               */
/************************************************************************/

/**
 * \brief "Steal" reference to curve.
 *
 * @param iCurve curve index from 0 to getNumCurves() - 1.
 *
 * @return pointer to curve.  May be NULL.
 */

OGRCurve* OGRCompoundCurve::stealCurve( int iCurve )
{
    return oCC.stealCurve(iCurve);
}

/************************************************************************/
/*                            addCurve()                                */
/************************************************************************/

/**
 * \brief Add a curve to the container.
 *
 * The passed geometry is cloned to make an internal copy.
 *
 * There is no ISO SQL/MM analog to this method.
 *
 * This method is the same as the C function OGR_G_AddGeometry().
 *
 * @param poCurve geometry to add to the container.
 * @param dfToleranceEps relative tolerance when checking that the first point of a
 *                       segment matches then end point of the previous one.
 *                       Default value: 1e-14.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE in case of error
 * (for example if curves are not contiguous)
 */

OGRErr OGRCompoundCurve::addCurve( const OGRCurve* poCurve, double dfToleranceEps )
{
    OGRCurve* poClonedCurve = poCurve->clone();
    const OGRErr eErr = addCurveDirectly( poClonedCurve, dfToleranceEps );
    if( eErr != OGRERR_NONE )
        delete poClonedCurve;
    return eErr;
}

/************************************************************************/
/*                          addCurveDirectly()                          */
/************************************************************************/

/**
 * \brief Add a curve directly to the container.
 *
 * Ownership of the passed geometry is taken by the container rather than
 * cloning as addCurve() does.
 *
 * There is no ISO SQL/MM analog to this method.
 *
 * This method is the same as the C function OGR_G_AddGeometryDirectly().
 *
 * @param poCurve geometry to add to the container.
 * @param dfToleranceEps relative tolerance when checking that the first point of a
 *                       segment matches then end point of the previous one.
 *                       Default value: 1e-14.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE in case of error
 * (for example if curves are not contiguous)
 */
OGRErr OGRCompoundCurve::addCurveDirectly( OGRCurve* poCurve,
                                           double dfToleranceEps )
{
    return addCurveDirectlyInternal(poCurve, dfToleranceEps, TRUE );
}

OGRErr OGRCompoundCurve::addCurveDirectlyInternal( OGRCurve* poCurve,
                                                   double dfToleranceEps,
                                                   int bNeedRealloc )
{
    if( poCurve->getNumPoints() == 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid curve: not enough points");
        return OGRERR_FAILURE;
    }

    const OGRwkbGeometryType eCurveType =
        wkbFlatten(poCurve->getGeometryType());
    if( EQUAL(poCurve->getGeometryName(), "LINEARRING") )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Linearring not allowed.");
        return OGRERR_FAILURE;
    }
    else if( eCurveType == wkbCompoundCurve )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot add a compound curve inside a compound curve");
        return OGRERR_FAILURE;
    }

    if( oCC.nCurveCount > 0 )
    {
        if( oCC.papoCurves[oCC.nCurveCount-1]->IsEmpty() ||
            poCurve->IsEmpty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Non contiguous curves");
            return OGRERR_FAILURE;
        }

        OGRPoint oEnd;
        OGRPoint start;
        oCC.papoCurves[oCC.nCurveCount-1]->EndPoint(&oEnd);
        poCurve->StartPoint(&start);
        if( fabs(oEnd.getX() - start.getX()) > dfToleranceEps * fabs(start.getX()) ||
            fabs(oEnd.getY() - start.getY()) > dfToleranceEps * fabs(start.getY()) ||
            fabs(oEnd.getZ() - start.getZ()) > dfToleranceEps * fabs(start.getZ()) )
        {
            poCurve->EndPoint(&start);
            if( fabs(oEnd.getX() - start.getX()) > dfToleranceEps * fabs(start.getX()) ||
                fabs(oEnd.getY() - start.getY()) > dfToleranceEps * fabs(start.getY()) ||
                fabs(oEnd.getZ() - start.getZ()) > dfToleranceEps * fabs(start.getZ()) )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Non contiguous curves");
                return OGRERR_FAILURE;
            }

            CPLDebug("GML", "reversing curve");
            poCurve->toSimpleCurve()->reversePoints();
        }
        // Patch so that it matches exactly.
        poCurve->toSimpleCurve()->setPoint(0, &oEnd);
    }

    return oCC.addCurveDirectly(this, poCurve, bNeedRealloc);
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRCompoundCurve::transform( OGRCoordinateTransformation *poCT )
{
    return oCC.transform(this, poCT);
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRCompoundCurve::flattenTo2D()
{
    oCC.flattenTo2D(this);
}

/************************************************************************/
/*                              segmentize()                            */
/************************************************************************/

void OGRCompoundCurve::segmentize( double dfMaxLength )
{
    oCC.segmentize(dfMaxLength);
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRCompoundCurve::swapXY()
{
    oCC.swapXY();
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRCompoundCurve::hasCurveGeometry( int bLookForNonLinear ) const
{
    if( bLookForNonLinear )
    {
        return oCC.hasCurveGeometry(bLookForNonLinear);
    }

    return TRUE;
}

/************************************************************************/
/*                         getLinearGeometry()                        */
/************************************************************************/

OGRGeometry *
OGRCompoundCurve::getLinearGeometry( double dfMaxAngleStepSizeDegrees,
                                     const char* const* papszOptions ) const
{
    return CurveToLine(dfMaxAngleStepSizeDegrees, papszOptions);
}

/************************************************************************/
/*                           getNumPoints()                             */
/************************************************************************/

int OGRCompoundCurve::getNumPoints() const
{
    int nPoints = 0;
    for( int i = 0; i < oCC.nCurveCount; i++ )
    {
        nPoints += oCC.papoCurves[i]->getNumPoints();
        if( i != 0 )
            nPoints--;
    }
    return nPoints;
}

/************************************************************************/
/*                      OGRCompoundCurvePointIterator                   */
/************************************************************************/

class OGRCompoundCurvePointIterator final: public OGRPointIterator
{
        CPL_DISALLOW_COPY_ASSIGN(OGRCompoundCurvePointIterator)

        const OGRCompoundCurve *poCC = nullptr;
        int                     iCurCurve = 0;
        OGRPointIterator       *poCurveIter = nullptr;

    public:
        explicit OGRCompoundCurvePointIterator( const OGRCompoundCurve* poCCIn ) :
            poCC(poCCIn) {}
        ~OGRCompoundCurvePointIterator() override { delete poCurveIter; }

        OGRBoolean getNextPoint( OGRPoint* p ) override;
};

/************************************************************************/
/*                            getNextPoint()                            */
/************************************************************************/

OGRBoolean OGRCompoundCurvePointIterator::getNextPoint( OGRPoint* p )
{
    if( iCurCurve == poCC->getNumCurves() )
        return FALSE;
    if( poCurveIter == nullptr )
        poCurveIter = poCC->getCurve(0)->getPointIterator();
    if( !poCurveIter->getNextPoint(p) )
    {
        iCurCurve++;
        if( iCurCurve == poCC->getNumCurves() )
            return FALSE;
        delete poCurveIter;
        poCurveIter = poCC->getCurve(iCurCurve)->getPointIterator();
        // Skip first point.
        return poCurveIter->getNextPoint(p) &&
               poCurveIter->getNextPoint(p);
    }
    return TRUE;
}

/************************************************************************/
/*                         getPointIterator()                           */
/************************************************************************/

OGRPointIterator* OGRCompoundCurve::getPointIterator() const
{
    return new OGRCompoundCurvePointIterator(this);
}

/************************************************************************/
/*                         CastToLineString()                        */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRLineString* OGRCompoundCurve::CastToLineString( OGRCompoundCurve* poCC )
{
    for( int i = 0; i < poCC->oCC.nCurveCount; i++ )
    {
        poCC->oCC.papoCurves[i] =
            OGRCurve::CastToLineString(poCC->oCC.papoCurves[i]);
        if( poCC->oCC.papoCurves[i] == nullptr )
        {
            delete poCC;
            return nullptr;
        }
    }

    if( poCC->oCC.nCurveCount == 1 )
    {
        OGRLineString* poLS = poCC->oCC.papoCurves[0]->toLineString();
        poLS->assignSpatialReference(poCC->getSpatialReference());
        poCC->oCC.papoCurves[0] = nullptr;
        delete poCC;
        return poLS;
    }

    OGRLineString* poLS = poCC->CurveToLineInternal(0, nullptr, FALSE);
    delete poCC;
    return poLS;
}

/************************************************************************/
/*                           CastToLinearRing()                         */
/************************************************************************/

/**
 * \brief Cast to linear ring.
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure)
 *
 * @param poCC the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRLinearRing* OGRCompoundCurve::CastToLinearRing( OGRCompoundCurve* poCC )
{
    for( int i = 0; i < poCC->oCC.nCurveCount; i++ )
    {
        poCC->oCC.papoCurves[i] =
            OGRCurve::CastToLineString(poCC->oCC.papoCurves[i]);
        if( poCC->oCC.papoCurves[i] == nullptr )
        {
            delete poCC;
            return nullptr;
        }
    }

    if( poCC->oCC.nCurveCount == 1 )
    {
        OGRLinearRing* poLR =
            OGRCurve::CastToLinearRing( poCC->oCC.papoCurves[0] );
        if( poLR != nullptr )
        {
            poLR->assignSpatialReference(poCC->getSpatialReference());
        }
        poCC->oCC.papoCurves[0] = nullptr;
        delete poCC;
        return poLR;
    }

    OGRLinearRing* poLR =
        poCC->CurveToLineInternal(0, nullptr, TRUE)->toLinearRing();
    delete poCC;
    return poLR;
}

/************************************************************************/
/*                     GetCasterToLineString()                          */
/************************************************************************/

OGRLineString* OGRCompoundCurve::CasterToLineString( OGRCurve* poCurve )
{
    OGRCompoundCurve* poCC = poCurve->toCompoundCurve();
    return OGRCompoundCurve::CastToLineString(poCC);
}

OGRCurveCasterToLineString OGRCompoundCurve::GetCasterToLineString() const {
    return OGRCompoundCurve::CasterToLineString;
}

/************************************************************************/
/*                        GetCasterToLinearRing()                       */
/************************************************************************/

OGRLinearRing* OGRCompoundCurve::CasterToLinearRing( OGRCurve* poCurve )
{
    OGRCompoundCurve* poCC = poCurve->toCompoundCurve();
    return OGRCompoundCurve::CastToLinearRing(poCC);
}

OGRCurveCasterToLinearRing OGRCompoundCurve::GetCasterToLinearRing() const {
    return OGRCompoundCurve::CasterToLinearRing;
}
//! @endcond

/************************************************************************/
/*                           get_Area()                                 */
/************************************************************************/

double OGRCompoundCurve::get_Area() const
{
    if( IsEmpty() || !get_IsClosed() )
        return 0;

    // Optimization for convex rings.
    if( IsConvex() )
    {
        // Compute area of shape without the circular segments.
        OGRPointIterator* poIter = getPointIterator();
        OGRLineString oLS;
        oLS.setNumPoints( getNumPoints() );
        OGRPoint p;
        for( int i = 0; poIter->getNextPoint(&p); i++ )
        {
            oLS.setPoint( i, p.getX(), p.getY() );
        }
        double dfArea = oLS.get_Area();
        delete poIter;

        // Add the area of the spherical segments.
        dfArea += get_AreaOfCurveSegments();

        return dfArea;
    }

    OGRLineString* poLS = CurveToLine();
    double dfArea = poLS->get_Area();
    delete poLS;

    return dfArea;
}

/************************************************************************/
/*                       get_AreaOfCurveSegments()                      */
/************************************************************************/

/** Return area of curve segments
 * @return area.
 */
double OGRCompoundCurve::get_AreaOfCurveSegments() const
{
    double dfArea = 0;
    for( int i = 0; i < getNumCurves(); i++ )
    {
        const OGRCurve* poPart = getCurve(i);
        dfArea += poPart->get_AreaOfCurveSegments();
    }
    return dfArea;
}
