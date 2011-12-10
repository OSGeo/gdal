/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements a few base methods on OGRGeometry.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
#include "ogr_api.h"
#include "ogr_p.h"
#include "ogr_geos.h"
#include "cpl_multiproc.h"
#include <assert.h>

CPL_CVSID("$Id$");

int OGRGeometry::bGenerate_DB2_V72_BYTE_ORDER = FALSE;

#ifdef HAVE_GEOS
static void _GEOSErrorHandler(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    CPLErrorV( CE_Failure, CPLE_AppDefined, fmt, args );
    va_end(args);
}

static void _GEOSWarningHandler(const char *fmt, ...)
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
    nCoordDimension = 2;
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

void OGRGeometry::dumpReadable( FILE * fp, const char * pszPrefix, char** papszOptions ) const

{
    char        *pszWkt = NULL;
    
    if( pszPrefix == NULL )
        pszPrefix = "";

    if( fp == NULL )
        fp = stdout;

    const char* pszDisplayGeometry =
                CSLFetchNameValue(papszOptions, "DISPLAY_GEOMETRY");
    if (pszDisplayGeometry != NULL && EQUAL(pszDisplayGeometry, "SUMMARY"))
    {
        OGRLineString *poLine;
        OGRPolygon *poPoly;
        OGRLinearRing *poRing;
        OGRGeometryCollection *poColl;
        fprintf( fp, "%s%s : ", pszPrefix, getGeometryName() );
        switch( getGeometryType() )
        {
            case wkbUnknown:
            case wkbNone:
            case wkbPoint:
            case wkbPoint25D:
                fprintf( fp, "\n");
                break;
            case wkbLineString:
            case wkbLineString25D:
                poLine = (OGRLineString*)this;
                fprintf( fp, "%d points\n", poLine->getNumPoints() );
                break;
            case wkbPolygon:
            case wkbPolygon25D:
            {
                int ir;
                int nRings;
                poPoly = (OGRPolygon*)this;
                poRing = poPoly->getExteriorRing();
                nRings = poPoly->getNumInteriorRings();
                if (poRing == NULL)
                    fprintf( fp, "empty");
                else
                {
                    fprintf( fp, "%d points", poRing->getNumPoints() );
                    if (nRings)
                    {
                        fprintf( fp, ", %d inner rings (", nRings);
                        for( ir = 0; ir < nRings; ir++)
                        {
                            if (ir)
                                fprintf( fp, ", ");
                            fprintf( fp, "%d points",
                                    poPoly->getInteriorRing(ir)->getNumPoints() );
                        }
                        fprintf( fp, ")");
                    }
                }
                fprintf( fp, "\n");
                break;
            }
            case wkbMultiPoint:
            case wkbMultiPoint25D:
            case wkbMultiLineString:
            case wkbMultiLineString25D:
            case wkbMultiPolygon:
            case wkbMultiPolygon25D:
            case wkbGeometryCollection:
            case wkbGeometryCollection25D:
            {
                int ig;
                poColl = (OGRGeometryCollection*)this;
                fprintf( fp, "%d geometries:\n", poColl->getNumGeometries() );
                for ( ig = 0; ig < poColl->getNumGeometries(); ig++)
                {
                    OGRGeometry * poChild = (OGRGeometry*)poColl->getGeometryRef(ig);
                    fprintf( fp, "%s", pszPrefix);
                    poChild->dumpReadable( fp, pszPrefix, papszOptions );
                }
                break;
            }
            case wkbLinearRing:
                break;
        }
    }
    else if (pszDisplayGeometry == NULL || CSLTestBoolean(pszDisplayGeometry) ||
             EQUAL(pszDisplayGeometry, "WKT"))
    {
        if( exportToWkt( &pszWkt ) == OGRERR_NONE )
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

    ((OGRGeometry *) hGeom)->dumpReadable( fp, pszPrefix );
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
 * @param hSRS handle on the  new spatial reference system to apply.
 */

void OGR_G_AssignSpatialReference( OGRGeometryH hGeom, 
                                   OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_AssignSpatialReference" );

    ((OGRGeometry *) hGeom)->assignSpatialReference( (OGRSpatialReference *)
                                                     hSRS );
}

/************************************************************************/
/*                             Intersects()                             */
/************************************************************************/

/**
 * \brief Do these features intersect?
 *
 * Determines whether two geometries intersect.  If GEOS is enabled, then
 * this is done in rigerous fashion otherwise TRUE is returned if the
 * envelopes (bounding boxes) of the two features overlap. 
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

OGRBoolean OGRGeometry::Intersects( OGRGeometry *poOtherGeom ) const

{
    OGREnvelope         oEnv1, oEnv2;

    if( this == NULL || poOtherGeom == NULL )
        return TRUE;

    this->getEnvelope( &oEnv1 );
    poOtherGeom->getEnvelope( &oEnv2 );

    if( oEnv1.MaxX < oEnv2.MinX
        || oEnv1.MaxY < oEnv2.MinY
        || oEnv2.MaxX < oEnv1.MinX
        || oEnv2.MaxY < oEnv1.MinY )
        return FALSE;

#ifndef HAVE_GEOS

    // Without GEOS we assume that envelope overlap is equivelent to
    // actual intersection.
    return TRUE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();
    
    OGRBoolean bResult = FALSE;
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        if( GEOSIntersects( hThisGeosGeom, hOtherGeosGeom ) != 0 )
            bResult = TRUE;
        else
            bResult = FALSE;
    }

    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return bResult;
#endif /* HAVE_GEOS */
}

// Old API compatibility function.                                 

OGRBoolean OGRGeometry::Intersect( OGRGeometry *poOtherGeom ) const

{
    return Intersects( poOtherGeom );
}

/************************************************************************/
/*                          OGR_G_Intersects()                          */
/************************************************************************/
/**
 * \brief Do these features intersect?
 *
 * Currently this is not implemented in a rigerous fashion, and generally
 * just tests whether the envelopes of the two features intersect.  Eventually
 * this will be made rigerous.
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

    return ((OGRGeometry *) hGeom)->Intersects( (OGRGeometry *) hOtherGeom );
}

int OGR_G_Intersect( OGRGeometryH hGeom, OGRGeometryH hOtherGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Intersect", FALSE );
    VALIDATE_POINTER1( hOtherGeom, "OGR_G_Intersect", FALSE );

    return ((OGRGeometry *) hGeom)->Intersects( (OGRGeometry *) hOtherGeom );
}

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
#ifdef DISABLE_OGRGEOM_TRANSFORM
    return OGRERR_FAILURE;
#else
    OGRCoordinateTransformation *poCT;
    OGRErr eErr;

    if( getSpatialReference() == NULL || poSR == NULL )
        return OGRERR_FAILURE;

    poCT = OGRCreateCoordinateTransformation( getSpatialReference(), poSR );
    if( poCT == NULL )
        return OGRERR_FAILURE;

    eErr = transform( poCT );

    delete poCT;

    return eErr;
#endif
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

    return ((OGRGeometry *) hGeom)->transformTo((OGRSpatialReference *) hSRS);
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

    return ((OGRGeometry *) hGeom)->transform(
        (OGRCoordinateTransformation *) hTransform );
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


/************************************************************************/
/*                  OGRGeometry::segmentize()                           */
/************************************************************************/
/**
 *
 * \brief Modify the geometry such it has no segment longer then the given distance.
 *
 * Interpolated points will have Z and M values (if needed) set to 0.
 * Distance computation is performed in 2d only
 *
 * This function is the same as the C function OGR_G_Segmentize()
 *
 * @param dfMaxLength the maximum distance between 2 points after segmentization
 */

void OGRGeometry::segmentize( double dfMaxLength )
{
    /* Do nothing */
}

/************************************************************************/
/*                         OGR_G_Segmentize()                           */
/************************************************************************/

/**
 *
 * \brief Modify the geometry such it has no segment longer then the given distance.
 *
 * Interpolated points will have Z and M values (if needed) set to 0.
 * Distance computation is performed in 2d only
 *
 * This function is the same as the CPP method OGRGeometry::segmentize().
 *
 * @param hGeom handle on the geometry to segmentize
 * @param dfMaxLength the maximum distance between 2 points after segmentization
 */

void   CPL_DLL OGR_G_Segmentize(OGRGeometryH hGeom, double dfMaxLength )
{
    VALIDATE_POINTER0( hGeom, "OGR_G_Segmentize" );

    if (dfMaxLength <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "dfMaxLength must be strictly positive");
        return;
    }
    ((OGRGeometry *) hGeom)->segmentize( dfMaxLength );
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

    return ((OGRGeometry *) hGeom)->getDimension();
}

/************************************************************************/
/*                       getCoordinateDimension()                       */
/************************************************************************/
/**
 * \brief Get the dimension of the coordinates in this object.
 *
 * This method corresponds to the SFCOM IGeometry::GetDimension() method.
 *
 * This method is the same as the C function OGR_G_GetCoordinateDimension().
 *
 * @return in practice this will return 2 or 3. It can also return 0 in the
 * case of an empty point.
 */

int OGRGeometry::getCoordinateDimension() const

{
    return nCoordDimension;
}

/************************************************************************/
/*                    OGR_G_GetCoordinateDimension()                    */
/************************************************************************/
/**
 *
 * \brief Get the dimension of the coordinates in this geometry.
 *
 * This function corresponds to the SFCOM IGeometry::GetDimension() method.
 *
 * This function is the same as the CPP method 
 * OGRGeometry::getCoordinateDimension().
 *
 * @param hGeom handle on the geometry to get the dimension of the 
 * coordinates from.
 *
 * @return in practice this will return 2 or 3. It can also return 0 in the
 * case of an empty point.
 */

int OGR_G_GetCoordinateDimension( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetCoordinateDimension", 0 );

    return ((OGRGeometry *) hGeom)->getCoordinateDimension();
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

/**
 * \brief Set the coordinate dimension. 
 *
 * This method sets the explicit coordinate dimension.  Setting the coordinate
 * dimension of a geometry to 2 should zero out any existing Z values.  Setting
 * the dimension of a geometry collection will not necessarily affect the
 * children geometries. 
 *
 * @param nNewDimension New coordinate dimension value, either 2 or 3.
 */

void OGRGeometry::setCoordinateDimension( int nNewDimension )

{
    nCoordDimension = nNewDimension;
}

/************************************************************************/
/*                    OGR_G_SetCoordinateDimension()                    */
/************************************************************************/

/**
 * \brief Set the coordinate dimension.
 *
 * This method sets the explicit coordinate dimension.  Setting the coordinate
 * dimension of a geometry to 2 should zero out any existing Z values.  Setting
 * the dimension of a geometry collection will not necessarily affect the
 * children geometries.
 *
 * @param hGeom handle on the geometry to set the dimension of the
 * coordinates.
 * @param nNewDimension New coordinate dimension value, either 2 or 3.
 */

void OGR_G_SetCoordinateDimension( OGRGeometryH hGeom, int nNewDimension)

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetCoordinateDimension" );

    ((OGRGeometry *) hGeom)->setCoordinateDimension( nNewDimension );
}

/**
 * \fn int OGRGeometry::Equals( OGRGeometry *poOtherGeom ) const;
 *
 * \brief Returns TRUE if two geometries are equivalent.
 *
 * This method is the same as the C function OGR_G_Equals().
 *
 * @return TRUE if equivalent or FALSE otherwise.
 */


// Backward compatibility method.

int OGRGeometry::Equal( OGRGeometry *poOtherGeom ) const
{
    return Equals( poOtherGeom );
}

/************************************************************************/
/*                            OGR_G_Equals()                            */
/************************************************************************/

/**
 * \brief Returns TRUE if two geometries are equivalent.
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

    if (hGeom == NULL) {
        CPLError ( CE_Failure, CPLE_ObjectNull, "hGeom was NULL in OGR_G_Equals");
        return 0;
    }

    if (hOther == NULL) {
        CPLError ( CE_Failure, CPLE_ObjectNull, "hOther was NULL in OGR_G_Equals");
        return 0;
    }
    
    return ((OGRGeometry *) hGeom)->Equals( (OGRGeometry *) hOther );
}

int OGR_G_Equal( OGRGeometryH hGeom, OGRGeometryH hOther )

{
    if (hGeom == NULL) {
        CPLError ( CE_Failure, CPLE_ObjectNull, "hGeom was NULL in OGR_G_Equal");
        return 0;
    }

    if (hOther == NULL) {
        CPLError ( CE_Failure, CPLE_ObjectNull, "hOther was NULL in OGR_G_Equal");
        return 0;
    }

    return ((OGRGeometry *) hGeom)->Equals( (OGRGeometry *) hOther );
}


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

    return ((OGRGeometry *) hGeom)->WkbSize();
}

/**
 * \fn void OGRGeometry::getEnvelope(OGREnvelope *psEnvelope) const;
 *
 * \brief Computes and returns the bounding envelope for this geometry in the passed psEnvelope structure.
 *
 * This method is the same as the C function OGR_G_GetEnvelope().
 *
 * @param psEnvelope the structure in which to place the results.
 */

/************************************************************************/
/*                         OGR_G_GetEnvelope()                          */
/************************************************************************/
/**
 * \brief Computes and returns the bounding envelope for this geometry in the passed psEnvelope structure.
 *
 * This function is the same as the CPP method OGRGeometry::getEnvelope().
 *
 * @param hGeom handle of the geometry to get envelope from.
 * @param psEnvelope the structure in which to place the results.
 */

void OGR_G_GetEnvelope( OGRGeometryH hGeom, OGREnvelope *psEnvelope )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_GetEnvelope" );

    ((OGRGeometry *) hGeom)->getEnvelope( psEnvelope );
}

/**
 * \fn void OGRGeometry::getEnvelope(OGREnvelope3D *psEnvelope) const;
 *
 * \brief Computes and returns the bounding envelope (3D) for this geometry in the passed psEnvelope structure.
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
 * \brief Computes and returns the bounding envelope (3D) for this geometry in the passed psEnvelope structure.
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

    ((OGRGeometry *) hGeom)->getEnvelope( psEnvelope );
}

/**
 * \fn OGRErr OGRGeometry::importFromWkb( unsigned char * pabyData, int nSize);
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
 * @param nSize the size of pabyData in bytes, or zero if not known.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
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
 * @param nSize the size of pabyData in bytes, or zero if not known.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

OGRErr OGR_G_ImportFromWkb( OGRGeometryH hGeom, 
                            unsigned char *pabyData, int nSize )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_ImportFromWkb", OGRERR_FAILURE );

    return ((OGRGeometry *) hGeom)->importFromWkb( pabyData, nSize );
}

/**
 * \fn OGRErr OGRGeometry::exportToWkb( OGRwkbByteOrder eByteOrder,
                                        unsigned char * pabyData ) const;
 *
 * \brief Convert a geometry into well known binary format.
 *
 * This method relates to the SFCOM IWks::ExportToWKB() method.
 *
 * This method is the same as the C function OGR_G_ExportToWkb().
 *
 * @param eByteOrder One of wkbXDR or wkbNDR indicating MSB or LSB byte order
 *               respectively.
 * @param pabyData a buffer into which the binary representation is
 *                      written.  This buffer must be at least
 *                      OGRGeometry::WkbSize() byte in size.
 *
 * @return Currently OGRERR_NONE is always returned.
 */

/************************************************************************/
/*                         OGR_G_ExportToWkb()                          */
/************************************************************************/
/**
 * \brief Convert a geometry into well known binary format.
 *
 * This function relates to the SFCOM IWks::ExportToWKB() method.
 *
 * This function is the same as the CPP method OGRGeometry::exportToWkb().
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

    return ((OGRGeometry *) hGeom)->exportToWkb( eOrder, pabyDstBuffer );
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
 * @param hGeom handle on the  geometry to assign well know text data to.
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

    return ((OGRGeometry *) hGeom)->importFromWkt( ppszSrcText );
}

/**
 * \fn OGRErr OGRGeometry::exportToWkt( char ** ppszDstText ) const;
 *
 * \brief Convert a geometry into well known text format.
 *
 * This method relates to the SFCOM IWks::ExportToWKT() method.
 *
 * This method is the same as the C function OGR_G_ExportToWkt().
 *
 * @param ppszDstText a text buffer is allocated by the program, and assigned
 *                    to the passed pointer.
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
 * This function is the same as the CPP method OGRGeometry::exportToWkt().
 *
 * @param hGeom handle on the geometry to convert to a text format from.
 * @param ppszSrcText a text buffer is allocated by the program, and assigned
                       to the passed pointer.
 *
 * @return Currently OGRERR_NONE is always returned.
 */

OGRErr OGR_G_ExportToWkt( OGRGeometryH hGeom, char **ppszSrcText )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_ExportToWkt", OGRERR_FAILURE );

    return ((OGRGeometry *) hGeom)->exportToWkt( ppszSrcText );
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

    return ((OGRGeometry *) hGeom)->getGeometryType();
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

    return ((OGRGeometry *) hGeom)->getGeometryName();
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
 * @return an handle on the  copy of the geometry with the spatial
 * reference system as the original.
 */

OGRGeometryH OGR_G_Clone( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Clone", NULL );

    return (OGRGeometryH) ((OGRGeometry *) hGeom)->clone();
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

    return (OGRSpatialReferenceH) 
        ((OGRGeometry *) hGeom)->getSpatialReference();
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

    ((OGRGeometry *) hGeom)->empty();
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

    return ((OGRGeometry *) hGeom)->IsEmpty();
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
OGRGeometry::IsValid(  ) const

{
#ifndef HAVE_GEOS

    return FALSE;

#else

    OGRBoolean bResult = FALSE;
    GEOSGeom hThisGeosGeom = NULL;
    
    hThisGeosGeom = exportToGEOS();

    if( hThisGeosGeom != NULL  )
    {
        bResult = GEOSisValid( hThisGeosGeom );
        GEOSGeom_destroy( hThisGeosGeom );
    }

    return bResult;

#endif /* HAVE_GEOS */
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

    return ((OGRGeometry *) hGeom)->IsValid();
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
OGRGeometry::IsSimple(  ) const

{
#ifndef HAVE_GEOS

    return FALSE;

#else

    OGRBoolean bResult = FALSE;
    GEOSGeom hThisGeosGeom = NULL;
    
    hThisGeosGeom = exportToGEOS();

    if( hThisGeosGeom != NULL  )
    {
        bResult = GEOSisSimple( hThisGeosGeom );
        GEOSGeom_destroy( hThisGeosGeom );
    }

    return bResult;

#endif /* HAVE_GEOS */
}


/**
 * \brief Returns TRUE if the geometry is simple.
 * 
 * Returns TRUE if the geometry has no anomalous geometric points, such
 * as self intersection or self tangency. The description of each
 * instantiable geometric class will include the specific conditions that
 * cause an instance of that class to be classified as not simple.
 *
 * This function is the same as the c++ method OGRGeometry::IsSimple() method.
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

    return ((OGRGeometry *) hGeom)->IsSimple();
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
OGRGeometry::IsRing(  ) const

{
#ifndef HAVE_GEOS

    return FALSE;

#else

    OGRBoolean bResult = FALSE;
    GEOSGeom hThisGeosGeom = NULL;
    
    hThisGeosGeom = exportToGEOS();

    if( hThisGeosGeom != NULL  )
    {
        bResult = GEOSisRing( hThisGeosGeom );
        GEOSGeom_destroy( hThisGeosGeom );
    }

    return bResult;

#endif /* HAVE_GEOS */
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

    return ((OGRGeometry *) hGeom)->IsRing();
}

/************************************************************************/
/*                     OGRFromOGCGeomType()                             */
/*      Map OGCgeometry format type to corresponding                    */
/*      OGR constants.                                                  */
/************************************************************************/

OGRwkbGeometryType OGRFromOGCGeomType( const char *pszGeomType )
{
    if ( EQUAL(pszGeomType, "POINT") )
        return wkbPoint;
    else if ( EQUAL(pszGeomType, "LINESTRING") )
        return wkbLineString;
    else if ( EQUAL(pszGeomType, "POLYGON") )
        return wkbPolygon;
    else if ( EQUAL(pszGeomType, "MULTIPOINT") )
        return wkbMultiPoint;
    else if ( EQUAL(pszGeomType, "MULTILINESTRING") )
        return wkbMultiLineString;
    else if ( EQUAL(pszGeomType, "MULTIPOLYGON") )
        return wkbMultiPolygon;
    else if ( EQUAL(pszGeomType, "GEOMETRYCOLLECTION") )
        return wkbGeometryCollection;
    else
        return wkbUnknown;
}

/************************************************************************/
/*                     OGRToOGCGeomType()                               */
/*      Map OGR geometry format constants to corresponding              */
/*      OGC geometry type                                               */
/************************************************************************/

const char * OGRToOGCGeomType( OGRwkbGeometryType eGeomType )
{
    switch ( wkbFlatten(eGeomType) )
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
        default:
            return "";
    }
}

/************************************************************************/
/*                       OGRGeometryTypeToName()                        */
/************************************************************************/

/**
 * \brief Fetch a human readable name corresponding to an OGRwkBGeometryType value.
 * The returned value should not be modified, or freed by the application.
 *
 * This function is C callable.
 *
 * @param eType the geometry type.
 *
 * @return internal human readable string, or NULL on failure.
 */

const char *OGRGeometryTypeToName( OGRwkbGeometryType eType )

{
    switch( (int)eType )
    {
      case wkbUnknown:
        return "Unknown (any)";

      case (wkbUnknown | wkb25DBit):
        return "3D Unknown (any)";

      case wkbPoint:
        return "Point";

      case wkbPoint25D:
        return "3D Point";

      case wkbLineString:
        return "Line String";

      case wkbLineString25D:
        return "3D Line String";

      case wkbPolygon:
        return "Polygon";

      case wkbPolygon25D:
        return "3D Polygon";

      case wkbMultiPoint:
        return "Multi Point";

      case wkbMultiPoint25D:
        return "3D Multi Point";

      case wkbMultiLineString:
        return "Multi Line String";

      case wkbMultiLineString25D:
        return "3D Multi Line String";

      case wkbMultiPolygon:
        return "Multi Polygon";

      case wkbMultiPolygon25D:
        return "3D Multi Polygon";

      case wkbGeometryCollection:
        return "Geometry Collection";

      case wkbGeometryCollection25D:
        return "3D Geometry Collection";

      case wkbNone:
        return "None";

      default:
      {
          // OGRThreadSafety: This static is judged to be a very low risk 
          // for thread safety because it is only used in case of error, 
          // and the worst that can happen is reporting the wrong code
          // in the generated message.
          static char szWorkName[33];
          sprintf( szWorkName, "Unrecognised: %d", (int) eType );
          return szWorkName;
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
 * encounted will establish the preliminary type.
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
    int n25DFlag = 0;
    OGRwkbGeometryType eFMain = wkbFlatten(eMain);
    OGRwkbGeometryType eFExtra = wkbFlatten(eExtra);
        
    if( eFMain != eMain || eFExtra != eExtra )
        n25DFlag = wkb25DBit;

    if( eFMain == wkbUnknown || eFExtra == wkbUnknown )
        return (OGRwkbGeometryType) (((int) wkbUnknown) | n25DFlag);

    if( eFMain == wkbNone )
        return eExtra;

    if( eFExtra == wkbNone )
        return eMain;

    if( eFMain == eFExtra )
        return (OGRwkbGeometryType) (((int) eFMain) | n25DFlag);

    // Both are geometry collections.
    if( (eFMain == wkbGeometryCollection
         || eFMain == wkbMultiPoint
         || eFMain == wkbMultiLineString
         || eFMain == wkbMultiPolygon)
        && (eFExtra == wkbGeometryCollection
            || eFExtra == wkbMultiPoint
            || eFExtra == wkbMultiLineString
            || eFMain == wkbMultiPolygon) )
    {
        return (OGRwkbGeometryType) (((int) wkbGeometryCollection) | n25DFlag);
    }

    // Nothing apparently in common.
    return (OGRwkbGeometryType) (((int) wkbUnknown) | n25DFlag);
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
    ((OGRGeometry *) hGeom)->flattenTo2D();
}

/************************************************************************/
/*                            exportToGML()                             */
/************************************************************************/

/**
 * \fn char *OGRGeometry::exportToGML( const char* const * papszOptions = NULL ) const;
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
 * <li> GML3_LINESTRING_ELEMENT=curve. (Only valid for FORMAT=GML3) To use gml:Curve element for linestrings.
 *     Otherwise gml:LineString will be used .
 * <li> GML3_LONGSRS=YES/NO. (Only valid for FORMAT=GML3) Default to YES. If YES, SRS with EPSG authority will
 *      be written with the "urn:ogc:def:crs:EPSG::" prefix.
 *      In the case, if the SRS is a geographic SRS without explicit AXIS order, but that the same SRS authority code
 *      imported with ImportFromEPSGA() should be treated as lat/long, then the function will take care of coordinate order swapping.
 *      If set to NO, SRS with EPSG authority will be written with the "EPSG:" prefix, even if they are in lat/long order.
 * </ul>
 *
 * This method is the same as the C function OGR_G_ExportToGMLEx().
 *
 * @param papszOptions NULL-terminated list of options.
 * @return A GML fragment or NULL in case of error.
 */

char *OGRGeometry::exportToGML( const char* const * papszOptions ) const
{
    return OGR_G_ExportToGMLEx( (OGRGeometryH) this, (char**)papszOptions );
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
#ifndef _WIN32_WCE
#ifdef OGR_ENABLED
    return OGR_G_ExportToKML( (OGRGeometryH) this, NULL );
#else
    CPLError( CE_Failure, CPLE_AppDefined,
              "OGRGeometry::exportToKML() not supported in builds without OGR drivers." );
    return NULL;
#endif
#else
    CPLError( CE_Failure, CPLE_AppDefined,
              "OGRGeometry::exportToKML() not supported in the WinCE build." );
    return NULL;
#endif
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
#ifndef _WIN32_WCE
#ifdef OGR_ENABLED
    OGRGeometry* poGeometry = const_cast<OGRGeometry*>(this);
    return OGR_G_ExportToJson( (OGRGeometryH) (poGeometry) );
#else
    CPLError( CE_Failure, CPLE_AppDefined,
              "OGRGeometry::exportToJson() not supported in builds without OGR drivers." );
    return NULL;
#endif
#else
    CPLError( CE_Failure, CPLE_AppDefined,
              "OGRGeometry::exportToJson() not supported in the WinCE build." );
    return NULL;
#endif
}

/************************************************************************/
/*                 OGRSetGenerate_DB2_V72_BYTE_ORDER()                  */
/************************************************************************/

/**
  * \brief Special entry point to enable the hack for generating DB2 V7.2 style WKB.
  *
  * DB2 seems to have placed  (and require) an extra 0x30 or'ed with the byte order in
  * WKB.  This entry point is used to turn on or off the
  * generation of such WKB.
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
/*                            exportToGEOS()                            */
/************************************************************************/

GEOSGeom OGRGeometry::exportToGEOS() const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return NULL;

#else

    static void *hGEOSInitMutex = NULL;
    static int bGEOSInitialized = FALSE;

    CPLMutexHolderD( &hGEOSInitMutex );

    if( !bGEOSInitialized )
    {
        bGEOSInitialized = TRUE;
        initGEOS( _GEOSWarningHandler, _GEOSErrorHandler );
    }

    /* POINT EMPTY is exported to WKB as if it were POINT(0 0) */
    /* so that particular case is necessary */
    if (wkbFlatten(getGeometryType()) == wkbPoint &&
        nCoordDimension == 0)
    {
        return GEOSGeomFromWKT("POINT EMPTY");
    }

    GEOSGeom hGeom = NULL;
    size_t nDataSize;
    unsigned char *pabyData = NULL;

    nDataSize = WkbSize();
    pabyData = (unsigned char *) CPLMalloc(nDataSize);
    if( exportToWkb( wkbNDR, pabyData ) == OGRERR_NONE )
        hGeom = GEOSGeomFromWKB_buf( pabyData, nDataSize );

    CPLFree( pabyData );

    return hGeom;

#endif /* HAVE_GEOS */
}


/************************************************************************/
/*                              Distance()                              */
/************************************************************************/

/**
 * \brief Compute distance between two geometries.
 *
 * Returns the shortest distance between the two geometries. 
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
        CPLDebug( "OGR", "OGRGeometry::Distance called with NULL geometry pointer" );
        return -1.0;
    }

#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return -1.0;

#else

    // GEOSGeom is a pointer
    GEOSGeom hThis = NULL;
    GEOSGeom hOther = NULL;

    hOther = poOtherGeom->exportToGEOS();
    hThis = exportToGEOS();
   
    int bIsErr = 0;
    double dfDistance = 0.0;

    if( hThis != NULL && hOther != NULL )
    {
        bIsErr = GEOSDistance( hThis, hOther, &dfDistance );
    }

    GEOSGeom_destroy( hThis );
    GEOSGeom_destroy( hOther );

    if ( bIsErr > 0 ) 
    {
        return dfDistance;
    }

    /* Calculations error */
    return -1.0;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                           OGR_G_Distance()                           */
/************************************************************************/
/**
 * \brief Compute distance between two geometries.
 *
 * Returns the shortest distance between the two geometries. 
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

    return ((OGRGeometry *) hFirst)->Distance( (OGRGeometry *) hOther );
}

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
 * @return a newly allocated geometry now owned by the caller, or NULL on failure.
 */

OGRGeometry *OGRGeometry::ConvexHull() const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return NULL;

#else

    GEOSGeom hGeosGeom = NULL;
    GEOSGeom hGeosHull = NULL;
    OGRGeometry *poHullOGRGeom = NULL;

    hGeosGeom = exportToGEOS();
    if( hGeosGeom != NULL )
    {
        hGeosHull = GEOSConvexHull( hGeosGeom );
        GEOSGeom_destroy( hGeosGeom );

        if( hGeosHull != NULL )
        {
            poHullOGRGeom = OGRGeometryFactory::createFromGEOS(hGeosHull);
            GEOSGeom_destroy( hGeosHull);
        }
    }

    return poHullOGRGeom;

#endif /* HAVE_GEOS */
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

    return (OGRGeometryH) ((OGRGeometry *) hTarget)->ConvexHull();
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
 * @return a newly allocated geometry now owned by the caller, or NULL on failure.
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
    GEOSGeom hGeosProduct = NULL;
    OGRGeometry *poOGRProduct = NULL;

    hGeosGeom = exportToGEOS();
    if( hGeosGeom != NULL )
    {
        hGeosProduct = GEOSBoundary( hGeosGeom );
        GEOSGeom_destroy( hGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct = OGRGeometryFactory::createFromGEOS(hGeosProduct);
            GEOSGeom_destroy( hGeosProduct );
        }
    }

    return poOGRProduct;

#endif /* HAVE_GEOS */
}


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

    return (OGRGeometryH) ((OGRGeometry *) hTarget)->Boundary();
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

    return (OGRGeometryH) ((OGRGeometry *) hTarget)->Boundary();
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
 * approximate polygons.  The nQuadSegs parameter can be used to control how many
 * segements should be used to define a 90 degree curve - a quadrant of a circle. 
 * A value of 30 is a reasonable default.  Large values result in large numbers
 * of vertices in the resulting buffer geometry while small numbers reduce the 
 * accuracy of the result. 
 *
 * This method is the same as the C function OGR_G_Buffer().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail, 
 * issuing a CPLE_NotSupported error. 
 *
 * @param dfDist the buffer distance to be applied. 
 *
 * @param nQuadSegs the number of segments used to approximate a 90 degree (quadrant) of
 * curvature. 
 *
 * @return the newly created geometry, or NULL if an error occurs. 
 */

OGRGeometry *OGRGeometry::Buffer( double dfDist, int nQuadSegs ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return NULL;

#else

    GEOSGeom hGeosGeom = NULL;
    GEOSGeom hGeosProduct = NULL;
    OGRGeometry *poOGRProduct = NULL;

    hGeosGeom = exportToGEOS();
    if( hGeosGeom != NULL )
    {
        hGeosProduct = GEOSBuffer( hGeosGeom, dfDist, nQuadSegs );
        GEOSGeom_destroy( hGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct = OGRGeometryFactory::createFromGEOS(hGeosProduct);
            GEOSGeom_destroy( hGeosProduct );
        }
    }

    return poOGRProduct;

#endif /* HAVE_GEOS */
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
 * approximate polygons.  The nQuadSegs parameter can be used to control how many
 * segements should be used to define a 90 degree curve - a quadrant of a circle. 
 * A value of 30 is a reasonable default.  Large values result in large numbers
 * of vertices in the resulting buffer geometry while small numbers reduce the 
 * accuracy of the result. 
 *
 * This function is the same as the C++ method OGRGeometry::Buffer().
 *
 * This function is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this function will always fail, 
 * issuing a CPLE_NotSupported error. 
 *
 * @param hTarget the geometry.
 * @param dfDist the buffer distance to be applied. 
 *
 * @param nQuadSegs the number of segments used to approximate a 90 degree
 * (quadrant) of curvature. 
 *
 * @return the newly created geometry, or NULL if an error occurs. 
 */

OGRGeometryH OGR_G_Buffer( OGRGeometryH hTarget, double dfDist, int nQuadSegs )

{
    VALIDATE_POINTER1( hTarget, "OGR_G_Buffer", NULL );

    return (OGRGeometryH) ((OGRGeometry *) hTarget)->Buffer( dfDist, nQuadSegs );
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

OGRGeometry *OGRGeometry::Intersection( const OGRGeometry *poOtherGeom ) const

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

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        hGeosProduct = GEOSIntersection( hThisGeosGeom, hOtherGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct = OGRGeometryFactory::createFromGEOS(hGeosProduct);
            GEOSGeom_destroy( hGeosProduct );
        }
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return poOGRProduct;

#endif /* HAVE_GEOS */
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

    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->Intersection( (OGRGeometry *) hOther );
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

OGRGeometry *OGRGeometry::Union( const OGRGeometry *poOtherGeom ) const

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

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        hGeosProduct = GEOSUnion( hThisGeosGeom, hOtherGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct = OGRGeometryFactory::createFromGEOS(hGeosProduct);
            GEOSGeom_destroy( hGeosProduct );
        }
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return poOGRProduct;

#endif /* HAVE_GEOS */
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

    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->Union( (OGRGeometry *) hOther );
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

/* GEOS >= 3.1.0 */
#elif GEOS_VERSION_MAJOR > 3 || (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 1)

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hGeosProduct = NULL;
    OGRGeometry *poOGRProduct = NULL;

    hThisGeosGeom = exportToGEOS();
    if( hThisGeosGeom != NULL )
    {
        hGeosProduct = GEOSUnionCascaded( hThisGeosGeom );
        GEOSGeom_destroy( hThisGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct = OGRGeometryFactory::createFromGEOS(hGeosProduct);
            GEOSGeom_destroy( hGeosProduct );
        }
    }

    return poOGRProduct;

#else
    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS >= 3.1.0 required for UnionCascaded() support." );
    return NULL;
#endif /* HAVE_GEOS */
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

    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->UnionCascaded();
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

OGRGeometry *OGRGeometry::Difference( const OGRGeometry *poOtherGeom ) const

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

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        hGeosProduct = GEOSDifference( hThisGeosGeom, hOtherGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct = OGRGeometryFactory::createFromGEOS(hGeosProduct);
            GEOSGeom_destroy( hGeosProduct );
        }
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return poOGRProduct;

#endif /* HAVE_GEOS */
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

    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->Difference( (OGRGeometry *) hOther );
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
OGRGeometry::SymDifference( const OGRGeometry *poOtherGeom ) const

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

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        hGeosProduct = GEOSSymDifference( hThisGeosGeom, hOtherGeosGeom );

        if( hGeosProduct != NULL )
        {
            poOGRProduct = OGRGeometryFactory::createFromGEOS(hGeosProduct);
            GEOSGeom_destroy( hGeosProduct );
        }
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return poOGRProduct;

#endif /* HAVE_GEOS */
}


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

/************************************************************************/
/*                      OGR_G_SymDifference()                           */
/************************************************************************/

/**
 * \brief Compute symmetric difference.
 *
 * Generates a new geometry which is the symmetric difference of this
 * geometry and the other geometry.
 *
 * This function is the same as the C++ method OGRGeometry::SymmetricDifference().
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

    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->SymDifference( (OGRGeometry *) hOther );
}

/**
 * \brief Compute symmetric difference (deprecated)
 *
 * @deprecated
 *
 * @see OGR_G_SymmetricDifference()
 */
OGRGeometryH OGR_G_SymmetricDifference( OGRGeometryH hThis, OGRGeometryH hOther )

{
    VALIDATE_POINTER1( hThis, "OGR_G_SymmetricDifference", NULL );

    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->SymDifference( (OGRGeometry *) hOther );
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
OGRGeometry::Disjoint( const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSDisjoint( hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return bResult;

#endif /* HAVE_GEOS */
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

    return ((OGRGeometry *) hThis)->Disjoint( (OGRGeometry *) hOther );
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
OGRGeometry::Touches( const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();

    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSTouches( hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return bResult;

#endif /* HAVE_GEOS */
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

    return ((OGRGeometry *) hThis)->Touches( (OGRGeometry *) hOther );
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
OGRGeometry::Crosses( const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();

    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSCrosses( hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return bResult;

#endif /* HAVE_GEOS */
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

    return ((OGRGeometry *) hThis)->Crosses( (OGRGeometry *) hOther );
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
OGRGeometry::Within( const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSWithin( hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return bResult;

#endif /* HAVE_GEOS */
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

    return ((OGRGeometry *) hThis)->Within( (OGRGeometry *) hOther );
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
OGRGeometry::Contains( const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSContains( hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return bResult;

#endif /* HAVE_GEOS */
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

    return ((OGRGeometry *) hThis)->Contains( (OGRGeometry *) hOther );
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
OGRGeometry::Overlaps( const OGRGeometry *poOtherGeom ) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return FALSE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    OGRBoolean bResult = FALSE;

    hThisGeosGeom = exportToGEOS();
    hOtherGeosGeom = poOtherGeom->exportToGEOS();
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        bResult = GEOSOverlaps( hThisGeosGeom, hOtherGeosGeom );
    }
    GEOSGeom_destroy( hThisGeosGeom );
    GEOSGeom_destroy( hOtherGeosGeom );

    return bResult;

#endif /* HAVE_GEOS */
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

    return ((OGRGeometry *) hThis)->Overlaps( (OGRGeometry *) hOther );
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

void OGRGeometry::closeRings()

{
}

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

    ((OGRGeometry *) hGeom)->closeRings();
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
 * SQL/MM-Part 3 defines the operation for surfaces and multisurfaces (multipolygons).
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
 * @since OGR 1.8.0 as a OGRGeometry method (previously was restricted to OGRPolygon)
 */

int OGRGeometry::Centroid( OGRPoint *poPoint ) const

{
    if( poPoint == NULL )
        return OGRERR_FAILURE;

#ifndef HAVE_GEOS
    // notdef ... not implemented yet.
    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return OGRERR_FAILURE;

#else

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hOtherGeosGeom = NULL;
    
    hThisGeosGeom = exportToGEOS();

    if( hThisGeosGeom != NULL )
    {
    	hOtherGeosGeom = GEOSGetCentroid( hThisGeosGeom );
        GEOSGeom_destroy( hThisGeosGeom );

        if( hOtherGeosGeom == NULL )
            return OGRERR_FAILURE;

        OGRGeometry *poCentroidGeom =
            OGRGeometryFactory::createFromGEOS( hOtherGeosGeom );

        GEOSGeom_destroy( hOtherGeosGeom );

        if (poCentroidGeom == NULL)
            return OGRERR_FAILURE;
        if (wkbFlatten(poCentroidGeom->getGeometryType()) != wkbPoint)
        {
            delete poCentroidGeom;
            return OGRERR_FAILURE;
        }

        OGRPoint *poCentroid = (OGRPoint *) poCentroidGeom;
	poPoint->setX( poCentroid->getX() );
	poPoint->setY( poCentroid->getY() );

        delete poCentroidGeom;

    	return OGRERR_NONE;
    }
    else
    {
    	return OGRERR_FAILURE;
    }

#endif /* HAVE_GEOS */
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

    OGRGeometry *poGeom = ((OGRGeometry *) hGeom);
    OGRPoint *poCentroid = ((OGRPoint *) hCentroidPoint);
    
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

OGRGeometry *OGRGeometry::Simplify(double dTolerance) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported, 
              "GEOS support not enabled." );
    return NULL;

/* GEOS >= 3.0.0 */
#elif GEOS_CAPI_VERSION_MAJOR >= 2 || (GEOS_CAPI_VERSION_MAJOR == 1 && GEOS_CAPI_VERSION_MINOR >= 4)

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hGeosProduct = NULL;
    OGRGeometry *poOGRProduct = NULL;

    hThisGeosGeom = exportToGEOS();
    if( hThisGeosGeom != NULL ) 
    {
        hGeosProduct = GEOSSimplify( hThisGeosGeom, dTolerance );
        GEOSGeom_destroy( hThisGeosGeom );
        if( hGeosProduct != NULL )
        {
            poOGRProduct = OGRGeometryFactory::createFromGEOS( hGeosProduct );
            GEOSGeom_destroy( hGeosProduct );
        }
    }
    return poOGRProduct;

#else
    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS >= 3.0.0 required for Simplify() support." );
    return NULL;
#endif /* HAVE_GEOS */

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
    return (OGRGeometryH) ((OGRGeometry *) hThis)->Simplify( dTolerance );
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

OGRGeometry *OGRGeometry::SimplifyPreserveTopology(double dTolerance) const

{
#ifndef HAVE_GEOS

    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS support not enabled." );
    return NULL;

/* GEOS >= 3.0.0 */
#elif GEOS_CAPI_VERSION_MAJOR >= 2 || (GEOS_CAPI_VERSION_MAJOR == 1 && GEOS_CAPI_VERSION_MINOR >= 4)

    GEOSGeom hThisGeosGeom = NULL;
    GEOSGeom hGeosProduct = NULL;
    OGRGeometry *poOGRProduct = NULL;

    hThisGeosGeom = exportToGEOS();
    if( hThisGeosGeom != NULL )
    {
        hGeosProduct = GEOSTopologyPreserveSimplify( hThisGeosGeom, dTolerance );
        GEOSGeom_destroy( hThisGeosGeom );
        if( hGeosProduct != NULL )
        {
            poOGRProduct = OGRGeometryFactory::createFromGEOS( hGeosProduct );
            GEOSGeom_destroy( hGeosProduct );
        }
    }
    return poOGRProduct;

#else
    CPLError( CE_Failure, CPLE_NotSupported,
              "GEOS >= 3.0.0 required for SimplifyPreserveTopology() support." );
    return NULL;
#endif /* HAVE_GEOS */

}

/************************************************************************/
/*                     OGR_G_SimplifyPreserveTopology()                 */
/************************************************************************/

/**
 * \brief Simplify the geometry while preserving topology.
 *
 * This function is the same as the C++ method OGRGeometry::SimplifyPreserveTopology().
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

OGRGeometryH OGR_G_SimplifyPreserveTopology( OGRGeometryH hThis, double dTolerance )

{
    VALIDATE_POINTER1( hThis, "OGR_G_SimplifyPreserveTopology", NULL );
    return (OGRGeometryH) ((OGRGeometry *) hThis)->SimplifyPreserveTopology( dTolerance );
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
