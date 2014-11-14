/******************************************************************************
 * $Id$
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

#include "ogr_geometry.h"
#include "ogr_p.h"
#include <assert.h>

CPL_CVSID("$Id");

/************************************************************************/
/*                         OGRCompoundCurve()                           */
/************************************************************************/

/**
 * \brief Create an empty compound curve.
 */

OGRCompoundCurve::OGRCompoundCurve()

{
}

/************************************************************************/
/*                         ~OGRCompoundCurve()                          */
/************************************************************************/

OGRCompoundCurve::~OGRCompoundCurve()

{
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRCompoundCurve::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
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
int OGRCompoundCurve::WkbSize() const
{
    return oCC.WkbSize();
}

/************************************************************************/
/*                       addCurveDirectlyFromWkt()                      */
/************************************************************************/

OGRErr OGRCompoundCurve::addCurveDirectlyFromWkb( OGRGeometry* poSelf,
                                                  OGRCurve* poCurve )
{
    OGRCompoundCurve* poCC = (OGRCompoundCurve*)poSelf;
    return poCC->addCurveDirectlyInternal( poCurve, 1e-14, FALSE );
}

/************************************************************************/
/*                           importFromWkb()                            */
/************************************************************************/

OGRErr OGRCompoundCurve::importFromWkb( unsigned char * pabyData,
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
                                 FALSE /* bAcceptCompoundCurve */, addCurveDirectlyFromWkb,
                                 eWkbVariant);
}

/************************************************************************/
/*                            exportToWkb()                             */
/************************************************************************/
OGRErr OGRCompoundCurve::exportToWkb( OGRwkbByteOrder eByteOrder,
                                      unsigned char * pabyData,
                                      OGRwkbVariant eWkbVariant  ) const
{
    if( eWkbVariant == wkbVariantOldOgc ) /* does not make sense for new geometries, so patch it */
        eWkbVariant = wkbVariantIso;
    return oCC.exportToWkb(this, eByteOrder, pabyData, eWkbVariant);
}

/************************************************************************/
/*                       addCurveDirectlyFromWkt()                      */
/************************************************************************/

OGRErr OGRCompoundCurve::addCurveDirectlyFromWkt( OGRGeometry* poSelf, OGRCurve* poCurve )
{
    return ((OGRCompoundCurve*)poSelf)->addCurveDirectly(poCurve);
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

OGRErr OGRCompoundCurve::importFromWkt( char ** ppszInput )
{
    return importCurveCollectionFromWkt( ppszInput,
                                         FALSE, /* bAllowEmptyComponent */
                                         TRUE, /* bAllowLineString */
                                         TRUE, /* bAllowCurve */
                                         FALSE, /* bAllowCompoundCurve */
                                         addCurveDirectlyFromWkt );
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/
OGRErr OGRCompoundCurve::exportToWkt( char ** ppszDstText,
                                      CPL_UNUSED OGRwkbVariant eWkbVariant ) const

{
    return oCC.exportToWkt(this, ppszDstText);
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRGeometry *OGRCompoundCurve::clone() const
{
    OGRCompoundCurve       *poNewCC;

    poNewCC = new OGRCompoundCurve;
    poNewCC->assignSpatialReference( getSpatialReference() );

    for( int i = 0; i < oCC.nCurveCount; i++ )
    {
        poNewCC->addCurve( oCC.papoCurves[i] );
    }

    return poNewCC;
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
/*      For now we return a simple euclidian 2D distance.               */
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

    double dfLength = 0;
    for( int iGeom = 0; iGeom < oCC.nCurveCount; iGeom++ )
    {
        double dfSegLength = oCC.papoCurves[iGeom]->get_Length();
        if (dfSegLength > 0)
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

OGRLineString* OGRCompoundCurve::CurveToLineInternal(double dfMaxAngleStepSizeDegrees,
                                                     const char* const* papszOptions,
                                                     int bIsLinearRing) const
{
    OGRLineString* poLine;
    if( bIsLinearRing )
        poLine = new OGRLinearRing();
    else
        poLine = new OGRLineString();
    poLine->assignSpatialReference(getSpatialReference());
    for( int iGeom = 0; iGeom < oCC.nCurveCount; iGeom++ )
    {
        OGRLineString* poSubLS = oCC.papoCurves[iGeom]->CurveToLine(dfMaxAngleStepSizeDegrees,
                                                                    papszOptions);
        poLine->addSubLineString(poSubLS, (iGeom == 0) ? 0 : 1);
        delete poSubLS;
    }
    return poLine;
}

/************************************************************************/
/*                          CurveToLine()                               */
/************************************************************************/

OGRLineString* OGRCompoundCurve::CurveToLine(double dfMaxAngleStepSizeDegrees,
                                             const char* const* papszOptions) const
{
    return CurveToLineInternal(dfMaxAngleStepSizeDegrees, papszOptions, FALSE);
}

/************************************************************************/
/*                               Equals()                                */
/************************************************************************/

OGRBoolean  OGRCompoundCurve::Equals( OGRGeometry *poOther ) const
{
    OGRCompoundCurve *poOCC = (OGRCompoundCurve *) poOther;

    if( poOCC == this )
        return TRUE;
    
    if( poOther->getGeometryType() != getGeometryType() )
        return FALSE;
    
    return oCC.Equals(&(poOCC->oCC));
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRCompoundCurve::setCoordinateDimension( int nNewDimension )
{
    oCC.setCoordinateDimension( this, nNewDimension );
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

int          OGRCompoundCurve::getNumCurves() const
{
    return oCC.nCurveCount;
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

/**
 * \brief Fetch reference to indicated internal ring.
 *
 * Note that the returned curve pointer is to an internal data object of
 * the OGRCompoundCurve.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use
 * the OGRGeometry::clone() method to make a separate copy within the
 * application.
 *
 * Relates to the ISO SQL/MM ST_CurveN() function.
 *
 * @param iRing curve index from 0 to getNumCurves() - 1.
 *
 * @return pointer to curve.  May be NULL.
 */

OGRCurve    *OGRCompoundCurve::getCurve( int i )
{
    return oCC.getCurve(i);
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

/**
 * \brief Fetch reference to indicated internal ring.
 *
 * Note that the returned curve pointer is to an internal data object of
 * the OGRCompoundCurve.  It should not be modified or deleted by the application,
 * and the pointer is only valid till the polygon is next modified.  Use
 * the OGRGeometry::clone() method to make a separate copy within the
 * application.
 *
 * Relates to the ISO SQL/MM ST_CurveN() function.
 *
 * @param iRing curve index from 0 to getNumCurves() - 1.
 *
 * @return pointer to curve.  May be NULL.
 */

const OGRCurve *OGRCompoundCurve::getCurve( int i ) const
{
    return oCC.getCurve(i);
}

/************************************************************************/
/*                           stealCurve()                               */
/************************************************************************/

OGRCurve* OGRCompoundCurve::stealCurve( int i )
{
    return oCC.stealCurve(i);
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
 * @param dfToleranceEps tolerance when checking that the first point of a
 *                       segment matches then end point of the previous one.
 *                       Default value: 1e-14.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE in case of error
 * (for example if curves are not contiguous)
 */

OGRErr OGRCompoundCurve::addCurve( OGRCurve* poCurve, double dfToleranceEps )
{
    OGRCurve* poClonedCurve = (OGRCurve*)poCurve->clone();
    OGRErr eErr = addCurveDirectly( poClonedCurve, dfToleranceEps );
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
 * @param dfToleranceEps tolerance when checking that the first point of a
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
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid curve: not enough points");
        return OGRERR_FAILURE;
    }

    OGRwkbGeometryType eCurveType = wkbFlatten(poCurve->getGeometryType());
    if( EQUAL(poCurve->getGeometryName(), "LINEARRING") )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Linearring not allowed.");
        return OGRERR_FAILURE;
    }
    else if( eCurveType == wkbCompoundCurve )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot add a compound curve inside a compound curve");
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

        OGRPoint end, start;
        oCC.papoCurves[oCC.nCurveCount-1]->EndPoint(&end);
        poCurve->StartPoint(&start);
        if( fabs(end.getX() - start.getX()) > dfToleranceEps ||
            fabs(end.getY() - start.getY()) > dfToleranceEps ||
            fabs(end.getZ() - start.getZ()) > dfToleranceEps )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Non contiguous curves");
            return OGRERR_FAILURE;
        }
        ((OGRSimpleCurve*)poCurve)->setPoint(0, &end); /* patch so that it matches exactly */
    }

    return oCC.addCurveDirectly(this, poCurve, bNeedRealloc);
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr  OGRCompoundCurve::transform( OGRCoordinateTransformation *poCT )
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

void OGRCompoundCurve::segmentize(double dfMaxLength)
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

OGRBoolean OGRCompoundCurve::hasCurveGeometry(int bLookForNonLinear) const
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

OGRGeometry* OGRCompoundCurve::getLinearGeometry(double dfMaxAngleStepSizeDegrees,
                                                   const char* const* papszOptions) const
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
            nPoints --;
    }
    return nPoints;
}

/************************************************************************/
/*                      OGRCompoundCurvePointIterator                   */
/************************************************************************/

class OGRCompoundCurvePointIterator: public OGRPointIterator
{
        const OGRCompoundCurve *poCC;
        int                     iCurCurve;
        OGRPointIterator       *poCurveIter;

    public:
        OGRCompoundCurvePointIterator(const OGRCompoundCurve* poCC) :
                            poCC(poCC), iCurCurve(0), poCurveIter(NULL) {}
       ~OGRCompoundCurvePointIterator() { delete poCurveIter; }

        virtual OGRBoolean getNextPoint(OGRPoint* p);
};

/************************************************************************/
/*                            getNextPoint()                            */
/************************************************************************/

OGRBoolean OGRCompoundCurvePointIterator::getNextPoint(OGRPoint* p)
{
    if( iCurCurve == poCC->getNumCurves() )
        return FALSE;
    if( poCurveIter == NULL )
        poCurveIter = poCC->getCurve(0)->getPointIterator();
    if( !poCurveIter->getNextPoint(p) )
    {
        iCurCurve ++;
        if( iCurCurve == poCC->getNumCurves() )
            return FALSE;
        delete poCurveIter;
        poCurveIter = poCC->getCurve(iCurCurve)->getPointIterator();
        /* skip first point */
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

OGRLineString* OGRCompoundCurve::CastToLineString(OGRCompoundCurve* poCC)
{
    for(int i=0;i<poCC->oCC.nCurveCount;i++)
    {
        poCC->oCC.papoCurves[i] = OGRCurve::CastToLineString(poCC->oCC.papoCurves[i]);
        if( poCC->oCC.papoCurves[i] == NULL )
        {
            delete poCC;
            return NULL;
        }
    }

    if( poCC->oCC.nCurveCount == 1 )
    {
        OGRLineString* poLS = (OGRLineString*) poCC->oCC.papoCurves[0];
        poLS->assignSpatialReference(poCC->getSpatialReference());
        poCC->oCC.papoCurves[0] = NULL;
        delete poCC;
        return poLS;
    }

    OGRLineString* poLS = poCC->CurveToLineInternal(0, NULL, FALSE);
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

OGRLinearRing* OGRCompoundCurve::CastToLinearRing(OGRCompoundCurve* poCC)
{
    for(int i=0;i<poCC->oCC.nCurveCount;i++)
    {
        poCC->oCC.papoCurves[i] = OGRCurve::CastToLineString(poCC->oCC.papoCurves[i]);
        if( poCC->oCC.papoCurves[i] == NULL )
        {
            delete poCC;
            return NULL;
        }
    }

    if( poCC->oCC.nCurveCount == 1 )
    {
        OGRLinearRing* poLR = OGRCurve::CastToLinearRing( poCC->oCC.papoCurves[0] );
        if( poLR != NULL )
        {
            poLR->assignSpatialReference(poCC->getSpatialReference());
        }
        poCC->oCC.papoCurves[0] = NULL;
        delete poCC;
        return poLR;
    }

    OGRLinearRing* poLR = (OGRLinearRing*)poCC->CurveToLineInternal(0, NULL, TRUE);
    delete poCC;
    return poLR;
}

/************************************************************************/
/*                     GetCasterToLineString()                          */
/************************************************************************/

OGRCurveCasterToLineString OGRCompoundCurve::GetCasterToLineString() const {
    return (OGRCurveCasterToLineString) OGRCompoundCurve::CastToLineString;
}

/************************************************************************/
/*                        GetCasterToLinearRing()                       */
/************************************************************************/

OGRCurveCasterToLinearRing OGRCompoundCurve::GetCasterToLinearRing() const {
    return (OGRCurveCasterToLinearRing) OGRCompoundCurve::CastToLinearRing;
}

/************************************************************************/
/*                           get_Area()                                 */
/************************************************************************/

double OGRCompoundCurve::get_Area() const
{
    if( IsEmpty() || !get_IsClosed() )
        return 0;

    /* Optimization for convex rings */
    if( IsConvex() )
    {
        /* Compute area of shape without the circular segments */
        OGRPointIterator* poIter = getPointIterator();
        OGRLineString oLS;
        oLS.setNumPoints( getNumPoints() );
        OGRPoint p;
        for(int i = 0; poIter->getNextPoint(&p); i++ )
        {
            oLS.setPoint( i, p.getX(), p.getY() );
        }
        double dfArea = oLS.get_Area();
        delete poIter;

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
/*                       get_AreaOfCurveSegments()                      */
/************************************************************************/

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
