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

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf("Usage: sfcdump [-provider provider_clsid_alias] [-ds datasource]\n"
           "              [-table tablename] [-column geom_column_name]\n"
           "              [-action {dumpgeom,dumpschema}] -quiet\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

void main( int nArgc, char ** papszArgv )
{
    SFCDataSource      *poDS;
    const char *pszProvider = "Microsoft.Jet.OLEDB.3.51";
    const char *pszDataSource = "f:\\opengis\\SFData\\World.mdb";
    const char *pszTable = "worldmif_geometry";
   
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

        else if( iArg < nArgc-1 && stricmp( papszArgv[iArg],"-action") == 0 )
        {
            pszAction = papszArgv[++iArg];
        }
#endif
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
/*      Report criteria.                                                */
/* -------------------------------------------------------------------- */
    if( bVerbose )
    {
        printf( "Provider:    %s\n", pszProvider );
        printf( "Data Source: %s\n", pszDataSource );
        printf( "Table:       %s\n", pszTable );
    }

/* -------------------------------------------------------------------- */
/*      Try to find the requested provider.  Issue a warning, if it     */
/*      is not an OGISDataProvider.                                     */
/* -------------------------------------------------------------------- */
    {
        SFCEnumerator      oEnumerator;

        if( FAILED(oEnumerator.Open()) )
        {
            printf( "Can't open ole db enumerator.\n" );
            exit( 1 );
        }

        if( !oEnumerator.Find((char*) pszProvider) )
        {
            printf( "Can't find OLE DB provider `%s'.\n", pszProvider );
            exit( 1 );
        }

        if( bVerbose && !oEnumerator.IsOGISProvider() )
        {
            printf( "Warning: Provider found, but does not advertise as "
                    "an OGISDataProvider.\n" );
            printf( "         Using anyways.\n" );
        }

/* -------------------------------------------------------------------- */
/*      Attempt to initialize access to the data store.                 */
/* -------------------------------------------------------------------- */
        poDS = new SFCDataSource;

        if( FAILED(poDS->Open( oEnumerator, pszDataSource  )) )
        {
            printf( "Attempt to access datasource %s failed.\n", 
                    pszDataSource );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Establish a session.                                            */
/* -------------------------------------------------------------------- */
    {
        CSession      oSession;

        if( FAILED( oSession.Open(*poDS) ) )
        {
            printf( "Attempt to establish a session failed.\n" );
            exit( 1 );
        }

        delete poDS;
        poDS = NULL;

/* -------------------------------------------------------------------- */
/*      Attempt to initialize access to rowset.                         */
/* -------------------------------------------------------------------- */
        SFCTable            oTable;

        if( FAILED(oTable.Open( oSession, pszTable )) )
        {
            printf( "Attempt to open table %s failed.\n", pszTable );
            exit( 1 );
        }

/* -------------------------------------------------------------------- */
/*      Dump a little info about each column.                           */
/* -------------------------------------------------------------------- */
        int      iColumn;

        for( iColumn = 1; iColumn < oTable.GetColumnCount(); iColumn++ )
        {
            OledbSupWriteColumnInfo( stdout, oTable.m_pColumnInfo + iColumn );
        }

/* -------------------------------------------------------------------- */
/*      Read records, and dump easy columns.                            */
/* -------------------------------------------------------------------- */
        HRESULT    hr;

        while( !FAILED((hr = oTable.MoveNext())) )
        {
            printf( "\nNew Record\n" );
            for( iColumn = 1; iColumn < oTable.GetColumnCount(); iColumn++ )
            {
                DBTYPE      nType;

                oTable.GetColumnType( iColumn, &nType );

                switch( nType )
                {
                    case DBTYPE_R8:
                    {
                        double      dfValue;

                        oTable.GetValue( iColumn, &dfValue );
                        printf( "    %S = %g\n", 
                                oTable.GetColumnName(iColumn), 
                                dfValue );
                    }
                    break;

                    case DBTYPE_I4:
                    {
                        int      nValue;

                        oTable.GetValue( iColumn, &nValue );
                        printf( "    %S = %d\n", 
                                oTable.GetColumnName(iColumn), 
                                nValue );
                    }
                    break;

                    case DBTYPE_IUNKNOWN:
                    {
                        IUnknown      *pIUnknown;

                        oTable.GetValue( iColumn, &pIUnknown );
                        printf( "    %S = %p\n", 
                                oTable.GetColumnName(iColumn), 
                                pIUnknown );
                    }
                    break;

                    default:
                        printf( "    %S = (unhandled type)\n", 
                                oTable.GetColumnName(iColumn) );
                }
            } /* next field */

            // fetch the geometry data.
            OGRGeometry * poGeom;

            poGeom = oTable.GetOGRGeometry();
            if( poGeom == NULL )
                printf( "Failed to reconstitute geometry!\n" );

            if( bVerbose )
                poGeom->dumpReadable( stdout ); 

            if( poGeom != NULL )
                delete poGeom;

            oTable.ReleaseIUnknowns();

        } /* next record */
    }

    OleSupUninitialize();
}



