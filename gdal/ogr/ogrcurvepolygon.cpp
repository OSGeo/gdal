/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCurvePolygon geometry class.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
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

#include <cstddef>

#include "cpl_error.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geos.h"
#include "ogr_sfcgal.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRCurvePolygon()                         */
/************************************************************************/

/**
 * \brief Create an empty curve polygon.
 */

OGRCurvePolygon::OGRCurvePolygon() = default;

/************************************************************************/
/*               OGRCurvePolygon( const OGRCurvePolygon& )              */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCurvePolygon::OGRCurvePolygon( const OGRCurvePolygon& ) = default;

/************************************************************************/
/*                           ~OGRCurvePolygon()                         */
/************************************************************************/

OGRCurvePolygon::~OGRCurvePolygon() = default;

/************************************************************************/
/*                 operator=( const OGRCurvePolygon&)                  */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCurvePolygon& OGRCurvePolygon::operator=( const OGRCurvePolygon& other )
{
    if( this != &other )
    {
        OGRSurface::operator=( other );

        oCC = other.oCC;
    }
    return *this;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRCurvePolygon *OGRCurvePolygon::clone() const

{
    return new (std::nothrow) OGRCurvePolygon(*this);
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRCurvePolygon::empty()

{
    oCC.empty(this);
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRCurvePolygon::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbCurvePolygonZM;
    else if( flags & OGR_G_MEASURED )
        return wkbCurvePolygonM;
    else if( flags & OGR_G_3D )
        return wkbCurvePolygonZ;
    else
        return wkbCurvePolygon;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRCurvePolygon::getDimension() const

{
    return 2;
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRCurvePolygon::flattenTo2D()

{
    oCC.flattenTo2D(this);
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRCurvePolygon::getGeometryName() const

{
    return "CURVEPOLYGON";
}

/************************************************************************/
/*                         getExteriorRingCurve()                       */
/************************************************************************/

/**
 * \brief Fetch reference to external polygon ring.
 *
 * Note that the returned ring pointer is to an internal data object of the
 * OGRCurvePolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use the
 * OGRGeometry::clone() method to make a separate copy within the application.
 *
 * Relates to the Simple Features for COM (SFCOM) IPolygon::get_ExteriorRing()
 * method.
 * TODO(rouault): What does that mean?
 *
 * @return pointer to external ring.  May be NULL if the OGRCurvePolygon is
 * empty.
 */

OGRCurve *OGRCurvePolygon::getExteriorRingCurve()

{
    return oCC.getCurve(0);
}

/**
 * \brief Fetch reference to external polygon ring.
 *
 * Note that the returned ring pointer is to an internal data object of the
 * OGRCurvePolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use the
 * OGRGeometry::clone() method to make a separate copy within the application.
 *
 * Relates to the SFCOM IPolygon::get_ExteriorRing() method.
 *
 * @return pointer to external ring.  May be NULL if the OGRCurvePolygon is
 * empty.
 */
const OGRCurve *OGRCurvePolygon::getExteriorRingCurve() const

{
    return oCC.getCurve(0);
}

/************************************************************************/
/*                        getNumInteriorRings()                         */
/************************************************************************/

/**
 * \brief Fetch the number of internal rings.
 *
 * Relates to the SFCOM IPolygon::get_NumInteriorRings() method.
 *
 * @return count of internal rings, zero or more.
 */

int OGRCurvePolygon::getNumInteriorRings() const

{
    if( oCC.nCurveCount > 0 )
        return oCC.nCurveCount-1;
    else
        return 0;
}

/************************************************************************/
/*                       getInteriorRingCurve()                         */
/************************************************************************/

/**
 * \brief Fetch reference to indicated internal ring.
 *
 * Note that the returned ring pointer is to an internal data object of the
 * OGRCurvePolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use the
 * OGRGeometry::clone() method to make a separate copy within the application.
 *
 * Relates to the SFCOM IPolygon::get_InternalRing() method.
 *
 * @param iRing internal ring index from 0 to getNumInteriorRings() - 1.
 *
 * @return pointer to interior ring.  May be NULL.
 */

OGRCurve *OGRCurvePolygon::getInteriorRingCurve( int iRing )

{
    return oCC.getCurve(iRing + 1);
}

/**
 * \brief Fetch reference to indicated internal ring.
 *
 * Note that the returned ring pointer is to an internal data object of the
 * OGRCurvePolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use the
 * OGRGeometry::clone() method to make a separate copy within the application.
 *
 * Relates to the SFCOM IPolygon::get_InternalRing() method.
 *
 * @param iRing internal ring index from 0 to getNumInteriorRings() - 1.
 *
 * @return pointer to interior ring.  May be NULL.
 */

const OGRCurve *OGRCurvePolygon::getInteriorRingCurve( int iRing ) const

{
    return oCC.getCurve(iRing + 1);
}

/************************************************************************/
/*                        stealExteriorRingCurve()                      */
/************************************************************************/

/**
 * \brief "Steal" reference to external ring.
 *
 * After the call to that function, only call to stealInteriorRing() or
 * destruction of the OGRCurvePolygon is valid. Other operations may crash.
 *
 * @return pointer to external ring.  May be NULL if the OGRCurvePolygon is
 * empty.
 */

OGRCurve *OGRCurvePolygon::stealExteriorRingCurve()
{
    if( oCC.nCurveCount == 0 )
        return nullptr;
    OGRCurve *poRet = oCC.papoCurves[0];
    oCC.papoCurves[0] = nullptr;
    return poRet;
}

/************************************************************************/
/*                            removeRing()                              */
/************************************************************************/

/**
 * \brief Remove a geometry from the container.
 *
 * Removing a geometry will cause the geometry count to drop by one, and all
 * "higher" geometries will shuffle down one in index.
 *
 * There is no SFCOM analog to this method.
 *
 * @param iIndex the index of the geometry to delete.  A value of -1 is a
 * special flag meaning that all geometries should be removed.
 *
 * @param bDelete if true the geometry will be deallocated, otherwise it will
 * not.  The default is true as the container is considered to own the
 * geometries in it.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE if the index is
 * out of range.
 */

OGRErr  OGRCurvePolygon::removeRing(int iIndex, bool bDelete)
{
    return oCC.removeCurve(iIndex, bDelete);
}

/************************************************************************/
/*                              addRing()                               */
/************************************************************************/

/**
 * \brief Add a ring to a polygon.
 *
 * If the polygon has no external ring (it is empty) this will be used as
 * the external ring, otherwise it is used as an internal ring.  The passed
 * OGRCurve remains the responsibility of the caller (an internal copy
 * is made).
 *
 * This method has no SFCOM analog.
 *
 * @param poNewRing ring to be added to the polygon.
 * @return OGRERR_NONE in case of success
 */

OGRErr OGRCurvePolygon::addRing( OGRCurve * poNewRing )

{
    OGRCurve* poNewRingCloned = poNewRing->clone();
    OGRErr eErr = addRingDirectly(poNewRingCloned);
    if( eErr != OGRERR_NONE )
        delete poNewRingCloned;
    return eErr;
}

/************************************************************************/
/*                            checkRing()                               */
/************************************************************************/

int OGRCurvePolygon::checkRing( OGRCurve * poNewRing ) const
{
    if( !poNewRing->IsEmpty() && !poNewRing->get_IsClosed() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Non closed ring.");
        return FALSE;
    }

    if( wkbFlatten(poNewRing->getGeometryType()) == wkbLineString )
    {
        if( poNewRing->getNumPoints() < 4 )
        {
            return FALSE;
        }

        if( EQUAL(poNewRing->getGeometryName(), "LINEARRING") )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Linearring not allowed.");
            return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                          addRingDirectly()                           */
/************************************************************************/

/**
 * \brief Add a ring to a polygon.
 *
 * If the polygon has no external ring (it is empty) this will be used as
 * the external ring, otherwise it is used as an internal ring.  Ownership
 * of the passed ring is assumed by the OGRCurvePolygon, but otherwise this
 * method operates the same as OGRCurvePolygon::AddRing().
 *
 * This method has no SFCOM analog.
 *
 * @param poNewRing ring to be added to the polygon.
 * @return OGRERR_NONE in case of success
 */

OGRErr OGRCurvePolygon::addRingDirectly( OGRCurve * poNewRing )
{
    return addRingDirectlyInternal( poNewRing, TRUE );
}

OGRErr OGRCurvePolygon::addRingDirectlyInternal( OGRCurve* poNewRing,
                                                 int bNeedRealloc )
{
    if( !checkRing(poNewRing) )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    return oCC.addCurveDirectly(this, poNewRing, bNeedRealloc);
}

/************************************************************************/
/*                              WkbSize()                               */
/*                                                                      */
/*      Return the size of this object in well known binary             */
/*      representation including the byte order, and type information.  */
/************************************************************************/

size_t OGRCurvePolygon::WkbSize() const

{
    return oCC.WkbSize();
}

/************************************************************************/
/*                       addCurveDirectlyFromWkb()                      */
/************************************************************************/

OGRErr OGRCurvePolygon::addCurveDirectlyFromWkb( OGRGeometry* poSelf,
                                                 OGRCurve* poCurve )
{
    OGRCurvePolygon* poCP = poSelf->toCurvePolygon();
    return poCP->addRingDirectlyInternal( poCurve, FALSE );
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRCurvePolygon::importFromWkb( const unsigned char * pabyData,
                                       size_t nSize,
                                       OGRwkbVariant eWkbVariant,
                                       size_t& nBytesConsumedOut )

{
    nBytesConsumedOut = 0;
    OGRwkbByteOrder eByteOrder;
    size_t nDataOffset = 0;
    // coverity[tainted_data]
    OGRErr eErr = oCC.importPreambleFromWkb(this, pabyData, nSize, nDataOffset,
                                             eByteOrder, 9, eWkbVariant);
    if( eErr != OGRERR_NONE )
        return eErr;

    eErr = oCC.importBodyFromWkb(this, pabyData + nDataOffset, nSize,
                                 true,  // bAcceptCompoundCurve
                                 addCurveDirectlyFromWkb,
                                 eWkbVariant,
                                 nBytesConsumedOut );
    if( eErr == OGRERR_NONE )
        nBytesConsumedOut += nDataOffset;
    return eErr;
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr OGRCurvePolygon::exportToWkb( OGRwkbByteOrder eByteOrder,
                                     unsigned char * pabyData,
                                     OGRwkbVariant eWkbVariant ) const

{
    if( eWkbVariant == wkbVariantOldOgc )
        // Does not make sense for new geometries, so patch it.
        eWkbVariant = wkbVariantIso;
    return oCC.exportToWkb(this, eByteOrder, pabyData, eWkbVariant);
}

/************************************************************************/
/*                       addCurveDirectlyFromWkt()                      */
/************************************************************************/

OGRErr OGRCurvePolygon::addCurveDirectlyFromWkt( OGRGeometry* poSelf,
                                                 OGRCurve* poCurve )
{
    OGRCurvePolygon *poCP = poSelf->toCurvePolygon();
    return poCP->addRingDirectly(poCurve);
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.                        */
/************************************************************************/

OGRErr OGRCurvePolygon::importFromWkt( const char ** ppszInput )

{
    return importCurveCollectionFromWkt( ppszInput,
                                         FALSE,  // bAllowEmptyComponent
                                         TRUE,  // bAllowLineString
                                         TRUE,  // bAllowCurve
                                         TRUE,  // bAllowCompoundCurve
                                         addCurveDirectlyFromWkt );
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

std::string OGRCurvePolygon::exportToWkt(const OGRWktOptions& opts, OGRErr *err) const
{
    return oCC.exportToWkt(this, opts, err);
}

/************************************************************************/
/*                           CurvePolyToPoly()                          */
/************************************************************************/

/**
 * \brief Return a polygon from a curve polygon.
 *
 * This method is the same as C function OGR_G_CurvePolyToPoly().
 *
 * The returned geometry is a new instance whose ownership belongs to
 * the caller.
 *
 * @param dfMaxAngleStepSizeDegrees the largest step in degrees along the
 * arc, zero to use the default setting.
 * @param papszOptions options as a null-terminated list of strings.
 *                     Unused for now. Must be set to NULL.
 *
 * @return a linestring
 *
 * @since OGR 2.0
 */

OGRPolygon *
OGRCurvePolygon::CurvePolyToPoly( double dfMaxAngleStepSizeDegrees,
                                  const char* const* papszOptions ) const
{
    OGRPolygon* poPoly = new OGRPolygon();
    poPoly->assignSpatialReference(getSpatialReference());
    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRLineString* poLS =
            oCC.papoCurves[iRing]->CurveToLine(dfMaxAngleStepSizeDegrees,
                                               papszOptions);
        OGRLinearRing* poRing = OGRCurve::CastToLinearRing(poLS);
        if( poRing == nullptr ) {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "OGRCurve::CastToLinearRing failed");
            break;
        }
        poPoly->addRingDirectly(poRing);
    }
    return poPoly;
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRCurvePolygon::hasCurveGeometry( int bLookForNonLinear ) const
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
OGRCurvePolygon::getLinearGeometry( double dfMaxAngleStepSizeDegrees,
                                    const char* const* papszOptions ) const
{
    return CurvePolyToPoly(dfMaxAngleStepSizeDegrees, papszOptions);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCurvePolygon::getEnvelope( OGREnvelope * psEnvelope ) const

{
    oCC.getEnvelope(psEnvelope);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCurvePolygon::getEnvelope( OGREnvelope3D * psEnvelope ) const

{
    oCC.getEnvelope(psEnvelope);
}

/************************************************************************/
/*                               Equals()                               */
/************************************************************************/

OGRBoolean OGRCurvePolygon::Equals( const OGRGeometry * poOther ) const

{
    if( poOther == this )
        return TRUE;

    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if( IsEmpty() && poOther->IsEmpty() )
        return TRUE;

    return oCC.Equals( &(poOther->toCurvePolygon()->oCC) );
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRCurvePolygon::transform( OGRCoordinateTransformation *poCT )

{
    return oCC.transform(this, poCT);
}

/************************************************************************/
/*                              get_Area()                              */
/************************************************************************/

double OGRCurvePolygon::get_Area() const

{
    if( getExteriorRingCurve() == nullptr )
        return 0.0;

    double dfArea = getExteriorRingCurve()->get_Area();

    for( int iRing = 0; iRing < getNumInteriorRings(); iRing++ )
    {
        dfArea -= getInteriorRingCurve(iRing)->get_Area();
    }

    return dfArea;
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRCurvePolygon::setCoordinateDimension( int nNewDimension )

{
    oCC.setCoordinateDimension(this, nNewDimension);
}

void OGRCurvePolygon::set3D( OGRBoolean bIs3D )
{
    oCC.set3D( this, bIs3D );
}

void OGRCurvePolygon::setMeasured( OGRBoolean bIsMeasured )
{
    oCC.setMeasured( this, bIsMeasured );
}

/************************************************************************/
/*                       assignSpatialReference()                       */
/************************************************************************/

void OGRCurvePolygon::assignSpatialReference( OGRSpatialReference * poSR )
{
    oCC.assignSpatialReference( this, poSR );
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRCurvePolygon::IsEmpty() const
{
    return oCC.IsEmpty();
}

/************************************************************************/
/*                              segmentize()                            */
/************************************************************************/

void OGRCurvePolygon::segmentize( double dfMaxLength )
{
    if (EQUAL(getGeometryName(), "TRIANGLE"))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "segmentize() is not valid for Triangle");
        return;
    }
    oCC.segmentize(dfMaxLength);
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRCurvePolygon::swapXY()
{
    oCC.swapXY();
}

/************************************************************************/
/*                           ContainsPoint()                             */
/************************************************************************/

OGRBoolean OGRCurvePolygon::ContainsPoint( const OGRPoint* p ) const
{
    if( getExteriorRingCurve() != nullptr &&
        getNumInteriorRings() == 0 )
    {
        const int nRet = getExteriorRingCurve()->ContainsPoint(p);
        if( nRet >= 0 )
            return nRet;
    }

    return OGRGeometry::Contains(p);
}

/************************************************************************/
/*                          IntersectsPoint()                           */
/************************************************************************/

OGRBoolean OGRCurvePolygon::IntersectsPoint( const OGRPoint* p ) const
{
    if( getExteriorRingCurve() != nullptr &&
        getNumInteriorRings() == 0 )
    {
        const int nRet = getExteriorRingCurve()->IntersectsPoint(p);
        if( nRet >= 0 )
            return nRet;
    }

    return OGRGeometry::Intersects(p);
}

/************************************************************************/
/*                               Contains()                             */
/************************************************************************/

OGRBoolean OGRCurvePolygon::Contains( const OGRGeometry *poOtherGeom ) const

{
    if( !IsEmpty() && poOtherGeom != nullptr &&
        wkbFlatten(poOtherGeom->getGeometryType()) == wkbPoint )
    {
        return ContainsPoint(poOtherGeom->toPoint());
    }

    return OGRGeometry::Contains(poOtherGeom);
}

/************************************************************************/
/*                              Intersects()                            */
/************************************************************************/

OGRBoolean OGRCurvePolygon::Intersects( const OGRGeometry *poOtherGeom ) const

{
    if( !IsEmpty() && poOtherGeom != nullptr &&
        wkbFlatten(poOtherGeom->getGeometryType()) == wkbPoint )
    {
        return IntersectsPoint(poOtherGeom->toPoint());
    }

    return OGRGeometry::Intersects(poOtherGeom);
}

/************************************************************************/
/*                           CastToPolygon()                            */
/************************************************************************/

/**
 * \brief Convert to polygon.
 *
 * This method should only be called if the curve polygon actually only contains
 * instances of OGRLineString. This can be verified if hasCurveGeometry(TRUE)
 * returns FALSE. It is not intended to approximate curve polygons. For that
 * use getLinearGeometry().
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure).
 *
 * @param poCP the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRPolygon* OGRCurvePolygon::CastToPolygon(OGRCurvePolygon* poCP)
{
    for( int i = 0; i < poCP->oCC.nCurveCount; i++ )
    {
        poCP->oCC.papoCurves[i] =
            OGRCurve::CastToLinearRing(poCP->oCC.papoCurves[i]);
        if( poCP->oCC.papoCurves[i] == nullptr )
        {
            delete poCP;
            return nullptr;
        }
    }
    OGRPolygon* poPoly = new OGRPolygon();
    poPoly->setCoordinateDimension(poCP->getCoordinateDimension());
    poPoly->assignSpatialReference(poCP->getSpatialReference());
    poPoly->oCC.nCurveCount = poCP->oCC.nCurveCount;
    poPoly->oCC.papoCurves = poCP->oCC.papoCurves;
    poCP->oCC.nCurveCount = 0;
    poCP->oCC.papoCurves = nullptr;
    delete poCP;
    return poPoly;
}

//! @cond Doxygen_Suppress
/************************************************************************/
/*                      GetCasterToPolygon()                            */
/************************************************************************/

OGRPolygon* OGRCurvePolygon::CasterToPolygon(OGRSurface* poSurface)
{
    OGRCurvePolygon* poCurvePoly = poSurface->toCurvePolygon();
    return OGRCurvePolygon::CastToPolygon(poCurvePoly);
}

OGRSurfaceCasterToPolygon OGRCurvePolygon::GetCasterToPolygon() const
{
    return OGRCurvePolygon::CasterToPolygon;
}

/************************************************************************/
/*                      GetCasterToCurvePolygon()                       */
/************************************************************************/

static OGRCurvePolygon* CasterToCurvePolygon(OGRSurface* poSurface)
{
    return poSurface->toCurvePolygon();
}

OGRSurfaceCasterToCurvePolygon OGRCurvePolygon::GetCasterToCurvePolygon() const
{
    return ::CasterToCurvePolygon;
}
//! @endcond
