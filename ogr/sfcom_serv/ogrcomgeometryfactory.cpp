/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRCOMGeometryFactory class.
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
 * Revision 1.3  1999/05/17 14:43:10  warmerda
 * Added Polygon, linestring and curve support.  Changed IGeometryTmpl to
 * also include COM interface class as an argument.
 *
 * Revision 1.2  1999/05/14 13:28:38  warmerda
 * client and service now working for IPoint
 *
 * Revision 1.1  1999/05/13 19:49:01  warmerda
 * New
 *
 */

#include "ogrcomgeometry.h"

/************************************************************************/
/*                       OGRComGeometryFactory()                        */
/************************************************************************/

OGRComGeometryFactory::OGRComGeometryFactory() 

{
    m_cRef = 0;
}

// =======================================================================
// IUnknown methods
// =======================================================================

/************************************************************************/
/*                           QueryInterface()                           */
/************************************************************************/

STDMETHODIMP OGRComGeometryFactory::QueryInterface(REFIID rIID,
                                            void** ppInterface)
{
   // Set the interface pointer
   if (rIID == IID_IUnknown) {
      *ppInterface = this;
   }

   else if (rIID == IID_IGeometryFactory) {
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

/************************************************************************/
/*                               AddRef()                               */
/************************************************************************/

STDMETHODIMP_(ULONG) OGRComGeometryFactory::AddRef()
{
   // Increment the reference count
   m_cRef++;

   return m_cRef;
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

STDMETHODIMP_(ULONG) OGRComGeometryFactory::Release()
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
// IGeometryFactory methods
// =======================================================================

/************************************************************************/
/*                           CreateFromWKB()                            */
/************************************************************************/

STDMETHODIMP 
OGRComGeometryFactory::CreateFromWKB( VARIANT wkb, 
                                      ISpatialReference * spatialRef, 
                                      IGeometry **geometry )

{
    OGRGeometry      *poOGRGeometry = NULL;
    OGRErr           eErr = OGRERR_NONE;
    unsigned char    *pabyRawData;

    assert( wkb.vt == (VT_UI1 | VT_ARRAY) );

    SafeArrayAccessData( *(wkb.pparray), (void **) &pabyRawData );
    eErr = OGRGeometryFactory::createFromWkb( pabyRawData, 
                                              &poOGRGeometry );
    SafeArrayUnaccessData( *(wkb.pparray) );

    if( eErr == OGRERR_NONE )
    {
        if( poOGRGeometry->getGeometryType() == wkbPoint )
        {
            *geometry = new OGRComPoint( (OGRPoint *) poOGRGeometry );
            (*geometry)->AddRef();
        }
        else if( poOGRGeometry->getGeometryType() == wkbLineString )
        {
            *geometry = new OGRComLineString( (OGRLineString *) 
                                              poOGRGeometry );
            (*geometry)->AddRef();
        }
        else if( poOGRGeometry->getGeometryType() == wkbPolygon )
        {
            *geometry = new OGRComPolygon( (OGRPolygon *) poOGRGeometry );
            (*geometry)->AddRef();
        }
        else
        {
            printf( "Didn't recognise type of OGRGeometry\n" );
            eErr = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }
    }
    else
    {
        printf( "OGRGeometryFactory::createFromWkb() failed.\n" );
    }

    if( eErr != OGRERR_NONE )
        return E_FAIL;
    else
        return ResultFromScode( S_OK );
}

/************************************************************************/
/*                           CreateFromWKT()                            */
/************************************************************************/

STDMETHODIMP 
OGRComGeometryFactory::CreateFromWKT( BSTR wrt,
                                      ISpatialReference * spatialRef, 
                                      IGeometry **geometry )

{
    return E_FAIL;
}

