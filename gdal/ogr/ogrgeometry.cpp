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
 ****************************************************************************/

#include "ogr_geometry.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "ogr_geos.h"
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
 * Dump geometry in well known text format to indicated output file.
 *
 * This method is the same as the C function OGR_G_DumpReadable().
 *
 * @param fp the text file to write the geometry to.
 * @param pszPrefix the prefix to put on each line of output.
 */

void OGRGeometry::dumpReadable( FILE * fp, const char * pszPrefix ) const

{
    char        *pszWkt = NULL;
    
    if( pszPrefix == NULL )
        pszPrefix = "";

    if( fp == NULL )
        fp = stdout;

    if( exportToWkt( &pszWkt ) == OGRERR_NONE )
    {
        fprintf( fp, "%s%s\n", pszPrefix, pszWkt );
        CPLFree( pszWkt );
    }
}

/************************************************************************/
/*                         OGR_G_DumpReadable()                         */
/************************************************************************/
/**
 * Dump geometry in well known text format to indicated output file.
 *
 * This method is the same as the CPP method OGRGeometry::dumpReadable.
 *
 * @param hGeom handle on the geometry to dump.
 * @param fp the text file to write the geometry to.
 * @param pszPrefix the prefix to put on each line of output.
 */

void OGR_G_DumpReadable( OGRGeometryH hGeom, FILE *fp, const char *pszPrefix )

{
    ((OGRGeometry *) hGeom)->dumpReadable( fp, pszPrefix );
}

/************************************************************************/
/*                       assignSpatialReference()                       */
/************************************************************************/

/**
 * \fn void OGRGeometry::assignSpatialReference( OGRSpatialReference * poSR );
 *
 * Assign spatial reference to this object.  Any existing spatial reference
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
 * Assign spatial reference to this object.  Any existing spatial reference
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
    ((OGRGeometry *) hGeom)->assignSpatialReference( (OGRSpatialReference *)
                                                     hSRS );
}

/************************************************************************/
/*                             Intersects()                             */
/************************************************************************/

/**
 * Do these features intersect?
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
    
    if( hThisGeosGeom != NULL && hOtherGeosGeom != NULL )
    {
        OGRBoolean bResult;
        
        if( GEOSIntersects( hThisGeosGeom, hOtherGeosGeom ) != 0 )
            bResult = TRUE;
        else
            bResult = FALSE;
        
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );

        return bResult;
    }
    else
    {
        return TRUE;
    }
#endif /* HAVE_GEOS */
}

// Old API compatibility function.                                 

OGRBoolean OGRGeometry::Intersect( OGRGeometry *poOtherGeom ) const

{
    return Intersects( poOtherGeom );
}

/************************************************************************/
/*                          OGR_G_Intersect()                           */
/************************************************************************/
/**
 * Do these features intersect?
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
    return ((OGRGeometry *) hGeom)->Intersects( (OGRGeometry *) hOtherGeom );
}

int OGR_G_Intersect( OGRGeometryH hGeom, OGRGeometryH hOtherGeom )

{
    return ((OGRGeometry *) hGeom)->Intersects( (OGRGeometry *) hOtherGeom );
}

/************************************************************************/
/*                            transformTo()                             */
/************************************************************************/

/**
 * Transform geometry to new spatial reference system.
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
 * Transform geometry to new spatial reference system.
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
    return ((OGRGeometry *) hGeom)->transformTo((OGRSpatialReference *) hSRS);
}

/**
 * \fn OGRErr OGRGeometry::transform( OGRCoordinateTransformation *poCT );
 *
 * Apply arbitrary coordinate transformation to geometry.
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
 * Apply arbitrary coordinate transformation to geometry.
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
    return ((OGRGeometry *) hGeom)->transform(
        (OGRCoordinateTransformation *) hTransform );
}

/**
 * \fn int OGRGeometry::getDimension() const;
 *
 * Get the dimension of this object.
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
/*                         OGR_G_GetDimension()                         */
/************************************************************************/
/**
 *
 * Get the dimension of this geometry.
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
    return ((OGRGeometry *) hGeom)->getDimension();
}

/************************************************************************/
/*                       getCoordinateDimension()                       */
/************************************************************************/
/**
 * Get the dimension of the coordinates in this object.
 *
 * This method corresponds to the SFCOM IGeometry::GetDimension() method.
 *
 * This method is the same as the C function OGR_G_GetCoordinateDimension().
 *
 * @return in practice this always returns 2 indicating that coordinates are
 * specified within a two dimensional space.
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
 * Get the dimension of the coordinates in this geometry.
 *
 * This function corresponds to the SFCOM IGeometry::GetDimension() method.
 *
 * This function is the same as the CPP method 
 * OGRGeometry::getCoordinateDimension().
 *
 * @param hGeom handle on the geometry to get the dimension of the 
 * coordinates from.
 * @return in practice this always returns 2 indicating that coordinates are
 * specified within a two dimensional space.
 */

int OGR_G_GetCoordinateDimension( OGRGeometryH hGeom )

{
    return ((OGRGeometry *) hGeom)->getCoordinateDimension();
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

/**
 * Set the coordinate dimension. 
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

void OGR_G_SetCoordinateDimension( OGRGeometryH hGeom, int nNewDimension)

{
    ((OGRGeometry *) hGeom)->setCoordinateDimension( nNewDimension );
}


/**
 * \fn OGRBoolean OGRGeometry::IsEmpty() const;
 *
 * Returns TRUE (non-zero) if the object has no points.  Normally this
 * returns FALSE except between when an object is instantiated and points
 * have been assigned.
 *
 * This method relates to the SFCOM IGeometry::IsEmpty() method.
 *
 * NOTE: This method is hardcoded to return FALSE at this time.
 *
 * @return TRUE if object is empty, otherwise FALSE.
 */

/**
 * \fn OGRBoolean OGRGeometry::IsSimple() const;
 *
 * Returns TRUE if the geometry is simple.
 * 
 * Returns TRUE if the geometry has no anomalous geometric points, such
 * as self intersection or self tangency. The description of each
 * instantiable geometric class will include the specific conditions that
 * cause an instance of that class to be classified as not simple.
 *
 * This method relates to the SFCOM IGeometry::IsSimple() method.
 *
 * NOTE: This method is hardcoded to return TRUE at this time.
 *
 * @return TRUE if object is simple, otherwise FALSE.
 */

/**
 * \fn int OGRGeometry::Equals( OGRGeometry *poOtherGeom ) const;
 *
 * Returns two if two geometries are equivalent.
 *
 * This method is the same as the C function OGR_G_Equal().
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
 * Returns two if two geometries are equivalent.
 *
 * This function is the same as the CPP method OGRGeometry::Equals() method.
 *
 * @param hGeom handle on the first geometry.
 * @param hOther handle on the other geometry to test against.
 * @return TRUE if equivalent or FALSE otherwise.
 */

int OGR_G_Equals( OGRGeometryH hGeom, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hGeom)->Equals( (OGRGeometry *) hOther );
}

int OGR_G_Equal( OGRGeometryH hGeom, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hGeom)->Equals( (OGRGeometry *) hOther );
}


/**
 * \fn int OGRGeometry::WkbSize() const;
 *
 * Returns size of related binary representation.
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
 * Returns size of related binary representation.
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
    return ((OGRGeometry *) hGeom)->WkbSize();
}

/**
 * \fn void OGRGeometry::getEnvelope(OGREnvelope *psEnvelope) const;
 *
 * Computes and returns the bounding envelope for this geometry in the
 * passed psEnvelope structure.
 *
 * This method is the same as the C function OGR_G_GetEnvelope().
 *
 * @param psEnvelope the structure in which to place the results.
 */

/************************************************************************/
/*                         OGR_G_GetEnvelope()                          */
/************************************************************************/
/**
 * Computes and returns the bounding envelope for this geometry in the
 * passed psEnvelope structure.
 *
 * This function is the same as the CPP method OGRGeometry::getEnvelope().
 *
 * @param hGeom handle of the geometry to get envelope from.
 * @param psEnvelope the structure in which to place the results.
 */

void OGR_G_GetEnvelope( OGRGeometryH hGeom, OGREnvelope *psEnvelope )

{
    ((OGRGeometry *) hGeom)->getEnvelope( psEnvelope );
}

/**
 * \fn OGRErr OGRGeometry::importFromWkb( unsigned char * pabyData, int nSize);
 *
 * Assign geometry from well known binary data.
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
 * Assign geometry from well known binary data.
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
    return ((OGRGeometry *) hGeom)->importFromWkb( pabyData, nSize );
}

/**
 * \fn OGRErr OGRGeometry::exportToWkb( OGRwkbByteOrder eByteOrder,
                                        unsigned char * pabyData ) const;
 *
 * Convert a geometry into well known binary format.
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
 * Convert a geometry into well known binary format.
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
    return ((OGRGeometry *) hGeom)->exportToWkb( eOrder, pabyDstBuffer );
}

/**
 * \fn OGRErr OGRGeometry::importFromWkt( char ** ppszInput );
 *
 * Assign geometry from well known text data.
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
 * Assign geometry from well known text data.
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
    return ((OGRGeometry *) hGeom)->importFromWkt( ppszSrcText );
}

/**
 * \fn OGRErr OGRGeometry::exportToWkt( char ** ppszDstText ) const;
 *
 * Convert a geometry into well known text format.
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
 * Convert a geometry into well known text format.
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
    return ((OGRGeometry *) hGeom)->exportToWkt( ppszSrcText );
}

/**
 * \fn OGRwkbGeometryType OGRGeometry::getGeometryType() const;
 *
 * Fetch geometry type.
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
 * Fetch geometry type.
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
    return ((OGRGeometry *) hGeom)->getGeometryType();
}

/**
 * \fn const char * OGRGeometry::getGeometryName() const;
 *
 * Fetch WKT name for geometry type.
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
 * Fetch WKT name for geometry type.
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
    return ((OGRGeometry *) hGeom)->getGeometryName();
}

/**
 * \fn OGRGeometry *OGRGeometry::clone() const;
 *
 * Make a copy of this object.
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
 * Make a copy of this object.
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
    return (OGRGeometryH) ((OGRGeometry *) hGeom)->clone();
}

/**
 * \fn OGRSpatialReference *OGRGeometry::getSpatialReference();
 *
 * Returns spatial reference system for object.
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
 * Returns spatial reference system for geometry.
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
    return (OGRSpatialReferenceH) 
        ((OGRGeometry *) hGeom)->getSpatialReference();
}

/**
 * \fn void OGRGeometry::empty();
 *
 * Clear geometry information.  This restores the geometry to it's initial
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
 * Clear geometry information.  This restores the geometry to it's initial
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
    ((OGRGeometry *) hGeom)->empty();
}

/************************************************************************/
/*                       OGRGeometryTypeToName()                        */
/************************************************************************/

/**
 * Fetch a human readable name corresponding to an OGRwkBGeometryType value.
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
    switch( eType )
    {
      case wkbUnknown:
        return "Unknown (any)";
        
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
          static char szWorkName[33];
          sprintf( szWorkName, "Unrecognised: %d", (int) eType );
          return szWorkName;
      }
    }
}

/**
 * \fn void OGRGeometry::flattenTo2D();
 *
 * Convert geometry to strictly 2D.  In a sense this converts all Z coordinates
 * to 0.0.
 *
 * This method is the same as the C function OGR_G_FlattenTo2D().
 */

/************************************************************************/
/*                         OGR_G_FlattenTo2D()                          */
/************************************************************************/
/**
 * Convert geometry to strictly 2D.  In a sense this converts all Z coordinates
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
 * \fn char *OGRGeometry::exportToGML() const;
 *
 * Convert a geometry into GML format.
 *
 * The GML geometry is expressed directly in terms of GML basic data
 * types assuming the this is available in the gml namespace.  The returned
 * string should be freed with CPLFree() when no longer required.
 *
 * This method is the same as the C function OGR_G_ExportToGML().
 *
 * @return A GML fragment or NULL in case of error.
 */

char *OGRGeometry::exportToGML() const
{
    return OGR_G_ExportToGML( (OGRGeometryH) this );
}

/************************************************************************/
/*                 OGRSetGenerate_DB2_V72_BYTE_ORDER()                  */
/*                                                                      */
/*      This is a special entry point to enable the hack for            */
/*      generating DB2 V7.2 style WKB.  DB2 seems to have placed        */
/*      (and require) an extra 0x30 or'ed with the byte order in        */
/*      WKB.  This entry point is used to turn on or off the            */
/*      generation of such WKB.                                         */
/************************************************************************/

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

    static int bGEOSInitialized = FALSE;

    if( !bGEOSInitialized )
    {
        bGEOSInitialized = TRUE;
        initGEOS( _GEOSWarningHandler, _GEOSErrorHandler );
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
 * Compute distance between two geometries.
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

double OGR_G_Distance( OGRGeometryH hFirst, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hFirst)->Distance( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                             ConvexHull()                             */
/************************************************************************/

/**
 * Compute convex hull.
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

OGRGeometryH OGR_G_ConvexHull( OGRGeometryH hTarget )

{
    return (OGRGeometryH) ((OGRGeometry *) hTarget)->ConvexHull();
}

/************************************************************************/
/*                            getBoundary()                             */
/************************************************************************/

/**
 * Compute boundary.
 *
 * A new geometry object is created and returned containing the boundary
 * of the geometry on which the method is invoked.  
 *
 * This method is the same as the C function OGR_G_GetBoundary().
 *
 * This method is built on the GEOS library, check it for the definition
 * of the geometry operation.
 * If OGR is built without the GEOS library, this method will always fail, 
 * issuing a CPLE_NotSupported error. 
 *
 * @return a newly allocated geometry now owned by the caller, or NULL on failure.
 */

OGRGeometry *OGRGeometry::getBoundary() const

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

/************************************************************************/
/*                         OGR_G_GetBoundary()                          */
/************************************************************************/

OGRGeometryH OGR_G_GetBoundary( OGRGeometryH hTarget )

{
    return (OGRGeometryH) ((OGRGeometry *) hTarget)->getBoundary();
}


/************************************************************************/
/*                               Buffer()                               */
/************************************************************************/

/**
 * Compute buffer of geometry.
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

OGRGeometryH OGR_G_Buffer( OGRGeometryH hTarget, double dfDist, int nQuadSegs )

{
    return (OGRGeometryH) ((OGRGeometry *) hTarget)->Buffer( dfDist, nQuadSegs );
}

/************************************************************************/
/*                            Intersection()                            */
/************************************************************************/

/**
 * Compute intersection.
 *
 * Generates a new geometry which is the region of intersection of the
 * two geometries operated on.  The Intersect() method can be used to test if
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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );

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
/*                         OGR_G_Intersection()                         */
/************************************************************************/

OGRGeometryH OGR_G_Intersection( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->Intersection( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                               Union()                                */
/************************************************************************/

/**
 * Compute union.
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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );

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
/*                            OGR_G_Union()                             */
/************************************************************************/

OGRGeometryH OGR_G_Union( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->Union( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                             Difference()                             */
/************************************************************************/

/**
 * Compute difference.
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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );

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
/*                          OGR_G_Difference()                          */
/************************************************************************/

OGRGeometryH OGR_G_Difference( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->Difference( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                        SymmetricDifference()                         */
/************************************************************************/

/**
 * Compute symmetric difference.
 *
 * Generates a new geometry which is the symmetric difference of this
 * geometry and the second geometry passed into the method.
 *
 * This method is the same as the C function OGR_G_SymmetricDifference().
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
 */

OGRGeometry *
OGRGeometry::SymmetricDifference( const OGRGeometry *poOtherGeom ) const

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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );

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
/*                          OGR_G_Difference()                          */
/************************************************************************/

OGRGeometryH OGR_G_SymmetricDifference( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return (OGRGeometryH) 
        ((OGRGeometry *) hThis)->SymmetricDifference( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                              Disjoint()                              */
/************************************************************************/

/**
 * Test for disjointness.
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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );
    }

    return bResult;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                           OGR_G_Disjoint()                           */
/************************************************************************/

int OGR_G_Disjoint( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hThis)->Disjoint( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                              Touches()                               */
/************************************************************************/

/**
 * Test for touching.
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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );
    }

    return bResult;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                           OGR_G_Touches()                            */
/************************************************************************/

int OGR_G_Touches( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hThis)->Touches( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                              Crosses()                               */
/************************************************************************/

/**
 * Test for crossing.
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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );
    }

    return bResult;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                           OGR_G_Crosses()                            */
/************************************************************************/

int OGR_G_Crosses( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hThis)->Crosses( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                               Within()                               */
/************************************************************************/

/**
 * Test for containment.
 *
 * Tests if the passed in geometry is within the target geometry.
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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );
    }

    return bResult;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                            OGR_G_Within()                            */
/************************************************************************/

int OGR_G_Within( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hThis)->Within( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                              Contains()                              */
/************************************************************************/

/**
 * Test for containment.
 *
 * Tests if the passed in geometry contains the target geometry.
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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );
    }

    return bResult;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                            OGR_G_Contains()                            */
/************************************************************************/

int OGR_G_Contains( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hThis)->Contains( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                              Overlaps()                              */
/************************************************************************/

/**
 * Test for overlap.
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
        GEOSGeom_destroy( hThisGeosGeom );
        GEOSGeom_destroy( hOtherGeosGeom );
    }

    return bResult;

#endif /* HAVE_GEOS */
}

/************************************************************************/
/*                           OGR_G_Overlaps()                           */
/************************************************************************/

int OGR_G_Overlaps( OGRGeometryH hThis, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hThis)->Overlaps( (OGRGeometry *) hOther );
}

/************************************************************************/
/*                             closeRings()                             */
/************************************************************************/

/**
 * Force rings to be closed.
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

void OGR_G_CloseRings( OGRGeometryH hGeom )

{
    ((OGRGeometry *) hGeom)->closeRings();
}

