/******************************************************************************
 * $Id$
 *
 * Project: OpenGIS Simple Features Reference Implementation
 * Purpose: SFCDataSource implementation.
 * Author:  Frank Warmerdam, warmerda@home.com
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

/************************************************************************/
/*                           SFCDataSource()                            */
/************************************************************************/

SFCDataSource::SFCDataSource()

{
    nSRInitialized = FALSE;
    papszSRName = NULL;
}

/************************************************************************/
/*                           ~SFCDataSource()                           */
/************************************************************************/

SFCDataSource::~SFCDataSource()

{
    CSLDestroy( papszSRName );
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
    CSession           oSession;
    CTables            oTables;

    if( FAILED(oSession.Open(*this)) )
    {
        return;
    }

    if( FAILED(oTables.Open(oSession)) )
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
    CSession           oSession;
    COGISFeatureTables oTables;

    if( FAILED(oSession.Open(*this)) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If this provider doesn't support this schema rowset, we         */
/*      silently return without making a big fuss.  The caller will     */
/*      try using the regular tables schema rowset instead.             */
/* -------------------------------------------------------------------- */
    if( FAILED(oTables.Open(oSession)) )
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
/*                                                                      */
/*      The CSession is temporary, but the table is returned.  We       */
/*      need a way of returning errors!                                 */
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
 * @param pszFilterOperator the name of the spatial operator to apply.  
 * (NOT IMPLEMENTED OR DEFINED). 
 *
 * @return a pointer to the new spatial table object, or NULL on failure. 
 */

SFCTable *SFCDataSource::CreateSFCTable( const char * pszTableName, 
                                         OGRGeometry * poFilterGeometry,
                                         const char * pszFilterOperator )

{
    CSession      oSession;
    SFCTable      *poTable;

    if( FAILED(oSession.Open(*this)) )
        return NULL;

    poTable = new SFCTable();

    if( FAILED(poTable->Open(oSession, pszTableName)) )
    {
        delete poTable;
        return NULL;
    }

    poTable->SetTableName( pszTableName );
    poTable->ReadSchemaInfo( this );

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
    CSession      oSession;

    if( FAILED(oSession.Open(*this)) )
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
