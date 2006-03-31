/******************************************************************************
 * $Id$
 *
 * Project: OpenGIS Simple Features Reference Implementation
 * Purpose: SFCDataSource implementation.
 * Author:  Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.10  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.9  2001/11/01 17:07:30  warmerda
 * lots of changes, including support for executing commands
 *
 * Revision 1.8  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.7  1999/09/07 12:06:13  warmerda
 * type casting warning fixed
 *
 * Revision 1.6  1999/06/26 05:26:49  warmerda
 * Separate out GetWKTFromSRSId static method for use of SFCTable
 *
 * Revision 1.5  1999/06/10 19:18:22  warmerda
 * added support for the spatial ref schema rowset
 *
 * Revision 1.4  1999/06/10 14:39:25  warmerda
 * Added use of OGIS Features Tables schema rowset
 *
 * Revision 1.3  1999/06/09 21:09:36  warmerda
 * updated docs
 *
 * Revision 1.2  1999/06/09 21:00:10  warmerda
 * added support for spatial table identification
 *
 * Revision 1.1  1999/06/08 03:50:25  warmerda
 * New
 *
 */

#include "sfcdatasource.h"
#include "sfctable.h"
#include "sfcschemarowsets.h"
#include "ogr_geometry.h"
#include "cpl_string.h"
#include "oledb_sup.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           SFCDataSource()                            */
/************************************************************************/

SFCDataSource::SFCDataSource()

{
    nSRInitialized = FALSE;
    papszSRName = NULL;
    bSessionEstablished = FALSE;
}

/************************************************************************/
/*                           ~SFCDataSource()                           */
/************************************************************************/

SFCDataSource::~SFCDataSource()

{
    CPLDebug( "SFC", "~SFCDataSource()" );
    CSLDestroy( papszSRName );
}

/************************************************************************/
/*                          EstablishSession()                          */
/************************************************************************/

int SFCDataSource::EstablishSession()

{
    if( !bSessionEstablished )
    {
        if( FAILED(oSession.Open(*this)) )
        {
            CPLDebug( "OGR_OLEDB", 
                      "Failed to open session on SFCDataSource!\n" );
        }
        else
            bSessionEstablished = TRUE;
    }

    return bSessionEstablished;
}

/************************************************************************/
/*                          GetSFTableCount()                           */
/************************************************************************/

/**
 * Get the number of spatial tables.
 *
 * See Reinitialize() method for details on the spatial table list.
 *
 * @return number of spatial tables.
 */

int SFCDataSource::GetSFTableCount()

{
    if( !nSRInitialized )
        Reinitialize();

    return CSLCount( papszSRName );
}

/************************************************************************/
/*                           GetSFTableName()                           */
/************************************************************************/

/**
 * Get the name of a spatial table.
 *
 * Fetches the name of the requested spatial table.  This name is suitable
 * for use with CreateSFCTable().  See Reinitialize() method for details
 * on the list of spatial tables.
 *
 * @param i value between 0 and GetSFTableCount()-1.
 *
 * @return pointer to internal table name.  Should not be modified, or
 * freed by the application.
 */

const char *SFCDataSource::GetSFTableName( int i )

{
    if( !nSRInitialized )
        Reinitialize();

    return( papszSRName[i] );
}

/************************************************************************/
/*                             AddSFTable()                             */
/************************************************************************/

void SFCDataSource::AddSFTable( const char * pszTableName )

{
    papszSRName = CSLAddString( papszSRName, pszTableName );
}

/************************************************************************/
/*                            Reinitialize()                            */
/************************************************************************/

/**
 * Reinitialize SFTable list.  
 *
 * This method can be called to trigger rebuilding of the list of spatial
 * tables returned by GetSFTableName().   Otherwise it is built on the first
 * request for SFTables, and cached - not reflecting additions or deletions.
 *
 * The list of spatial tables is intended to be a list of all tables in
 * this data source that have spatial information in them.  That is those
 * for which an SFCTable would be able to get geometry information from the
 * table.  Some data sources may not support any means of returning the list
 * of tables in which case none will be identified.  In this case the
 * user would have to enter a table name directly to use with
 * CreateSFCTable().
 *
 * This method will try to build the list of simple features tables by 
 * traversing the DBSCHEMA_OGIS_FEATURE_TABLES schema rowset.  If that doesn't
 * exist, it will traverse the DBSCHEMA_TABLES schema rowset, selecting only
 * those tables with OGIS style geometry columns apparent present. 
 */

void SFCDataSource::Reinitialize()

{
/* -------------------------------------------------------------------- */
/*      Reinitialize list.                                              */
/* -------------------------------------------------------------------- */
    nSRInitialized = TRUE;
    CSLDestroy( papszSRName );
    papszSRName = NULL;

/* -------------------------------------------------------------------- */
/*      Try the OGIS features tables schema rowset.  If that doesn't    */
/*      work, fallback to the regular tables schema rowset.             */
/* -------------------------------------------------------------------- */
    if( !UseOGISFeaturesTables() )
        UseTables();
}

/************************************************************************/
/*                             UseTables()                              */
/*                                                                      */
/*      This method attempts to construct a list of spatial tables      */
/*      from the general tables DBSCHEMA_TABLES rowset.                 */
/************************************************************************/

void SFCDataSource::UseTables()

{
    CTables            oTables;

    if( !EstablishSession() || FAILED(oTables.Open(oSession)) )
    {
        return;
    }

/* -------------------------------------------------------------------- */
/*      For now we use the most expensive approach to deciding if       */
/*      this table could be instantiated as a spatial table             */
/*      ... actually go ahead and try.  Eventually we should use the    */
/*      DBSCHEMA_COLUMNS or something else to try and do this more      */
/*      cheaply.                                                        */
/* -------------------------------------------------------------------- */
    while( oTables.MoveNext() == S_OK )
    {
        SFCTable      *poSFCTable;

        // skip system tables.
        if( !EQUAL(oTables.m_szType,"TABLE") )
            continue;

        poSFCTable = CreateSFCTable( oTables.m_szName );
        if( poSFCTable == NULL )
            continue;

        if( poSFCTable->HasGeometry() )
            AddSFTable( oTables.m_szName );

        delete poSFCTable;
    }
}

/************************************************************************/
/*                       UseOGISFeaturesTables()                        */
/*                                                                      */
/*      Construct the list of spatial tables from the OGISFeatures      */
/*      schema rowset.                                                  */
/************************************************************************/

int SFCDataSource::UseOGISFeaturesTables()

{
    COGISFeatureTables oTables;

/* -------------------------------------------------------------------- */
/*      If this provider doesn't support this schema rowset, we         */
/*      silently return without making a big fuss.  The caller will     */
/*      try using the regular tables schema rowset instead.             */
/* -------------------------------------------------------------------- */
    if( !EstablishSession() || FAILED(oTables.Open(oSession)) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      For now we use the most expensive approach to deciding if       */
/*      this table could be instantiated as a spatial table             */
/*      ... actually go ahead and try.  Eventually we should use the    */
/*      DBSCHEMA_COLUMNS or something else to try and do this more      */
/*      cheaply.                                                        */
/* -------------------------------------------------------------------- */
    while( oTables.MoveNext() == S_OK )
    {
        AddSFTable( oTables.m_szName );
    }

    return TRUE;
}

/************************************************************************/
/*                           CreateSFCTable()                           */
/************************************************************************/

/** 
 * Open a spatial table.
 *
 * This method creates an instance of an SFCTable to access a spatial
 * table.  On failure NULL is returned; however, there is currently no
 * way to interogate the error that caused the failure. 
 *
 * @param pszTableName the name of the spatial table.  Generally selected
 * from the list of tables exposed by GetSFTableName().
 *
 * @param poFilterGeometry the geometry to use as a spatial filter, or more
 * often NULL to get all features from the spatial table.  (NOT IMPLEMENTED)
 *
 * @param eOperator One of the geometry operators (DBPROP_OGIS_*) from 
 * oledbgis.h.  Defaults to DBPROP_ENVELOPE_INTERSECTS.
 *
 * @return a pointer to the new spatial table object, or NULL on failure. 
 */

SFCTable *SFCDataSource::CreateSFCTable( const char * pszTableName, 
                                         OGRGeometry * poFilterGeometry,
                                         DBPROPOGISENUM eOperator )

{
    SFCTable      *poTable;
    DBID          idTable;
    USES_CONVERSION;

    if( !EstablishSession() )
        return NULL;
        
    poTable = new SFCTable();
    
    idTable.eKind           = DBKIND_NAME;
    idTable.uName.pwszName  = (LPOLESTR)T2COLE(pszTableName);

    if( FAILED(poTable->Open(oSession, idTable)) )
    {
        CPLDebug( "SFCDUMP", "poTable->Open(%s) failed.", pszTableName );
        delete poTable;
        return NULL;
    }

    poTable->SetTableName( pszTableName );
    poTable->ReadSchemaInfo( this, &oSession );

    return poTable;
}

/************************************************************************/
/*                              Execute()                               */
/*                                                                      */
/*      Execute an SQL command, with spatial constraints.               */
/************************************************************************/

SFCTable *SFCDataSource::Execute(const char *pszCommand,
                                 OGRGeometry * poFilterGeometry,
                                 DBPROPOGISENUM eOperator )

{
    HRESULT hr;

    if( !EstablishSession() )
        return NULL;

    if( poFilterGeometry == NULL )
        return Execute( pszCommand );

/* -------------------------------------------------------------------- */
/*      Create a command.                                               */
/* -------------------------------------------------------------------- */
    CComPtr<IDBCreateCommand> spCC;
    CComPtr<ICommand> spCommand;

    hr = oSession.m_spOpenRowset->QueryInterface(IID_IDBCreateCommand, 
                                                 (void ** )&spCC);
    if( !SUCCEEDED(hr) )
        return NULL;

    hr = spCC->CreateCommand( NULL, IID_ICommand, (IUnknown **) &spCommand );
    if( !SUCCEEDED(hr) )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Set command text.                                               */
/* -------------------------------------------------------------------- */
    CComPtr<ICommandText> spCText;
    LPOLESTR  pwszCommand;

    hr = spCommand->QueryInterface(IID_ICommandText, (void ** )&spCText);
    if( !SUCCEEDED(hr) )
        return NULL;
    
    AnsiToUnicode( pszCommand, &pwszCommand );
    hr = spCText->SetCommandText( DBGUID_DEFAULT, pwszCommand );
    if( !SUCCEEDED(hr) )
        return NULL;

    CoTaskMemFree( pwszCommand );
    
/* -------------------------------------------------------------------- */
/*      Setup the bindings for the parameters.                          */
/* -------------------------------------------------------------------- */
    DBBINDING          rgBindings[3];
#define STR_SIZE 512
#define BUF_SIZE sizeof(VARIANT)+STR_SIZE+4
    
    memset( rgBindings, 0, sizeof(rgBindings) );

    rgBindings[0].iOrdinal = 1;
    rgBindings[0].obValue = 0;
    rgBindings[0].obLength = 0;
    rgBindings[0].dwPart = DBPART_VALUE;
    rgBindings[0].dwMemOwner = DBMEMOWNER_CLIENTOWNED;
    rgBindings[0].eParamIO = DBPARAMIO_INPUT;
    rgBindings[0].cbMaxLen = sizeof(VARIANT);
    rgBindings[0].wType = DBTYPE_VARIANT;

    rgBindings[1].iOrdinal = 2;
    rgBindings[1].obValue = sizeof(VARIANT);
    rgBindings[1].dwPart = DBPART_VALUE;
    rgBindings[1].dwMemOwner = DBMEMOWNER_CLIENTOWNED;
    rgBindings[1].eParamIO = DBPARAMIO_INPUT;
    rgBindings[1].cbMaxLen = 4;
    rgBindings[1].wType = DBTYPE_UI4;

    rgBindings[2].iOrdinal = 3;
    rgBindings[2].obValue = sizeof(VARIANT)+4;
    rgBindings[2].dwPart = DBPART_VALUE;
    rgBindings[2].dwMemOwner = DBMEMOWNER_CLIENTOWNED;
    rgBindings[2].eParamIO = DBPARAMIO_INPUT;
    rgBindings[2].cbMaxLen = STR_SIZE;
    rgBindings[2].wType = DBTYPE_WSTR;

/* -------------------------------------------------------------------- */
/*      Create a parameter accessor                                     */
/* -------------------------------------------------------------------- */
    CComPtr<IAccessor> spCAccessor;
    HACCESSOR          hAccessor;
    DBBINDSTATUS       rgStatus[3];
    DBPARAMS           oParams;

    hr = spCommand->QueryInterface(IID_IAccessor, (void ** )&spCAccessor);
    if( !SUCCEEDED(hr) )
        return NULL;
    
    hr = spCAccessor->CreateAccessor( DBACCESSOR_PARAMETERDATA, 3, 
                                      rgBindings, BUF_SIZE, 
                                      &hAccessor, rgStatus );
    if( !SUCCEEDED(hr) )
        return NULL;
                               
/* -------------------------------------------------------------------- */
/*      Setup buffer with parameters.                                   */
/* -------------------------------------------------------------------- */
    int nGeomSize = poFilterGeometry->WkbSize();
    unsigned char      buffer[BUF_SIZE];
    VARIANT            *pVariant;
    SAFEARRAY          *pArray;
    void               *pGeomData;
    SAFEARRAYBOUND     saBound[1];

    *((int *) (buffer + rgBindings[1].obValue)) = eOperator;
    lstrcpyW( (unsigned short *) (buffer + rgBindings[2].obValue), 
              L"OGIS_GEOMETRY" );

    saBound[0].lLbound = 0;
    saBound[0].cElements = nGeomSize;
    pArray = SafeArrayCreate( VT_UI1, 1, saBound );
    SafeArrayAccessData( pArray, &pGeomData );
    poFilterGeometry->exportToWkb( wkbNDR, (unsigned char *) pGeomData );

    pVariant = (VARIANT *) (buffer + rgBindings[0].obValue);
    VariantInit( pVariant );
    pVariant->vt = VT_UI1 | VT_ARRAY;
    pVariant->parray = pArray;
    
    oParams.pData = buffer;
    oParams.hAccessor = hAccessor;
    oParams.cParamSets = 1;
       
/* -------------------------------------------------------------------- */
/*      Execute command.                                                */
/* -------------------------------------------------------------------- */
    LONG      cRowsAffected;
    IRowset *pIRowset;

    hr = spCommand->Execute( NULL, IID_IRowset, &oParams, &cRowsAffected, 
                             (IUnknown **) &pIRowset );
    if( !SUCCEEDED(hr) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the rowset.                                                */
/* -------------------------------------------------------------------- */
    SFCTable      *poTable;

    poTable = new SFCTable();
    
    if( FAILED(poTable->OpenFromRowset(pIRowset)) )
    {
        CPLDebug( "SFCDUMP", "poTable->OpenFromRowset(%s) failed.", 
                  pszCommand  );
        delete poTable;
        return NULL;
    }

    poTable->SetTableName( "Command" );
    poTable->ReadSchemaInfo( this, &oSession );

/* -------------------------------------------------------------------- */
/*      Release the accessor.                                           */
/* -------------------------------------------------------------------- */
    VariantClear( pVariant );
    hr = spCAccessor->ReleaseAccessor( hAccessor, NULL );

    return poTable;
}

/************************************************************************/
/*                              Execute()                               */
/*                                                                      */
/*      Execute a command, possibly with parameters.                    */
/************************************************************************/

SFCTable *SFCDataSource::Execute(const char *pszCommand,
                                 DBPROPSET* pPropSet,
                                 DBPARAMS *pParams )
{
    HRESULT hr;

    CPLDebug( "OGR_SFC", "Execute(%S)", pszCommand );

    if( !EstablishSession() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a command.                                               */
/* -------------------------------------------------------------------- */
    CComPtr<IDBCreateCommand> spCC;
    CComPtr<ICommand> spCommand;

    hr = oSession.m_spOpenRowset->QueryInterface(IID_IDBCreateCommand, 
                                                 (void ** )&spCC);
    if( !SUCCEEDED(hr) )
        return NULL;

    hr = spCC->CreateCommand( NULL, IID_ICommand, (IUnknown **) &spCommand );
    if( !SUCCEEDED(hr) )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Set command text.                                               */
/* -------------------------------------------------------------------- */
    CComPtr<ICommandText> spCText;
    LPOLESTR  pwszCommand;

    hr = spCommand->QueryInterface(IID_ICommandText, (void ** )&spCText);
    if( !SUCCEEDED(hr) )
        return NULL;

    
    AnsiToUnicode( pszCommand, &pwszCommand );
    hr = spCText->SetCommandText( DBGUID_DEFAULT, pwszCommand );
    if( !SUCCEEDED(hr) )
        return NULL;

    CoTaskMemFree( pwszCommand );
    
/* -------------------------------------------------------------------- */
/*      Execute command.                                                */
/* -------------------------------------------------------------------- */
    LONG      cRowsAffected;
    IRowset *pIRowset;

    hr = spCommand->Execute( NULL, IID_IRowset, pParams, &cRowsAffected, 
                             (IUnknown **) &pIRowset );
    if( !SUCCEEDED(hr) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the rowset.                                                */
/* -------------------------------------------------------------------- */
    SFCTable      *poTable;

    poTable = new SFCTable();
    
    if( FAILED(poTable->OpenFromRowset(pIRowset)) )
    {
        CPLDebug( "SFCDUMP", "poTable->OpenFromRowset(%s) failed.", 
                  pszCommand  );
        delete poTable;
        return NULL;
    }

    poTable->SetTableName( "Command" );
    poTable->ReadSchemaInfo( this, &oSession );

    return poTable;
}

/************************************************************************/
/*                          GetWKTFromSRSId()                           */
/************************************************************************/

/**
 * Get WKT format from spatial ref system id.
 *
 * Read the spatial reference system system schema rowset to translate
 * a data source specific SRS ID into it's well known text format 
 * equivelent.  The returned string should be freed with CoTaskMemFree()
 * when no longer needed.
 *
 * A return value of "Unknown" will indicate that the SRS ID was not
 * successfully translated.
 *
 * @param nSRS_ID the id for the SRS to fetch, as returned from the 
 * SFCTable::GetSpatialRefID() for instance.
 *
 * @return string representation of SRS. 
 */

char * SFCDataSource::GetWKTFromSRSId( int nSRS_ID )

{
    if( !EstablishSession() )
        return NULL;

    return GetWKTFromSRSId( &oSession, nSRS_ID );
}

/************************************************************************/
/*                          GetWKTFromSRSId()                           */
/*                                                                      */
/*      This undocumented version is implemented statically, so that    */
/*      code (such as SFCTable) which has a CSession can still call     */
/*      it without an SFCDataSource instance.                           */
/************************************************************************/

char * SFCDataSource::GetWKTFromSRSId( CSession * poSession, int nSRS_ID )

{
    COGISSpatialRefSystemsTable oTable;
    const char    *pszReturnString = "(Unknown)";

/* -------------------------------------------------------------------- */
/*      If this provider doesn't support this schema rowset, we         */
/*      silently return without making a big fuss.  The caller will     */
/*      try using the regular tables schema rowset instead.             */
/* -------------------------------------------------------------------- */
    if( FAILED(oTable.Open(*poSession)) )
    {
        goto ReturnValue;
    }

/* -------------------------------------------------------------------- */
/*      Search for requested id.                                        */
/* -------------------------------------------------------------------- */
    while( oTable.MoveNext() == S_OK )
    {
        if( oTable.m_nSRS_ID == (ULONG) nSRS_ID )
        {
            pszReturnString = oTable.m_szSpatialRefSystemWKT;
            goto ReturnValue;
        }
    }

/* -------------------------------------------------------------------- */
/*      Allocate a copy of the string for the application.              */
/* -------------------------------------------------------------------- */
  ReturnValue:
    char      *pszAppString;

    pszAppString = (char *) CoTaskMemAlloc( strlen(pszReturnString) + 1 );
    strcpy( pszAppString, pszReturnString );

    return pszAppString;
}
