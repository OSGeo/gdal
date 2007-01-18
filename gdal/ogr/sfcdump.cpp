/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Mainline for dumping stuff from an SFCOM OLEDB provider.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 ****************************************************************************/

#define INITGUID
#define DBINITCONSTANTS

#include "oledb_sup.h"
#include "sfcenumerator.h"
#include "sfcdatasource.h"
#include "sfctable.h"
#include "ogr_feature.h"
#include "cpl_conv.h"
#include <atldbsch.h>
#include "oledbgis.h"

static int      bVerbose = TRUE;

static void SFCDumpProviders();
static SFCDataSource * SFCOpenDataSource( const char * pszProvider, 
                                          const char * pszDataStore,
                                          const char * pszProviderString );
static void SFCDumpTableSchema( SFCTable * );
static void SFCDumpTableGeometry( SFCTable * );
static void SFCDumpTables( SFCDataSource * );
static void SFCDumpSFTables( SFCDataSource * );
static void SFCDumpTableFeatures( SFCTable * poTable );
static DWORD WINAPI main_thread( LPVOID );
static int SFCDump(  int nArgc, char ** papszArgv );

OGRGeometry      *poSpatialFilter = NULL;
DBPROPOGISENUM   eSpatialOperator = DBPROP_OGIS_ENVELOPE_INTERSECTS;

static int g_nArgc;
static char ** g_papszArgv;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf(
      "Usage: sfcdump  [-mt thread_count] [-provider classname] [-ds datasource] \n"
      "           [-table tablename][-cmd 'sql statement']\n"
      "           [-region top bottom left right]\n"
      "           [-action {dumpprov, dumptables, dumpsftables,\n"
      "                     dumpgeom, dumpfeat, dumpschema}]\n"
      "           [-quiet] [-rc repeat_count]\n"
      "\n"
      "Example:\n"
      "     C:> sfcdump -provider Microsoft.Jet.OLEDB.3.51\n"
      "                 -ds c:\\World.mdb -table worldmif_geometry\n"
      "or\n"
      "     C:> sfcdump -provider Softmap.SF.Shape -ds c:\\polygon\n"
      "                 -table Shape\n" );

    OleSupUninitialize();
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )
{
/* -------------------------------------------------------------------- */
/*      Are we run as a CGI-BIN?                                        */
/* -------------------------------------------------------------------- */
    if( getenv("SERVER_NAME") != NULL )
    {
        printf( "Content-type: text/html\n\n" );
        printf( "<h1>SFCDUMP</h1><pre>\n" );
    }

/* -------------------------------------------------------------------- */
/*      Initialize OLE                                                  */
/* -------------------------------------------------------------------- */
    if( !OleSupInitialize() )
    {
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Are we run in multi-threaded mode?                              */
/* -------------------------------------------------------------------- */
    if( nArgc > 2 && EQUAL(papszArgv[1],"-mt") )
    {
        int            nThreadCount = atoi(papszArgv[2]);
        int            iThread;
        HANDLE         ahThreadHandles[1000];

        assert( nThreadCount <= 1000 );

        g_nArgc = nArgc - 2;
        g_papszArgv = papszArgv + 2;

        CPLSetErrorHandler( CPLLoggingErrorHandler );
        for( iThread = 0; iThread < nThreadCount; iThread++ )
        {
            DWORD  nThreadId;

            ahThreadHandles[iThread] = 
                CreateThread( NULL, 0, main_thread, NULL, 0, &nThreadId );
            CPLDebug( "OGR_SFC", "Created thread %d", nThreadId );
        }

        WaitForMultipleObjects( nThreadCount, ahThreadHandles, TRUE, 
                                INFINITE );
        CPLDebug( "OGR_SFC", "All threads completed." );
    }
    else
        SFCDump( nArgc, papszArgv );
   
/* -------------------------------------------------------------------- */
/*      Cleanup and exit.                                               */
/* -------------------------------------------------------------------- */
    OleSupUninitialize();

    return 0;
}

/************************************************************************/
/*                              SFCDump()                               */
/*                                                                      */
/*      Thread ready "mainline" for sfcdump.                            */
/************************************************************************/

int SFCDump( int nArgc, char ** papszArgv )

{
    int        nRepeatCount = 1;

    const char *pszCommand = NULL;
    const char *pszProvider = "Softmap.SF.Shape";
    const char *pszDataSource = "E:\\data\\esri\\shape\\eg_data\\polygon.shp";
    const char *pszProviderString = NULL;
    const char *pszTable = "polygon";
    const char *pszAction = "dumpfeat";
#ifdef notdef
    const char *pszProvider = "Microsoft.Jet.OLEDB.3.51";
    const char *pszDataSource = "f:\\opengis\\SFData\\World.mdb";
    const char *pszTable = "worldmif_geometry";
    const char *pszAction = "dumpfeat";
#endif

#ifdef notdef
    pszProvider = "ESRI GeoDatabase OLE DB Provider";
    pszDataSource = "D:\\data";
    pszProviderString = "workspacetype=esriCore.ShapefileWorkspaceFactory.1;Geometry=WKB";
    pszTable = "polygon";
#endif

/* -------------------------------------------------------------------- */
/*      Process commandline switches                                    */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( iArg < nArgc - 1 && stricmp( papszArgv[iArg], "-provider") == 0 )
        {
            iArg++;
            pszProvider = papszArgv[iArg];
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-ps") == 0 )
        {
            pszProviderString = papszArgv[++iArg];
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-ds") == 0 )
        {
            pszDataSource = papszArgv[++iArg];
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-rc") == 0 )
        {
            nRepeatCount = atoi(papszArgv[++iArg]);
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-table") == 0 )
        {
            pszTable = papszArgv[++iArg];
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-cmd") == 0 )
        {
            pszCommand = papszArgv[++iArg];
        }
        else if( iArg < nArgc-4 && stricmp( papszArgv[iArg],"-region") == 0 )
        {
            OGRLinearRing  oRing;
            OGRPolygon     *poPoly;
            
            double         dfNorth = atof( papszArgv[iArg+1]);
            double         dfSouth = atof( papszArgv[iArg+2]);
            double         dfEast = atof( papszArgv[iArg+3]);
            double         dfWest = atof( papszArgv[iArg+4]);

            oRing.addPoint( dfWest, dfNorth );
            oRing.addPoint( dfEast, dfNorth );
            oRing.addPoint( dfEast, dfSouth );
            oRing.addPoint( dfWest, dfSouth );
            oRing.addPoint( dfWest, dfNorth );
            
            poPoly = new OGRPolygon();
            poPoly->addRing( &oRing );
            
            poSpatialFilter = poPoly;

            iArg += 4;
        }
#ifdef notdef
        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-column") == 0 )
        {
            pszGeomColumn = papszArgv[++iArg];
        }
#endif
        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-action") == 0 )
        {
            pszAction = papszArgv[++iArg];
        }
        else if( stricmp( papszArgv[iArg],"-quiet") == 0 )
        {
            bVerbose = FALSE;
        }
        else if( stricmp( papszArgv[iArg],"-help") == 0 )
        {
            Usage();
        }
        else
        {
            printf( "Unrecognised option: %s\n\n", papszArgv[iArg] );
            Usage();
        }
    }

/* -------------------------------------------------------------------- */
/*      Perform dump provider action before trying to open anything.    */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszAction,"dumpprov") )
    {
        SFCDumpProviders();
        return 0;
    }

/* -------------------------------------------------------------------- */
/*      Access the requested data source.                               */
/* -------------------------------------------------------------------- */

    int           iRepetition;

    for( iRepetition = 0; iRepetition < nRepeatCount; iRepetition++ )
    {
        SFCDataSource      *poDS;

        poDS = SFCOpenDataSource( pszProvider, pszDataSource, 
                                  pszProviderString );
        if( poDS == NULL )
            return 0;

/* -------------------------------------------------------------------- */
/*      If the action is to dump tables, do it now, without trying      */
/*      to open a table.                                                */
/* -------------------------------------------------------------------- */
        if( EQUAL(pszAction,"dumptables") )
        {
            SFCDumpTables( poDS );
            delete poDS;
            continue;
        }

/* -------------------------------------------------------------------- */
/*      If the action is to dump SF tables, do it now, without trying   */
/*      to open a table.                                                */
/* -------------------------------------------------------------------- */
        if( EQUAL(pszAction,"dumpsftables") )
        {
            SFCDumpSFTables( poDS );
            delete poDS;
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Open the requested table.                                       */
/* -------------------------------------------------------------------- */
        SFCTable      *poTable;
        if( pszCommand == NULL )
        {
            poTable = poDS->CreateSFCTable( pszTable );
            if( poTable == NULL )
            {
                printf( "Failed to open table %s.\n",  pszTable );
                return 1;
            }
        }
        else
        {
            poTable = poDS->Execute( pszCommand, poSpatialFilter, 
                                     eSpatialOperator );
            if( poTable == NULL )
            {
                printf( "Failed to execute %s.\n",  pszCommand );
                return 1;
            }
        }

/* -------------------------------------------------------------------- */
/*      Display a little bit of information about the opened table.     */
/* -------------------------------------------------------------------- */
        char      *pszSRS_WKT;

        pszSRS_WKT = poDS->GetWKTFromSRSId( poTable->GetSpatialRefID() );
        printf( "Spatial Reference System ID: %d (%s)\n", 
                poTable->GetSpatialRefID(), pszSRS_WKT );
        CoTaskMemFree( pszSRS_WKT );

        printf( "Geometry Type: %d\n", 
                poTable->GetGeometryType() );

/* -------------------------------------------------------------------- */
/*      Perform action on the table.                                    */
/* -------------------------------------------------------------------- */
        if( EQUAL(pszAction,"dumpgeom") )
            SFCDumpTableGeometry( poTable );
        else if( EQUALN(pszAction, "dumpfeat",8) )
            SFCDumpTableFeatures( poTable );
        else
            SFCDumpTableSchema( poTable );

        delete poTable;
        delete poDS;
    }

    return 0;
}

/************************************************************************/
/*                         SFCDumpTableSchema()                         */
/************************************************************************/

static void SFCDumpTableSchema( SFCTable * poTable )

{
    for( ULONG iColumn = 0; iColumn < poTable->GetColumnCount(); iColumn++ )
    {
        OledbSupWriteColumnInfo( stdout, poTable->m_pColumnInfo + iColumn );
    }
}

/************************************************************************/
/*                        SFCDumpTableGeometry()                        */
/************************************************************************/

static void SFCDumpTableGeometry( SFCTable * poTable )

{
    while( poTable->MoveNext() == S_OK )
    {
        OGRGeometry * poGeom;

        poGeom = poTable->GetOGRGeometry();
        poTable->ReleaseIUnknowns();

        if( poGeom == NULL )
        {
            printf( "Failed to reconstitute geometry!\n" );
            break;
        }
        else
        {
            poGeom->dumpReadable( stdout ); 
            OGRGeometryFactory::destroyGeometry( poGeom );
        }
    }
}

/************************************************************************/
/*                        SFCDumpTableFeatures()                        */
/************************************************************************/

static void SFCDumpTableFeatures( SFCTable * poTable )

{
    while( poTable->MoveNext() == S_OK )
    {
        OGRFeature * poFeature = poTable->GetOGRFeature();

        poTable->ReleaseIUnknowns();

        if( poFeature == NULL )
        {
            printf( "Failed to reconstitute feature!\n" );
            break;
        }
        else
        {
            poFeature->DumpReadable( stdout ); 
            OGRFeature::DestroyFeature( poFeature );
        }
    }
}

/************************************************************************/
/*                         SFCOpenDataSource()                          */
/*                                                                      */
/*      Open the named datastore with the named provider.               */
/************************************************************************/

static SFCDataSource * SFCOpenDataSource( const char * pszProvider, 
                                          const char * pszDataSource,
                                          const char * pszProviderString )

{
    SFCEnumerator      oEnumerator;

    if( FAILED(oEnumerator.Open()) )
    {
        printf( "Can't open ole db enumerator.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If any provider is OK, try them all.                            */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszProvider,"") || EQUAL(pszProvider,"any") )
    {
        SFCDataSource      *poDS;

        poDS = oEnumerator.OpenAny( pszDataSource );
        if( poDS != NULL )
        {
            printf( "Data source opened with %S provider.\n", 
                    oEnumerator.m_szName );
            return poDS;
        }

        printf( "Attempt to access datasource %s failed,\n"
                " all providers tried.\n", 
                pszDataSource );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Find the requested provider.                                    */
/* -------------------------------------------------------------------- */
    if( !oEnumerator.Find((char*) pszProvider) )
    {
        printf( "Can't find OLE DB provider `%s'.\n", pszProvider );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Attempt to initialize access to the data store.                 */
/* -------------------------------------------------------------------- */
    SFCDataSource *poDS;
    CDBPropSet    oPropSet(DBPROPSET_DBINIT);

    poDS = new SFCDataSource;
    oPropSet.AddProperty( DBPROP_INIT_DATASOURCE, pszDataSource );

    if( pszProviderString != NULL )
        oPropSet.AddProperty( DBPROP_INIT_PROVIDERSTRING, pszProviderString );

    if( FAILED(poDS->Open( oEnumerator, &oPropSet )) )
    {
        delete poDS;
        printf( "Attempt to access datasource %s failed.\n", 
                pszDataSource );
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          SFCDumpProviders()                          */
/*                                                                      */
/*      Display a list of providers to the user, marking those that     */
/*      claim OpenGIS compliance.                                       */
/************************************************************************/

static void SFCDumpProviders()

{
    SFCEnumerator      oEnum;

    printf( "Available OLE DB Providers\n" );
    printf( "==========================\n" );

    if( FAILED(oEnum.Open()) )
    {
        printf( "Failed to initialize SFCEnumerator.\n" );
        return;
    }

    while( oEnum.MoveNext() == S_OK )
    {
        printf( "%S: %S\n", 
                oEnum.m_szName, oEnum.m_szDescription );

        if( oEnum.IsOGISProvider() )
            printf( "    (OGISDataProvider)\n" );

        printf( "\n" );
    }
}

/************************************************************************/
/*                           SFCDumpTables()                            */
/************************************************************************/

static void SFCDumpTables( SFCDataSource * poDS )

{
    CSession           oSession;
    CTables            oTables;

    if( FAILED(oSession.Open(*poDS)) )
    {
        printf( "Failed to create CSession.\n" );
        return;
    }

    if( FAILED(oTables.Open(oSession)) )
    {
        printf( "Failed to create CTables rowset. \n" );
        return;
    }

    while( oTables.MoveNext() == S_OK )
    {
        printf( "%s: %s\n", 
                oTables.m_szName, oTables.m_szType );
    }
}

/************************************************************************/
/*                          SFCDumpSFTables()                           */
/************************************************************************/

static void SFCDumpSFTables( SFCDataSource * poDS )

{
    int                nSFCount = poDS->GetSFTableCount();

    printf( "SF Tables\n" );
    printf( "=========\n" );

    for( int i = 0; i < nSFCount; i++ )
    {
        printf( "%s\n", poDS->GetSFTableName( i ) );
    }
}

/************************************************************************/
/*                            main_thread()                             */
/*                                                                      */
/*      Entry point for threads.  Just calls main() again.              */
/************************************************************************/

static DWORD WINAPI main_thread( LPVOID )

{
    int      nRetVal = main( g_nArgc, g_papszArgv );
    int      nThreadId = (int) GetCurrentThreadId();

    CPLDebug( "OGR_SFC", "Thread %d complete.", nThreadId );

    return nRetVal;
}
