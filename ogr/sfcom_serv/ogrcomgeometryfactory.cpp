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
 * Revision 1.5  1999/05/20 19:46:15  warmerda
 * add some automatation, and Wkt support
 *
 * Revision 1.4  1999/05/20 14:54:55  warmerda
 * started work on automation
 *
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
    oDispatcher.SetOwner( this );
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
    OGRComDebug( "info", "OGRComGeometryFactory::QueryInterface()\n" );

    if (rIID == IID_IUnknown) {
        *ppInterface = this;
    }

    else if (rIID == IID_IGeometryFactory) {
        *ppInterface = this;
    }

    else if (rIID == IID_IDispatch) {
        *ppInterface = &oDispatcher;
        OGRComDebug( "info", 
                     "OGRComGeometryFactory::QueryInterface()"
                     " - got IDispatch\n" );
    }

    // We don't support this interface
    else {
        OGRComDebug( "Failure",
                     "E_NOINTERFACE from "
                     "OGRComGeometryFactory::QueryInterface()\n" );

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
/*                           COMifyGeometry()                           */
/*                                                                      */
/*      Local method for turning an OGRGeometry into an                 */
/*      OGRComGeometry.  This method assumes ownership of the passed    */
/*      OGRGeometry.  Note the method is static, and the returned       */
/*      object will be set to a reference count of 1.                   */
/************************************************************************/

IGeometry *OGRComGeometryFactory::COMifyGeometry( OGRGeometry * poGeom )

{
    IGeometry      *poCOMGeom = NULL;

    if( poGeom->getGeometryType() == wkbPoint )
    {
        poCOMGeom = new OGRComPoint( (OGRPoint *) poGeom );
    }
    else if( poGeom->getGeometryType() == wkbLineString )
    {
        poCOMGeom = new OGRComLineString( (OGRLineString *) 
                                          poGeom );
    }
    else if( poGeom->getGeometryType() == wkbPolygon )
    {
        poCOMGeom = new OGRComPolygon( (OGRPolygon *) poGeom );
    }
    else
    {
        OGRComDebug( "failure", 
                     "Didn't recognise type of OGRGeometry\n" );
    }

    if( poCOMGeom != NULL )
        poCOMGeom->AddRef();

    return poCOMGeom;
}

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

    // notdef: not doing anything with spatial ref yet. 

    assert( wkb.vt == (VT_UI1 | VT_ARRAY) );

    SafeArrayAccessData( *(wkb.pparray), (void **) &pabyRawData );
    eErr = OGRGeometryFactory::createFromWkb( pabyRawData, NULL,
                                              &poOGRGeometry );
    SafeArrayUnaccessData( *(wkb.pparray) );

    if( eErr == OGRERR_NONE )
    {
        *geometry = COMifyGeometry( poOGRGeometry );
        if( *geometry == NULL )
            eErr = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }
    else
    {
        OGRComDebug( "failure",
                     "OGRGeometryFactory::createFromWkb() failed.\n" );
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
    OGRGeometry      *poOGRGeometry = NULL;
    OGRErr           eErr = OGRERR_NONE;
    char             *pszANSIWkt = NULL;

    // notdef: not doing anything with spatial ref yet. 
    OGRComDebug( "info", "createFromWKT(%S)\n", wrt );

    UnicodeToAnsi( wrt, &pszANSIWkt );

    eErr = OGRGeometryFactory::createFromWkt( pszANSIWkt, NULL,
                                              &poOGRGeometry );
    
    CoTaskMemFree( pszANSIWkt );

    if( eErr == OGRERR_NONE )
    {
        *geometry = COMifyGeometry( poOGRGeometry );
        if( *geometry == NULL )
            eErr = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }
    else
    {
        OGRComDebug( "failure",
                     "OGRGeometryFactory::createFromWkb() failed.\n" );
    }

    if( eErr != OGRERR_NONE )
        return E_FAIL;
    else
        return ResultFromScode( S_OK );
}

/************************************************************************/
/* ==================================================================== */
/*                   OGRComGeometryFactoryDispatcher                    */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                  OGRComGeometryFactoryDispatcher()                   */
/************************************************************************/

OGRComGeometryFactoryDispatcher::OGRComGeometryFactoryDispatcher()

{
    poOwner = NULL;
}

/************************************************************************/
/*                              SetOwner()                              */
/************************************************************************/

void OGRComGeometryFactoryDispatcher::SetOwner( OGRComGeometryFactory * po )

{
    poOwner = po;
}

// =======================================================================
//    IUnknown methods
// =======================================================================

/************************************************************************/
/*                           QueryInterface()                           */
/************************************************************************/

STDMETHODIMP 
OGRComGeometryFactoryDispatcher::QueryInterface(REFIID rIID,
                                                void** ppInterface)
{
    return poOwner->QueryInterface( rIID, ppInterface );
}

/************************************************************************/
/*                               AddRef()                               */
/************************************************************************/

STDMETHODIMP_(ULONG) 
OGRComGeometryFactoryDispatcher::AddRef()
{
    return poOwner->AddRef();
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

STDMETHODIMP_(ULONG) 
OGRComGeometryFactoryDispatcher::Release()
{
    return poOwner->Release();
}

// =======================================================================
// IDispatch methods
// =======================================================================

/************************************************************************/
/*                          GetTypeInfoCount()                          */
/************************************************************************/
STDMETHODIMP 
OGRComGeometryFactoryDispatcher::GetTypeInfoCount(UINT *pctInfo)
{
    *pctInfo=0;
    OGRComDebug( "info", "GetTypeInfoCount\n" );
    return NOERROR;
}

/************************************************************************/
/*                            GetTypeInfo()                             */
/************************************************************************/
STDMETHODIMP 
OGRComGeometryFactoryDispatcher::GetTypeInfo(UINT itinfo, LCID lcid,
                                             ITypeInfo **pptInfo)
{
    OGRComDebug( "info", "GetTypeInfo\n" );

    *pptInfo=NULL;
    return ResultFromScode(E_NOTIMPL);
}

/************************************************************************/
/*                           GetIDsOfNames()                            */
/************************************************************************/

#define METHOD_createFromWkb      100
#define METHOD_createFromWkt      101

STDMETHODIMP 
OGRComGeometryFactoryDispatcher::GetIDsOfNames(REFIID riid,
                                               OLECHAR **rgszNames, 
                                               UINT cNames, LCID lcid, 
                                               DISPID *rgDispID)
{
    HRESULT     hr;
    int         i;
    int         idsMin;
    LPTSTR      psz;

    if (IID_NULL!=riid)
        return ResultFromScode(DISP_E_UNKNOWNINTERFACE);

    rgDispID[0]=DISPID_UNKNOWN;
    hr=ResultFromScode(DISP_E_UNKNOWNNAME);
    
    OGRComDebug( "info", "GetIdsOfNames(%S)\n", rgszNames[0] );

    if( wcsicmp( rgszNames[0], L"createFromWkt" ) == 0 )
    {
        rgDispID[0] = METHOD_createFromWkt;
        hr = NOERROR;
    }
    else if( wcsicmp( rgszNames[0], L"createFromWkb" ) == 0 )
    {
        rgDispID[0] = METHOD_createFromWkb;
        hr = NOERROR;
    }

    return hr;
}

/************************************************************************/
/*                               Invoke()                               */
/************************************************************************/

STDMETHODIMP 
OGRComGeometryFactoryDispatcher::Invoke( DISPID dispID, REFIID riid, LCID lcid,
                                         unsigned short wFlags, 
                                         DISPPARAMS * pDispParams,
                                         VARIANT * pVarResult, 
                                         EXCEPINFO *pExcepInfo,
                                         UINT * puArgErr )

{
    OGRComDebug( "info", 
                 "OGRComGeometryFactoryDispatcher::Invoke(%d)\n", 
                 dispID );

#ifdef notdef
    HRESULT     hr;
    //riid is supposed to be IID_NULL always.
    if (IID_NULL!=riid)
        return ResultFromScode(DISP_E_UNKNOWNINTERFACE);
    switch (dispID)
    {
        case PROPERTY_SOUND:
            if (DISPATCH_PROPERTYGET & wFlags
                œœ DISPATCH_METHOD & wFlags)
            {
                if (NULL==pVarResult)
                    return ResultFromScode(E_INVALIDARG);
                VariantInit(pVarResult);
                V_VT(pVarResult)=VT_I4;
                V_I4(pVarResult)=m_pObj->m_lSound;
                return NOERROR;
            }
            else
            {
                //DISPATCH_PROPERTYPUT
                long        lSound;
                int         c;
                VARIANT     vt;
                if (1!=pDispParams->cArgs)
                    return ResultFromScode(DISP_E_BADPARAMCOUNT);
                c=pDispParams->cNamedArgs;
                if (1!=c œœ (1==c && DISPID_PROPERTYPUT
                                   !=pDispParams->rgdispidNamedArgs[0]))
                    return ResultFromScode(DISP_E_PARAMNOTOPTIONAL);
                VariantInit(&vt);
                hr=VariantChangeType(&vt, &pDispParams->rgvarg[0]
                                     , 0, VT_I4);
                if (FAILED(hr))
                {
                    if (NULL!=puArgErr)
                        *puArgErr=0;
                    return hr;
                }
                lSound=vt.lVal;
                if (MB_OK!=lSound && MB_ICONEXCLAMATION!=lSound
                    && MB_ICONQUESTION!=lSound && MB_ICONHAND!=lSound
                    && MB_ICONASTERISK!=lSound)
                {
                    if (NULL==pExcepInfo)
                        return ResultFromScode(E_INVALIDARG);
                    pExcepInfo->wCode=EXCEPTION_INVALIDSOUND;
                    pExcepInfo->scode=
                        (SCODE)MAKELONG(EXCEPTION_INVALIDSOUND
                                        , PRIMARYLANGID(lcid));
                    FillException(pExcepInfo);
                    return ResultFromScode(DISP_E_EXCEPTION);
                }
                //Everything checks out: save new value.
                m_pObj->m_lSound=lSound;
            }
            break;
        case METHOD_BEEP:
            if (!(DISPATCH_METHOD & wFlags))
                return ResultFromScode(DISP_E_MEMBERNOTFOUND);
            if (0!=pDispParams->cArgs)
                return ResultFromScode(DISP_E_BADPARAMCOUNT);
            MessageBeep((UINT)m_pObj->m_lSound);
            //The result of this method is the sound we played.
            if (NULL!=pVarResult)
            {
                VariantInit(pVarResult);
                V_VT(pVarResult)=VT_I4;
                V_I4(pVarResult)=m_pObj->m_lSound;
            }
            break;
        default:
            ResultFromScode(DISP_E_MEMBERNOTFOUND);
    }
    return NOERROR;
#endif
    return E_FAIL;
}







