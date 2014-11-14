/******************************************************************************
 * $Id$
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

#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_geos.h"
#include "ogr_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRCurvePolygon()                         */
/************************************************************************/

/**
 * \brief Create an empty curve polygon.
 */

OGRCurvePolygon::OGRCurvePolygon()

{
}

/************************************************************************/
/*                           ~OGRCurvePolygon()                         */
/************************************************************************/

OGRCurvePolygon::~OGRCurvePolygon()

{
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRCurvePolygon::clone() const

{
    OGRCurvePolygon  *poNewPolygon;

    poNewPolygon = (OGRCurvePolygon*)
            OGRGeometryFactory::createGeometry(getGeometryType());
    poNewPolygon->assignSpatialReference( getSpatialReference() );

    for( int i = 0; i < oCC.nCurveCount; i++ )
    {
        poNewPolygon->addRing( oCC.papoCurves[i] );
    }

    return poNewPolygon;
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
    if( nCoordDimension == 3 )
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
 * Note that the returned ring pointer is to an internal data object of
 * the OGRCurvePolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use
 * the OGRGeometry::clone() method to make a separate copy within the
 * application.
 *
 * Relates to the SFCOM IPolygon::get_ExteriorRing() method.
 *
 * @return pointer to external ring.  May be NULL if the OGRCurvePolygon is empty.
 */

OGRCurve *OGRCurvePolygon::getExteriorRingCurve()

{
    return oCC.getCurve(0);
}

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
 * Note that the returned ring pointer is to an internal data object of
 * the OGRCurvePolygon.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use
 * the OGRGeometry::clone() method to make a separate copy within the
 * application.
 *
 * Relates to the SFCOM IPolygon::get_InternalRing() method.
 *
 * @param iRing internal ring index from 0 to getNumInternalRings() - 1.
 *
 * @return pointer to interior ring.  May be NULL.
 */

OGRCurve *OGRCurvePolygon::getInteriorRingCurve( int iRing )

{
    return oCC.getCurve(iRing + 1);
}

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
 * @return pointer to external ring.  May be NULL if the OGRCurvePolygon is empty.
 */

OGRCurve *OGRCurvePolygon::stealExteriorRingCurve()
{
    if( oCC.nCurveCount == 0 )
        return NULL;
    OGRCurve *poRet = oCC.papoCurves[0];
    oCC.papoCurves[0] = NULL;
    return poRet;
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
    OGRCurve* poNewRingCloned = (OGRCurve* )poNewRing->clone();
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
        if( poNewRing->getNumPoints() == 0 || poNewRing->getNumPoints() < 4 )
        {
            return FALSE;
        }

        if( EQUAL(poNewRing->getGeometryName(), "LINEARRING") )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Linearring not allowed.");
            return NULL;
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

int OGRCurvePolygon::WkbSize() const

{
    return oCC.WkbSize();
}

/************************************************************************/
/*                       addCurveDirectlyFromWkt()                      */
/************************************************************************/

OGRErr OGRCurvePolygon::addCurveDirectlyFromWkb( OGRGeometry* poSelf,
                                                 OGRCurve* poCurve )
{
    OGRCurvePolygon* poCP = (OGRCurvePolygon*)poSelf;
    return poCP->addRingDirectlyInternal( poCurve, FALSE );
}

/************************************************************************/
/*                           importFromWkb()                            */
/*                                                                      */
/*      Initialize from serialized stream in well known binary          */
/*      format.                                                         */
/************************************************************************/

OGRErr OGRCurvePolygon::importFromWkb( unsigned char * pabyData,
                                       int nSize,
                                       OGRwkbVariant eWkbVariant )

{
    OGRwkbByteOrder eByteOrder;
    int nDataOffset = 0;
    OGRErr eErr = oCC.importPreambuleFromWkb(this, pabyData, nSize, nDataOffset,
                                             eByteOrder, 9, eWkbVariant);
    if( eErr >= 0 )
        return eErr;

    return oCC.importBodyFromWkb(this, pabyData, nSize, nDataOffset,
                                 TRUE /* bAcceptCompoundCurve */, addCurveDirectlyFromWkb,
                                 eWkbVariant);
}

/************************************************************************/
/*                            exportToWkb()                             */
/*                                                                      */
/*      Build a well known binary representation of this object.        */
/************************************************************************/

OGRErr  OGRCurvePolygon::exportToWkb( OGRwkbByteOrder eByteOrder,
                                      unsigned char * pabyData,
                                      OGRwkbVariant eWkbVariant ) const

{
    if( eWkbVariant == wkbVariantOldOgc ) /* does not make sense for new geometries, so patch it */
        eWkbVariant = wkbVariantIso;
    return oCC.exportToWkb(this, eByteOrder, pabyData, eWkbVariant);
}

/************************************************************************/
/*                       addCurveDirectlyFromWkt()                      */
/************************************************************************/

OGRErr OGRCurvePolygon::addCurveDirectlyFromWkt( OGRGeometry* poSelf, OGRCurve* poCurve )
{
    return ((OGRCurvePolygon*)poSelf)->addRingDirectly(poCurve);
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.                        */
/************************************************************************/

OGRErr OGRCurvePolygon::importFromWkt( char ** ppszInput )

{
    return importCurveCollectionFromWkt( ppszInput,
                                         FALSE, /* bAllowEmptyComponent */
                                         TRUE, /* bAllowLineString */
                                         TRUE, /* bAllowCurve */
                                         TRUE, /* bAllowCompoundCurve */
                                         addCurveDirectlyFromWkt );
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

OGRErr OGRCurvePolygon::exportToWkt( char ** ppszDstText,
                                CPL_UNUSED OGRwkbVariant eWkbVariant ) const

{
    return oCC.exportToWkt(this, ppszDstText);
}

/************************************************************************/
/*                           CurvePolyToPoly()                          */
/************************************************************************/

/**
 * \brief Return a polygon from a curve polygon.
 *
 * This method is the same as C function OGR_G_CurvePolyToPoly().
 *
 * The returned geometry is a new instance whose ownership belongs to the caller.
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

OGRPolygon* OGRCurvePolygon::CurvePolyToPoly(double dfMaxAngleStepSizeDegrees,
                                             const char* const* papszOptions) const
{
    OGRPolygon* poPoly = new OGRPolygon();
    poPoly->assignSpatialReference(getSpatialReference());
    for( int iRing = 0; iRing < oCC.nCurveCount; iRing++ )
    {
        OGRLineString* poLS = oCC.papoCurves[iRing]->CurveToLine(dfMaxAngleStepSizeDegrees,
                                                                 papszOptions);
        poPoly->addRingDirectly(OGRCurve::CastToLinearRing(poLS));
    }
    return poPoly;
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRCurvePolygon::hasCurveGeometry(int bLookForNonLinear) const
{
    if( bLookForNonLinear )
    {
        return oCC.hasCurveGeometry(bLookForNonLinear);
    }
    else
        return TRUE;
}

/************************************************************************/
/*                         getLinearGeometry()                        */
/************************************************************************/

OGRGeometry* OGRCurvePolygon::getLinearGeometry(double dfMaxAngleStepSizeDegrees,
                                                  const char* const* papszOptions) const
{
    return CurvePolyToPoly(dfMaxAngleStepSizeDegrees, papszOptions);
}

/************************************************************************/
/*                           PointOnSurface()                           */
/************************************************************************/

int OGRCurvePolygon::PointOnSurface( OGRPoint *poPoint ) const

{
    OGRPolygon* poPoly = CurvePolyToPoly();
    int ret = poPoly->PointOnSurface(poPoint);
    delete poPoly;
    return ret;
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
/*                               Equal()                                */
/************************************************************************/

OGRBoolean OGRCurvePolygon::Equals( OGRGeometry * poOther ) const

{
    OGRCurvePolygon *poOPoly = (OGRCurvePolygon *) poOther;

    if( poOPoly == this )
        return TRUE;
    
    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;

    if ( IsEmpty() && poOther->IsEmpty() )
        return TRUE;
    
    return oCC.Equals( &(poOPoly->oCC) );
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
    double dfArea = 0.0;

    if( getExteriorRingCurve() != NULL )
    {
        int iRing;

        dfArea = getExteriorRingCurve()->get_Area();

        for( iRing = 0; iRing < getNumInteriorRings(); iRing++ )
        {
            dfArea -= getInteriorRingCurve(iRing)->get_Area();
        }
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


/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRCurvePolygon::IsEmpty(  ) const
{
    return oCC.IsEmpty();
}

/************************************************************************/
/*                              segmentize()                            */
/************************************************************************/

void OGRCurvePolygon::segmentize( double dfMaxLength )
{
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
    if( getExteriorRingCurve() != NULL &&
        getNumInteriorRings() == 0 )
    {
        int nRet = getExteriorRingCurve()->ContainsPoint(p);
        if( nRet >= 0 )
            return nRet;
    }
    return OGRGeometry::Contains(p);
}

/************************************************************************/
/*                               Contains()                             */
/************************************************************************/

OGRBoolean OGRCurvePolygon::Contains( const OGRGeometry *poOtherGeom ) const

{
    if( !IsEmpty() && poOtherGeom != NULL &&
        wkbFlatten(poOtherGeom->getGeometryType()) == wkbPoint )
    {
        return ContainsPoint((OGRPoint*)poOtherGeom);
    }
    else
        return OGRGeometry::Contains(poOtherGeom);
}

/************************************************************************/
/*                              Intersects()                            */
/************************************************************************/

OGRBoolean OGRCurvePolygon::Intersects( const OGRGeometry *poOtherGeom ) const

{
    if( !IsEmpty() && poOtherGeom != NULL &&
        wkbFlatten(poOtherGeom->getGeometryType()) == wkbPoint )
    {
        return ContainsPoint((OGRPoint*)poOtherGeom);
    }
    else
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
 * @param poMS the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRPolygon* OGRCurvePolygon::CastToPolygon(OGRCurvePolygon* poCP)
{
    for(int i=0;i<poCP->oCC.nCurveCount;i++)
    {
        poCP->oCC.papoCurves[i] = OGRCurve::CastToLinearRing(poCP->oCC.papoCurves[i]);
        if( poCP->oCC.papoCurves[i] == NULL )
        {
            delete poCP;
            return NULL;
        }
    }
    OGRPolygon* poPoly = new OGRPolygon();
    poPoly->setCoordinateDimension(poCP->getCoordinateDimension());
    poPoly->assignSpatialReference(poCP->getSpatialReference());
    poPoly->oCC.nCurveCount = poCP->oCC.nCurveCount;
    poPoly->oCC.papoCurves = poCP->oCC.papoCurves;
    poCP->oCC.nCurveCount = 0;
    poCP->oCC.papoCurves = NULL;
    delete poCP;
    return poPoly;
}

/************************************************************************/
/*                      GetCasterToPolygon()                            */
/************************************************************************/

OGRSurfaceCasterToPolygon OGRCurvePolygon::GetCasterToPolygon() const {
    return (OGRSurfaceCasterToPolygon) OGRCurvePolygon::CastToPolygon;
}

/************************************************************************/
/*                      OGRSurfaceCasterToCurvePolygon()                */
/************************************************************************/

OGRSurfaceCasterToCurvePolygon OGRCurvePolygon::GetCasterToCurvePolygon() const {
    return (OGRSurfaceCasterToCurvePolygon) OGRGeometry::CastToIdentity;
}
