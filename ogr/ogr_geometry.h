/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes for manipulating simple features that is not specific
 *           to a particular interface technology.
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
    
  public:
                OGRGeometry();
    virtual     ~OGRGeometry();
                        
    // standard IGeometry
    virtual int getDimension() = 0;
    virtual int getCoordinateDimension() = 0;
    virtual OGRBoolean  IsEmpty() { return 0; } 
    virtual OGRBoolean  IsSimple() { return 1; }
    virtual void        empty() = 0;
    virtual OGRGeometry *clone() = 0;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) = 0;

    // IWks Interface
    virtual int WkbSize() = 0;
    virtual OGRErr importFromWkb( unsigned char *, int=-1 )=0;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * ) = 0;
    virtual OGRErr importFromWkt( char ** ppszInput ) = 0;
    virtual OGRErr exportToWkt( char ** ppszDstText ) = 0;
    
    // non-standard
    virtual OGRwkbGeometryType getGeometryType() = 0;
    virtual const char *getGeometryName() = 0;
    virtual void   dumpReadable( FILE *, const char * = NULL );
    virtual void   flattenTo2D() = 0;
    virtual char * exportToGML() const;

    void    assignSpatialReference( OGRSpatialReference * poSR );
    OGRSpatialReference *getSpatialReference( void ) { return poSRS; }

    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) = 0;
    OGRErr  transformTo( OGRSpatialReference *poSR );

    // ISpatialRelation
    virtual OGRBoolean  Intersect( OGRGeometry * );
    virtual OGRBoolean  Equal( OGRGeometry * ) = 0;
    
#ifdef notdef
    
    // I presume all these should be virtual?  How many
    // should be pure?
    OGRGeometry *getBoundary();

    OGRBoolean  Disjoint( OGRGeometry * );
    OGRBoolean  Touch( OGRGeometry * );
    OGRBoolean  Cross( OGRGeometry * );
    OGRBoolean  Within( OGRGeometry * );
    OGRBoolean  Contains( OGRGeometry * );
    OGRBoolean  Overlap( OGRGeometry * );
    OGRBoolean  Relate( OGRGeometry *, const char * );

    double      Distance( OGRGeometry * );
    OGRGeometry *Intersection(OGRGeometry *);
    OGRGeometry *Buffer( double );
    OGRGeometry *ConvexHull();
    OGRGeometry *Union( OGRGeometry * );
    OGRGeometry *Difference( OGRGeometry * );
    OGRGeometry *SymmetricDifference( OGRGeometry * );
#endif    

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
                OGRPoint( double x, double y, double z = 0.0 );
    virtual     ~OGRPoint();

    // IWks Interface
    virtual int WkbSize();
    virtual OGRErr importFromWkb( unsigned char *, int=-1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * );
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText );
    
    // IGeometry
    virtual int getDimension();
    virtual int getCoordinateDimension();
    virtual OGRGeometry *clone();
    virtual void empty();
    virtual void getEnvelope( OGREnvelope * psEnvelope );

    // IPoint
    double      getX() { return x; }
    double      getY() { return y; }
    double      getZ() { return z; }

    // Non standard
    void        setX( double xIn ) { x = xIn; }
    void        setY( double yIn ) { y = yIn; }
    void        setZ( double zIn ) { z = zIn; }

    // ISpatialRelation
    virtual OGRBoolean  Equal( OGRGeometry * );
    
    // Non standard from OGRGeometry
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
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
    // ICurve methods
    virtual double get_Length() = 0;
    virtual void StartPoint(OGRPoint *) = 0;
    virtual void EndPoint(OGRPoint *) = 0;
    virtual int  get_IsClosed();
    virtual void Value( double, OGRPoint * ) = 0;

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
    virtual int WkbSize();
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * );
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText );

    // IGeometry interface
    virtual int getDimension();
    virtual int getCoordinateDimension();
    virtual OGRGeometry *clone();
    virtual void empty();
    virtual void getEnvelope( OGREnvelope * psEnvelope );

    // ICurve methods
    virtual double get_Length();
    virtual void StartPoint(OGRPoint *);
    virtual void EndPoint(OGRPoint *);
    virtual void Value( double, OGRPoint * );
    
    // ILineString methods
    int         getNumPoints() { return nPointCount; }
    void        getPoint( int, OGRPoint * );
    double      getX( int i ) { return paoPoints[i].x; }
    double      getY( int i ) { return paoPoints[i].y; }
    double      getZ( int i );

    // ISpatialRelation
    virtual OGRBoolean  Equal( OGRGeometry * );
    
    // non standard.
    void        setNumPoints( int );
    void        setPoint( int, OGRPoint * );
    void        setPoint( int, double, double, double = 0.0 );
    void        setPoints( int, OGRRawPoint *, double * = NULL );
    void        setPoints( int, double * padfX, double * padfY,
                           double *padfZ = NULL );
    void        addPoint( OGRPoint * );
    void        addPoint( double, double, double = 0.0 );

    // non-standard from OGRGeometry
    virtual OGRwkbGeometryType getGeometryType();
    virtual const char *getGeometryName();
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
    virtual int _WkbSize( int b3D );
    virtual OGRErr _importFromWkb( OGRwkbByteOrder, int b3D,
                                   unsigned char *, int=-1 );
    virtual OGRErr _exportToWkb( OGRwkbByteOrder, int b3D, unsigned char * );
    
  public:
                        OGRLinearRing();
                        OGRLinearRing( OGRLinearRing * );

    // Non standard.
    virtual const char *getGeometryName();
    virtual OGRGeometry *clone();
    virtual int isClockwise();
    
    // IWks Interface - Note this isnt really a first class object
    // for the purposes of WKB form.  These methods always fail since this
    // object cant be serialized on its own. 
    virtual int WkbSize();
    virtual OGRErr importFromWkb( unsigned char *, int=-1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * );
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
    virtual double      get_Area() = 0;
    virtual OGRErr      Centroid( OGRPoint * poPoint ) = 0;
    virtual OGRErr      PointOnSurface( OGRPoint * poPoint ) = 0;
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
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
    virtual OGRGeometry *clone();
    virtual void empty();
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();
    
    // ISurface Interface
    virtual double      get_Area();
    virtual int         Centroid( OGRPoint * poPoint );
    virtual int         PointOnSurface( OGRPoint * poPoint );
    
    // IWks Interface
    virtual int WkbSize();
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * );
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText );

    // IGeometry
    virtual int getDimension();
    virtual int getCoordinateDimension();
    virtual void getEnvelope( OGREnvelope * psEnvelope );

    // ISpatialRelation
    virtual OGRBoolean  Equal( OGRGeometry * );
    
    // Non standard
    void        addRing( OGRLinearRing * );
    void        addRingDirectly( OGRLinearRing * );

    OGRLinearRing *getExteriorRing();
    int         getNumInteriorRings();
    OGRLinearRing *getInteriorRing( int );
        

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
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
    virtual OGRGeometry *clone();
    virtual void empty();
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();
    
    // IWks Interface
    virtual int WkbSize();
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * );
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText );

    // IGeometry methods
    virtual int getDimension();
    virtual int getCoordinateDimension();
    virtual void getEnvelope( OGREnvelope * psEnvelope );

    // IGeometryCollection
    int         getNumGeometries();
    OGRGeometry *getGeometryRef( int );

    // ISpatialRelation
    virtual OGRBoolean  Equal( OGRGeometry * );
    
    // Non standard
    virtual OGRErr addGeometry( OGRGeometry * );
    virtual OGRErr addGeometryDirectly( OGRGeometry * );
    virtual OGRErr removeGeometry( int iIndex, int bDelete = TRUE );
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
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
    virtual OGRGeometry *clone();
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** );
    
    // Non standard
    virtual OGRErr addGeometryDirectly( OGRGeometry * );
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
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
    virtual OGRGeometry *clone();
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** );
    
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
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
    virtual OGRGeometry *clone();
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** );
    
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

    static void   destroyGeometry( OGRGeometry * );
    static OGRGeometry *createGeometry( OGRwkbGeometryType );

    static OGRGeometry * forceToPolygon( OGRGeometry * );
    static OGRGeometry * forceToMultiPolygon( OGRGeometry * );
    static OGRGeometry * forceToMultiPoint( OGRGeometry * );
    static OGRGeometry * forceToMultiLineString( OGRGeometry * );
};

#endif /* ndef _OGR_GEOMETRY_H_INCLUDED */
