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
 * Revision 1.2  1999/06/09 21:00:10  warmerda
 * added support for spatial table identification
 *
 * Revision 1.1  1999/06/08 03:50:25  warmerda
 * New
 *
 */

#include "sfcdatasource.h"
#include "ogr_geometry.h"
#include "sfctable.h"
#include "cpl_string.h"

#include <atldbsch.h>

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

int SFCDataSource::GetSFTableCount()

{
    if( !nSRInitialized )
        Reinitialize();

    return CSLCount( papszSRName );
}

/************************************************************************/
/*                           GetSFTableName()                           */
/************************************************************************/

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
 * This method can be called to trigger rebuilding of the list of tables
 * returned by GetSFCTableName().   Otherwise it is built on the first
 * request for SFTables, and cached - not reflecting additions or deletions.
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
/*      For now we just go straight to the table of tables.             */
/* -------------------------------------------------------------------- */
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
        printf( "Failed to create CSession.\n" );
        return;
    }

    if( FAILED(oTables.Open(oSession)) )
    {
        printf( "Failed to create CTables rowset. \n" );
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

    return poTable;
}

