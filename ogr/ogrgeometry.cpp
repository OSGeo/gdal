/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements a few base methods on OGRGeometry.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.14  2002/10/24 16:46:33  warmerda
 * added docs and C implementation for Equal and FlattenTo2D
 *
 * Revision 1.13  2002/09/26 18:12:38  warmerda
 * added C support
 *
 * Revision 1.12  2002/05/03 14:16:23  warmerda
 * improve 3D geometry names
 *
 * Revision 1.11  2001/11/01 17:20:33  warmerda
 * added DISABLE_OGRGEOM_TRANSFORM macro
 *
 * Revision 1.10  2001/09/21 16:24:20  warmerda
 * added transform() and transformTo() methods
 *
 * Revision 1.9  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.8  2001/02/06 14:14:09  warmerda
 * fixed up documentation
 *
 * Revision 1.7  2000/03/14 21:38:17  warmerda
 * added method to translate geometrytype to name
 *
 * Revision 1.6  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.5  1999/07/06 21:36:47  warmerda
 * tenatively added getEnvelope() and Intersect()
 *
 * Revision 1.4  1999/06/25 20:44:43  warmerda
 * implemented assignSpatialReference, carry properly
 *
 * Revision 1.3  1999/05/31 20:42:28  warmerda
 * added empty method
 *
 * Revision 1.2  1999/05/31 11:05:08  warmerda
 * added some documentation
 *
 * Revision 1.1  1999/05/20 14:35:00  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include <assert.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRGeometry()                             */
/************************************************************************/

OGRGeometry::OGRGeometry()

{
    poSRS = NULL;
}

/************************************************************************/
/*                            ~OGRGeometry()                            */
/************************************************************************/

OGRGeometry::~OGRGeometry()

{
    if( poSRS != NULL )
    {
        poSRS->Dereference();
    }
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

void OGRGeometry::dumpReadable( FILE * fp, const char * pszPrefix )

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
        poSRS->Dereference();

    poSRS = poSR;
    if( poSRS != NULL )
        poSRS->Reference();
}

/************************************************************************/
/*                    OGR_G_AssignSpatialReference()                    */
/************************************************************************/

void OGR_G_AssignSpatialReference( OGRGeometryH hGeom, 
                                   OGRSpatialReferenceH hSRS )

{
    ((OGRGeometry *) hGeom)->assignSpatialReference( (OGRSpatialReference *)
                                                     hSRS );
}

/************************************************************************/
/*                             Intersect()                              */
/************************************************************************/

/**
 * Do these features intersect?
 *
 * Currently this is not implemented in a rigerous fashion, and generally
 * just tests whether the envelopes of the two features intersect.  Eventually
 * this will be made rigerous.
 *
 * This method is the same as the C function OGR_G_Intersect().
 *
 * @param poOtherGeom the other geometry to test against.
 *
 * @return TRUE if the geometries intersect, otherwise FALSE.
 */

OGRBoolean OGRGeometry::Intersect( OGRGeometry *poOtherGeom )

{
    OGREnvelope         oEnv1, oEnv2;

    this->getEnvelope( &oEnv1 );
    poOtherGeom->getEnvelope( &oEnv2 );

    if( oEnv1.MaxX < oEnv2.MinX
        || oEnv1.MaxY < oEnv2.MinY
        || oEnv2.MaxX < oEnv1.MinX
        || oEnv2.MaxY < oEnv1.MinY )
        return FALSE;
    else
        return TRUE;
}

/************************************************************************/
/*                          OGR_G_Intersect()                           */
/************************************************************************/

int OGR_G_Intersect( OGRGeometryH hGeom, OGRGeometryH hOtherGeom )

{
    return ((OGRGeometry *) hGeom)->Intersect( (OGRGeometry *) hOtherGeom );
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

OGRErr OGR_G_Transform( OGRGeometryH hGeom, 
                        OGRCoordinateTransformationH hTransform )

{
    return ((OGRGeometry *) hGeom)->transform(
        (OGRCoordinateTransformation *) hTransform );
}

/**
 * \fn int OGRGeometry::getDimension();
 *
 * Get the dimension of this object.
 *
 * This method corresponds to the SFCOM IGeometry::GetDimension() method.
 * It indicates the dimension of the object, but does not indicate the
 * dimension of the underlying space (as indicated by
 * OGRGeometry::getCoordinateDimension().
 *
 * This method is the same as the C function OGR_G_GetDimension().
 *
 * @return 0 for points, 1 for lines and 2 for surfaces.
 */

/************************************************************************/
/*                         OGR_G_GetDimension()                         */
/************************************************************************/

int OGR_G_GetDimension( OGRGeometryH hGeom )

{
    return ((OGRGeometry *) hGeom)->getDimension();
}

/**
 * \fn int OGRGeometry::getCoordinateDimension();
 *
 * Get the dimension of the coordinates in this object.
 *
 * This method corresponds to the SFCOM IGeometry::GetDimension() method.
 *
 * This method is the same as the C function OGR_G_GetCoordinateDimension().
 *
 * @return in practice this always returns 2 indicating that coordinates are
 * specified within a two dimensional space.
 */

/************************************************************************/
/*                    OGR_G_GetCoordinateDimension()                    */
/************************************************************************/

int OGR_G_GetCoordinateDimension( OGRGeometryH hGeom )

{
    return ((OGRGeometry *) hGeom)->getCoordinateDimension();
}


/**
 * \fn OGRBoolean OGRGeometry::IsEmpty();
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
 * \fn OGRBoolean OGRGeometry::IsSimple();
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
 * \fn int OGRGeometry::Equal( OGRGeometry *poOtherGeom );
 *
 * Returns two if two geometries are equivelent.
 *
 * This method is the same as the C function OGR_G_Equal().
 *
 * @return TRUE if equivelent or FALSE otherwise.
 */

/************************************************************************/
/*                            OGR_G_Equal()                             */
/************************************************************************/

int OGR_G_Equal( OGRGeometryH hGeom, OGRGeometryH hOther )

{
    return ((OGRGeometry *) hGeom)->Equal( (OGRGeometry *) hOther );
}


/**
 * \fn int OGRGeometry::WkbSize();
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

int OGR_G_WkbSize( OGRGeometryH hGeom )

{
    return ((OGRGeometry *) hGeom)->WkbSize();
}

/**
 * \fn void OGRGeometry::getEnvelope(OGREnvelope *psEnvelope);
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

OGRErr OGR_G_ImportFromWkb( OGRGeometryH hGeom, 
                            unsigned char *pabyData, int nSize )

{
    return ((OGRGeometry *) hGeom)->importFromWkb( pabyData, nSize );
}

/**
 * \fn OGRErr OGRGeometry::exportToWkb( OGRwkbByteOrder eOrder,
                                        unsigned char * pabyDstBuffer );
 *
 * Convert a geometry into well known binary format.
 *
 * This method relates to the SFCOM IWks::ExportToWKB() method.
 *
 * This method is the same as the C function OGR_G_ExportToWkb().
 *
 * @param eOrder One of wkbXDR or wkbNDR indicating MSB or LSB byte order
 *               respectively.
 * @param pabyDstBuffer a buffer into which the binary representation is
 *                      written.  This buffer must be at least
 *                      OGRGeometry::WkbSize() byte in size.
 *
 * @return Currently OGRERR_NONE is always returned.
 */

/************************************************************************/
/*                         OGR_G_ExportToWkb()                          */
/************************************************************************/

OGRErr OGR_G_ExportToWkb( OGRGeometryH hGeom, OGRwkbByteOrder eOrder,
                          unsigned char *pabyDstBuffer )

{
    return ((OGRGeometry *) hGeom)->exportToWkb( eOrder, pabyDstBuffer );
}

/**
 * \fn OGRErr OGRGeometry::importFromWkt( char ** ppszSrcText );
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
 * @param ppszSrcText pointer to a pointer to the source text.  The pointer is
 *                    updated to pointer after the consumed text.
 *
 * @return OGRERR_NONE if all goes well, otherwise any of
 * OGRERR_NOT_ENOUGH_DATA, OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or
 * OGRERR_CORRUPT_DATA may be returned.
 */

/************************************************************************/
/*                        OGR_G_ImportFromWkt()                         */
/************************************************************************/

OGRErr OGR_G_ImportFromWkt( OGRGeometryH hGeom, char ** ppszSrcText )

{
    return ((OGRGeometry *) hGeom)->importFromWkt( ppszSrcText );
}

/**
 * \fn OGRErr OGRGeometry::exportToWkt( char ** ppszDstText );
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

OGRErr OGR_G_ExportToWkt( OGRGeometryH hGeom, char **ppszSrcText )

{
    return ((OGRGeometry *) hGeom)->exportToWkt( ppszSrcText );
}

/**
 * \fn OGRwkbGeometryType OGRGeometry::getGeometryType();
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

OGRwkbGeometryType OGR_G_GetGeometryType( OGRGeometryH hGeom )

{
    return ((OGRGeometry *) hGeom)->getGeometryType();
}

/**
 * \fn const char * OGRGeometry::getGeometryName();
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

const char *OGR_G_GetGeometryName( OGRGeometryH hGeom )

{
    return ((OGRGeometry *) hGeom)->getGeometryName();
}

/**
 * \fn OGRGeometry *OGRGeometry::clone();
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

void OGR_G_Empty( OGRGeometryH hGeom )

{
    ((OGRGeometry *) hGeom)->empty();
}

/************************************************************************/
/*                       OGRGeometryTypeToName()                        */
/************************************************************************/

/**
 * \fn const char *OGRGeometryTypeToName(OGRwkbGeometryType)
 *
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
        static char szWorkName[33];
        sprintf( szWorkName, "Unrecognised: %d", (int) eType );
        return szWorkName;
    }
}

/**
 * \fn void OGRGeometry::flattenTo2D();
 *
 * Covnert geometry to strictly 2D.  In a sense this converts all Z coordinates
 * to 0.0.
 *
 * This method is the same as the C function OGR_G_FlattenTo2D().
 */

/************************************************************************/
/*                         OGR_G_FlattenTo2D()                          */
/************************************************************************/

void OGR_G_FlattenTo2D( OGRGeometryH hGeom )

{
    ((OGRGeometry *) hGeom)->flattenTo2D();
}

