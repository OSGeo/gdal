/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRComGeometryTmpl template class.
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
 * Revision 1.1  1999/05/14 14:08:04  warmerda
 * New
 *
 */

/************************************************************************/
/*                         OGRComGeometryTmpl()                         */
/************************************************************************/

template<class GC> 
OGRComGeometryTmpl<GC>::OGRComGeometryTmpl<GC>( GC * poGeometryIn )

{
    poGeometry = poGeometryIn;
    m_cRef = 0;
}

// =======================================================================
// IUnknown methods
// =======================================================================

/************************************************************************/
/*                               AddRef()                               */
/************************************************************************/

template<class GC> 
STDMETHODIMP_(ULONG) OGRComGeometryTmpl<GC>::AddRef()
{
   // Increment the reference count
   m_cRef++;

   return m_cRef;
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

template<class GC> STDMETHODIMP_(ULONG) 
OGRComGeometryTmpl<GC>::Release()
{
   // Decrement the reference count
   m_cRef--;

   // Is this the last reference to the object?
   if (m_cRef)
      return m_cRef;

   // Decrement the server object count
//   Counters::DecObjectCount();

   // self destruct 
   // Does this make sense in the case of an object that is just 
   // aggregated in other objects?
   delete this;

   return 0;
}

// =======================================================================
// IGeometry methods
// =======================================================================

/************************************************************************/
/*                           get_Dimension()                            */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::get_Dimension( long * dimension )

{
    *dimension = poGeometry->getDimension();

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                        get_SpatialReference()                        */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::get_SpatialReference( ISpatialReference ** sRef )

{
    *sRef = NULL;      // none available for now. 

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                      putref_SpatialReference()                       */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::putref_SpatialReference( ISpatialReference * sRef )

{
    // we should eventually do something. 

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                            get_IsEmpty()                             */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::get_IsEmpty( VARIANT_BOOL* isEmpty )

{
    VarBoolFromUI1( (BYTE) poGeometry->IsEmpty(), isEmpty );

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                              SetEmpty()                              */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::SetEmpty(void)

{
    // notdef ... how will we do this?

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                            get_IsSimple()                            */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::get_IsSimple( VARIANT_BOOL * isEmpty )

{
    VarBoolFromUI1( (BYTE) poGeometry->IsSimple(), isEmpty );

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                              Envelope()                              */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::Envelope( IGeometry ** envelope )

{
    return E_FAIL;
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::Clone( IGeometry ** newShape )

{
    // notdef
    *newShape = NULL;

    return E_FAIL;
}

/************************************************************************/
/*                              Project()                               */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::Project( ISpatialReference * newSystem,
                                 IGeometry ** result )

{
    // notdef
    *result = NULL;

    return E_FAIL;
}

/************************************************************************/
/*                              Extent2D()                              */
/************************************************************************/

template<class GC> STDMETHODIMP 
OGRComGeometryTmpl<GC>::Extent2D( double * minX, double *minY,
                                  double * maxX, double *maxY )

{
    return E_FAIL;
}

