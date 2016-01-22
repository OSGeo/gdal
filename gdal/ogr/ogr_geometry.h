/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes for manipulating simple features that is not specific
 *           to a particular interface technology.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGR_GEOMETRY_H_INCLUDED
#define _OGR_GEOMETRY_H_INCLUDED

#include "ogr_core.h"
#include "ogr_spatialref.h"

/**
 * \file ogr_geometry.h
 *
 * Simple feature geometry classes.
 */

/**
 * Simple container for a position.
 */
class OGRRawPoint
{
  public:
          OGRRawPoint()
          {
                  x = y = 0.0;
          }
    double      x;
    double      y;
};

typedef struct GEOSGeom_t *GEOSGeom;
typedef struct GEOSContextHandle_HS *GEOSContextHandle_t;

/************************************************************************/
/*                             OGRGeometry                              */
/************************************************************************/

class OGRPoint;

/**
 * Abstract base class for all geometry classes.
 *
 * Some spatial analysis methods require that OGR is built on the GEOS library
 * to work properly. The precise meaning of methods that describe spatial relationships
 * between geometries is described in the SFCOM, or other simple features interface
 * specifications, like "OpenGIS® Implementation Specification for
 * Geographic information - Simple feature access - Part 1: Common architecture"
 * (<a href="http://www.opengeospatial.org/standards/sfa">OGC 06-103r3</a>)
 *
 */
 
class CPL_DLL OGRGeometry
{
  private:
    OGRSpatialReference * poSRS;                // may be NULL

  protected:
    int                   nCoordDimension;
    int getIsoGeometryType() const;
    
  public:
                OGRGeometry();
    virtual     ~OGRGeometry();
                        
    // standard IGeometry
    virtual int getDimension() const = 0;
    virtual int getCoordinateDimension() const;
    virtual OGRBoolean  IsEmpty() const = 0; 
    virtual OGRBoolean  IsValid() const;
    virtual OGRBoolean  IsSimple() const;
    virtual OGRBoolean  IsRing() const;
    virtual void        empty() = 0;
    virtual OGRGeometry *clone() const = 0;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const = 0;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const = 0;

    // IWks Interface
    virtual int WkbSize() const = 0;
    virtual OGRErr importFromWkb( unsigned char *, int=-1 )=0;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOgc ) const = 0;
    virtual OGRErr importFromWkt( char ** ppszInput ) = 0;
    virtual OGRErr exportToWkt( char ** ppszDstText ) const = 0;
    
    // non-standard
    virtual OGRwkbGeometryType getGeometryType() const = 0;
    virtual const char *getGeometryName() const = 0;
    virtual void   dumpReadable( FILE *, const char * = NULL, char** papszOptions = NULL ) const;
    virtual void   flattenTo2D() = 0;
    virtual char * exportToGML( const char* const * papszOptions = NULL ) const;
	virtual char * exportToKML() const;
    virtual char * exportToJson() const;

    static GEOSContextHandle_t createGEOSContext();
    static void freeGEOSContext(GEOSContextHandle_t hGEOSCtxt);
    virtual GEOSGeom exportToGEOS(GEOSContextHandle_t hGEOSCtxt) const;

    virtual void closeRings();

    virtual void setCoordinateDimension( int nDimension ); 

    void    assignSpatialReference( OGRSpatialReference * poSR );
    OGRSpatialReference *getSpatialReference( void ) const { return poSRS; }

    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) = 0;
    OGRErr  transformTo( OGRSpatialReference *poSR );
    
    virtual void segmentize(double dfMaxLength);

    // ISpatialRelation
    virtual OGRBoolean  Intersects( OGRGeometry * ) const;
    virtual OGRBoolean  Equals( OGRGeometry * ) const = 0;
    virtual OGRBoolean  Disjoint( const OGRGeometry * ) const;
    virtual OGRBoolean  Touches( const OGRGeometry * ) const;
    virtual OGRBoolean  Crosses( const OGRGeometry * ) const;
    virtual OGRBoolean  Within( const OGRGeometry * ) const;
    virtual OGRBoolean  Contains( const OGRGeometry * ) const;
    virtual OGRBoolean  Overlaps( const OGRGeometry * ) const;
//    virtual OGRBoolean  Relate( const OGRGeometry *, const char * ) const;

    virtual OGRGeometry *Boundary() const;
    virtual double  Distance( const OGRGeometry * ) const;
    virtual OGRGeometry *ConvexHull() const;
    virtual OGRGeometry *Buffer( double dfDist, int nQuadSegs = 30 ) const;
    virtual OGRGeometry *Intersection( const OGRGeometry *) const;
    virtual OGRGeometry *Union( const OGRGeometry * ) const;
    virtual OGRGeometry *UnionCascaded() const;
    virtual OGRGeometry *Difference( const OGRGeometry * ) const;
    virtual OGRGeometry *SymDifference( const OGRGeometry * ) const;
    virtual OGRErr       Centroid( OGRPoint * poPoint ) const;
    virtual OGRGeometry *Simplify(double dTolerance) const;
    OGRGeometry *SimplifyPreserveTopology(double dTolerance) const;

    virtual OGRGeometry *Polygonize() const;

    // backward compatibility to non-standard method names. 
    OGRBoolean  Intersect( OGRGeometry * ) const CPL_WARN_DEPRECATED("Non standard method. Use Intersects() instead");
    OGRBoolean  Equal( OGRGeometry * ) const CPL_WARN_DEPRECATED("Non standard method. Use Equals() instead");
    virtual OGRGeometry *SymmetricDifference( const OGRGeometry * ) const CPL_WARN_DEPRECATED("Non standard method. Use SymDifference() instead");
    virtual OGRGeometry *getBoundary() const CPL_WARN_DEPRECATED("Non standard method. Use Boundary() instead");
    
    // Special HACK for DB2 7.2 support
    static int bGenerate_DB2_V72_BYTE_ORDER;

    virtual void        swapXY();
};

/************************************************************************/
/*                               OGRPoint                               */
/************************************************************************/

/**
 * Point class.
 *
 * Implements SFCOM IPoint methods.
 */

class CPL_DLL OGRPoint : public OGRGeometry
{
    double      x;
    double      y;
    double      z;

  public:
                OGRPoint();
                OGRPoint( double x, double y );
                OGRPoint( double x, double y, double z );
    virtual     ~OGRPoint();

    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int=-1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText ) const;
    
    // IGeometry
    virtual int getDimension() const;
    virtual OGRGeometry *clone() const;
    virtual void empty();
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const;
    virtual OGRBoolean  IsEmpty() const;

    // IPoint
    double      getX() const { return x; } 
    double      getY() const { return y; }
    double      getZ() const { return z; }

    // Non standard
    virtual void setCoordinateDimension( int nDimension ); 
    void        setX( double xIn ) { x = xIn; if (nCoordDimension == 0) nCoordDimension = 2; }
    void        setY( double yIn ) { y = yIn; if (nCoordDimension == 0) nCoordDimension = 2; }
    void        setZ( double zIn ) { z = zIn; nCoordDimension=3; }

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const;
    
    // Non standard from OGRGeometry
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();

    virtual void        swapXY();
};

/************************************************************************/
/*                               OGRCurve                               */
/************************************************************************/

/**
 * Abstract curve base class.
 */

class CPL_DLL OGRCurve : public OGRGeometry
{
  public:
            OGRCurve();
    virtual ~OGRCurve();
    // ICurve methods
    virtual double get_Length() const = 0;
    virtual void StartPoint(OGRPoint *) const = 0;
    virtual void EndPoint(OGRPoint *) const = 0;
    virtual int  get_IsClosed() const;
    virtual void Value( double, OGRPoint * ) const = 0;

};

/************************************************************************/
/*                            OGRLineString                             */
/************************************************************************/

/**
 * Concrete representation of a multi-vertex line.
 */

class CPL_DLL OGRLineString : public OGRCurve
{
  protected:
    int         nPointCount;
    OGRRawPoint *paoPoints;
    double      *padfZ;

    void        Make3D();
    void        Make2D();

  public:
                OGRLineString();
    virtual     ~OGRLineString();

    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText ) const;

    // IGeometry interface
    virtual int getDimension() const;
    virtual OGRGeometry *clone() const;
    virtual void empty();
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const;
    virtual OGRBoolean  IsEmpty() const;

    // ICurve methods
    virtual double get_Length() const;
    virtual void StartPoint(OGRPoint *) const;
    virtual void EndPoint(OGRPoint *) const;
    virtual void Value( double, OGRPoint * ) const;
    virtual double Project(const OGRPoint *) const;
    virtual OGRLineString* getSubLine(double, double, int) const;
    
    // ILineString methods
    int         getNumPoints() const { return nPointCount; }
    void        getPoint( int, OGRPoint * ) const;
    double      getX( int i ) const { return paoPoints[i].x; }
    double      getY( int i ) const { return paoPoints[i].y; }
    double      getZ( int i ) const;

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const;
    
    // non standard.
    virtual void setCoordinateDimension( int nDimension ); 
    void        setNumPoints( int nNewPointCount, int bZeroizeNewContent = TRUE );
    void        setPoint( int, OGRPoint * );
    void        setPoint( int, double, double );
    void        setZ( int, double );
    void        setPoint( int, double, double, double );
    void        setPoints( int, OGRRawPoint *, double * = NULL );
    void        setPoints( int, double * padfX, double * padfY,
                           double *padfZ = NULL );
    void        addPoint( OGRPoint * );
    void        addPoint( double, double );
    void        addPoint( double, double, double );

    void        getPoints( OGRRawPoint *, double * = NULL ) const;
    void        getPoints( void* pabyX, int nXStride,
                           void* pabyY, int nYStride,
                           void* pabyZ = NULL, int nZStride = 0 ) const;

    void        addSubLineString( const OGRLineString *, 
                                  int nStartVertex = 0, int nEndVertex = -1 );
    void        reversePoints( void );

    // non-standard from OGRGeometry
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual const char *getGeometryName() const;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();
    virtual void segmentize(double dfMaxLength);

    virtual void        swapXY();
};

/************************************************************************/
/*                            OGRLinearRing                             */
/************************************************************************/

/**
 * Concrete representation of a closed ring.
 *
 * This class is functionally equivelent to an OGRLineString, but has a
 * separate identity to maintain alignment with the OpenGIS simple feature
 * data model.  It exists to serve as a component of an OGRPolygon.  
 *
 * The OGRLinearRing has no corresponding free standing well known binary
 * representation, so importFromWkb() and exportToWkb() will not actually
 * work.  There is a non-standard GDAL WKT representation though.
 *
 * Because OGRLinearRing is not a "proper" free standing simple features 
 * object, it cannot be directly used on a feature via SetGeometry(), and
 * cannot genearally be used with GEOS for operations like Intersects(). 
 * Instead the polygon should be used, or the OGRLinearRing should be
 * converted to an OGRLineString for such operations. 
 */

class CPL_DLL OGRLinearRing : public OGRLineString
{
  private:
    friend class OGRPolygon; 
    
    // These are not IWks compatible ... just a convenience for OGRPolygon.
    virtual int _WkbSize( int b3D ) const;
    virtual OGRErr _importFromWkb( OGRwkbByteOrder, int b3D,
                                   unsigned char *, int=-1 );
    virtual OGRErr _exportToWkb( OGRwkbByteOrder, int b3D, 
                                 unsigned char * ) const;
    
  public:
                        OGRLinearRing();
                        OGRLinearRing( OGRLinearRing * );
                        ~OGRLinearRing();

    // Non standard.
    virtual const char *getGeometryName() const;
    virtual OGRGeometry *clone() const;
    virtual int isClockwise() const;
    virtual void reverseWindingOrder();
    virtual void closeRings();
    virtual double get_Area() const;
    OGRBoolean isPointInRing(const OGRPoint* pt, int bTestEnvelope = TRUE) const;
    OGRBoolean isPointOnRingBoundary(const OGRPoint* pt, int bTestEnvelope = TRUE) const;
    
    // IWks Interface - Note this isnt really a first class object
    // for the purposes of WKB form.  These methods always fail since this
    // object cant be serialized on its own. 
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int=-1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOgc ) const;
};

/************************************************************************/
/*                              OGRSurface                              */
/************************************************************************/

/**
 * Abstract base class for 2 dimensional objects like polygons.
 */

class CPL_DLL OGRSurface : public OGRGeometry
{
  public:
    virtual double      get_Area() const = 0;
    virtual OGRErr      PointOnSurface( OGRPoint * poPoint ) const = 0;
};

/************************************************************************/
/*                              OGRPolygon                              */
/************************************************************************/

/**
 * Concrete class representing polygons.
 *
 * Note that the OpenGIS simple features polygons consist of one outer
 * ring, and zero or more inner rings.  A polygon cannot represent disconnected
 * regions (such as multiple islands in a political body).  The
 * OGRMultiPolygon must be used for this.
 */

class CPL_DLL OGRPolygon : public OGRSurface
{
    int         nRingCount;
    OGRLinearRing **papoRings;
    
  public:
                OGRPolygon();
    virtual     ~OGRPolygon();

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRGeometry *clone() const;
    virtual void empty();
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();
    virtual OGRBoolean  IsEmpty() const;
    virtual void segmentize(double dfMaxLength);

    // ISurface Interface
    virtual double      get_Area() const;
    virtual int         PointOnSurface( OGRPoint * poPoint ) const;
    
    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText ) const;

    // IGeometry
    virtual int getDimension() const;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const;

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const;
    
    // Non standard
    virtual void setCoordinateDimension( int nDimension ); 

    void        addRing( OGRLinearRing * );
    void        addRingDirectly( OGRLinearRing * );

    OGRLinearRing *getExteriorRing();
    const OGRLinearRing *getExteriorRing() const;
    int         getNumInteriorRings() const;
    OGRLinearRing *getInteriorRing( int );
    const OGRLinearRing *getInteriorRing( int ) const;

    OGRLinearRing *stealExteriorRing();
    OGRLinearRing *stealInteriorRing(int);

    OGRBoolean IsPointOnSurface( const OGRPoint * ) const;

    virtual void closeRings();

    virtual void        swapXY();
};

/************************************************************************/
/*                        OGRGeometryCollection                         */
/************************************************************************/

/**
 * A collection of 1 or more geometry objects.
 *
 * All geometries must share a common spatial reference system, and
 * Subclasses may impose additional restrictions on the contents.
 */

class CPL_DLL OGRGeometryCollection : public OGRGeometry
{
    int         nGeomCount;
    OGRGeometry **papoGeoms;

    OGRErr      importFromWkbInternal( unsigned char * pabyData, int nSize, int nRecLevel );
    OGRErr      importFromWktInternal( char **ppszInput, int nRecLevel );

  public:
                OGRGeometryCollection();
    virtual     ~OGRGeometryCollection();

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRGeometry *clone() const;
    virtual void empty();
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();
    virtual OGRBoolean  IsEmpty() const;
    virtual void segmentize(double dfMaxLength);

    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText ) const;

    virtual double get_Length() const;
    virtual double get_Area() const;

    // IGeometry methods
    virtual int getDimension() const;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const;

    // IGeometryCollection
    int         getNumGeometries() const;
    OGRGeometry *getGeometryRef( int );
    const OGRGeometry *getGeometryRef( int ) const;

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const;
    
    // Non standard
    virtual void setCoordinateDimension( int nDimension ); 
    virtual OGRErr addGeometry( const OGRGeometry * );
    virtual OGRErr addGeometryDirectly( OGRGeometry * );
    virtual OGRErr removeGeometry( int iIndex, int bDelete = TRUE );

    void closeRings();

    virtual void        swapXY();
};

/************************************************************************/
/*                           OGRMultiPolygon                            */
/************************************************************************/

/**
 * A collection of non-overlapping OGRPolygons.
 *
 * Note that the IMultiSurface class hasn't been modelled, nor have any
 * of it's methods. 
 */

class CPL_DLL OGRMultiPolygon : public OGRGeometryCollection
{
  public:
            OGRMultiPolygon();
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRGeometry *clone() const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ) const;

    // IGeometry methods
    virtual int getDimension() const;

    // Non standard
    virtual OGRErr addGeometryDirectly( OGRGeometry * );

    virtual double  get_Area() const;
};

/************************************************************************/
/*                            OGRMultiPoint                             */
/************************************************************************/

/**
 * A collection of OGRPoints.
 */

class CPL_DLL OGRMultiPoint : public OGRGeometryCollection
{
  private:
    OGRErr  importFromWkt_Bracketed( char **, int bHasM, int bHasZ );

  public:
            OGRMultiPoint();
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRGeometry *clone() const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ) const;

    // IGeometry methods
    virtual int getDimension() const;

    // Non standard
    virtual OGRErr addGeometryDirectly( OGRGeometry * );
};

/************************************************************************/
/*                          OGRMultiLineString                          */
/************************************************************************/

/**
 * A collection of OGRLineStrings.
 */

class CPL_DLL OGRMultiLineString : public OGRGeometryCollection
{
  public:
            OGRMultiLineString();
            ~OGRMultiLineString();
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRGeometry *clone() const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ) const;

    // IGeometry methods
    virtual int getDimension() const;
    
    // Non standard
    virtual OGRErr addGeometryDirectly( OGRGeometry * );
};


/************************************************************************/
/*                          OGRGeometryFactory                          */
/************************************************************************/

/**
 * Create geometry objects from well known text/binary.
 */

class CPL_DLL OGRGeometryFactory
{
    static OGRErr createFromFgfInternal( unsigned char *pabyData,
                                         OGRSpatialReference * poSR,
                                         OGRGeometry **ppoReturn,
                                         int nBytes,
                                         int *pnBytesConsumed,
                                         int nRecLevel );
  public:
    static OGRErr createFromWkb( unsigned char *, OGRSpatialReference *,
                                 OGRGeometry **, int = -1 );
    static OGRErr createFromWkt( char **, OGRSpatialReference *,
                                 OGRGeometry ** );
    static OGRErr createFromFgf( unsigned char *, OGRSpatialReference *,
                                 OGRGeometry **, int = -1, int * = NULL );
    static OGRGeometry *createFromGML( const char * );
    static OGRGeometry *createFromGEOS( GEOSContextHandle_t hGEOSCtxt, GEOSGeom );

    static void   destroyGeometry( OGRGeometry * );
    static OGRGeometry *createGeometry( OGRwkbGeometryType );

    static OGRGeometry * forceToPolygon( OGRGeometry * );
    static OGRGeometry * forceToLineString( OGRGeometry *, bool bOnlyInOrder = true );
    static OGRGeometry * forceToMultiPolygon( OGRGeometry * );
    static OGRGeometry * forceToMultiPoint( OGRGeometry * );
    static OGRGeometry * forceToMultiLineString( OGRGeometry * );

    static OGRGeometry * organizePolygons( OGRGeometry **papoPolygons,
                                           int nPolygonCount,
                                           int *pbResultValidGeometry,
                                           const char **papszOptions = NULL);
    static int haveGEOS();

    static OGRGeometry* transformWithOptions( const OGRGeometry* poSrcGeom,
                                              OGRCoordinateTransformation *poCT,
                                              char** papszOptions );

    static OGRGeometry* 
        approximateArcAngles( double dfX, double dfY, double dfZ,
                              double dfPrimaryRadius, double dfSecondaryAxis, 
                              double dfRotation, 
                              double dfStartAngle, double dfEndAngle,
                              double dfMaxAngleStepSizeDegrees );
};

OGRwkbGeometryType CPL_DLL OGRFromOGCGeomType( const char *pszGeomType );
const char CPL_DLL * OGRToOGCGeomType( OGRwkbGeometryType eGeomType );

/* Prepared geometry API (needs GEOS >= 3.1.0) */
typedef struct _OGRPreparedGeometry OGRPreparedGeometry;
int OGRHasPreparedGeometrySupport();
OGRPreparedGeometry* OGRCreatePreparedGeometry( const OGRGeometry* poGeom );
void OGRDestroyPreparedGeometry( OGRPreparedGeometry* poPreparedGeom );
int OGRPreparedGeometryIntersects( const OGRPreparedGeometry* poPreparedGeom,
                                   const OGRGeometry* poOtherGeom );

#endif /* ndef _OGR_GEOMETRY_H_INCLUDED */
