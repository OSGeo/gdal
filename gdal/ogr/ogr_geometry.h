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

#ifndef OGR_GEOMETRY_H_INCLUDED
#define OGR_GEOMETRY_H_INCLUDED

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
          /** Constructor */
          OGRRawPoint() : x(0.0), y(0.0) {}

          /** Constructor */
          OGRRawPoint(double xIn, double yIn) : x(xIn), y(yIn) {}

    /** x */
    double      x;
    /** y */
    double      y;
};

/** GEOS geometry type */
typedef struct GEOSGeom_t *GEOSGeom;
/** GEOS context handle type */
typedef struct GEOSContextHandle_HS *GEOSContextHandle_t;
/** SFCGAL geometry type */
typedef void sfcgal_geometry_t;

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
class OGRTriangle;
class OGRPolyhedralSurface;
class OGRTriangulatedSurface;

//! @cond Doxygen_Suppress
typedef OGRLineString* (*OGRCurveCasterToLineString)(OGRCurve*);
typedef OGRLinearRing* (*OGRCurveCasterToLinearRing)(OGRCurve*);

typedef OGRPolygon*      (*OGRSurfaceCasterToPolygon)(OGRSurface*);
typedef OGRCurvePolygon* (*OGRSurfaceCasterToCurvePolygon)(OGRSurface*);
typedef OGRMultiPolygon* (*OGRPolyhedralSurfaceCastToMultiPolygon)(OGRPolyhedralSurface*);
//! @endcond

/************************************************************************/
/*                             OGRGeometry                              */
/************************************************************************/

/**
 * Abstract base class for all geometry classes.
 *
 * Some spatial analysis methods require that OGR is built on the GEOS library
 * to work properly. The precise meaning of methods that describe spatial
 * relationships between geometries is described in the SFCOM, or other simple
 * features interface specifications, like "OpenGIS® Implementation
 * Specification for Geographic information - Simple feature access - Part 1:
 * Common architecture"
 * (<a href="http://www.opengeospatial.org/standards/sfa">OGC 06-103r4</a>)
 *
 * In GDAL 2.0, the hierarchy of classes has been extended with
 * <a href="https://portal.opengeospatial.org/files/?artifact_id=32024">
 * (working draft) ISO SQL/MM Part 3 (ISO/IEC 13249-3)</a> curve geometries :
 * CIRCULARSTRING (OGRCircularString), COMPOUNDCURVE (OGRCompoundCurve),
 * CURVEPOLYGON (OGRCurvePolygon), MULTICURVE (OGRMultiCurve) and
 *  MULTISURFACE (OGRMultiSurface).
 *
 */

class CPL_DLL OGRGeometry
{
  private:
    OGRSpatialReference * poSRS;                // may be NULL

  protected:
//! @cond Doxygen_Suppress
    friend class OGRCurveCollection;

    unsigned int flags;

    OGRErr       importPreambuleFromWkt( char ** ppszInput,
                                         int* pbHasZ, int* pbHasM,
                                         bool* pbIsEmpty );
    OGRErr       importCurveCollectionFromWkt(
                     char ** ppszInput,
                     int bAllowEmptyComponent,
                     int bAllowLineString,
                     int bAllowCurve,
                     int bAllowCompoundCurve,
                     OGRErr (*pfnAddCurveDirectly)(OGRGeometry* poSelf,
                                                   OGRCurve* poCurve) );
    OGRErr       importPreambuleFromWkb( const unsigned char * pabyData,
                                         int nSize,
                                         OGRwkbByteOrder& eByteOrder,
                                         OGRwkbVariant eWkbVariant );
    OGRErr       importPreambuleOfCollectionFromWkb(
                     const unsigned char * pabyData,
                     int& nSize,
                     int& nDataOffset,
                     OGRwkbByteOrder& eByteOrder,
                     int nMinSubGeomSize,
                     int& nGeomCount,
                     OGRwkbVariant eWkbVariant );
    OGRErr       PointOnSurfaceInternal( OGRPoint * poPoint ) const;
    OGRBoolean   IsSFCGALCompatible() const;
//! @endcond

  public:

/************************************************************************/
/*                   Bit flags for OGRGeometry                          */
/*          The OGR_G_NOT_EMPTY_POINT is used *only* for points.        */
/*          Do not use these outside of the core.                       */
/*          Use Is3D, IsMeasured, set3D, and setMeasured instead        */
/************************************************************************/

//! @cond Doxygen_Suppress
    static const unsigned int OGR_G_NOT_EMPTY_POINT = 0x1;
    static const unsigned int OGR_G_3D = 0x2;
    static const unsigned int OGR_G_MEASURED = 0x4;
//! @endcond

                OGRGeometry();
                OGRGeometry( const OGRGeometry& other );
    virtual     ~OGRGeometry();

    OGRGeometry& operator=( const OGRGeometry& other );

    // Standard IGeometry.
    virtual int getDimension() const = 0;
    virtual int getCoordinateDimension() const;
    int CoordinateDimension() const;
    virtual OGRBoolean  IsEmpty() const = 0;
    virtual OGRBoolean  IsValid() const;
    virtual OGRBoolean  IsSimple() const;
    /*! Returns whether the geometry has a Z component. */
    OGRBoolean  Is3D() const { return flags & OGR_G_3D; }
    /*! Returns whether the geometry has a M component. */
    OGRBoolean  IsMeasured() const { return flags & OGR_G_MEASURED; }
    virtual OGRBoolean  IsRing() const;
    virtual void        empty() = 0;
    virtual OGRGeometry *clone() const CPL_WARN_UNUSED_RESULT = 0;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const = 0;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const = 0;

    // IWks Interface.
    virtual int WkbSize() const = 0;
    OGRErr importFromWkb( unsigned char *, int=-1,
                                  OGRwkbVariant=wkbVariantOldOgc );
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) = 0;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc ) const = 0;
    virtual OGRErr importFromWkt( char ** ppszInput ) = 0;
    virtual OGRErr exportToWkt( char ** ppszDstText,
                                OGRwkbVariant=wkbVariantOldOgc ) const = 0;

    // Non-standard.
    virtual OGRwkbGeometryType getGeometryType() const = 0;
    OGRwkbGeometryType    getIsoGeometryType() const;
    virtual const char *getGeometryName() const = 0;
    virtual void   dumpReadable( FILE *, const char * = NULL
                                 , char** papszOptions = NULL ) const;
    virtual void   flattenTo2D() = 0;
    virtual char * exportToGML( const char* const * papszOptions = NULL ) const;
    virtual char * exportToKML() const;
    virtual char * exportToJson() const;

    static GEOSContextHandle_t createGEOSContext();
    static void freeGEOSContext( GEOSContextHandle_t hGEOSCtxt );
    virtual GEOSGeom exportToGEOS( GEOSContextHandle_t hGEOSCtxt )
        const CPL_WARN_UNUSED_RESULT;
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE) const;
    virtual OGRGeometry* getCurveGeometry(
        const char* const* papszOptions = NULL ) const CPL_WARN_UNUSED_RESULT;
    virtual OGRGeometry* getLinearGeometry(
        double dfMaxAngleStepSizeDegrees = 0,
        const char* const* papszOptions = NULL ) const CPL_WARN_UNUSED_RESULT;

    // SFCGAL interfacing methods.
//! @cond Doxygen_Suppress
    static sfcgal_geometry_t* OGRexportToSFCGAL( OGRGeometry *poGeom );
    static OGRGeometry* SFCGALexportToOGR( sfcgal_geometry_t* _geometry );
//! @endcond
    virtual void closeRings();

    virtual void setCoordinateDimension( int nDimension );
    virtual void set3D( OGRBoolean bIs3D );
    virtual void setMeasured( OGRBoolean bIsMeasured );

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
//    virtual OGRGeometry *LocateAlong( double mValue ) const;
//    virtual OGRGeometry *LocateBetween( double mStart, double mEnd ) const;

    virtual OGRGeometry *Boundary() const CPL_WARN_UNUSED_RESULT;
    virtual double  Distance( const OGRGeometry * ) const ;
    virtual OGRGeometry *ConvexHull() const CPL_WARN_UNUSED_RESULT;
    virtual OGRGeometry *Buffer( double dfDist, int nQuadSegs = 30 )
        const CPL_WARN_UNUSED_RESULT;
    virtual OGRGeometry *Intersection( const OGRGeometry *)
        const CPL_WARN_UNUSED_RESULT;
    virtual OGRGeometry *Union( const OGRGeometry * )
        const CPL_WARN_UNUSED_RESULT;
    virtual OGRGeometry *UnionCascaded() const CPL_WARN_UNUSED_RESULT;
    virtual OGRGeometry *Difference( const OGRGeometry * )
        const CPL_WARN_UNUSED_RESULT;
    virtual OGRGeometry *SymDifference( const OGRGeometry * )
        const CPL_WARN_UNUSED_RESULT;
    virtual OGRErr       Centroid( OGRPoint * poPoint ) const;
    virtual OGRGeometry *Simplify(double dTolerance)
        const CPL_WARN_UNUSED_RESULT;
    OGRGeometry *SimplifyPreserveTopology(double dTolerance)
        const CPL_WARN_UNUSED_RESULT;
    virtual OGRGeometry *DelaunayTriangulation(
        double dfTolerance, int bOnlyEdges ) const CPL_WARN_UNUSED_RESULT;

    virtual OGRGeometry *Polygonize() const CPL_WARN_UNUSED_RESULT;

    virtual double Distance3D( const OGRGeometry *poOtherGeom ) const;

//! @cond Doxygen_Suppress
    // backward compatibility to non-standard method names.
    OGRBoolean  Intersect( OGRGeometry * )
        const CPL_WARN_DEPRECATED("Non standard method. "
                                  "Use Intersects() instead");
    OGRBoolean  Equal( OGRGeometry * )
        const CPL_WARN_DEPRECATED("Non standard method. "
                                  "Use Equals() instead");
    OGRGeometry *SymmetricDifference( const OGRGeometry * )
        const CPL_WARN_DEPRECATED("Non standard method. "
                                  "Use SymDifference() instead");
    OGRGeometry *getBoundary()
        const CPL_WARN_DEPRECATED("Non standard method. "
                                  "Use Boundary() instead");
//! @endcond

//! @cond Doxygen_Suppress
    // Special HACK for DB2 7.2 support
    static int bGenerate_DB2_V72_BYTE_ORDER;
//! @endcond

    virtual void        swapXY();
//! @cond Doxygen_Suppress
    static OGRGeometry* CastToIdentity( OGRGeometry* poGeom ) { return poGeom; }
    static OGRGeometry* CastToError( OGRGeometry* poGeom );
//! @endcond
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
    double      m;

  public:
                OGRPoint();
                OGRPoint( double x, double y );
                OGRPoint( double x, double y, double z );
                OGRPoint( double x, double y, double z, double m );
                OGRPoint( const OGRPoint& other );
    virtual     ~OGRPoint();

    OGRPoint& operator=( const OGRPoint& other );

    // IWks Interface
    virtual int WkbSize() const CPL_OVERRIDE;
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char ** ppszDstText,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IGeometry
    virtual int getDimension() const CPL_OVERRIDE;
    virtual OGRGeometry *clone() const CPL_OVERRIDE;
    virtual void empty() CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const CPL_OVERRIDE;
    virtual OGRBoolean  IsEmpty() const CPL_OVERRIDE
        { return !(flags & OGR_G_NOT_EMPTY_POINT); }

    // IPoint
    /** Return x */
    double      getX() const { return x; }
    /** Return y */
    double      getY() const { return y; }
    /** Return z */
    double      getZ() const { return z; }
    /** Return m */
    double      getM() const { return m; }

    // Non standard
    virtual void setCoordinateDimension( int nDimension ) CPL_OVERRIDE;
    /** Set x
     * @param xIn x
     */
    void        setX( double xIn ) { x = xIn; flags |= OGR_G_NOT_EMPTY_POINT; }
    /** Set y
     * @param yIn y
     */
    void        setY( double yIn ) { y = yIn; flags |= OGR_G_NOT_EMPTY_POINT; }
    /** Set z
     * @param zIn z
     */
    void        setZ( double zIn )
        { z = zIn; flags |= (OGR_G_NOT_EMPTY_POINT | OGR_G_3D); }
    /** Set m
     * @param mIn m
     */
    void        setM( double mIn )
        { m = mIn; flags |= (OGR_G_NOT_EMPTY_POINT | OGR_G_MEASURED); }

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const CPL_OVERRIDE;
    virtual OGRBoolean  Intersects( const OGRGeometry * ) const CPL_OVERRIDE;
    virtual OGRBoolean  Within( const OGRGeometry * ) const CPL_OVERRIDE;

    // Non standard from OGRGeometry
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) CPL_OVERRIDE;
    virtual void flattenTo2D() CPL_OVERRIDE;

    virtual void        swapXY() CPL_OVERRIDE;
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
        virtual OGRBoolean getNextPoint( OGRPoint* p ) = 0;

        static void destroy( OGRPointIterator* );
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
//! @cond Doxygen_Suppress
            OGRCurve();
            OGRCurve( const OGRCurve& other );

    virtual OGRCurveCasterToLineString GetCasterToLineString() const = 0;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing() const = 0;

    friend class OGRCurvePolygon;
    friend class OGRCompoundCurve;
//! @endcond
    virtual int    ContainsPoint( const OGRPoint* p ) const;
    virtual double get_AreaOfCurveSegments() const = 0;

  public:
    virtual ~OGRCurve();

//! @cond Doxygen_Suppress
    OGRCurve& operator=( const OGRCurve& other );
//! @endcond

    // ICurve methods
    virtual double get_Length() const = 0;
    virtual void StartPoint( OGRPoint * ) const = 0;
    virtual void EndPoint( OGRPoint * ) const = 0;
    virtual int  get_IsClosed() const;
    virtual void Value( double, OGRPoint * ) const = 0;
    virtual OGRLineString* CurveToLine( double dfMaxAngleStepSizeDegrees = 0,
                                        const char* const* papszOptions = NULL)
        const = 0;
    virtual int getDimension() const CPL_OVERRIDE;

    // non standard
    virtual int getNumPoints() const = 0;
    virtual OGRPointIterator* getPointIterator() const = 0;
    virtual OGRBoolean IsConvex() const;
    virtual double get_Area() const = 0;

    static OGRCompoundCurve* CastToCompoundCurve( OGRCurve* puCurve );
    static OGRLineString*    CastToLineString( OGRCurve* poCurve );
    static OGRLinearRing*    CastToLinearRing( OGRCurve* poCurve );
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
//! @cond Doxygen_Suppress
    friend class OGRGeometry;

    int         nPointCount;
    OGRRawPoint *paoPoints;
    double      *padfZ;
    double      *padfM;

    void        Make3D();
    void        Make2D();
    void        RemoveM();
    void        AddM();

    OGRErr      importFromWKTListOnly( char ** ppszInput, int bHasZ, int bHasM,
                                       OGRRawPoint*& paoPointsIn,
                                       int& nMaxPoints,
                                       double*& padfZIn );

//! @endcond

    virtual double get_LinearArea() const;

                OGRSimpleCurve();
                OGRSimpleCurve( const OGRSimpleCurve& other );

  public:
    virtual     ~OGRSimpleCurve();

    OGRSimpleCurve& operator=( const OGRSimpleCurve& other );

    // IWks Interface.
    virtual int WkbSize() const CPL_OVERRIDE;
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char ** ppszDstText,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IGeometry interface.
    virtual OGRGeometry *clone() const CPL_OVERRIDE;
    virtual void empty() CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const CPL_OVERRIDE;
    virtual OGRBoolean  IsEmpty() const CPL_OVERRIDE;

    // ICurve methods.
    virtual double get_Length() const CPL_OVERRIDE;
    virtual void StartPoint( OGRPoint * ) const CPL_OVERRIDE;
    virtual void EndPoint( OGRPoint * ) const CPL_OVERRIDE;
    virtual void Value( double, OGRPoint * ) const CPL_OVERRIDE;
    virtual double Project( const OGRPoint * ) const;
    virtual OGRLineString* getSubLine( double, double, int ) const;

    // ILineString methods.
    virtual int getNumPoints() const CPL_OVERRIDE { return nPointCount; }
    void        getPoint( int, OGRPoint * ) const;
    double      getX( int i ) const { return paoPoints[i].x; }
    double      getY( int i ) const { return paoPoints[i].y; }
    double      getZ( int i ) const;
    double      getM( int i ) const;

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const CPL_OVERRIDE;

    // non standard.
    virtual void setCoordinateDimension( int nDimension ) CPL_OVERRIDE;
    virtual void set3D( OGRBoolean bIs3D ) CPL_OVERRIDE;
    virtual void setMeasured( OGRBoolean bIsMeasured ) CPL_OVERRIDE;
    void        setNumPoints( int nNewPointCount,
                              int bZeroizeNewContent = TRUE );
    void        setPoint( int, OGRPoint * );
    void        setPoint( int, double, double );
    void        setZ( int, double );
    void        setM( int, double );
    void        setPoint( int, double, double, double );
    void        setPointM( int, double, double, double );
    void        setPoint( int, double, double, double, double );
    void        setPoints( int, OGRRawPoint *, double * = NULL );
    void        setPointsM( int, OGRRawPoint *, double * );
    void        setPoints( int, OGRRawPoint *, double *, double * );
    void        setPoints( int, double * padfX, double * padfY,
                           double *padfZIn = NULL );
    void        setPointsM( int, double * padfX, double * padfY,
                            double *padfMIn = NULL );
    void        setPoints( int, double * padfX, double * padfY,
                           double *padfZIn, double *padfMIn );
    void        addPoint( const OGRPoint * );
    void        addPoint( double, double );
    void        addPoint( double, double, double );
    void        addPointM( double, double, double );
    void        addPoint( double, double, double, double );

    void        getPoints( OGRRawPoint *, double * = NULL ) const;
    void        getPoints( void* pabyX, int nXStride,
                           void* pabyY, int nYStride,
                           void* pabyZ = NULL, int nZStride = 0 ) const;
    void        getPoints( void* pabyX, int nXStride,
                           void* pabyY, int nYStride,
                           void* pabyZ, int nZStride,
                           void* pabyM, int nMStride ) const;

    void        addSubLineString( const OGRLineString *,
                                  int nStartVertex = 0, int nEndVertex = -1 );
    void        reversePoints( void );
    virtual OGRPointIterator* getPointIterator() const CPL_OVERRIDE;

    // non-standard from OGRGeometry
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) CPL_OVERRIDE;
    virtual void flattenTo2D() CPL_OVERRIDE;
    virtual void segmentize(double dfMaxLength) CPL_OVERRIDE;

    virtual void        swapXY() CPL_OVERRIDE;
};

/************************************************************************/
/*                            OGRLineString                             */
/************************************************************************/

/**
 * Concrete representation of a multi-vertex line.
 *
 * Note: for implementation convenience, we make it inherit from OGRSimpleCurve
 * whereas SFSQL and SQL/MM only make it inherits from OGRCurve.
 */

class CPL_DLL OGRLineString : public OGRSimpleCurve
{
    static OGRLinearRing*          CasterToLinearRing(OGRCurve* poCurve);

  protected:
//! @cond Doxygen_Suppress
    static OGRLineString* TransferMembersAndDestroy(
                                            OGRLineString* poSrc,
                                            OGRLineString* poDst);

    virtual OGRCurveCasterToLineString GetCasterToLineString()
        const CPL_OVERRIDE;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing()
        const CPL_OVERRIDE;

    virtual double get_AreaOfCurveSegments() const CPL_OVERRIDE;
//! @endcond

    static OGRLinearRing* CastToLinearRing( OGRLineString* poLS );

  public:
                OGRLineString();
                OGRLineString( const OGRLineString& other );
    virtual    ~OGRLineString();

    OGRLineString& operator=(const OGRLineString& other);

    virtual OGRLineString* CurveToLine( double dfMaxAngleStepSizeDegrees = 0,
                                        const char* const* papszOptions = NULL )
        const CPL_OVERRIDE;
    virtual OGRGeometry* getCurveGeometry(
        const char* const* papszOptions = NULL ) const CPL_OVERRIDE;
    virtual double get_Area() const CPL_OVERRIDE;

    // Non-standard from OGRGeometry.
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual const char *getGeometryName() const CPL_OVERRIDE;
};

/************************************************************************/
/*                            OGRLinearRing                             */
/************************************************************************/

/**
 * Concrete representation of a closed ring.
 *
 * This class is functionally equivalent to an OGRLineString, but has a
 * separate identity to maintain alignment with the OpenGIS simple feature
 * data model.  It exists to serve as a component of an OGRPolygon.
 *
 * The OGRLinearRing has no corresponding free standing well known binary
 * representation, so importFromWkb() and exportToWkb() will not actually
 * work.  There is a non-standard GDAL WKT representation though.
 *
 * Because OGRLinearRing is not a "proper" free standing simple features
 * object, it cannot be directly used on a feature via SetGeometry(), and
 * cannot generally be used with GEOS for operations like Intersects().
 * Instead the polygon should be used, or the OGRLinearRing should be
 * converted to an OGRLineString for such operations.
 *
 * Note: this class exists in SFSQL 1.2, but not in ISO SQL/MM Part 3.
 */

class CPL_DLL OGRLinearRing : public OGRLineString
{
    static OGRLineString*       CasterToLineString( OGRCurve* poCurve );

  protected:
//! @cond Doxygen_Suppress
    friend class OGRPolygon;
    friend class OGRTriangle;

    // These are not IWks compatible ... just a convenience for OGRPolygon.
    virtual int _WkbSize( int _flags ) const;
    virtual OGRErr _importFromWkb( OGRwkbByteOrder, int _flags,
                                   const unsigned char *, int,
                                   int& nBytesConsumedOut );
    virtual OGRErr _exportToWkb( OGRwkbByteOrder, int _flags,
                                 unsigned char * ) const;

    virtual OGRCurveCasterToLineString GetCasterToLineString()
        const CPL_OVERRIDE;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing()
        const CPL_OVERRIDE;
//! @endcond

    static OGRLineString* CastToLineString( OGRLinearRing* poLR );

  public:
                        OGRLinearRing();
                        OGRLinearRing( const OGRLinearRing& other );
               explicit OGRLinearRing( OGRLinearRing * );
    virtual            ~OGRLinearRing();

    OGRLinearRing& operator=( const OGRLinearRing& other );

    // Non standard.
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRGeometry *clone() const CPL_OVERRIDE;
    virtual int isClockwise() const;
    virtual void reverseWindingOrder();
    virtual void closeRings() CPL_OVERRIDE;
    OGRBoolean isPointInRing( const OGRPoint* pt,
                              int bTestEnvelope = TRUE ) const;
    OGRBoolean isPointOnRingBoundary( const OGRPoint* pt,
                                      int bTestEnvelope = TRUE ) const;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) CPL_OVERRIDE;

    // IWks Interface - Note this isn't really a first class object
    // for the purposes of WKB form.  These methods always fail since this
    // object can't be serialized on its own.
    virtual int WkbSize() const CPL_OVERRIDE;
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;
};

/************************************************************************/
/*                         OGRCircularString                            */
/************************************************************************/

/**
 * Concrete representation of a circular string, that is to say a curve made
 * of one or several arc circles.
 *
 * Note: for implementation convenience, we make it inherit from OGRSimpleCurve
 * whereas SQL/MM only makes it inherits from OGRCurve.
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
//! @cond Doxygen_Suppress
    virtual OGRCurveCasterToLineString GetCasterToLineString()
        const CPL_OVERRIDE;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing()
        const CPL_OVERRIDE;
    virtual int    ContainsPoint( const OGRPoint* p ) const CPL_OVERRIDE;
    virtual double get_AreaOfCurveSegments() const CPL_OVERRIDE;
//! @endcond

  public:
                OGRCircularString();
                OGRCircularString(const OGRCircularString& other);
    virtual    ~OGRCircularString();

    OGRCircularString& operator=(const OGRCircularString& other);

    // IWks Interface.
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char ** ppszDstText,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IGeometry interface.
    virtual OGRBoolean  IsValid() const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const CPL_OVERRIDE;

    // ICurve methods.
    virtual double get_Length() const CPL_OVERRIDE;
    virtual OGRLineString* CurveToLine( double dfMaxAngleStepSizeDegrees = 0,
                                        const char* const* papszOptions = NULL )
        const CPL_OVERRIDE;
    virtual void Value( double, OGRPoint * ) const CPL_OVERRIDE;
    virtual double get_Area() const CPL_OVERRIDE;

    // Non-standard from OGRGeometry.
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual void segmentize( double dfMaxLength ) CPL_OVERRIDE;
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;
    virtual OGRGeometry* getLinearGeometry(
        double dfMaxAngleStepSizeDegrees = 0,
        const char* const* papszOptions = NULL) const CPL_OVERRIDE;
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

//! @cond Doxygen_Suppress
class CPL_DLL OGRCurveCollection
{
  protected:
    friend class OGRCompoundCurve;
    friend class OGRCurvePolygon;
    friend class OGRPolygon;
    friend class OGRTriangle;

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
                                            const unsigned char * pabyData,
                                            int& nSize,
                                            int& nDataOffset,
                                            OGRwkbByteOrder& eByteOrder,
                                            int nMinSubGeomSize,
                                            OGRwkbVariant eWkVariant );
    OGRErr      importBodyFromWkb(
                    OGRGeometry* poGeom,
                    const unsigned char * pabyData,
                    int nSize,
                    int bAcceptCompoundCurve,
                    OGRErr (*pfnAddCurveDirectlyFromWkb)( OGRGeometry* poGeom,
                                                          OGRCurve* poCurve ),
                    OGRwkbVariant eWkVariant,
                    int& nBytesConsumedOut );
    OGRErr          exportToWkt( const OGRGeometry* poGeom,
                                 char ** ppszDstText ) const;
    OGRErr          exportToWkb( const OGRGeometry* poGeom, OGRwkbByteOrder,
                                 unsigned char *,
                                 OGRwkbVariant eWkbVariant ) const;
    OGRBoolean      Equals(OGRCurveCollection *poOCC) const;
    void            setCoordinateDimension( OGRGeometry* poGeom,
                                            int nNewDimension );
    void            set3D( OGRGeometry* poGeom, OGRBoolean bIs3D );
    void            setMeasured( OGRGeometry* poGeom, OGRBoolean bIsMeasured );
    int             getNumCurves() const;
    OGRCurve       *getCurve( int );
    const OGRCurve *getCurve( int ) const;
    OGRCurve       *stealCurve( int );
    OGRErr          transform( OGRGeometry* poGeom,
                               OGRCoordinateTransformation *poCT );
    void            flattenTo2D( OGRGeometry* poGeom );
    void            segmentize( double dfMaxLength );
    void            swapXY();
    OGRBoolean      hasCurveGeometry(int bLookForNonLinear) const;
};
//! @endcond

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
    static OGRErr addCurveDirectlyFromWkt( OGRGeometry* poSelf,
                                           OGRCurve* poCurve );
    static OGRErr addCurveDirectlyFromWkb( OGRGeometry* poSelf,
                                           OGRCurve* poCurve );
    OGRLineString* CurveToLineInternal( double dfMaxAngleStepSizeDegrees,
                                        const char* const* papszOptions,
                                        int bIsLinearRing ) const;
    static OGRLineString* CasterToLineString( OGRCurve* poCurve );
    static OGRLinearRing* CasterToLinearRing( OGRCurve* poCurve );

  protected:
//! @cond Doxygen_Suppress
    static OGRLineString* CastToLineString( OGRCompoundCurve* poCC );
    static OGRLinearRing* CastToLinearRing( OGRCompoundCurve* poCC );

    virtual OGRCurveCasterToLineString GetCasterToLineString()
        const CPL_OVERRIDE;
    virtual OGRCurveCasterToLinearRing GetCasterToLinearRing()
        const CPL_OVERRIDE;
//! @endcond

  public:
                OGRCompoundCurve();
                OGRCompoundCurve( const OGRCompoundCurve& other );
    virtual     ~OGRCompoundCurve();

    OGRCompoundCurve& operator=( const OGRCompoundCurve& other );

    // IWks Interface
    virtual int WkbSize() const CPL_OVERRIDE;
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char ** ppszDstText,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IGeometry interface.
    virtual OGRGeometry *clone() const CPL_OVERRIDE;
    virtual void empty() CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const CPL_OVERRIDE;
    virtual OGRBoolean  IsEmpty() const CPL_OVERRIDE;

    // ICurve methods.
    virtual double get_Length() const CPL_OVERRIDE;
    virtual void StartPoint( OGRPoint * ) const CPL_OVERRIDE;
    virtual void EndPoint( OGRPoint * ) const CPL_OVERRIDE;
    virtual void Value( double, OGRPoint * ) const CPL_OVERRIDE;
    virtual OGRLineString* CurveToLine( double dfMaxAngleStepSizeDegrees = 0,
                                        const char* const* papszOptions = NULL )
        const CPL_OVERRIDE;

    virtual int getNumPoints() const CPL_OVERRIDE;
    virtual double get_AreaOfCurveSegments() const CPL_OVERRIDE;
    virtual double get_Area() const CPL_OVERRIDE;

    // ISpatialRelation.
    virtual OGRBoolean  Equals( OGRGeometry * ) const CPL_OVERRIDE;

    // ICompoundCurve method.
    int             getNumCurves() const;
    OGRCurve       *getCurve( int );
    const OGRCurve *getCurve( int ) const;

    // Non-standard.
    virtual void setCoordinateDimension( int nDimension ) CPL_OVERRIDE;
    virtual void set3D( OGRBoolean bIs3D ) CPL_OVERRIDE;
    virtual void setMeasured( OGRBoolean bIsMeasured ) CPL_OVERRIDE;

    OGRErr         addCurve( OGRCurve*, double dfToleranceEps = 1e-14  );
    OGRErr         addCurveDirectly( OGRCurve*, double dfToleranceEps = 1e-14 );
    OGRCurve      *stealCurve( int );
    virtual OGRPointIterator* getPointIterator() const CPL_OVERRIDE;

    // Non-standard from OGRGeometry.
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) CPL_OVERRIDE;
    virtual void flattenTo2D() CPL_OVERRIDE;
    virtual void segmentize(double dfMaxLength) CPL_OVERRIDE;
    virtual OGRBoolean hasCurveGeometry(int bLookForNonLinear = FALSE)
        const CPL_OVERRIDE;
    virtual OGRGeometry* getLinearGeometry(
        double dfMaxAngleStepSizeDegrees = 0,
        const char* const* papszOptions = NULL) const CPL_OVERRIDE;

    virtual void        swapXY() CPL_OVERRIDE;
};

/************************************************************************/
/*                              OGRSurface                              */
/************************************************************************/

/**
 * Abstract base class for 2 dimensional objects like polygons or curve
 * polygons.
 */

class CPL_DLL OGRSurface : public OGRGeometry
{
  protected:
//! @cond Doxygen_Suppress
    virtual OGRSurfaceCasterToPolygon      GetCasterToPolygon() const = 0;
    virtual OGRSurfaceCasterToCurvePolygon GetCasterToCurvePolygon() const = 0;
//! @endcond

  public:
    virtual double      get_Area() const = 0;
    virtual OGRErr      PointOnSurface( OGRPoint * poPoint ) const = 0;
//! @cond Doxygen_Suppress
    static OGRPolygon*      CastToPolygon(OGRSurface* poSurface);
    static OGRCurvePolygon* CastToCurvePolygon(OGRSurface* poSurface);
//! @endcond
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
    static OGRPolygon*      CasterToPolygon(OGRSurface* poSurface);

  private:
    OGRBoolean      ContainsPoint( const OGRPoint* p ) const;
    virtual int   checkRing( OGRCurve * poNewRing ) const;
    OGRErr        addRingDirectlyInternal( OGRCurve* poCurve,
                                           int bNeedRealloc );
    static OGRErr addCurveDirectlyFromWkt( OGRGeometry* poSelf,
                                           OGRCurve* poCurve );
    static OGRErr addCurveDirectlyFromWkb( OGRGeometry* poSelf,
                                           OGRCurve* poCurve );

  protected:
//! @cond Doxygen_Suppress
    friend class OGRPolygon;
    friend class OGRTriangle;
    OGRCurveCollection oCC;

    virtual OGRSurfaceCasterToPolygon      GetCasterToPolygon()
        const CPL_OVERRIDE;
    virtual OGRSurfaceCasterToCurvePolygon GetCasterToCurvePolygon()
        const CPL_OVERRIDE;
//! @endcond

    static OGRPolygon* CastToPolygon( OGRCurvePolygon* poCP );

  public:
                OGRCurvePolygon();
                OGRCurvePolygon( const OGRCurvePolygon& );
    virtual    ~OGRCurvePolygon();

    OGRCurvePolygon& operator=( const OGRCurvePolygon& other );

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual OGRGeometry *clone() const CPL_OVERRIDE;
    virtual void empty() CPL_OVERRIDE;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) CPL_OVERRIDE;
    virtual void flattenTo2D() CPL_OVERRIDE;
    virtual OGRBoolean  IsEmpty() const CPL_OVERRIDE;
    virtual void segmentize( double dfMaxLength ) CPL_OVERRIDE;
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;
    virtual OGRGeometry* getLinearGeometry(
        double dfMaxAngleStepSizeDegrees = 0,
        const char* const* papszOptions = NULL ) const CPL_OVERRIDE;

    // ISurface Interface
    virtual double      get_Area() const CPL_OVERRIDE;
    virtual OGRErr      PointOnSurface( OGRPoint * poPoint ) const CPL_OVERRIDE;

    // IWks Interface
    virtual int WkbSize() const CPL_OVERRIDE;
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char ** ppszDstText,
                                OGRwkbVariant eWkbVariant = wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IGeometry
    virtual int getDimension() const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const CPL_OVERRIDE;

    // ICurvePolygon
    virtual OGRPolygon* CurvePolyToPoly(
        double dfMaxAngleStepSizeDegrees = 0,
        const char* const* papszOptions = NULL ) const;

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const CPL_OVERRIDE;
    virtual OGRBoolean  Intersects( const OGRGeometry * ) const CPL_OVERRIDE;
    virtual OGRBoolean  Contains( const OGRGeometry * ) const CPL_OVERRIDE;

    // Non standard
    virtual void setCoordinateDimension( int nDimension ) CPL_OVERRIDE;
    virtual void set3D( OGRBoolean bIs3D ) CPL_OVERRIDE;
    virtual void setMeasured( OGRBoolean bIsMeasured ) CPL_OVERRIDE;

    virtual OGRErr addRing( OGRCurve * );
    virtual OGRErr addRingDirectly( OGRCurve * );

    OGRCurve *getExteriorRingCurve();
    const OGRCurve *getExteriorRingCurve() const;
    int         getNumInteriorRings() const;
    OGRCurve *getInteriorRingCurve( int );
    const OGRCurve *getInteriorRingCurve( int ) const;

    OGRCurve *stealExteriorRingCurve();

    virtual void        swapXY() CPL_OVERRIDE;
};

/************************************************************************/
/*                              OGRPolygon                              */
/************************************************************************/

/**
 * Concrete class representing polygons.
 *
 * Note that the OpenGIS simple features polygons consist of one outer ring
 * (linearring), and zero or more inner rings.  A polygon cannot represent
 * disconnected regions (such as multiple islands in a political body).  The
 * OGRMultiPolygon must be used for this.
 */

class CPL_DLL OGRPolygon : public OGRCurvePolygon
{
    static OGRCurvePolygon*     CasterToCurvePolygon(OGRSurface* poSurface);

  protected:
//! @cond Doxygen_Suppress
    friend class OGRMultiSurface;
    friend class OGRPolyhedralSurface;
    friend class OGRTriangulatedSurface;

    virtual int checkRing( OGRCurve * poNewRing ) const CPL_OVERRIDE;
    virtual OGRErr importFromWKTListOnly( char ** ppszInput,
                                          int bHasZ, int bHasM,
                                          OGRRawPoint*& paoPoints,
                                          int& nMaxPoints,
                                          double*& padfZ );

    static OGRCurvePolygon* CastToCurvePolygon(OGRPolygon* poPoly);

    virtual OGRSurfaceCasterToPolygon      GetCasterToPolygon()
        const CPL_OVERRIDE;
    virtual OGRSurfaceCasterToCurvePolygon GetCasterToCurvePolygon()
        const CPL_OVERRIDE;
//! @endcond

  public:
                OGRPolygon();
                OGRPolygon(const OGRPolygon& other);
    virtual    ~OGRPolygon();

    OGRPolygon& operator=(const OGRPolygon& other);

    // Non-standard (OGRGeometry).
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;
    virtual OGRGeometry* getCurveGeometry(
    const char* const* papszOptions = NULL ) const CPL_OVERRIDE;
    virtual OGRGeometry* getLinearGeometry(
        double dfMaxAngleStepSizeDegrees = 0,
        const char* const* papszOptions = NULL) const CPL_OVERRIDE;

    // ISurface Interface.
    virtual OGRErr        PointOnSurface( OGRPoint * poPoint )
        const CPL_OVERRIDE;

    // IWks Interface.
    virtual int WkbSize() const CPL_OVERRIDE;
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char ** ppszDstText,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // ICurvePolygon.
    virtual OGRPolygon* CurvePolyToPoly(
        double dfMaxAngleStepSizeDegrees = 0,
        const char* const* papszOptions = NULL ) const CPL_OVERRIDE;

    OGRLinearRing *getExteriorRing();
    const OGRLinearRing *getExteriorRing() const;
    virtual OGRLinearRing *getInteriorRing( int );
    virtual const OGRLinearRing *getInteriorRing( int ) const;

    OGRLinearRing *stealExteriorRing();
    virtual OGRLinearRing *stealInteriorRing(int);

    OGRBoolean IsPointOnSurface( const OGRPoint * ) const;

    virtual void closeRings() CPL_OVERRIDE;
};

/************************************************************************/
/*                              OGRTriangle                             */
/************************************************************************/

/**
 * Triangle class.
 *
 * @since GDAL 2.2
 */

class CPL_DLL OGRTriangle : public OGRPolygon
{
  private:
    static OGRPolygon*          CasterToPolygon(OGRSurface* poSurface);
    bool quickValidityCheck() const;

  protected:
//! @cond Doxygen_Suppress
    virtual OGRSurfaceCasterToPolygon   GetCasterToPolygon() const CPL_OVERRIDE;
    virtual OGRErr importFromWKTListOnly( char ** ppszInput,
                                          int bHasZ, int bHasM,
                                          OGRRawPoint*& paoPoints,
                                          int& nMaxPoints,
                                          double*& padfZ ) CPL_OVERRIDE;
//! @endcond

  public:
    OGRTriangle();
    OGRTriangle( const OGRPoint &p, const OGRPoint &q, const OGRPoint &r );
    OGRTriangle( const OGRTriangle &other );
    OGRTriangle( const OGRPolygon &other, OGRErr &eErr );
    OGRTriangle& operator=( const OGRTriangle& other );
    virtual ~OGRTriangle();
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;

    // IWks Interface.
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;

    // New methods rewritten from OGRPolygon/OGRCurvePolygon/OGRGeometry.
    virtual OGRErr addRingDirectly( OGRCurve * poNewRing ) CPL_OVERRIDE;

//! @cond Doxygen_Suppress
    static OGRGeometry* CastToPolygon( OGRGeometry* poGeom );
//! @endcond
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
    OGRErr      importFromWkbInternal( const unsigned char * pabyData,
                                       int nSize,
                                       int nRecLevel,
                                       OGRwkbVariant, int& nBytesConsumedOut );
    OGRErr      importFromWktInternal( char **ppszInput, int nRecLevel );

  protected:
//! @cond Doxygen_Suppress
    int         nGeomCount;
    OGRGeometry **papoGeoms;

    OGRErr              exportToWktInternal( char ** ppszDstText,
                                             OGRwkbVariant eWkbVariant,
                                             const char* pszSkipPrefix ) const;
    static OGRGeometryCollection* TransferMembersAndDestroy(
        OGRGeometryCollection* poSrc,
        OGRGeometryCollection* poDst );
//! @endcond
    virtual OGRBoolean         isCompatibleSubType( OGRwkbGeometryType ) const;

  public:
                OGRGeometryCollection();
                OGRGeometryCollection( const OGRGeometryCollection& other );
    virtual     ~OGRGeometryCollection();

    OGRGeometryCollection& operator=( const OGRGeometryCollection& other );

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual OGRGeometry *clone() const CPL_OVERRIDE;
    virtual void empty() CPL_OVERRIDE;
    virtual OGRErr  transform( OGRCoordinateTransformation *poCT ) CPL_OVERRIDE;
    virtual void flattenTo2D() CPL_OVERRIDE;
    virtual OGRBoolean  IsEmpty() const CPL_OVERRIDE;
    virtual void segmentize(double dfMaxLength) CPL_OVERRIDE;
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;
    virtual OGRGeometry* getCurveGeometry(
        const char* const* papszOptions = NULL ) const CPL_OVERRIDE;
    virtual OGRGeometry* getLinearGeometry(
        double dfMaxAngleStepSizeDegrees = 0,
        const char* const* papszOptions = NULL ) const CPL_OVERRIDE;

    // IWks Interface
    virtual int WkbSize() const CPL_OVERRIDE;
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char ** ppszDstText,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    virtual double get_Length() const;
    virtual double get_Area() const;

    // IGeometry methods
    virtual int getDimension() const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const CPL_OVERRIDE;

    // IGeometryCollection
    int         getNumGeometries() const;
    OGRGeometry *getGeometryRef( int );
    const OGRGeometry *getGeometryRef( int ) const;

    // ISpatialRelation
    virtual OGRBoolean  Equals( OGRGeometry * ) const CPL_OVERRIDE;

    // Non standard
    virtual void setCoordinateDimension( int nDimension ) CPL_OVERRIDE;
    virtual void set3D( OGRBoolean bIs3D ) CPL_OVERRIDE;
    virtual void setMeasured( OGRBoolean bIsMeasured ) CPL_OVERRIDE;
    virtual OGRErr addGeometry( const OGRGeometry * );
    virtual OGRErr addGeometryDirectly( OGRGeometry * );
    virtual OGRErr removeGeometry( int iIndex, int bDelete = TRUE );

    void closeRings() CPL_OVERRIDE;

    virtual void swapXY() CPL_OVERRIDE;

    static OGRGeometryCollection* CastToGeometryCollection(
        OGRGeometryCollection* poSrc );
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
    virtual OGRBoolean isCompatibleSubType( OGRwkbGeometryType )
        const CPL_OVERRIDE;

  public:
            OGRMultiSurface();
            OGRMultiSurface( const OGRMultiSurface& other );
    virtual ~OGRMultiSurface();

    OGRMultiSurface& operator=( const OGRMultiSurface& other );

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IMultiSurface methods
    virtual OGRErr PointOnSurface( OGRPoint * poPoint ) const;

    // IGeometry methods
    virtual int getDimension() const CPL_OVERRIDE;

    // Non standard
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;

    static OGRMultiPolygon* CastToMultiPolygon( OGRMultiSurface* poMS );
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
    virtual OGRBoolean isCompatibleSubType( OGRwkbGeometryType )
        const CPL_OVERRIDE;
    friend class OGRPolyhedralSurface;
    friend class OGRTriangulatedSurface;

  private:
//! @cond Doxygen_Suppress
            OGRErr _addGeometryWithExpectedSubGeometryType(
                const OGRGeometry * poNewGeom,
                OGRwkbGeometryType eSubGeometryType );
            OGRErr _addGeometryDirectlyWithExpectedSubGeometryType(
                OGRGeometry * poNewGeom,
                OGRwkbGeometryType eSubGeometryType );
//! @endcond


  public:
            OGRMultiPolygon();
            OGRMultiPolygon(const OGRMultiPolygon& other);
    virtual ~OGRMultiPolygon();

    OGRMultiPolygon& operator=(const OGRMultiPolygon& other);

    // Non-standard (OGRGeometry).
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IMultiSurface methods
    virtual OGRErr      PointOnSurface( OGRPoint * poPoint ) const CPL_OVERRIDE;

    // Non standard
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;

    static OGRMultiSurface* CastToMultiSurface( OGRMultiPolygon* poMP );
};

/************************************************************************/
/*                         OGRPolyhedralSurface                         */
/************************************************************************/

/**
 * PolyhedralSurface class.
 *
 * @since GDAL 2.2
 */

class CPL_DLL OGRPolyhedralSurface : public OGRSurface
{
  protected:
//! @cond Doxygen_Suppress
    friend class OGRTriangulatedSurface;
    OGRMultiPolygon oMP;
    virtual OGRSurfaceCasterToPolygon      GetCasterToPolygon()
        const CPL_OVERRIDE;
    virtual OGRSurfaceCasterToCurvePolygon GetCasterToCurvePolygon()
      const CPL_OVERRIDE;
    virtual OGRBoolean         isCompatibleSubType( OGRwkbGeometryType ) const;
    virtual const char*        getSubGeometryName() const;
    virtual OGRwkbGeometryType getSubGeometryType() const;
    OGRErr exportToWktInternal (char ** ppszDstText, OGRwkbVariant eWkbVariant,
                                const char* pszSkipPrefix ) const;

    virtual OGRPolyhedralSurfaceCastToMultiPolygon GetCasterToMultiPolygon()
        const;
    static OGRMultiPolygon* CastToMultiPolygonImpl(OGRPolyhedralSurface* poPS);
//! @endcond

  public:
    OGRPolyhedralSurface();
    OGRPolyhedralSurface(const OGRPolyhedralSurface &poGeom);
    virtual ~OGRPolyhedralSurface();
    OGRPolyhedralSurface& operator=(const OGRPolyhedralSurface& other);

    // IWks Interface.
    virtual int WkbSize() const CPL_OVERRIDE;
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const  CPL_OVERRIDE;
    virtual OGRErr importFromWkb( const unsigned char *,
                                  int,
                                  OGRwkbVariant,
                                  int& nBytesConsumedOut ) CPL_OVERRIDE;
    virtual OGRErr exportToWkb( OGRwkbByteOrder, unsigned char *,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char ** ppszDstText,
                                OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IGeometry methods.
    virtual int getDimension() const CPL_OVERRIDE;

    virtual void empty() CPL_OVERRIDE;

    virtual OGRGeometry *clone() const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope * psEnvelope ) const CPL_OVERRIDE;
    virtual void getEnvelope( OGREnvelope3D * psEnvelope ) const CPL_OVERRIDE;

    virtual void flattenTo2D() CPL_OVERRIDE;
    virtual OGRErr transform( OGRCoordinateTransformation* ) CPL_OVERRIDE;
    virtual OGRBoolean Equals( OGRGeometry* ) const CPL_OVERRIDE;
    virtual double get_Area() const CPL_OVERRIDE;
    virtual OGRErr PointOnSurface( OGRPoint* ) const CPL_OVERRIDE;

    static OGRMultiPolygon* CastToMultiPolygon( OGRPolyhedralSurface* poPS );
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;
    virtual OGRErr addGeometry( const OGRGeometry * );
    OGRErr addGeometryDirectly( OGRGeometry *poNewGeom );
    int getNumGeometries() const;
    OGRGeometry* getGeometryRef(int i);
    const OGRGeometry* getGeometryRef(int i) const;

    virtual OGRBoolean  IsEmpty() const CPL_OVERRIDE;
    virtual void setCoordinateDimension( int nDimension ) CPL_OVERRIDE;
    virtual void set3D( OGRBoolean bIs3D ) CPL_OVERRIDE;
    virtual void setMeasured( OGRBoolean bIsMeasured ) CPL_OVERRIDE;
    virtual void swapXY() CPL_OVERRIDE;
    OGRErr removeGeometry( int iIndex, int bDelete = TRUE );
};

/************************************************************************/
/*                        OGRTriangulatedSurface                        */
/************************************************************************/

/**
 * TriangulatedSurface class.
 *
 * @since GDAL 2.2
 */

class CPL_DLL OGRTriangulatedSurface : public OGRPolyhedralSurface
{
  protected:
//! @cond Doxygen_Suppress
    virtual OGRBoolean         isCompatibleSubType( OGRwkbGeometryType )
        const CPL_OVERRIDE;
    virtual const char*        getSubGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getSubGeometryType() const CPL_OVERRIDE;

    virtual OGRPolyhedralSurfaceCastToMultiPolygon GetCasterToMultiPolygon()
        const CPL_OVERRIDE;
    static OGRMultiPolygon *
        CastToMultiPolygonImpl( OGRPolyhedralSurface* poPS );
//! @endcond

  public:
    OGRTriangulatedSurface();
    OGRTriangulatedSurface( const OGRTriangulatedSurface &other );
    ~OGRTriangulatedSurface();

    OGRTriangulatedSurface& operator=( const OGRTriangulatedSurface& other );
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;

    // IWks Interface.
    virtual OGRErr addGeometry( const OGRGeometry * ) CPL_OVERRIDE;

    static OGRPolyhedralSurface *
        CastToPolyhedralSurface( OGRTriangulatedSurface* poTS );
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
    virtual OGRBoolean  isCompatibleSubType( OGRwkbGeometryType )
        const CPL_OVERRIDE;

  public:
            OGRMultiPoint();
            OGRMultiPoint(const OGRMultiPoint& other);
    virtual ~OGRMultiPoint();

    OGRMultiPoint& operator=(const OGRMultiPoint& other);

    // Non-standard (OGRGeometry).
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IGeometry methods.
    virtual int getDimension() const CPL_OVERRIDE;

    // Non-standard.
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;
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
//! @cond Doxygen_Suppress
    static OGRErr addCurveDirectlyFromWkt( OGRGeometry* poSelf,
                                           OGRCurve* poCurve );
//! @endcond
    virtual OGRBoolean isCompatibleSubType( OGRwkbGeometryType )
        const CPL_OVERRIDE;

  public:
            OGRMultiCurve();
            OGRMultiCurve( const OGRMultiCurve& other );
    virtual ~OGRMultiCurve();

    OGRMultiCurve& operator=( const OGRMultiCurve& other );

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual OGRErr importFromWkt( char ** ) CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // IGeometry methods.
    virtual int getDimension() const CPL_OVERRIDE;

    // Non-standard.
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;

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
    virtual OGRBoolean  isCompatibleSubType( OGRwkbGeometryType )
        const CPL_OVERRIDE;

  public:
            OGRMultiLineString();
            OGRMultiLineString( const OGRMultiLineString& other );
    virtual ~OGRMultiLineString();

    OGRMultiLineString& operator=( const OGRMultiLineString& other );

    // Non standard (OGRGeometry).
    virtual const char *getGeometryName() const CPL_OVERRIDE;
    virtual OGRwkbGeometryType getGeometryType() const CPL_OVERRIDE;
    virtual OGRErr exportToWkt( char **, OGRwkbVariant=wkbVariantOldOgc )
        const CPL_OVERRIDE;

    // Non standard
    virtual OGRBoolean hasCurveGeometry( int bLookForNonLinear = FALSE )
        const CPL_OVERRIDE;

    static OGRMultiCurve* CastToMultiCurve( OGRMultiLineString* poMLS );
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
                                 OGRGeometry **, int = -1,
                                 OGRwkbVariant=wkbVariantOldOgc );
    static OGRErr createFromWkb( const unsigned char * pabyData,
                                 OGRSpatialReference *,
                                 OGRGeometry **,
                                 int nSize,
                                 OGRwkbVariant eVariant,
                                 int& nBytesConsumedOut );

    static OGRErr createFromWkt( char **, OGRSpatialReference *,
                                 OGRGeometry ** );
    static OGRErr createFromFgf( unsigned char *, OGRSpatialReference *,
                                 OGRGeometry **, int = -1, int * = NULL );
    static OGRGeometry *createFromGML( const char * );
    static OGRGeometry *createFromGEOS( GEOSContextHandle_t hGEOSCtxt,
                                        GEOSGeom );

    static void   destroyGeometry( OGRGeometry * );
    static OGRGeometry *createGeometry( OGRwkbGeometryType );

    static OGRGeometry * forceToPolygon( OGRGeometry * );
    static OGRGeometry * forceToLineString( OGRGeometry *,
                                            bool bOnlyInOrder = true );
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
    static bool haveGEOS();

    static OGRGeometry* transformWithOptions( const OGRGeometry* poSrcGeom,
                                              OGRCoordinateTransformation *poCT,
                                              char** papszOptions );

    static OGRGeometry*
        approximateArcAngles( double dfX, double dfY, double dfZ,
                              double dfPrimaryRadius, double dfSecondaryAxis,
                              double dfRotation,
                              double dfStartAngle, double dfEndAngle,
                              double dfMaxAngleStepSizeDegrees );

    static int GetCurveParmeters( double x0, double y0,
                                  double x1, double y1,
                                  double x2, double y2,
                                  double& R, double& cx, double& cy,
                                  double& alpha0, double& alpha1,
                                  double& alpha2 );
    static OGRLineString* curveToLineString(
        double x0, double y0, double z0,
        double x1, double y1, double z1,
        double x2, double y2, double z2,
        int bHasZ,
        double dfMaxAngleStepSizeDegrees,
        const char* const * papszOptions = NULL );
    static OGRCurve* curveFromLineString(
        const OGRLineString* poLS,
        const char* const * papszOptions = NULL);
};

OGRwkbGeometryType CPL_DLL OGRFromOGCGeomType( const char *pszGeomType );
const char CPL_DLL * OGRToOGCGeomType( OGRwkbGeometryType eGeomType );

/** Prepared geometry API (needs GEOS >= 3.1.0) */
typedef struct _OGRPreparedGeometry OGRPreparedGeometry;
int OGRHasPreparedGeometrySupport();
OGRPreparedGeometry* OGRCreatePreparedGeometry( const OGRGeometry* poGeom );
void OGRDestroyPreparedGeometry( OGRPreparedGeometry* poPreparedGeom );
int OGRPreparedGeometryIntersects( const OGRPreparedGeometry* poPreparedGeom,
                                   const OGRGeometry* poOtherGeom );
int OGRPreparedGeometryContains( const OGRPreparedGeometry* poPreparedGeom,
                                 const OGRGeometry* poOtherGeom );

#endif /* ndef OGR_GEOMETRY_H_INCLUDED */
