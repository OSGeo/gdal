/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRComWks class.  It is only intended to
 *           be used as an inner class within an OGRComGeometryTmpl derived
 *           class.
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
 * Revision 1.1  1999/05/21 02:38:51  warmerda
 * New
 *
 */

#include "ogrcomgeometry.h"
#include "ogrcomgeometrytmpl.h"

/************************************************************************/
/*                             OGRComWks()                              */
/************************************************************************/

OGRComWks::OGRComWks()

{
    pOuter = NULL;
    poOGRGeometry = NULL;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void OGRComWks::Initialize( IUnknown * pOuterIn, 
                            OGRGeometry * poOGRGeometryIn )

{
    pOuter = pOuterIn;
    poOGRGeometry = poOGRGeometryIn;
}

// =======================================================================
// IUnknown methods ... defer to outer object.
// =======================================================================

/************************************************************************/
/*                           QueryInterface()                           */
/************************************************************************/

STDMETHODIMP OGRComWks::QueryInterface(REFIID rIID, void** ppInterface)
{
    return pOuter->QueryInterface( rIID, ppInterface );
}

/************************************************************************/
/*                               AddRef()                               */
/************************************************************************/

STDMETHODIMP_(ULONG) OGRComWks::AddRef()
{
    return pOuter->AddRef();
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

STDMETHODIMP_(ULONG) OGRComWks::Release()
{
    return pOuter->Release();
}

// =======================================================================
//  IWks methods.
// =======================================================================

/************************************************************************/
/*                            ExportToWKB()                             */
/************************************************************************/

STDMETHODIMP OGRComWks::ExportToWKB( VARIANT *wkb )

{
    HRESULT      hr;

/* -------------------------------------------------------------------- */
/*      Create a safe array to hold this data.                          */
/* -------------------------------------------------------------------- */
    SAFEARRAYBOUND      aoBounds[1];
    SAFEARRAY           *pArray;
    unsigned char       *pSafeData;
    int                 nDataBytes = poOGRGeometry->WkbSize();

    aoBounds[0].lLbound = 0;
    aoBounds[0].cElements = nDataBytes;

    pArray = SafeArrayCreate(VT_UI1, 1, aoBounds);
    if( pArray == NULL )
    {
        OGRComDebug( "failure", 
                     "failed to create %d byte SafeArray() in ExportToWkb\n", 
                     nDataBytes );
        return E_FAIL;
    }

    hr = SafeArrayAccessData( pArray, (void **) &pSafeData );

/* -------------------------------------------------------------------- */
/*      Transform the geometry into this safe arrays buffer.            */
/* -------------------------------------------------------------------- */
    if( poOGRGeometry->exportToWkb( wkbNDR, pSafeData ) != OGRERR_NONE )
    {
        // we should cleanup the safe array.
        return E_FAIL;
    }

    SafeArrayUnaccessData( pArray );
    
/* -------------------------------------------------------------------- */
/*      Update the VARIANT to hold the safe array.                      */
/* -------------------------------------------------------------------- */
    VariantInit( wkb );
    wkb->vt = VT_UI1 | VT_ARRAY;
    wkb->pparray = &pArray;

    // notdef: we can't keep a  pointer to pArray ... it's a local variable!

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                            ExportToWKT()                             */
/************************************************************************/

STDMETHODIMP OGRComWks::ExportToWKT( BSTR *wkt )

{
    HRESULT      hr;
    char         *pszASCIIWkt = NULL;

/* -------------------------------------------------------------------- */
/*      Transform the geometry into an ASCII string.                    */
/* -------------------------------------------------------------------- */
    if( poOGRGeometry->exportToWkt( &pszASCIIWkt ) != OGRERR_NONE )
    {
        // we should cleanup the safe array.
        return E_FAIL;
    }

/* -------------------------------------------------------------------- */
/*      Translate the ASCII string into a unicode string.               */
/* -------------------------------------------------------------------- */
    hr = AnsiToBSTR( pszASCIIWkt, wkt );
    
    CPLFree( pszASCIIWkt );

    return hr;
}

/************************************************************************/
/*                           ImportFromWKB()                            */
/************************************************************************/

STDMETHODIMP OGRComWks::ImportFromWKB( VARIANT wkb, ISpatialReference *poSR )

{
    unsigned char     *pabyRawData;
    OGRErr            eErr;

    if( wkb.vt != (VT_UI1 | VT_ARRAY) )
    {
        OGRComDebug( "failure", 
                     "Didn't get a UI1|ARRAY VARIANT in ImportFromWKB()\n" );
        return E_FAIL;
    }

    SafeArrayAccessData( *(wkb.pparray), (void **) &pabyRawData );
    eErr = poOGRGeometry->importFromWkb( pabyRawData );
    SafeArrayUnaccessData( *(wkb.pparray) );

    if( eErr != OGRERR_NONE )
        return E_FAIL;

    // notdef: not assigning the spatial reference yet.
    
    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                           ImportFromWKT()                            */
/************************************************************************/

STDMETHODIMP OGRComWks::ImportFromWKT( BSTR wkt, ISpatialReference *poSR )

{
    OGRErr       eErr;
    HRESULT      hr;
    char         *pszAnsiWkt = NULL, *pszTempAnsiWkt;

    hr = UnicodeToAnsi( wkt, &pszAnsiWkt );
    if( FAILED(hr) )
        return hr;

    pszTempAnsiWkt = pszAnsiWkt;
    eErr = poOGRGeometry->importFromWkt( &pszTempAnsiWkt );

    CoTaskMemFree( pszAnsiWkt );

    if( eErr != OGRERR_NONE )
        return E_FAIL;

    // notdef: not assigning the spatial reference yet.
    
    return ResultFromScode( S_OK );
}
