#define INITGUID
#define DBINITCONSTANTS

#include "oledb_sup.h"

#include "msdaguid.h"
#include "MSjetoledb.h"

static HRESULT SFDumpGeomColumn( IOpenRowset*, LPWSTR, const char * );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

void main( int argc, char ** argv )
{
   REFCLSID    hProviderCLSID = CLSID_JETOLEDB_3_51;
   LPWSTR      pwszDataSource = L"f:\\opengis\\SFCOMTestData\\World.mdb";
   LPWSTR      pwszTable = L"worldmif_geometry";
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
/*      Process commandline switches ... nothing for now.  How do we    */
/*      convert regular strings into LPWSTR's?                          */
/* -------------------------------------------------------------------- */
   /* .... */

/* -------------------------------------------------------------------- */
/*      Open the data provider source (for instance select JET, and     */
/*      access an MDB file.                                             */
/* -------------------------------------------------------------------- */
   hr = OledbSupGetDataSource( hProviderCLSID, pwszDataSource, 
                               &pIOpenRowset );
   
   if( FAILED( hr ) )
      goto error;

   fprintf( stdout, "Acquired data source %S.\n", pwszDataSource );

/* -------------------------------------------------------------------- */
/*      Ask for a dump of a particular column.                          */
/* -------------------------------------------------------------------- */
   SFDumpGeomColumn( pIOpenRowset, pwszTable, pszGeomColumn );

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
                                 LPWSTR pwszTableName, 
                                 const char *pszColumnName )

{
   HRESULT            hr;
   OledbSupRowset     oTable;
   int                iColOrdinal;

/* -------------------------------------------------------------------- */
/*      Open the table.                                                 */
/* -------------------------------------------------------------------- */
   hr = oTable.OpenTable( pIOpenRowset, pwszTableName );
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
      nRecordCount++;
   }

   printf( "Read %d records.\n", nRecordCount );

   return ResultFromScode( S_OK );
}

