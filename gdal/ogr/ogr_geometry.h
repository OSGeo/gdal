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

          OGRRawPoint(double x, double y) : x(x), y(y) {}
    double      x;
    double      y;
};

typedef struct GEOSGeom_t *GEOSGeom;
typedef struct GEOSContextHandle_HS *GEOSContextHandle_t;

class OGRPoint;
class OGRCurve;
class OGRCompoundCurve;
class OGRLinearRing;
class OGRLineString;
class OGRSurface;
class OGRCurvePolygon;
class OGRPolygon;
class OGRMultiSurface;
class OGRMultiPolygon;
class OGRMultiCurve;
class OGRMultiLineString;

typedef OGRLineString* (*OGRCurveCasterToLineString)(OGRCurve*);
typedef OGRLinearRing* (*OGRCurveCasterToLinearRing)(OGRCurve*);

typedef OGRPolygon*      (*OGRSurfaceCasterToPolygon)(OGRSurface*);
typedef OGRCurvePolygon* (*OGRSurfaceCasterToCurvePolygon)(OGRSurface*);

/************************************************************************/
/*                             OGRGeometry                              */
/************************************************************************/

/**
 * Abstract base class for all geometry classes.
 *
 * Some spatial analysis methods require that OGR is built on the GEOS library
 * to work properly. The precise meaning of methods that describe spatial relationships
 * between geometries is described in the SFCOM, or other simple features interface
 * specifications, like "OpenGIS® Implementation Specification for
 * Geographic information - Simple feature access - Part 1: Common architecture"
 * (<a href="http://www.opengeospatial.org/standards/sfa">OGC 06-103r4</a>)
 *
 * In GDAL 2.0, the hierarchy of classes has been extended with
 * <a href="https://portal.opengeospatial.org/files/?artifact_id=32024">
 * (working draft) ISO SQL/MM Part 3 (ISO/IEC 13249-3)</a> curve geometries :
 * CIRCULARSTRING (OGRCircularString), COMPOUNDCURVE (OGRCompoundCurve),
 * CURVEPOLYGON (OGRCurvePolygon), MULTICURVE (OGRMultiCurve) and MULTISURFACE (OGRMultiSurface).
 *
 */
 
class CPL_DLL OGRGeometry
{
  private:
    OGRSpatialReference * poSRS;                // may be NULL

  protected:
    friend class OGRCurveCollection;

    int                   nCoordDimension;

    OGRErr                importPreambuleFromWkt( char ** ppszInput,
                                                  int* pbHasZ, int* pbHasM );
    OGRErr                importCurveCollectionFromWkt( char ** ppszInput,
                                                  int bAllowEmptyComponent,
                                                  int bAllowLineString,
                                                  int bAllowCurve,
                                                  int bAllowCompoundCurve,
                                                  OGRErr (*pfnAddCurveDirectly)(OGRGeometry* poSelf, OGRCurve* poCurve) );
    OGRErr                importPreambuleFromWkb( unsigned char * pabyData,
                                                  int nSize,
                                                  OGRwkbByteOrder& eByteOrder,
                                                  OGRBoolean& b3D,
                                                  OGRwkbVariant eWkbVariant );
    OGRErr                importPreambuleOfCollectionFromWkb(
                                                        unsigned char * pabyData,
                                                        int& nSize,
                                                        int& nDataOffset,
                                                        OGRwkbByteOrder& eByteOrder,
                                                        int nMinSubGeomSize,
                                                        int& nGeomCount,
                                                        OGRwkbVariant eWkbVariant );
  public:
                OGRGeometry();
                OGRGeometry( const OGRGeometry& other );
    virtual     ~OGRGeometry();
    
    OGRGeometry& operator=( const OGRGeometry& other );
                        
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
    virtual OGRErr importFromWkb( unsigned char *, int=-1, OGRwkbVariant=wkbVariantOldOgc )=0;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc ) const = 0;
    virtual OGRErr importFromWkt( char ** ppszInput ) = 0;
    virtual OGRErr exportToWkt( char ** ppszDstText, OGRwkbVariant=wkbVariantOldOgc ) const = 0;
    
    // non-standard
    virtual OGRwkbGeometryType getGeometryType() const = 0;
    OGRwkbGeometryType    getIsoGeometryType() const;
    virtual const char *getGeometryName() const = 0;
    virtual void   dumpReadable( FILE *, const char * = NULL, char** papszOptions = NULL ) const;
    virtual void   flattenTo2D() = 0;
    virtual char * exportToGML( const char* const * papszOptions = NULL ) const;
    virtual char * exportToKML() const;
    virtual char * exportToJson() const;

    static GEOSContextHandle_t createGEOSContext();
    static void freeGEOSContext(GEOSContextHandle_t hGEOSCtxt);
    virtual GEOSGeom exportToGEOS(GEOSContextHandle_t hGEOSCtxt) const;
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;
    virtual OGRGeometry* getCurveGeometry(const char* const* papszOptions = NULL) const;
    virtual OGRGeometry* getLinearGeometry(double dfMaxAngleStepSizeDegrees = 0,
                                             const char* const* papszOptions = NULL) const;

    virtual void closeRings();

    virtual void setCoordinateDimension( int nDimension ); 

    void    assignSpatialReference( OGRSpatialReference * poSR );
    OGRSpatialReference *getSpatialReference( void ) const { return poSRS; }

    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) = 0;
    OGRErr  transformTo( OGRSpatialReference *poSR );
    
    virtual void segmentize(double dfMaxLength);

    // ISpatialRelation
    virtual OGRBoolean  Intersects( const OGRGeometry * ) const;
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
    virtual OGRGeometry *DelaunayTriangulation(double dfTolerance, int bOnlyEdges) const;

    virtual OGRGeometry *Polygonize() const;

    // backward compatibility to non-standard method names. 
    OGRBoolean  Intersect( OGRGeometry * ) const CPL_WARN_DEPRECATED("Non standard method. Use Intersects() instead");
    OGRBoolean  Equal( OGRGeometry * ) const CPL_WARN_DEPRECATED("Non standard method. Use Equals() instead");
    virtual OGRGeometry *SymmetricDifference( const OGRGeometry * ) const CPL_WARN_DEPRECATED("Non standard method. Use SymDifference() instead");
    virtual OGRGeometry *getBoundary() const CPL_WARN_DEPRECATED("Non standard method. Use Boundary() instead");
    
    // Special HACK for DB2 7.2 support
    static int bGenerate_DB2_V72_BYTE_ORDER;

    virtual void        swapXY();
    
    static OGRGeometry* CastToIdentity(OGRGeometry* poGeom) { return poGeom; }
    static OGRGeometry* CastToError(OGRGeometry* poGeom);
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
                OGRPoint( const OGRPoint& other );
    virtual     ~OGRPoint();
    
    OGRPoint& operator=( const OGRPoint& other );

    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int=-1, OGRwkbVariant=wkbVariantOldOgc );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText, OGRwkbVariant=wkbVariantOldOgc ) const;
    
    // IGeometry
    virtual int getDimension() const;
    virtual int getCoordinateDimension() const;
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
    void        setX( double xIn ) { x = xIn; if (nCoordDimension <= 0) nCoordDimension = 2; }
    void        setY( double yIn ) { y = yIn; if (nCoordDimension <= 0) nCoordDimension = 2; }
    void        setZ( double zIn ) { z = zIn; nCoordDimension=3; }

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const;
    virtual OGRBoolean  Intersects( const OGRGeometry * ) const;
    virtual OGRBoolean  Within( const OGRGeometry * ) const;
    
    // Non standard from OGRGeometry
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();

    virtual void        swapXY();
};

/************************************************************************/
/*                            OGRPointIterator                          */
/************************************************************************/

/**
 * Interface for a point iterator.
 *
 * @since GDAL 2.0
 */

class CPL_DLL OGRPointIterator
{
    public:
        virtual ~OGRPointIterator();
        virtual OGRBoolean getNextPoint(OGRPoint* p) = 0;

        static void destroy(OGRPointIterator*);
};

/************************************************************************/
/*                               OGRCurve                               */
/************************************************************************/

/**
 * Abstract curve base class for OGRLineString, OGRCircularString and
 * OGRCompoundCurve
 */

class CPL_DLL OGRCurve : public OGRGeometry
{
  protected:
            OGRCurve();
            OGRCurve( const OGRCurve& other );

    virtual OGRCurveCasterToLineString GetCasterToLineString() const = 0;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing() const = 0;

    friend class OGRCurvePolygon;
    friend class OGRCompoundCurve;
    virtual int    ContainsPoint( const OGRPoint* p ) const;
    virtual double get_AreaOfCurveSegments() const = 0;

  public:
    virtual ~OGRCurve();
    
    OGRCurve& operator=( const OGRCurve& other );

    // ICurve methods
    virtual double get_Length() const = 0;
    virtual void StartPoint(OGRPoint *) const = 0;
    virtual void EndPoint(OGRPoint *) const = 0;
    virtual int  get_IsClosed() const;
    virtual void Value( double, OGRPoint * ) const = 0;
    virtual OGRLineString* CurveToLine(double dfMaxAngleStepSizeDegrees = 0,
                                       const char* const* papszOptions = NULL) const = 0;
    virtual int getDimension() const;

    // non standard
    virtual int getNumPoints() const = 0;
    virtual OGRPointIterator* getPointIterator() const = 0;
    virtual OGRBoolean IsConvex() const;
    virtual double get_Area() const = 0;

    static OGRCompoundCurve* CastToCompoundCurve(OGRCurve* puCurve);
    static OGRLineString*    CastToLineString(OGRCurve* poCurve);
    static OGRLinearRing*    CastToLinearRing(OGRCurve* poCurve);
};

/************************************************************************/
/*                             OGRSimpleCurve                           */
/************************************************************************/

/**
 * Abstract curve base class for OGRLineString and OGRCircularString
 *
 * Note: this class does not exist in SQL/MM standard and exists for
 * implementation convenience.
 *
 * @since GDAL 2.0
 */

class CPL_DLL OGRSimpleCurve: public OGRCurve
{
  protected:
    friend class OGRGeometry;

    int         nPointCount;
    OGRRawPoint *paoPoints;
    double      *padfZ;

    void        Make3D();
    void        Make2D();

    OGRErr      importFromWKTListOnly( char ** ppszInput, int bHasZ, int bHasM,
                                       OGRRawPoint*& paoPointsIn, int& nMaxPoints,
                                       double*& padfZIn );

    virtual double get_LinearArea() const;

                OGRSimpleCurve();
                OGRSimpleCurve( const OGRSimpleCurve& other );

  public:
    virtual     ~OGRSimpleCurve();
    
    OGRSimpleCurve& operator=( const OGRSimpleCurve& other );

    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1, OGRwkbVariant=wkbVariantOldOgc );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText, OGRwkbVariant=wkbVariantOldOgc ) const;

    // IGeometry interface
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
    virtual int getNumPoints() const { return nPointCount; }
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
                           double *padfZIn = NULL );
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
    virtual OGRPointIterator* getPointIterator() const;

    // non-standard from OGRGeometry
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();
    virtual void segmentize(double dfMaxLength);

    virtual void        swapXY();
};

/************************************************************************/
/*                            OGRLineString                             */
/************************************************************************/

/**
 * Concrete representation of a multi-vertex line.
 *
 * Note: for implementation convenience, we make it inherit from OGRSimpleCurve whereas
 * SFSQL and SQL/MM only make it inherits from OGRCurve.
 */

class CPL_DLL OGRLineString : public OGRSimpleCurve
{
  protected:
    static OGRLineString* TransferMembersAndDestroy(
                                            OGRLineString* poSrc,
                                            OGRLineString* poDst);

    static OGRLinearRing* CastToLinearRing(OGRLineString* poLS);

    virtual OGRCurveCasterToLineString GetCasterToLineString() const;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing() const;

    virtual double get_AreaOfCurveSegments() const;

  public:
                OGRLineString();
                OGRLineString(const OGRLineString& other);
    virtual    ~OGRLineString();
    
    OGRLineString& operator=(const OGRLineString& other);

    virtual OGRLineString* CurveToLine(double dfMaxAngleStepSizeDegrees = 0,
                                       const char* const* papszOptions = NULL) const;
    virtual OGRGeometry* getCurveGeometry(const char* const* papszOptions = NULL) const;
    virtual double get_Area() const;

    // non-standard from OGRGeometry
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual const char *getGeometryName() const;
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
 *
 * Note: this class exists in SFSQL 1.2, but not in ISO SQL/MM Part 3.
 */

class CPL_DLL OGRLinearRing : public OGRLineString
{
  protected:
    friend class OGRPolygon; 
    
    // These are not IWks compatible ... just a convenience for OGRPolygon.
    virtual int _WkbSize( int b3D ) const;
    virtual OGRErr _importFromWkb( OGRwkbByteOrder, int b3D,
                                   unsigned char *, int=-1 );
    virtual OGRErr _exportToWkb( OGRwkbByteOrder, int b3D, 
                                 unsigned char * ) const;
    
    static OGRLineString* CastToLineString(OGRLinearRing* poLR);

    virtual OGRCurveCasterToLineString GetCasterToLineString() const;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing() const;

  public:
                        OGRLinearRing();
                        OGRLinearRing(const OGRLinearRing& other);
                        OGRLinearRing( OGRLinearRing * );
    virtual            ~OGRLinearRing();
    
    OGRLinearRing& operator=(const OGRLinearRing& other);

    // Non standard.
    virtual const char *getGeometryName() const;
    virtual OGRGeometry *clone() const;
    virtual int isClockwise() const;
    virtual void reverseWindingOrder();
    virtual void closeRings();
    OGRBoolean isPointInRing(const OGRPoint* pt, int bTestEnvelope = TRUE) const;
    OGRBoolean isPointOnRingBoundary(const OGRPoint* pt, int bTestEnvelope = TRUE) const;
    
    // IWks Interface - Note this isnt really a first class object
    // for the purposes of WKB form.  These methods always fail since this
    // object cant be serialized on its own. 
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int=-1, OGRwkbVariant=wkbVariantOldOgc );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc ) const;
};

/************************************************************************/
/*                         OGRCircularString                            */
/************************************************************************/

/**
 * Concrete representation of a circular string, that is to say a curve made
 * of one or several arc circles.
 *
 * Note: for implementation convenience, we make it inherit from OGRSimpleCurve whereas
 * SQL/MM only makes it inherits from OGRCurve.
 *
 * Compatibility: ISO SQL/MM Part 3.
 *
 * @since GDAL 2.0
 */

class CPL_DLL OGRCircularString : public OGRSimpleCurve
{
  private:
    void        ExtendEnvelopeWithCircular( OGREnvelope * psEnvelope ) const;
    OGRBoolean  IsValidFast() const;
    int         IsFullCircle( double& cx, double& cy, double& square_R ) const;

  protected:
    virtual OGRCurveCasterToLineString GetCasterToLineString() const;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing() const;
    virtual int    ContainsPoint( const OGRPoint* p ) const;
    virtual double get_AreaOfCurveSegments() const;

  public:
                OGRCircularString();
                OGRCircularString(const OGRCircularString& other);
    virtual    ~OGRCircularString();
    
    OGRCircularString& operator=(const OGRCircularString& other);

    // IWks Interface
    virtual OGRErr importFromWkb( unsigned char *, int = -1, OGRwkbVariant=wkbVariantOldOgc );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText, OGRwkbVariant=wkbVariantOldOgc ) const;

    // IGeometry interface
    virtual OGRBoolean  IsValid() const;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const;

    // ICurve methods
    virtual double get_Length() const;
    virtual OGRLineString* CurveToLine(double dfMaxAngleStepSizeDegrees = 0,
                                       const char* const* papszOptions = NULL) const;
    virtual void Value( double, OGRPoint * ) const;
    virtual double get_Area() const;

    // non-standard from OGRGeometry
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual const char *getGeometryName() const;
    virtual void segmentize(double dfMaxLength);
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;
    virtual OGRGeometry* getLinearGeometry(double dfMaxAngleStepSizeDegrees = 0,
                                             const char* const* papszOptions = NULL) const;
};

/************************************************************************/
/*                           OGRCurveCollection                         */
/************************************************************************/

/**
 * Utility class to store a collection of curves. Used as a member of
 * OGRCompoundCurve and OGRCurvePolygon.
 *
 * This class is only exported because of linking issues. It should never
 * be directly used.
 *
 * @since GDAL 2.0
 */

class CPL_DLL OGRCurveCollection
{
  protected:
    friend class OGRCompoundCurve;
    friend class OGRCurvePolygon;
    friend class OGRPolygon;

    int         nCurveCount;
    OGRCurve  **papoCurves;

  public:
                OGRCurveCollection();
                OGRCurveCollection(const OGRCurveCollection& other);
               ~OGRCurveCollection();
    
    OGRCurveCollection& operator=(const OGRCurveCollection& other);

    void            empty(OGRGeometry* poGeom);
    OGRBoolean      IsEmpty() const;
    void            getEnvelope( OGREnvelope * psEnvelope ) const;
    void            getEnvelope( OGREnvelope3D * psEnvelope ) const;

    OGRErr          addCurveDirectly( OGRGeometry* poGeom, OGRCurve* poCurve,
                                      int bNeedRealloc );
    int             WkbSize() const;
    OGRErr          importPreambuleFromWkb( OGRGeometry* poGeom,
                                            unsigned char * pabyData,
                                            int& nSize,
                                            int& nDataOffset,
                                            OGRwkbByteOrder& eByteOrder,
                                            int nMinSubGeomSize,
                                            OGRwkbVariant eWkVariant );
    OGRErr          importBodyFromWkb( OGRGeometry* poGeom,
                                       unsigned char * pabyData,
                                       int nSize,
                                       int nDataOffset,
                                       int bAcceptCompoundCurve,
                                       OGRErr (*pfnAddCurveDirectlyFromWkb)(OGRGeometry* poGeom, OGRCurve* poCurve),
                                       OGRwkbVariant eWkVariant );
    OGRErr          exportToWkt( const OGRGeometry* poGeom, char ** ppszDstText ) const;
    OGRErr          exportToWkb( const OGRGeometry* poGeom, OGRwkbByteOrder,
                                 unsigned char *, OGRwkbVariant eWkbVariant ) const;
    OGRBoolean      Equals(OGRCurveCollection *poOCC) const;
    void            setCoordinateDimension( OGRGeometry* poGeom, int nNewDimension );
    int             getNumCurves() const;
    OGRCurve       *getCurve( int );
    const OGRCurve *getCurve( int ) const;
    OGRCurve       *stealCurve( int );
    OGRErr          transform( OGRGeometry* poGeom,
                               OGRCoordinateTransformation *poCT );
    void            flattenTo2D(OGRGeometry* poGeom);
    void            segmentize(double dfMaxLength);
    void            swapXY();
    OGRBoolean      hasCurveGeometry(int bLookForNonLinear) const;
};

/************************************************************************/
/*                            OGRCompoundCurve                          */
/************************************************************************/

/**
 * Concrete representation of a compound curve, made of curves: OGRLineString
 * and OGRCircularString. Each curve is connected by its first point to
 * the last point of the previous curve.
 *
 * Compatibility: ISO SQL/MM Part 3.
 *
 * @since GDAL 2.0
 */

class CPL_DLL OGRCompoundCurve : public OGRCurve
{
  private:
    OGRCurveCollection oCC;

    OGRErr      addCurveDirectlyInternal( OGRCurve* poCurve,
                                          double dfToleranceEps,
                                          int bNeedRealloc );
    static OGRErr addCurveDirectlyFromWkt( OGRGeometry* poSelf, OGRCurve* poCurve );
    static OGRErr addCurveDirectlyFromWkb( OGRGeometry* poSelf, OGRCurve* poCurve );
    OGRLineString* CurveToLineInternal(double dfMaxAngleStepSizeDegrees,
                                       const char* const* papszOptions,
                                       int bIsLinearRing) const;

  protected:
    static OGRLineString* CastToLineString(OGRCompoundCurve* poCC);
    static OGRLinearRing* CastToLinearRing(OGRCompoundCurve* poCC);

    virtual OGRCurveCasterToLineString GetCasterToLineString() const;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing() const;

  public:
                OGRCompoundCurve();
                OGRCompoundCurve(const OGRCompoundCurve& other);
    virtual     ~OGRCompoundCurve();
    
    OGRCompoundCurve& operator=(const OGRCompoundCurve& other);

    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1, OGRwkbVariant=wkbVariantOldOgc );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText, OGRwkbVariant=wkbVariantOldOgc ) const;

    // IGeometry interface
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
    virtual OGRLineString* CurveToLine(double dfMaxAngleStepSizeDegrees = 0,
                                       const char* const* papszOptions = NULL) const;
    
    virtual int getNumPoints() const;
    virtual double get_AreaOfCurveSegments() const;
    virtual double get_Area() const;

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const;

    // ICompoundCurve method
    int             getNumCurves() const;
    OGRCurve       *getCurve( int );
    const OGRCurve *getCurve( int ) const;
    
    // non standard.
    virtual void setCoordinateDimension( int nDimension ); 
        
    OGRErr         addCurve( OGRCurve*, double dfToleranceEps = 1e-14  );
    OGRErr         addCurveDirectly( OGRCurve*, double dfToleranceEps = 1e-14 );
    OGRCurve      *stealCurve( int );
    virtual OGRPointIterator* getPointIterator() const;

    // non-standard from OGRGeometry
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual const char *getGeometryName() const;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();
    virtual void segmentize(double dfMaxLength);
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;
    virtual OGRGeometry* getLinearGeometry(double dfMaxAngleStepSizeDegrees = 0,
                                             const char* const* papszOptions = NULL) const;

    virtual void        swapXY();
};

/************************************************************************/
/*                              OGRSurface                              */
/************************************************************************/

/**
 * Abstract base class for 2 dimensional objects like polygons or curve polygons.
 */

class CPL_DLL OGRSurface : public OGRGeometry
{
  protected:

    virtual OGRSurfaceCasterToPolygon      GetCasterToPolygon() const = 0;
    virtual OGRSurfaceCasterToCurvePolygon GetCasterToCurvePolygon() const = 0;

  public:
    virtual double      get_Area() const = 0;
    virtual OGRErr      PointOnSurface( OGRPoint * poPoint ) const = 0;

    static OGRPolygon*      CastToPolygon(OGRSurface* poSurface);
    static OGRCurvePolygon* CastToCurvePolygon(OGRSurface* poSurface);
};


/************************************************************************/
/*                          OGRCurvePolygon                             */
/************************************************************************/

/**
 * Concrete class representing curve polygons.
 *
 * Note that curve polygons consist of one outer (curve) ring, and zero or
 * more inner rings.  A curve polygon cannot represent disconnected
 * regions (such as multiple islands in a political body).  The
 * OGRMultiSurface must be used for this.
 *
 * Compatibility: ISO SQL/MM Part 3.
 *
 * @since GDAL 2.0
 */

class CPL_DLL OGRCurvePolygon : public OGRSurface
{
  private:
    OGRBoolean      ContainsPoint( const OGRPoint* p ) const;
    virtual int   checkRing( OGRCurve * poNewRing ) const;
    OGRErr        addRingDirectlyInternal( OGRCurve* poCurve, int bNeedRealloc );
    static OGRErr addCurveDirectlyFromWkt( OGRGeometry* poSelf, OGRCurve* poCurve );
    static OGRErr addCurveDirectlyFromWkb( OGRGeometry* poSelf, OGRCurve* poCurve );

  protected:
    friend class OGRPolygon;
    OGRCurveCollection oCC;

    static OGRPolygon* CastToPolygon(OGRCurvePolygon* poCP);

    virtual OGRSurfaceCasterToPolygon      GetCasterToPolygon() const;
    virtual OGRSurfaceCasterToCurvePolygon GetCasterToCurvePolygon() const;

  public:
                OGRCurvePolygon();
                OGRCurvePolygon(const OGRCurvePolygon&);
    virtual    ~OGRCurvePolygon();
    
    OGRCurvePolygon& operator=(const OGRCurvePolygon& other);

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRGeometry *clone() const;
    virtual void empty();
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();
    virtual OGRBoolean  IsEmpty() const;
    virtual void segmentize(double dfMaxLength);
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;
    virtual OGRGeometry* getLinearGeometry(double dfMaxAngleStepSizeDegrees = 0,
                                             const char* const* papszOptions = NULL) const;

    // ISurface Interface
    virtual double      get_Area() const;
    virtual int         PointOnSurface( OGRPoint * poPoint ) const;
    
    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1, OGRwkbVariant=wkbVariantOldOgc );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText, OGRwkbVariant eWkbVariant = wkbVariantOldOgc ) const;

    // IGeometry
    virtual int getDimension() const;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const;
    
    // ICurvePolygon
    virtual OGRPolygon* CurvePolyToPoly(double dfMaxAngleStepSizeDegrees = 0,
                                        const char* const* papszOptions = NULL) const;

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const;
    virtual OGRBoolean  Intersects( const OGRGeometry * ) const;
    virtual OGRBoolean  Contains( const OGRGeometry * ) const;

    // Non standard
    virtual void setCoordinateDimension( int nDimension ); 

    OGRErr        addRing( OGRCurve * );
    OGRErr        addRingDirectly( OGRCurve * );

    OGRCurve *getExteriorRingCurve();
    const OGRCurve *getExteriorRingCurve() const;
    int         getNumInteriorRings() const;
    OGRCurve *getInteriorRingCurve( int );
    const OGRCurve *getInteriorRingCurve( int ) const;

    OGRCurve *stealExteriorRingCurve();

    virtual void        swapXY();
};

/************************************************************************/
/*                              OGRPolygon                              */
/************************************************************************/

/**
 * Concrete class representing polygons.
 *
 * Note that the OpenGIS simple features polygons consist of one outer
 * ring (linearring), and zero or more inner rings.  A polygon cannot represent disconnected
 * regions (such as multiple islands in a political body).  The
 * OGRMultiPolygon must be used for this.
 */

class CPL_DLL OGRPolygon : public OGRCurvePolygon
{
  protected:
    friend class OGRMultiSurface;

    virtual int checkRing( OGRCurve * poNewRing ) const;
    OGRErr      importFromWKTListOnly( char ** ppszInput, int bHasZ, int bHasM,
                                       OGRRawPoint*& paoPoints, int& nMaxPoints,
                                       double*& padfZ );

    static OGRCurvePolygon* CastToCurvePolygon(OGRPolygon* poPoly);

    virtual OGRSurfaceCasterToPolygon      GetCasterToPolygon() const;
    virtual OGRSurfaceCasterToCurvePolygon GetCasterToCurvePolygon() const;

  public:
                OGRPolygon();
                OGRPolygon(const OGRPolygon& other);
    virtual    ~OGRPolygon();
    
    OGRPolygon& operator=(const OGRPolygon& other);

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;
    virtual OGRGeometry* getCurveGeometry(const char* const* papszOptions = NULL) const;
    virtual OGRGeometry* getLinearGeometry(double dfMaxAngleStepSizeDegrees = 0,
                                             const char* const* papszOptions = NULL) const;

    // ISurface Interface
    virtual int         PointOnSurface( OGRPoint * poPoint ) const;
    
    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1, OGRwkbVariant=wkbVariantOldOgc );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText, OGRwkbVariant=wkbVariantOldOgc ) const;
    
    // ICurvePolygon
    virtual OGRPolygon* CurvePolyToPoly(double dfMaxAngleStepSizeDegrees = 0,
                                        const char* const* papszOptions = NULL) const;

    OGRLinearRing *getExteriorRing();
    const OGRLinearRing *getExteriorRing() const;
    OGRLinearRing *getInteriorRing( int );
    const OGRLinearRing *getInteriorRing( int ) const;

    OGRLinearRing *stealExteriorRing();
    OGRLinearRing *stealInteriorRing(int);

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
    OGRErr      importFromWkbInternal( unsigned char * pabyData, int nSize, int nRecLevel,
                                       OGRwkbVariant );
    OGRErr      importFromWktInternal( char **ppszInput, int nRecLevel );

  protected:
    int         nGeomCount;
    OGRGeometry **papoGeoms;

    OGRErr                      exportToWktInternal( char ** ppszDstText,
                                                     OGRwkbVariant eWkbVariant,
                                                     const char* pszSkipPrefix ) const;
    virtual OGRBoolean         isCompatibleSubType( OGRwkbGeometryType ) const;
    
    static OGRGeometryCollection* TransferMembersAndDestroy(OGRGeometryCollection* poSrc,
                                                  OGRGeometryCollection* poDst);

  public:
                OGRGeometryCollection();
                OGRGeometryCollection(const OGRGeometryCollection& other);
    virtual     ~OGRGeometryCollection();
    
    OGRGeometryCollection& operator=(const OGRGeometryCollection& other);

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRGeometry *clone() const;
    virtual void empty();
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT );
    virtual void flattenTo2D();
    virtual OGRBoolean  IsEmpty() const;
    virtual void segmentize(double dfMaxLength);
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;
    virtual OGRGeometry* getCurveGeometry(const char* const* papszOptions = NULL) const;
    virtual OGRGeometry* getLinearGeometry(double dfMaxAngleStepSizeDegrees = 0, const char* const* papszOptions = NULL) const;

    // IWks Interface
    virtual int WkbSize() const;
    virtual OGRErr importFromWkb( unsigned char *, int = -1, OGRwkbVariant=wkbVariantOldOgc );
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *, OGRwkbVariant=wkbVariantOldOgc ) const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char ** ppszDstText, OGRwkbVariant=wkbVariantOldOgc ) const;

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
/*                          OGRMultiSurface                             */
/************************************************************************/

/**
 * A collection of non-overlapping OGRSurface.
 *
 * @since GDAL 2.0
 */

class CPL_DLL OGRMultiSurface : public OGRGeometryCollection
{
  protected:
    virtual OGRBoolean  isCompatibleSubType( OGRwkbGeometryType ) const;

  public:
            OGRMultiSurface();
            OGRMultiSurface(const OGRMultiSurface& other);
    virtual ~OGRMultiSurface();
    
    OGRMultiSurface& operator=(const OGRMultiSurface& other);

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc ) const;
    
    // IMultiSurface methods
    virtual OGRErr      PointOnSurface( OGRPoint * poPoint ) const;

    // IGeometry methods
    virtual int getDimension() const;

    // Non standard
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;

    static OGRMultiPolygon* CastToMultiPolygon(OGRMultiSurface* poMS);
};

/************************************************************************/
/*                           OGRMultiPolygon                            */
/************************************************************************/

/**
 * A collection of non-overlapping OGRPolygon.
 */

class CPL_DLL OGRMultiPolygon : public OGRMultiSurface
{
  protected:
    virtual OGRBoolean  isCompatibleSubType( OGRwkbGeometryType ) const;

  public:
            OGRMultiPolygon();
            OGRMultiPolygon(const OGRMultiPolygon& other);
    virtual ~OGRMultiPolygon();
    
    OGRMultiPolygon& operator=(const OGRMultiPolygon& other);

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc ) const;
    
    // IMultiSurface methods
    virtual OGRErr      PointOnSurface( OGRPoint * poPoint ) const;

    // Non standard
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;
    
    static OGRMultiSurface* CastToMultiSurface(OGRMultiPolygon* poMP);
};

/************************************************************************/
/*                            OGRMultiPoint                             */
/************************************************************************/

/**
 * A collection of OGRPoint.
 */

class CPL_DLL OGRMultiPoint : public OGRGeometryCollection
{
  private:
    OGRErr  importFromWkt_Bracketed( char **, int bHasM, int bHasZ );

  protected:
    virtual OGRBoolean  isCompatibleSubType( OGRwkbGeometryType ) const;

  public:
            OGRMultiPoint();
            OGRMultiPoint(const OGRMultiPoint& other);
    virtual ~OGRMultiPoint();
    
    OGRMultiPoint& operator=(const OGRMultiPoint& other);

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc ) const;

    // IGeometry methods
    virtual int getDimension() const;

    // Non standard
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;
};

/************************************************************************/
/*                          OGRMultiCurve                               */
/************************************************************************/

/**
 * A collection of OGRCurve.
 *
 * @since GDAL 2.0
 */

class CPL_DLL OGRMultiCurve : public OGRGeometryCollection
{
  protected:
    static OGRErr addCurveDirectlyFromWkt( OGRGeometry* poSelf, OGRCurve* poCurve );
    virtual OGRBoolean  isCompatibleSubType( OGRwkbGeometryType ) const;

  public:
            OGRMultiCurve();
            OGRMultiCurve(const OGRMultiCurve& other);
    virtual ~OGRMultiCurve();
    
    OGRMultiCurve& operator=(const OGRMultiCurve& other);

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRErr importFromWkt( char ** );
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc ) const;

    // IGeometry methods
    virtual int getDimension() const;
    
    // Non standard
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;

    static OGRMultiLineString* CastToMultiLineString(OGRMultiCurve* poMC);
};

/************************************************************************/
/*                          OGRMultiLineString                          */
/************************************************************************/

/**
 * A collection of OGRLineString.
 */

class CPL_DLL OGRMultiLineString : public OGRMultiCurve
{
  protected:
    virtual OGRBoolean  isCompatibleSubType( OGRwkbGeometryType ) const;

  public:
            OGRMultiLineString();
            OGRMultiLineString(const OGRMultiLineString& other);
    virtual ~OGRMultiLineString();
    
    OGRMultiLineString& operator=(const OGRMultiLineString& other);

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const;
    virtual OGRwkbGeometryType getGeometryType() const;
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc ) const;
    
    // Non standard
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;

    static OGRMultiCurve* CastToMultiCurve(OGRMultiLineString* poMLS);
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
                                 OGRGeometry **, int = -1, OGRwkbVariant=wkbVariantOldOgc );
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
    
    static OGRGeometry * forceTo( OGRGeometry* poGeom,
                                  OGRwkbGeometryType eTargetType,
                                  const char*const* papszOptions = NULL );

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

    static int GetCurveParmeters(double x0, double y0,
                                 double x1, double y1,
                                 double x2, double y2,
                                 double& R, double& cx, double& cy,
                                 double& alpha0, double& alpha1, double& alpha2 );
    static OGRLineString* curveToLineString( double x0, double y0, double z0,
                                             double x1, double y1, double z1,
                                             double x2, double y2, double z2,
                                             int bHasZ,
                                             double dfMaxAngleStepSizeDegrees,
                                             const char*const* papszOptions = NULL );
    static OGRCurve* curveFromLineString(const OGRLineString* poLS,
                                         const char*const* papszOptions = NULL);
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
