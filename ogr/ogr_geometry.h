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
 * List of well known binary geometry types.  These are used within the BLOBs
 * but are also returned from OGRGeometry::getGeometryType() to identify the
 * type of a geometry object.
 */

enum OGRwkbGeometryType
{
    wkbUnknown = 0,             // non-standard
    wkbPoint = 1,               // rest are standard WKB type codes
    wkbLineString = 2,
    wkbPolygon = 3,
    wkbMultiPoint = 4,
    wkbMultiLineString = 5,
    wkbMultiPolygon = 6,
    wkbGeometryCollection = 7,
    wkbNone = 100,              // non-standard, for pure attribute records
    wkbPoint25D = 0x8001,
    wkbLineString25D = 0x8002,
    wkbPolygon25D = 0x8003
};

#define ogrZMarker 0x21125711

const char CPL_DLL * OGRGeometryTypeToName( OGRwkbGeometryType );

enum OGRwkbByteOrder
{
    wkbXDR = 0,         /* MSB/Sun/Motoroloa: Most Significant Byte First   */
    wkbNDR = 1          /* LSB/Intel/Vax: Least Significant Byte First      */
};

/**
 * Simple container for a position.
 */
class OGRRawPoint
{
  public:
    double      x;
    double      y;
};

/**
 * Simple container for a bounding region.
 */

class OGREnvelope
{
  public:
    double      MinX;
    double      MaxX;
    double      MinY;
    double      MaxY;
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
 
class OGRGeometry
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
    virtual OGRErr importFromWkt( char ** ) = 0;
    virtual OGRErr exportToWkt( char ** ) = 0;
    
    // non-standard
    virtual OGRwkbGeometryType getGeometryType() = 0;
    virtual const char *getGeometryName() = 0;
    virtual void   dumpReadable( FILE *, const char * = NULL );

    void    assignSpatialReference( OGRSpatialReference * poSR );
    OGRSpatialReference *getSpatialReference( void ) { return poSRS; }

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
};

/************************************************************************/
/*                               OGRPoint                               */
/************************************************************************/

/**
 * Point class.
 *
 * Implements SFCOM IPoint methods.
 */

class OGRPoint : public OGRGeometry
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
    virtual OGRErr exportToWkt( char ** );
    
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
};

/************************************************************************/
/*                               OGRCurve                               */
/************************************************************************/

/**
 * Abstract curve base class.
 */

class OGRCurve : public OGRGeometry
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

class OGRLineString : public OGRCurve
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
    virtual OGRErr exportToWkt( char ** );

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

class OGRLinearRing : public OGRLineString
{
  private:
    friend class OGRPolygon; 
    
    // These are not IWks compatible ... just a convenience for OGRPolygon.
    virtual int _WkbSize();
    virtual OGRErr _importFromWkb( OGRwkbByteOrder, unsigned char *, int=-1 );
    virtual OGRErr _exportToWkb( OGRwkbByteOrder, unsigned char * );
    
  public:
                        OGRLinearRing();
                        OGRLinearRing( OGRLinearRing * );

    // Non standard.
    virtual const char *getGeometryName();
    virtual OGRGeometry *clone();
    
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

class OGRSurface : public OGRGeometry
{
  public:
    virtual double      get_Area() = 0;
    virtual OGRErr      Centroid( OGRPoint * ) = 0;
    virtual OGRErr      PointOnSurface( OGRPoint * ) = 0;
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

class OGRPolygon : public OGRSurface
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
    
    // ISurface Interface
    virtual double      get_Area();
    virtual int         Centroid( OGRPoint * );
    virtual int         PointOnSurface( OGRPoint * );
    
    // IWks Interface
    virtual int WkbSize();
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * );
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** );

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

class OGRGeometryCollection : public OGRGeometry
{
    int         nGeomCount;
    OGRGeometry **papoGeoms;
    
  public:
                OGRGeometryCollection();
    virtual     ~OGRGeometryCollection();

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
    virtual OGRGeometry *clone();
    virtual void empty();
    
    // IWks Interface
    virtual int WkbSize();
    virtual OGRErr importFromWkb( unsigned char *, int = -1 );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char * );
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** );

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

class OGRMultiPolygon : public OGRGeometryCollection
{
  public:
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
    virtual OGRGeometry *clone();
    
    // Non standard
    virtual OGRErr addGeometry( OGRGeometry * );
};

/************************************************************************/
/*                            OGRMultiPoint                             */
/************************************************************************/

/**
 * A collection of OGRPoints.
 */

class OGRMultiPoint : public OGRGeometryCollection
{
  public:
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
    virtual OGRGeometry *clone();
    
    // Non standard
    virtual OGRErr addGeometry( OGRGeometry * );
};

/************************************************************************/
/*                          OGRMultiLineString                          */
/************************************************************************/

/**
 * A collection of OGRLineStrings.
 */

class OGRMultiLineString : public OGRGeometryCollection
{
  public:
    // Non standard (OGRGeometry).
    virtual const char *getGeometryName();
    virtual OGRwkbGeometryType getGeometryType();
    virtual OGRGeometry *clone();
    
    // Non standard
    virtual OGRErr addGeometry( OGRGeometry * );
};


/************************************************************************/
/*                          OGRGeometryFactory                          */
/************************************************************************/

/**
 * Create geometry objects from well known text/binary.
 */

class OGRGeometryFactory
{
  public:
    static OGRErr createFromWkb( unsigned char *, OGRSpatialReference *,
                                 OGRGeometry **, int = -1 );
    static OGRErr createFromWkt( char **, OGRSpatialReference *,
                                 OGRGeometry ** );
};

#endif /* ndef _OGR_GEOMETRY_H_INCLUDED */
