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

const IID IID_IGeometryFactory = {0x6A124033,0xFE38,0x11d0,{0xBE,0xCE,0x00,0x80,0x5F,0x7C,0x42,0x68}};

const IID IID_ISpatialReferenceFactory = {0x620600B1,0xFEA1,0x11d0,{0xB0,0x4B,0x00,0x80,0xC7,0xF7,0x94,0x81}};


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
    if( FAILED(hr) ) 
    {
        DumpErrorHResult( hr, "CoCreateInstance" );
    }

/* -------------------------------------------------------------------- */
/*      Report our geometry factory id.                                 */
/* -------------------------------------------------------------------- */
    printf( "pIGeometryFactory = %p\n", pIGeometryFactory );

/* -------------------------------------------------------------------- */
/*      Error and cleanup.                                              */
/* -------------------------------------------------------------------- */
  error:    

    if( pIGeometryFactory != NULL )
        pIGeometryFactory->Release();

    OleSupUninitialize();
}    

