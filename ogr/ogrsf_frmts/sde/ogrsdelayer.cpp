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
    iShapeColumn = -1;
//    poSRS = NULL;
    poFeatureDefn = NULL;

    bQueryInstalled = FALSE;
    hStream = NULL;
    papszAllColumns = NULL;
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

    if( hStream )
    {
        SE_stream_free( hStream );
        hStream = NULL;
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();
//    if( poSRS )
//        poSRS->Release();

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
        int nWidth = -1;

        papszAllColumns = CSLAddString( papszAllColumns, 
                                        asColumnDefs[iCol].column_name );

        switch( asColumnDefs[iCol].sde_type )
        {
          case SE_SMALLINT_TYPE:
          case SE_INTEGER_TYPE:
            eOGRType = OFTInteger;
            break;

          case SE_FLOAT_TYPE:
          case SE_DOUBLE_TYPE:
            eOGRType = OFTReal;
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
        // TODO: add width and precision. 

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
/*                            ResetReading()                            */
/************************************************************************/

void OGRSDELayer::ResetReading()

{
    bQueryInstalled = FALSE;
}

/************************************************************************/
/*                            InstallQuery()                            */
/*                                                                      */
/*      Setup the stream with current query characteristics.            */
/************************************************************************/

int OGRSDELayer::InstallQuery()

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
    nSDEErr = SE_queryinfo_set_where_clause( hQueryInfo, ""); // fill in later.
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
/*      Establish the columns to query.                                 */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_queryinfo_set_columns( hQueryInfo, CSLCount(papszAllColumns),
                                        (const char **) papszAllColumns );
    if( nSDEErr != SE_SUCCESS) {
        poDS->IssueSDEError( nSDEErr, "SE_queryinfo_set_columns" );
        return FALSE;
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
    // TODO

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
          if( padfZ )
              poGeom = new OGRPoint( pasPoints[0].x, pasPoints[0].y, padfZ[0] );
          else
              poGeom = new OGRPoint( pasPoints[0].x, pasPoints[0].y );
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
/*      Handle polygon.  Each subpart is a ring.                        */
/* -------------------------------------------------------------------- */
      case SG_AREA_SHAPE:
      {
          OGRPolygon *poPoly = new OGRPolygon();
          int iVert, iSubPart;

          CPLAssert( nPartCount == 1 );

          for( iSubPart = 0; iSubPart < nSubPartCount; iSubPart++ )
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

          poGeom = poPoly;
      }
      break;
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
    if( !bQueryInstalled && !InstallQuery() )
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
            if( (m_poFilterGeom == NULL
                 || FilterGeometry( poFeature->GetGeometryRef() ) )
                && (m_poAttrQuery == NULL
                    || m_poAttrQuery->Evaluate( poFeature )) )
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
//    OGRFeature *poFeature = 
//         SHPReadOGRFeature( hSHP, hDBF, poFeatureDefn, nFeatureId );

//    if( poFeature != NULL )
//        m_nFeaturesRead++;

//    return poFeature;

    return NULL;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

#ifdef notdef
int OGRSDELayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return nTotalShapeCount;
}
#endif

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

#ifdef notdef
OGRErr OGRSDELayer::GetExtent (OGREnvelope *psExtent, int bForce)

{
    double adMin[4], adMax[4];

    if( hSHP == NULL )
        return OGRERR_FAILURE;

    SHPGetInfo(hSHP, NULL, NULL, adMin, adMax);

    psExtent->MinX = adMin[0];
    psExtent->MinY = adMin[1];
    psExtent->MaxX = adMax[0];
    psExtent->MaxY = adMax[1];

    return OGRERR_NONE;
}
#endif

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSDELayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL;

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

#ifdef notdef
OGRSpatialReference *OGRSDELayer::GetSpatialRef()

{
    return poSRS;
}
#endif
