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
 * Revision 1.4  1999/05/21 02:39:50  warmerda
 * Added IWks support
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

#define INITGUID
#define DBINITCONSTANTS

#include "oledb_sup.h"
#include "ogr_geometry.h"
#include "geometryidl.h"

#include "sfclsid.h"

#include "sfiiddef.h"

unsigned char abyPoint[21] = { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x59,
                               0x40, 0, 0, 0, 0, 0, 0, 0x69, 0x40 };

void TestInternalPoint( IGeometryFactory * pIGeometryFactory );
void TestFileGeometry( IGeometryFactory *, const char * );
void ReportGeometry( IGeometry * );

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
/*      Report on the internal point, or if a file is given on the      */
/*      command line, report on the binary geometry in that file.       */
/* -------------------------------------------------------------------- */
    if( nArgc > 1 )
        TestFileGeometry( pIGeometryFactory, papszArgv[1] );
    else
        TestInternalPoint( pIGeometryFactory );

/* -------------------------------------------------------------------- */
/*      Error and cleanup.                                              */
/* -------------------------------------------------------------------- */
  error:    

    if( pIGeometryFactory != NULL )
        pIGeometryFactory->Release();

    OleSupUninitialize();
}    

/************************************************************************/
/*                         TestBinaryGeometry()                         */
/*                                                                      */
/*      Make a geometry object from binary data, and report on the      */
/*      object.                                                         */
/************************************************************************/

void TestBinaryGeometry( IGeometryFactory * pIGeometryFactory, 
                         unsigned char * pabyData, int nDataBytes )

{
    HRESULT      hr;

/* -------------------------------------------------------------------- */
/*      Try to create a SafeArray holding our point in well known       */
/*      binary format.                                                  */
/* -------------------------------------------------------------------- */
    SAFEARRAYBOUND      aoBounds[1];
    SAFEARRAY           *pArray;
    void                *pSafeData;
    
    aoBounds[0].lLbound = 0;
    aoBounds[0].cElements = nDataBytes;

    pArray = SafeArrayCreate(VT_UI1, 1, aoBounds);
    hr = SafeArrayAccessData( pArray, &pSafeData );
    memcpy( pSafeData, pabyData, nDataBytes );
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

    if( FAILED(hr) || pIGeometryFactory == NULL ) 
    {
        DumpErrorHResult( hr, "pIGeometryFactory->CreateFromWKB()" );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Report the geometry.                                            */
/* -------------------------------------------------------------------- */
    ReportGeometry( pIGeometry );

    pIGeometry->Release();
}

/************************************************************************/
/*                         TestInternalPoint()                          */
/************************************************************************/

void TestInternalPoint( IGeometryFactory * pIGeometryFactory )

{
    TestBinaryGeometry( pIGeometryFactory, abyPoint, sizeof(abyPoint) );
}

/************************************************************************/
/*                          TestFileGeometry()                          */
/************************************************************************/

#define MAX_DATA      1000000

void TestFileGeometry( IGeometryFactory * pIGeometryFactory, 
                       const char * pszFilename )

{
    FILE      *fp;
    unsigned char abyData[MAX_DATA];
    int      nBytes;

/* -------------------------------------------------------------------- */
/*      Read the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = fopen( pszFilename, "rb" );
    if( fp == NULL )
    {
        perror( "fopen" );
        return;
    }
    
    nBytes = fread( abyData, 1, MAX_DATA, fp );

    fclose( fp );

/* -------------------------------------------------------------------- */
/*      Test this binary data.                                          */
/* -------------------------------------------------------------------- */
    TestBinaryGeometry( pIGeometryFactory, abyData, nBytes );
}


/************************************************************************/
/*                            ReportPoint()                             */
/************************************************************************/

static void ReportPoint( IPoint * pIPoint, const char * pszPrefix  )

{									
    double            x, y;
    HRESULT           hr;
    
    hr = pIPoint->Coords( &x, &y );

    if( FAILED(hr) ) 
    {
        DumpErrorHResult( hr, "IPoint->Coord()" );
        return;
    }

    printf( "%s X = %f, Y = %f\n", pszPrefix, x, y );
}

/************************************************************************/
/*                          ReportLineString()                          */
/************************************************************************/

static void ReportLineString( ILineString * pILineString, 
                              const char * pszPrefix  )

{					
    long            nPointCount;
    HRESULT           hr;

    hr = pILineString->get_NumPoints( &nPointCount );
    if( FAILED(hr) ) 
    {
        DumpErrorHResult( hr, "ILineString->get_NumPoints()" );
        return;
    }

    printf( "%s NumPoints = %ld\n", pszPrefix, nPointCount );

    for( int i = 0; i < nPointCount; i++ )
    {
        IPoint      *pIPoint = NULL;

        hr = pILineString->Point( i, &pIPoint );
        if( FAILED(hr) ) 
        {
            DumpErrorHResult( hr, "ILineString->Point()" );
            return;
        }

        ReportPoint( pIPoint, "     " );

        pIPoint->Release();
    }
}

/************************************************************************/
/*                           ReportPolygon()                            */
/************************************************************************/

static void ReportPolygon( IPolygon * pIPolygon,
                           const char * pszPrefix  )

{					
    long            nRingCount;
    HRESULT         hr;
    ILinearRing     *pILinearRing;

/* -------------------------------------------------------------------- */
/*      Report on the exterior ring.                                    */
/* -------------------------------------------------------------------- */
    hr = pIPolygon->ExteriorRing( &pILinearRing );
    if( FAILED(hr) ) 
    {
        DumpErrorHResult( hr, "IPolygon->ExteriorRing()" );
        return;
    }

    printf( "%sExterior Ring:\n", pszPrefix );

    ReportLineString( pILinearRing, pszPrefix );

    pILinearRing->Release();

/* -------------------------------------------------------------------- */
/*      Report on interior count.                                       */
/* -------------------------------------------------------------------- */
    long            ringCount;

    hr = pIPolygon->get_NumInteriorRings( &ringCount );
    if( FAILED(hr) ) 
    {
        DumpErrorHResult( hr, "IPolygon->get_NumInteriorRings()" );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Report on interior rings.                                       */
/* -------------------------------------------------------------------- */
    
    printf( "%s NumInternalRings = %ld\n", pszPrefix, ringCount );

    for( int i = 0; i < ringCount; i++ )
    {
        hr = pIPolygon->InteriorRing( i, &pILinearRing );
        if( FAILED(hr) ) 
        {
            DumpErrorHResult( hr, "IPolygon->InternalRing()" );
            return;
        }

        ReportLineString( pILinearRing, "     " );

        pILinearRing->Release();
    }
}


/************************************************************************/
/*                           ReportGeometry()                           */
/************************************************************************/

void ReportGeometry( IGeometry * pIGeometry )

{
    HRESULT      hr;

/* -------------------------------------------------------------------- */
/*      Try as a point.                                                 */
/* -------------------------------------------------------------------- */
    IPoint      *pIPoint;
    
    hr = pIGeometry->QueryInterface( IID_IPoint, (void **) &pIPoint );
    if( !FAILED(hr) )
    {
        ReportPoint( pIPoint, "IPoint:" );

        pIPoint->Release();
        return;
    }

/* -------------------------------------------------------------------- */
/*      Try as a linestring.                                            */
/* -------------------------------------------------------------------- */
    ILineString      *pILineString;
    
    hr = pIGeometry->QueryInterface( IID_ILineString, 
                                     (void **) &pILineString );
    if( !FAILED(hr) )
    {
        printf( "LineString: \n" );
        ReportLineString( pILineString, "  " );
        
        pILineString->Release();
        return;
    }

/* -------------------------------------------------------------------- */
/*      Try as a polygon.                                               */
/* -------------------------------------------------------------------- */
    IPolygon      *pIPolygon;
    
    hr = pIGeometry->QueryInterface( IID_IPolygon, 
                                     (void **) &pIPolygon );
    if( !FAILED(hr) )
    {
        printf( "Polygon: \n" );
        ReportPolygon( pIPolygon, "  " );
        pIPolygon->Release();
        return;
    }

    printf( "Geometry unrecognised.\n" );
}
