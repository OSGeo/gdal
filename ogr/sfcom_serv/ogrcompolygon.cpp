/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRComPolygon class.
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
 * Revision 1.2  1999/05/21 02:39:50  warmerda
 * Added IWks support
 *
 * Revision 1.1  1999/05/17 14:40:39  warmerda
 * New
 *
 */

#include "ogrcomgeometry.h"
#include "ogrcomgeometrytmpl.h"

/************************************************************************/
/*                           OGRComPolygon()                            */
/************************************************************************/

OGRComPolygon::OGRComPolygon( OGRPolygon * poPolygonIn ) 
        : OGRComGeometryTmpl<OGRPolygon,IPolygon>( poPolygonIn )

{
}

// =======================================================================
// IUnknown methods
// =======================================================================

/************************************************************************/
/*                           QueryInterface()                           */
/************************************************************************/

STDMETHODIMP OGRComPolygon::QueryInterface(REFIID rIID,
                                         void** ppInterface)
{
   // Set the interface Polygoner
   if (rIID == IID_IUnknown) {
      *ppInterface = this;
   }

   else if (rIID == IID_IGeometry) {
      *ppInterface = this;
   }

   else if (rIID == IID_IPolygon) {
      *ppInterface = this;
   }

   else if (rIID == IID_ISurface) {
      *ppInterface = this;
   }

   else if (rIID == IID_IWks) {
      *ppInterface = &oWks;
   }

   // We don't support this interface
   else {
      *ppInterface = NULL;
      return E_NOINTERFACE;
   }

   // Bump up the reference count
   ((LPUNKNOWN) *ppInterface)->AddRef();

   return NOERROR;
}

// =======================================================================
// ISurface methods
// =======================================================================

/************************************************************************/
/*                              get_Area()                              */
/************************************************************************/

STDMETHODIMP OGRComPolygon::get_Area( double * area )

{
    *area = poGeometry->get_Area();

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                              Centroid()                              */
/************************************************************************/

STDMETHODIMP OGRComPolygon::Centroid( IPoint ** point )

{
    OGRPoint         *poOGRPoint;
    int              nSuccess;

    poOGRPoint = new OGRPoint();
    nSuccess = poGeometry->Centroid( poOGRPoint );

    *point = new OGRComPoint( poOGRPoint );

    if( nSuccess )
        return ResultFromScode( S_OK );
    else
        return E_FAIL;
}

/************************************************************************/
/*                           PointOnSurface()                           */
/************************************************************************/

STDMETHODIMP OGRComPolygon::PointOnSurface( IPoint ** point )

{
    OGRPoint         *poOGRPoint;
    int              nSuccess;

    poOGRPoint = new OGRPoint();
    nSuccess = poGeometry->PointOnSurface( poOGRPoint );

    *point = new OGRComPoint( poOGRPoint );

    if( nSuccess )
        return ResultFromScode( S_OK );
    else
        return E_FAIL;
}

// =======================================================================
// IPolygon methods
// =======================================================================

/************************************************************************/
/*                            ExteriorRing()                            */
/************************************************************************/

STDMETHODIMP OGRComPolygon::ExteriorRing( ILinearRing **exteriorRing )

{
    OGRLinearRing      *poLinearRing;

    poLinearRing = poGeometry->getExteriorRing();
    if( poLinearRing == NULL )
    {
        *exteriorRing = NULL;
        return E_FAIL;
    }

    *exteriorRing = (ILinearRing *) new OGRComLineString(poLinearRing);

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                        get_NumInteriorRings()                        */
/************************************************************************/

STDMETHODIMP OGRComPolygon::get_NumInteriorRings( long * count )

{
    *count = poGeometry->getNumInteriorRings();


    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                            InteriorRing()                            */
/************************************************************************/

STDMETHODIMP OGRComPolygon::InteriorRing( long ring_index,
                                          ILinearRing **ring )

{
    OGRLinearRing      *poLinearRing;

    poLinearRing = poGeometry->getInteriorRing( ring_index );
    if( poLinearRing == NULL )
    {
        *ring = NULL;
        return E_FAIL;
    }

    *ring = (ILinearRing *) new OGRComLineString(poLinearRing);

    return ResultFromScode( S_OK );
}
