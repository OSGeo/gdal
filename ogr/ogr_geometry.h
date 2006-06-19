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
 * Revision 1.62  2006/06/19 23:19:45  mloskot
 * Added new functions OGRLinearRing::isPointInRing and OGRPolygon::IsPointOnSurface.
 *
 * Revision 1.61  2006/03/31 17:57:32  fwarmerdam
 * header updates
 *
 * Revision 1.60  2005/12/02 11:06:51  osemykin
 * added const for OGRLineString::getPoints(...)
 *
 * Revision 1.59  2005/10/21 15:58:00  fwarmerdam
 * getGEOSGeometryFactory now returns void *
 *
 * Revision 1.58  2005/10/21 14:58:53  fwarmerdam
 * don't use geos::Geometry typedef for GEOSGeom
 *
 * Revision 1.57  2005/10/20 19:55:29  fwarmerdam
 * added GEOS C API support
 *
 * Revision 1.56  2005/09/17 12:11:30  osemykin
 * added OGRLineString::getPoints
 *
 * Revision 1.55  2005/08/04 17:18:59  fwarmerdam
 * now have separate 2D and 3D OGRPoint constructors
 *
 * Revision 1.54  2005/07/20 01:43:51  fwarmerdam
 * upgraded OGR geometry dimension handling
 *
 * Revision 1.53  2005/03/25 06:31:12  fwarmerdam
 * added addSubLineString
 *
 * Revision 1.52  2005/02/22 12:48:09  fwarmerdam
 * added OGRGeometryFactory::haveGEOS()
 *
 * Revision 1.51  2005/02/22 12:37:26  fwarmerdam
 * rename Equal/Intersect to Equals/Intersects
 *
 * Revision 1.50  2004/09/17 15:05:36  fwarmerdam
 * added get_Area() support
 *
 * Revision 1.49  2004/08/20 21:21:28  warmerda
 * added support for managing a persistent geos::GeometryFactory
 *
 * Revision 1.48  2004/07/10 04:54:23  warmerda
 * added GEOS methods, and closeRings
 *
 * Revision 1.47  2004/02/21 15:36:14  warmerda
 * const correctness updates for geometry: bug 289
 *
 * Revision 1.46  2003/09/11 22:47:54  aamici
 * add class constructors and destructors where needed in order to
 * let the mingw/cygwin binutils produce sensible partially linked objet files
 * with 'ld -r'.
 *
 * Revision 1.45  2003/08/27 15:40:37  warmerda
 * added support for generating DB2 V7.2 compatible WKB
 *
 * Revision 1.44  2003/05/28 19:16:42  warmerda
 * fixed up argument names and stuff for docs
 *
 * Revision 1.43  2003/04/28 15:39:33  warmerda
 * ryan added forceToMultiPolyline and forceToMultiPoint
 *
 * Revision 1.42  2003/03/06 20:29:27  warmerda
 * added GML import/export entry points
 *
 * Revision 1.41  2003/01/14 22:13:35  warmerda
 * added isClockwise() method on OGRLinearRing
 *
 * Revision 1.40  2003/01/08 22:04:11  warmerda
 * added forceToPolygon and forceToMultiPolygon methods
 *
 * Revision 1.39  2003/01/07 16:44:27  warmerda
 * added removeGeometry
 *
 * Revision 1.38  2003/01/02 21:45:23  warmerda
 * move OGRBuildPolygonsFromEdges into C API
 *
 * Revision 1.37  2002/10/25 15:20:50  warmerda
 * fixed MULTIPOINT WKT format
 *
 * Revision 1.36  2002/10/24 20:53:02  warmerda
 * expand tabs
 *
 * Revision 1.35  2002/09/26 18:13:17  warmerda
 * moved some defs to ogr_core.h for sharing with ogr_api.h
 *
 * Revision 1.34  2002/09/11 13:47:17  warmerda
 * preliminary set of fixes for 3D WKB enum
 *
 * Revision 1.33  2002/08/12 15:02:18  warmerda
 * added OGRRawPoint and OGREnvelope initializes
 *
 * Revision 1.32  2002/05/02 19:45:36  warmerda
 * added flattenTo2D() method
 *
 * Revision 1.31  2002/02/22 22:23:38  warmerda
 * added tolerances when assembling polygons
 *
 * Revision 1.30  2002/02/18 21:12:23  warmerda
 * added OGRBuildPolygonFromEdges
 *
 * Revision 1.29  2001/11/01 16:56:08  warmerda
 * added createGeometry and destroyGeometry methods
 *
 * Revision 1.28  2001/09/21 16:24:20  warmerda
 * added transform() and transformTo() methods
 *
 * Revision 1.27  2001/09/04 14:48:34  warmerda
 * added some more 2.5D geometry types
 *
 * Revision 1.26  2001/05/24 18:05:18  warmerda
 * substantial fixes to WKT support for MULTIPOINT/LINE/POLYGON
 *
 * Revision 1.25  2001/02/06 17:10:28  warmerda
 * export entry points from DLL
 *
 * Revision 1.24  2001/01/19 21:10:47  warmerda
 * replaced tabs
 *
 * Revision 1.23  2000/10/17 17:55:26  warmerda
 * added comments for byte orders
 *
 * Revision 1.22  2000/04/26 18:25:55  warmerda
 * added missing CPL_DLL attributes
 *
 * Revision 1.21  2000/03/14 21:38:17  warmerda
 * added method to translate geometrytype to name
 *
 * Revision 1.20  1999/11/18 19:02:20  warmerda
 * expanded tabs
 *
 * Revision 1.19  1999/11/04 16:26:12  warmerda
 * Added the addGeometryDirectly() method for containers.
 *
 * Revision 1.18  1999/09/22 13:19:09  warmerda
 * Added the addRingDirectly() method on OGRPolygon.
 *
 * Revision 1.17  1999/09/13 14:34:07  warmerda
 * updated to use wkbZ of 0x8000 instead of 0x80000000
 *
 * Revision 1.16  1999/09/13 02:27:32  warmerda
 * incorporated limited 2.5d support
 *
 * Revision 1.15  1999/08/29 17:14:29  warmerda
 * Added wkbNone.
 *
 * Revision 1.14  1999/07/27 00:48:11  warmerda
 * Added Equal() support
 *
 * Revision 1.13  1999/07/08 20:26:03  warmerda
 * No longer override getGeometryType() on OGRLinearRing.
 *
 * Revision 1.12  1999/07/07 04:23:07  danmo
 * Fixed typo in  #define _OGR_..._H_INCLUDED  line
 *
 * Revision 1.11  1999/07/06 21:36:46  warmerda
 * tenatively added getEnvelope() and Intersect()
 *
 * Revision 1.10  1999/06/25 20:44:42  warmerda
 * implemented assignSpatialReference, carry properly
 *
 * Revision 1.9  1999/05/31 20:44:11  warmerda
 * ogr_geometry.h
 *
 * Revision 1.8  1999/05/31 15:01:59  warmerda
 * OGRCurve now an abstract base class with essentially no implementation.
 * Everything moved down to OGRLineString where it belongs.  Also documented
 * classes.
 *
 * Revision 1.7  1999/05/31 11:05:08  warmerda
 * added some documentation
 *
 * Revision 1.6  1999/05/23 05:34:40  warmerda
 * added support for clone(), multipolygons and geometry collections
 *
 * Revision 1.5  1999/05/20 14:35:44  warmerda
 * added support for well known text format
 *
 * Revision 1.4  1999/05/17 14:39:13  warmerda
 * Added ICurve, and some other IGeometry and related methods.
 *
 * Revision 1.3  1999/05/14 13:30:59  warmerda
 * added IsEmpty() and IsSimple()
 *
 * Revision 1.2  1999/03/30 21:21:43  warmerda
 * added linearring/polygon support
 *
 * Revision 1.1  1999/03/29 21:21:10  warmerda
 * New
 *
 */

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

/************************************************************************/
/*                             OGRGeometry                              */
/************************************************************************/

/**
 * Abstract base class for all geometry classes.
 *
 * Note that the family of spatial analysis methods (Equal(), Disjoint(), ...,
 * ConvexHull(), Buffer(), ...) are not implemented at ths time.  Some other
 * required and optional geometry methods have also been omitted at this
 * time.
 */
 
class CPL_DLL OGRGeometry
{
  private:
    OGRSpatialReference * poSRS;                // may be NULL

  protected:
    int                   nCoordDimension;
    
  public:
                OGRGeometry();
    virtual     ~OGRGeometry();
                        
    // standard IGeometry
    virtual int getDimension() const = 0;
    virtual int getCoordinateDimension() const;
    virtual OGRBoolean  IsEmpty() const { return 0; } 
    virtual OGRBoolean  IsSimple() const { return 1; }
    virtual void        empty() = 0;
    virtual OGRGeometry *clone() const = 0;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const = 0;

    // IWks Interface
    virtual int WkbSize() const = 0;
    virtual OGRErr importFromWkb( unsigned char *, int=-1 )=0;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * ) const = 0;
    virtual OGRErr importFromWkt( char ** ppszInput ) = 0;
    virtual OGRErr exportToWkt( char ** ppszDstText ) const = 0;
    
    // non-standard
    virtual OGRwkbGeometryType getGeometryType() const = 0;
    virtual const char *getGeometryName() const = 0;
    virtual void   dumpReadable( FILE *, const char * = NULL );
    virtual void   flattenTo2D() = 0;
    virtual char * exportToGML() const;
    virtual GEOSGeom exportToGEOS() const;
    virtual void closeRings();

    virtual void setCoordinateDimension( int nDimension ); 

    void    assignSpatialReference( OGRSpatialReference * poSR );
    OGRSpatialReference *getSpatialReference( void ) const { return poSRS; }

    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) = 0;
    OGRErr  transformTo( OGRSpatialReference *poSR );

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

    virtual OGRGeometry *getBoundary() const;
    virtual double  Distance( const OGRGeometry * ) const;
    virtual OGRGeometry *ConvexHull() const;
    virtual OGRGeometry *Buffer( double dfDist, int nQuadSegs = 30 ) const;
    virtual OGRGeometry *Intersection( const OGRGeometry *) const;
    virtual OGRGeometry *Union( const OGRGeometry * ) const;
    virtual OGRGeometry *Difference( const OGRGeometry * ) const;
    virtual OGRGeometry *SymmetricDifference( const OGRGeometry * ) const;

    // backward compatibility methods. 
    OGRBoolean  Intersect( OGRGeometry * ) const;
    OGRBoolean  Equal( OGRGeometry * ) const;

    // Special HACK for DB2 7.2 support
    static int bGenerate_DB2_V72_BYTE_ORDER;
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
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText ) const;
    
    // IGeometry
    virtual int getDimension() const;
    virtual OGRGeometry *clone() const;
    virtual void empty();
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;

    // IPoint
    double      getX() const { return x; } 
    double      getY() const { return y; }
    double      getZ() const { return z; }

    // Non standard
    void        setX( double xIn ) { x = xIn; }
    void        setY( double yIn ) { y = yIn; }
    void        setZ( double zIn ) { z = zIn; nCoordDimension=3; }

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const;
    
    // Non standard from OGRGeometry
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();

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
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText ) const;

    // IGeometry interface
    virtual int getDimension() const;
    virtual OGRGeometry *clone() const;
    virtual void empty();
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;

    // ICurve methods
    virtual double get_Length() const;
    virtual void StartPoint(OGRPoint *) const;
    virtual void EndPoint(OGRPoint *) const;
    virtual void Value( double, OGRPoint * ) const;
    
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
    void        setNumPoints( int );
    void        setPoint( int, OGRPoint * );
    void        setPoint( int, double, double );
    void        setPoint( int, double, double, double );
    void        setPoints( int, OGRRawPoint *, double * = NULL );
    void        setPoints( int, double * padfX, double * padfY,
                           double *padfZ = NULL );
    void        addPoint( OGRPoint * );
    void        addPoint( double, double );
    void        addPoint( double, double, double );

    void        getPoints( OGRRawPoint *, double * = NULL ) const;

    void        addSubLineString( const OGRLineString *, 
                                  int nStartVertex = 0, int nEndVertex = -1 );

    // non-standard from OGRGeometry
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual const char *getGeometryName() const;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();

};

/************************************************************************/
/*                            OGRLinearRing                             */
/*                                                                      */
/*      This is an alias for OGRLineString for now.                     */
/************************************************************************/

/**
 * Concrete representation of a closed ring.
 *
 * This class is functionally equivelent to an OGRLineString, but has a
 * separate identity to maintain alignment with the OpenGIS simple feature
 * data model.  It exists to serve as a component of an OGRPolygon.
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
    virtual void closeRings();
    virtual double get_Area() const;
    OGRBoolean isPointInRing(const OGRPoint* pt) const;
    
    // IWks Interface - Note this isnt really a first class object
    // for the purposes of WKB form.  These methods always fail since this
    // object cant be serialized on its own. 
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int=-1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * ) const;
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
    virtual OGRErr      Centroid( OGRPoint * poPoint ) const = 0;
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
    
    // ISurface Interface
    virtual double      get_Area() const;
    virtual int         Centroid( OGRPoint * poPoint ) const;
    virtual int         PointOnSurface( OGRPoint * poPoint ) const;
    
    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText ) const;

    // IGeometry
    virtual int getDimension() const;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;

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

    OGRBoolean IsPointOnSurface( const OGRPoint * ) const;

    virtual void closeRings();
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

    int         nCoordinateDimension;
    
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
    
    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText ) const;

    // IGeometry methods
    virtual int getDimension() const;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;

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
    
    // Non standard
    virtual OGRErr addGeometryDirectly( OGRGeometry * );

    double  get_Area() const;
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
    OGRErr  importFromWkt_Bracketed( char ** );

  public:
            OGRMultiPoint();
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRGeometry *clone() const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ) const;
    
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
  public:
    static OGRErr createFromWkb( unsigned char *, OGRSpatialReference *,
                                 OGRGeometry **, int = -1 );
    static OGRErr createFromWkt( char **, OGRSpatialReference *,
                                 OGRGeometry ** );
    static OGRGeometry *createFromGML( const char * );
    static OGRGeometry *createFromGEOS( GEOSGeom );

    static void   destroyGeometry( OGRGeometry * );
    static OGRGeometry *createGeometry( OGRwkbGeometryType );

    static OGRGeometry * forceToPolygon( OGRGeometry * );
    static OGRGeometry * forceToMultiPolygon( OGRGeometry * );
    static OGRGeometry * forceToMultiPoint( OGRGeometry * );
    static OGRGeometry * forceToMultiLineString( OGRGeometry * );

    static void *getGEOSGeometryFactory();

    static int haveGEOS();

};

#endif /* ndef _OGR_GEOMETRY_H_INCLUDED */
