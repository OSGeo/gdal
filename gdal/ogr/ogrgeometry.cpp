/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements a few base methods on OGRGeometry.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_geos.h"
#include "ogr_sfcgal.h"
#include "ogr_libs.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"

#ifndef HAVE_GEOS
#define UNUSED_IF_NO_GEOS CPL_UNUSED
#else
#define UNUSED_IF_NO_GEOS
#endif

CPL_CVSID("$Id$");

//! @cond Doxygen_Suppress
int OGRGeometry::bGenerate_DB2_V72_BYTE_ORDER = FALSE;
//! @endcond

#ifdef HAVE_GEOS
static void OGRGEOSErrorHandler(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    CPLErrorV( CE_Failure, CPLE_AppDefined, fmt, args );
    va_end(args);
}

static void OGRGEOSWarningHandler(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    CPLErrorV( CE_Warning, CPLE_AppDefined, fmt, args );
    va_end(args);
}
#endif

/************************************************************************/
/*                            OGRGeometry()                             */
/************************************************************************/

OGRGeometry::OGRGeometry()

{
    poSRS = NULL;
    flags = 0;
}

/************************************************************************/
/*                   OGRGeometry( const OGRGeometry& )                  */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRGeometry::OGRGeometry( const OGRGeometry& other ) :
    poSRS(other.poSRS),
    flags(other.flags)
{
    if( poSRS != NULL )
        poSRS->Reference();
}

/************************************************************************/
/*                            ~OGRGeometry()                            */
/************************************************************************/

OGRGeometry::~OGRGeometry()

{
    if( poSRS != NULL )
        poSRS->Release();
}

/************************************************************************/
/*                    operator=( const OGRGeometry&)                    */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRGeometry& OGRGeometry::operator=( const OGRGeometry& other )
{
    if( this != &other)
    {
        assignSpatialReference( other.getSpatialReference() );
        flags = other.flags;
    }
    return *this;
}

/************************************************************************/
/*                            dumpReadable()                            */
/************************************************************************/

/**
 * \brief Dump geometry in well known text format to indicated output file.
 *
 * A few options can be defined to change the default dump :
 * <ul>
 * <li>DISPLAY_GEOMETRY=NO : to hide the dump of the geometry</li>
 * <li>DISPLAY_GEOMETRY=WKT or YES (default) : dump the geometry as a WKT</li>
 * <li>DISPLAY_GEOMETRY=SUMMARY : to get only a summary of the geometry</li>
 * </ul>
 *
 * This method is the same as the C function OGR_G_DumpReadable().
 *
 * @param fp the text file to write the geometry to.
 * @param pszPrefix the prefix to put on each line of output.
 * @param papszOptions NULL terminated list of options (may be NULL)
 */

void OGRGeometry::dumpReadable( FILE * fp, const char * pszPrefix,
                                char** papszOptions ) const

{
    if( pszPrefix == NULL )
        pszPrefix = "";

    if( fp == NULL )
        fp = stdout;

    const char* pszDisplayGeometry =
        CSLFetchNameValue(papszOptions, "DISPLAY_GEOMETRY");
    if( pszDisplayGeometry != NULL && EQUAL(pszDisplayGeometry, "SUMMARY") )
    {
        fprintf( fp, "%s%s : ", pszPrefix, getGeometryName() );
        switch( getGeometryType() )
        {
            case wkbUnknown:
            case wkbNone:
            case wkbPoint:
            case wkbPoint25D:
            case wkbPointM:
            case wkbPointZM:
                break;
            case wkbPolyhedralSurface:
            case wkbTIN:
            case wkbPolyhedralSurfaceZ:
            case wkbTINZ:
            case wkbPolyhedralSurfaceM:
            case wkbTINM:
            case wkbPolyhedralSurfaceZM:
            case wkbTINZM:
            {
                int ig;
                OGRPolyhedralSurface* poPS = (OGRPolyhedralSurface *)this;
                fprintf( fp, "%d geometries:\n", poPS->getNumGeometries() );
                for ( ig = 0; ig < poPS->getNumGeometries(); ig++)
                {
                    const OGRGeometry *poChild = poPS->getGeometryRef(ig);
                    fprintf( fp, "%s", pszPrefix);
                    poChild->dumpReadable( fp, pszPrefix, papszOptions );
                }
                break;
            }
            case wkbLineString:
            case wkbLineString25D:
            case wkbLineStringM:
            case wkbLineStringZM:
            case wkbCircularString:
            case wkbCircularStringZ:
            case wkbCircularStringM:
            case wkbCircularStringZM:
            {
                OGRLineString *poLine = dynamic_cast<OGRLineString *>(
                    const_cast<OGRGeometry *>(this));
                if( poLine == NULL )
                {
                    CPLError(CE_Fatal, CPLE_AppDefined,
                             "dynamic_cast failed.  Expected OGRLineString.");
                    return;
                }
                fprintf( fp, "%d points\n", poLine->getNumPoints() );
                break;
            }
            case wkbPolygon:
            case wkbTriangle:
            case wkbTriangleZ:
            case wkbTriangleM:
            case wkbTriangleZM:
            case wkbPolygon25D:
            case wkbPolygonM:
            case wkbPolygonZM:
            case wkbCurvePolygon:
            case wkbCurvePolygonZ:
            case wkbCurvePolygonM:
            case wkbCurvePolygonZM:
            {
                OGRCurvePolygon *poPoly = dynamic_cast<OGRCurvePolygon *>(
                    const_cast<OGRGeometry *>(this));
                if( poPoly == NULL )
                {
                    CPLError(CE_Fatal, CPLE_AppDefined,
                             "dynamic_cast failed.  Expected OGRCurvePolygon.");
                    return;
                }

                OGRCurve *poRing = poPoly->getExteriorRingCurve();
                const int nRings = poPoly->getNumInteriorRings();
                if( poRing == NULL )
                {
                    fprintf( fp, "empty");
                }
                else
                {
                    fprintf( fp, "%d points", poRing->getNumPoints() );
                    if( wkbFlatten(poRing->getGeometryType()) ==
                        wkbCompoundCurve )
                    {
                        fprintf( fp, " (");
                        poRing->dumpReadable(fp, NULL, papszOptions);
                        fprintf( fp, ")");
                    }
                    if( nRings )
                    {
                        fprintf( fp, ", %d inner rings (", nRings);
                        for( int ir = 0; ir < nRings; ir++ )
                        {
                            poRing = poPoly->getInteriorRingCurve(ir);
                            if( ir )
                                fprintf( fp, ", ");
                            fprintf( fp, "%d points", poRing->getNumPoints() );
                            if( wkbFlatten(poRing->getGeometryType()) ==
                                wkbCompoundCurve )
                            {
                                fprintf( fp, " (");
                                poRing->dumpReadable(fp, NULL, papszOptions);
                                fprintf( fp, ")");
                            }
                        }
                        fprintf( fp, ")");
                    }
                }
                fprintf( fp, "\n");
                break;
            }
            case wkbCompoundCurve:
            case wkbCompoundCurveZ:
            case wkbCompoundCurveM:
            case wkbCompoundCurveZM:
            {
                OGRCompoundCurve* poCC = dynamic_cast<OGRCompoundCurve *>(
                    const_cast<OGRGeometry *>(this));
                if( poCC == NULL )
                {
                    CPLError(
                        CE_Fatal, CPLE_AppDefined,
                        "dynamic_cast failed.  Expected OGRCompoundCurve.");
                    return;
                }
                if( poCC->getNumCurves() == 0 )
                {
                    fprintf( fp, "empty");
                }
                else
                {
                    for( int i = 0; i < poCC->getNumCurves(); i++ )
                    {
                        if( i )
                            fprintf( fp, ", ");
                        fprintf( fp, "%s (%d points)",
                                 poCC->getCurve(i)->getGeometryName(),
                                 poCC->getCurve(i)->getNumPoints() );
                    }
                }
                break;
            }

            case wkbMultiPoint:
            case wkbMultiLineString:
            case wkbMultiPolygon:
            case wkbMultiCurve:
            case wkbMultiSurface:
            case wkbGeometryCollection:
            case wkbMultiPoint25D:
            case wkbMultiLineString25D:
            case wkbMultiPolygon25D:
            case wkbMultiCurveZ:
            case wkbMultiSurfaceZ:
            case wkbGeometryCollection25D:
            case wkbMultiPointM:
            case wkbMultiLineStringM:
            case wkbMultiPolygonM:
            case wkbMultiCurveM:
            case wkbMultiSurfaceM:
            case wkbGeometryCollectionM:
            case wkbMultiPointZM:
            case wkbMultiLineStringZM:
            case wkbMultiPolygonZM:
            case wkbMultiCurveZM:
            case wkbMultiSurfaceZM:
            case wkbGeometryCollectionZM:
            {
                OGRGeometryCollection *poColl =
                    dynamic_cast<OGRGeometryCollection *>(
                        const_cast<OGRGeometry *>(this));
                if( poColl == NULL )
                {
                    CPLError(CE_Fatal, CPLE_AppDefined,
                             "dynamic_cast failed.  "
                             "Expected OGRGeometryCollection.");
                    return;
                }
                fprintf( fp, "%d geometries:\n", poColl->getNumGeometries() );
                for( int ig = 0; ig < poColl->getNumGeometries(); ig++ )
                {
                    OGRGeometry *poChild = poColl->getGeometryRef(ig);
                    fprintf( fp, "%s", pszPrefix);
                    poChild->dumpReadable( fp, pszPrefix, papszOptions );
                }
                break;
            }
            case wkbLinearRing:
            case wkbCurve:
            case wkbSurface:
            case wkbCurveZ:
            case wkbSurfaceZ:
            case wkbCurveM:
            case wkbSurfaceM:
            case wkbCurveZM:
            case wkbSurfaceZM:
                break;
        }
    }
    else if( pszDisplayGeometry != NULL && EQUAL(pszDisplayGeometry, "WKT") )
    {
        char *pszWkt = NULL;
        if( exportToWkt( &pszWkt ) == OGRERR_NONE )
        {
            fprintf( fp, "%s%s\n", pszPrefix, pszWkt );
            CPLFree( pszWkt );
        }
    }
    else if( pszDisplayGeometry == NULL ||
             CPLTestBool(pszDisplayGeometry) ||
             EQUAL(pszDisplayGeometry, "ISO_WKT") )
    {
        char *pszWkt = NULL;
        if( exportToWkt( &pszWkt, wkbVariantIso ) == OGRERR_NONE )
        {
            fprintf( fp, "%s%s\n", pszPrefix, pszWkt );
            CPLFree( pszWkt );
        }
    }
}

/************************************************************************/
/*                         OGR_G_DumpReadable()                         */
/************************************************************************/
/**
 * \brief Dump geometry in well known text format to indicated output file.
 *
 * This method is the same as the CPP method OGRGeometry::dumpReadable.
 *
 * @param hGeom handle on the geometry to dump.
 * @param fp the text file to write the geometry to.
 * @param pszPrefix the prefix to put on each line of output.
 */

void OGR_G_DumpReadable( OGRGeometryH hGeom, FILE *fp, const char *pszPrefix )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_DumpReadable" );

    reinterpret_cast<OGRGeometry *>(hGeom)->dumpReadable( fp, pszPrefix );
}

/************************************************************************/
/*                       assignSpatialReference()                       */
/************************************************************************/

/**
 * \fn void OGRGeometry::assignSpatialReference( OGRSpatialReference * poSR );
 *
 * \brief Assign spatial reference to this object.
 *
 * Any existing spatial reference
 * is replaced, but under no circumstances does this result in the object
 * being reprojected.  It is just changing the interpretation of the existing
 * geometry.  Note that assigning a spatial reference increments the
 * reference count on the OGRSpatialReference, but does not copy it.
 *
 * This is similar to the SFCOM IGeometry::put_SpatialReference() method.
 *
 * This method is the same as the C function OGR_G_AssignSpatialReference().
 *
 * @param poSR new spatial reference system to apply.
 */

void OGRGeometry::assignSpatialReference( OGRSpatialReference * poSR )

{
    if( poSRS != NULL )
        poSRS->Release();

    poSRS = poSR;
    if( poSRS != NULL )
        poSRS->Reference();
}

/************************************************************************/
/*                    OGR_G_AssignSpatialReference()                    */
/************************************************************************/
/**
 * \brief Assign spatial reference to this object.
 *
 * Any existing spatial reference
 * is replaced, but under no circumstances does this result in the object
 * being reprojected.  It is just changing the interpretation of the existing
 * geometry.  Note that assigning a spatial reference increments the
 * reference count on the OGRSpatialReference, but does not copy it.
 *
 * This is similar to the SFCOM IGeometry::put_SpatialReference() method.
 *
 * This function is the same as the CPP method
 * OGRGeometry::assignSpatialReference.
 *
 * @param hGeom handle on the geometry to apply the new spatial reference
 * system.
 * @param hSRS handle on the new spatial reference system to apply.
 */

void OGR_G_AssignSpatialReference( OGRGeometryH hGeom,
                                   OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_AssignSpatialReference" );

    reinterpret_cast<OGRGeometry *>(hGeom)->
        assignSpatialReference(reinterpret_cast<OGRSpatialReference *>(hSRS));
}

/************************************************************************/
/*                             Intersects()                             */
/************************************************************************/

/**
 * \brief Do these features intersect?
 *
 * Determines whether two geometries intersect.  If GEOS is enabled, then
 * this is done in rigorous fashion otherwise TRUE is returned if the
 * envelopes (bounding boxes) of the two geometries overlap.
 *
 * The poOtherGeom argument may be safely NULL, but in this case the method
 * will always return TRUE.   That is, a NULL geometry is treated as being
 * everywhere.
 *
 * This method is the same as the C function OGR_G_Intersects().
 *
 * @param poOtherGeom the other geometry to test against.
 *
 * @return TRUE if the geometries intersect, otherwise FALSE.
 */

OGRBoolean OGRGeometry::Intersects( const OGRGeometry *poOtherGeom ) const

{
    if( poOtherGeom == NULL )
        return TRUE;

    OGREnvelope oEnv1;
    getEnvelope( &oEnv1 );

    OGREnvelope oEnv2;
    poOtherGeom->getEnvelope( &oEnv2 );

    if( oEnv1.MaxX < oEnv2.MinX
        || oEnv1.MaxY < oEnv2.MinY
        || oEnv2.MaxX < oEnv1.MinX
        || oEnv2.MaxY < oEnv1.MinY )
        return FALSE;

#ifndef HAVE_GEOS
    // Without GEOS we assume that envelope overlap is equivalent to
    // actual intersection.
    return TRUE;
#else


    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    GEOSGeom hThisGeosGeom  = exportToGEOS(hGEOSCtxt);
    GEOSGeom hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);

    OGRBoolean bResult = FALSE;
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult =
            GEOSIntersects_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom ) != 0;
    }

    GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
    GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
    freeGEOSContext( hGEOSCtxt );

    return bResult;
#endif  // HAVE_GEOS
}

// Old API compatibility function.

//! @cond Doxygen_Suppress
OGRBoolean OGRGeometry::Intersect( OGRGeometry *poOtherGeom ) const

{
    return Intersects( poOtherGeom );
}
//! @endcond

/************************************************************************/
/*                          OGR_G_Intersects()                          */
/************************************************************************/
/**
 * \brief Do these features intersect?
 *
 * Determines whether two geometries intersect.  If GEOS is enabled, then
 * this is done in rigorous fashion otherwise TRUE is returned if the
 * envelopes (bounding boxes) of the two geometries overlap.
 *
 * This function is the same as the CPP method OGRGeometry::Intersects.
 *
 * @param hGeom handle on the first geometry.
 * @param hOtherGeom handle on the other geometry to test against.
 *
 * @return TRUE if the geometries intersect, otherwise FALSE.
 */

int OGR_G_Intersects( OGRGeometryH hGeom, OGRGeometryH hOtherGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Intersects", FALSE );
    VALIDATE_POINTER1( hOtherGeom, "OGR_G_Intersects", FALSE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        Intersects( reinterpret_cast<OGRGeometry *>(hOtherGeom) );
}

//! @cond Doxygen_Suppress
int OGR_G_Intersect( OGRGeometryH hGeom, OGRGeometryH hOtherGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Intersect", FALSE );
    VALIDATE_POINTER1( hOtherGeom, "OGR_G_Intersect", FALSE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        Intersects( reinterpret_cast<OGRGeometry *>(hOtherGeom));
}
//! @endcond

/************************************************************************/
/*                            transformTo()                             */
/************************************************************************/

/**
 * \brief Transform geometry to new spatial reference system.
 *
 * This method will transform the coordinates of a geometry from
 * their current spatial reference system to a new target spatial
 * reference system.  Normally this means reprojecting the vectors,
 * but it could include datum shifts, and changes of units.
 *
 * This method will only work if the geometry already has an assigned
 * spatial reference system, and if it is transformable to the target
 * coordinate system.
 *
 * Because this method requires internal creation and initialization of an
 * OGRCoordinateTransformation object it is significantly more expensive to
 * use this method to transform many geometries than it is to create the
 * OGRCoordinateTransformation in advance, and call transform() with that
 * transformation.  This method exists primarily for convenience when only
 * transforming a single geometry.
 *
 * This method is the same as the C function OGR_G_TransformTo().
 *
 * @param poSR spatial reference system to transform to.
 *
 * @return OGRERR_NONE on success, or an error code.
 */

OGRErr OGRGeometry::transformTo( OGRSpatialReference *poSR )

{
    if( getSpatialReference() == NULL || poSR == NULL )
        return OGRERR_FAILURE;

    OGRCoordinateTransformation *poCT =
        OGRCreateCoordinateTransformation( getSpatialReference(), poSR );
    if( poCT == NULL )
        return OGRERR_FAILURE;

    const OGRErr eErr = transform( poCT );

    delete poCT;

    return eErr;
}

/************************************************************************/
/*                         OGR_G_TransformTo()                          */
/************************************************************************/
/**
 * \brief Transform geometry to new spatial reference system.
 *
 * This function will transform the coordinates of a geometry from
 * their current spatial reference system to a new target spatial
 * reference system.  Normally this means reprojecting the vectors,
 * but it could include datum shifts, and changes of units.
 *
 * This function will only work if the geometry already has an assigned
 * spatial reference system, and if it is transformable to the target
 * coordinate system.
 *
 * Because this function requires internal creation and initialization of an
 * OGRCoordinateTransformation object it is significantly more expensive to
 * use this function to transform many geometries than it is to create the
 * OGRCoordinateTransformation in advance, and call transform() with that
 * transformation.  This function exists primarily for convenience when only
 * transforming a single geometry.
 *
 * This function is the same as the CPP method OGRGeometry::transformTo.
 *
 * @param hGeom handle on the geometry to apply the transform to.
 * @param hSRS handle on the spatial reference system to apply.
 *
 * @return OGRERR_NONE on success, or an error code.
 */

OGRErr OGR_G_TransformTo( OGRGeometryH hGeom, OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_TransformTo", OGRERR_FAILURE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        transformTo(reinterpret_cast<OGRSpatialReference *>(hSRS));
}

/**
 * \fn OGRErr OGRGeometry::transform( OGRCoordinateTransformation *poCT );
 *
 * \brief Apply arbitrary coordinate transformation to geometry.
 *
 * This method will transform the coordinates of a geometry from
 * their current spatial reference system to a new target spatial
 * reference system.  Normally this means reprojecting the vectors,
 * but it could include datum shifts, and changes of units.
 *
 * Note that this method does not require that the geometry already
 * have a spatial reference system.  It will be assumed that they can
 * be treated as having the source spatial reference system of the
 * OGRCoordinateTransformation object, and the actual SRS of the geometry
 * will be ignored.  On successful completion the output OGRSpatialReference
 * of the OGRCoordinateTransformation will be assigned to the geometry.
 *
 * This method is the same as the C function OGR_G_Transform().
 *
 * @param poCT the transformation to apply.
 *
 * @return OGRERR_NONE on success or an error code.
 */

/************************************************************************/
/*                          OGR_G_Transform()                           */
/************************************************************************/
/**
 * \brief Apply arbitrary coordinate transformation to geometry.
 *
 * This function will transform the coordinates of a geometry from
 * their current spatial reference system to a new target spatial
 * reference system.  Normally this means reprojecting the vectors,
 * but it could include datum shifts, and changes of units.
 *
 * Note that this function does not require that the geometry already
 * have a spatial reference system.  It will be assumed that they can
 * be treated as having the source spatial reference system of the
 * OGRCoordinateTransformation object, and the actual SRS of the geometry
 * will be ignored.  On successful completion the output OGRSpatialReference
 * of the OGRCoordinateTransformation will be assigned to the geometry.
 *
 * This function is the same as the CPP method OGRGeometry::transform.
 *
 * @param hGeom handle on the geometry to apply the transform to.
 * @param hTransform handle on the transformation to apply.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr OGR_G_Transform( OGRGeometryH hGeom,
                        OGRCoordinateTransformationH hTransform )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Transform", OGRERR_FAILURE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        transform(reinterpret_cast<OGRCoordinateTransformation *>(hTransform));
}

/**
 * \fn int OGRGeometry::getDimension() const;
 *
 * \brief Get the dimension of this object.
 *
 * This method corresponds to the SFCOM IGeometry::GetDimension() method.
 * It indicates the dimension of the object, but does not indicate the
 * dimension of the underlying space (as indicated by
 * OGRGeometry::getCoordinateDimension()).
 *
 * This method is the same as the C function OGR_G_GetDimension().
 *
 * @return 0 for points, 1 for lines and 2 for surfaces.
 */

/**
 * \brief Get the geometry type that conforms with ISO SQL/MM Part3
 *
 * @return the geometry type that conforms with ISO SQL/MM Part3
 */
OGRwkbGeometryType OGRGeometry::getIsoGeometryType() const
{
    OGRwkbGeometryType nGType = wkbFlatten(getGeometryType());

    if( flags & OGR_G_3D )
        nGType = static_cast<OGRwkbGeometryType>(nGType + 1000);
    if( flags & OGR_G_MEASURED )
        nGType = static_cast<OGRwkbGeometryType>(nGType + 2000);

    return nGType;
}

/************************************************************************/
/*                  OGRGeometry::segmentize()                           */
/************************************************************************/
/**
 *
 * \brief Modify the geometry such it has no segment longer then the
 * given distance.
 *
 * Interpolated points will have Z and M values (if needed) set to 0.
 * Distance computation is performed in 2d only
 *
 * This function is the same as the C function OGR_G_Segmentize()
 *
 * @param dfMaxLength the maximum distance between 2 points after segmentization
 */

void OGRGeometry::segmentize( CPL_UNUSED double dfMaxLength )
{
    // Do nothing.
}

/************************************************************************/
/*                         OGR_G_Segmentize()                           */
/************************************************************************/

/**
 *
 * \brief Modify the geometry such it has no segment longer then the given
 * distance.
 *
 * Interpolated points will have Z and M values (if needed) set to 0.
 * Distance computation is performed in 2d only.
 *
 * This function is the same as the CPP method OGRGeometry::segmentize().
 *
 * @param hGeom handle on the geometry to segmentize
 * @param dfMaxLength the maximum distance between 2 points after segmentization
 */

void CPL_DLL OGR_G_Segmentize( OGRGeometryH hGeom, double dfMaxLength )
{
    VALIDATE_POINTER0( hGeom, "OGR_G_Segmentize" );

    if( dfMaxLength <= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfMaxLength must be strictly positive");
        return;
    }
    reinterpret_cast<OGRGeometry *>(hGeom)->segmentize( dfMaxLength );
}

/************************************************************************/
/*                         OGR_G_GetDimension()                         */
/************************************************************************/
/**
 *
 * \brief Get the dimension of this geometry.
 *
 * This function corresponds to the SFCOM IGeometry::GetDimension() method.
 * It indicates the dimension of the geometry, but does not indicate the
 * dimension of the underlying space (as indicated by
 * OGR_G_GetCoordinateDimension() function).
 *
 * This function is the same as the CPP method OGRGeometry::getDimension().
 *
 * @param hGeom handle on the geometry to get the dimension from.
 * @return 0 for points, 1 for lines and 2 for surfaces.
 */

int OGR_G_GetDimension( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetDimension", 0 );

    return reinterpret_cast<OGRGeometry *>(hGeom)->getDimension();
}

/************************************************************************/
/*                       getCoordinateDimension()                       */
/************************************************************************/
/**
 * \brief Get the dimension of the coordinates in this object.
 *
 * This method is the same as the C function OGR_G_GetCoordinateDimension().
 *
 * @deprecated use CoordinateDimension().
 *
 * @return this will return 2 or 3.
 */

int OGRGeometry::getCoordinateDimension() const

{
    return (flags & OGR_G_3D) ? 3 : 2;
}

/************************************************************************/
/*                        CoordinateDimension()                         */
/************************************************************************/
/**
 * \brief Get the dimension of the coordinates in this object.
 *
 * This method is the same as the C function OGR_G_CoordinateDimension().
 *
 * @return this will return 2 for XY, 3 for XYZ and XYM, and 4 for XYZM data.
 *
 * @since GDAL 2.1
 */

int OGRGeometry::CoordinateDimension() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return 4;
    else if( (flags & OGR_G_3D) || (flags & OGR_G_MEASURED) )
        return 3;
    else
        return 2;
}

/************************************************************************/
/*                    OGR_G_GetCoordinateDimension()                    */
/************************************************************************/
/**
 *
 * \brief Get the dimension of the coordinates in this geometry.
 *
 * This function is the same as the CPP method
 * OGRGeometry::getCoordinateDimension().
 *
 * @param hGeom handle on the geometry to get the dimension of the
 * coordinates from.
 *
 * @deprecated use OGR_G_CoordinateDimension(), OGR_G_Is3D(), or
 * OGR_G_IsMeasured().
 *
 * @return this will return 2 or 3.
 */

int OGR_G_GetCoordinateDimension( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetCoordinateDimension", 0 );

    return reinterpret_cast<OGRGeometry *>(hGeom)->getCoordinateDimension();
}

/************************************************************************/
/*                    OGR_G_CoordinateDimension()                       */
/************************************************************************/
/**
 *
 * \brief Get the dimension of the coordinates in this geometry.
 *
 * This function is the same as the CPP method
 * OGRGeometry::CoordinateDimension().
 *
 * @param hGeom handle on the geometry to get the dimension of the
 * coordinates from.
 *
 * @return this will return 2 for XY, 3 for XYZ and XYM, and 4 for XYZM data.
 *
 * @since GDAL 2.1
 */

int OGR_G_CoordinateDimension( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_CoordinateDimension", 0 );

    return reinterpret_cast<OGRGeometry *>(hGeom)->CoordinateDimension();
}

/**
 *
 * \brief See whether this geometry has Z coordinates.
 *
 * This function is the same as the CPP method
 * OGRGeometry::Is3D().
 *
 * @param hGeom handle on the geometry to check whether it has Z coordinates.
 *
 * @return TRUE if the geometry has Z coordinates.
 * @since GDAL 2.1
 */

int OGR_G_Is3D( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Is3D", 0 );

    return reinterpret_cast<OGRGeometry *>(hGeom)->Is3D();
}

/**
 *
 * \brief See whether this geometry is measured.
 *
 * This function is the same as the CPP method
 * OGRGeometry::IsMeasured().
 *
 * @param hGeom handle on the geometry to check whether it is measured.
 *
 * @return TRUE if the geometry has M coordinates.
 * @since GDAL 2.1
 */

int OGR_G_IsMeasured( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_IsMeasured", 0 );

    return reinterpret_cast<OGRGeometry *>(hGeom)->IsMeasured();
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

/**
 * \brief Set the coordinate dimension.
 *
 * This method sets the explicit coordinate dimension.  Setting the coordinate
 * dimension of a geometry to 2 should zero out any existing Z values.  Setting
 * the dimension of a geometry collection, a compound curve, a polygon, etc.
 * will affect the children geometries.
 * This will also remove the M dimension if present before this call.
 *
 * @deprecated use set3D() or setMeasured().
 *
 * @param nNewDimension New coordinate dimension value, either 2 or 3.
 */

void OGRGeometry::setCoordinateDimension( int nNewDimension )

{
    if( nNewDimension == 2 )
        flags &= ~OGR_G_3D;
    else
        flags |= OGR_G_3D;
    setMeasured( FALSE );
}

/**
 * \brief Add or remove the Z coordinate dimension.
 *
 * This method adds or removes the explicit Z coordinate dimension.
 * Removing the Z coordinate dimension of a geometry will remove any
 * existing Z values.  Adding the Z dimension to a geometry
 * collection, a compound curve, a polygon, etc.  will affect the
 * children geometries.
 *
 * @param bIs3D Should the geometry have a Z dimension, either TRUE or FALSE.
 * @since GDAL 2.1
 */

void OGRGeometry::set3D( OGRBoolean bIs3D )

{
    if( bIs3D )
        flags |= OGR_G_3D;
    else
        flags &= ~OGR_G_3D;
}

/**
 * \brief Add or remove the M coordinate dimension.
 *
 * This method adds or removes the explicit M coordinate dimension.
 * Removing the M coordinate dimension of a geometry will remove any
 * existing M values.  Adding the M dimension to a geometry
 * collection, a compound curve, a polygon, etc.  will affect the
 * children geometries.
 *
 * @param bIsMeasured Should the geometry have a M dimension, either
 * TRUE or FALSE.
 * @since GDAL 2.1
 */

void OGRGeometry::setMeasured( OGRBoolean bIsMeasured )

{
    if( bIsMeasured )
        flags |= OGR_G_MEASURED;
    else
        flags &= ~OGR_G_MEASURED;
}

/************************************************************************/
/*                    OGR_G_SetCoordinateDimension()                    */
/************************************************************************/

/**
 * \brief Set the coordinate dimension.
 *
 * This method sets the explicit coordinate dimension.  Setting the coordinate
 * dimension of a geometry to 2 should zero out any existing Z values. Setting
 * the dimension of a geometry collection, a compound curve, a polygon, etc.
 * will affect the children geometries.
 * This will also remove the M dimension if present before this call.
 *
 * @deprecated use OGR_G_Set3D() or OGR_G_SetMeasured().
 *
 * @param hGeom handle on the geometry to set the dimension of the
 * coordinates.
 * @param nNewDimension New coordinate dimension value, either 2 or 3.
 */

void OGR_G_SetCoordinateDimension( OGRGeometryH hGeom, int nNewDimension)

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetCoordinateDimension" );

    reinterpret_cast<OGRGeometry *>(hGeom)->
        setCoordinateDimension(nNewDimension);
}

/************************************************************************/
/*                    OGR_G_Set3D()                                     */
/************************************************************************/

/**
 * \brief Add or remove the Z coordinate dimension.
 *
 * This method adds or removes the explicit Z coordinate dimension.
 * Removing the Z coordinate dimension of a geometry will remove any
 * existing Z values.  Adding the Z dimension to a geometry
 * collection, a compound curve, a polygon, etc.  will affect the
 * children geometries.
 *
 * @param hGeom handle on the geometry to set or unset the Z dimension.
 * @param bIs3D Should the geometry have a Z dimension, either TRUE or FALSE.
 * @since GDAL 2.1
 */

void OGR_G_Set3D( OGRGeometryH hGeom, int bIs3D)

{
    VALIDATE_POINTER0( hGeom, "OGR_G_Set3D" );

    reinterpret_cast<OGRGeometry *>(hGeom)->set3D(bIs3D);
}

/************************************************************************/
/*                    OGR_G_SetMeasured()                               */
/************************************************************************/

/**
 * \brief Add or remove the M coordinate dimension.
 *
 * This method adds or removes the explicit M coordinate dimension.
 * Removing the M coordinate dimension of a geometry will remove any
 * existing M values.  Adding the M dimension to a geometry
 * collection, a compound curve, a polygon, etc.  will affect the
 * children geometries.
 *
 * @param hGeom handle on the geometry to set or unset the M dimension.
 * @param bIsMeasured Should the geometry have a M dimension, either
 * TRUE or FALSE.
 * @since GDAL 2.1
 */

void OGR_G_SetMeasured( OGRGeometryH hGeom, int bIsMeasured )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetMeasured" );

    reinterpret_cast<OGRGeometry *>(hGeom)->setMeasured(bIsMeasured);
}

/**
 * \fn int OGRGeometry::Equals( OGRGeometry *poOtherGeom ) const;
 *
 * \brief Returns TRUE if two geometries are equivalent.
 *
 * This operation implements the SQL/MM ST_OrderingEquals() operation.
 *
 * The comparison is done in a structural way, that is to say that the geometry
 * types must be identical, as well as the number and ordering of sub-geometries
 * and vertices.
 * Or equivalently, two geometries are considered equal by this method if their
 * WKT/WKB representation is equal.
 * Note: this must be distinguished for equality in a spatial way (which is
 * the purpose of the ST_Equals() operation).
 *
 * This method is the same as the C function OGR_G_Equals().
 *
 * @return TRUE if equivalent or FALSE otherwise.
 */

// Backward compatibility method.

//! @cond Doxygen_Suppress
int OGRGeometry::Equal( OGRGeometry *poOtherGeom ) const
{
    return Equals( poOtherGeom );
}
//! @endcond

/************************************************************************/
/*                            OGR_G_Equals()                            */
/************************************************************************/

/**
 * \brief Returns TRUE if two geometries are equivalent.
 *
 * This operation implements the SQL/MM ST_OrderingEquals() operation.
 *
 * The comparison is done in a structural way, that is to say that the geometry
 * types must be identical, as well as the number and ordering of sub-geometries
 * and vertices.
 * Or equivalently, two geometries are considered equal by this method if their
 * WKT/WKB representation is equal.
 * Note: this must be distinguished for equality in a spatial way (which is
 * the purpose of the ST_Equals() operation).
 *
 * This function is the same as the CPP method OGRGeometry::Equals() method.
 *
 * @param hGeom handle on the first geometry.
 * @param hOther handle on the other geometry to test against.
 * @return TRUE if equivalent or FALSE otherwise.
 */

int OGR_G_Equals( OGRGeometryH hGeom, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Equals", FALSE );

    if( hOther == NULL )
    {
        CPLError ( CE_Failure, CPLE_ObjectNull,
                   "hOther was NULL in OGR_G_Equals");
        return 0;
    }

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        Equals(reinterpret_cast<OGRGeometry *>(hOther));
}

//! @cond Doxygen_Suppress
int OGR_G_Equal( OGRGeometryH hGeom, OGRGeometryH hOther )

{
    if( hGeom == NULL )
    {
        CPLError(CE_Failure, CPLE_ObjectNull,
                  "hGeom was NULL in OGR_G_Equal");
        return 0;
    }

    if( hOther == NULL )
    {
        CPLError(CE_Failure, CPLE_ObjectNull,
                 "hOther was NULL in OGR_G_Equal");
        return 0;
    }

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        Equals(reinterpret_cast<OGRGeometry *>(hOther));
}
//! @endcond

/**
 * \fn int OGRGeometry::WkbSize() const;
 *
 * \brief Returns size of related binary representation.
 *
 * This method returns the exact number of bytes required to hold the
 * well known binary representation of this geometry object.  Its computation
 * may be slightly expensive for complex geometries.
 *
 * This method relates to the SFCOM IWks::WkbSize() method.
 *
 * This method is the same as the C function OGR_G_WkbSize().
 *
 * @return size of binary representation in bytes.
 */

/************************************************************************/
/*                           OGR_G_WkbSize()                            */
/************************************************************************/
/**
 * \brief Returns size of related binary representation.
 *
 * This function returns the exact number of bytes required to hold the
 * well known binary representation of this geometry object.  Its computation
 * may be slightly expensive for complex geometries.
 *
 * This function relates to the SFCOM IWks::WkbSize() method.
 *
 * This function is the same as the CPP method OGRGeometry::WkbSize().
 *
 * @param hGeom handle on the geometry to get the binary size from.
 * @return size of binary representation in bytes.
 */

int OGR_G_WkbSize( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_WkbSize", 0 );

    return reinterpret_cast<OGRGeometry *>(hGeom)->WkbSize();
}

/**
 * \fn void OGRGeometry::getEnvelope(OGREnvelope *psEnvelope) const;
 *
 * \brief Computes and returns the bounding envelope for this geometry
 * in the passed psEnvelope structure.
 *
 * This method is the same as the C function OGR_G_GetEnvelope().
 *
 * @param psEnvelope the structure in which to place the results.
 */

/************************************************************************/
/*                         OGR_G_GetEnvelope()                          */
/************************************************************************/
/**
 * \brief Computes and returns the bounding envelope for this geometry
 * in the passed psEnvelope structure.
 *
 * This function is the same as the CPP method OGRGeometry::getEnvelope().
 *
 * @param hGeom handle of the geometry to get envelope from.
 * @param psEnvelope the structure in which to place the results.
 */

void OGR_G_GetEnvelope( OGRGeometryH hGeom, OGREnvelope *psEnvelope )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_GetEnvelope" );

    reinterpret_cast<OGRGeometry *>(hGeom)->getEnvelope( psEnvelope );
}

/**
 * \fn void OGRGeometry::getEnvelope(OGREnvelope3D *psEnvelope) const;
 *
 * \brief Computes and returns the bounding envelope (3D) for this
 * geometry in the passed psEnvelope structure.
 *
 * This method is the same as the C function OGR_G_GetEnvelope3D().
 *
 * @param psEnvelope the structure in which to place the results.
 *
 * @since OGR 1.9.0
 */

/************************************************************************/
/*                        OGR_G_GetEnvelope3D()                         */
/************************************************************************/
/**
 * \brief Computes and returns the bounding envelope (3D) for this
 * geometry in the passed psEnvelope structure.
 *
 * This function is the same as the CPP method OGRGeometry::getEnvelope().
 *
 * @param hGeom handle of the geometry to get envelope from.
 * @param psEnvelope the structure in which to place the results.
 *
 * @since OGR 1.9.0
 */

void OGR_G_GetEnvelope3D( OGRGeometryH hGeom, OGREnvelope3D *psEnvelope )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_GetEnvelope3D" );

    reinterpret_cast<OGRGeometry *>(hGeom)->getEnvelope( psEnvelope );
}


/************************************************************************/
/*                        importFromWkb()                               */
/************************************************************************/

/**
 * \brief Assign geometry from well known binary data.
 *
 * The object must have already been instantiated as the correct derived
 * type of geometry object to match the binaries type.  This method is used
 * by the OGRGeometryFactory class, but not normally called by application
 * code.
 *
 * This method relates to the SFCOM IWks::ImportFromWKB() method.
 *
 * This method is the same as the C function OGR_G_ImportFromWkb().
 *
 * @param pabyData the binary input data.
 * @param nSize the size of pabyData in bytes, or -1 if not known.
 * @param eWkbVariant if wkbVariantPostGIS1, special interpretation is
 * done for curve geometries code
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGRGeometry::importFromWkb( unsigned char * pabyData,
                                   int nSize, OGRwkbVariant eWkbVariant )
{
    int nBytesConsumedOutIgnored = -1;
    return importFromWkb( const_cast<const unsigned char*>(pabyData),
                          nSize, eWkbVariant, nBytesConsumedOutIgnored );
}

/**
 * \fn OGRErr OGRGeometry::importFromWkb( const unsigned char * pabyData,
 * int nSize, OGRwkbVariant eWkbVariant, int& nBytesConsumedOut );
 *
 * \brief Assign geometry from well known binary data.
 *
 * The object must have already been instantiated as the correct derived
 * type of geometry object to match the binaries type.  This method is used
 * by the OGRGeometryFactory class, but not normally called by application
 * code.
 *
 * This method relates to the SFCOM IWks::ImportFromWKB() method.
 *
 * This method is the same as the C function OGR_G_ImportFromWkb().
 *
 * @param pabyData the binary input data.
 * @param nSize the size of pabyData in bytes, or -1 if not known.
 * @param eWkbVariant if wkbVariantPostGIS1, special interpretation is
 * done for curve geometries code
 * @param nBytesConsumedOut output parameter. Number of bytes consumed.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 * 
 * @since GDAL 2.3
 */

/************************************************************************/
/*                        OGR_G_ImportFromWkb()                         */
/************************************************************************/
/**
 * \brief Assign geometry from well known binary data.
 *
 * The object must have already been instantiated as the correct derived
 * type of geometry object to match the binaries type.
 *
 * This function relates to the SFCOM IWks::ImportFromWKB() method.
 *
 * This function is the same as the CPP method OGRGeometry::importFromWkb().
 *
 * @param hGeom handle on the geometry to assign the well know binary data to.
 * @param pabyData the binary input data.
 * @param nSize the size of pabyData in bytes, or -1 if not known.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGR_G_ImportFromWkb( OGRGeometryH hGeom,
                            unsigned char *pabyData, int nSize )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_ImportFromWkb", OGRERR_FAILURE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        importFromWkb( pabyData, nSize );
}

/**
 * \fn OGRErr OGRGeometry::exportToWkb( OGRwkbByteOrder eByteOrder,
                                        unsigned char * pabyData,
                                        OGRwkbVariant eWkbVariant=wkbVariantOldOgc ) const
 *
 * \brief Convert a geometry into well known binary format.
 *
 * This method relates to the SFCOM IWks::ExportToWKB() method.
 *
 * This method is the same as the C function OGR_G_ExportToWkb() or
 * OGR_G_ExportToIsoWkb(), depending on the value of eWkbVariant.
 *
 * @param eByteOrder One of wkbXDR or wkbNDR indicating MSB or LSB byte order
 *               respectively.
 * @param pabyData a buffer into which the binary representation is
 *                      written.  This buffer must be at least
 *                      OGRGeometry::WkbSize() byte in size.
 * @param eWkbVariant What standard to use when exporting geometries
 *                      with three dimensions (or more). The default
 *                      wkbVariantOldOgc is the historical OGR
 *                      variant. wkbVariantIso is the variant defined
 *                      in ISO SQL/MM and adopted by OGC for SFSQL
 *                      1.2.
 *
 * @return Currently OGRERR_NONE is always returned.
 */

/************************************************************************/
/*                         OGR_G_ExportToWkb()                          */
/************************************************************************/
/**
 * \brief Convert a geometry well known binary format
 *
 * This function relates to the SFCOM IWks::ExportToWKB() method.
 *
 * For backward compatibility purposes, it exports the Old-style 99-402
 * extended dimension (Z) WKB types for types Point, LineString, Polygon,
 * MultiPoint, MultiLineString, MultiPolygon and GeometryCollection.
 * For other geometry types, it is equivalent to OGR_G_ExportToIsoWkb().
 *
 * This function is the same as the CPP method
 * OGRGeometry::exportToWkb(OGRwkbByteOrder, unsigned char *,
 * OGRwkbVariant) with eWkbVariant = wkbVariantOldOgc.
 *
 * @param hGeom handle on the geometry to convert to a well know binary
 * data from.
 * @param eOrder One of wkbXDR or wkbNDR indicating MSB or LSB byte order
 *               respectively.
 * @param pabyDstBuffer a buffer into which the binary representation is
 *                      written.  This buffer must be at least
 *                      OGR_G_WkbSize() byte in size.
 *
 * @return Currently OGRERR_NONE is always returned.
 */

OGRErr OGR_G_ExportToWkb( OGRGeometryH hGeom, OGRwkbByteOrder eOrder,
                          unsigned char *pabyDstBuffer )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_ExportToWkb", OGRERR_FAILURE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        exportToWkb( eOrder, pabyDstBuffer );
}

/************************************************************************/
/*                        OGR_G_ExportToIsoWkb()                        */
/************************************************************************/
/**
 * \brief Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well known
 * binary format
 *
 * This function relates to the SFCOM IWks::ExportToWKB() method.
 * It exports the SFSQL 1.2 and ISO SQL/MM Part 3 extended dimension (Z&M) WKB
 * types.
 *
 * This function is the same as the CPP method
 * OGRGeometry::exportToWkb(OGRwkbByteOrder, unsigned char *, OGRwkbVariant)
 * with eWkbVariant = wkbVariantIso.
 *
 * @param hGeom handle on the geometry to convert to a well know binary
 * data from.
 * @param eOrder One of wkbXDR or wkbNDR indicating MSB or LSB byte order
 *               respectively.
 * @param pabyDstBuffer a buffer into which the binary representation is
 *                      written.  This buffer must be at least
 *                      OGR_G_WkbSize() byte in size.
 *
 * @return Currently OGRERR_NONE is always returned.
 *
 * @since GDAL 2.0
 */

OGRErr OGR_G_ExportToIsoWkb( OGRGeometryH hGeom, OGRwkbByteOrder eOrder,
                             unsigned char *pabyDstBuffer )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_ExportToIsoWkb", OGRERR_FAILURE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        exportToWkb( eOrder, pabyDstBuffer, wkbVariantIso );
}

/**
 * \fn OGRErr OGRGeometry::importFromWkt( char ** ppszInput );
 *
 * \brief Assign geometry from well known text data.
 *
 * The object must have already been instantiated as the correct derived
 * type of geometry object to match the text type.  This method is used
 * by the OGRGeometryFactory class, but not normally called by application
 * code.
 *
 * This method relates to the SFCOM IWks::ImportFromWKT() method.
 *
 * This method is the same as the C function OGR_G_ImportFromWkt().
 *
 * @param ppszInput pointer to a pointer to the source text.  The pointer is
 *                    updated to pointer after the consumed text.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

/************************************************************************/
/*                        OGR_G_ImportFromWkt()                         */
/************************************************************************/
/**
 * \brief Assign geometry from well known text data.
 *
 * The object must have already been instantiated as the correct derived
 * type of geometry object to match the text type.
 *
 * This function relates to the SFCOM IWks::ImportFromWKT() method.
 *
 * This function is the same as the CPP method OGRGeometry::importFromWkt().
 *
 * @param hGeom handle on the geometry to assign well know text data to.
 * @param ppszSrcText pointer to a pointer to the source text.  The pointer is
 *                    updated to pointer after the consumed text.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGR_G_ImportFromWkt( OGRGeometryH hGeom, char ** ppszSrcText )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_ImportFromWkt", OGRERR_FAILURE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->importFromWkt( ppszSrcText );
}

/************************************************************************/
/*                        importPreambuleFromWkt()                      */
/************************************************************************/

// Returns -1 if processing must continue.
//! @cond Doxygen_Suppress
OGRErr OGRGeometry::importPreambuleFromWkt( char ** ppszInput,
                                            int* pbHasZ, int* pbHasM,
                                            bool* pbIsEmpty )
{
    const char *pszInput = *ppszInput;

/* -------------------------------------------------------------------- */
/*      Clear existing Geoms.                                           */
/* -------------------------------------------------------------------- */
    empty();
    *pbIsEmpty = false;

/* -------------------------------------------------------------------- */
/*      Read and verify the type keyword, and ensure it matches the     */
/*      actual type of this container.                                  */
/* -------------------------------------------------------------------- */
    bool bHasM = false;
    bool bHasZ = false;
    bool bIsoWKT = true;

    char szToken[OGR_WKT_TOKEN_MAX] = {};
    pszInput = OGRWktReadToken( pszInput, szToken );
    if( szToken[0] != '\0' )
    {
        // Postgis EWKT: POINTM instead of POINT M.
        const size_t nTokenLen = strlen(szToken);
        if( szToken[nTokenLen-1] == 'M' )
        {
            szToken[nTokenLen-1] = '\0';
            bHasM = true;
            bIsoWKT = false;
        }
    }

    if( !EQUAL(szToken, getGeometryName()) )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Check for EMPTY ...                                             */
/* -------------------------------------------------------------------- */
    const char *pszPreScan = OGRWktReadToken( pszInput, szToken );
    if( !bIsoWKT )
    {
        // Go on.
    }
    else if( EQUAL(szToken, "EMPTY") )
    {
        *ppszInput = const_cast<char *>(pszPreScan);
        *pbIsEmpty = true;
        *pbHasM = bHasM;
        empty();
        return OGRERR_NONE;
    }
/* -------------------------------------------------------------------- */
/*      Check for Z, M or ZM. Will ignore the Measure                   */
/* -------------------------------------------------------------------- */
    else if( EQUAL(szToken, "Z") )
    {
        bHasZ = true;
    }
    else if( EQUAL(szToken, "M") )
    {
        bHasM = true;
    }
    else if( EQUAL(szToken, "ZM") )
    {
        bHasZ = true;
        bHasM = true;
    }
    *pbHasZ = bHasZ;
    *pbHasM = bHasM;

    if( bIsoWKT && (bHasZ || bHasM) )
    {
        pszInput = pszPreScan;
        pszPreScan = OGRWktReadToken( pszInput, szToken );
        if( EQUAL(szToken, "EMPTY") )
        {
            *ppszInput = const_cast<char *>(pszPreScan);
            empty();
            if( bHasZ )
                set3D(TRUE);
            if( bHasM )
                setMeasured(TRUE);
            *pbIsEmpty = true;
            return OGRERR_NONE;
        }
    }

    if( !EQUAL(szToken, "(") )
        return OGRERR_CORRUPT_DATA;

    if( !bHasZ && !bHasM )
    {
        // Test for old-style XXXXXXXXX(EMPTY).
        pszPreScan = OGRWktReadToken( pszPreScan, szToken );
        if( EQUAL(szToken, "EMPTY") )
        {
            pszPreScan = OGRWktReadToken( pszPreScan, szToken );

            if( EQUAL(szToken, ",") )
            {
                // This is OK according to SFSQL SPEC.
            }
            else if( !EQUAL(szToken, ")") )
            {
                return OGRERR_CORRUPT_DATA;
            }
            else
            {
                *ppszInput = const_cast<char *>(pszPreScan);
                empty();
                *pbIsEmpty = true;
                return OGRERR_NONE;
            }
        }
    }

    *ppszInput = const_cast<char *>(pszInput);

    return OGRERR_NONE;
}
//! @endcond

/**
 * \fn OGRErr OGRGeometry::exportToWkt( char ** ppszDstText,
 * OGRwkbVariant eWkbVariant = wkbVariantOldOgc ) const;
 *
 * \brief Convert a geometry into well known text format.
 *
 * This method relates to the SFCOM IWks::ExportToWKT() method.
 *
 * This method is the same as the C function OGR_G_ExportToWkt().
 *
 * @param ppszDstText a text buffer is allocated by the program, and assigned
 *                    to the passed pointer. After use, *ppszDstText should be
 *                    freed with CPLFree().
 * @param eWkbVariant the specification that must be conformed too :
 *                    - wbkVariantOgc for old-style 99-402 extended
 *                      dimension (Z) WKB types
 *                    - wbkVariantIso for SFSQL 1.2 and ISO SQL/MM Part 3
 *
 * @return Currently OGRERR_NONE is always returned.
 */

/************************************************************************/
/*                         OGR_G_ExportToWkt()                          */
/************************************************************************/

/**
 * \brief Convert a geometry into well known text format.
 *
 * This function relates to the SFCOM IWks::ExportToWKT() method.
 *
 * For backward compatibility purposes, it exports the Old-style 99-402
 * extended dimension (Z) WKB types for types Point, LineString, Polygon,
 * MultiPoint, MultiLineString, MultiPolygon and GeometryCollection.
 * For other geometry types, it is equivalent to OGR_G_ExportToIsoWkt().
 *
 * This function is the same as the CPP method OGRGeometry::exportToWkt().
 *
 * @param hGeom handle on the geometry to convert to a text format from.
 * @param ppszSrcText a text buffer is allocated by the program, and assigned
 *                    to the passed pointer. After use, *ppszDstText should be
 *                    freed with CPLFree().
 *
 * @return Currently OGRERR_NONE is always returned.
 */

OGRErr OGR_G_ExportToWkt( OGRGeometryH hGeom, char **ppszSrcText )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_ExportToWkt", OGRERR_FAILURE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->exportToWkt( ppszSrcText );
}

/************************************************************************/
/*                      OGR_G_ExportToIsoWkt()                          */
/************************************************************************/

/**
 * \brief Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well
 * known text format.
 *
 * This function relates to the SFCOM IWks::ExportToWKT() method.
 * It exports the SFSQL 1.2 and ISO SQL/MM Part 3 extended dimension
 * (Z&M) WKB types.
 *
 * This function is the same as the CPP method
 * OGRGeometry::exportToWkt(wkbVariantIso).
 *
 * @param hGeom handle on the geometry to convert to a text format from.
 * @param ppszSrcText a text buffer is allocated by the program, and assigned
 *                    to the passed pointer. After use, *ppszDstText should be
 *                    freed with CPLFree().
 *
 * @return Currently OGRERR_NONE is always returned.
 *
 * @since GDAL 2.0
 */

OGRErr OGR_G_ExportToIsoWkt( OGRGeometryH hGeom, char **ppszSrcText )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_ExportToIsoWkt", OGRERR_FAILURE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->
        exportToWkt( ppszSrcText, wkbVariantIso );
}

/**
 * \fn OGRwkbGeometryType OGRGeometry::getGeometryType() const;
 *
 * \brief Fetch geometry type.
 *
 * Note that the geometry type may include the 2.5D flag.  To get a 2D
 * flattened version of the geometry type apply the wkbFlatten() macro
 * to the return result.
 *
 * This method is the same as the C function OGR_G_GetGeometryType().
 *
 * @return the geometry type code.
 */

/************************************************************************/
/*                       OGR_G_GetGeometryType()                        */
/************************************************************************/
/**
 * \brief Fetch geometry type.
 *
 * Note that the geometry type may include the 2.5D flag.  To get a 2D
 * flattened version of the geometry type apply the wkbFlatten() macro
 * to the return result.
 *
 * This function is the same as the CPP method OGRGeometry::getGeometryType().
 *
 * @param hGeom handle on the geometry to get type from.
 * @return the geometry type code.
 */

OGRwkbGeometryType OGR_G_GetGeometryType( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetGeometryType", wkbUnknown );

    return reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryType();
}

/**
 * \fn const char * OGRGeometry::getGeometryName() const;
 *
 * \brief Fetch WKT name for geometry type.
 *
 * There is no SFCOM analog to this method.
 *
 * This method is the same as the C function OGR_G_GetGeometryName().
 *
 * @return name used for this geometry type in well known text format.  The
 * returned pointer is to a static internal string and should not be modified
 * or freed.
 */

/************************************************************************/
/*                       OGR_G_GetGeometryName()                        */
/************************************************************************/
/**
 * \brief Fetch WKT name for geometry type.
 *
 * There is no SFCOM analog to this function.
 *
 * This function is the same as the CPP method OGRGeometry::getGeometryName().
 *
 * @param hGeom handle on the geometry to get name from.
 * @return name used for this geometry type in well known text format.
 */

const char *OGR_G_GetGeometryName( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetGeometryName", "" );

    return reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryName();
}

/**
 * \fn OGRGeometry *OGRGeometry::clone() const;
 *
 * \brief Make a copy of this object.
 *
 * This method relates to the SFCOM IGeometry::clone() method.
 *
 * This method is the same as the C function OGR_G_Clone().
 *
 * @return a new object instance with the same geometry, and spatial
 * reference system as the original.
 */

/************************************************************************/
/*                            OGR_G_Clone()                             */
/************************************************************************/
/**
 * \brief Make a copy of this object.
 *
 * This function relates to the SFCOM IGeometry::clone() method.
 *
 * This function is the same as the CPP method OGRGeometry::clone().
 *
 * @param hGeom handle on the geometry to clone from.
 * @return an handle on the copy of the geometry with the spatial
 * reference system as the original.
 */

OGRGeometryH OGR_G_Clone( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Clone", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hGeom)->clone());
}

/**
 * \fn OGRSpatialReference *OGRGeometry::getSpatialReference();
 *
 * \brief Returns spatial reference system for object.
 *
 * This method relates to the SFCOM IGeometry::get_SpatialReference() method.
 *
 * This method is the same as the C function OGR_G_GetSpatialReference().
 *
 * @return a reference to the spatial reference object.  The object may be
 * shared with many geometry objects, and should not be modified.
 */

/************************************************************************/
/*                     OGR_G_GetSpatialReference()                      */
/************************************************************************/
/**
 * \brief Returns spatial reference system for geometry.
 *
 * This function relates to the SFCOM IGeometry::get_SpatialReference() method.
 *
 * This function is the same as the CPP method
 * OGRGeometry::getSpatialReference().
 *
 * @param hGeom handle on the geometry to get spatial reference from.
 * @return a reference to the spatial reference geometry.
 */

OGRSpatialReferenceH OGR_G_GetSpatialReference( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetSpatialReference", NULL );

    return reinterpret_cast<OGRSpatialReferenceH>(
        reinterpret_cast<OGRGeometry *>(hGeom)->getSpatialReference());
}

/**
 * \fn void OGRGeometry::empty();
 *
 * \brief Clear geometry information.
 * This restores the geometry to it's initial
 * state after construction, and before assignment of actual geometry.
 *
 * This method relates to the SFCOM IGeometry::Empty() method.
 *
 * This method is the same as the C function OGR_G_Empty().
 */

/************************************************************************/
/*                            OGR_G_Empty()                             */
/************************************************************************/
/**
 * \brief Clear geometry information.
 * This restores the geometry to it's initial
 * state after construction, and before assignment of actual geometry.
 *
 * This function relates to the SFCOM IGeometry::Empty() method.
 *
 * This function is the same as the CPP method OGRGeometry::empty().
 *
 * @param hGeom handle on the geometry to empty.
 */

void OGR_G_Empty( OGRGeometryH hGeom )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_Empty" );

    reinterpret_cast<OGRGeometry *>(hGeom)->empty();
}

/**
 * \fn OGRBoolean OGRGeometry::IsEmpty() const;
 *
 * \brief Returns TRUE (non-zero) if the object has no points.
 *
 * Normally this
 * returns FALSE except between when an object is instantiated and points
 * have been assigned.
 *
 * This method relates to the SFCOM IGeometry::IsEmpty() method.
 *
 * @return TRUE if object is empty, otherwise FALSE.
 */

/************************************************************************/
/*                         OGR_G_IsEmpty()                              */
/************************************************************************/

/**
 * \brief Test if the geometry is empty.
 *
 * This method is the same as the CPP method OGRGeometry::IsEmpty().
 *
 * @param hGeom The Geometry to test.
 *
 * @return TRUE if the geometry has no points, otherwise FALSE.
 */

int OGR_G_IsEmpty( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_IsEmpty", TRUE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->IsEmpty();
}

/************************************************************************/
/*                              IsValid()                               */
/************************************************************************/

/**
 * \brief Test if the geometry is valid.
 *
 * This method is the same as the C function OGR_G_IsValid().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always return
 * FALSE.
 *
 *
 * @return TRUE if the geometry has no points, otherwise FALSE.
 */

OGRBoolean
OGRGeometry::IsValid() const

{
    if( IsSFCGALCompatible() )
    {
#ifndef HAVE_SFCGAL
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SFCGAL support not enabled." );
        return FALSE;
#else
        sfcgal_init();
        sfcgal_geometry_t *poThis =
            OGRGeometry::OGRexportToSFCGAL(const_cast<OGRGeometry *>(this));
        if (poThis == NULL)
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "SFCGAL geometry returned is NULL" );
            return FALSE;
        }

        const int res = sfcgal_geometry_is_valid(poThis);
        sfcgal_geometry_delete(poThis);
        return res == 1;
#endif
    }

    else
    {
#ifndef HAVE_GEOS

        return FALSE;

#else
        OGRBoolean bResult = FALSE;

        GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
        GEOSGeom hThisGeosGeom = exportToGEOS(hGEOSCtxt);

        if( hThisGeosGeom != NULL  )
        {
            bResult = GEOSisValid_r( hGEOSCtxt, hThisGeosGeom );
            GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
        }
        freeGEOSContext( hGEOSCtxt );

        return bResult;

#endif  // HAVE_GEOS
    }
}

/************************************************************************/
/*                           OGR_G_IsValid()                            */
/************************************************************************/

/**
 * \brief Test if the geometry is valid.
 *
 * This function is the same as the C++ method OGRGeometry::IsValid().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always return
 * FALSE.
 *
 * @param hGeom The Geometry to test.
 *
 * @return TRUE if the geometry has no points, otherwise FALSE.
 */

int OGR_G_IsValid( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_IsValid", FALSE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->IsValid();
}

/************************************************************************/
/*                              IsSimple()                               */
/************************************************************************/

/**
 * \brief Test if the geometry is simple.
 *
 * This method is the same as the C function OGR_G_IsSimple().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always return
 * FALSE.
 *
 *
 * @return TRUE if the geometry has no points, otherwise FALSE.
 */

OGRBoolean
OGRGeometry::IsSimple() const

{
#ifndef HAVE_GEOS

    return FALSE;

#else

    OGRBoolean bResult = FALSE;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    GEOSGeom hThisGeosGeom = exportToGEOS(hGEOSCtxt);

    if( hThisGeosGeom != NULL )
    {
        bResult = GEOSisSimple_r( hGEOSCtxt, hThisGeosGeom );
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
    }
    freeGEOSContext( hGEOSCtxt );

    return bResult;

#endif  // HAVE_GEOS
}

/**
 * \brief Returns TRUE if the geometry is simple.
 *
 * Returns TRUE if the geometry has no anomalous geometric points, such
 * as self intersection or self tangency. The description of each
 * instantiable geometric class will include the specific conditions that
 * cause an instance of that class to be classified as not simple.
 *
 * This function is the same as the C++ method OGRGeometry::IsSimple() method.
 *
 * If OGR is built without the GEOS library, this function will always return
 * FALSE.
 *
 * @param hGeom The Geometry to test.
 *
 * @return TRUE if object is simple, otherwise FALSE.
 */

int OGR_G_IsSimple( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_IsSimple", TRUE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->IsSimple();
}

/************************************************************************/
/*                              IsRing()                               */
/************************************************************************/

/**
 * \brief Test if the geometry is a ring
 *
 * This method is the same as the C function OGR_G_IsRing().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always return
 * FALSE.
 *
 *
 * @return TRUE if the geometry has no points, otherwise FALSE.
 */

OGRBoolean
OGRGeometry::IsRing() const

{
#ifndef HAVE_GEOS

    return FALSE;

#else

    OGRBoolean bResult = FALSE;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    GEOSGeom hThisGeosGeom = exportToGEOS(hGEOSCtxt);

    if( hThisGeosGeom != NULL )
    {
        bResult = GEOSisRing_r( hGEOSCtxt, hThisGeosGeom );
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
    }
    freeGEOSContext( hGEOSCtxt );

    return bResult;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                            OGR_G_IsRing()                            */
/************************************************************************/

/**
 * \brief Test if the geometry is a ring
 *
 * This function is the same as the C++ method OGRGeometry::IsRing().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always return
 * FALSE.
 *
 * @param hGeom The Geometry to test.
 *
 * @return TRUE if the geometry has no points, otherwise FALSE.
 */

int OGR_G_IsRing( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_IsRing", FALSE );

    return reinterpret_cast<OGRGeometry *>(hGeom)->IsRing();
}

/************************************************************************/
/*                     OGRFromOGCGeomType()                             */
/************************************************************************/

/** Map OGCgeometry format type to corresponding OGR constants.
 * @param pszGeomType POINT[ ][Z][M], LINESTRING[ ][Z][M], etc...
 * @return OGR constant.
 */
OGRwkbGeometryType OGRFromOGCGeomType( const char *pszGeomType )
{
    OGRwkbGeometryType eType = wkbUnknown;
    bool bConvertTo3D = false;
    bool bIsMeasured = false;
    if( *pszGeomType != '\0' )
    {
        char ch = pszGeomType[strlen(pszGeomType)-1];
        if( ch == 'm' || ch == 'M' )
        {
            bIsMeasured = true;
            if( strlen(pszGeomType) > 1 )
                ch = pszGeomType[strlen(pszGeomType)-2];
        }
        if( ch == 'z' || ch == 'Z' )
        {
            bConvertTo3D = true;
        }
    }

    if( STARTS_WITH_CI(pszGeomType, "POINT") )
        eType = wkbPoint;
    else if( STARTS_WITH_CI(pszGeomType, "LINESTRING") )
        eType = wkbLineString;
    else if( STARTS_WITH_CI(pszGeomType, "POLYGON") )
        eType = wkbPolygon;
    else if( STARTS_WITH_CI(pszGeomType, "MULTIPOINT") )
        eType = wkbMultiPoint;
    else if( STARTS_WITH_CI(pszGeomType, "MULTILINESTRING") )
        eType = wkbMultiLineString;
    else if( STARTS_WITH_CI(pszGeomType, "MULTIPOLYGON") )
        eType = wkbMultiPolygon;
    else if( STARTS_WITH_CI(pszGeomType, "GEOMETRYCOLLECTION") )
        eType = wkbGeometryCollection;
    else if( STARTS_WITH_CI(pszGeomType, "CIRCULARSTRING") )
        eType = wkbCircularString;
    else if( STARTS_WITH_CI(pszGeomType, "COMPOUNDCURVE") )
        eType = wkbCompoundCurve;
    else if( STARTS_WITH_CI(pszGeomType, "CURVEPOLYGON") )
        eType = wkbCurvePolygon;
    else if( STARTS_WITH_CI(pszGeomType, "MULTICURVE") )
        eType = wkbMultiCurve;
    else if( STARTS_WITH_CI(pszGeomType, "MULTISURFACE") )
        eType = wkbMultiSurface;
    else if ( STARTS_WITH_CI(pszGeomType, "TRIANGLE") )
        eType = wkbTriangle;
    else if ( STARTS_WITH_CI(pszGeomType, "POLYHEDRALSURFACE") )
        eType = wkbPolyhedralSurface;
    else if ( STARTS_WITH_CI(pszGeomType, "TIN") )
        eType = wkbTIN;
    else if ( STARTS_WITH_CI(pszGeomType, "CURVE") )
        eType = wkbCurve;
    else if( STARTS_WITH_CI(pszGeomType, "SURFACE") )
        eType = wkbSurface;
    else
        eType = wkbUnknown;

    if( bConvertTo3D )
        eType = wkbSetZ(eType);
    if( bIsMeasured )
        eType = wkbSetM(eType);

    return eType;
}

/************************************************************************/
/*                     OGRToOGCGeomType()                               */
/************************************************************************/

/** Map OGR geometry format constants to corresponding OGC geometry type.
 * @param eGeomType OGR geometry type
 * @return string with OGC geometry type (without dimensionality)
 */
const char * OGRToOGCGeomType( OGRwkbGeometryType eGeomType )
{
    switch( wkbFlatten(eGeomType) )
    {
        case wkbUnknown:
            return "GEOMETRY";
        case wkbPoint:
            return "POINT";
        case wkbLineString:
            return "LINESTRING";
        case wkbPolygon:
            return "POLYGON";
        case wkbMultiPoint:
            return "MULTIPOINT";
        case wkbMultiLineString:
            return "MULTILINESTRING";
        case wkbMultiPolygon:
            return "MULTIPOLYGON";
        case wkbGeometryCollection:
            return "GEOMETRYCOLLECTION";
        case wkbCircularString:
            return "CIRCULARSTRING";
        case wkbCompoundCurve:
            return "COMPOUNDCURVE";
        case wkbCurvePolygon:
            return "CURVEPOLYGON";
        case wkbMultiCurve:
            return "MULTICURVE";
        case wkbMultiSurface:
            return "MULTISURFACE";
        case wkbTriangle:
            return "TRIANGLE";
        case wkbPolyhedralSurface:
            return "POLYHEDRALSURFACE";
        case wkbTIN:
            return "TIN";
        case wkbCurve:
            return "CURVE";
        case wkbSurface:
            return "SURFACE";
        default:
            return "";
    }
}

/************************************************************************/
/*                       OGRGeometryTypeToName()                        */
/************************************************************************/

/**
 * \brief Fetch a human readable name corresponding to an OGRwkbGeometryType
 * value.  The returned value should not be modified, or freed by the
 * application.
 *
 * This function is C callable.
 *
 * @param eType the geometry type.
 *
 * @return internal human readable string, or NULL on failure.
 */

const char *OGRGeometryTypeToName( OGRwkbGeometryType eType )

{
    bool b3D = wkbHasZ(eType);
    bool bMeasured = wkbHasM(eType);

    switch( wkbFlatten(eType) )
    {
        case wkbUnknown:
            if( b3D && bMeasured )
                return "3D Measured Unknown (any)";
            else if( b3D )
                return "3D Unknown (any)";
            else if( bMeasured )
                return "Measured Unknown (any)";
            else
                return "Unknown (any)";

        case wkbPoint:
            if( b3D && bMeasured )
                return "3D Measured Point";
            else if( b3D )
                return "3D Point";
            else if( bMeasured )
                return "Measured Point";
            else
                return "Point";

        case wkbLineString:
            if( b3D && bMeasured )
                return "3D Measured Line String";
            else if( b3D )
                return "3D Line String";
            else if( bMeasured )
                return "Measured Line String";
            else
                return "Line String";

        case wkbPolygon:
            if( b3D && bMeasured )
                return "3D Measured Polygon";
            else if( b3D )
                return "3D Polygon";
            else if( bMeasured )
                return "Measured Polygon";
            else
                return "Polygon";

        case wkbMultiPoint:
            if( b3D && bMeasured )
                return "3D Measured Multi Point";
            else if( b3D )
                return "3D Multi Point";
            else if( bMeasured )
                return "Measured Multi Point";
            else
                return "Multi Point";

        case wkbMultiLineString:
            if( b3D && bMeasured )
                return "3D Measured Multi Line String";
            else if( b3D )
                return "3D Multi Line String";
            else if( bMeasured )
                return "Measured Multi Line String";
            else
                return "Multi Line String";

        case wkbMultiPolygon:
          if( b3D && bMeasured )
                return "3D Measured Multi Polygon";
            else if( b3D )
                return "3D Multi Polygon";
            else if( bMeasured )
                return "Measured Multi Polygon";
            else
                return "Multi Polygon";

        case wkbGeometryCollection:
            if( b3D && bMeasured )
                return "3D Measured Geometry Collection";
            else if( b3D )
                return "3D Geometry Collection";
            else if( bMeasured )
                return "Measured Geometry Collection";
            else
                return "Geometry Collection";

        case wkbCircularString:
            if( b3D && bMeasured )
                return "3D Measured Circular String";
            else if( b3D )
                return "3D Circular String";
            else if( bMeasured )
                return "Measured Circular String";
            else
                return "Circular String";

        case wkbCompoundCurve:
            if( b3D && bMeasured )
                return "3D Measured Compound Curve";
            else if( b3D )
                return "3D Compound Curve";
            else if( bMeasured )
                return "Measured Compound Curve";
            else
                return "Compound Curve";

        case wkbCurvePolygon:
            if( b3D && bMeasured )
                return "3D Measured Curve Polygon";
            else if( b3D )
                return "3D Curve Polygon";
            else if( bMeasured )
                return "Measured Curve Polygon";
            else
                return "Curve Polygon";

        case wkbMultiCurve:
            if( b3D && bMeasured )
                return "3D Measured Multi Curve";
            else if( b3D )
                return "3D Multi Curve";
            else if( bMeasured )
                return "Measured Multi Curve";
            else
                return "Multi Curve";

        case wkbMultiSurface:
            if( b3D && bMeasured )
                return "3D Measured Multi Surface";
            else if( b3D )
                return "3D Multi Surface";
            else if( bMeasured )
                return "Measured Multi Surface";
            else
                return "Multi Surface";

        case wkbCurve:
            if( b3D && bMeasured )
                return "3D Measured Curve";
            else if( b3D )
                return "3D Curve";
            else if( bMeasured )
                return "Measured Curve";
            else
                return "Curve";

        case wkbSurface:
            if( b3D && bMeasured )
                return "3D Measured Surface";
            else if( b3D )
                return "3D Surface";
            else if( bMeasured )
                return "Measured Surface";
            else
                return "Surface";

        case wkbTriangle:
            if (b3D && bMeasured)
                return "3D Measured Triangle";
            else if (b3D)
                return "3D Triangle";
            else if (bMeasured)
                return "Measured Triangle";
            else
                return "Triangle";

        case wkbPolyhedralSurface:
            if (b3D && bMeasured)
                return "3D Measured PolyhedralSurface";
            else if (b3D)
                return "3D PolyhedralSurface";
            else if (bMeasured)
                return "Measured PolyhedralSurface";
            else
                return "PolyhedralSurface";

        case wkbTIN:
            if (b3D && bMeasured)
                return "3D Measured TIN";
            else if (b3D)
                return "3D TIN";
            else if (bMeasured)
                return "Measured TIN";
            else
                return "TIN";

        case wkbNone:
            return "None";

        default:
        {
            return CPLSPrintf("Unrecognized: %d", static_cast<int>(eType));
        }
    }
}

/************************************************************************/
/*                       OGRMergeGeometryTypes()                        */
/************************************************************************/

/**
 * \brief Find common geometry type.
 *
 * Given two geometry types, find the most specific common
 * type.  Normally used repeatedly with the geometries in a
 * layer to try and establish the most specific geometry type
 * that can be reported for the layer.
 *
 * NOTE: wkbUnknown is the "worst case" indicating a mixture of
 * geometry types with nothing in common but the base geometry
 * type.  wkbNone should be used to indicate that no geometries
 * have been encountered yet, and means the first geometry
 * encountered will establish the preliminary type.
 *
 * @param eMain the first input geometry type.
 * @param eExtra the second input geometry type.
 *
 * @return the merged geometry type.
 */

OGRwkbGeometryType
OGRMergeGeometryTypes( OGRwkbGeometryType eMain,
                       OGRwkbGeometryType eExtra )

{
    return OGRMergeGeometryTypesEx(eMain, eExtra, FALSE);
}

/**
 * \brief Find common geometry type.
 *
 * Given two geometry types, find the most specific common
 * type.  Normally used repeatedly with the geometries in a
 * layer to try and establish the most specific geometry type
 * that can be reported for the layer.
 *
 * NOTE: wkbUnknown is the "worst case" indicating a mixture of
 * geometry types with nothing in common but the base geometry
 * type.  wkbNone should be used to indicate that no geometries
 * have been encountered yet, and means the first geometry
 * encountered will establish the preliminary type.
 *
 * If bAllowPromotingToCurves is set to TRUE, mixing Polygon and CurvePolygon
 * will return CurvePolygon. Mixing LineString, CircularString, CompoundCurve
 * will return CompoundCurve. Mixing MultiPolygon and MultiSurface will return
 * MultiSurface. Mixing MultiCurve and MultiLineString will return MultiCurve.
 *
 * @param eMain the first input geometry type.
 * @param eExtra the second input geometry type.
 * @param bAllowPromotingToCurves determine if promotion to curve type
 * must be done.
 *
 * @return the merged geometry type.
 *
 * @since GDAL 2.0
 */

OGRwkbGeometryType
OGRMergeGeometryTypesEx( OGRwkbGeometryType eMain,
                         OGRwkbGeometryType eExtra,
                         int bAllowPromotingToCurves )

{
    OGRwkbGeometryType eFMain = wkbFlatten(eMain);
    OGRwkbGeometryType eFExtra = wkbFlatten(eExtra);

    const bool bHasZ = ( wkbHasZ(eMain) || wkbHasZ(eExtra) );
    const bool bHasM = ( wkbHasM(eMain) || wkbHasM(eExtra) );

    if( eFMain == wkbUnknown || eFExtra == wkbUnknown )
        return OGR_GT_SetModifier(wkbUnknown, bHasZ, bHasM);

    if( eFMain == wkbNone )
        return eExtra;

    if( eFExtra == wkbNone )
        return eMain;

    if( eFMain == eFExtra )
    {
        return OGR_GT_SetModifier(eFMain, bHasZ, bHasM);
    }

    if( bAllowPromotingToCurves )
    {
        if( OGR_GT_IsCurve(eFMain) && OGR_GT_IsCurve(eFExtra) )
            return OGR_GT_SetModifier(wkbCompoundCurve, bHasZ, bHasM);

        if( OGR_GT_IsSubClassOf(eFMain, eFExtra) )
            return OGR_GT_SetModifier(eFExtra, bHasZ, bHasM);

        if( OGR_GT_IsSubClassOf(eFExtra, eFMain) )
            return OGR_GT_SetModifier(eFMain, bHasZ, bHasM);
    }

    // Both are geometry collections.
    if( OGR_GT_IsSubClassOf(eFMain, wkbGeometryCollection) &&
        OGR_GT_IsSubClassOf(eFExtra, wkbGeometryCollection) )
    {
        return OGR_GT_SetModifier(wkbGeometryCollection, bHasZ, bHasM);
    }

    // One is subclass of the other one
    if( OGR_GT_IsSubClassOf(eFMain, eFExtra) )
    {
        return OGR_GT_SetModifier(eFExtra, bHasZ, bHasM);
    }
    else if( OGR_GT_IsSubClassOf(eFExtra, eFMain) )
    {
        return OGR_GT_SetModifier(eFMain, bHasZ, bHasM);
    }

    // Nothing apparently in common.
    return OGR_GT_SetModifier(wkbUnknown, bHasZ, bHasM);
}

/**
 * \fn void OGRGeometry::flattenTo2D();
 *
 * \brief Convert geometry to strictly 2D.
 * In a sense this converts all Z coordinates
 * to 0.0.
 *
 * This method is the same as the C function OGR_G_FlattenTo2D().
 */

/************************************************************************/
/*                         OGR_G_FlattenTo2D()                          */
/************************************************************************/
/**
 * \brief Convert geometry to strictly 2D.
 * In a sense this converts all Z coordinates
 * to 0.0.
 *
 * This function is the same as the CPP method OGRGeometry::flattenTo2D().
 *
 * @param hGeom handle on the geometry to convert.
 */

void OGR_G_FlattenTo2D( OGRGeometryH hGeom )

{
    reinterpret_cast<OGRGeometry *>(hGeom)->flattenTo2D();
}

/************************************************************************/
/*                            exportToGML()                             */
/************************************************************************/

/**
 * \fn char *OGRGeometry::exportToGML( const char* const *
 * papszOptions = NULL ) const;
 *
 * \brief Convert a geometry into GML format.
 *
 * The GML geometry is expressed directly in terms of GML basic data
 * types assuming the this is available in the gml namespace.  The returned
 * string should be freed with CPLFree() when no longer required.
 *
 * The supported options in OGR 1.8.0 are :
 * <ul>
 * <li> FORMAT=GML3. Otherwise it will default to GML 2.1.2 output.
 * <li> GML3_LINESTRING_ELEMENT=curve. (Only valid for FORMAT=GML3) To
 *     use gml:Curve element for linestrings.  Otherwise
 *     gml:LineString will be used .
 * <li> GML3_LONGSRS=YES/NO. (Only valid for FORMAT=GML3) Default to
 *      YES. If YES, SRS with EPSG authority will be written with the
 *      "urn:ogc:def:crs:EPSG::" prefix.  In the case, if the SRS is a
 *      geographic SRS without explicit AXIS order, but that the same
 *      SRS authority code imported with ImportFromEPSGA() should be
 *      treated as lat/long, then the function will take care of
 *      coordinate order swapping.  If set to NO, SRS with EPSG
 *      authority will be written with the "EPSG:" prefix, even if
 *      they are in lat/long order.
 * </ul>
 *
 * This method is the same as the C function OGR_G_ExportToGMLEx().
 *
 * @param papszOptions NULL-terminated list of options.
 * @return A GML fragment or NULL in case of error.
 */

char *OGRGeometry::exportToGML( const char* const * papszOptions ) const
{
    return OGR_G_ExportToGMLEx(
        reinterpret_cast<OGRGeometryH>(const_cast<OGRGeometry *>(this)),
        const_cast<char **>(papszOptions));
}

/************************************************************************/
/*                            exportToKML()                             */
/************************************************************************/

/**
 * \fn char *OGRGeometry::exportToKML() const;
 *
 * \brief Convert a geometry into KML format.
 *
 * The returned string should be freed with CPLFree() when no longer required.
 *
 * This method is the same as the C function OGR_G_ExportToKML().
 *
 * @return A KML fragment or NULL in case of error.
 */

char *OGRGeometry::exportToKML() const
{
    return OGR_G_ExportToKML(
        reinterpret_cast<OGRGeometryH>(const_cast<OGRGeometry *>(this)), NULL);
}

/************************************************************************/
/*                            exportToJson()                             */
/************************************************************************/

/**
 * \fn char *OGRGeometry::exportToJson() const;
 *
 * \brief Convert a geometry into GeoJSON format.
 *
 * The returned string should be freed with CPLFree() when no longer required.
 *
 * This method is the same as the C function OGR_G_ExportToJson().
 *
 * @return A GeoJSON fragment or NULL in case of error.
 */

char *OGRGeometry::exportToJson() const
{
    OGRGeometry* poGeometry = const_cast<OGRGeometry*>(this);
    return OGR_G_ExportToJson( reinterpret_cast<OGRGeometryH>(poGeometry) );
}

/************************************************************************/
/*                 OGRSetGenerate_DB2_V72_BYTE_ORDER()                  */
/************************************************************************/

/**
  * \brief Special entry point to enable the hack for generating DB2 V7.2 style
  * WKB.
  *
  * DB2 seems to have placed (and require) an extra 0x30 or'ed with the byte
  * order in WKB.  This entry point is used to turn on or off the generation of
  * such WKB.
  */
OGRErr OGRSetGenerate_DB2_V72_BYTE_ORDER( int bGenerate_DB2_V72_BYTE_ORDER )

{
#if defined(HACK_FOR_IBM_DB2_V72)
    OGRGeometry::bGenerate_DB2_V72_BYTE_ORDER = bGenerate_DB2_V72_BYTE_ORDER;
    return OGRERR_NONE;
#else
    if( bGenerate_DB2_V72_BYTE_ORDER )
        return OGRERR_FAILURE;
    else
        return OGRERR_NONE;
#endif
}
/************************************************************************/
/*                 OGRGetGenerate_DB2_V72_BYTE_ORDER()                  */
/*                                                                      */
/*      This is a special entry point to get the value of static flag   */
/*      OGRGeometry::bGenerate_DB2_V72_BYTE_ORDER.                      */
/************************************************************************/
int OGRGetGenerate_DB2_V72_BYTE_ORDER()
{
   return OGRGeometry::bGenerate_DB2_V72_BYTE_ORDER;
}

/************************************************************************/
/*                          createGEOSContext()                         */
/************************************************************************/

/** Create a new GEOS context.
 * @return a new GEOS context.
 */
GEOSContextHandle_t OGRGeometry::createGEOSContext()
{
#ifndef HAVE_GEOS
    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;
#else
    return initGEOS_r( OGRGEOSWarningHandler, OGRGEOSErrorHandler );
#endif
}

/************************************************************************/
/*                          freeGEOSContext()                           */
/************************************************************************/

/** Destroy a GEOS context.
 * @param hGEOSCtxt GEOS context
 */
void OGRGeometry::freeGEOSContext(
    UNUSED_IF_NO_GEOS GEOSContextHandle_t hGEOSCtxt)
{
#ifdef HAVE_GEOS
    if( hGEOSCtxt != NULL )
    {
        finishGEOS_r( hGEOSCtxt );
    }
#endif
}

/************************************************************************/
/*                            exportToGEOS()                            */
/************************************************************************/

/** Returns a GEOSGeom object corresponding to the geometry.
 *
 * @param hGEOSCtxt GEOS context
 * @return a GEOSGeom object corresponding to the geometry.
 */
GEOSGeom OGRGeometry::exportToGEOS(
    UNUSED_IF_NO_GEOS GEOSContextHandle_t hGEOSCtxt) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;

#else

    if( hGEOSCtxt == NULL )
        return NULL;

    // POINT EMPTY is exported to WKB as if it were POINT(0 0),
    // so that particular case is necessary.
    const OGRwkbGeometryType eType = wkbFlatten(getGeometryType());
    if( eType == wkbPoint && IsEmpty() )
    {
        return GEOSGeomFromWKT_r(hGEOSCtxt, "POINT EMPTY");
    }

    GEOSGeom hGeom = NULL;

    OGRGeometry* poLinearGeom = NULL;
    if( hasCurveGeometry() )
    {
        poLinearGeom = getLinearGeometry();
        if( poLinearGeom->IsMeasured() )
            poLinearGeom->setMeasured(FALSE);
    }
    else
    {
        poLinearGeom = const_cast<OGRGeometry*>(this);
        if( IsMeasured() )
        {
            poLinearGeom = clone();
            poLinearGeom->setMeasured(FALSE);
        }
    }
    const size_t nDataSize = poLinearGeom->WkbSize();
    unsigned char *pabyData =
        static_cast<unsigned char *>(CPLMalloc(nDataSize));
    if (eType == wkbTriangle)
    {
        OGRPolygon poPolygon(*((OGRPolygon *)poLinearGeom));
        if( poPolygon.exportToWkb( wkbNDR, pabyData ) == OGRERR_NONE )
            hGeom = GEOSGeomFromWKB_buf_r( hGEOSCtxt, pabyData, nDataSize );
    }
    else if ( eType == wkbPolyhedralSurface || eType == wkbTIN )
    {
        OGRGeometry *poGC = OGRGeometryFactory::forceTo(
                        poLinearGeom->clone(), wkbGeometryCollection, NULL );
        OGRErr eErr = poGC->exportToWkb( wkbNDR, pabyData );
        if( eErr == OGRERR_NONE )
            hGeom = GEOSGeomFromWKB_buf_r( hGEOSCtxt, pabyData, nDataSize );
        delete poGC;
    }
    else if ( eType == wkbGeometryCollection )
    {
        bool bCanConvertToMultiPoly = true;
        bool bMustConvertToMultiPoly = true;
        OGRGeometryCollection* poGC = dynamic_cast<OGRGeometryCollection*>(
                                                                poLinearGeom);
        for( int iGeom = 0; poGC != NULL && iGeom < poGC->getNumGeometries();
             iGeom++ )
        {
            OGRwkbGeometryType eSubGeomType =
                wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType());
            if( eSubGeomType == wkbPolyhedralSurface || eSubGeomType == wkbTIN )
            {
                bMustConvertToMultiPoly = true;
            }
            else if( eSubGeomType != wkbMultiPolygon &&
                     eSubGeomType != wkbPolygon )
            {
                bCanConvertToMultiPoly = false;
                break;
            }
        }
        if( bCanConvertToMultiPoly && bMustConvertToMultiPoly )
        {
            OGRGeometry *poMultiPolygon = OGRGeometryFactory::forceTo(
                                poLinearGeom->clone(), wkbMultiPolygon, NULL );
            OGRGeometry* poGCDest = OGRGeometryFactory::forceTo(
                                poMultiPolygon, wkbGeometryCollection, NULL );
            OGRErr eErr = poGCDest->exportToWkb( wkbNDR, pabyData );
            if( eErr == OGRERR_NONE )
                hGeom = GEOSGeomFromWKB_buf_r( hGEOSCtxt, pabyData, nDataSize );
            delete poGCDest;
        }
        else
        {
            if( poLinearGeom->exportToWkb( wkbNDR, pabyData ) == OGRERR_NONE )
                hGeom = GEOSGeomFromWKB_buf_r( hGEOSCtxt, pabyData, nDataSize );
        }
    }
    else if( poLinearGeom->exportToWkb( wkbNDR, pabyData ) == OGRERR_NONE )
        hGeom = GEOSGeomFromWKB_buf_r( hGEOSCtxt, pabyData, nDataSize );

    CPLFree( pabyData );

    if( poLinearGeom != this )
        delete poLinearGeom;

    return hGeom;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

/**
 * \brief Returns if this geometry is or has curve geometry.
 *
 * Returns if a geometry is, contains or may contain a CIRCULARSTRING,
 * COMPOUNDCURVE, CURVEPOLYGON, MULTICURVE or MULTISURFACE.
 *
 * If bLookForNonLinear is set to TRUE, it will be actually looked if
 * the geometry or its subgeometries are or contain a non-linear
 * geometry in them. In which case, if the method returns TRUE, it
 * means that getLinearGeometry() would return an approximate version
 * of the geometry. Otherwise, getLinearGeometry() would do a
 * conversion, but with just converting container type, like
 * COMPOUNDCURVE -> LINESTRING, MULTICURVE -> MULTILINESTRING or
 * MULTISURFACE -> MULTIPOLYGON, resulting in a "loss-less"
 * conversion.
 *
 * This method is the same as the C function OGR_G_HasCurveGeometry().
 *
 * @param bLookForNonLinear set it to TRUE to check if the geometry is
 * or contains a CIRCULARSTRING.
 *
 * @return TRUE if this geometry is or has curve geometry.
 *
 * @since GDAL 2.0
 */

OGRBoolean OGRGeometry::hasCurveGeometry(
    CPL_UNUSED int bLookForNonLinear ) const
{
    return FALSE;
}

/************************************************************************/
/*                         getLinearGeometry()                        */
/************************************************************************/

/**
 * \brief Return, possibly approximate, non-curve version of this geometry.
 *
 * Returns a geometry that has no CIRCULARSTRING, COMPOUNDCURVE, CURVEPOLYGON,
 * MULTICURVE or MULTISURFACE in it, by approximating curve geometries.
 *
 * The ownership of the returned geometry belongs to the caller.
 *
 * The reverse method is OGRGeometry::getCurveGeometry().
 *
 * This method is the same as the C function OGR_G_GetLinearGeometry().
 *
 * @param dfMaxAngleStepSizeDegrees the largest step in degrees along the
 * arc, zero to use the default setting.
 * @param papszOptions options as a null-terminated list of strings.
 *                     See OGRGeometryFactory::curveToLineString() for
 *                     valid options.
 *
 * @return a new geometry.
 *
 * @since GDAL 2.0
 */

OGRGeometry* OGRGeometry::getLinearGeometry(
    CPL_UNUSED double dfMaxAngleStepSizeDegrees,
    CPL_UNUSED const char* const* papszOptions) const
{
    return clone();
}

/************************************************************************/
/*                             getCurveGeometry()                       */
/************************************************************************/

/**
 * \brief Return curve version of this geometry.
 *
 * Returns a geometry that has possibly CIRCULARSTRING, COMPOUNDCURVE,
 * CURVEPOLYGON, MULTICURVE or MULTISURFACE in it, by de-approximating
 * curve geometries.
 *
 * If the geometry has no curve portion, the returned geometry will be a clone
 * of it.
 *
 * The ownership of the returned geometry belongs to the caller.
 *
 * The reverse method is OGRGeometry::getLinearGeometry().
 *
 * This function is the same as C function OGR_G_GetCurveGeometry().
 *
 * @param papszOptions options as a null-terminated list of strings.
 *                     Unused for now. Must be set to NULL.
 *
 * @return a new geometry.
 *
 * @since GDAL 2.0
 */

OGRGeometry* OGRGeometry::getCurveGeometry(
    CPL_UNUSED const char* const* papszOptions) const
{
    return clone();
}

/************************************************************************/
/*                              Distance()                              */
/************************************************************************/

/**
 * \brief Compute distance between two geometries.
 *
 * Returns the shortest distance between the two geometries. The distance is
 * expressed into the same unit as the coordinates of the geometries.
 *
 * This method is the same as the C function OGR_G_Distance().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the other geometry to compare against.
 *
 * @return the distance between the geometries or -1 if an error occurs.
 */

double OGRGeometry::Distance( const OGRGeometry *poOtherGeom ) const

{
    if( NULL == poOtherGeom )
    {
        CPLDebug( "OGR",
                  "OGRGeometry::Distance called with NULL geometry pointer" );
        return -1.0;
    }

    if (IsSFCGALCompatible() || poOtherGeom->IsSFCGALCompatible())
    {
    #ifndef HAVE_SFCGAL

        CPLError( CE_Failure, CPLE_NotSupported,
                  "SFCGAL support not enabled." );
        return -1.0;

    #else

        sfcgal_geometry_t *poThis =
            OGRGeometry::OGRexportToSFCGAL(const_cast<OGRGeometry *>(this));
        if (poThis == NULL)
            return -1.0;

        sfcgal_geometry_t *poOther =
            OGRGeometry::OGRexportToSFCGAL(
                const_cast<OGRGeometry *>(poOtherGeom));
        if (poOther == NULL)
            return -1.0;

        const double dfDistance = sfcgal_geometry_distance(poThis, poOther);

        sfcgal_geometry_delete(poThis);
        sfcgal_geometry_delete(poOther);

        return dfDistance > 0.0 ? dfDistance : -1.0;

    #endif
    }

    else
    {
    #ifndef HAVE_GEOS

        CPLError( CE_Failure, CPLE_NotSupported,
                  "GEOS support not enabled." );
        return -1.0;

    #else

        GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
        // GEOSGeom is a pointer
        GEOSGeom hOther = poOtherGeom->exportToGEOS(hGEOSCtxt);
        GEOSGeom hThis = exportToGEOS(hGEOSCtxt);

        int bIsErr = 0;
        double dfDistance = 0.0;

        if( hThis != NULL && hOther != NULL )
        {
            bIsErr = GEOSDistance_r( hGEOSCtxt, hThis, hOther, &dfDistance );
        }

        GEOSGeom_destroy_r( hGEOSCtxt, hThis );
        GEOSGeom_destroy_r( hGEOSCtxt, hOther );
        freeGEOSContext( hGEOSCtxt );

        if ( bIsErr > 0 )
        {
            return dfDistance;
        }

        /* Calculations error */
        return -1.0;

    #endif /* HAVE_GEOS */
    }
}

/************************************************************************/
/*                           OGR_G_Distance()                           */
/************************************************************************/
/**
 * \brief Compute distance between two geometries.
 *
 * Returns the shortest distance between the two geometries. The distance is
 * expressed into the same unit as the coordinates of the geometries.
 *
 * This function is the same as the C++ method OGRGeometry::Distance().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hFirst the first geometry to compare against.
 * @param hOther the other geometry to compare against.
 *
 * @return the distance between the geometries or -1 if an error occurs.
 */

double OGR_G_Distance( OGRGeometryH hFirst, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hFirst, "OGR_G_Distance", 0.0 );

    return reinterpret_cast<OGRGeometry *>(hFirst)->
        Distance(reinterpret_cast<OGRGeometry *>(hOther));
}

/************************************************************************/
/*                             Distance3D()                             */
/************************************************************************/

/**
 * \brief Returns the 3D distance between two geometries
 *
 * The distance is expressed into the same unit as the coordinates of the
 * geometries.
 *
 * This method is built on the SFCGAL library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the SFCGAL library, this method will always return
 * -1.0
 *
 * This function is the same as the C function OGR_G_Distance3D().
 *
 * @return distance between the two geometries
 * @since GDAL 2.2
 */

double OGRGeometry::Distance3D(
    UNUSED_IF_NO_SFCGAL const OGRGeometry *poOtherGeom ) const
{
    if( poOtherGeom == NULL )
    {
        CPLDebug( "OGR",
                  "OGRTriangle::Distance3D called with NULL geometry pointer" );
        return -1.0;
    }

    if( !(poOtherGeom->Is3D() && Is3D()) )
    {
        CPLDebug( "OGR",
                  "OGRGeometry::Distance3D called with two dimensional "
                  "geometry(geometries)" );
        return -1.0;
    }

#ifndef HAVE_SFCGAL

    CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
    return -1.0;

#else

    sfcgal_init();
    sfcgal_geometry_t *poThis =
        OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)this);
    if( poThis == NULL )
        return -1.0;

    sfcgal_geometry_t *poOther =
        OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)poOtherGeom);
    if( poOther == NULL )
    {
        sfcgal_geometry_delete(poThis);
        return -1.0;
    }

    const double dfDistance = sfcgal_geometry_distance_3d(poThis, poOther);

    sfcgal_geometry_delete(poThis);
    sfcgal_geometry_delete(poOther);

    return dfDistance > 0 ? dfDistance : -1.0;

#endif
}

/************************************************************************/
/*                           OGR_G_Distance3D()                         */
/************************************************************************/
/**
 * \brief Returns the 3D distance between two geometries
 *
 * The distance is expressed into the same unit as the coordinates of the
 * geometries.
 *
 * This method is built on the SFCGAL library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the SFCGAL library, this method will always return
 * -1.0
 *
 * This function is the same as the C++ method OGRGeometry::Distance3D().
 *
 * @param hFirst the first geometry to compare against.
 * @param hOther the other geometry to compare against.
 * @return distance between the two geometries
 * @since GDAL 2.2
 *
 * @return the distance between the geometries or -1 if an error occurs.
 */


double OGR_G_Distance3D( OGRGeometryH hFirst, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hFirst, "OGR_G_Distance3D", 0.0 );

    return
        reinterpret_cast<OGRGeometry *>(hFirst)->
            Distance3D(reinterpret_cast<OGRGeometry *>(hOther));
}

/************************************************************************/
/*                       OGRGeometryRebuildCurves()                     */
/************************************************************************/

#ifdef HAVE_GEOS
static OGRGeometry* OGRGeometryRebuildCurves( const OGRGeometry* poGeom,
                                              const OGRGeometry* poOtherGeom,
                                              OGRGeometry* poOGRProduct )
{
    if( poOGRProduct != NULL &&
        wkbFlatten(poOGRProduct->getGeometryType()) != wkbPoint &&
        (poGeom->hasCurveGeometry() ||
         (poOtherGeom && poOtherGeom->hasCurveGeometry())) )
    {
        OGRGeometry* poCurveGeom = poOGRProduct->getCurveGeometry();
        delete poOGRProduct;
        return poCurveGeom;
    }
    return poOGRProduct;
}
#endif

/************************************************************************/
/*                             ConvexHull()                             */
/************************************************************************/

/**
 * \brief Compute convex hull.
 *
 * A new geometry object is created and returned containing the convex
 * hull of the geometry on which the method is invoked.
 *
 * This method is the same as the C function OGR_G_ConvexHull().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @return a newly allocated geometry now owned by the caller, or NULL
 * on failure.
 */

OGRGeometry *OGRGeometry::ConvexHull() const

{
    if( IsSFCGALCompatible() )
    {
#ifndef HAVE_SFCGAL

        CPLError( CE_Failure, CPLE_NotSupported,
                  "SFCGAL support not enabled." );
        return NULL;

#else

        sfcgal_geometry_t *poThis =
            OGRGeometry::OGRexportToSFCGAL(const_cast<OGRGeometry *>(this));
        if (poThis == NULL)
            return FALSE;

        sfcgal_geometry_t *poRes = sfcgal_geometry_convexhull_3d(poThis);
        OGRGeometry *h_prodGeom = SFCGALexportToOGR(poRes);

        h_prodGeom->assignSpatialReference(getSpatialReference());

        sfcgal_geometry_delete(poThis);
        sfcgal_geometry_delete(poRes);

        return h_prodGeom;

#endif
    }

    else
    {
#ifndef HAVE_GEOS

        CPLError( CE_Failure, CPLE_NotSupported,
                  "GEOS support not enabled." );
        return NULL;

#else

        OGRGeometry *poOGRProduct = NULL;

        GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
        GEOSGeom hGeosGeom = exportToGEOS(hGEOSCtxt);
        if( hGeosGeom != NULL )
        {
            GEOSGeom hGeosHull = GEOSConvexHull_r( hGEOSCtxt, hGeosGeom );
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosGeom );

            if( hGeosHull != NULL )
            {
                poOGRProduct =
                    OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosHull);
                if( poOGRProduct != NULL && getSpatialReference() != NULL )
                    poOGRProduct->assignSpatialReference(getSpatialReference());
                poOGRProduct =
                    OGRGeometryRebuildCurves(this, NULL, poOGRProduct);
                GEOSGeom_destroy_r( hGEOSCtxt, hGeosHull);
            }
        }
        freeGEOSContext( hGEOSCtxt );

        return poOGRProduct;

#endif /* HAVE_GEOS */
    }
}

/************************************************************************/
/*                          OGR_G_ConvexHull()                          */
/************************************************************************/
/**
 * \brief Compute convex hull.
 *
 * A new geometry object is created and returned containing the convex
 * hull of the geometry on which the method is invoked.
 *
 * This function is the same as the C++ method OGRGeometry::ConvexHull().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hTarget The Geometry to calculate the convex hull of.
 *
 * @return a handle to a newly allocated geometry now owned by the caller,
 *         or NULL on failure.
 */

OGRGeometryH OGR_G_ConvexHull( OGRGeometryH hTarget )

{
    VALIDATE_POINTER1( hTarget, "OGR_G_ConvexHull", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hTarget)->ConvexHull());
}

/************************************************************************/
/*                            Boundary()                                */
/************************************************************************/

/**
 * \brief Compute boundary.
 *
 * A new geometry object is created and returned containing the boundary
 * of the geometry on which the method is invoked.
 *
 * This method is the same as the C function OGR_G_Boundary().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @return a newly allocated geometry now owned by the caller, or NULL
 * on failure.
 *
 * @since OGR 1.8.0
 */

OGRGeometry *OGRGeometry::Boundary() const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;

#else

    GEOSGeom hGeosGeom = NULL;
    OGRGeometry *poOGRProduct = NULL;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hGeosGeom = exportToGEOS(hGEOSCtxt);
    if( hGeosGeom != NULL )
    {
        GEOSGeom hGeosProduct = GEOSBoundary_r( hGEOSCtxt, hGeosGeom );
        GEOSGeom_destroy_r( hGEOSCtxt, hGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct =
                OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct);
            if( poOGRProduct != NULL && getSpatialReference() != NULL )
                poOGRProduct->assignSpatialReference(getSpatialReference());
            poOGRProduct = OGRGeometryRebuildCurves(this, NULL, poOGRProduct);
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
        }
    }
    freeGEOSContext( hGEOSCtxt );

    return poOGRProduct;

#endif  // HAVE_GEOS
}

//! @cond Doxygen_Suppress
/**
 * \brief Compute boundary (deprecated)
 *
 * @deprecated
 *
 * @see Boundary()
 */
OGRGeometry *OGRGeometry::getBoundary() const

{
    return Boundary();
}
//! @endcond

/************************************************************************/
/*                         OGR_G_Boundary()                             */
/************************************************************************/
/**
 * \brief Compute boundary.
 *
 * A new geometry object is created and returned containing the boundary
 * of the geometry on which the method is invoked.
 *
 * This function is the same as the C++ method OGR_G_Boundary().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hTarget The Geometry to calculate the boundary of.
 *
 * @return a handle to a newly allocated geometry now owned by the caller,
 *         or NULL on failure.
 *
 * @since OGR 1.8.0
 */
OGRGeometryH OGR_G_Boundary( OGRGeometryH hTarget )

{
    VALIDATE_POINTER1( hTarget, "OGR_G_Boundary", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hTarget)->Boundary());
}

/**
 * \brief Compute boundary (deprecated)
 *
 * @deprecated
 *
 * @see OGR_G_Boundary()
 */
OGRGeometryH OGR_G_GetBoundary( OGRGeometryH hTarget )

{
    VALIDATE_POINTER1( hTarget, "OGR_G_GetBoundary", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hTarget)->Boundary());
}

/************************************************************************/
/*                               Buffer()                               */
/************************************************************************/

/**
 * \brief Compute buffer of geometry.
 *
 * Builds a new geometry containing the buffer region around the geometry
 * on which it is invoked.  The buffer is a polygon containing the region within
 * the buffer distance of the original geometry.
 *
 * Some buffer sections are properly described as curves, but are converted to
 * approximate polygons.  The nQuadSegs parameter can be used to control how
 * many segments should be used to define a 90 degree curve - a quadrant of a
 * circle.  A value of 30 is a reasonable default.  Large values result in
 * large numbers of vertices in the resulting buffer geometry while small
 * numbers reduce the accuracy of the result.
 *
 * This method is the same as the C function OGR_G_Buffer().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param dfDist the buffer distance to be applied. Should be expressed into
 *               the same unit as the coordinates of the geometry.
 *
 * @param nQuadSegs the number of segments used to approximate a 90
 * degree (quadrant) of curvature.
 *
 * @return the newly created geometry, or NULL if an error occurs.
 */

OGRGeometry *OGRGeometry::Buffer( UNUSED_IF_NO_GEOS double dfDist,
                                  UNUSED_IF_NO_GEOS int nQuadSegs ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;

#else

    GEOSGeom hGeosGeom = NULL;
    OGRGeometry *poOGRProduct = NULL;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hGeosGeom = exportToGEOS(hGEOSCtxt);
    if( hGeosGeom != NULL )
    {
        GEOSGeom hGeosProduct =
            GEOSBuffer_r( hGEOSCtxt, hGeosGeom, dfDist, nQuadSegs );
        GEOSGeom_destroy_r( hGEOSCtxt, hGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct =
                OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct);
            if( poOGRProduct != NULL && getSpatialReference() != NULL )
                poOGRProduct->assignSpatialReference(getSpatialReference());
            poOGRProduct = OGRGeometryRebuildCurves(this, NULL, poOGRProduct);
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
        }
    }
    freeGEOSContext(hGEOSCtxt);

    return poOGRProduct;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                            OGR_G_Buffer()                            */
/************************************************************************/

/**
 * \brief Compute buffer of geometry.
 *
 * Builds a new geometry containing the buffer region around the geometry
 * on which it is invoked.  The buffer is a polygon containing the region within
 * the buffer distance of the original geometry.
 *
 * Some buffer sections are properly described as curves, but are converted to
 * approximate polygons.  The nQuadSegs parameter can be used to control how
 * many segments should be used to define a 90 degree curve - a quadrant of a
 * circle.  A value of 30 is a reasonable default.  Large values result in
 * large numbers of vertices in the resulting buffer geometry while small
 * numbers reduce the accuracy of the result.
 *
 * This function is the same as the C++ method OGRGeometry::Buffer().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hTarget the geometry.
 * @param dfDist the buffer distance to be applied. Should be expressed into
 *               the same unit as the coordinates of the geometry.
 *
 * @param nQuadSegs the number of segments used to approximate a 90 degree
 * (quadrant) of curvature.
 *
 * @return the newly created geometry, or NULL if an error occurs.
 */

OGRGeometryH OGR_G_Buffer( OGRGeometryH hTarget, double dfDist, int nQuadSegs )

{
    VALIDATE_POINTER1( hTarget, "OGR_G_Buffer", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hTarget)->Buffer( dfDist, nQuadSegs ));
}

/************************************************************************/
/*                            Intersection()                            */
/************************************************************************/

/**
 * \brief Compute intersection.
 *
 * Generates a new geometry which is the region of intersection of the
 * two geometries operated on.  The Intersects() method can be used to test if
 * two geometries intersect.
 *
 * This method is the same as the C function OGR_G_Intersection().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the other geometry intersected with "this" geometry.
 *
 * @return a new geometry representing the intersection or NULL if there is
 * no intersection or an error occurs.
 */

OGRGeometry *OGRGeometry::Intersection(
    UNUSED_PARAMETER const OGRGeometry *poOtherGeom ) const

{
    if (IsSFCGALCompatible() || poOtherGeom->IsSFCGALCompatible())
    {
    #ifndef HAVE_SFCGAL

        CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
        return NULL;

    #else

        sfcgal_geometry_t *poThis = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)this);
        if (poThis == NULL)
            return FALSE;

        sfcgal_geometry_t *poOther = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)poOtherGeom);
        if (poThis == NULL)
            return FALSE;

        sfcgal_geometry_t *poRes = sfcgal_geometry_intersection_3d(poThis, poOther);
        OGRGeometry *h_prodGeom = SFCGALexportToOGR(poRes);

        if (h_prodGeom != NULL && getSpatialReference() != NULL
            && poOtherGeom->getSpatialReference() != NULL
            && poOtherGeom->getSpatialReference()->IsSame(getSpatialReference()))
            h_prodGeom->assignSpatialReference(getSpatialReference());

        sfcgal_geometry_delete(poThis);
        sfcgal_geometry_delete(poOther);
        sfcgal_geometry_delete(poRes);

        return h_prodGeom;

    #endif
    }

    else
    {
    #ifndef HAVE_GEOS

        CPLError( CE_Failure, CPLE_NotSupported,
                  "GEOS support not enabled." );
        return NULL;

    #else

        GEOSGeom hThisGeosGeom = NULL;
        GEOSGeom hOtherGeosGeom = NULL;
        GEOSGeom hGeosProduct = NULL;
        OGRGeometry *poOGRProduct = NULL;

        GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
        hThisGeosGeom = exportToGEOS(hGEOSCtxt);
        hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);
        if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
        {
            hGeosProduct = GEOSIntersection_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );

            if( hGeosProduct != NULL )
            {
                poOGRProduct = OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct);
                if( poOGRProduct != NULL && getSpatialReference() != NULL &&
                    poOtherGeom->getSpatialReference() != NULL &&
                    poOtherGeom->getSpatialReference()->IsSame(getSpatialReference()) )
                {
                    poOGRProduct->assignSpatialReference(getSpatialReference());
                }
                poOGRProduct = OGRGeometryRebuildCurves(this, poOtherGeom, poOGRProduct);
                GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
            }
        }
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
        GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
        freeGEOSContext( hGEOSCtxt );

        return poOGRProduct;

    #endif /* HAVE_GEOS */
    }
}

/************************************************************************/
/*                         OGR_G_Intersection()                         */
/************************************************************************/

/**
 * \brief Compute intersection.
 *
 * Generates a new geometry which is the region of intersection of the
 * two geometries operated on.  The OGR_G_Intersects() function can be used to
 * test if two geometries intersect.
 *
 * This function is the same as the C++ method OGRGeometry::Intersection().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry.
 * @param hOther the other geometry.
 *
 * @return a new geometry representing the intersection or NULL if there is
 * no intersection or an error occurs.
 */

OGRGeometryH OGR_G_Intersection( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Intersection", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hThis)->
            Intersection(reinterpret_cast<OGRGeometry *>(hOther)));
}

/************************************************************************/
/*                               Union()                                */
/************************************************************************/

/**
 * \brief Compute union.
 *
 * Generates a new geometry which is the region of union of the
 * two geometries operated on.
 *
 * This method is the same as the C function OGR_G_Union().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the other geometry unioned with "this" geometry.
 *
 * @return a new geometry representing the union or NULL if an error occurs.
 */

OGRGeometry *OGRGeometry::Union(
    UNUSED_PARAMETER const OGRGeometry *poOtherGeom ) const

{
    if (IsSFCGALCompatible() || poOtherGeom->IsSFCGALCompatible())
    {
    #ifndef HAVE_SFCGAL

        CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
        return NULL;

    #else

        sfcgal_geometry_t *poThis = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)this);
        if (poThis == NULL)
            return NULL;

        sfcgal_geometry_t *poOther = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)poOtherGeom);
        if (poOther == NULL)
            return NULL;

        sfcgal_geometry_t *poRes = sfcgal_geometry_union_3d(poThis, poOther);
        if (poRes == NULL)
            return NULL;

        OGRGeometry *h_prodGeom = OGRGeometry::SFCGALexportToOGR(poRes);
        if (h_prodGeom == NULL)
            return NULL;

        if (h_prodGeom != NULL && getSpatialReference() != NULL
            && poOtherGeom->getSpatialReference() != NULL
            && poOtherGeom->getSpatialReference()->IsSame(getSpatialReference()))
            h_prodGeom->assignSpatialReference(getSpatialReference());

        sfcgal_geometry_delete(poThis);
        sfcgal_geometry_delete(poOther);

        return h_prodGeom;

    #endif
    }

    else
    {
    #ifndef HAVE_GEOS

        CPLError( CE_Failure, CPLE_NotSupported,
                  "GEOS support not enabled." );
        return NULL;

    #else

        GEOSGeom hThisGeosGeom = NULL;
        GEOSGeom hOtherGeosGeom = NULL;
        GEOSGeom hGeosProduct = NULL;
        OGRGeometry *poOGRProduct = NULL;

        GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
        hThisGeosGeom = exportToGEOS(hGEOSCtxt);
        hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);
        if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
        {
            hGeosProduct = GEOSUnion_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );

            if( hGeosProduct != NULL )
            {
                poOGRProduct = OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct);
                if( poOGRProduct != NULL && getSpatialReference() != NULL &&
                    poOtherGeom->getSpatialReference() != NULL &&
                    poOtherGeom->getSpatialReference()->IsSame(getSpatialReference()) )
                {
                    poOGRProduct->assignSpatialReference(getSpatialReference());
                }
                poOGRProduct = OGRGeometryRebuildCurves(this, poOtherGeom, poOGRProduct);
                GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
            }
        }
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
        GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
        freeGEOSContext( hGEOSCtxt );

        return poOGRProduct;

    #endif /* HAVE_GEOS */
    }
}

/************************************************************************/
/*                            OGR_G_Union()                             */
/************************************************************************/

/**
 * \brief Compute union.
 *
 * Generates a new geometry which is the region of union of the
 * two geometries operated on.
 *
 * This function is the same as the C++ method OGRGeometry::Union().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry.
 * @param hOther the other geometry.
 *
 * @return a new geometry representing the union or NULL if an error occurs.
 */

OGRGeometryH OGR_G_Union( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Union", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hThis)->
          Union(reinterpret_cast<OGRGeometry *>(hOther)));
}

/************************************************************************/
/*                               UnionCascaded()                        */
/************************************************************************/

/**
 * \brief Compute union using cascading.
 *
 * This method is the same as the C function OGR_G_UnionCascaded().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @return a new geometry representing the union or NULL if an error occurs.
 *
 * @since OGR 1.8.0
 */

OGRGeometry *OGRGeometry::UnionCascaded() const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;
#else
    GEOSGeom hThisGeosGeom = NULL;
    OGRGeometry *poOGRProduct = NULL;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL )
    {
        GEOSGeom hGeosProduct = GEOSUnionCascaded_r(hGEOSCtxt, hThisGeosGeom);
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct =
                OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct);
            if( poOGRProduct != NULL && getSpatialReference() != NULL )
                poOGRProduct->assignSpatialReference(getSpatialReference());
            poOGRProduct = OGRGeometryRebuildCurves(this, NULL, poOGRProduct);
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
        }
    }
    freeGEOSContext( hGEOSCtxt );

    return poOGRProduct;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                            OGR_G_UnionCascaded()                     */
/************************************************************************/

/**
 * \brief Compute union using cascading.
 *
 * This function is the same as the C++ method OGRGeometry::UnionCascaded().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry.
 *
 * @return a new geometry representing the union or NULL if an error occurs.
 */

OGRGeometryH OGR_G_UnionCascaded( OGRGeometryH hThis )

{
    VALIDATE_POINTER1( hThis, "OGR_G_UnionCascaded", NULL );

    return reinterpret_cast<OGRGeometryH>(
      reinterpret_cast<OGRGeometry *>(hThis)->UnionCascaded());
}

/************************************************************************/
/*                             Difference()                             */
/************************************************************************/

/**
 * \brief Compute difference.
 *
 * Generates a new geometry which is the region of this geometry with the
 * region of the second geometry removed.
 *
 * This method is the same as the C function OGR_G_Difference().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the other geometry removed from "this" geometry.
 *
 * @return a new geometry representing the difference or NULL if the
 * difference is empty or an error occurs.
 */

OGRGeometry *OGRGeometry::Difference(
    UNUSED_PARAMETER const OGRGeometry *poOtherGeom ) const

{
    if (IsSFCGALCompatible() || poOtherGeom->IsSFCGALCompatible())
    {
    #ifndef HAVE_SFCGAL

        CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
        return NULL;

    #else

        sfcgal_geometry_t *poThis = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)this);
        if (poThis == NULL)
            return NULL;

        sfcgal_geometry_t *poOther = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)poOtherGeom);
        if (poOther == NULL)
            return NULL;

        sfcgal_geometry_t *poRes = sfcgal_geometry_difference_3d(poThis, poOther);
        OGRGeometry *h_prodGeom = OGRGeometry::SFCGALexportToOGR(poRes);

        if (h_prodGeom == NULL)
            return NULL;

        if (h_prodGeom != NULL && getSpatialReference() != NULL
            && poOtherGeom->getSpatialReference() != NULL
            && poOtherGeom->getSpatialReference()->IsSame(getSpatialReference()))
            h_prodGeom->assignSpatialReference(getSpatialReference());

        sfcgal_geometry_delete(poThis);
        sfcgal_geometry_delete(poOther);
        sfcgal_geometry_delete(poRes);

        return h_prodGeom;

    #endif
    }

    else
    {
    #ifndef HAVE_GEOS

        CPLError( CE_Failure, CPLE_NotSupported,
                  "GEOS support not enabled." );
        return NULL;

    #else

        GEOSGeom hThisGeosGeom = NULL;
        GEOSGeom hOtherGeosGeom = NULL;
        GEOSGeom hGeosProduct = NULL;
        OGRGeometry *poOGRProduct = NULL;

        GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
        hThisGeosGeom = exportToGEOS(hGEOSCtxt);
        hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);
        if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
        {
            hGeosProduct = GEOSDifference_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );

            if( hGeosProduct != NULL )
            {
                poOGRProduct = OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct);
                if( poOGRProduct != NULL && getSpatialReference() != NULL &&
                    poOtherGeom->getSpatialReference() != NULL &&
                    poOtherGeom->getSpatialReference()->IsSame(getSpatialReference()) )
                {
                    poOGRProduct->assignSpatialReference(getSpatialReference());
                }
                poOGRProduct = OGRGeometryRebuildCurves(this, poOtherGeom, poOGRProduct);
                GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
            }
        }
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
        GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
        freeGEOSContext( hGEOSCtxt );

        return poOGRProduct;

    #endif /* HAVE_GEOS */
    }
}

/************************************************************************/
/*                          OGR_G_Difference()                          */
/************************************************************************/

/**
 * \brief Compute difference.
 *
 * Generates a new geometry which is the region of this geometry with the
 * region of the other geometry removed.
 *
 * This function is the same as the C++ method OGRGeometry::Difference().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry.
 * @param hOther the other geometry.
 *
 * @return a new geometry representing the difference or NULL if the
 * difference is empty or an error occurs.
 */

OGRGeometryH OGR_G_Difference( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Difference", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hThis)->
        Difference(reinterpret_cast<OGRGeometry *>(hOther)));
}

/************************************************************************/
/*                        SymDifference()                               */
/************************************************************************/

/**
 * \brief Compute symmetric difference.
 *
 * Generates a new geometry which is the symmetric difference of this
 * geometry and the second geometry passed into the method.
 *
 * This method is the same as the C function OGR_G_SymDifference().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the other geometry.
 *
 * @return a new geometry representing the symmetric difference or NULL if the
 * difference is empty or an error occurs.
 *
 * @since OGR 1.8.0
 */

OGRGeometry *
OGRGeometry::SymDifference(
    UNUSED_IF_NO_GEOS const OGRGeometry *poOtherGeom ) const

{
    if (IsSFCGALCompatible() || poOtherGeom->IsSFCGALCompatible())
    {
#ifndef HAVE_SFCGAL
        CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
        return NULL;
#else
        OGRGeometry* poFirstDifference = Difference(poOtherGeom);
        if (poFirstDifference == NULL)
            return NULL;

        OGRGeometry* poOtherDifference = poOtherGeom->Difference(this);
        if (poOtherDifference == NULL)
        {
            delete poFirstDifference;
            return NULL;
        }

        OGRGeometry* poSymDiff = poFirstDifference->Union(poOtherDifference);
        delete poFirstDifference;
        delete poOtherDifference;
        return poSymDiff;
#endif
    }

#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRGeometry *poOGRProduct = NULL;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        GEOSGeom hGeosProduct =
            GEOSSymDifference_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct =
                OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct);
            if( poOGRProduct != NULL && getSpatialReference() != NULL &&
                poOtherGeom->getSpatialReference() != NULL &&
                poOtherGeom->getSpatialReference()->
                    IsSame(getSpatialReference()) )
            {
                poOGRProduct->assignSpatialReference(getSpatialReference());
            }
            poOGRProduct =
                OGRGeometryRebuildCurves(this, poOtherGeom, poOGRProduct);
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
        }
    }
    GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
    GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
    freeGEOSContext( hGEOSCtxt );

    return poOGRProduct;

#endif  // HAVE_GEOS
}

//! @cond Doxygen_Suppress
/**
 * \brief Compute symmetric difference (deprecated)
 *
 * @deprecated
 *
 * @see OGRGeometry::SymDifference()
 */
OGRGeometry *
OGRGeometry::SymmetricDifference( const OGRGeometry *poOtherGeom ) const

{
    return SymDifference( poOtherGeom );
}
//! @endcond

/************************************************************************/
/*                      OGR_G_SymDifference()                           */
/************************************************************************/

/**
 * \brief Compute symmetric difference.
 *
 * Generates a new geometry which is the symmetric difference of this
 * geometry and the other geometry.
 *
 * This function is the same as the C++ method
 * OGRGeometry::SymmetricDifference().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry.
 * @param hOther the other geometry.
 *
 * @return a new geometry representing the symmetric difference or NULL if the
 * difference is empty or an error occurs.
 *
 * @since OGR 1.8.0
 */

OGRGeometryH OGR_G_SymDifference( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_SymDifference", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hThis)->
            SymDifference(reinterpret_cast<OGRGeometry *>(hOther)));
}

/**
 * \brief Compute symmetric difference (deprecated)
 *
 * @deprecated
 *
 * @see OGR_G_SymmetricDifference()
 */
OGRGeometryH OGR_G_SymmetricDifference( OGRGeometryH hThis,
                                        OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_SymmetricDifference", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hThis)->
        SymDifference(reinterpret_cast<OGRGeometry *>(hOther)));
}

/************************************************************************/
/*                              Disjoint()                              */
/************************************************************************/

/**
 * \brief Test for disjointness.
 *
 * Tests if this geometry and the other passed into the method are disjoint.
 *
 * This method is the same as the C function OGR_G_Disjoint().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the geometry to compare to this geometry.
 *
 * @return TRUE if they are disjoint, otherwise FALSE.
 */

OGRBoolean
OGRGeometry::Disjoint( UNUSED_IF_NO_GEOS const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSDisjoint_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
    GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
    freeGEOSContext( hGEOSCtxt );

    return bResult;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                           OGR_G_Disjoint()                           */
/************************************************************************/

/**
 * \brief Test for disjointness.
 *
 * Tests if this geometry and the other geometry are disjoint.
 *
 * This function is the same as the C++ method OGRGeometry::Disjoint().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry to compare.
 * @param hOther the other geometry to compare.
 *
 * @return TRUE if they are disjoint, otherwise FALSE.
 */
int OGR_G_Disjoint( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Disjoint", FALSE );

    return reinterpret_cast<OGRGeometry *>(hThis)->
        Disjoint(reinterpret_cast<OGRGeometry *>(hOther));
}

/************************************************************************/
/*                              Touches()                               */
/************************************************************************/

/**
 * \brief Test for touching.
 *
 * Tests if this geometry and the other passed into the method are touching.
 *
 * This method is the same as the C function OGR_G_Touches().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the geometry to compare to this geometry.
 *
 * @return TRUE if they are touching, otherwise FALSE.
 */

OGRBoolean
OGRGeometry::Touches( UNUSED_IF_NO_GEOS const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);

    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSTouches_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
    GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
    freeGEOSContext( hGEOSCtxt );

    return bResult;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                           OGR_G_Touches()                            */
/************************************************************************/
/**
 * \brief Test for touching.
 *
 * Tests if this geometry and the other geometry are touching.
 *
 * This function is the same as the C++ method OGRGeometry::Touches().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry to compare.
 * @param hOther the other geometry to compare.
 *
 * @return TRUE if they are touching, otherwise FALSE.
 */

int OGR_G_Touches( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Touches", FALSE );

    return reinterpret_cast<OGRGeometry *>(hThis)->
        Touches(reinterpret_cast<OGRGeometry *>(hOther));
}

/************************************************************************/
/*                              Crosses()                               */
/************************************************************************/

/**
 * \brief Test for crossing.
 *
 * Tests if this geometry and the other passed into the method are crossing.
 *
 * This method is the same as the C function OGR_G_Crosses().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the geometry to compare to this geometry.
 *
 * @return TRUE if they are crossing, otherwise FALSE.
 */

OGRBoolean
OGRGeometry::Crosses( UNUSED_PARAMETER const OGRGeometry *poOtherGeom ) const

{
    if (IsSFCGALCompatible() || poOtherGeom->IsSFCGALCompatible())
    {
    #ifndef HAVE_SFCGAL

        CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
        return FALSE;

    #else

        sfcgal_geometry_t *poThis = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)this);
        if (poThis == NULL)
            return FALSE;

        sfcgal_geometry_t *poOther = OGRGeometry::OGRexportToSFCGAL((OGRGeometry *)poOtherGeom);
        if (poOther == NULL)
            return FALSE;

        int res = sfcgal_geometry_intersects_3d(poThis, poOther);

        sfcgal_geometry_delete(poThis);
        sfcgal_geometry_delete(poOther);

        return (res == 1)? TRUE: FALSE;

    #endif
    }

    else
    {

    #ifndef HAVE_GEOS

        CPLError( CE_Failure, CPLE_NotSupported,
                  "GEOS support not enabled." );
        return FALSE;

    #else

        GEOSGeom hThisGeosGeom = NULL;
        GEOSGeom hOtherGeosGeom = NULL;
        OGRBoolean bResult = FALSE;

        GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
        hThisGeosGeom = exportToGEOS(hGEOSCtxt);
        hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);

        if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
        {
            bResult = GEOSCrosses_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );
        }
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
        GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
        freeGEOSContext( hGEOSCtxt );

        return bResult;

    #endif /* HAVE_GEOS */
    }
}

/************************************************************************/
/*                           OGR_G_Crosses()                            */
/************************************************************************/
/**
 * \brief Test for crossing.
 *
 * Tests if this geometry and the other geometry are crossing.
 *
 * This function is the same as the C++ method OGRGeometry::Crosses().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry to compare.
 * @param hOther the other geometry to compare.
 *
 * @return TRUE if they are crossing, otherwise FALSE.
 */

int OGR_G_Crosses( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Crosses", FALSE );

    return reinterpret_cast<OGRGeometry *>(hThis)->
      Crosses(reinterpret_cast<OGRGeometry *>(hOther));
}

/************************************************************************/
/*                               Within()                               */
/************************************************************************/

/**
 * \brief Test for containment.
 *
 * Tests if actual geometry object is within the passed geometry.
 *
 * This method is the same as the C function OGR_G_Within().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the geometry to compare to this geometry.
 *
 * @return TRUE if poOtherGeom is within this geometry, otherwise FALSE.
 */

OGRBoolean
OGRGeometry::Within( UNUSED_IF_NO_GEOS const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSWithin_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
    GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
    freeGEOSContext( hGEOSCtxt );

    return bResult;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                            OGR_G_Within()                            */
/************************************************************************/

/**
 * \brief Test for containment.
 *
 * Tests if this geometry is within the other geometry.
 *
 * This function is the same as the C++ method OGRGeometry::Within().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry to compare.
 * @param hOther the other geometry to compare.
 *
 * @return TRUE if hThis is within hOther, otherwise FALSE.
 */
int OGR_G_Within( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Within", FALSE );

    return reinterpret_cast<OGRGeometry *>(hThis)->
        Within(reinterpret_cast<OGRGeometry *>(hOther));
}

/************************************************************************/
/*                              Contains()                              */
/************************************************************************/

/**
 * \brief Test for containment.
 *
 * Tests if actual geometry object contains the passed geometry.
 *
 * This method is the same as the C function OGR_G_Contains().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the geometry to compare to this geometry.
 *
 * @return TRUE if poOtherGeom contains this geometry, otherwise FALSE.
 */

OGRBoolean
OGRGeometry::Contains( UNUSED_IF_NO_GEOS const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSContains_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
    GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
    freeGEOSContext( hGEOSCtxt );

    return bResult;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                            OGR_G_Contains()                            */
/************************************************************************/

/**
 * \brief Test for containment.
 *
 * Tests if this geometry contains the other geometry.
 *
 * This function is the same as the C++ method OGRGeometry::Contains().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry to compare.
 * @param hOther the other geometry to compare.
 *
 * @return TRUE if hThis contains hOther geometry, otherwise FALSE.
 */
int OGR_G_Contains( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Contains", FALSE );

    return reinterpret_cast<OGRGeometry *>(hThis)->
      Contains(reinterpret_cast<OGRGeometry *>(hOther));
}

/************************************************************************/
/*                              Overlaps()                              */
/************************************************************************/

/**
 * \brief Test for overlap.
 *
 * Tests if this geometry and the other passed into the method overlap, that is
 * their intersection has a non-zero area.
 *
 * This method is the same as the C function OGR_G_Overlaps().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param poOtherGeom the geometry to compare to this geometry.
 *
 * @return TRUE if they are overlapping, otherwise FALSE.
 */

OGRBoolean
OGRGeometry::Overlaps( UNUSED_IF_NO_GEOS const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    hOtherGeosGeom = poOtherGeom->exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSOverlaps_r( hGEOSCtxt, hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
    GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );
    freeGEOSContext( hGEOSCtxt );

    return bResult;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                           OGR_G_Overlaps()                           */
/************************************************************************/
/**
 * \brief Test for overlap.
 *
 * Tests if this geometry and the other geometry overlap, that is their
 * intersection has a non-zero area.
 *
 * This function is the same as the C++ method OGRGeometry::Overlaps().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry to compare.
 * @param hOther the other geometry to compare.
 *
 * @return TRUE if they are overlapping, otherwise FALSE.
 */

int OGR_G_Overlaps( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Overlaps", FALSE );

    return reinterpret_cast<OGRGeometry *>(hThis)->
        Overlaps(reinterpret_cast<OGRGeometry *>(hOther));
}

/************************************************************************/
/*                             closeRings()                             */
/************************************************************************/

/**
 * \brief Force rings to be closed.
 *
 * If this geometry, or any contained geometries has polygon rings that
 * are not closed, they will be closed by adding the starting point at
 * the end.
 */

void OGRGeometry::closeRings() {}

/************************************************************************/
/*                          OGR_G_CloseRings()                          */
/************************************************************************/

/**
 * \brief Force rings to be closed.
 *
 * If this geometry, or any contained geometries has polygon rings that
 * are not closed, they will be closed by adding the starting point at
 * the end.
 *
 * @param hGeom handle to the geometry.
 */

void OGR_G_CloseRings( OGRGeometryH hGeom )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_CloseRings" );

    reinterpret_cast<OGRGeometry *>(hGeom)->closeRings();
}

/************************************************************************/
/*                              Centroid()                              */
/************************************************************************/

/**
 * \brief Compute the geometry centroid.
 *
 * The centroid location is applied to the passed in OGRPoint object.
 * The centroid is not necessarily within the geometry.
 *
 * This method relates to the SFCOM ISurface::get_Centroid() method
 * however the current implementation based on GEOS can operate on other
 * geometry types such as multipoint, linestring, geometrycollection such as
 * multipolygons.
 * OGC SF SQL 1.1 defines the operation for surfaces (polygons).
 * SQL/MM-Part 3 defines the operation for surfaces and multisurfaces
 * (multipolygons).
 *
 * This function is the same as the C function OGR_G_Centroid().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @return OGRERR_NONE on success or OGRERR_FAILURE on error.
 *
 * @since OGR 1.8.0 as a OGRGeometry method (previously was restricted
 * to OGRPolygon)
 */

OGRErr OGRGeometry::Centroid( OGRPoint *poPoint ) const

{
    if( poPoint == NULL )
        return OGRERR_FAILURE;

#ifndef HAVE_GEOS
    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return OGRERR_FAILURE;

#else

    GEOSGeom hThisGeosGeom = NULL;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);

    if( hThisGeosGeom != NULL )
    {
        GEOSGeom hOtherGeosGeom = GEOSGetCentroid_r( hGEOSCtxt, hThisGeosGeom );
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );

        if( hOtherGeosGeom == NULL )
        {
            freeGEOSContext( hGEOSCtxt );
            return OGRERR_FAILURE;
        }

        OGRGeometry *poCentroidGeom =
            OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hOtherGeosGeom );

        GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );

        if( poCentroidGeom == NULL )
        {
            freeGEOSContext( hGEOSCtxt );
            return OGRERR_FAILURE;
        }
        if( wkbFlatten(poCentroidGeom->getGeometryType()) != wkbPoint )
        {
            delete poCentroidGeom;
            freeGEOSContext( hGEOSCtxt );
            return OGRERR_FAILURE;
        }

        if( poCentroidGeom != NULL && getSpatialReference() != NULL )
            poCentroidGeom->assignSpatialReference(getSpatialReference());

        OGRPoint *poCentroid = dynamic_cast<OGRPoint *>(poCentroidGeom);
        if( poCentroid == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "dynamic_cast failed.  Expected OGRPoint.");
            delete poCentroidGeom;
            freeGEOSContext( hGEOSCtxt );
            return OGRERR_FAILURE;
        }

        if( !poCentroid->IsEmpty() )
        {
            poPoint->setX( poCentroid->getX() );
            poPoint->setY( poCentroid->getY() );
        }
        else
        {
            poPoint->empty();
        }

        delete poCentroidGeom;

        freeGEOSContext( hGEOSCtxt );
        return OGRERR_NONE;
    }
    else
    {
        freeGEOSContext( hGEOSCtxt );
        return OGRERR_FAILURE;
    }

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                           OGR_G_Centroid()                           */
/************************************************************************/

/**
 * \brief Compute the geometry centroid.
 *
 * The centroid location is applied to the passed in OGRPoint object.
 * The centroid is not necessarily within the geometry.
 *
 * This method relates to the SFCOM ISurface::get_Centroid() method
 * however the current implementation based on GEOS can operate on other
 * geometry types such as multipoint, linestring, geometrycollection such as
 * multipolygons.
 * OGC SF SQL 1.1 defines the operation for surfaces (polygons).
 * SQL/MM-Part 3 defines the operation for surfaces and multisurfaces (multipolygons).
 *
 * This function is the same as the C++ method OGRGeometry::Centroid().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @return OGRERR_NONE on success or OGRERR_FAILURE on error.
 */

int OGR_G_Centroid( OGRGeometryH hGeom, OGRGeometryH hCentroidPoint )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Centroid", OGRERR_FAILURE );

    OGRGeometry *poGeom = reinterpret_cast<OGRGeometry *>(hGeom);
    OGRPoint *poCentroid = reinterpret_cast<OGRPoint *>(hCentroidPoint);

    if( poCentroid == NULL )
        return OGRERR_FAILURE;

    if( wkbFlatten(poCentroid->getGeometryType()) != wkbPoint )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Passed wrong geometry type as centroid argument." );
        return OGRERR_FAILURE;
    }

    return poGeom->Centroid( poCentroid );
}

/************************************************************************/
/*                        OGR_G_PointOnSurface()                        */
/************************************************************************/

/**
 * \brief Returns a point guaranteed to lie on the surface.
 *
 * This method relates to the SFCOM ISurface::get_PointOnSurface() method
 * however the current implementation based on GEOS can operate on other
 * geometry types than the types that are supported by SQL/MM-Part 3 :
 * surfaces (polygons) and multisurfaces (multipolygons).
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hGeom the geometry to operate on.
 * @return a point guaranteed to lie on the surface or NULL if an error
 *         occurred.
 *
 * @since OGR 1.10
 */

OGRGeometryH OGR_G_PointOnSurface( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_PointOnSurface", NULL );

#ifndef HAVE_GEOS
    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;
#else
    GEOSGeom hThisGeosGeom = NULL;
    OGRGeometry* poThis = reinterpret_cast<OGRGeometry *>(hGeom);

    GEOSContextHandle_t hGEOSCtxt = OGRGeometry::createGEOSContext();
    hThisGeosGeom = poThis->exportToGEOS(hGEOSCtxt);

    if( hThisGeosGeom != NULL )
    {
        GEOSGeom hOtherGeosGeom =
            GEOSPointOnSurface_r( hGEOSCtxt, hThisGeosGeom );
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );

        if( hOtherGeosGeom == NULL )
        {
            OGRGeometry::freeGEOSContext( hGEOSCtxt );
            return NULL;
        }

        OGRGeometry *poInsidePointGeom =
            OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hOtherGeosGeom );

        GEOSGeom_destroy_r( hGEOSCtxt, hOtherGeosGeom );

        if( poInsidePointGeom == NULL )
        {
            OGRGeometry::freeGEOSContext( hGEOSCtxt );
            return NULL;
        }
        if( wkbFlatten(poInsidePointGeom->getGeometryType()) != wkbPoint )
        {
            delete poInsidePointGeom;
            OGRGeometry::freeGEOSContext( hGEOSCtxt );
            return NULL;
        }

        if( poInsidePointGeom != NULL && poThis->getSpatialReference() != NULL )
            poInsidePointGeom->
                assignSpatialReference(poThis->getSpatialReference());

        OGRGeometry::freeGEOSContext( hGEOSCtxt );
        return reinterpret_cast<OGRGeometryH>(poInsidePointGeom);
    }

    OGRGeometry::freeGEOSContext( hGEOSCtxt );
    return NULL;
#endif
}

/************************************************************************/
/*                          PointOnSurfaceInternal()                    */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRErr OGRGeometry::PointOnSurfaceInternal( OGRPoint * poPoint ) const
{
    if( poPoint == NULL || poPoint->IsEmpty() )
        return OGRERR_FAILURE;

    OGRGeometryH hInsidePoint =
      OGR_G_PointOnSurface(
          reinterpret_cast<OGRGeometryH>(const_cast<OGRGeometry *>(this)));
    if( hInsidePoint == NULL )
        return OGRERR_FAILURE;

    OGRPoint *poInsidePoint = reinterpret_cast<OGRPoint *>(hInsidePoint);
    if( poInsidePoint->IsEmpty() )
    {
        poPoint->empty();
    }
    else
    {
        poPoint->setX( poInsidePoint->getX() );
        poPoint->setY( poInsidePoint->getY() );
    }

    return OGRERR_NONE;
}
//! @endcond

/************************************************************************/
/*                              Simplify()                              */
/************************************************************************/

/**
 * \brief Simplify the geometry.
 *
 * This function is the same as the C function OGR_G_Simplify().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param dTolerance the distance tolerance for the simplification.
 *
 * @return the simplified geometry or NULL if an error occurs.
 *
 * @since OGR 1.8.0
 */

OGRGeometry *OGRGeometry::Simplify( UNUSED_IF_NO_GEOS double dTolerance ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;

#else
    OGRGeometry *poOGRProduct = NULL;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    GEOSGeom hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL )
    {
        GEOSGeom hGeosProduct =
            GEOSSimplify_r( hGEOSCtxt, hThisGeosGeom, dTolerance );
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
        if( hGeosProduct != NULL )
        {
            poOGRProduct =
                OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct );
            if( poOGRProduct != NULL && getSpatialReference() != NULL )
                poOGRProduct->assignSpatialReference(getSpatialReference());
            poOGRProduct = OGRGeometryRebuildCurves(this, NULL, poOGRProduct);
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
        }
    }
    freeGEOSContext( hGEOSCtxt );
    return poOGRProduct;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                         OGR_G_Simplify()                             */
/************************************************************************/

/**
 * \brief Compute a simplified geometry.
 *
 * This function is the same as the C++ method OGRGeometry::Simplify().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry.
 * @param dTolerance the distance tolerance for the simplification.
 *
 * @return the simplified geometry or NULL if an error occurs.
 *
 * @since OGR 1.8.0
 */

OGRGeometryH OGR_G_Simplify( OGRGeometryH hThis, double dTolerance )

{
    VALIDATE_POINTER1( hThis, "OGR_G_Simplify", NULL );
    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hThis)->Simplify(dTolerance));
}

/************************************************************************/
/*                         SimplifyPreserveTopology()                   */
/************************************************************************/

/**
 * \brief Simplify the geometry while preserving topology.
 *
 * This function is the same as the C function OGR_G_SimplifyPreserveTopology().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param dTolerance the distance tolerance for the simplification.
 *
 * @return the simplified geometry or NULL if an error occurs.
 *
 * @since OGR 1.9.0
 */

OGRGeometry *OGRGeometry::SimplifyPreserveTopology(
    UNUSED_IF_NO_GEOS double dTolerance ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;

#else
    GEOSGeom hThisGeosGeom = NULL;
    OGRGeometry *poOGRProduct = NULL;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL )
    {
        GEOSGeom hGeosProduct =
            GEOSTopologyPreserveSimplify_r( hGEOSCtxt, hThisGeosGeom,
                                            dTolerance );
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
        if( hGeosProduct != NULL )
        {
            poOGRProduct =
                OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct );
            if( poOGRProduct != NULL && getSpatialReference() != NULL )
                poOGRProduct->assignSpatialReference(getSpatialReference());
            poOGRProduct = OGRGeometryRebuildCurves(this, NULL, poOGRProduct);
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
        }
    }
    freeGEOSContext( hGEOSCtxt );
    return poOGRProduct;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                     OGR_G_SimplifyPreserveTopology()                 */
/************************************************************************/

/**
 * \brief Simplify the geometry while preserving topology.
 *
 * This function is the same as the C++ method
 * OGRGeometry::SimplifyPreserveTopology().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry.
 * @param dTolerance the distance tolerance for the simplification.
 *
 * @return the simplified geometry or NULL if an error occurs.
 *
 * @since OGR 1.9.0
 */

OGRGeometryH OGR_G_SimplifyPreserveTopology( OGRGeometryH hThis,
                                             double dTolerance )

{
    VALIDATE_POINTER1( hThis, "OGR_G_SimplifyPreserveTopology", NULL );
    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hThis)->
            SimplifyPreserveTopology(dTolerance));
}

/************************************************************************/
/*                         DelaunayTriangulation()                      */
/************************************************************************/

/**
 * \brief Return a Delaunay triangulation of the vertices of the geometry.
 *
 * This function is the same as the C function OGR_G_DelaunayTriangulation().
 *
 * This function is built on the GEOS library, v3.4 or above.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param dfTolerance optional snapping tolerance to use for improved robustness
 * @param bOnlyEdges if TRUE, will return a MULTILINESTRING, otherwise it will
 *                   return a GEOMETRYCOLLECTION containing triangular POLYGONs.
 *
 * @return the geometry resulting from the Delaunay triangulation or
 * NULL if an error occurs.
 *
 * @since OGR 2.1
 */

#ifndef HAVE_GEOS
OGRGeometry *OGRGeometry::DelaunayTriangulation(double /*dfTolerance*/,
                                                int /*bOnlyEdges*/) const
{
    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;
}
#elif GEOS_VERSION_MAJOR < 3 || \
    (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR < 4)
OGRGeometry *OGRGeometry::DelaunayTriangulation(double /*dfTolerance*/,
                                                int /*bOnlyEdges*/) const
{
    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS 3.4 or later needed for DelaunayTriangulation." );
    return NULL;
}
#else
OGRGeometry *OGRGeometry::DelaunayTriangulation( double dfTolerance,
                                                 int bOnlyEdges ) const
{
    GEOSGeom hThisGeosGeom = NULL;
    OGRGeometry *poOGRProduct = NULL;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();
    hThisGeosGeom = exportToGEOS(hGEOSCtxt);
    if( hThisGeosGeom != NULL )
    {
        GEOSGeom hGeosProduct =
            GEOSDelaunayTriangulation_r( hGEOSCtxt, hThisGeosGeom, dfTolerance,
                                         bOnlyEdges );
        GEOSGeom_destroy_r( hGEOSCtxt, hThisGeosGeom );
        if( hGeosProduct != NULL )
        {
            poOGRProduct =
                OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosProduct );
            if( poOGRProduct != NULL && getSpatialReference() != NULL )
                poOGRProduct->assignSpatialReference(getSpatialReference());
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosProduct );
        }
    }
    freeGEOSContext( hGEOSCtxt );
    return poOGRProduct;
}
#endif

/************************************************************************/
/*                     OGR_G_DelaunayTriangulation()                    */
/************************************************************************/

/**
 * \brief Return a Delaunay triangulation of the vertices of the geometry.
 *
 * This function is the same as the C++ method
 * OGRGeometry::DelaunayTriangulation().
 *
 * This function is built on the GEOS library, v3.4 or above.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hThis the geometry.
 * @param dfTolerance optional snapping tolerance to use for improved robustness
 * @param bOnlyEdges if TRUE, will return a MULTILINESTRING, otherwise it will
 *                   return a GEOMETRYCOLLECTION containing triangular POLYGONs.
 *
 * @return the geometry resulting from the Delaunay triangulation or
 * NULL if an error occurs.
 *
 * @since OGR 2.1
 */

OGRGeometryH OGR_G_DelaunayTriangulation( OGRGeometryH hThis,
                                          double dfTolerance,
                                          int bOnlyEdges )

{
    VALIDATE_POINTER1( hThis, "OGR_G_DelaunayTriangulation", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hThis)->
            DelaunayTriangulation(dfTolerance, bOnlyEdges));
}

/************************************************************************/
/*                             Polygonize()                             */
/************************************************************************/
/* Contributor: Alessandro Furieri, a.furieri@lqt.it                    */
/* Developed for Faunalia (http://www.faunalia.it) with funding from    */
/* Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED        */
/*                   AMBIENTALE                                         */
/************************************************************************/

/**
 * \brief Polygonizes a set of sparse edges.
 *
 * A new geometry object is created and returned containing a collection
 * of reassembled Polygons: NULL will be returned if the input collection
 * doesn't corresponds to a MultiLinestring, or when reassembling Edges
 * into Polygons is impossible due to topological inconsistencies.
 *
 * This method is the same as the C function OGR_G_Polygonize().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @return a newly allocated geometry now owned by the caller, or NULL
 * on failure.
 *
 * @since OGR 1.9.0
 */

OGRGeometry *OGRGeometry::Polygonize() const

{
    if (EQUAL(getGeometryName(), "TRIANGLE"))
    {
        OGRPolygon *poPolygon = new OGRPolygon(*((OGRPolygon *)this));
        return poPolygon;
    }

#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;

#else

    OGRGeometryCollection *poColl = NULL;
    if( wkbFlatten(getGeometryType()) == wkbGeometryCollection ||
        wkbFlatten(getGeometryType()) == wkbMultiLineString )
        poColl = (OGRGeometryCollection *)this;
    else
        return NULL;

    int iCount = poColl->getNumGeometries();

    GEOSGeom *hGeosGeomList = NULL;
    OGRGeometry *poPolygsOGRGeom = NULL;
    bool bError = false;

    GEOSContextHandle_t hGEOSCtxt = createGEOSContext();

    hGeosGeomList = new GEOSGeom [iCount];
    for( int ig = 0; ig < iCount; ig++ )
    {
        GEOSGeom hGeosGeom = NULL;
        OGRGeometry *poChild = poColl->getGeometryRef(ig);
        if( poChild == NULL ||
            wkbFlatten(poChild->getGeometryType()) != wkbLineString )
            bError = true;
        else
        {
            hGeosGeom = poChild->exportToGEOS(hGEOSCtxt);
            if( hGeosGeom == NULL)
                bError = true;
        }
        *(hGeosGeomList + ig) = hGeosGeom;
    }

    if( !bError )
    {
        GEOSGeom hGeosPolygs =
            GEOSPolygonize_r(hGEOSCtxt, hGeosGeomList, iCount);

        if( hGeosPolygs != NULL )
        {
            poPolygsOGRGeom =
                OGRGeometryFactory::createFromGEOS(hGEOSCtxt, hGeosPolygs);
            if( poPolygsOGRGeom != NULL && getSpatialReference() != NULL )
                poPolygsOGRGeom->assignSpatialReference(getSpatialReference());
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosPolygs);
        }
    }

    for( int ig = 0; ig < iCount; ig++ )
    {
        GEOSGeom hGeosGeom = *(hGeosGeomList + ig);
        if( hGeosGeom != NULL)
            GEOSGeom_destroy_r( hGEOSCtxt, hGeosGeom );
    }
    delete [] hGeosGeomList;
    freeGEOSContext( hGEOSCtxt );

    return poPolygsOGRGeom;

#endif  // HAVE_GEOS
}

/************************************************************************/
/*                          OGR_G_Polygonize()                          */
/************************************************************************/
/**
 * \brief Polygonizes a set of sparse edges.
 *
 * A new geometry object is created and returned containing a collection
 * of reassembled Polygons: NULL will be returned if the input collection
 * doesn't corresponds to a MultiLinestring, or when reassembling Edges
 * into Polygons is impossible due to topological inconsistencies.
 *
 * This function is the same as the C++ method OGRGeometry::Polygonize().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail,
 * issuing a CPLE_NotSupported error.
 *
 * @param hTarget The Geometry to be polygonized.
 *
 * @return a handle to a newly allocated geometry now owned by the caller,
 *         or NULL on failure.
 *
 * @since OGR 1.9.0
 */

OGRGeometryH OGR_G_Polygonize( OGRGeometryH hTarget )

{
    VALIDATE_POINTER1( hTarget, "OGR_G_Polygonize", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hTarget)->Polygonize());
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

/**
 * \brief Swap x and y coordinates.
 *
 * @since OGR 1.8.0
 */

void OGRGeometry::swapXY()

{
}

/************************************************************************/
/*                        Prepared geometry API                         */
/************************************************************************/

/* GEOS >= 3.1.0 for prepared geometries */
#if defined(HAVE_GEOS)
#define HAVE_GEOS_PREPARED_GEOMETRY
#endif

#ifdef HAVE_GEOS_PREPARED_GEOMETRY
struct _OGRPreparedGeometry
{
    GEOSContextHandle_t           hGEOSCtxt;
    GEOSGeom                      hGEOSGeom;
    const GEOSPreparedGeometry*   poPreparedGEOSGeom;
};
#endif

/************************************************************************/
/*                       OGRHasPreparedGeometrySupport()                */
/************************************************************************/

/** Returns if GEOS has prepared geometry support.
 * @return TRUE or FALSE
 */
int OGRHasPreparedGeometrySupport()
{
#ifdef HAVE_GEOS_PREPARED_GEOMETRY
    return TRUE;
#else
    return FALSE;
#endif
}

/************************************************************************/
/*                         OGRCreatePreparedGeometry()                  */
/************************************************************************/

/** Creates a prepared geometry.
 *
 * To free with OGRDestroyPreparedGeometry()
 *
 * @param poGeom input geometry to prepare.
 * @return handle to a prepared geometry.
 */
OGRPreparedGeometry* OGRCreatePreparedGeometry( UNUSED_IF_NO_GEOS const OGRGeometry* poGeom )
{
#ifdef HAVE_GEOS_PREPARED_GEOMETRY
    GEOSContextHandle_t hGEOSCtxt = OGRGeometry::createGEOSContext();
    GEOSGeom hGEOSGeom = poGeom->exportToGEOS(hGEOSCtxt);
    if( hGEOSGeom == NULL )
    {
        OGRGeometry::freeGEOSContext( hGEOSCtxt );
        return NULL;
    }
    const GEOSPreparedGeometry* poPreparedGEOSGeom = GEOSPrepare_r(hGEOSCtxt, hGEOSGeom);
    if( poPreparedGEOSGeom == NULL )
    {
        GEOSGeom_destroy_r( hGEOSCtxt, hGEOSGeom );
        OGRGeometry::freeGEOSContext( hGEOSCtxt );
        return NULL;
    }

    OGRPreparedGeometry* poPreparedGeom = new OGRPreparedGeometry;
    poPreparedGeom->hGEOSCtxt = hGEOSCtxt;
    poPreparedGeom->hGEOSGeom = hGEOSGeom;
    poPreparedGeom->poPreparedGEOSGeom = poPreparedGEOSGeom;

    return poPreparedGeom;
#else
    return NULL;
#endif
}

/************************************************************************/
/*                        OGRDestroyPreparedGeometry()                  */
/************************************************************************/

/** Destroys a prepared geometry.
 * @param poPreparedGeom preprated geometry.
 */
void OGRDestroyPreparedGeometry( UNUSED_IF_NO_GEOS OGRPreparedGeometry* poPreparedGeom )
{
#ifdef HAVE_GEOS_PREPARED_GEOMETRY
    if( poPreparedGeom != NULL )
    {
        GEOSPreparedGeom_destroy_r(poPreparedGeom->hGEOSCtxt, poPreparedGeom->poPreparedGEOSGeom);
        GEOSGeom_destroy_r( poPreparedGeom->hGEOSCtxt, poPreparedGeom->hGEOSGeom );
        OGRGeometry::freeGEOSContext( poPreparedGeom->hGEOSCtxt );
        delete poPreparedGeom;
    }
#endif
}

/************************************************************************/
/*                      OGRPreparedGeometryIntersects()                 */
/************************************************************************/

/** Returns whether a prepared geometry intersects with a geometry.
 * @param poPreparedGeom prepared geometry.
 * @param poOtherGeom other geometry.
 * @return TRUE or FALSE.
 */
int OGRPreparedGeometryIntersects(
    UNUSED_IF_NO_GEOS const OGRPreparedGeometry* poPreparedGeom,
    UNUSED_IF_NO_GEOS const OGRGeometry* poOtherGeom )
{
#ifdef HAVE_GEOS_PREPARED_GEOMETRY
    if( poPreparedGeom == NULL || poOtherGeom == NULL )
        return FALSE;

    GEOSGeom hGEOSOtherGeom =
        poOtherGeom->exportToGEOS(poPreparedGeom->hGEOSCtxt);
    if( hGEOSOtherGeom == NULL )
        return FALSE;

    const bool bRet = CPL_TO_BOOL(
        GEOSPreparedIntersects_r(poPreparedGeom->hGEOSCtxt,
                                 poPreparedGeom->poPreparedGEOSGeom,
                                 hGEOSOtherGeom));
    GEOSGeom_destroy_r( poPreparedGeom->hGEOSCtxt, hGEOSOtherGeom );

    return bRet;
#else
    return FALSE;
#endif
}

/** Returns whether a prepared geometry contains a geometry.
 * @param poPreparedGeom prepared geometry.
 * @param poOtherGeom other geometry.
 * @return TRUE or FALSE.
 */
int OGRPreparedGeometryContains(
    UNUSED_IF_NO_GEOS const OGRPreparedGeometry* poPreparedGeom,
    UNUSED_IF_NO_GEOS const OGRGeometry* poOtherGeom )
{
#ifdef HAVE_GEOS_PREPARED_GEOMETRY
    if( poPreparedGeom == NULL || poOtherGeom == NULL )
        return FALSE;

    GEOSGeom hGEOSOtherGeom =
        poOtherGeom->exportToGEOS(poPreparedGeom->hGEOSCtxt);
    if( hGEOSOtherGeom == NULL )
        return FALSE;

    const bool bRet = CPL_TO_BOOL(
        GEOSPreparedContains_r(poPreparedGeom->hGEOSCtxt,
                               poPreparedGeom->poPreparedGEOSGeom,
                               hGEOSOtherGeom));
    GEOSGeom_destroy_r( poPreparedGeom->hGEOSCtxt, hGEOSOtherGeom );

    return bRet;
#else
    return FALSE;
#endif
}

/************************************************************************/
/*                       OGRGeometryFromEWKB()                          */
/************************************************************************/

/* Flags for creating WKB format for PostGIS */
// #define WKBZOFFSET 0x80000000
// #define WKBMOFFSET 0x40000000
#define WKBSRIDFLAG 0x20000000
// #define WKBBBOXFLAG 0x10000000

OGRGeometry *OGRGeometryFromEWKB( GByte *pabyWKB, int nLength, int* pnSRID,
                                  int bIsPostGIS1_EWKB )

{
    OGRGeometry *poGeometry = NULL;

    if( nLength < 5 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid EWKB content : %d bytes", nLength );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Detect byte order                                               */
/* -------------------------------------------------------------------- */
    OGRwkbByteOrder eByteOrder = (pabyWKB[0] == 0 ? wkbXDR : wkbNDR);

/* -------------------------------------------------------------------- */
/*      PostGIS EWKB format includes an SRID, but this won't be         */
/*      understood by OGR, so if the SRID flag is set, we remove the    */
/*      SRID (bytes at offset 5 to 8).                                  */
/* -------------------------------------------------------------------- */
    if( nLength > 9 &&
        ((pabyWKB[0] == 0 /* big endian */ && (pabyWKB[1] & 0x20) )
        || (pabyWKB[0] != 0 /* little endian */ && (pabyWKB[4] & 0x20))) )
    {
        if( pnSRID )
        {
            memcpy(pnSRID, pabyWKB+5, 4);
            if( OGR_SWAP( eByteOrder ) )
                *pnSRID = CPL_SWAP32(*pnSRID);
        }
        memmove( pabyWKB+5, pabyWKB+9, nLength-9 );
        nLength -= 4;
        if( pabyWKB[0] == 0 )
            pabyWKB[1] &= (~0x20);
        else
            pabyWKB[4] &= (~0x20);
    }

/* -------------------------------------------------------------------- */
/*      Try to ingest the geometry.                                     */
/* -------------------------------------------------------------------- */
    (void) OGRGeometryFactory::createFromWkb( pabyWKB, NULL, &poGeometry, nLength,
                                              (bIsPostGIS1_EWKB) ? wkbVariantPostGIS1 : wkbVariantOldOgc );

    return poGeometry;
}

/************************************************************************/
/*                     OGRGeometryFromHexEWKB()                         */
/************************************************************************/

OGRGeometry *OGRGeometryFromHexEWKB( const char *pszBytea, int* pnSRID,
                                     int bIsPostGIS1_EWKB )

{
    if( pszBytea == NULL )
        return NULL;

    int nWKBLength = 0;
    GByte *pabyWKB = CPLHexToBinary(pszBytea, &nWKBLength);

    OGRGeometry *poGeometry =
        OGRGeometryFromEWKB(pabyWKB, nWKBLength, pnSRID, bIsPostGIS1_EWKB);

    CPLFree(pabyWKB);

    return poGeometry;
}

/************************************************************************/
/*                       OGRGeometryToHexEWKB()                         */
/************************************************************************/

char* OGRGeometryToHexEWKB( OGRGeometry * poGeometry, int nSRSId,
                            int nPostGISMajor, int nPostGISMinor )
{
    const int nWkbSize = poGeometry->WkbSize();
    GByte *pabyWKB = static_cast<GByte *>(CPLMalloc(nWkbSize));

    if( (nPostGISMajor > 2 || (nPostGISMajor == 2 && nPostGISMinor >= 2)) &&
        wkbFlatten(poGeometry->getGeometryType()) == wkbPoint &&
        poGeometry->IsEmpty() )
    {
        if( poGeometry->exportToWkb( wkbNDR, pabyWKB,
                                     wkbVariantIso ) != OGRERR_NONE )
        {
            CPLFree( pabyWKB );
            return CPLStrdup("");
        }
    }
    else if( poGeometry->exportToWkb( wkbNDR, pabyWKB,
            (nPostGISMajor < 2) ? wkbVariantPostGIS1 : wkbVariantOldOgc ) != OGRERR_NONE )
    {
        CPLFree( pabyWKB );
        return CPLStrdup("");
    }

    // When converting to hex, each byte takes 2 hex characters.  In addition
    // we add in 8 characters to represent the SRID integer in hex, and
    // one for a null terminator.

    const int pszSize = nWkbSize * 2 + 8 + 1;
    char *pszTextBuf = static_cast<char *>(CPLMalloc(pszSize));
    char *pszTextBufCurrent = pszTextBuf;

    // Convert the 1st byte, which is the endianness flag, to hex.
    char *pszHex = CPLBinaryToHex( 1, pabyWKB );
    strcpy(pszTextBufCurrent, pszHex );
    CPLFree ( pszHex );
    pszTextBufCurrent += 2;

    // Next, get the geom type which is bytes 2 through 5.
    GUInt32 geomType;
    memcpy( &geomType, pabyWKB+1, 4 );

    // Now add the SRID flag if an SRID is provided.
    if( nSRSId > 0 )
    {
        // Change the flag to wkbNDR (little) endianness.
        GUInt32 nGSrsFlag = CPL_LSBWORD32( WKBSRIDFLAG );
        // Apply the flag.
        geomType = geomType | nGSrsFlag;
    }

    // Now write the geom type which is 4 bytes.
    pszHex = CPLBinaryToHex( 4, (GByte*) &geomType );
    strcpy(pszTextBufCurrent, pszHex );
    CPLFree ( pszHex );
    pszTextBufCurrent += 8;

    // Now include SRID if provided.
    if( nSRSId > 0 )
    {
        // Force the srsid to wkbNDR (little) endianness.
        const GUInt32 nGSRSId = CPL_LSBWORD32( nSRSId );
        pszHex = CPLBinaryToHex( sizeof(nGSRSId), (GByte*) &nGSRSId );
        strcpy(pszTextBufCurrent, pszHex );
        CPLFree ( pszHex );
        pszTextBufCurrent += 8;
    }

    // Copy the rest of the data over - subtract
    // 5 since we already copied 5 bytes above.
    pszHex = CPLBinaryToHex( nWkbSize - 5, pabyWKB + 5 );
    strcpy(pszTextBufCurrent, pszHex );
    CPLFree ( pszHex );

    CPLFree( pabyWKB );

    return pszTextBuf;
}

/**
 * \fn void OGRGeometry::segmentize(double dfMaxLength);
 *
 * \brief Add intermediate vertices to a geometry.
 *
 * This method modifies the geometry to add intermediate vertices if necessary
 * so that the maximum length between 2 consecutive vertices is lower than
 * dfMaxLength.
 *
 * @param dfMaxLength maximum length between 2 consecutive vertices.
 */

/************************************************************************/
/*                       importPreambuleFromWkb()                       */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRErr OGRGeometry::importPreambuleFromWkb( const unsigned char * pabyData,
                                            int nSize,
                                            OGRwkbByteOrder& eByteOrder,
                                            OGRwkbVariant eWkbVariant )
{
    if( nSize < 9 && nSize != -1 )
        return OGRERR_NOT_ENOUGH_DATA;

/* -------------------------------------------------------------------- */
/*      Get the byte order byte.                                        */
/* -------------------------------------------------------------------- */
    int nByteOrder = DB2_V72_FIX_BYTE_ORDER(*pabyData);
    if( !( nByteOrder == wkbXDR || nByteOrder == wkbNDR ) )
        return OGRERR_CORRUPT_DATA;
    eByteOrder = (OGRwkbByteOrder) nByteOrder;

/* -------------------------------------------------------------------- */
/*      Get the geometry feature type.                                  */
/* -------------------------------------------------------------------- */
    OGRwkbGeometryType eGeometryType;
    const OGRErr err =
        OGRReadWKBGeometryType( pabyData, eWkbVariant, &eGeometryType );
    if( wkbHasZ(eGeometryType) )
        flags |= OGR_G_3D;
    if( wkbHasM(eGeometryType) )
        flags |= OGR_G_MEASURED;

    if( err != OGRERR_NONE || eGeometryType != getGeometryType() )
        return OGRERR_CORRUPT_DATA;

    return OGRERR_NONE;
}

/************************************************************************/
/*                    importPreambuleOfCollectionFromWkb()              */
/*                                                                      */
/*      Utility method for OGRSimpleCurve, OGRCompoundCurve,            */
/*      OGRCurvePolygon and OGRGeometryCollection.                      */
/************************************************************************/

OGRErr OGRGeometry::importPreambuleOfCollectionFromWkb( const unsigned char * pabyData,
                                                        int& nSize,
                                                        int& nDataOffset,
                                                        OGRwkbByteOrder& eByteOrder,
                                                        int nMinSubGeomSize,
                                                        int& nGeomCount,
                                                        OGRwkbVariant eWkbVariant )
{
    nGeomCount = 0;

    OGRErr eErr = importPreambuleFromWkb( pabyData, nSize, eByteOrder, eWkbVariant );
    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Clear existing Geoms.                                           */
/* -------------------------------------------------------------------- */
    int _flags = flags; // flags set in importPreambuleFromWkb
    empty(); // may reset flags etc.

    // restore
    if( _flags & OGR_G_3D )
        set3D(TRUE);
    if( _flags & OGR_G_MEASURED )
        setMeasured(TRUE);

/* -------------------------------------------------------------------- */
/*      Get the sub-geometry count.                                     */
/* -------------------------------------------------------------------- */
    memcpy( &nGeomCount, pabyData + 5, 4 );

    if( OGR_SWAP( eByteOrder ) )
        nGeomCount = CPL_SWAP32(nGeomCount);

    if( nGeomCount < 0 || nGeomCount > INT_MAX / nMinSubGeomSize )
    {
        nGeomCount = 0;
        return OGRERR_CORRUPT_DATA;
    }

    // Each ring has a minimum of nMinSubGeomSize bytes.
    if( nSize != -1 && nSize - 9 < nGeomCount * nMinSubGeomSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Length of input WKB is too small" );
        nGeomCount = 0;
        return OGRERR_NOT_ENOUGH_DATA;
    }

    nDataOffset = 9;
    if( nSize != -1 )
    {
        CPLAssert( nSize >= nDataOffset );
        nSize -= nDataOffset;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                      importCurveCollectionFromWkt()                  */
/*                                                                      */
/*      Utility method for OGRCompoundCurve, OGRCurvePolygon and        */
/*      OGRMultiCurve.                                                  */
/************************************************************************/

OGRErr OGRGeometry::importCurveCollectionFromWkt(
    char ** ppszInput,
    int bAllowEmptyComponent,
    int bAllowLineString,
    int bAllowCurve,
    int bAllowCompoundCurve,
    OGRErr (*pfnAddCurveDirectly)(OGRGeometry* poSelf, OGRCurve* poCurve) )

{
    int bHasZ = FALSE;
    int bHasM = FALSE;
    bool bIsEmpty = false;
    OGRErr eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    flags = 0;
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bHasZ ) flags |= OGR_G_3D;
    if( bHasM ) flags |= OGR_G_MEASURED;
    if( bIsEmpty )
        return OGRERR_NONE;

    char szToken[OGR_WKT_TOKEN_MAX];
    const char *pszInput = *ppszInput;
    eErr = OGRERR_NONE;

    // Skip first '('.
    pszInput = OGRWktReadToken( pszInput, szToken );

/* ==================================================================== */
/*      Read each curve in turn.  Note that we try to reuse the same    */
/*      point list buffer from curve to curve to cut down on            */
/*      allocate/deallocate overhead.                                   */
/* ==================================================================== */
    OGRRawPoint *paoPoints = NULL;
    int nMaxPoints = 0;
    double *padfZ = NULL;

    do
    {

    /* -------------------------------------------------------------------- */
    /*      Get the first token, which should be the geometry type.         */
    /* -------------------------------------------------------------------- */
        const char* pszInputBefore = pszInput;
        pszInput = OGRWktReadToken( pszInput, szToken );

    /* -------------------------------------------------------------------- */
    /*      Do the import.                                                  */
    /* -------------------------------------------------------------------- */
        OGRCurve* poCurve = NULL;
        if( EQUAL(szToken, "(") )
        {
            OGRLineString* poLine = new OGRLineString();
            poCurve = poLine;
            pszInput = pszInputBefore;
            eErr = poLine->importFromWKTListOnly(
                const_cast<char **>(&pszInput),
                bHasZ, bHasM,
                paoPoints, nMaxPoints,
                padfZ );
        }
        else if( bAllowEmptyComponent && EQUAL(szToken, "EMPTY") )
        {
            poCurve = new OGRLineString();
        }
        // Accept LINESTRING(), but this is an extension to the BNF, also
        // accepted by PostGIS.
        else if( (bAllowLineString && EQUAL(szToken, "LINESTRING")) ||
                 (bAllowCurve && !EQUAL(szToken, "LINESTRING") &&
                  !EQUAL(szToken, "COMPOUNDCURVE") &&
                  OGR_GT_IsCurve(OGRFromOGCGeomType(szToken))) ||
                 (bAllowCompoundCurve && EQUAL(szToken, "COMPOUNDCURVE")) )
        {
            OGRGeometry* poGeom = NULL;
            pszInput = pszInputBefore;
            eErr = OGRGeometryFactory::createFromWkt(
                const_cast<char **>(&pszInput),
                NULL, &poGeom );
            poCurve = dynamic_cast<OGRCurve *>(poGeom);
            if( poCurve == NULL )
            {
                delete poGeom;
                eErr = OGRERR_CORRUPT_DATA;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected token : %s", szToken);
            eErr = OGRERR_CORRUPT_DATA;
        }

        // If this has M it is an error if poGeom does not have M.
        if( poCurve && !Is3D() && IsMeasured() && !poCurve->IsMeasured() )
            eErr = OGRERR_CORRUPT_DATA;

        if( eErr == OGRERR_NONE )
            eErr = pfnAddCurveDirectly( this, poCurve );
        if( eErr != OGRERR_NONE )
        {
            delete poCurve;
            break;
        }

/* -------------------------------------------------------------------- */
/*      Read the delimiter following the surface.                       */
/* -------------------------------------------------------------------- */
        pszInput = OGRWktReadToken( pszInput, szToken );
    } while( szToken[0] == ',' && eErr == OGRERR_NONE );

    CPLFree( paoPoints );
    CPLFree( padfZ );

/* -------------------------------------------------------------------- */
/*      freak if we don't get a closing bracket.                        */
/* -------------------------------------------------------------------- */

    if( eErr != OGRERR_NONE )
        return eErr;

    if( szToken[0] != ')' )
        return OGRERR_CORRUPT_DATA;

    *ppszInput = (char *) pszInput;
    return OGRERR_NONE;
}
//! @endcond

/************************************************************************/
/*                          OGR_GT_Flatten()                            */
/************************************************************************/
/**
 * \brief Returns the 2D geometry type corresponding to the passed geometry
 * type.
 *
 * This function is intended to work with geometry types as old-style 99-402
 * extended dimension (Z) WKB types, as well as with newer SFSQL 1.2 and
 * ISO SQL/MM Part 3 extended dimension (Z&M) WKB types.
 *
 * @param eType Input geometry type
 *
 * @return 2D geometry type corresponding to the passed geometry type.
 *
 * @since GDAL 2.0
 */

OGRwkbGeometryType OGR_GT_Flatten( OGRwkbGeometryType eType )
{
    eType = (OGRwkbGeometryType) (eType & (~wkb25DBitInternalUse));
    if( eType >= 1000 && eType < 2000 )  // ISO Z.
        return (OGRwkbGeometryType) (eType - 1000);
    if( eType >= 2000 && eType < 3000 )  // ISO M.
        return (OGRwkbGeometryType) (eType - 2000);
    if( eType >= 3000 && eType < 4000 )  // ISO ZM.
        return (OGRwkbGeometryType) (eType - 3000);
    return eType;
}

/************************************************************************/
/*                          OGR_GT_HasZ()                               */
/************************************************************************/
/**
 * \brief Return if the geometry type is a 3D geometry type.
 *
 * @param eType Input geometry type
 *
 * @return TRUE if the geometry type is a 3D geometry type.
 *
 * @since GDAL 2.0
 */

int OGR_GT_HasZ( OGRwkbGeometryType eType )
{
    if( eType & wkb25DBitInternalUse )
        return TRUE;
    if( eType >= 1000 && eType < 2000 )  // Accept 1000 for wkbUnknownZ.
        return TRUE;
    if( eType >= 3000 && eType < 4000 )  // Accept 3000 for wkbUnknownZM.
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                          OGR_GT_HasM()                               */
/************************************************************************/
/**
 * \brief Return if the geometry type is a measured type.
 *
 * @param eType Input geometry type
 *
 * @return TRUE if the geometry type is a measured type.
 *
 * @since GDAL 2.1
 */

int OGR_GT_HasM( OGRwkbGeometryType eType )
{
    if( eType >= 2000 && eType < 3000 )  // Accept 2000 for wkbUnknownM.
        return TRUE;
    if( eType >= 3000 && eType < 4000 )  // Accept 3000 for wkbUnknownZM.
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                           OGR_GT_SetZ()                              */
/************************************************************************/
/**
 * \brief Returns the 3D geometry type corresponding to the passed geometry type.
 *
 * @param eType Input geometry type
 *
 * @return 3D geometry type corresponding to the passed geometry type.
 *
 * @since GDAL 2.0
 */

OGRwkbGeometryType OGR_GT_SetZ( OGRwkbGeometryType eType )
{
    if( OGR_GT_HasZ(eType) || eType == wkbNone )
        return eType;
    if( eType <= wkbGeometryCollection )
        return (OGRwkbGeometryType)(eType | wkb25DBitInternalUse);
    else
        return (OGRwkbGeometryType)(eType + 1000);
}

/************************************************************************/
/*                           OGR_GT_SetM()                              */
/************************************************************************/
/**
 * \brief Returns the measured geometry type corresponding to the passed geometry type.
 *
 * @param eType Input geometry type
 *
 * @return measured geometry type corresponding to the passed geometry type.
 *
 * @since GDAL 2.1
 */

OGRwkbGeometryType OGR_GT_SetM( OGRwkbGeometryType eType )
{
    if( OGR_GT_HasM(eType) || eType == wkbNone )
        return eType;
    if( eType & wkb25DBitInternalUse)
    {
        eType = (OGRwkbGeometryType)(eType & ~wkb25DBitInternalUse);
        eType = (OGRwkbGeometryType)(eType + 1000);
    }
    return (OGRwkbGeometryType)(eType + 2000);
}

/************************************************************************/
/*                        OGR_GT_SetModifier()                          */
/************************************************************************/
/**
 * \brief Returns a XY, XYZ, XYM or XYZM geometry type depending on parameter.
 *
 * @param eType Input geometry type
 * @param bHasZ TRUE if the output geometry type must be 3D.
 * @param bHasM TRUE if the output geometry type must be measured.
 *
 * @return Output geometry type.
 *
 * @since GDAL 2.0
 */

OGRwkbGeometryType OGR_GT_SetModifier( OGRwkbGeometryType eType, int bHasZ,
                                       int bHasM )
{
    if( bHasZ && bHasM )
        return OGR_GT_SetM(OGR_GT_SetZ(eType));
    else if( bHasM )
        return OGR_GT_SetM(wkbFlatten(eType));
    else if( bHasZ )
        return OGR_GT_SetZ(wkbFlatten(eType));
    else
        return wkbFlatten(eType);
}

/************************************************************************/
/*                        OGR_GT_IsSubClassOf)                          */
/************************************************************************/
/**
 * \brief Returns if a type is a subclass of another one
 *
 * @param eType Type.
 * @param eSuperType Super type
 *
 * @return TRUE if eType is a subclass of eSuperType.
 *
 * @since GDAL 2.0
 */

int OGR_GT_IsSubClassOf( OGRwkbGeometryType eType,
                         OGRwkbGeometryType eSuperType )
{
    eSuperType = wkbFlatten(eSuperType);
    eType = wkbFlatten(eType);

    if( eSuperType == eType || eSuperType == wkbUnknown )
        return TRUE;

    if( eSuperType == wkbGeometryCollection )
        return eType == wkbMultiPoint || eType == wkbMultiLineString ||
               eType == wkbMultiPolygon || eType == wkbMultiCurve ||
               eType == wkbMultiSurface;

    if( eSuperType == wkbCurvePolygon )
        return eType == wkbPolygon || eType == wkbTriangle;

    if( eSuperType == wkbMultiCurve )
        return eType == wkbMultiLineString;

    if( eSuperType == wkbMultiSurface )
        return eType == wkbMultiPolygon;

    if( eSuperType == wkbCurve )
        return eType == wkbLineString || eType == wkbCircularString ||
               eType == wkbCompoundCurve;

    if( eSuperType == wkbSurface )
        return eType == wkbCurvePolygon || eType == wkbPolygon ||
               eType == wkbTriangle ||
               eType == wkbPolyhedralSurface || eType == wkbTIN;

    if( eSuperType == wkbPolygon )
        return eType == wkbTriangle;

    if (eSuperType == wkbPolyhedralSurface)
        return eType == wkbTIN;

    return FALSE;
}

/************************************************************************/
/*                       OGR_GT_GetCollection()                         */
/************************************************************************/
/**
 * \brief Returns the collection type that can contain the passed geometry type
 *
 * Handled conversions are : wkbNone->wkbNone, wkbPoint -> wkbMultiPoint,
 * wkbLineString->wkbMultiLineString,
 * wkbPolygon/wkbTriangle/wkbPolyhedralSurface/wkbTIN->wkbMultiPolygon,
 * wkbCircularString->wkbMultiCurve, wkbCompoundCurve->wkbMultiCurve,
 * wkbCurvePolygon->wkbMultiSurface.
 * In other cases, wkbUnknown is returned
 *
 * Passed Z, M, ZM flag is preserved.
 *
 *
 * @param eType Input geometry type
 *
 * @return the collection type that can contain the passed geometry type or wkbUnknown
 *
 * @since GDAL 2.0
 */

OGRwkbGeometryType OGR_GT_GetCollection( OGRwkbGeometryType eType )
{
    const bool bHasZ = wkbHasZ(eType);
    const bool bHasM = wkbHasM(eType);
    if( eType == wkbNone )
        return wkbNone;
    OGRwkbGeometryType eFGType = wkbFlatten(eType);
    if( eFGType == wkbPoint )
        eType = wkbMultiPoint;

    else if( eFGType == wkbLineString )
        eType = wkbMultiLineString;

    else if( eFGType == wkbPolygon )
        eType = wkbMultiPolygon;

    else if( eFGType == wkbTriangle )
        eType = wkbTIN;

    else if( OGR_GT_IsCurve(eFGType) )
        eType = wkbMultiCurve;

    else if( OGR_GT_IsSurface(eFGType) )
        eType = wkbMultiSurface;

    else
        return wkbUnknown;

    if( bHasZ )
        eType = wkbSetZ(eType);
    if( bHasM )
        eType = wkbSetM(eType);

    return eType;
}

/************************************************************************/
/*                        OGR_GT_GetCurve()                             */
/************************************************************************/
/**
 * \brief Returns the curve geometry type that can contain the passed geometry type
 *
 * Handled conversions are : wkbPolygon -> wkbCurvePolygon,
 * wkbLineString->wkbCompoundCurve, wkbMultiPolygon->wkbMultiSurface
 * and wkbMultiLineString->wkbMultiCurve.
 * In other cases, the passed geometry is returned.
 *
 * Passed Z, M, ZM flag is preserved.
 *
 * @param eType Input geometry type
 *
 * @return the curve type that can contain the passed geometry type
 *
 * @since GDAL 2.0
 */

OGRwkbGeometryType OGR_GT_GetCurve( OGRwkbGeometryType eType )
{
    const bool bHasZ = wkbHasZ(eType);
    const bool bHasM = wkbHasM(eType);
    OGRwkbGeometryType eFGType = wkbFlatten(eType);

    if( eFGType == wkbLineString )
        eType = wkbCompoundCurve;

    else if( eFGType == wkbPolygon )
        eType = wkbCurvePolygon;

    else if( eFGType == wkbTriangle )
        eType = wkbCurvePolygon;

    else if( eFGType == wkbMultiLineString )
        eType = wkbMultiCurve;

    else if( eFGType == wkbMultiPolygon )
        eType = wkbMultiSurface;

    if( bHasZ )
        eType = wkbSetZ(eType);
    if( bHasM )
        eType = wkbSetM(eType);

    return eType;
}

/************************************************************************/
/*                        OGR_GT_GetLinear()                          */
/************************************************************************/
/**
 * \brief Returns the non-curve geometry type that can contain the passed geometry type
 *
 * Handled conversions are : wkbCurvePolygon -> wkbPolygon,
 * wkbCircularString->wkbLineString, wkbCompoundCurve->wkbLineString,
 * wkbMultiSurface->wkbMultiPolygon and wkbMultiCurve->wkbMultiLineString.
 * In other cases, the passed geometry is returned.
 *
 * Passed Z, M, ZM flag is preserved.
 *
 * @param eType Input geometry type
 *
 * @return the non-curve type that can contain the passed geometry type
 *
 * @since GDAL 2.0
 */

OGRwkbGeometryType OGR_GT_GetLinear( OGRwkbGeometryType eType )
{
    const bool bHasZ = wkbHasZ(eType);
    const bool bHasM = wkbHasM(eType);
    OGRwkbGeometryType eFGType = wkbFlatten(eType);

    if( OGR_GT_IsCurve(eFGType) )
        eType = wkbLineString;

    else if( OGR_GT_IsSurface(eFGType) )
        eType = wkbPolygon;

    else if( eFGType == wkbMultiCurve )
        eType = wkbMultiLineString;

    else if( eFGType == wkbMultiSurface )
        eType = wkbMultiPolygon;

    if( bHasZ )
        eType = wkbSetZ(eType);
    if( bHasM )
        eType = wkbSetM(eType);

    return eType;
}

/************************************************************************/
/*                           OGR_GT_IsCurve()                           */
/************************************************************************/

/**
 * \brief Return if a geometry type is an instance of Curve
 *
 * Such geometry type are wkbLineString, wkbCircularString, wkbCompoundCurve
 * and their Z/M/ZM variant.
 *
 * @param eGeomType the geometry type
 * @return TRUE if the geometry type is an instance of Curve
 *
 * @since GDAL 2.0
 */

int OGR_GT_IsCurve( OGRwkbGeometryType eGeomType )
{
    return OGR_GT_IsSubClassOf( eGeomType, wkbCurve );
}

/************************************************************************/
/*                         OGR_GT_IsSurface()                           */
/************************************************************************/

/**
 * \brief Return if a geometry type is an instance of Surface
 *
 * Such geometry type are wkbCurvePolygon and wkbPolygon
 * and their Z/M/ZM variant.
 *
 * @param eGeomType the geometry type
 * @return TRUE if the geometry type is an instance of Surface
 *
 * @since GDAL 2.0
 */

int OGR_GT_IsSurface( OGRwkbGeometryType eGeomType )
{
    return OGR_GT_IsSubClassOf( eGeomType, wkbSurface );
}

/************************************************************************/
/*                          OGR_GT_IsNonLinear()                        */
/************************************************************************/

/**
 * \brief Return if a geometry type is a non-linear geometry type.
 *
 * Such geometry type are wkbCurve, wkbCircularString, wkbCompoundCurve,
 * wkbSurface, wkbCurvePolygon, wkbMultiCurve, wkbMultiSurface and their
 * Z/M variants.
 *
 * @param eGeomType the geometry type
 * @return TRUE if the geometry type is a non-linear geometry type.
 *
 * @since GDAL 2.0
 */

int OGR_GT_IsNonLinear( OGRwkbGeometryType eGeomType )
{
    OGRwkbGeometryType eFGeomType = wkbFlatten(eGeomType);
    return eFGeomType == wkbCurve || eFGeomType == wkbSurface ||
           eFGeomType == wkbCircularString || eFGeomType == wkbCompoundCurve ||
           eFGeomType == wkbCurvePolygon || eFGeomType == wkbMultiCurve ||
           eFGeomType == wkbMultiSurface;
}

/************************************************************************/
/*                          CastToError()                               */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRGeometry* OGRGeometry::CastToError(OGRGeometry* poGeom)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "%s found. Conversion impossible", poGeom->getGeometryName());
    delete poGeom;
    return NULL;
}
//! @endcond

/************************************************************************/
/*                          OGRexportToSFCGAL()                         */
/************************************************************************/

//! @cond Doxygen_Suppress
sfcgal_geometry_t* OGRGeometry::OGRexportToSFCGAL(UNUSED_IF_NO_SFCGAL OGRGeometry *poGeom)
{
#ifdef HAVE_SFCGAL
    sfcgal_init();
    char *buffer;

    // special cases - LinearRing, Circular String, Compound Curve, Curve Polygon

    if (EQUAL(poGeom->getGeometryName(), "LINEARRING"))
    {
        // cast it to LineString and get the WKT
        OGRLineString *poLineString = OGRCurve::CastToLineString((OGRCurve *)poGeom);
        if (poLineString->exportToWkt(&buffer) == OGRERR_NONE)
        {
            size_t length = 0;
            while(buffer[length++] != '\0');
            length--;
            sfcgal_geometry_t *_geometry = sfcgal_io_read_wkt(buffer,length);
            free(buffer);
            return _geometry;
        }
        else
            return NULL;
    }
    else if (EQUAL(poGeom->getGeometryName(), "CIRCULARSTRING"))
    {
        // cast it to LineString and get the WKT
        OGRLineString *poLineString = OGRCurve::CastToLineString((OGRCurve *)poGeom);
        if (poLineString->exportToWkt(&buffer) == OGRERR_NONE)
        {
            size_t length = 0;
            while(buffer[length++] != '\0');
            length--;
            sfcgal_geometry_t *_geometry = sfcgal_io_read_wkt(buffer,length);
            free(buffer);
            return _geometry;
        }
        else
            return NULL;
    }
    else if (EQUAL(poGeom->getGeometryName(), "COMPOUNDCURVE"))
    {
        // cast it to LineString and get the WKT
        OGRLineString *poLineString = OGRCurve::CastToLineString((OGRCompoundCurve *)poGeom);
        if (poLineString->exportToWkt(&buffer) == OGRERR_NONE)
        {
            size_t length = 0;
            while(buffer[length++] != '\0');
            length--;
            sfcgal_geometry_t *_geometry = sfcgal_io_read_wkt(buffer,length);
            free(buffer);
            return _geometry;
        }
        else
            return NULL;
    }
    else if (EQUAL(poGeom->getGeometryName(), "CURVEPOLYGON"))
    {
        // cast it to Polygon and get the WKT
        OGRPolygon *poPolygon = (OGRPolygon *)OGRGeometryFactory::forceToPolygon((OGRCurvePolygon *)poGeom);
        if (poPolygon->exportToWkt(&buffer) == OGRERR_NONE)
        {
            size_t length = 0;
            while(buffer[length++] != '\0');
            length--;
            sfcgal_geometry_t *_geometry = sfcgal_io_read_wkt(buffer,length);
            free(buffer);
            return _geometry;
        }
        else
            return NULL;
    }
    else if (poGeom->exportToWkt(&buffer) == OGRERR_NONE)
    {
        size_t length = 0;
        while(buffer[length++] != '\0');
        length--;
        sfcgal_geometry_t *_geometry = sfcgal_io_read_wkt(buffer,length);
        free(buffer);
        return _geometry;
    }
    else
        return NULL;
#else
    CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
    return NULL;
#endif
}
//! @endcond

/************************************************************************/
/*                          SFCGALexportToOGR()                         */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRGeometry* OGRGeometry::SFCGALexportToOGR(
    UNUSED_IF_NO_SFCGAL sfcgal_geometry_t* geometry )
{
#ifdef HAVE_SFCGAL

    sfcgal_init();
    char* pabySFCGALWKT = NULL;
    size_t nLength = 0;
    sfcgal_geometry_as_text_decim(geometry, 19, &pabySFCGALWKT, &nLength);
    char* pszWKT = static_cast<char*>(CPLMalloc(nLength + 1));
    memcpy(pszWKT, pabySFCGALWKT, nLength);
    pszWKT[nLength] = 0;
    free(pabySFCGALWKT);
    char *pszTmpWKT = pszWKT;

    sfcgal_geometry_type_t geom_type = sfcgal_geometry_type_id (geometry);

    OGRGeometry *poGeom = NULL;
    if( geom_type == SFCGAL_TYPE_POINT )
    {
        poGeom = new OGRPoint();
    }
    else if( geom_type == SFCGAL_TYPE_LINESTRING )
    {
        poGeom = new OGRLineString();
    }
    else if( geom_type == SFCGAL_TYPE_POLYGON )
    {
        poGeom = new OGRPolygon();
    }
    else if( geom_type == SFCGAL_TYPE_MULTIPOINT )
    {
        poGeom = new OGRMultiPoint();
    }
    else if( geom_type == SFCGAL_TYPE_MULTILINESTRING )
    {
        poGeom = new OGRMultiLineString();
    }
    else if( geom_type == SFCGAL_TYPE_MULTIPOLYGON )
    {
        poGeom = new OGRMultiPolygon();
    }
    else if( geom_type == SFCGAL_TYPE_GEOMETRYCOLLECTION )
    {
        poGeom = new OGRGeometryCollection();
    }
    else if( geom_type == SFCGAL_TYPE_TRIANGLE )
    {
        poGeom = new OGRTriangle();
    }
    else if( geom_type == SFCGAL_TYPE_POLYHEDRALSURFACE )
    {
        poGeom = new OGRPolyhedralSurface();
    }
    else if( geom_type == SFCGAL_TYPE_TRIANGULATEDSURFACE )
    {
        poGeom = new OGRTriangulatedSurface();
    }
    else
    {
        return NULL;
    }

    if( poGeom->importFromWkt(&pszTmpWKT) == OGRERR_NONE )
    {
        CPLFree(pszWKT);
        return poGeom;
    }
    else
    {
        delete poGeom;
        CPLFree(pszWKT);
        return NULL;
    }

#else
    CPLError( CE_Failure, CPLE_NotSupported, "SFCGAL support not enabled." );
    return NULL;
#endif
}
//! @endcond

//! @cond Doxygen_Suppress
OGRBoolean OGRGeometry::IsSFCGALCompatible() const
{
    const OGRwkbGeometryType eGType = wkbFlatten(getGeometryType());
    if( eGType== wkbTriangle || eGType == wkbPolyhedralSurface ||
        eGType == wkbTIN )
    {
        return TRUE;
    }
    if( eGType == wkbGeometryCollection || eGType == wkbMultiSurface )
    {
        const OGRGeometryCollection *poGC =
                            dynamic_cast<const OGRGeometryCollection *>(this);
        bool bIsSFCGALCompatible = false;
        for( int iGeom = 0; poGC != NULL &&
                            iGeom < poGC->getNumGeometries(); iGeom++ )
        {
            OGRwkbGeometryType eSubGeomType =
                wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType());
            if( eSubGeomType == wkbTIN ||
                eSubGeomType == wkbPolyhedralSurface )
            {
                bIsSFCGALCompatible = true;
            }
            else if( eSubGeomType != wkbMultiPolygon )
            {
                bIsSFCGALCompatible = false;
                break;
            }
        }
        return bIsSFCGALCompatible;
    }
    return FALSE;
}
//! @endcond
