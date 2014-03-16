/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGRASSLayer class.
 * Author:   Radim Blazek, radim.blazek@gmail.com 
 *
 ******************************************************************************
 * Copyright (c) 2005, Radim Blazek <radim.blazek@gmail.com>
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <signal.h>
#include "ogrgrass.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRGRASSLayer()                            */
/************************************************************************/
OGRGRASSLayer::OGRGRASSLayer( int layerIndex,  struct Map_info * map )
{
    CPLDebug ( "GRASS", "OGRGRASSLayer::OGRGRASSLayer layerIndex = %d", layerIndex );
    
    iLayerIndex = layerIndex;
    poMap = map; 
    poSRS = NULL;
    iNextId = 0;
    poPoints = Vect_new_line_struct();
    poCats = Vect_new_cats_struct();
    pszQuery = NULL;
    paQueryMatch = NULL;
    paSpatialMatch = NULL;

    iLayer = Vect_cidx_get_field_number ( poMap, iLayerIndex);
    CPLDebug ( "GRASS", "iLayer = %d", iLayer );
    
    poLink = Vect_get_field ( poMap, iLayer ); // May be NULL if not defined

    // Layer name
    if ( poLink && poLink->name )
    {
	pszName = CPLStrdup( poLink->name );	
    }
    else
    {	
	char buf[20]; 
	sprintf ( buf, "%d", iLayer ); 
	pszName = CPLStrdup( buf );
    }

    // Because we don't represent centroids as any simple feature, we have to scan
    // category index and create index of feature IDs pointing to category index
    nTotalCount = Vect_cidx_get_type_count(poMap,iLayer, GV_POINT|GV_LINES|GV_AREA);
    CPLDebug ( "GRASS", "nTotalCount = %d", nTotalCount );
    paFeatureIndex = (int *) CPLMalloc ( nTotalCount * sizeof(int) );
    
    int n = Vect_cidx_get_type_count(poMap,iLayer, GV_POINTS|GV_LINES|GV_AREA);
    int cnt = 0;
    for ( int i = 0; i < n; i++ ) 
    {
	int cat,type, id;
	
	Vect_cidx_get_cat_by_index ( poMap, iLayerIndex, i, &cat, &type, &id );
    
	if ( !( type & (GV_POINT|GV_LINES|GV_AREA) ) ) continue;
	paFeatureIndex[cnt++] = i;
    }

    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();

    // Get type definition
    int nTypes = Vect_cidx_get_num_types_by_index ( poMap, iLayerIndex );
    int types = 0;
    for ( int i = 0; i < nTypes; i++ ) {
	int type, count;
	Vect_cidx_get_type_count_by_index ( poMap, iLayerIndex, i, &type, &count);
	if ( !(type & (GV_POINT|GV_LINES|GV_AREA) ) ) continue;
	types |= type;
        CPLDebug ( "GRASS", "type = %d types = %d", type, types );
    }
    
    OGRwkbGeometryType eGeomType = wkbUnknown;
    if ( types == GV_LINE || types == GV_BOUNDARY || types == GV_LINES ) 
    {
        eGeomType = wkbLineString;
    } 
    else if ( types == GV_POINT )
    {
        eGeomType = wkbPoint;
    }
    else if ( types == GV_AREA )
    {
        CPLDebug ( "GRASS", "set wkbPolygon" );
        eGeomType = wkbPolygon;
    }

    if (Vect_is_3d(poMap))
        poFeatureDefn->SetGeomType ( (OGRwkbGeometryType)(eGeomType | wkb25DBit) );
    else
        poFeatureDefn->SetGeomType ( eGeomType );

    // Get attributes definition
    poDbString = (dbString*) CPLMalloc ( sizeof(dbString) );
    poCursor = (dbCursor*) CPLMalloc ( sizeof(dbCursor) );
    bCursorOpened = FALSE;

    poDriver = NULL;
    bHaveAttributes = false;
    db_init_string ( poDbString );
    if ( poLink ) 
    {
	if ( StartDbDriver() ) 
	{
	    db_set_string ( poDbString, poLink->table );
	    dbTable *table;
	    if ( db_describe_table ( poDriver, poDbString, &table) == DB_OK )
	    {
		nFields = db_get_table_number_of_columns ( table );
		iCatField = -1;
		for ( int i = 0; i < nFields; i++) 
		{
		    dbColumn *column = db_get_table_column ( table, i );
		    int ctype = db_sqltype_to_Ctype ( db_get_column_sqltype(column) );
    
		    OGRFieldType ogrFtype = OFTInteger;
 	     	    switch ( ctype ) {
			 case DB_C_TYPE_INT:
			    ogrFtype = OFTInteger;
			    break; 
			 case DB_C_TYPE_DOUBLE:
			    ogrFtype = OFTReal;
			    break; 
			 case DB_C_TYPE_STRING:
			    ogrFtype = OFTString;
			    break; 
			 case DB_C_TYPE_DATETIME:
			    ogrFtype = OFTDateTime;
			    break; 
		    }

		    CPLDebug ( "GRASS", "column = %s type = %d", 
			       db_get_column_name(column), ctype );
		    
		    OGRFieldDefn oField ( db_get_column_name(column), ogrFtype );
		    poFeatureDefn->AddFieldDefn( &oField );

		    if ( G_strcasecmp(db_get_column_name(column),poLink->key) == 0 )
		    {
			iCatField = i;
		    }
		}
		if ( iCatField >= 0  ) 
		{
    		    bHaveAttributes = true;
		}
		else
		{
		    CPLError( CE_Failure, CPLE_AppDefined, "Cannot find key field" );
		    db_close_database_shutdown_driver ( poDriver );
		    poDriver = NULL;
		}
	    }
	    else
	    {
		CPLError( CE_Failure, CPLE_AppDefined, "Cannot describe table %s", 
			  poLink->table );

	    }
	    db_close_database_shutdown_driver ( poDriver );
	    poDriver = NULL;
	}
    } 
	
    if ( !bHaveAttributes && iLayer > 0 ) // Because features in layer 0 have no cats  
    {
	OGRFieldDefn oField("cat", OFTInteger);
	poFeatureDefn->AddFieldDefn( &oField );
    }

    if ( getenv("GISBASE") )  // We have some projection info in GISBASE
    {
        struct Key_Value *projinfo, *projunits;

	// Note: we dont have to reset GISDBASE and LOCATION_NAME because 
	// OGRGRASSLayer constructor is called from OGRGRASSDataSource::Open
	// where those variables are set

        projinfo = G_get_projinfo();
	projunits = G_get_projunits();

	char *srsWkt = GPJ_grass_to_wkt ( projinfo, projunits, 0, 0);
	if ( srsWkt ) 
	{
	    poSRS = new OGRSpatialReference ( srsWkt );
	    G_free ( srsWkt );
	}

        G_free_key_value(projinfo);
        G_free_key_value(projunits);
    }
}

/************************************************************************/
/*                           ~OGRGRASSLayer()                           */
/************************************************************************/
OGRGRASSLayer::~OGRGRASSLayer()
{
    if ( bCursorOpened ) 
    {
	db_close_cursor ( poCursor);
    }

    if ( poDriver ) 
    {
	StopDbDriver();
    }
    
    if ( pszName ) CPLFree ( pszName );
    if ( poFeatureDefn )
        poFeatureDefn->Release();
    if ( poSRS )
        poSRS->Release();

    if ( pszQuery ) CPLFree ( pszQuery );
    
    if ( paFeatureIndex ) CPLFree ( paFeatureIndex );
    
    if ( poLink ) G_free ( poLink );
    
    Vect_destroy_line_struct ( poPoints );
    Vect_destroy_cats_struct ( poCats );

    db_free_string ( poDbString );
    CPLFree ( poDbString );
    CPLFree ( poCursor );

    if ( paSpatialMatch ) CPLFree ( paSpatialMatch );
    if ( paQueryMatch ) CPLFree ( paQueryMatch );
}

/************************************************************************/
/*                            StartDbDriver                             */
/************************************************************************/
bool OGRGRASSLayer::StartDbDriver()
{
    CPLDebug ( "GRASS", "StartDbDriver()" ); 

    bCursorOpened = false;
	    
    if ( !poLink ) 
    {
	return false;
    }
    poDriver = db_start_driver_open_database ( poLink->driver, poLink->database );
    
    if ( poDriver == NULL) 
    {
	CPLError( CE_Failure, CPLE_AppDefined, "Cannot open database %s by driver %s, "
		  "check if GISBASE enviroment variable is set, the driver is available "
		  " and the database is accessible.", poLink->driver, poLink->database );
	return false;
    } 
    return true;
}

/************************************************************************/
/*                            StopDbDriver                              */
/************************************************************************/
bool OGRGRASSLayer::StopDbDriver()
{
    if ( !poDriver ) 
    {
	CPLError( CE_Failure, CPLE_AppDefined, "Driver is not started" );
	return true; // I think that true is OK here
    }

    // TODO!!!: Because of bug in GRASS library it is impossible 
    // to stop drivers in FIFO order. Until this is fixed 
    // we have to use kill
    CPLDebug ( "GRASS", "driver PID = %d", poDriver->pid ); 

#if defined(_WIN32) || defined(__WIN32__)
    db_close_database_shutdown_driver ( poDriver );
#else
    if ( kill (poDriver->pid, SIGINT) != 0 ) 
    {
	if ( kill (poDriver->pid, SIGKILL) != 0 ) 
	{
	    CPLError( CE_Failure, CPLE_AppDefined, "Cannot stop database "
		      "driver pid = %d", poDriver->pid );
	}
    }
#endif
	    
    bCursorOpened = false;
    
    return true;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/
void OGRGRASSLayer::ResetReading()
{
    iNextId = 0;
    
    if ( bCursorOpened ) {
	ResetSequentialCursor();
    }
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/*                                                                      */
/*      If we already have an FID list, we can easily resposition       */
/*      ourselves in it.                                                */
/************************************************************************/
OGRErr OGRGRASSLayer::SetNextByIndex( long nIndex )
{
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL ) 
    {
	iNextId = 0;
	int count = 0;
	
	while ( true ) {
	    if( iNextId >= nTotalCount ) break;
	    if ( count == nIndex ) break;

	    // Attributes
	    if( pszQuery != NULL && !paQueryMatch[iNextId] ) {
		iNextId++;
		continue;
	    }

	    // Spatial
	    if( m_poFilterGeom && !paSpatialMatch[iNextId] ) {
		iNextId++;
		continue;
	    }
	    count++;
	}
    }

    iNextId = nIndex;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetAttributeFilter                         */
/************************************************************************/
OGRErr OGRGRASSLayer::SetAttributeFilter( const char *query )
{
    CPLDebug ( "GRASS", "SetAttributeFilter: %s", query  );

    if ( query == NULL ) {
	// Release old if any
	if ( pszQuery ) {
	    CPLFree ( pszQuery );
	    pszQuery = NULL;
	}
	if ( paQueryMatch ) {
	    CPLFree ( paQueryMatch );
	    paQueryMatch = NULL;
	}
	return OGRERR_NONE;
    }

    paQueryMatch = (char *) CPLMalloc ( nTotalCount );
    memset ( paQueryMatch, 0x0, nTotalCount );
    pszQuery = strdup ( query );

    OGRLayer::SetAttributeFilter(query); // Otherwise crash on delete

    if ( bHaveAttributes ) {

	if ( !poDriver ) 
	{
	    StartDbDriver();
	}

	if ( poDriver ) 
	{
	    if ( bCursorOpened )
	    {
		db_close_cursor ( poCursor ); 
		bCursorOpened = false;
	    }
	    OpenSequentialCursor();
	    if ( bCursorOpened )
	    {
		SetQueryMatch();
		db_close_cursor ( poCursor );
		bCursorOpened = false;
	    }
	    else
	    {
		CPLFree ( pszQuery );
		pszQuery = NULL;
		return OGRERR_FAILURE;
	    }
	    db_close_database_shutdown_driver ( poDriver );
	    poDriver = NULL;
	}
	else
	{
	    CPLFree ( pszQuery );
	    pszQuery = NULL;
	    return OGRERR_FAILURE;
	}
    }
    else
    {
	// Use OGR to evaluate category match
	for ( int i = 0; i < nTotalCount; i++ ) 
	{
	    OGRFeature *feature = GetFeature(i); 
	    CPLDebug ( "GRASS", "i = %d eval = %d", i, m_poAttrQuery->Evaluate ( feature ) );
	    if ( m_poAttrQuery->Evaluate ( feature ) )
	    {
		paQueryMatch[i] = 1;
	    }
	}
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetQueryMatch                              */
/************************************************************************/
bool OGRGRASSLayer::SetQueryMatch()
{
    CPLDebug ( "GRASS", "SetQueryMatch" );

    // NOTE: we don't have to call ResetSequentialCursor() first because
    // this method is called immediately after OpenSequentialCursor()
    
    if ( !bCursorOpened ) {
	CPLError( CE_Failure, CPLE_AppDefined, "Cursor is not opened.");
	return false;
    }

    int more;
    int cidx = 0; // index to category index
    int fidx = 0; // index to feature index (paFeatureIndex)
    // number of categories in category index
    int ncats = Vect_cidx_get_num_cats_by_index ( poMap, iLayerIndex );
    dbTable *table = db_get_cursor_table ( poCursor );
    while ( true ) {
	if( db_fetch ( poCursor, DB_NEXT, &more) != DB_OK ) 
	{
	    CPLError( CE_Failure, CPLE_AppDefined, "Cannot fetch attributes.");
	    return false;
	}
	if ( !more ) break;

	dbColumn *column = db_get_table_column ( table, iCatField );
	dbValue *value = db_get_column_value ( column );
	int cat = db_get_value_int ( value );

	// NOTE: because of bug in GRASS library it is impossible to use
	//       Vect_cidx_find_next
	
	// Go through category index until first record of current category 
	// is found or a category > current is found
	int cidxcat, type, id;
	while ( cidx < ncats ) {
	    Vect_cidx_get_cat_by_index ( poMap, iLayerIndex, cidx, 
		                         &cidxcat, &type, &id );

	    if ( cidxcat < cat ) {
	    	cidx++;
		continue;
	    }
	    if ( cidxcat > cat ) break; // Not found
	    
	    // We have the category we want, check type
	    if ( !(type & (GV_POINT|GV_LINES|GV_AREA)) )
	    {
	    	cidx++;
		continue;
	    }

	    // Both category and type match -> find feature and set it on
	    while ( true ) {
		if ( fidx > nTotalCount || paFeatureIndex[fidx] > cidx ) { 
		    // should not happen
		    break;
		}
		    
		if ( paFeatureIndex[fidx] == cidx ) {
		    paQueryMatch[fidx] = 1;
		    fidx++;
		    break;
		}
		fidx++;
	    }
	    cidx++;
	}

	if ( id < 0 ) continue; // not found
    }

    return true;
}
    
/************************************************************************/
/*                           OpenSequentialCursor                       */
/************************************************************************/
bool OGRGRASSLayer::OpenSequentialCursor()
{
    CPLDebug ( "GRASS", "OpenSequentialCursor: %s", pszQuery  );

    if ( !poDriver ) 
    {
	CPLError( CE_Failure, CPLE_AppDefined, "Driver not opened.");
	return false;
    }

    if ( bCursorOpened )
    {
	db_close_cursor ( poCursor );
	bCursorOpened = false;
    }

    char buf[2000];
    sprintf ( buf, "SELECT * FROM %s ", poLink->table );
    db_set_string ( poDbString, buf);

    if ( pszQuery ) {
	sprintf ( buf, "WHERE %s ", pszQuery );
	db_append_string ( poDbString, buf);
    }

    sprintf ( buf, "ORDER BY %s", poLink->key);
    db_append_string ( poDbString, buf);

    CPLDebug ( "GRASS", "Query: %s", db_get_string(poDbString) );
    
    if ( db_open_select_cursor ( poDriver, poDbString, 
		poCursor, DB_SCROLL) == DB_OK ) 
    {
	iCurrentCat = -1;
	bCursorOpened = true;
	CPLDebug ( "GRASS", "num rows = %d", db_get_num_rows ( poCursor ) );
    } 
    else 
    {
	CPLError( CE_Failure, CPLE_AppDefined, "Cannot open cursor.");
	return false;
    }
    return true;
}

/************************************************************************/
/*                           ResetSequentialCursor                      */
/************************************************************************/
bool OGRGRASSLayer::ResetSequentialCursor()
{
    CPLDebug ( "GRASS", "ResetSequentialCursor" );

    int more;
    if( db_fetch ( poCursor, DB_FIRST, &more) != DB_OK ) 
    {
	CPLError( CE_Failure, CPLE_AppDefined, "Cannot reset cursor.");
	return false;
    }
    if( db_fetch ( poCursor, DB_PREVIOUS, &more) != DB_OK ) 
    {
	CPLError( CE_Failure, CPLE_AppDefined, "Cannot reset cursor.");
	return false;
    }
    return true;
}

/************************************************************************/
/*                           SetSpatialFilter                           */
/************************************************************************/
void OGRGRASSLayer::SetSpatialFilter( OGRGeometry * poGeomIn )
{
    CPLDebug ( "GRASS", "SetSpatialFilter" );

    OGRLayer::SetSpatialFilter ( poGeomIn );

    if ( poGeomIn == NULL ) {
	// Release old if any
    	if ( paSpatialMatch ) {
	    CPLFree ( paSpatialMatch );
	    paSpatialMatch = NULL;
	}
	return;
    }

    SetSpatialMatch();
}

/************************************************************************/
/*                           SetSpatialMatch                            */
/************************************************************************/
bool OGRGRASSLayer::SetSpatialMatch()
{
    CPLDebug ( "GRASS", "SetSpatialMatch" );

    if ( !paSpatialMatch ) 
    {
	paSpatialMatch = (char *) CPLMalloc ( nTotalCount );
    }
    memset ( paSpatialMatch, 0x0, nTotalCount );

    OGRGeometry *geom; 
    OGRLineString *lstring = new OGRLineString();
    lstring->setNumPoints ( 5 );
    geom = lstring;

    for ( int i = 0; i < nTotalCount; i++ ) {
	int cidx = paFeatureIndex[i];

	int cat, type, id;
	
	Vect_cidx_get_cat_by_index ( poMap, iLayerIndex, cidx, &cat, &type, &id );

#if GRASS_VERSION_MAJOR  >= 7
    struct bound_box box;
#else
	BOUND_BOX box;
#endif

	switch ( type ) 
	{
	    case GV_POINT:
	    case GV_LINE:
	    case GV_BOUNDARY:
		Vect_get_line_box ( poMap, id, &box );
		break;

	    case GV_AREA:
		Vect_get_area_box ( poMap, id, &box );
		break;
	}
		
	lstring->setPoint( 0, box.W, box.N, 0. );
	lstring->setPoint( 1, box.W, box.S, 0. );
	lstring->setPoint( 2, box.E, box.S, 0. );
	lstring->setPoint( 3, box.E, box.N, 0. );
	lstring->setPoint( 4, box.W, box.N, 0. );

	if ( FilterGeometry(geom) ) {
    	    CPLDebug ( "GRASS", "Feature %d in filter", i );
	    paSpatialMatch[i] = 1;
	}
    }
    delete lstring;
    return true;
}
    
/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/
OGRFeature *OGRGRASSLayer::GetNextFeature()
{
    CPLDebug ( "GRASS", "OGRGRASSLayer::GetNextFeature" );
    OGRFeature  *poFeature = NULL;

    int cat;

    // Get next iNextId
    while ( true ) {
	if( iNextId >= nTotalCount ) // No more features
	{ 
	    // Close cursor / driver if opened 
	    if ( bCursorOpened ) 
	    {
	    	db_close_cursor ( poCursor);
	    	bCursorOpened = false;
	    }
	    if ( poDriver ) 
	    {
    	    	db_close_database_shutdown_driver ( poDriver );
		poDriver = NULL;
	    }

	    return NULL;
	}

	// Attributes
	if( pszQuery != NULL && !paQueryMatch[iNextId] ) {
	    iNextId++;
	    continue;
	}

	// Spatial
	if( m_poFilterGeom && !paSpatialMatch[iNextId] ) {
	    iNextId++;
	    continue;
	}
	
	break; // Attributes & spatial filter match
    }

    OGRGeometry *poOGR = GetFeatureGeometry ( iNextId, &cat );

    poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetGeometryDirectly( poOGR );
    poFeature->SetFID ( iNextId );
    iNextId++;
    
    // Get attributes
    CPLDebug ( "GRASS", "bHaveAttributes = %d", bHaveAttributes );
    if ( bHaveAttributes ) 
    {
	if ( !poDriver ) 
	{
	    StartDbDriver();
	}
	if ( poDriver ) {
	    if ( !bCursorOpened ) 
	    {
		OpenSequentialCursor();
	    }
	    if ( bCursorOpened ) 
	    {
		dbTable  *table = db_get_cursor_table ( poCursor );
		if ( iCurrentCat < cat ) 
		{
		    while ( true ) {
			int more;
			if( db_fetch ( poCursor, DB_NEXT, &more) != DB_OK ) 
			{
			    CPLError( CE_Failure, CPLE_AppDefined, 
				      "Cannot fetch attributes.");
			    break;
			}
			if ( !more ) break;

			dbColumn *column = db_get_table_column ( table, iCatField );
			dbValue *value = db_get_column_value ( column );
			iCurrentCat = db_get_value_int ( value );

			if ( iCurrentCat >= cat ) break;
		    }
		}
		if ( cat == iCurrentCat )
		{
		    SetAttributes ( poFeature, table );
		} 
		else 
		{
		    CPLError( CE_Failure, CPLE_AppDefined, "Attributes not found.");
		}
	    }
	}
    } 
    else if ( iLayer > 0 ) // Add category
    {
	poFeature->SetField( 0, cat );
    }	
    
    m_nFeaturesRead++;
    return poFeature;
}
/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/
OGRFeature *OGRGRASSLayer::GetFeature( long nFeatureId )

{
    CPLDebug ( "GRASS", "OGRGRASSLayer::GetFeature nFeatureId = %ld", nFeatureId );

    int cat;
    OGRFeature *poFeature = NULL;

    OGRGeometry *poOGR = GetFeatureGeometry ( nFeatureId, &cat );

    poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetGeometryDirectly( poOGR );
    poFeature->SetFID ( nFeatureId );

    // Get attributes
    if ( bHaveAttributes && !poDriver ) 
    {
	StartDbDriver();
    }
    if ( poDriver ) 
    {
	if ( bCursorOpened ) 
	{
	    db_close_cursor ( poCursor);
	    bCursorOpened = false;
	}
	CPLDebug ( "GRASS", "Open cursor for key = %d", cat );
	char buf[2000];
	sprintf ( buf, "SELECT * FROM %s WHERE %s = %d", 
		       poLink->table, poLink->key, cat );
	db_set_string ( poDbString, buf);
	if ( db_open_select_cursor ( poDriver, poDbString, 
		    poCursor, DB_SEQUENTIAL) == DB_OK ) 
	{
	    iCurrentCat = cat; // Not important
	    bCursorOpened = true;
	} 
	else 
	{
	    CPLError( CE_Failure, CPLE_AppDefined, "Cannot open cursor.");
	}

	if ( bCursorOpened ) 
	{
	    int more;
	    if( db_fetch ( poCursor, DB_NEXT, &more) != DB_OK ) 
	    {
		CPLError( CE_Failure, CPLE_AppDefined, "Cannot fetch attributes.");
	    } 
	    else 
	    {
		if ( !more ) 
		{
		    CPLError( CE_Failure, CPLE_AppDefined, "Attributes not found.");
		} 
		else 
		{
	    	    dbTable *table = db_get_cursor_table ( poCursor );
		    SetAttributes ( poFeature, table );
		}
	    }
	    db_close_cursor ( poCursor);
	    bCursorOpened = false;
	}
    } 
    else if ( iLayer > 0 ) // Add category
    {
	poFeature->SetField( 0, cat );
    }	
    
    m_nFeaturesRead++;
    return poFeature;
}

/************************************************************************/
/*                             GetFeatureGeometry()                     */
/************************************************************************/
OGRGeometry *OGRGRASSLayer::GetFeatureGeometry ( long nFeatureId, int *cat )
{
    CPLDebug ( "GRASS", "OGRGRASSLayer::GetFeatureGeometry nFeatureId = %ld", nFeatureId );

    int cidx = paFeatureIndex[(int)nFeatureId];

    int type, id;
    Vect_cidx_get_cat_by_index ( poMap, iLayerIndex, cidx, cat, &type, &id );

    //CPLDebug ( "GRASS", "cat = %d type = %d id = %d", *cat, type, id );

    OGRGeometry *poOGR = NULL;
    int bIs3D = Vect_is_3d(poMap);

    switch ( type ) {
	case GV_POINT:
        {
	    Vect_read_line ( poMap, poPoints, poCats, id);
            if (bIs3D)
                poOGR = new OGRPoint( poPoints->x[0], poPoints->y[0], poPoints->z[0] );
            else
                poOGR = new OGRPoint( poPoints->x[0], poPoints->y[0] );
        }
        break;
	    
	case GV_LINE:
	case GV_BOUNDARY:
        {
	    Vect_read_line ( poMap, poPoints, poCats, id);
	    OGRLineString *poOGRLine = new OGRLineString();
            if (bIs3D)
                poOGRLine->setPoints( poPoints->n_points, 
                                      poPoints->x, poPoints->y, poPoints->z );
            else
                poOGRLine->setPoints( poPoints->n_points, 
                                      poPoints->x, poPoints->y );

            poOGR = poOGRLine;
        }
        break;

	case GV_AREA:
        {
	    Vect_get_area_points ( poMap, id, poPoints );
	    
	    OGRPolygon 		*poOGRPoly;
	    poOGRPoly = new OGRPolygon();

	    OGRLinearRing       *poRing;
	    poRing = new OGRLinearRing();
            if (bIs3D)
                poRing->setPoints( poPoints->n_points,
                                poPoints->x, poPoints->y, poPoints->z );
            else
                poRing->setPoints( poPoints->n_points,
                                poPoints->x, poPoints->y ); 

	    poOGRPoly->addRingDirectly( poRing );

	    // Islands
	    int nisles = Vect_get_area_num_isles ( poMap, id );
	    for ( int i = 0; i < nisles; i++ ) {
		int isle =  Vect_get_area_isle ( poMap, id, i );
		Vect_get_isle_points ( poMap, isle, poPoints );

		poRing = new OGRLinearRing();
                if (bIs3D)
                    poRing->setPoints( poPoints->n_points,
                                    poPoints->x, poPoints->y, poPoints->z );
                else
                    poRing->setPoints( poPoints->n_points,
                                    poPoints->x, poPoints->y );

		poOGRPoly->addRingDirectly( poRing );
	    }
	    
	    poOGR = poOGRPoly;
        }   
        break;

	default: // Should not happen
        {
	    CPLError( CE_Failure, CPLE_AppDefined, "Unknown GRASS feature type.");
	    return NULL;
        }
    }
	    
    return poOGR;
}

/************************************************************************/
/*                          SetAttributes()                             */
/************************************************************************/
bool OGRGRASSLayer::SetAttributes ( OGRFeature *poFeature, dbTable *table )
{
    CPLDebug ( "GRASS", "OGRGRASSLayer::SetAttributes" );

    for ( int i = 0; i < nFields; i++) 
    {
	dbColumn *column = db_get_table_column ( table, i );
	dbValue *value = db_get_column_value ( column );

	int ctype = db_sqltype_to_Ctype ( db_get_column_sqltype(column) );

	if ( !db_test_value_isnull(value) )
	{
	    switch ( ctype ) {
		case DB_C_TYPE_INT:
		    poFeature->SetField( i, db_get_value_int ( value ));
		    break; 
		case DB_C_TYPE_DOUBLE:
		    poFeature->SetField( i, db_get_value_double ( value ));
		    break; 
		case DB_C_TYPE_STRING:
		    poFeature->SetField( i, db_get_value_string ( value ));
		    break; 
		case DB_C_TYPE_DATETIME:
		    db_convert_column_value_to_string ( column, poDbString );
		    poFeature->SetField( i, db_get_string ( poDbString ));
		    break; 
	    }
	}
	
	db_convert_column_value_to_string ( column, poDbString );
	//CPLDebug ( "GRASS", "val = %s", db_get_string ( poDbString ));
    }
    return true;
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/
OGRErr OGRGRASSLayer::SetFeature( OGRFeature *poFeature )
{
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/
OGRErr OGRGRASSLayer::CreateFeature( OGRFeature *poFeature )
{
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/
int OGRGRASSLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
        
    return nTotalCount;
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      Fetch extent of the data currently stored in the dataset.       */
/*      The bForce flag has no effect on SHO files since that value     */
/*      is always in the header.                                        */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRRERR_FAILURE.                            */
/************************************************************************/
OGRErr OGRGRASSLayer::GetExtent (OGREnvelope *psExtent, int bForce)
{
#if GRASS_VERSION_MAJOR  >= 7
    struct bound_box box;
#else
    BOUND_BOX box;
#endif

    Vect_get_map_box ( poMap, &box );

    psExtent->MinX = box.W;
    psExtent->MinY = box.S;
    psExtent->MaxX = box.E;
    psExtent->MaxY = box.N;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/
int OGRGRASSLayer::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSetNextByIndex) )
        return TRUE;

    else 
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/
OGRErr OGRGRASSLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )
{
    CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create fields on a GRASS layer.\n");
    
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/
OGRSpatialReference *OGRGRASSLayer::GetSpatialRef()
{
    return poSRS;
}

