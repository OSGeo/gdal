/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRComLineString class.
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
 * Revision 1.1  1999/05/17 14:40:50  warmerda
 * New
 *
 */

#include "ogrcomgeometry.h"
#include "ogrcomgeometrytmpl.h"

/************************************************************************/
/*                          OGRComLineString()                          */
/************************************************************************/

OGRComLineString::OGRComLineString( OGRLineString * poLineStringIn ) 
        : OGRComGeometryTmpl<OGRLineString,ILineString>( poLineStringIn )

{
}

// =======================================================================
// IUnknown methods
// =======================================================================

/************************************************************************/
/*                           QueryInterface()                           */
/************************************************************************/

STDMETHODIMP OGRComLineString::QueryInterface(REFIID rIID,
                                         void** ppInterface)
{
   // Set the interface LineStringer
   if (rIID == IID_IUnknown) {
      *ppInterface = this;
   }

   else if (rIID == IID_IGeometry) {
      *ppInterface = this;
   }

   else if (rIID == IID_ILineString) {
      *ppInterface = this;
   }

   else if (rIID == IID_ICurve) {
      *ppInterface = this;
   }

   else if (rIID == IID_ILinearRing) {
      *ppInterface = this;
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
// ICurve methods
// =======================================================================

/************************************************************************/
/*                             get_Length()                             */
/************************************************************************/

STDMETHODIMP OGRComLineString::get_Length( double * length )

{
    *length = poGeometry->get_Length();

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                             StartPoint()                             */
/*                                                                      */
/*      We use the LineString Point() method to minimize the            */
/*      number of places constructing OGRComPoint objects.              */
/************************************************************************/

STDMETHODIMP OGRComLineString::StartPoint( IPoint ** point )

{
    return( Point( 0, point ) );
}

/************************************************************************/
/*                              EndPoint()                              */
/*                                                                      */
/*      We use the LineString Point() method to minimize the            */
/*      number of places constructing OGRComPoint objects.              */
/************************************************************************/

STDMETHODIMP OGRComLineString::EndPoint( IPoint ** point )

{
    return( Point( poGeometry->getNumPoints()-1, point ) );
}

/************************************************************************/
/*                            get_IsClosed()                            */
/************************************************************************/

STDMETHODIMP OGRComLineString::get_IsClosed( VARIANT_BOOL * is_closed )

{
    VarBoolFromUI1( (BYTE) poGeometry->get_IsClosed(), is_closed );
    
    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                               Value()                                */
/************************************************************************/

STDMETHODIMP OGRComLineString::Value( double distance, IPoint ** point )

{
    OGRPoint      *poPoint;

    poPoint = new OGRPoint;

    poGeometry->Value( distance, poPoint );

    *point = new OGRComPoint( poPoint );
    
    return ResultFromScode( S_OK );
}

// =======================================================================
// ILineString methods
// =======================================================================

/************************************************************************/
/*                           get_NumPoints()                            */
/************************************************************************/

STDMETHODIMP OGRComLineString::get_NumPoints( long * num_points )

{
    *num_points = poGeometry->getNumPoints();

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                               Point()                                */
/************************************************************************/

STDMETHODIMP OGRComLineString::Point( long pt_index, IPoint ** point )

{
    OGRPoint      *poPoint;

    poPoint = new OGRPoint;

    poGeometry->getPoint( pt_index, poPoint );

    *point = new OGRComPoint( poPoint );
    
    return ResultFromScode( S_OK );
}

