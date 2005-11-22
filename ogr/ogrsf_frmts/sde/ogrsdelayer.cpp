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
 * Revision 1.2  2005/11/22 17:09:17  fwarmerdam
 * Preinitalize poFeature in GetNextFeature().
 *
 * Revision 1.1  2005/11/22 17:01:48  fwarmerdam
 * New
 *
 */

#include "ogr_sde.h"
#include "cpl_conv.h"

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

    if( poFeatureDefn )
        poFeatureDefn->Release();
//    if( poSRS )
//        poSRS->Release();
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
        poDS->IssueSDEError( nSDEErr, NULL );
        return FALSE;
    }

    poFeatureDefn = new OGRFeatureDefn( pszTableName );
    poFeatureDefn->Reference();

    for( iCol = 0; iCol < nColumnCount; iCol++ )
    {
        OGRFieldType eOGRType = OFTIntegerList; // dummy

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

        if( pszFIDColumn
            && EQUAL(asColumnDefs[iCol].column_name,pszFIDColumn) )
            iFIDColumn = iCol;

        OGRFieldDefn  oField( asColumnDefs[iCol].column_name, eOGRType );
        
        // TODO: add width and precision. 

        poFeatureDefn->AddFieldDefn( &oField );
        
        anFieldMap.push_back( iCol );
        anFieldTypeMap.push_back( asColumnDefs[iCol].sde_type );
    }

    SE_table_free_descriptions( asColumnDefs );

    return TRUE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSDELayer::ResetReading()

{
    int nSDEErr;

    // Reset stream.  If this is expensive, perhaps we should keep track 
    // of whether the stream is actually active or not. 
    nSDEErr = SE_stream_close( poDS->GetStream(), 1 );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSDELayer::GetNextFeature()

{
    OGRFeature  *poFeature = NULL;

/* -------------------------------------------------------------------- */
/*      Loop till we find a feature matching our criteria.              */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        // add feature reading here. 

        if( poFeature != NULL )
        {
            m_nFeaturesRead++;

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
