/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definition of SFCOM Geometry implementation classes.
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
 * Revision 1.6  1999/05/20 19:46:15  warmerda
 * add some automatation, and Wkt support
 *
 * Revision 1.5  1999/05/20 14:54:55  warmerda
 * started work on automation
 *
 * Revision 1.4  1999/05/17 14:43:10  warmerda
 * Added Polygon, linestring and curve support.  Changed IGeometryTmpl to
 * also include COM interface class as an argument.
 *
 * Revision 1.3  1999/05/14 14:08:39  warmerda
 * OGRComGeometry converted to template OGRComGeometryTmpl
 *
 * Revision 1.2  1999/05/14 13:28:38  warmerda
 * client and service now working for IPoint
 *
 * Revision 1.1  1999/05/13 19:49:01  warmerda
 * New
 *
 */

#ifndef _OGRCOMGEOMETRY_H_INCLUDED
#define _OGRCOMGEOMETRY_H_INCLUDED

// this is really just for the various com/windows include files. 
#include "oledb_sup.h"

#include "geometryidl.h"
#include "ogr_geometry.h"
#include "ocg_public.h"

void OGRComDebug( const char *, const char *, ... );

/************************************************************************/
/*                          OGRComClassFactory                          */
/*                                                                      */
/*      This class is responsible for creating directly instantiable    */
/*      classes such as the IGeometryFactory.                           */
/************************************************************************/

class OGRComClassFactory : public IClassFactory
{
public:
   // Constructor
                        OGRComClassFactory();
                        
   // IUnknown
   STDMETHODIMP         QueryInterface(REFIID, void **);
   STDMETHODIMP_(ULONG) AddRef();
   STDMETHODIMP_(ULONG) Release();

   //IClassFactory members
   STDMETHODIMP         CreateInstance(LPUNKNOWN, REFIID, void **);
   STDMETHODIMP         LockServer(BOOL);

protected:
   // Reference count for class factory
   ULONG m_cRef;
};

/************************************************************************/
/*                   OGRComGeometryFactoryDispatcher                    */
/************************************************************************/

class OGRComGeometryFactory;

class OGRComGeometryFactoryDispatcher : public IDispatch
{
    OGRComGeometryFactory *poOwner;
  public:
                         OGRComGeometryFactoryDispatcher();
    void                 SetOwner( OGRComGeometryFactory * );
    
    // IUnknown
    STDMETHODIMP         QueryInterface(REFIID, void**);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IDispatch
    STDMETHOD(GetTypeInfoCount) (UINT *pctInfo);
    STDMETHOD(GetTypeInfo)(UINT itinfo, LCID lcid, ITypeInfo ** ppt);
    STDMETHOD(GetIDsOfNames)(REFIID, OLECHAR **, UINT, LCID, DISPID *);
    STDMETHOD(Invoke)(DISPID, REFIID, LCID, unsigned short, DISPPARAMS*,
                      VARIANT *, EXCEPINFO *, UINT * );
};

/************************************************************************/
/*                        OGRComGeometryFactory                         */
/************************************************************************/

class OGRComGeometryFactory : public IGeometryFactory
{
  public:
                         OGRComGeometryFactory();
                         
    // IUnknown
    STDMETHODIMP         QueryInterface(REFIID, void**);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IGeometryFactory
    STDMETHOD(CreateFromWKB)( VARIANT wkb, ISpatialReference *spatialRef,
                              IGeometry **geometry);
    STDMETHOD(CreateFromWKT)( BSTR wkt, ISpatialReference *spatialRef,
                              IGeometry **geometry);

    // local helper method.
    static IGeometry      *COMifyGeometry( OGRGeometry * );

  protected:
    ULONG      m_cRef;
    OGRComGeometryFactoryDispatcher oDispatcher;
};



/************************************************************************/
/*                          OGRComGeometryTmpl                          */
/*                                                                      */
/*      Template class with generic geometry capabilities for           */
/*      different interfaces derived from IGeometry.                    */
/*                                                                      */
/*      Note that the argument class must be derived from               */
/*      OGRGeometry.                                                    */
/*                                                                      */
/*      The implementation definitions for this template are in         */
/*      ogrcomgeometrytmpl.h ... it should be included after            */
/*      ogrcomgeometry.h by any classes derived from                    */
/*      OGRComGeometryTmpl.                                             */
/************************************************************************/

template<class GC, class IC> class OGRComGeometryTmpl : public IC
{
  public:
                         OGRComGeometryTmpl( GC * );
                         
    // IUnknown
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IGeometry
    STDMETHOD(get_Dimension)( long *dimension);
    STDMETHOD(get_SpatialReference)(ISpatialReference ** spatialRef);
    STDMETHOD(putref_SpatialReference)(ISpatialReference *spatialRef);
    STDMETHOD(get_IsEmpty)(VARIANT_BOOL *isEmpty);
    STDMETHOD(SetEmpty)(void);
    STDMETHOD(get_IsSimple)(VARIANT_BOOL *isSimple);
    STDMETHOD(Envelope)(IGeometry ** envelope);
    STDMETHOD(Clone)(IGeometry **newShape);
    STDMETHOD(Project)(ISpatialReference *newSystem, IGeometry **result);
    STDMETHOD(Extent2D)(double *minX, double *minY,
                        double *maxX, double *maxY);

  protected:
    ULONG      m_cRef;

    GC         *poGeometry;
};

/************************************************************************/
/*                             OGRComPoint                              */
/************************************************************************/
class OGRComPoint : public OGRComGeometryTmpl<OGRPoint,IPoint>
{
  public:
                         OGRComPoint( OGRPoint * );

    // IUnknown
    STDMETHODIMP         QueryInterface(REFIID, void**);
    
    // IPoint
    STDMETHOD(Coords)( double *x, double *y );
    STDMETHOD(get_X)( double *x );
    STDMETHOD(get_Y)( double *y );
};

/************************************************************************/
/*                          OGRComLineString                            */
/*                                                                      */
/*      Note that we use the OGRComLineString for ICurve, and           */
/*      ILinearRing as well.                                            */
/************************************************************************/

class OGRComLineString : public OGRComGeometryTmpl<OGRLineString,
                                                     ILineString>
{
  public:
                         OGRComLineString( OGRLineString * );

    // IUnknown
    STDMETHODIMP         QueryInterface(REFIID, void**);

    // ICurve
    STDMETHOD(get_Length)(double *);
    STDMETHOD(StartPoint)(IPoint **);
    STDMETHOD(EndPoint)(IPoint **);
    STDMETHOD(get_IsClosed)(VARIANT_BOOL *);
    STDMETHOD(Value)(double, IPoint **);

    // ILineString
    STDMETHOD(get_NumPoints)(long * numPoints);
    STDMETHOD(Point)(long index, IPoint ** point );
};

/************************************************************************/
/*                            OGRComPolygon                             */
/************************************************************************/
class OGRComPolygon : public OGRComGeometryTmpl<OGRPolygon,IPolygon>
{
  public:
                         OGRComPolygon( OGRPolygon * );

    // IUnknown
    STDMETHODIMP         QueryInterface(REFIID, void**);
    
    // ISurface
    STDMETHOD(get_Area)(double * area );
    STDMETHOD(Centroid)(IPoint **result );
    STDMETHOD(PointOnSurface)(IPoint ** result );
    
    // IPolygon
    STDMETHOD(ExteriorRing)(ILinearRing **exteriorRing);
    STDMETHOD(get_NumInteriorRings)( long * count );
    STDMETHOD(InteriorRing)( long ring_index, ILinearRing **interiorRing );
};

#endif /* ndef _OGRCOMGEOMETRY_H_INCLUDED */


