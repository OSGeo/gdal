#define INITGUID
#define DBINITCONSTANTS

#include "oledb_sup.h"
#include "ogr_geometry.h"

// Get various classid.
#include "msdaguid.h"
#include "MSjetoledb.h"
#include "sfclsid.h"

static HRESULT SFDumpGeomColumn( IOpenRowset*, const char *, const char * );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: sfdump [-provider provider_clsid_alias] [-ds datasource]\n"
            "              [-table tablename] [-column geom_column_name]\n" );
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
    const char *pszGeomColumn = "WKB_GEOMETRY";
    HRESULT     hr;
    IOpenRowset *pIOpenRowset = NULL;
   
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
    SFDumpGeomColumn( pIOpenRowset, pszTable, pszGeomColumn );

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
  error:    

    if( pIOpenRowset != NULL )
        pIOpenRowset->Release();

    OleSupUninitialize();
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
    HRESULT            hr;
    OledbSupRowset     oTable;
    int                iColOrdinal;

/* -------------------------------------------------------------------- */
/*      Open the table.                                                 */
/* -------------------------------------------------------------------- */
    hr = oTable.OpenTable( pIOpenRowset, pszTableName );
    if( FAILED( hr ) )
        return hr;

/* -------------------------------------------------------------------- */
/*      Find the ordinal of the requested column.                       */
/* -------------------------------------------------------------------- */
    iColOrdinal = oTable.GetColumnOrdinal( pszColumnName );
    if( iColOrdinal == -1 )
    {
        fprintf( stderr, "Unable to find column `%s'.\n", pszColumnName );
        return ResultFromScode( S_FALSE );
    }
   
/* -------------------------------------------------------------------- */
/*      For now we just read through, counting records to verify        */
/*      things are working.                                             */
/* -------------------------------------------------------------------- */
    int      nRecordCount = 0;

    while( oTable.GetNextRecord( &hr ) )
    {
        BYTE      *pabyData;
        int       nSize, nDBType;
        OGRGeometry * poGeom;


        pabyData = (BYTE *) 
            oTable.GetFieldData( iColOrdinal, &nDBType, NULL, &nSize );

        assert( nDBType == DBTYPE_BYTES );

        if( OGRGeometryFactory::createFromWkb( pabyData, &poGeom, nSize )
            == OGRERR_NONE )
        {
            poGeom->dumpReadable( stdout );
        }
        else
        {
            fprintf( stderr, "Unable to decode record %d\n", nRecordCount );
        }

        nRecordCount++;
    }

    printf( "Read %d records.\n", nRecordCount );

    return ResultFromScode( S_OK );
}

