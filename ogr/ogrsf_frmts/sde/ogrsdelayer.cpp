/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSDELayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.9  2006/04/11 01:26:10  fwarmerdam
 * First draft multipoint support, fixing multipolygons.
 *
 * Revision 1.8  2006/03/09 22:50:43  fwarmerdam
 * Added  support for multipolygons
 *
 * Revision 1.7  2006/03/08 00:22:46  fwarmerdam
 * preliminary update to support rowid-less tables
 *
 * Revision 1.6  2005/11/30 01:53:31  fwarmerdam
 * added OGR_SDE_SEARCHORDER config option
 *
 * Revision 1.5  2005/11/30 01:34:02  fwarmerdam
 * Ensure layerinfo is loaded for GetSpatialRef().
 *
 * Revision 1.4  2005/11/25 18:55:15  fwarmerdam
 * layer implementation essentially complete
 *
 * Revision 1.3  2005/11/25 05:58:27  fwarmerdam
 * preliminary operation of feature reading
 *
 * Revision 1.2  2005/11/22 17:09:17  fwarmerdam
 * Preinitalize poFeature in GetNextFeature().
 *
 * Revision 1.1  2005/11/22 17:01:48  fwarmerdam
 * New
 *
 */

#include "ogr_sde.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRSDELayer()                             */
/************************************************************************/

OGRSDELayer::OGRSDELayer( OGRSDEDataSource *poDSIn )

{
    poDS = poDSIn;

    iFIDColumn = -1;
    nNextFID = 0;

    iShapeColumn = -1;
    poSRS = NULL;
    poFeatureDefn = NULL;

    bQueryInstalled = FALSE;
    hStream = NULL;
    hCoordRef = NULL;
    papszAllColumns = NULL;
    bHaveLayerInfo = FALSE;
}

/************************************************************************/
/*                            ~OGRSDELayer()                            */
/************************************************************************/

OGRSDELayer::~OGRSDELayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "SDE", "%d features read on layer '%s'.",
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

    nSDEErr = 
        SE_table_describe( poDS->GetConnection(), pszTableName, 
                           &nColumnCount, &asColumnDefs );

    if( nSDEErr != SE_SUCCESS )
    {
        poDS->IssueSDEError( nSDEErr, "SE_table_describe" );
        return FALSE;
    }

    poFeatureDefn = new OGRFeatureDefn( pszTableName );
    poFeatureDefn->Reference();

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
            eOGRType = OFTString;
            nWidth = asColumnDefs[iCol].size;
            break;

          case SE_BLOB_TYPE:
            eOGRType = OFTBinary;
            break;

          case SE_DATE_TYPE:
            eOGRType = OFTString;
            nWidth = asColumnDefs[iCol].size;
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
            iFIDColumn = anFieldMap.size() - 1;
    }

    SE_table_free_descriptions( asColumnDefs );

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
        char szWKT[4000];

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
    }

    return TRUE;
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
    if( hStream == NULL )
    {
        nSDEErr = SE_stream_create( poDS->GetConnection(), &hStream );
        if( nSDEErr != SE_SUCCESS) {
            poDS->IssueSDEError( nSDEErr, "SE_stream_create" );
            return FALSE;
        }
    }
    else
    {
        nSDEErr = SE_stream_close( hStream, TRUE );
    }

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

        SE_shape_create( hCoordRef, &hRectShape );
        sEnvelope.minx = m_sFilterEnvelope.MinX;
        sEnvelope.miny = m_sFilterEnvelope.MinY;
        sEnvelope.maxx = m_sFilterEnvelope.MaxX;
        sEnvelope.maxy = m_sFilterEnvelope.MaxY;
        SE_shape_generate_rectangle( &sEnvelope, hRectShape );

        sConstraint.filter.shape = hRectShape;
        strcpy( sConstraint.table, poFeatureDefn->GetName() );
        strcpy( sConstraint.column, osShapeColumnName.c_str() );
        sConstraint.method = SM_ENVP;
        sConstraint.filter_type = SE_SHAPE_FILTER;
        sConstraint.truth = TRUE;

        nSDEErr = SE_stream_set_spatial_constraints( hStream, SE_SPATIAL_FIRST,
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
    if( pszQuery == NULL )
        osAttributeFilter = "";
    else
        osAttributeFilter = pszQuery;

    ResetReading();

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
              nNextSubPart;

              if( iPart == nPartCount-1 )
                  nNextSubPart = nSubPartCount;
              else
                  nNextSubPart = panPart[iPart+1];
              
              for( iSubPart = panPart[iPart]; iSubPart < nNextSubPart; iSubPart++ )
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
                    nSDEGeomType );
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

          case SE_DATE_TYPE:
          {
              struct tm sDateVal;

              nSDEErr = SE_stream_get_date( hStream, anFieldMap[i]+1, 
                                            &sDateVal );
              if( nSDEErr == SE_SUCCESS )
              {
                  char szDate[128];
                  strftime( szDate, sizeof(szDate), "%T %m/%d/%Y", &sDateVal );
                  poFeat->SetField( i, szDate );
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

OGRFeature *OGRSDELayer::GetFeature( long nFeatureId )

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
    if( hStream == NULL )
    {
        nSDEErr = SE_stream_create( poDS->GetConnection(), &hStream );
        if( nSDEErr != SE_SUCCESS) {
            poDS->IssueSDEError( nSDEErr, "SE_stream_create" );
            return FALSE;
        }
    }
    else
    {
        nSDEErr = SE_stream_close( hStream, TRUE );
    }

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
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      Issue a special "counter only" query that will just fetch       */
/*      objectids, and count the result set.  This will inherently      */
/*      include all the spatial and attribute filtering logic in the    */
/*      database.  It would be nice if we could also use a COUNT()      */
/*      operator in the database.                                       */
/************************************************************************/

int OGRSDELayer::GetFeatureCount( int bForce )

{
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
/*                           TestCapability()                           */
/************************************************************************/

int OGRSDELayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return iFIDColumn != -1;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

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
