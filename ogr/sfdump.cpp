#define INITGUID
#define DBINITCONSTANTS

#include "oledb_sup.h"
#include "ogr_geometry.h"

// Get various classid.
#include "msdaguid.h"
#include "MSjetoledb.h"
#include "sfclsid.h"

const IID IID_IGeometry = {0x6A124031,0xFE38,0x11d0,{0xBE,0xCE,0x00,0x80,0x5F,0x7C,0x42,0x68}};

static HRESULT SFDumpGeomColumn( IOpenRowset*, const char *, const char * );
static HRESULT SFDumpSchema( IOpenRowset*, const char * );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: sfdump [-provider provider_clsid_alias] [-ds datasource]\n"
            "              [-table tablename] [-column geom_column_name]\n"
            "              [-action {dumpgeom,dumpschema}]\n" );
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
    else
        fprintf( stdout, "Geometry Column Ordinal = %d\n", iColOrdinal );
   
/* -------------------------------------------------------------------- */
/*      For now we just read through, counting records to verify        */
/*      things are working.                                             */
/* -------------------------------------------------------------------- */
    int      nRecordCount = 0;

    while( oTable.GetNextRecord( &hr ) )
    {
        BYTE      *pabyData;
        int       nSize, nDBType, nStatus;
        OGRGeometry * poGeom;

        pabyData = (BYTE *) 
            oTable.GetFieldData( iColOrdinal, &nDBType, &nStatus, &nSize );

/* -------------------------------------------------------------------- */
/*      Handle raw binary data using our own client side classes.       */
/* -------------------------------------------------------------------- */
        if( nDBType == DBTYPE_BYTES )
        {
            if( OGRGeometryFactory::createFromWkb( pabyData, &poGeom, nSize )
                == OGRERR_NONE )
            {
                poGeom->dumpReadable( stdout );
                delete poGeom;
            }
            else 
            {
                fprintf( stderr, "Unable to decode record %d\n", 
                         nRecordCount );
            }
        }

/* -------------------------------------------------------------------- */
/*      Try to instantiate persistent objects.                          */
/* -------------------------------------------------------------------- */
        else if( nDBType == DBTYPE_IUNKNOWN )
        {
            IUnknown * pIUnknown = *((IUnknown **) pabyData);
            IStream *  pIStream = NULL;
            
            hr = pIUnknown->QueryInterface( IID_IStream,
                                            (void**)&pIStream );
            if( FAILED(hr) )
            {
                printf( "After QueryInterface hr=%d, pInterface=%p\n", 
                        hr, pIStream );
                if( FAILED(hr) )
                    DumpErrorHResult( hr, "QueryInterfacE" );
            }
            else
            {
                STATSTG      oStreamStat;
                BYTE         *pabyData = NULL;
                ULONG        nSize;

                hr = pIStream->Stat( &oStreamStat, STATFLAG_NONAME );
                if( FAILED(hr) )
                {
                    DumpErrorHResult( hr, "IStream::Stat()" );
                    break;
                }
                
                nSize = oStreamStat.cbSize.LowPart;
                printf( "Storage length = %d\n", nSize );

                pabyData = (BYTE *) CoTaskMemAlloc( nSize );
                hr = pIStream->Read( pabyData, nSize, NULL );
                if( FAILED(hr) )
                {
                    DumpErrorHResult( hr, "IStream::Read()" );
                }
                else
                {
                    OGRGeometry      *poGeom;

                    if( OGRGeometryFactory::createFromWkb( pabyData, &poGeom, 
                                                           nSize )
                        == OGRERR_NONE )
                    {
                        poGeom->dumpReadable( stdout );
                        delete poGeom;
                    }
                    else 
                    {
                        fprintf( stderr, "Unable to decode record %d\n", 
                                 nRecordCount );
                    }
                }
                CoTaskMemFree( pabyData );
                pIStream->Release();
            }
        }

        else
        {
            fprintf( stdout, 
                     "Geometry column not of appropriate type: %d\n",
                     nDBType );
            break;
        }

        nRecordCount++;
    }

    printf( "Read %d records.\n", nRecordCount );

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

