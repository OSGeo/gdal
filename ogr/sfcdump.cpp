/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Mainline for dumping stuff from an SFCOM OLEDB provider.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  1999/06/08 17:51:16  warmerda
 * cleaned up using SFC classes
 *
 * Revision 1.1  1999/06/08 15:41:55  warmerda
 * New
 *
 */

#define INITGUID
#define DBINITCONSTANTS

#include "oledb_sup.h"
#include "sfcenumerator.h"
#include "sfcdatasource.h"
#include "sfctable.h"
#include "ogr_geometry.h"
#include "cpl_conv.h"

#ifdef notdef
#include "ogr_geometry.h"

#include "geometryidl.h"
#include "spatialreferenceidl.h"

// Get various classid.
#include "msdaguid.h"
//#include "MSjetoledb.h"
#include "sfclsid.h"
#include "sfiiddef.h"
#endif

static int      bVerbose = TRUE;

static void SFCDumpProviders();
static SFCDataSource * SFCOpenDataSource( const char * pszProvider, 
                                          const char * pszDataStore );
static void SFCDumpTableSchema( SFCTable * );
static void SFCDumpTableGeometry( SFCTable * );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf("Usage: sfcdump [-provider provider_clsid_alias] [-ds datasource]\n"
           "              [-table tablename] [-column geom_column_name]\n"
           "              [-action {dumpprov,dumpgeom,dumpschema}] -quiet\n" );

    OleSupUninitialize();
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

void main( int nArgc, char ** papszArgv )
{
    const char *pszProvider = "Microsoft.Jet.OLEDB.3.51";
    const char *pszDataSource = "f:\\opengis\\SFData\\World.mdb";
    const char *pszTable = "worldmif_geometry";
    const char *pszAction = "dumpgeom";
   
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
            pszProvider = papszArgv[iArg];
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-ds") == 0 )
        {
            pszDataSource = papszArgv[++iArg];
        }

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-table") == 0 )
        {
            pszTable = papszArgv[++iArg];
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
        goto CleanupAndExit;
    }

/* -------------------------------------------------------------------- */
/*      Access the requested data source.                               */
/* -------------------------------------------------------------------- */
    SFCDataSource      *poDS;

    poDS = SFCOpenDataSource( pszProvider, pszDataSource );
    if( poDS == NULL )
        goto CleanupAndExit;

/* -------------------------------------------------------------------- */
/*      Open the requested table.                                       */
/* -------------------------------------------------------------------- */
    SFCTable      *poTable;

    poTable = poDS->CreateSFCTable( pszTable );

    delete poDS;

    if( poTable == NULL )
    {
        printf( "Failed to open table %s.\n",  pszTable );
        goto CleanupAndExit;
    }

/* -------------------------------------------------------------------- */
/*      Perform action on the table.                                    */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszAction,"dumpgeom") )
        SFCDumpTableGeometry( poTable );
    else
        SFCDumpTableSchema( poTable );

    delete poTable;

/* -------------------------------------------------------------------- */
/*      Cleanup and exit.                                               */
/* -------------------------------------------------------------------- */
  CleanupAndExit:
    OleSupUninitialize();
}

/************************************************************************/
/*                         SFCDumpTableSchema()                         */
/************************************************************************/

static void SFCDumpTableSchema( SFCTable * poTable )

{
    for( int iColumn = 0; iColumn < poTable->GetColumnCount(); iColumn++ )
    {
        OledbSupWriteColumnInfo( stdout, poTable->m_pColumnInfo + iColumn );
    }
}

/************************************************************************/
/*                        SFCDumpTableGeometry()                        */
/************************************************************************/

static void SFCDumpTableGeometry( SFCTable * poTable )

{
    HRESULT      hr;

    while( !FAILED((hr = poTable->MoveNext())) )
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
            delete poGeom;
        }
    }
}

/************************************************************************/
/*                         SFCOpenDataSource()                          */
/*                                                                      */
/*      Open the named datastore with the named provider.               */
/************************************************************************/

static SFCDataSource * SFCOpenDataSource( const char * pszProvider, 
                                          const char * pszDataSource )

{
    SFCEnumerator      oEnumerator;

    if( FAILED(oEnumerator.Open()) )
    {
        printf( "Can't open ole db enumerator.\n" );
        return NULL;
    }

    if( !oEnumerator.Find((char*) pszProvider) )
    {
        printf( "Can't find OLE DB provider `%s'.\n", pszProvider );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Attempt to initialize access to the data store.                 */
/* -------------------------------------------------------------------- */
    SFCDataSource *poDS;

    poDS = new SFCDataSource;

    if( FAILED(poDS->Open( oEnumerator, pszDataSource  )) )
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

