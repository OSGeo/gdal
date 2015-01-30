/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSDELayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008, Shawn Gervais <project10@project10.net> 
 * Copyright (c) 2008, Howard Butler <hobu.inc@gmail.com>
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

#include "ogr_sde.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRSDELayer()                             */
/************************************************************************/

OGRSDELayer::OGRSDELayer( OGRSDEDataSource *poDSIn, int bUpdate )

{
    poDS = poDSIn;
    
    bUpdateAccess = bUpdate;
    bPreservePrecision = TRUE;

    iFIDColumn = -1;
    nNextFID = 0;
    iNextFIDToWrite = 1;

    iShapeColumn = -1;
    poSRS = NULL;
    poFeatureDefn = NULL;

    bQueryInstalled = FALSE;
    hStream = NULL;
    hCoordRef = NULL;
    papszAllColumns = NULL;
    bHaveLayerInfo = FALSE;
    bUseNSTRING = FALSE;
}

/************************************************************************/
/*                            ~OGRSDELayer()                            */
/************************************************************************/

OGRSDELayer::~OGRSDELayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "OGR_SDE", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( bHaveLayerInfo )
        SE_layerinfo_free( hLayerInfo );

    if( hStream )
    {
        SE_stream_free( hStream );
        hStream = NULL;
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( hCoordRef )
        SE_coordref_free( hCoordRef );

    if( poSRS )
        poSRS->Release();

    CSLDestroy( papszAllColumns );
    
    CPLFree( pszOwnerName );
    CPLFree( pszDbTableName );
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

int OGRSDELayer::Initialize( const char *pszTableName, 
                             const char *pszFIDColumn,
                             const char *pszShapeColumn )

{
    SE_COLUMN_DEF *asColumnDefs;
    SHORT   nColumnCount;
    int nSDEErr, iCol;
    
/* -------------------------------------------------------------------- */
/*      Determine DBMS table owner name and the table name part         */
/*      from pszTableName which is a fully-qualified table name         */
/* -------------------------------------------------------------------- */
    char               *pszTableNameCopy = strdup( pszTableName );
    char               *pszPeriodPtr;
    
    if( (pszPeriodPtr = strstr( pszTableNameCopy,"." )) != NULL )
    {
        *pszPeriodPtr  = '\0';
        pszOwnerName   = strdup( pszTableNameCopy );
        pszDbTableName = strdup( pszPeriodPtr+1 );
    }
    else
    {
        pszOwnerName   = NULL;
        pszDbTableName = strdup( pszTableName );
    }
    
    CPLFree( pszTableNameCopy );

/* -------------------------------------------------------------------- */
/*      Determine whether multi-versioning is enabled for this table.   */
/* -------------------------------------------------------------------- */
    SE_REGINFO          hRegInfo;
    
    nSDEErr = SE_reginfo_create( &hRegInfo );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_reginfo_create" );
        return FALSE;
    }
    
    // TODO: This is called from places that have RegInfo already -
    // should we just pass that in?
    nSDEErr = SE_registration_get_info( poDS->GetConnection(),
                                        pszTableName,
                                        hRegInfo );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_registration_get_info" );
        return FALSE;
    }
    
    bVersioned = SE_reginfo_is_multiversion( hRegInfo );
    
/* -------------------------------------------------------------------- */
/*      Describe table                                                  */
/* -------------------------------------------------------------------- */
    nSDEErr = 
        SE_table_describe( poDS->GetConnection(), pszTableName, 
                           &nColumnCount, &asColumnDefs );

    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_table_describe" );
        return FALSE;
    }

    poFeatureDefn = new OGRFeatureDefn( pszTableName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

/* -------------------------------------------------------------------- */
/*  If the OGR_SDE_GETLAYERTYPE option is set to TRUE, then the layer   */
/*  is tested to see if it contains one, and only one, type of geometry */
/*  then that geometry type is placed into the LayerDefn.               */
/* -------------------------------------------------------------------- */
    const char *pszLayerType = CPLGetConfigOption( "OGR_SDE_GETLAYERTYPE", 
                                                   "FALSE" );

    if( CSLTestBoolean(pszLayerType) != FALSE )
    {
        poFeatureDefn->SetGeomType(DiscoverLayerType());
    }

    for( iCol = 0; iCol < nColumnCount; iCol++ )
    {
        OGRFieldType eOGRType = OFTIntegerList; // dummy
        int nWidth = -1, nPrecision = -1;

        papszAllColumns = CSLAddString( papszAllColumns, 
                                        asColumnDefs[iCol].column_name );

        switch( asColumnDefs[iCol].sde_type )
        {
          case SE_SMALLINT_TYPE:
          case SE_INTEGER_TYPE:
            eOGRType = OFTInteger;
            nWidth = asColumnDefs[iCol].size;
            break;

          case SE_FLOAT_TYPE:
          case SE_DOUBLE_TYPE:
            eOGRType = OFTReal;
            nWidth = asColumnDefs[iCol].size;
            nPrecision = asColumnDefs[iCol].decimal_digits;
            break;

          case SE_STRING_TYPE:
#ifdef SE_UUID_TYPE
          case SE_UUID_TYPE:
#endif
#ifdef SE_NSTRING_TYPE
          case SE_NSTRING_TYPE:
#endif
#ifdef SE_CLOB_TYPE
          case SE_CLOB_TYPE:
#endif
#ifdef SE_NCLOB_TYPE
          case SE_NCLOB_TYPE:
#endif
            eOGRType = OFTString;
            nWidth = asColumnDefs[iCol].size;
            break;

          case SE_BLOB_TYPE:
            eOGRType = OFTBinary;
            break;

          case SE_DATE_TYPE:
            eOGRType = OFTDateTime;
            break;

          case SE_SHAPE_TYPE:
          {
              if( iShapeColumn == -1 )
              {
                  if( pszShapeColumn == NULL
                      || EQUAL(pszShapeColumn,
                               asColumnDefs[iCol].column_name) )
                  {
                      iShapeColumn = iCol;
                      osShapeColumnName = asColumnDefs[iCol].column_name;
                  }
              }
          }
          break;

          default:
            break;
        }

        if( eOGRType == OFTIntegerList )
            continue;

        OGRFieldDefn  oField( asColumnDefs[iCol].column_name, eOGRType );

        if( nWidth != -1 )
            oField.SetWidth( nWidth );
        if( nPrecision != -1 )
            oField.SetPrecision( nPrecision );

        poFeatureDefn->AddFieldDefn( &oField );
        
        anFieldMap.push_back( iCol );
        anFieldTypeMap.push_back( asColumnDefs[iCol].sde_type );

        if( pszFIDColumn
            && EQUAL(asColumnDefs[iCol].column_name,pszFIDColumn) )
        {
            osFIDColumnName = asColumnDefs[iCol].column_name;
            iFIDColumn = anFieldMap.size() - 1;
        }
    }

    SE_table_free_descriptions( asColumnDefs );
    SE_reginfo_free( hRegInfo );

    return TRUE;
}

/************************************************************************/
/*                           NeedLayerInfo()                            */
/*                                                                      */
/*      Verify layerinfo is available, and load if not.  Loading        */
/*      layerinfo is relatively expensive so we try to put it off as    */
/*      long as possible.                                               */
/************************************************************************/

int OGRSDELayer::NeedLayerInfo()

{
    int nSDEErr;

    if( bHaveLayerInfo )
        return TRUE;

    nSDEErr = SE_layerinfo_create( NULL, &hLayerInfo );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_layerinfo_create" );
        return FALSE;
    }

    CPLDebug( "OGR_SDE", "Loading %s layerinfo.", 
              poFeatureDefn->GetName() );

    nSDEErr = SE_layer_get_info( poDS->GetConnection(), 
                                 poFeatureDefn->GetName(), 
                                 osShapeColumnName.c_str(),
                                 hLayerInfo );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_layer_get_info" );
        return FALSE;
    }

    bHaveLayerInfo = TRUE;

/* -------------------------------------------------------------------- */
/*      Fetch coordinate reference system.                              */
/* -------------------------------------------------------------------- */
    SE_coordref_create( &hCoordRef );
    
    nSDEErr = SE_layerinfo_get_coordref( hLayerInfo, hCoordRef );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_layerinfo_get_coordref" );
    }
    else
    {
        char szWKT[SE_MAX_SPATIALREF_SRTEXT_LEN];

        nSDEErr = SE_coordref_get_description( hCoordRef, szWKT );
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_coordref_get_description" );
        }
        else
        {
            poSRS = new OGRSpatialReference(szWKT);
            poSRS->morphFromESRI();
        }

	LFLOAT falsex, falsey, xyunits;
	nSDEErr = SE_coordref_get_xy( hCoordRef, &falsex, &falsey, &xyunits );
	CPLDebug( "SDE", "SE_coordref_get_xy(%s) = %g/%g/%g",
		  pszDbTableName, falsex, falsey, xyunits );
    }

    return TRUE;
}

/************************************************************************/
/*                          DiscoverLayerType()                         */
/************************************************************************/

OGRwkbGeometryType OGRSDELayer::DiscoverLayerType()
{
    if( !NeedLayerInfo() )
        return wkbUnknown;

    int nSDEErr;
    LONG nShapeTypeMask = 0;
  
/* -------------------------------------------------------------------- */
/*      Check layerinfo flags to establish what geometry types may      */
/*      occur.                                                          */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_layerinfo_get_shape_types( hLayerInfo, &nShapeTypeMask );
    if (nSDEErr != SE_SUCCESS)
    {
        CPLDebug( "OGR_SDE",
                  "Unable to read the layer type information, defaulting to wkbUnknown:  error=%d.",
                  nSDEErr );

        return wkbUnknown;
    }

    int bIsMultipart = ( nShapeTypeMask & SE_MULTIPART_TYPE_MASK ? 1 : 0);
    nShapeTypeMask &= ~SE_MULTIPART_TYPE_MASK;

    // Since we assume that all layers can bear a NULL geometry, 
    // throw the flag away.
    nShapeTypeMask &= ~SE_NIL_TYPE_MASK;

    int nTypeCount = 0;
    if ( nShapeTypeMask & SE_POINT_TYPE_MASK )
        nTypeCount++;
    if ( nShapeTypeMask & SE_LINE_TYPE_MASK 
         || nShapeTypeMask & SE_SIMPLE_LINE_TYPE_MASK )
        nTypeCount++;

    if ( nShapeTypeMask & SE_AREA_TYPE_MASK )
        nTypeCount++;

/* -------------------------------------------------------------------- */
/*      When the flags indicate multiple geometry types are             */
/*      possible, we examine the layer statistics to see if in          */
/*      reality only one geometry type does occur.  This is somewhat    */
/*      expensive, so we avoid it if we can.                            */
/* -------------------------------------------------------------------- */
    if ( nTypeCount == 0 )
    {
        CPLDebug( "OGR_SDE", "There is no layer type indicated for the current layer." );
        return wkbUnknown;
    }
    else if ( nTypeCount > 1 )
    {
        CPLDebug( "OGR_SDE", "More than one layer type is indicated for this layer, gathering layer statistics are being gathered." );
        SE_LAYER_STATS layerstats = {0};
        char szTableName[SE_QUALIFIED_TABLE_NAME];
        char szShapeColumn[SE_MAX_COLUMN_LEN];

        szTableName[0] = '\0';
        szShapeColumn[0] = '\0';

        nSDEErr = SE_layerinfo_get_spatial_column( hLayerInfo, szTableName, 
                                                   szShapeColumn );
        if ( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_layerinfo_get_spatial_column" );
            return wkbUnknown;
        }

        nSDEErr = SE_layer_get_statistics( poDS->GetConnection(), szTableName, 
                                           szShapeColumn,
                                           &layerstats);
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_layer_get_statistics" );
            return wkbUnknown;
        }

        if ( nShapeTypeMask & SE_POINT_TYPE_MASK && 
             ( layerstats.POINTs + layerstats.MultiPOINTs ) == 0 )
            nShapeTypeMask &= ~SE_POINT_TYPE_MASK;

        if ( nShapeTypeMask & SE_LINE_TYPE_MASK &&
             ( layerstats.LINEs + layerstats.MultiLINEs ) == 0 )
            nShapeTypeMask &= ~SE_LINE_TYPE_MASK;

        if ( nShapeTypeMask & SE_SIMPLE_LINE_TYPE_MASK &&
             ( layerstats.SIMPLE_LINEs + layerstats.MultiSIMPLE_LINEs ) == 0 )
            nShapeTypeMask &= ~SE_SIMPLE_LINE_TYPE_MASK;

        if ( nShapeTypeMask & SE_AREA_TYPE_MASK &&
             ( layerstats.AREAs + layerstats.MultiAREAs ) == 0 )
            nShapeTypeMask &= ~SE_AREA_TYPE_MASK;
    }

/* -------------------------------------------------------------------- */
/*      Select a geometry type based on the remaining flags.  If        */
/*      there is a mix we will fall through to the default (wkbUknown). */
/* -------------------------------------------------------------------- */
    OGRwkbGeometryType eGeoType;
    char *pszTypeName;
    switch (nShapeTypeMask)
    {
      case SE_POINT_TYPE_MASK:
        if (bIsMultipart)
            eGeoType = wkbMultiPoint;
        else
            eGeoType = wkbPoint;
        pszTypeName = "point";
        break;

      case (SE_SIMPLE_LINE_TYPE_MASK | SE_LINE_TYPE_MASK):
      case SE_SIMPLE_LINE_TYPE_MASK:
      case SE_LINE_TYPE_MASK:
        if (bIsMultipart)
            eGeoType = wkbMultiLineString;
        else
            eGeoType = wkbLineString;
        pszTypeName = "line";
        break;
      
      case SE_AREA_TYPE_MASK:
        if (bIsMultipart)
            eGeoType = wkbMultiPolygon;
        else
            eGeoType = wkbPolygon;
        pszTypeName = "polygon";
        break;
      
      default:
        eGeoType = wkbUnknown;
        pszTypeName = "unknown";
        break;
    }

    CPLDebug( "OGR_SDE", 
              "DiscoverLayerType is returning type=%d (%s), multipart=%d.",
              eGeoType, pszTypeName, bIsMultipart );

    return eGeoType;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSDELayer::ResetReading()

{
    bQueryInstalled = FALSE;
    nNextFID = 0;
}

/************************************************************************/
/*                            InstallQuery()                            */
/*                                                                      */
/*      Setup the stream with current query characteristics.            */
/************************************************************************/

int OGRSDELayer::InstallQuery( int bCountingOnly )

{
    int nSDEErr;

/* -------------------------------------------------------------------- */
/*      Create stream, or reset it.                                     */
/* -------------------------------------------------------------------- */
    if( ResetStream() != OGRERR_NONE )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Create query info.                                              */
/* -------------------------------------------------------------------- */
    SE_QUERYINFO hQueryInfo;
    const char *pszTableName = poFeatureDefn->GetName();

    nSDEErr = SE_queryinfo_create (&hQueryInfo);
    if( nSDEErr != SE_SUCCESS) {
        poDS->IssueSDEError( nSDEErr, "SE_queryinfo_create" );
        return FALSE;
    }    
    
/* -------------------------------------------------------------------- */
/*      Select table.                                                   */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_queryinfo_set_tables( hQueryInfo, 1, 
                                       &pszTableName, NULL );
    if( nSDEErr != SE_SUCCESS) {
        poDS->IssueSDEError( nSDEErr, "SE_queryinfo_set_tables" );
        return FALSE;
    }    

/* -------------------------------------------------------------------- */
/*      Set where clause.                                               */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_queryinfo_set_where_clause( hQueryInfo, 
                                             osAttributeFilter.c_str() );
    if( nSDEErr != SE_SUCCESS) {
        poDS->IssueSDEError( nSDEErr, "SE_queryinfo_set_where_clause" );
        return FALSE;
    }    

/* -------------------------------------------------------------------- */
/*      We want to join the spatial and attribute tables.               */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_queryinfo_set_query_type(hQueryInfo,SE_QUERYTYPE_JSF);
    if( nSDEErr != SE_SUCCESS) {
        poDS->IssueSDEError( nSDEErr, "SE_queryinfo_set_query_type" );
        return FALSE;
    }    

/* -------------------------------------------------------------------- */
/*      Establish the columns to query.  If only counting features,     */
/*      we will just use the FID column, otherwise we use all           */
/*      columns.                                                        */
/* -------------------------------------------------------------------- */
    if( bCountingOnly && iFIDColumn != -1 )
    {
        const char *pszFIDColName = 
            poFeatureDefn->GetFieldDefn( iFIDColumn )->GetNameRef();
        
        nSDEErr = SE_queryinfo_set_columns( hQueryInfo, 1, 
                                            (const char **) &pszFIDColName );
        if( nSDEErr != SE_SUCCESS) {
            poDS->IssueSDEError( nSDEErr, "SE_queryinfo_set_columns" );
            return FALSE;
        }    
    }
    else
    {
        nSDEErr = SE_queryinfo_set_columns( hQueryInfo, 
                                            CSLCount(papszAllColumns),
                                            (const char **) papszAllColumns );
        if( nSDEErr != SE_SUCCESS) {
            poDS->IssueSDEError( nSDEErr, "SE_queryinfo_set_columns" );
            return FALSE;
        }    
    }

/* -------------------------------------------------------------------- */
/*      Apply the query to the stream.                                  */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_stream_query_with_info( hStream, hQueryInfo );
    if( nSDEErr != SE_SUCCESS) {
        poDS->IssueSDEError( nSDEErr, "SE_stream_query_with_info" );
        return FALSE;
    }    

/* -------------------------------------------------------------------- */
/*      Free query resources.                                           */
/* -------------------------------------------------------------------- */
    SE_queryinfo_free( hQueryInfo );

/* -------------------------------------------------------------------- */
/*      Setup spatial filter on stream if one is installed.             */
/* -------------------------------------------------------------------- */
    if( m_poFilterGeom != NULL )
    {
        SE_FILTER sConstraint;
        SE_ENVELOPE sEnvelope;
        SE_ENVELOPE  sLayerEnvelope;
        SE_SHAPE hRectShape;
        SHORT nSearchOrder = SE_SPATIAL_FIRST;

        if( osAttributeFilter.size() > 0 )
        {
            const char *pszOrder = CPLGetConfigOption( "OGR_SDE_SEARCHORDER", 
                                                       "ATTRIBUTE_FIRST" );

            if( EQUAL(pszOrder, "ATTRIBUTE_FIRST") )
                nSearchOrder = SE_ATTRIBUTE_FIRST;
            else 
            {
                if( !EQUAL(pszOrder, "SPATIAL_FIRST") )
                    CPLError( CE_Warning, CPLE_AppDefined, 
                              "Unrecognised OGR_SDE_SEARCHORDER value of %s.",
                              pszOrder );
                nSearchOrder = SE_SPATIAL_FIRST;
            }
        }

        NeedLayerInfo(); // need hCoordRef

        nSDEErr = SE_shape_create( hCoordRef, &hRectShape );
        if( nSDEErr != SE_SUCCESS) 
        {
            poDS->IssueSDEError( nSDEErr, "SE_shape_create");
            return FALSE;
        }
        sEnvelope.minx = m_sFilterEnvelope.MinX;
        sEnvelope.miny = m_sFilterEnvelope.MinY;
        sEnvelope.maxx = m_sFilterEnvelope.MaxX;
        sEnvelope.maxy = m_sFilterEnvelope.MaxY;

        nSDEErr = SE_layerinfo_get_envelope( hLayerInfo, &sLayerEnvelope );
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_layerinfo_get_envelope" );
            return FALSE;
        }
        // ensure that the spatial filter overlaps area of the layer
        if (sEnvelope.minx > sLayerEnvelope.maxx || sEnvelope.maxx < sLayerEnvelope.minx ||
            sEnvelope.miny > sLayerEnvelope.maxy || sEnvelope.maxy < sLayerEnvelope.miny ) 
        {
            // using a small rectangle to filter out all the shapes
            sEnvelope.minx = sLayerEnvelope.minx;
            sEnvelope.miny = sLayerEnvelope.miny;
            sEnvelope.maxx = sLayerEnvelope.minx + 0.00000001;
            sEnvelope.maxy = sLayerEnvelope.miny + 0.00000001;  
        }
        
        nSDEErr = SE_shape_generate_rectangle( &sEnvelope, hRectShape );
        if( nSDEErr != SE_SUCCESS) 
        {
            poDS->IssueSDEError( nSDEErr, "SE_shape_generate_rectangle");
            return FALSE;
        }
        
        sConstraint.filter.shape = hRectShape;
        strcpy( sConstraint.table, poFeatureDefn->GetName() );
        strcpy( sConstraint.column, osShapeColumnName.c_str() );
        sConstraint.method = SM_ENVP;
        sConstraint.filter_type = SE_SHAPE_FILTER;
        sConstraint.truth = TRUE;

        nSDEErr = SE_stream_set_spatial_constraints( hStream, nSearchOrder,
                                                     FALSE, 1, &sConstraint );
        if( nSDEErr != SE_SUCCESS) 
        {
            poDS->IssueSDEError( nSDEErr, "SE_stream_set_spatial_constraints");
            return FALSE;
        }    

        SE_shape_free( hRectShape );
    }

/* -------------------------------------------------------------------- */
/*      Execute the query.                                              */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_stream_execute( hStream );
    if( nSDEErr != SE_SUCCESS ) {
        poDS->IssueSDEError( nSDEErr, "SE_stream_execute" );
        return FALSE;
    }    

    bQueryInstalled = TRUE;

    return TRUE;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRSDELayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

    if( pszQuery == NULL )
        osAttributeFilter = "";
    else
        osAttributeFilter = pszQuery;

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                        TranslateOGRRecord()                          */
/*                                                                      */
/*      Translates OGR feature semantics to SDE ones and sets items     */
/*      in the stream for an update or insert operation. The stream is  */
/*      set in insert or update mode depending on the value of the      */
/*      bIsInsert flag. The stream must have already been reset by      */
/*      the caller. Actual execution of the stream operation is the     */
/*      responsibility of the caller.                                   */
/************************************************************************/
OGRErr OGRSDELayer::TranslateOGRRecord( OGRFeature *poFeature,
                                        int bIsInsert )
        
{
    SE_SHAPE            hShape;
    LONG                nSDEErr;

/* -------------------------------------------------------------------- */
/*      Translate geometry to SDE geometry                              */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() != NULL )
    {
        if( TranslateOGRGeometry( poFeature->GetGeometryRef(), &hShape,
                                  hCoordRef ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to convert geometry from OGR -> SDE");
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Determine which fields to insert                                */
/* -------------------------------------------------------------------- */
    int                *paiColToDefMap;
    char              **papszInsertCols = NULL;
    int                 nAttributeCols = 0;
    int                 nSpecialCols = 0;
    int                 i;
    
    paiColToDefMap = (int *) CPLMalloc( sizeof(int) 
                                        * poFeatureDefn->GetFieldCount() );
    
    /*
     * If the row id is managed by USER, and not SDE, then we need to take
     * care to set the FID column ourselves. If the row id column is managed by
     * SDE we are forbidden from setting it.
     */
    if( nFIDColumnType == SE_REGISTRATION_ROW_ID_COLUMN_TYPE_USER
        && iFIDColumn != -1 )
    {
        papszInsertCols = CSLAddString( papszInsertCols,
                                        osFIDColumnName.c_str() );
        
        nSpecialCols++;
    }

    if( poFeature->GetGeometryRef() != NULL )
    {
        papszInsertCols = CSLAddString( papszInsertCols, 
                                        osShapeColumnName.c_str() );
        
        nSpecialCols++;
    }

    // Add attribute fields of this feature, build mapping of
    // indexes to field definitions vs. the columns we will insert
    for( i=0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn   *poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        
        if( !poFeature->IsFieldSet(i) )
            continue;
        
        // Skip FID and Geometry columns
        if( EQUAL(poFieldDefn->GetNameRef(), osFIDColumnName.c_str()) )
        {
            // Skip the column if it's managed by SDE
            if( nFIDColumnType == SE_REGISTRATION_ROW_ID_COLUMN_TYPE_SDE )
                continue;
        }
        
        if( EQUAL(poFieldDefn->GetNameRef(), osShapeColumnName.c_str()) )
            continue;
        
        papszInsertCols = CSLAddString( papszInsertCols,
                                        poFieldDefn->GetNameRef() );
        
        paiColToDefMap[nAttributeCols] = i;
        nAttributeCols++;
    }
    
    
/* -------------------------------------------------------------------- */
/*      Prepare the insert or update stream mode                        */
/* -------------------------------------------------------------------- */
    const char         *pszMethod;
    
    if( bIsInsert )
    {
        nSDEErr = SE_stream_insert_table( hStream, poFeatureDefn->GetName(),
                                          nSpecialCols + nAttributeCols, 
                                          (const char **)papszInsertCols );
        pszMethod = "SE_stream_insert_table";
    }
    else // It's an UPDATE
    {
        const char     *pszWhere;
        
        // Check that there is a FID column detected on the table, and that
        // this feature has a non-null FID
        if( iFIDColumn == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot update feature: Layer \"%s\" has no FID column",
                      poFeatureDefn->GetName() );
            
            CSLDestroy( papszInsertCols );
            CPLFree( paiColToDefMap );
            
            return OGRERR_FAILURE;
        }
        else if( poFeature->GetFID() == OGRNullFID )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot update feature: Feature has a NULL Feature ID" );
            
            CSLDestroy( papszInsertCols );
            CPLFree( paiColToDefMap );
            
            return OGRERR_FAILURE;
        }
        
        // Build WHERE clause
        pszWhere = CPLSPrintf( "%s = %ld", osFIDColumnName.c_str(),
                                           poFeature->GetFID() );
        
        nSDEErr = SE_stream_update_table( hStream, poFeatureDefn->GetName(),
                                          nSpecialCols + nAttributeCols,
                                          (const char **)papszInsertCols,
                                          pszWhere );

        pszMethod = "SE_stream_update_table";
    }
        
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, pszMethod );
        return OGRERR_FAILURE;
    }

    
/* -------------------------------------------------------------------- */
/*      Set the feature attributes                                      */
/* -------------------------------------------------------------------- */
    short               iCurColNum = 1;
    
    if( nFIDColumnType == SE_REGISTRATION_ROW_ID_COLUMN_TYPE_USER
        && iFIDColumn != -1 )
    {
        LONG            nFID;
        
        nFID = poFeature->GetFID();
        if( nFID == OGRNullFID )
        {
            nFID = iNextFIDToWrite++;
            poFeature->SetFID( nFID );
        }
        
        nSDEErr = SE_stream_set_integer( hStream, iCurColNum++,
                                         &nFID );
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_stream_set_integer" );
            
            CSLDestroy( papszInsertCols );
            CPLFree( paiColToDefMap );
            
            return OGRERR_FAILURE;
        }
    }

    // Set geometry (shape) column
    if( poFeature->GetGeometryRef() != NULL )
    {
        nSDEErr = SE_stream_set_shape( hStream, iCurColNum++, hShape );
        
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_stream_set_shape" );
            
            CSLDestroy( papszInsertCols );
            CPLFree( paiColToDefMap );
            SE_shape_free( hShape );
            return OGRERR_FAILURE;
        }
    }
    
    // Set attribute columns
    for( i=0; i < nAttributeCols; i++ )
    {
        int             iFieldDefnIdx = paiColToDefMap[i];
        OGRFieldDefn   *poFieldDefn;
        OGRField       *poField;

        poFieldDefn = poFeatureDefn->GetFieldDefn(iFieldDefnIdx);
        poField = poFeature->GetRawFieldRef(iFieldDefnIdx);
        
        CPLAssert( poFieldDefn != NULL );
        CPLAssert( poField != NULL );
        
        if( poFieldDefn->GetType() == OFTInteger )
        {
            LONG        nLong = poField->Integer;
            
            nSDEErr = SE_stream_set_integer( hStream, iCurColNum++,
                                             &nLong );
            if( nSDEErr != SE_SUCCESS )
            {
                poDS->IssueSDEError( nSDEErr, "SE_stream_set_integer" );
                CSLDestroy( papszInsertCols );
                CPLFree( paiColToDefMap );
                return OGRERR_FAILURE;
            }
        }
        
        else if( poFieldDefn->GetType() == OFTReal )
        {
            LFLOAT      nDouble = poField->Real;
            
            nSDEErr = SE_stream_set_double( hStream, iCurColNum++,
                                            &nDouble );
            if( nSDEErr != SE_SUCCESS )
            {
                poDS->IssueSDEError( nSDEErr, "SE_stream_set_float" );
                CSLDestroy( papszInsertCols );
                CPLFree( paiColToDefMap );
                return OGRERR_FAILURE;
            }
        }
        
        else if( poFieldDefn->GetType() == OFTString 
                 && anFieldTypeMap[iFieldDefnIdx] == SE_NSTRING_TYPE )
        {
            SE_WCHAR *pszUTF16 = (SE_WCHAR *) 
                CPLRecodeToWChar( poField->String, CPL_ENC_UTF8, 
                                  CPL_ENC_UTF16 );

            nSDEErr = SE_stream_set_nstring( hStream, iCurColNum++,
                                             pszUTF16 );
            CPLFree( pszUTF16 );

            if( nSDEErr != SE_SUCCESS )
            {
                poDS->IssueSDEError( nSDEErr, "SE_stream_set_nstring" );
                CSLDestroy( papszInsertCols );
                CPLFree( paiColToDefMap );
                return OGRERR_FAILURE;
            }
        }
        
        else if( poFieldDefn->GetType() == OFTString )
        {
            nSDEErr = SE_stream_set_string( hStream, iCurColNum++,
                                            poField->String );
            if( nSDEErr != SE_SUCCESS )
            {
                poDS->IssueSDEError( nSDEErr, "SE_stream_set_string" );
                CSLDestroy( papszInsertCols );
                CPLFree( paiColToDefMap );
                return OGRERR_FAILURE;
            }
        }
        
        else if( poFieldDefn->GetType() == OFTDate
                 || poFieldDefn->GetType() == OFTDateTime )
        {
            struct tm sDateVal;
            
            // TODO: hobu, please double-check this.
            sDateVal.tm_year  = poField->Date.Year - 1900;
            sDateVal.tm_mon   = poField->Date.Month - 1;
            sDateVal.tm_mday  = poField->Date.Day;
            sDateVal.tm_hour  = poField->Date.Hour;
            sDateVal.tm_min   = poField->Date.Minute;
            sDateVal.tm_sec   = poField->Date.Second;
            sDateVal.tm_isdst = (poField->Date.TZFlag == 0 ? 0 : 1);
            
            nSDEErr = SE_stream_set_date( hStream, iCurColNum++,
                                          &sDateVal );
            if( nSDEErr != SE_SUCCESS )
            {
                poDS->IssueSDEError( nSDEErr, "SE_stream_set_date" );
                CSLDestroy( papszInsertCols );
                CPLFree( paiColToDefMap );
                return OGRERR_FAILURE;
            }
        }
        
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Cannot set attribute of type %s in SDE layer: "
                      "attempting to create as STRING",
                      OGRFieldDefn::GetFieldTypeName(poFieldDefn->GetType()) );
            
            CSLDestroy( papszInsertCols );
            CPLFree( paiColToDefMap );
        
            return OGRERR_FAILURE;
        }
    }

    CSLDestroy( papszInsertCols );
    CPLFree( paiColToDefMap );
    SE_shape_free(hShape);
    return OGRERR_NONE;
}

/************************************************************************/
/*                        TranslateOGRGeometry()                        */
/************************************************************************/
OGRErr OGRSDELayer::TranslateOGRGeometry( OGRGeometry *poGeom,
                                          SE_SHAPE *phShape,
                                          SE_COORDREF hCoordRef )
        
{
    LONG                nSDEErr;
    
    CPLAssert( poGeom != NULL );
    CPLAssert( phShape != NULL );
    
/* -------------------------------------------------------------------- */
/*      Initialize shape.                                               */
/* -------------------------------------------------------------------- */    
    nSDEErr = SE_shape_create( hCoordRef, phShape );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_shape_create" );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Determine whether the geometry includes Z coordinates.          */
/* -------------------------------------------------------------------- */    
    int                 b3D = FALSE;
    
    b3D = wkbHasZ(poGeom->getGeometryType());
    
/* -------------------------------------------------------------------- */
/*      Translate POINT/MULTIPOINT type.                                */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        OGRPoint       *poPoint = (OGRPoint *) poGeom;
        LONG            nParts = 1;
        SE_POINT      *asPointParts;
        LFLOAT         *anfZcoords;

        asPointParts = (SE_POINT*) CPLMalloc(nParts * sizeof(SE_POINT));
        if (!asPointParts) {
            CPLError( CE_Fatal, CPLE_AppDefined,
                      "Cannot allocate asPointParts" );
            return OGRERR_FAILURE;
        }
        
        anfZcoords = (LFLOAT*) CPLMalloc(nParts * sizeof(LFLOAT));
        if (!anfZcoords) {
            CPLError( CE_Fatal, CPLE_AppDefined,
                      "Cannot allocate anfZcoords" );
            return OGRERR_FAILURE;
        }        
        asPointParts[0].x = poPoint->getX();
        asPointParts[0].y = poPoint->getY();
        
        if( b3D )
        {
            anfZcoords[0] = poPoint->getZ();
            nSDEErr = SE_shape_generate_point( nParts, asPointParts, anfZcoords,
                                               NULL, *phShape );
        }
        else
        {
            nSDEErr = SE_shape_generate_point( nParts, asPointParts, NULL, NULL,
                                               *phShape );
        }

        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_shape_generate_point" );
            return OGRERR_FAILURE;
        }
    }

    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
    {
        OGRMultiPoint  *poMulPoint = (OGRMultiPoint *) poGeom;
        LONG            nParts;
        SE_POINT       *pasPointParts;
        LFLOAT         *panfZcoords = NULL;
        int             i;
        
        nParts = poMulPoint->getNumGeometries();
        
        pasPointParts = (SE_POINT *) CPLMalloc( sizeof(SE_POINT) * nParts );
        
        if( b3D )
            panfZcoords = (LFLOAT *) CPLMalloc( sizeof(LFLOAT) * nParts );
        
        for( i=0; i < nParts; i++ )
        {
            OGRPoint   *poThisPoint;
            
            poThisPoint = (OGRPoint *) poMulPoint->getGeometryRef(i);
                    
            pasPointParts[i].x = poThisPoint->getX();
            pasPointParts[i].y = poThisPoint->getY();
            
            if( b3D )
                panfZcoords[i] = poThisPoint->getZ();
        }
        
        nSDEErr = SE_shape_generate_point( nParts, pasPointParts, panfZcoords,
                                           NULL, *phShape );
        
        CPLFree( pasPointParts );
        if( b3D )
            CPLFree( panfZcoords );

        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_shape_generate_point" );
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate POLYGON/MULTIPOLYGON type.                            */
/* -------------------------------------------------------------------- */
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
    {
        OGRPolygon     *poPoly = (OGRPolygon *) poGeom;
        LONG            nPoints=0, iCurPoint=0;
        SE_POINT       *pasPoints;
        LFLOAT         *panfZcoords = NULL;
        int             i, j;
        
        // Get exterior ring
        OGRLinearRing  *poExtRing = poPoly->getExteriorRing();
        
        if( poExtRing == NULL )
        {
            // The polygon is empty
            // TODO: Does this mean that the shape is NULL, then?
            nSDEErr = SE_shape_make_nil( *phShape );
            if( nSDEErr != SE_SUCCESS )
            {
                poDS->IssueSDEError( nSDEErr, "SE_shape_make_nil" );
                return OGRERR_FAILURE;
            }
        }
        
        // Get total number of points in polygon
        nPoints += poExtRing->getNumPoints();
        
        for( i=0; i < poPoly->getNumInteriorRings(); i++ )
            nPoints += poPoly->getInteriorRing(i)->getNumPoints();
        
        pasPoints = (SE_POINT *) CPLMalloc( sizeof(SE_POINT) * nPoints );
        
        if( b3D )
            panfZcoords = (LFLOAT *) CPLMalloc( sizeof(LFLOAT) * nPoints );
        
        for( i=0; i < poExtRing->getNumPoints(); i++ )
        {
            OGRPoint    oLRPnt;
            
            poExtRing->getPoint( i, &oLRPnt );
            
            pasPoints[iCurPoint].x = oLRPnt.getX();
            pasPoints[iCurPoint].y = oLRPnt.getY();
            
            if( b3D )
                panfZcoords[iCurPoint] = oLRPnt.getZ();

            iCurPoint++;
        }
        
        for( i=0; i < poPoly->getNumInteriorRings(); i++ )
        {
            OGRLinearRing   *poIntRing = poPoly->getInteriorRing(i);
            
            for( j=0; j < poIntRing->getNumPoints(); j++ )
            {
                OGRPoint    oLRPnt;
                
                poIntRing->getPoint( j, &oLRPnt );
                
                pasPoints[iCurPoint].x = oLRPnt.getX();
                pasPoints[iCurPoint].y = oLRPnt.getY();
                
                if( b3D )
                    panfZcoords[iCurPoint] = oLRPnt.getZ();
                
                iCurPoint++;
            }
        }
        
        nSDEErr = SE_shape_generate_polygon( nPoints, 1, NULL, pasPoints,
                                             panfZcoords, NULL, *phShape );
        
        CPLFree( pasPoints );
        if( b3D )
            CPLFree( panfZcoords );
        
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_shape_generate_polygon" );
            return OGRERR_FAILURE;
        }
    }
    
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon )
    {
        OGRMultiPolygon *poMP = (OGRMultiPolygon *) poGeom;
        LONG             nPoints=0;
        LONG             nParts;
        LONG             iCurPoint=0;
        LONG            *panPartOffsets;
        SE_POINT        *pasPoints;
        LFLOAT          *panfZcoords = NULL;
        int              i, j, k;
        
        nParts = poMP->getNumGeometries();
        
        // Find total number of points
        for( i=0; i < nParts; i++ )
        {
            OGRPolygon      *poPoly = (OGRPolygon *) poMP->getGeometryRef(i);
            OGRLinearRing   *poExtRing = poPoly->getExteriorRing();

            nPoints += poExtRing->getNumPoints();

            for( j=0; j < poPoly->getNumInteriorRings(); j++ )
                nPoints += poPoly->getInteriorRing(j)->getNumPoints();
        }
        
        // Allocate points and part offset arrays
        pasPoints = (SE_POINT *) CPLMalloc( sizeof(SE_POINT) * nPoints );
        panPartOffsets = (LONG *) CPLMalloc( sizeof(LONG) * nParts );
        if( b3D )
            panfZcoords = (LFLOAT *) CPLMalloc( sizeof(LFLOAT) * nPoints );
        
        
        // Build arrays of points and part offsets
        for( i=0; i < nParts; i++ )
        {
            OGRPolygon      *poPoly = (OGRPolygon *) poMP->getGeometryRef(i);
            OGRLinearRing   *poExtRing = poPoly->getExteriorRing();
            
            panPartOffsets[i] = iCurPoint;
            
            for( j=0; j < poExtRing->getNumPoints(); j++ )
            {
                OGRPoint    oLRPnt;

                poExtRing->getPoint( j, &oLRPnt );

                pasPoints[iCurPoint].x = oLRPnt.getX();
                pasPoints[iCurPoint].y = oLRPnt.getY();
                
                if( b3D )
                    panfZcoords[iCurPoint] = oLRPnt.getZ();

                iCurPoint++;
            }

            for( j=0; j < poPoly->getNumInteriorRings(); j++ )
            {
                OGRLinearRing   *poIntRing = poPoly->getInteriorRing(j);

                for( k=0; k < poIntRing->getNumPoints(); k++ )
                {
                    OGRPoint    oLRPnt;

                    poIntRing->getPoint( k, &oLRPnt );

                    pasPoints[iCurPoint].x = oLRPnt.getX();
                    pasPoints[iCurPoint].y = oLRPnt.getY();
                    
                    if( b3D )
                        panfZcoords[iCurPoint] = oLRPnt.getZ();

                    iCurPoint++;
                }
            }
        }
        
        nSDEErr = SE_shape_generate_polygon( nPoints, nParts, panPartOffsets,
                                             pasPoints, panfZcoords, NULL,
                                             *phShape );
        
        CPLFree( pasPoints );
        CPLFree( panPartOffsets );
        if( b3D )
            CPLFree( panfZcoords );
        
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_shape_generate_polygon" );
            return OGRERR_FAILURE;
        }
    }

    
/* -------------------------------------------------------------------- */
/*      Translate LINESTRING/MULTILINESTRING type.                      */
/* -------------------------------------------------------------------- */    
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        OGRLineString  *poLineString = (OGRLineString *) poGeom;
        LONG            nParts = 1;
        LONG            nPoints;
        SE_POINT       *pasPoints;
        LFLOAT         *panfZcoords = NULL;
        int             i;
        
        nPoints = poLineString->getNumPoints();
        
        pasPoints = (SE_POINT *) CPLMalloc( sizeof(SE_POINT) * nPoints );
        if( b3D )
            panfZcoords = (LFLOAT *) CPLMalloc( sizeof(LFLOAT) * nPoints );
        
        for( i=0; i < nPoints; i++ )
        {
            OGRPoint   oPoint;
            
            poLineString->getPoint( i, &oPoint );

            pasPoints[i].x = oPoint.getX();
            pasPoints[i].y = oPoint.getY();
            
            if( b3D )
                panfZcoords[i] = oPoint.getZ();
        }
        
        nSDEErr = SE_shape_generate_line( nPoints, nParts, NULL, pasPoints,
                                          panfZcoords, NULL, *phShape );
        
        CPLFree( pasPoints );
        if( b3D )
            CPLFree( panfZcoords );
        
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_shape_generate_line" );
            return OGRERR_FAILURE;
        }
    }
    
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString )
    {
        OGRMultiLineString *poMLS = (OGRMultiLineString *) poGeom;
        LONG            nParts;
        LONG            nPoints = 0;
        LONG            iCurPoint = 0;
        SE_POINT       *pasPoints;
        LONG           *panPartOffsets;
        LFLOAT         *panfZcoords = NULL;
        int             i, j;
        
        // Get number of parts and total number of points
        nParts = poMLS->getNumGeometries();
        
        for( i=0; i < nParts; i++ )
        {
            OGRLineString   *poLS = (OGRLineString *) poMLS->getGeometryRef(i);
            
            nPoints += poLS->getNumPoints();
        }
        
        // Allocate arrays for points and part offsets
        pasPoints = (SE_POINT *) CPLMalloc( sizeof(SE_POINT) * nPoints );
        panPartOffsets = (LONG *) CPLMalloc( sizeof(LONG) * nParts );
        if( b3D )
            panfZcoords = (LFLOAT *) CPLMalloc( sizeof(LFLOAT) * nPoints );
        
        for( i=0; i < nParts; i++ )
        {
            OGRLineString   *poLS = (OGRLineString *) poMLS->getGeometryRef(i);
            
            panPartOffsets[i] = iCurPoint;
            
            for( j=0; j < poLS->getNumPoints(); j++ )
            {
                OGRPoint    oPoint;
                
                poLS->getPoint( j, &oPoint );
                
                pasPoints[iCurPoint].x = oPoint.getX();
                pasPoints[iCurPoint].y = oPoint.getY();
                
                if( b3D )
                    panfZcoords[iCurPoint] = oPoint.getZ();
                
                iCurPoint++;
            }
        }
        
        nSDEErr = SE_shape_generate_line( nPoints, nParts, panPartOffsets,
                                          pasPoints, panfZcoords, NULL,
                                          *phShape );
        
        CPLFree( pasPoints );
        CPLFree( panPartOffsets );
        if( b3D )
            CPLFree( panfZcoords );
        
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_shape_generate_line" );
            return OGRERR_FAILURE;
        }
    }
    
/* -------------------------------------------------------------------- */
/*       Error on other geometry types.                                 */
/* -------------------------------------------------------------------- */    
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "OGR_SDE: TranslateOGRGeometry() cannot translate "
                  "geometries of type %s (%d)", poGeom->getGeometryName(),
                  poGeom->getGeometryType() );
        
        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                        TranslateSDEGeometry()                        */
/************************************************************************/

OGRGeometry *OGRSDELayer::TranslateSDEGeometry( SE_SHAPE hShape )

{
    LONG nSDEGeomType;
    OGRGeometry *poGeom = NULL;

/* -------------------------------------------------------------------- */
/*      Fetch geometry type.                                            */
/* -------------------------------------------------------------------- */
    SE_shape_get_type( hShape, &nSDEGeomType );

    if( nSDEGeomType == SG_NIL_SHAPE )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Fetch points and parts.                                         */
/* -------------------------------------------------------------------- */
    LONG nPointCount, nPartCount, nSubPartCount, nSDEErr;
    SE_POINT *pasPoints;
    LONG *panSubParts;
    LONG *panParts;
    LFLOAT *padfZ = NULL;

    SE_shape_get_num_points( hShape, 0, 0, &nPointCount );
    SE_shape_get_num_parts( hShape, &nPartCount, &nSubPartCount );

    pasPoints = (SE_POINT *) CPLMalloc(nPointCount * sizeof(SE_POINT));
    panParts = (LONG *) CPLMalloc(nPartCount * sizeof(LONG));
    panSubParts = (LONG *) CPLMalloc(nSubPartCount * sizeof(LONG));
    
    if( SE_shape_is_3D( hShape ) )
        padfZ = (LFLOAT *) CPLMalloc(nPointCount * sizeof(LFLOAT));

    nSDEErr = SE_shape_get_all_points( hShape, SE_DEFAULT_ROTATION, 
                                       panParts, panSubParts, 
                                       pasPoints, padfZ, NULL );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_shape_get_all_points" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Handle simple point.                                            */
/* -------------------------------------------------------------------- */
    switch( nSDEGeomType )
    {
      case SG_POINT_SHAPE:
      {
          CPLAssert( nPointCount == 1 );
          CPLAssert( nSubPartCount == 1 );
          CPLAssert( nPartCount == 1 );
          if( padfZ )
              poGeom = new OGRPoint( pasPoints[0].x, pasPoints[0].y, padfZ[0] );
          else
              poGeom = new OGRPoint( pasPoints[0].x, pasPoints[0].y );
      }
      break;

/* -------------------------------------------------------------------- */
/*      Handle simple point.                                            */
/* -------------------------------------------------------------------- */
      case SG_MULTI_POINT_SHAPE:
      {
          OGRMultiPoint *poMP = new OGRMultiPoint();
          int iPart;

          CPLAssert( nPartCount == nSubPartCount ); // one vertex per point.
          CPLAssert( nPointCount == nPartCount );

          for( iPart = 0; iPart < nPartCount; iPart++ )
          {
              if( padfZ != NULL )
                  poMP->addGeometryDirectly( new OGRPoint( pasPoints[iPart].x,
                                                           pasPoints[iPart].y,
                                                           padfZ[iPart] ) );
              else
                  poMP->addGeometryDirectly( new OGRPoint( pasPoints[iPart].x,
                                                           pasPoints[iPart].y ) );
          }
          poGeom = poMP;
      }
      break;

/* -------------------------------------------------------------------- */
/*      Handle line.                                                    */
/* -------------------------------------------------------------------- */
      case SG_LINE_SHAPE:
      case SG_SIMPLE_LINE_SHAPE:
      {
          OGRLineString *poLine = new OGRLineString();
          int i;
          
          CPLAssert( nPartCount == 1 && nSubPartCount == 1 );

          poLine->setNumPoints( nPointCount );

          for( i = 0; i < nPointCount; i++ )
          {
              if( padfZ )
                  poLine->setPoint( i, pasPoints[i].x, pasPoints[i].y, 
                                    padfZ[i] );
              else
                  poLine->setPoint( i, pasPoints[i].x, pasPoints[i].y );
          }

          poGeom = poLine;
      }
      break;

/* -------------------------------------------------------------------- */
/*      Handle multi line.                                              */
/* -------------------------------------------------------------------- */
      case SG_MULTI_LINE_SHAPE:
      case SG_MULTI_SIMPLE_LINE_SHAPE:
      {
          OGRMultiLineString *poMLS = new OGRMultiLineString();
	  int iPart;

          CPLAssert( nPartCount == nSubPartCount );
              
          for( iPart = 0; iPart < nPartCount; iPart++ )
          {
              OGRLineString *poLine = new OGRLineString();
              int i, nLineVertCount;

              CPLAssert( panParts[iPart] == iPart ); // 1:1 correspondance
          
              if( iPart == nPartCount-1 )
                  nLineVertCount = nPointCount - panSubParts[iPart];
              else
                  nLineVertCount = panSubParts[iPart+1] - panSubParts[iPart];

              poLine->setNumPoints( nLineVertCount );
              
              for( i = 0; i < nLineVertCount; i++ )
              {
                  int iVert = i + panSubParts[iPart];

                  if( padfZ )
                      poLine->setPoint( i, 
                                        pasPoints[iVert].x, 
                                        pasPoints[iVert].y, 
                                        padfZ[iVert] );
                  else
                      poLine->setPoint( i, 
                                        pasPoints[iVert].x, 
                                        pasPoints[iVert].y );
              }

              poMLS->addGeometryDirectly( poLine );
          }

          poGeom = poMLS;
      }
      break;

/* -------------------------------------------------------------------- */
/*      Handle polygon and multipolygon.  Each subpart is a ring.       */
/* -------------------------------------------------------------------- */
      case SG_AREA_SHAPE:
      case SG_MULTI_AREA_SHAPE:
      {
          int iPart;
          OGRMultiPolygon *poMP = NULL;

          if( nSDEGeomType == SG_MULTI_AREA_SHAPE )
              poMP = new OGRMultiPolygon();

          for( iPart = 0; iPart < nPartCount; iPart++ )
          {
              OGRPolygon *poPoly = new OGRPolygon();
              int iVert, iSubPart;
              int nNextSubPart;

              if( iPart == nPartCount-1 )
                  nNextSubPart = nSubPartCount;
              else
                  nNextSubPart = panParts[iPart+1];
              
              for( iSubPart = panParts[iPart]; iSubPart < nNextSubPart; iSubPart++ )
              {
                  OGRLinearRing *poRing = new OGRLinearRing();
                  int nRingVertCount; 
                  
                  if( iSubPart == nSubPartCount-1 )
                      nRingVertCount = nPointCount - panSubParts[iSubPart];
                  else
                      nRingVertCount = 
                          panSubParts[iSubPart+1] - panSubParts[iSubPart];
                  
                  poRing->setNumPoints( nRingVertCount );
                  
                  for( iVert=0; iVert < nRingVertCount; iVert++ )
                  {
                      if( padfZ )
                          poRing->setPoint( 
                              iVert, 
                              pasPoints[iVert+panSubParts[iSubPart]].x,
                              pasPoints[iVert+panSubParts[iSubPart]].y,
                              padfZ[iVert+panSubParts[iSubPart]] );
                      else
                          poRing->setPoint( 
                              iVert, 
                              pasPoints[iVert+panSubParts[iSubPart]].x,
                              pasPoints[iVert+panSubParts[iSubPart]].y );
                  }
                  
                  poPoly->addRingDirectly( poRing );
              }

              if( poMP )
                  poMP->addGeometryDirectly( poPoly );
              else
                  poGeom = poPoly;
          }

          if( poMP )
              poGeom = poMP;
      }
      break;

/* -------------------------------------------------------------------- */
/*      Report unsupported geometries.                                  */
/* -------------------------------------------------------------------- */
      default:
      {
          CPLError( CE_Warning, CPLE_NotSupported, 
                    "Unsupported geometry type: %d", 
                    (int) nSDEGeomType );
      }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pasPoints );
    CPLFree( panParts );
    CPLFree( panSubParts );
    CPLFree( padfZ );

    return poGeom;
}

/************************************************************************/
/*                         TranslateSDERecord()                         */
/************************************************************************/

OGRFeature *OGRSDELayer::TranslateSDERecord()

{
    unsigned int i;
    int nSDEErr;
    OGRFeature *poFeat = new OGRFeature( poFeatureDefn );

/* -------------------------------------------------------------------- */
/*      Translate field values.                                         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < anFieldMap.size(); i++ )
    {
        OGRFieldDefn *poFieldDef = poFeatureDefn->GetFieldDefn( i );

        switch( anFieldTypeMap[i] )
        {
          case SE_SMALLINT_TYPE:
          {
              short   nShort;
              nSDEErr = SE_stream_get_smallint( hStream, anFieldMap[i]+1, 
                                                &nShort );
              if( nSDEErr == SE_SUCCESS )
                  poFeat->SetField( i, (int) nShort );
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_smallint" );
                  return NULL;
              }
          }
          break;

          case SE_INTEGER_TYPE:
          {
              LONG nValue;
              nSDEErr = SE_stream_get_integer( hStream, anFieldMap[i]+1, 
                                               &nValue );
              if( nSDEErr == SE_SUCCESS )
                  poFeat->SetField( i, (int) nValue );
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_integer" );
                  return NULL;
              }
          }
          break;

          case SE_FLOAT_TYPE:
          {
              float fValue;

              nSDEErr = SE_stream_get_float( hStream, anFieldMap[i]+1, 
                                             &fValue );
              if( nSDEErr == SE_SUCCESS )
                  poFeat->SetField( i, (double) fValue );
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_float" );
                  return NULL;
              }
          }
          break;

          case SE_DOUBLE_TYPE:
          {
              double dfValue;

              nSDEErr = SE_stream_get_double( hStream, anFieldMap[i]+1, 
                                              &dfValue );
              if( nSDEErr == SE_SUCCESS )
                  poFeat->SetField( i, dfValue );
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_double" );
                  return NULL;
              }
          }
          break;

          case SE_STRING_TYPE:
          {
              char *pszTempString = (char *)
                  CPLMalloc(poFieldDef->GetWidth()+1);

              nSDEErr = SE_stream_get_string( hStream, anFieldMap[i]+1, 
                                              pszTempString );
              if( nSDEErr == SE_SUCCESS )
                  poFeat->SetField( i, pszTempString );
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_string" );
                  return NULL;
              }
              CPLFree( pszTempString );
          }
          break;

          case SE_NSTRING_TYPE:
          {
              SE_WCHAR * pszTempStringUTF16 = (SE_WCHAR *) 
                  CPLMalloc ((poFieldDef->GetWidth()+1) * sizeof(SE_WCHAR ));

              nSDEErr = SE_stream_get_nstring( hStream, anFieldMap[i]+1, 
                                               pszTempStringUTF16 );

              if( nSDEErr == SE_SUCCESS ) 
              {
                  char* pszUTF8 = CPLRecodeFromWChar((const wchar_t*)pszTempStringUTF16, CPL_ENC_UTF16, CPL_ENC_UTF8);

                  poFeat->SetField( i, pszUTF8 );
                  CPLFree( pszUTF8 );

              } 
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_nstring" );
                  CPLFree( pszTempStringUTF16 );

                  return NULL;
              }
              CPLFree( pszTempStringUTF16 );
          }
          break;

#ifdef SE_UUID_TYPE
          case SE_UUID_TYPE:
          {
              char *pszTempString = (char *)
                  CPLMalloc(poFieldDef->GetWidth()+1);

              nSDEErr = SE_stream_get_uuid( hStream, anFieldMap[i]+1, 
                                              pszTempString );
              if( nSDEErr == SE_SUCCESS )
                  poFeat->SetField( i, pszTempString );
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_string" );
                  return NULL;
              }
              CPLFree( pszTempString );
          }
          break;
#endif
          case SE_BLOB_TYPE:
          {
              SE_BLOB_INFO sBlobVal;

              nSDEErr = SE_stream_get_blob( hStream, anFieldMap[i]+1, 
                                            &sBlobVal );
              if( nSDEErr == SE_SUCCESS )
              {
                  poFeat->SetField( i, 
                                    sBlobVal.blob_length, 
                                    (GByte *) sBlobVal.blob_buffer );
                  SE_blob_free( &sBlobVal );
              }
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_blob" );
                  return NULL;
              }
          }
          break;

#ifdef SE_CLOB_TYPE
          case SE_CLOB_TYPE:
          {
              SE_CLOB_INFO sClobVal;

              memset(&sClobVal, 0, sizeof(sClobVal)); /* to prevent from the crash in SE_stream_get_clob */
              nSDEErr = SE_stream_get_clob( hStream, anFieldMap[i]+1, 
                                            &sClobVal );
              if( nSDEErr == SE_SUCCESS )
              {
                  /* the returned string is not null-terminated */
                  char* sClobstring = (char*)CPLMalloc(sizeof(char)*(sClobVal.clob_length+1));
                  memcpy(sClobstring, sClobVal.clob_buffer, sClobVal.clob_length);
				  sClobstring[sClobVal.clob_length] = '\0';
                  
                  poFeat->SetField( i, sClobstring );
                  SE_clob_free( &sClobVal );
                  CPLFree(sClobstring);
              }
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_clob" );
                  return NULL;
              }
          }
          break;

#endif

#ifdef SE_NCLOB_TYPE
          case SE_NCLOB_TYPE:
          {
              SE_NCLOB_INFO sNclobVal;

              memset(&sNclobVal, 0, sizeof(sNclobVal)); /* to prevent from the crash in SE_stream_get_nclob */
              nSDEErr = SE_stream_get_nclob( hStream, anFieldMap[i]+1, 
                                            &sNclobVal );
              if( nSDEErr == SE_SUCCESS )
              {
                  /* the returned string is not null-terminated */
                  SE_WCHAR* sNclobstring = (SE_WCHAR*)CPLMalloc(sizeof(char)*(sNclobVal.nclob_length+2));
                  memcpy(sNclobstring, sNclobVal.nclob_buffer, sNclobVal.nclob_length);
				  sNclobstring[sNclobVal.nclob_length / 2] = '\0';

                  char* pszUTF8 = CPLRecodeFromWChar((const wchar_t*)sNclobstring, CPL_ENC_UTF16, CPL_ENC_UTF8);

                  poFeat->SetField( i, pszUTF8 );
                  CPLFree( pszUTF8 );
                  
                  SE_nclob_free( &sNclobVal );
                  CPLFree(sNclobstring);
              }
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_nclob" );
                  return NULL;
              }
          }
          break;

#endif

          case SE_DATE_TYPE:
          {
              struct tm sDateVal;

              nSDEErr = SE_stream_get_date( hStream, anFieldMap[i]+1, 
                                            &sDateVal );
              if( nSDEErr == SE_SUCCESS )
			  {
			      poFeat->SetField( i, sDateVal.tm_year + 1900, sDateVal.tm_mon + 1, sDateVal.tm_mday, 
					  sDateVal.tm_hour, sDateVal.tm_min, sDateVal.tm_sec, (sDateVal.tm_isdst > 0));
              }
              else if( nSDEErr != SE_NULL_VALUE )
              {
                  poDS->IssueSDEError( nSDEErr, "SE_stream_get_date" );
                  return NULL;
              }
          }
          break;

        }
    }

/* -------------------------------------------------------------------- */
/*      Apply FID.                                                      */
/* -------------------------------------------------------------------- */
    if( iFIDColumn != -1 )
        poFeat->SetFID( poFeat->GetFieldAsInteger( iFIDColumn ) );
    else
        poFeat->SetFID( nNextFID++ );

/* -------------------------------------------------------------------- */
/*      Fetch geometry.                                                 */
/* -------------------------------------------------------------------- */
    if( iShapeColumn != -1 )
    {
        SE_SHAPE hShape = 0;

        nSDEErr = SE_shape_create( NULL, &hShape );

        if( nSDEErr != SE_SUCCESS )
            poDS->IssueSDEError( nSDEErr, "SE_shape_create" );
        else
        {
            nSDEErr = SE_stream_get_shape( hStream, (short) (iShapeColumn+1), 
                                           hShape );
            if( nSDEErr != SE_SUCCESS )
                poDS->IssueSDEError( nSDEErr, "SE_stream_get_shape" );
        }

        if( nSDEErr == SE_SUCCESS )
            poFeat->SetGeometryDirectly( TranslateSDEGeometry( hShape ) );

        SE_shape_free( hShape );
    }

    return poFeat;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSDELayer::GetNextFeature()

{
    int nSDEErr;

/* -------------------------------------------------------------------- */
/*      Make sure we have an installed query executed.                  */
/* -------------------------------------------------------------------- */
    if( !bQueryInstalled && !InstallQuery( FALSE ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Fetch the next record.                                          */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        nSDEErr = SE_stream_fetch( hStream );
        if( nSDEErr == SE_FINISHED )
        {
            bQueryInstalled = FALSE;
            return NULL;
        }
        else if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_stream_fetch" );
            return NULL;
        }
        
        m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Translate into an OGRFeature.                                   */
/* -------------------------------------------------------------------- */
        OGRFeature *poFeature;

        poFeature = TranslateSDERecord();

        if( poFeature != NULL )
        {
            if( m_poFilterGeom == NULL
                || m_bFilterIsEnvelope
                || FilterGeometry( poFeature->GetGeometryRef() ) )
                return poFeature;

            delete poFeature;
        }
    }        
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRSDELayer::GetFeature( GIntBig nFeatureId )

{
    int nSDEErr;
    
    if( iFIDColumn == -1 )
        return OGRLayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Our direct row access will terminate any active queries.        */
/* -------------------------------------------------------------------- */
    ResetReading(); 

/* -------------------------------------------------------------------- */
/*      Create stream, or reset it.                                     */
/* -------------------------------------------------------------------- */
    if( ResetStream() != OGRERR_NONE )
        return NULL;

/* -------------------------------------------------------------------- */
/*      We want to fetch all the columns, just like we normally         */
/*      would for GetNextFeature().                                     */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_stream_fetch_row( hStream, poFeatureDefn->GetName(), 
                                   nFeatureId, 
                                   CSLCount( papszAllColumns ), 
                                   (const char **) papszAllColumns );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_stream_fetch_row" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      We got our row, now translate it.                               */
/* -------------------------------------------------------------------- */
    return TranslateSDERecord();
}


/************************************************************************/
/*                          ResetStream()                               */
/*                                                                      */
/*      Create or reset stream environment                              */
/************************************************************************/

OGRErr OGRSDELayer::ResetStream()

{
    LONG                nSDEErr;
    
    if( hStream == NULL )
    {
        nSDEErr = SE_stream_create( poDS->GetConnection(), &hStream );
        if( nSDEErr != SE_SUCCESS) {
            poDS->IssueSDEError( nSDEErr, "SE_stream_create" );
            return OGRERR_FAILURE;
        }
        if (poDS->IsOpenForUpdate() && poDS->UseVersionEdits()) {
            nSDEErr = SE_stream_set_state(  hStream, 
                                            poDS->GetNextState(), 
                                            SE_NULL_STATE_ID, 
                                            SE_STATE_DIFF_NOCHECK );
            if( nSDEErr != SE_SUCCESS) {
                poDS->IssueSDEError( nSDEErr, "SE_stream_set_state" );
                return OGRERR_FAILURE;
            }
        }
        else {
            nSDEErr = SE_stream_set_state(  hStream, 
                                            poDS->GetState(), 
                                            poDS->GetState(), 
                                            SE_STATE_DIFF_NOCHECK );
            if( nSDEErr != SE_SUCCESS) {
                poDS->IssueSDEError( nSDEErr, "SE_stream_set_state" );
                return OGRERR_FAILURE;
            }
        }   
    }
    else
    {
        nSDEErr = SE_stream_close( hStream, TRUE );
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_stream_close" );
            return OGRERR_FAILURE;
        }
        if (poDS->IsOpenForUpdate() && poDS->UseVersionEdits()) {
            nSDEErr = SE_stream_set_state(  hStream, 
                                            poDS->GetNextState(), 
                                            SE_NULL_STATE_ID, 
                                            SE_STATE_DIFF_NOCHECK );
            if( nSDEErr != SE_SUCCESS) {
                poDS->IssueSDEError( nSDEErr, "SE_stream_set_state" );
                return OGRERR_FAILURE;
            }
        }
        else {
            nSDEErr = SE_stream_set_state(  hStream, 
                                            poDS->GetState(), 
                                            poDS->GetState(), 
                                            SE_STATE_DIFF_NOCHECK );
            if( nSDEErr != SE_SUCCESS) {
                poDS->IssueSDEError( nSDEErr, "SE_stream_set_state" );
                return OGRERR_FAILURE;
            }
        }        

    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      Issue a special "counter only" query that will just fetch       */
/*      objectids, and count the result set.  This will inherently      */
/*      include all the spatial and attribute filtering logic in the    */
/*      database.  It would be nice if we could also use a COUNT()      */
/*      operator in the database.                                       */
/************************************************************************/

GIntBig OGRSDELayer::GetFeatureCount( int bForce )

{
/* -------------------------------------------------------------------- */
/*      If there is no attribute or spatial filter in place, then       */
/*      use the SDE function call to obtain the number of               */
/*      features. The performance difference between this and manual    */
/*      iteration is significant.                                       */
/* -------------------------------------------------------------------- */
    if( osAttributeFilter.empty() && m_poFilterGeom == NULL 
        && NeedLayerInfo() )
    {
        SE_LAYER_STATS layerstats = {0};
        char szTableName[SE_QUALIFIED_TABLE_NAME];
        char szShapeColumn[SE_MAX_COLUMN_LEN];
        int nSDEErr;

        szTableName[0] = '\0';
        szShapeColumn[0] = '\0';
      
        nSDEErr = SE_layerinfo_get_spatial_column( hLayerInfo, szTableName, szShapeColumn );
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_layerinfo_get_spatial_column" );
            return -1;
        }
      
        nSDEErr = SE_layer_get_statistics( poDS->GetConnection(), szTableName, szShapeColumn,
                                           &layerstats);
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_layer_get_statistics" );
            return -1;
        }

        return layerstats.TotalFeatures;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise use direct reading of the result set, though we       */
/*      skip translating into OGRFeatures at least.                     */
/* -------------------------------------------------------------------- */
    int nSDEErr, nFeatureCount = 0;

    ResetReading();
    if( !InstallQuery( TRUE ) )
        return -1;

    for( nSDEErr = SE_stream_fetch( hStream ); 
         nSDEErr == SE_SUCCESS;
         nSDEErr = SE_stream_fetch( hStream ) )
    {
        nFeatureCount++;
    }

    if( nSDEErr != SE_FINISHED )
    {
        poDS->IssueSDEError( nSDEErr, "SE_stream_fetch" );
        return -1;
    }
        
    ResetReading();

    return nFeatureCount;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRSDELayer::GetExtent (OGREnvelope *psExtent, int bForce)

{
    if( !NeedLayerInfo() )
        return OGRERR_FAILURE;
    
    if (bForce) {
        return OGRLayer::GetExtent( psExtent, bForce );
    }        

    SE_ENVELOPE  sEnvelope;
    int nSDEErr;

    nSDEErr = SE_layerinfo_get_envelope( hLayerInfo, &sEnvelope );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_layerinfo_get_envelope" );
        return OGRERR_FAILURE;
    }

    psExtent->MinX = sEnvelope.minx;
    psExtent->MinY = sEnvelope.miny;
    psExtent->MaxX = sEnvelope.maxx;
    psExtent->MaxY = sEnvelope.maxy;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/
OGRErr OGRSDELayer::CreateField( OGRFieldDefn *poFieldIn, int bApproxOK )

{
    SE_COLUMN_DEF       sColumnDef;
    OGRFieldDefn        oField( poFieldIn );
    LONG                nSDEErr;
    
    CPLAssert( poFieldIn != NULL );
    CPLAssert( poFeatureDefn != NULL );
    
    /* TODO: Do we need to launder column names in the same way that OCI/PG
     * do? If so, do we also need to launder table names? */
    strncpy( sColumnDef.column_name, oField.GetNameRef(), SE_MAX_COLUMN_LEN );

    sColumnDef.nulls_allowed = TRUE;
    sColumnDef.decimal_digits = 0;
    
/* -------------------------------------------------------------------- */
/*      Set the new column's SDE type. We intentionally use deprecated  */
/*      SDE field types for backwards compatibility with 8.x servers    */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
        sColumnDef.sde_type = SE_INTEGER_TYPE;

    else if( oField.GetType() == OFTReal )
        sColumnDef.sde_type = SE_DOUBLE_TYPE;

    else if( oField.GetType() == OFTString )
    {
        const char *pszUseNSTRING = 
            CPLGetConfigOption( "OGR_SDE_USE_NSTRING", "FALSE" );

        if( bUseNSTRING || CSLTestBoolean( pszUseNSTRING ) )
            sColumnDef.sde_type = SE_NSTRING_TYPE;
        else
            sColumnDef.sde_type = SE_STRING_TYPE;
    }
    else if(    oField.GetType() == OFTDate
             || oField.GetType() == OFTTime
             || oField.GetType() == OFTDateTime
           )
        sColumnDef.sde_type = SE_DATE_TYPE;

    else if( bApproxOK )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Can't create field %s with type %s on SDE layers - creating "
                  "as SE_STRING_TYPE.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        
        sColumnDef.sde_type = SE_STRING_TYPE;
    }

    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on SDE layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        
        return OGRERR_FAILURE;
    }

    
/* -------------------------------------------------------------------- */
/*      Set field width and precision                                   */
/* -------------------------------------------------------------------- */
    if( bPreservePrecision && oField.GetWidth() != 0 )
    {
        sColumnDef.size = oField.GetWidth();

        if( oField.GetPrecision() != 0 && oField.GetType() == OFTReal )
            sColumnDef.decimal_digits = oField.GetPrecision();

        else if( oField.GetType() == OFTReal )
        {
            /* Float types require a >0 decimal_digits */
            sColumnDef.decimal_digits = 6;
        }
    }
    else if( !bPreservePrecision || oField.GetWidth() == 0 )
    {
        if( oField.GetType() == OFTReal )
        {
            sColumnDef.size = 24;
            sColumnDef.decimal_digits = 6;
        }
        else
        {
           /* Set the size and decimal digits to 0, which instructs SDE to use
            * DBMS-sensible defaults for these columns
            */
            sColumnDef.size = 0;
        }
    }
    

/* -------------------------------------------------------------------- */
/*      Create the new field                                            */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_table_add_column( poDS->GetConnection(),
                                   poFeatureDefn->GetName(),
                                   &sColumnDef );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_table_add_column" );
        return OGRERR_FAILURE;
    }

    poFeatureDefn->AddFieldDefn( &oField );
    anFieldTypeMap.push_back( sColumnDef.sde_type );
    
    return OGRERR_NONE;
}


/************************************************************************/
/*                           ISetFeature()                               */
/************************************************************************/
OGRErr OGRSDELayer::ISetFeature( OGRFeature *poFeature )

{
    LONG                nSDEErr;
    
    CPLAssert( poFeature != NULL );
    CPLAssert( poFeatureDefn != NULL );
    
    if( !NeedLayerInfo() ) // Need hCoordRef, layerinfo shape types
        return OGRERR_FAILURE;
    
    ResetReading();
    
    if( ResetStream() != OGRERR_NONE )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Delegate setup of update operation to TranslateOGRRecord()      */
/* -------------------------------------------------------------------- */
    if( TranslateOGRRecord( poFeature, FALSE ) != OGRERR_NONE )
        return OGRERR_FAILURE; // TranslateOGRRecord() will report the error
    
/* -------------------------------------------------------------------- */
/*      Execute the update                                              */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_stream_execute( hStream );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_stream_execute" );
        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/
OGRErr OGRSDELayer::ICreateFeature( OGRFeature *poFeature )

{
    LONG                nSDEErr;
    
    CPLAssert( poFeature != NULL );
    CPLAssert( poFeatureDefn != NULL );
    
    if( !NeedLayerInfo() ) // Need hCoordRef, layerinfo shape types
        return OGRERR_FAILURE;
    
    ResetReading();
    
    if( ResetStream() != OGRERR_NONE )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Delegate setup of insert operation to TranslateOGRRecord()      */
/* -------------------------------------------------------------------- */
    if( TranslateOGRRecord( poFeature, TRUE ) != OGRERR_NONE )
        return OGRERR_FAILURE; // TranslateOGRRecord() will report the error
    
/* -------------------------------------------------------------------- */
/*      Execute the insert                                              */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_stream_execute( hStream );
    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_stream_execute" );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      If ROWIDs are managed by SDE, then get the last inserted ID     */
/*      which is the feature ID for this new feature. If the ROWID      */
/*      is USER-managed, then TranslateOGRRecord() will have set        */
/*      the FID.                                                        */
/* -------------------------------------------------------------------- */
    if( nFIDColumnType == SE_REGISTRATION_ROW_ID_COLUMN_TYPE_SDE )
    {
        LONG            nLastFID;

        nSDEErr = SE_stream_last_inserted_row_id( hStream, &nLastFID );
        if( nSDEErr != SE_SUCCESS )
        {
            poDS->IssueSDEError( nSDEErr, "SE_stream_last_inserted_row_id" );
            return OGRERR_FAILURE;
        }
        
        poFeature->SetFID( nLastFID );
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/
OGRErr OGRSDELayer::DeleteFeature( GIntBig nFID )

{
    LONG                nSDEErr;
    const char         *pszWhere;
    
    ResetReading();
    
    if( ResetStream() != OGRERR_NONE )
        return OGRERR_FAILURE;
    
/* -------------------------------------------------------------------- */
/*      Verify that this layer has a FID column                         */
/* -------------------------------------------------------------------- */    
    if( iFIDColumn == -1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Layer \"%s\": cannot DeleteFeature(%ld): the layer has no "
                  "FID column detected.", poFeatureDefn->GetName(), nFID );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Perform the deletion                                            */
/* -------------------------------------------------------------------- */
    pszWhere = CPLSPrintf( "%s = %ld", osFIDColumnName.c_str(), nFID );
    
    nSDEErr = SE_stream_delete_from_table( hStream, poFeatureDefn->GetName(),
                                           pszWhere );
    
    if( nSDEErr == SE_NO_ROWS_DELETED )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Layer \"%s\": Tried to delete a feature by FID, but no "
                  "rows were deleted!",
                  poFeatureDefn->GetName() );
    }
    else if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_stream_delete_from_table" );
        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSDELayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return iFIDColumn != -1;

    else if( EQUAL(pszCap,OLCFastFeatureCount) 
             && osAttributeFilter.empty()
             && m_poFilterGeom == NULL )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;
    
    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCSequentialWrite)
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;
    
    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
    {
        // We always treat NSTRING fields by translating to UTF8, but
        // we don't do anything to regular string fields so this is a
        // bit hard to answer simply.  Also, whether writes support UTF8
        // depend on whether the field(s) were created as NSTRING fields. 
        return TRUE;
    }
    else 
        return FALSE;
}
/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRSDELayer::GetSpatialRef()

{
    NeedLayerInfo();

    return poSRS;
}
