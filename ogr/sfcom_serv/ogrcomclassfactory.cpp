/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRComClassFactory, a factory to build
 *           externally creatable objects in the geometry service.
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
 * Revision 1.2  1999/05/14 13:28:38  warmerda
 * client and service now working for IPoint
 *
 * Revision 1.1  1999/05/13 19:49:01  warmerda
 * New
 *
 */

#include "ogrcomgeometry.h"

/************************************************************************/
/*                         OGRComClassFactory()                         */
/************************************************************************/

OGRComClassFactory::OGRComClassFactory() 

{
    m_cRef = 0;
}

// =======================================================================
// IUnknown methods
// =======================================================================

/************************************************************************/
/*                           QueryInterface()                           */
/************************************************************************/

STDMETHODIMP OGRComClassFactory::QueryInterface(REFIID rIID,
                                                void** ppInterface)
{
   // Set the interface pointer
   if (rIID == IID_IUnknown) {
      *ppInterface = this;
   }

   else if (rIID == IID_IClassFactory) {
      *ppInterface = this;
   }

   // We don't support this interface
   else {
      printf( "OGRComClassFactory::QueryInterface() ... failed.\n" );
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

STDMETHODIMP_(ULONG) OGRComClassFactory::AddRef()
{
   // Increment the reference count
   m_cRef++;

   return m_cRef;
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

STDMETHODIMP_(ULONG) OGRComClassFactory::Release()
{
   // Decrement the reference count
   m_cRef--;

   // Is this the last reference to the object?
   if (m_cRef)
      return m_cRef;

   // Decrement the server object count
//   Counters::DecObjectCount();

   // self destruct 
   delete this;

   return 0;
}

// =======================================================================
// IClassFactory methods
// =======================================================================

/************************************************************************/
/*                           CreateInstance()                           */
/************************************************************************/

STDMETHODIMP OGRComClassFactory::CreateInstance(LPUNKNOWN pUnkOuter,
                                                     REFIID rIID,
                                                     void** ppvInterface)
{
   IUnknown           *pObj = NULL;
   HRESULT            hResult;

   // Initialize returned interface pointer
   *ppvInterface = NULL;
   
   // Check for a controlling unknown (notdef)
   if (pUnkOuter && rIID != IID_IUnknown)
      return CLASS_E_NOAGGREGATION;

   if( rIID == IID_IGeometryFactory )
   {
       pObj = new OGRComGeometryFactory;
   }

   // eventually there may be other classes here. 

   // Now obtain the requested interface
   if( pObj == NULL )
   {
       printf( "OGRComClassFactory::CreateInstance() ... interface not"
               " recognised.\n" );
   }
   else
       hResult = pObj->QueryInterface(rIID, ppvInterface);

   // Destroy the object if the desired interface couldn't be obtained
   if (FAILED(hResult)) {
      printf( "In OGRComClassFactory::CreateInstance() ... couldn't"
              "get desired interface.\n" );
      delete pObj;
      return hResult;
   }

   return NOERROR;
}

/************************************************************************/
/*                             LockServer()                             */
/************************************************************************/

STDMETHODIMP OGRComClassFactory::LockServer(BOOL fLock)
{
#ifdef notdef
   VerboseMsg("In OGRComClassFactory::LockServer\n");

   // Are we locking or unlocking?
   if (fLock) {

      // Locking
      VerboseMsg("   Incrementing server lock count\n");
      Counters::IncLockCount();
   }
   else {

      // Unlocking
      VerboseMsg("   Decrementing server lock count\n");
      Counters::DecLockCount();
   }
#endif

   return NOERROR;
}
