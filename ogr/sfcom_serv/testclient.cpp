/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Mainline for simple client testing geometry service.
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

#define INITGUID
#define DBINITCONSTANTS

#include "oledb_sup.h"
#include "ogr_geometry.h"
#include "geometryidl.h"

#include "sfclsid.h"

//const IID IID_IGeometry = {0x6A124031,0xFE38,0x11d0,{0xBE,0xCE,0x00,0x80,0x5F,0x7C,0x42,0x68}};

const IID IID_IPoint = 
      {0x6A124035,0xFE38,0x11d0,{0xBE,0xCE,0x00,0x80,0x5F,0x7C,0x42,0x68}};

const IID IID_IGeometryFactory = 
      {0x6A124033,0xFE38,0x11d0,{0xBE,0xCE,0x00,0x80,0x5F,0x7C,0x42,0x68}};

const IID IID_ISpatialReferenceFactory = 
      {0x620600B1,0xFEA1,0x11d0,{0xB0,0x4B,0x00,0x80,0xC7,0xF7,0x94,0x81}};

unsigned char abyPoint[21] = { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x59,
                               0x40, 0, 0, 0, 0, 0, 0, 0x69, 0x40 };

/************************************************************************/
/*                                main()                                */
/************************************************************************/

void main( int nArgc, char ** papszArgv )
{
//    CLSID        &hGeomFactoryCLSID = (CLSID) CLSID_CadcorpSFGeometryFactory;
    CLSID        &hGeomFactoryCLSID = (CLSID) CLSID_OGRComClassFactory;
    IGeometryFactory *pIGeometryFactory = NULL;
    HRESULT      hr;

/* -------------------------------------------------------------------- */
/*      Initialize OLE                                                  */
/* -------------------------------------------------------------------- */
    if( !OleSupInitialize() )
    {
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Try and instantiate a Cadcorp geometry factory.                 */
/* -------------------------------------------------------------------- */ 
    hr = CoCreateInstance( hGeomFactoryCLSID, NULL, 
                           CLSCTX_INPROC_SERVER, 
                           IID_IGeometryFactory, (void **)&pIGeometryFactory); 
    printf( "pIGeometryFactory = %p\n", pIGeometryFactory );
    if( FAILED(hr) || pIGeometryFactory == NULL ) 
    {
        DumpErrorHResult( hr, "CoCreateInstance" );
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Try to create a SafeArray holding our point in well known       */
/*      binary format.                                                  */
/* -------------------------------------------------------------------- */
    SAFEARRAYBOUND      aoBounds[1];
    SAFEARRAY           *pArray;
    void                *pSafeData;
    
    aoBounds[0].lLbound = 0;
    aoBounds[0].cElements = sizeof(abyPoint);

    pArray = SafeArrayCreate(VT_UI1, 1, aoBounds);
    hr = SafeArrayAccessData( pArray, &pSafeData );
    memcpy( pSafeData, abyPoint, sizeof(abyPoint) );
    SafeArrayUnaccessData( pArray );
    
/* -------------------------------------------------------------------- */
/*      Create a VARIANT to hold the safe array.                        */
/* -------------------------------------------------------------------- */
    VARIANT      oVarData;

    VariantInit( &oVarData );
    oVarData.vt = VT_UI1 | VT_ARRAY;
    oVarData.pparray = &pArray;

/* -------------------------------------------------------------------- */
/*      Try to create a geometry object for this information.           */
/* -------------------------------------------------------------------- */ 
    IGeometry      *pIGeometry;

    hr = pIGeometryFactory->CreateFromWKB( oVarData, NULL,
                                           &pIGeometry );

    printf( "pIGeometry = %p\n", pIGeometry );

    if( FAILED(hr) || pIGeometryFactory == NULL ) 
    {
        DumpErrorHResult( hr, "CoCreateInstance" );
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Try to get a point interface on this geometry object.           */
/* -------------------------------------------------------------------- */
    IPoint      *pIPoint;

    hr = pIGeometry->QueryInterface( IID_IPoint, (void **) &pIPoint );

    printf( "IPoint = %p\n", pIPoint );

    if( FAILED(hr) || pIPoint == NULL ) 
    {
        DumpErrorHResult( hr, "QueryInterface(IPoint)" );
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Get information on point.                                       */
/* -------------------------------------------------------------------- */
    double            x, y;
    
    hr = pIPoint->Coords( &x, &y );

    if( FAILED(hr) ) 
    {
        DumpErrorHResult( hr, "IPoint->Coord()" );
        goto error;
    }

    printf( "X = %f, Y = %f\n", x, y );
    
/* -------------------------------------------------------------------- */
/*      Error and cleanup.                                              */
/* -------------------------------------------------------------------- */
  error:    

    if( pIGeometryFactory != NULL )
        pIGeometryFactory->Release();

    OleSupUninitialize();
}    

