/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Mainline for dumping stuff from an SFCOM OLEDB provider.
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
 * Revision 1.11  1999/05/21 02:37:35  warmerda
 * use IWks::exportToWKT() to report geometry info
 *
 * Revision 1.10  1999/05/20 14:52:48  warmerda
 * create OGRCom class factory instead of cadcorp
 *
 * Revision 1.9  1999/05/17 14:40:14  warmerda
 * Get IIDs from sfiiddef.h.
 *
 * Revision 1.8  1999/05/14 13:32:25  warmerda
 * added temporary debugging stuff
 *
 */

#define INITGUID
#define DBINITCONSTANTS

#include "oledb_sup.h"
#include "oledb_sf.h"

#include "ogr_geometry.h"

#include "geometryidl.h"
#include "spatialreferenceidl.h"

// Get various classid.
#include "msdaguid.h"
#include "MSjetoledb.h"
#include "sfclsid.h"
#include "sfiiddef.h"

static HRESULT SFDumpGeomColumn( IOpenRowset*, const char *, const char * );
static HRESULT SFDumpSchema( IOpenRowset*, const char * );
static HRESULT SFDumpRowset( OledbSupRowset * );

static int      bVerbose = TRUE;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: sfdump [-provider provider_clsid_alias] [-ds datasource]\n"
            "              [-table tablename] [-column geom_column_name]\n"
            "              [-action {dumpgeom,dumpschema}] -quiet\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

void main( int nArgc, char ** papszArgv )
{
    CLSID       &hProviderCLSID = (CLSID) CLSID_JETOLEDB_3_51;
    const char *pszDataSource = "f:\\opengis\\SFData\\World.mdb";
    const char *pszTable = "worldmif_geometry";
    const char *pszGeomColumn = NULL;
    HRESULT     hr;
    IOpenRowset *pIOpenRowset = NULL;
    const char  *pszAction = "dumpgeom";
   
/* -------------------------------------------------------------------- */
/*      Initialize OLE                                                  */
/* -------------------------------------------------------------------- */
    if( !OleSupInitialize() )
    {
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Process commandline switches                                    */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( iArg < nArgc - 1 && stricmp( papszArgv[iArg], "-provider") == 0 )
        {
            iArg++;
            if( stricmp(papszArgv[iArg],"Cadcorp") == 0 )
                hProviderCLSID = CLSID_CadcorpSFProvider;
            else
                /* need generic translator */;
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-ds") == 0 )
        {
            pszDataSource = papszArgv[++iArg];
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-table") == 0 )
        {
            pszTable = papszArgv[++iArg];
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-column") == 0 )
        {
            pszGeomColumn = papszArgv[++iArg];
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-action") == 0 )
        {
            pszAction = papszArgv[++iArg];
        }
        else if( stricmp( papszArgv[iArg],"-quiet") == 0 )
        {
            bVerbose = FALSE;
        }
        else
        {
            printf( "Unrecognised option: %s\n\n", papszArgv[iArg] );
            Usage();
        }
    }

/* -------------------------------------------------------------------- */
/*      Open the data provider source (for instance select JET, and     */
/*      access an MDB file.                                             */
/* -------------------------------------------------------------------- */
    hr = OledbSupGetDataSource( hProviderCLSID, pszDataSource, 
                                &pIOpenRowset );
   
    if( FAILED( hr ) )
        goto error;

    fprintf( stdout, "Acquired data source %S.\n", pszDataSource );

/* -------------------------------------------------------------------- */
/*      Ask for a dump of a particular column.                          */
/* -------------------------------------------------------------------- */
    if( stricmp(pszAction,"dumpgeom") == 0 )
        SFDumpGeomColumn( pIOpenRowset, pszTable, pszGeomColumn );

    else if( stricmp(pszAction,"dumpschema") == 0 )
        SFDumpSchema( pIOpenRowset, pszTable );

    else if( stricmp(pszAction,"dump") == 0 )
    {
        OledbSupRowset      oTable;

        hr = oTable.OpenTable( pIOpenRowset, pszTable );
        if( !FAILED(hr) )
            SFDumpRowset( &oTable );
    }
    else
    {
        printf( "Action not recognised: %s\n\n", pszAction );
        Usage();
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
  error:    

    if( pIOpenRowset != NULL )
        pIOpenRowset->Release();

    OleSupUninitialize();
}    
/************************************************************************/
/*                        SFDumpGeometryAsWKT()                         */
/************************************************************************/

void SFDumpGeometryAsWKT( IGeometry * pIGeometry )

{
    IWks      *pIWks;
    HRESULT   hr;

    hr = pIGeometry->QueryInterface( IID_IWks, (void **) &pIWks );
    if( FAILED(hr) )
    {
        DumpErrorHResult( hr, 
                          "Can't get IID_IWks on IGeometry.\n" );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Now get geometry as well known text representation.             */
/* -------------------------------------------------------------------- */
    BSTR      wkt;

    hr = pIWks->ExportToWKT( &wkt );
    if( FAILED(hr) )
        DumpErrorHResult( hr, "exportToWkt()" );
    else
    {
        printf( "WKT = `%S'\n", wkt );
        SysFreeString( wkt );
    }
    pIWks->Release();
}

/************************************************************************/
/*                          SFDumpGeomColumn()                          */
/*                                                                      */
/*      Dump all the geometry objects in a table based on a geometry    */
/*      column name.                                                    */
/************************************************************************/

static HRESULT SFDumpGeomColumn( IOpenRowset* pIOpenRowset, 
                                 const char *pszTableName, 
                                 const char *pszColumnName )

{
    HRESULT           hr;
    OledbSFTable      oTable;
    IGeometryFactory *pIGeometryFactory = NULL;
    ISpatialReference *pISR = NULL;

/* -------------------------------------------------------------------- */
/*      Open the table.                                                 */
/* -------------------------------------------------------------------- */
    hr = oTable.OpenTable( pIOpenRowset, pszTableName );
    if( FAILED( hr ) )
        return hr;

/* -------------------------------------------------------------------- */
/*      If a specific column was requested, select it now.              */
/* -------------------------------------------------------------------- */
    if( pszColumnName != NULL )
        oTable.SelectGeometryColumn( pszColumnName );

/* -------------------------------------------------------------------- */
/*      Try and instantiate a Cadcorp geometry factory.                 */
/* -------------------------------------------------------------------- */ 
//    hr = CoCreateInstance( CLSID_CadcorpSFGeometryFactory, NULL, 
    hr = CoCreateInstance( CLSID_OGRComClassFactory, NULL, 
                           CLSCTX_INPROC_SERVER, 
                           IID_IGeometryFactory, (void **)&pIGeometryFactory); 
    if( FAILED(hr) ) 
    {
        DumpErrorHResult( hr, 
                          "CoCreateInstance of CLSID_CadcorpSFGeomtryFactory" );
    }

/* -------------------------------------------------------------------- */
/*      If we got a geometry factory, try to make a spatial             */
/*      reference factory.                                              */
/* -------------------------------------------------------------------- */
#ifdef notdef
    if( pIGeometryFactory != NULL )
    {
        ISpatialReferenceFactory *pISRFactory = NULL;

        hr = CoCreateInstance( CLSID_CadcorpSFSpatialReferenceFactory, NULL, 
                               CLSCTX_INPROC_SERVER, 
                               IID_ISpatialReferenceFactory, 
                               (void **)&pISRFactory); 
        if( FAILED(hr) ) 
        {
            DumpErrorHResult( hr, 
                              "CoCreateInstance of "
                              "CLSID_CadcorpSFSpatialReferenceFactory" );

            pISRFactory = NULL;
        }

/* -------------------------------------------------------------------- */
/*      Now try to create a spatial reference object.                   */
/* -------------------------------------------------------------------- */
        if( pISRFactory != NULL )
        {
            BSTR sWKT = L"GEOGCS[\"Latitude/Longitude.WGS 84\",DATUM[\"WGS 84\",SPHEROID[\"anon\",6378137,298.25722356049]],PRIMEM[\"Greenwich\",0],UNIT[\"Degree\",0.0174532925199433]]";

           hr = pISRFactory->CreateFromWKT( sWKT, &pISR );
            if( FAILED(hr) )
            {
                DumpErrorHResult( hr, 
                                  "CreateFromWKT failed." );
                pISR = NULL;
            }

            pISRFactory->Release();
            pISRFactory = NULL;
        }

        if( pISR == NULL )
        {
            pIGeometryFactory->Release();
            pIGeometryFactory = NULL;
        }
    }
#endif

/* -------------------------------------------------------------------- */
/*      For now we just read through, counting records to verify        */
/*      things are working.                                             */
/* -------------------------------------------------------------------- */
    int      nRecordCount = 0;

    while( oTable.GetNextRecord( &hr ) )
    {
        BYTE      *pabyData;
        int       nSize;
        OGRGeometry * poGeom;

/* -------------------------------------------------------------------- */
/*      Get the raw geometry data.                                      */
/* -------------------------------------------------------------------- */
        pabyData = oTable.GetWKBGeometry( &nSize );

        if( pabyData == NULL )
            continue;

        printf( "Read %d bytes.\n", nSize );

/* -------------------------------------------------------------------- */
/*      Create and report geometry using built in class.                */
/* -------------------------------------------------------------------- */
        if( pIGeometryFactory == NULL )
        {
            if( OGRGeometryFactory::createFromWkb( pabyData, NULL, 
                                                   &poGeom, nSize )
                == OGRERR_NONE )
            {
                printf( "(0x%02x%02x%02x%02x%02x)\n", 
                        pabyData[0], 
                        pabyData[1], 
                        pabyData[2], 
                        pabyData[3], 
                        pabyData[4] );
                if( bVerbose )
                    poGeom->dumpReadable( stdout );

                delete poGeom;
            }
            else 
            {
                fprintf( stderr, 
                         "Unable to decode record %d "
                         "(0x%02x%02x%02x%02x%02x)\n", 
                         nRecordCount,
                         pabyData[0], 
                         pabyData[1], 
                         pabyData[2], 
                         pabyData[3], 
                         pabyData[4] );
            }
        }

/* -------------------------------------------------------------------- */
/*      Create geometry objects via COM.                                */
/* -------------------------------------------------------------------- */
        else
        {
            IGeometry      *pIGeometry = NULL;
            VARIANT        oVarData;
            SAFEARRAYBOUND aoBounds[1];
            SAFEARRAY      *pArray;
            void		   *pSafeData;

            aoBounds[0].lLbound = 0;
            aoBounds[0].cElements = nSize;
            
            pArray = SafeArrayCreate(VT_UI1,1,aoBounds);

            hr = SafeArrayAccessData( pArray, &pSafeData );
            if( !FAILED(hr) )
            {
                memcpy( pSafeData, pabyData, nSize );
                SafeArrayUnaccessData( pArray );
            }

            VariantInit( &oVarData );
            oVarData.vt = VT_UI1 | VT_ARRAY;
            oVarData.pparray = &pArray;

            hr = pIGeometryFactory->CreateFromWKB( oVarData, pISR,
                                                   &pIGeometry );

            if( FAILED(hr) )
                DumpErrorHResult( hr, "CreateFromWKB()" );
            else if( pIGeometry != NULL )
            {
                if( bVerbose )
                    SFDumpGeometryAsWKT( pIGeometry );
                pIGeometry->Release();
            }
        }

        nRecordCount++;
    }

    printf( "Read %d records.\n", nRecordCount );

    if( pIGeometryFactory != NULL )
        pIGeometryFactory->Release();

    if( pISR != NULL )
        pISR->Release();

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                            SFDumpSchema()                            */
/************************************************************************/

HRESULT SFDumpSchema( IOpenRowset * pIOpenRowset, const char * pszTable )

{
    HRESULT            hr;
    OledbSupRowset     oTable;
    
/* -------------------------------------------------------------------- */
/*      Open the table.                                                 */
/* -------------------------------------------------------------------- */
    hr = oTable.OpenTable( pIOpenRowset, pszTable );
    if( FAILED( hr ) )
        return hr;

/* -------------------------------------------------------------------- */
/*      Dump each column                                                */
/*                                                                      */
/*      Note that iterating between 0 and numcolumns-1 is't really      */
/*      the same as iterating over the ordinals.  If this table is a    */
/*      subset view, we will miss some columns, and get lots of         */
/*      NULLs.                                                          */
/* -------------------------------------------------------------------- */
    for( int iCol = 0; iCol < oTable.GetNumColumns(); iCol++ )
    {
        DBCOLUMNINFO      *poColumnInfo = oTable.GetColumnInfo( iCol );

        if( poColumnInfo != NULL )
            OledbSupWriteColumnInfo( stdout, poColumnInfo );
    }

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                            SFDumpRowset()                            */
/************************************************************************/

static HRESULT SFDumpRowset( OledbSupRowset * poTable )

{
    HRESULT           hr;
    int      nRecordCount = 0;

    while( poTable->GetNextRecord( &hr ) )
    {
        poTable->DumpRow( stdout );
        nRecordCount++;
    }

    printf( "Read %d records.\n", nRecordCount );

    return hr;
}
