/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIngresLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_ingres.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRIngresLayer()                            */
/************************************************************************/

OGRIngresLayer::OGRIngresLayer()

{
    poDS = NULL;

    pszGeomColumn = NULL;
    pszGeomColumnTable = NULL;
    pszFIDColumn = NULL;
    pszQueryStatement = NULL;

    bHasFid = FALSE;
    pszFIDColumn = NULL;

    iNextShapeId = 0;
    nResultOffset = 0;

    poSRS = NULL;
    nSRSId = -2; // we haven't even queried the database for it yet. 

    poFeatureDefn = NULL;

    poResultSet = NULL;
}

/************************************************************************/
/*                           ~OGRIngresLayer()                           */
/************************************************************************/

OGRIngresLayer::~OGRIngresLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "Ingres", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    ResetReading();

    CPLFree( pszGeomColumn );
    CPLFree( pszGeomColumnTable );
    CPLFree( pszFIDColumn );
    CPLFree( pszQueryStatement );

    if( poSRS != NULL )
        poSRS->Release();

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIngresLayer::ResetReading()

{
    iNextShapeId = 0;

    if( poResultSet != NULL )
    {
        delete poResultSet;
        poResultSet = NULL;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRIngresLayer::GetNextFeature()

{
    for( ; TRUE; )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}
/************************************************************************/
/*                          RecordToFeature()                           */
/*                                                                      */
/*      Convert the indicated record of the current result set into     */
/*      a feature.                                                      */
/************************************************************************/

OGRFeature *OGRIngresLayer::RecordToFeature( char **papszRow )

{
/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    int         iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    poFeature->SetFID( iNextShapeId );
    m_nFeaturesRead++;

/* ==================================================================== */
/*      Transfer all result fields we can.                              */
/* ==================================================================== */
    for( iField = 0; 
         iField < (int) poResultSet->getDescrParm.gd_descriptorCount;
         iField++ )
    {
        IIAPI_DATAVALUE *psDV = 
            poResultSet->pasDataBuffer + iField;
        IIAPI_DESCRIPTOR *psFDesc = 
            poResultSet->getDescrParm.gd_descriptor + iField;
        int     iOGRField;

/* -------------------------------------------------------------------- */
/*      Ignore NULL fields.                                             */
/* -------------------------------------------------------------------- */
        if( psDV->dv_null ) 
            continue;

/* -------------------------------------------------------------------- */
/*      Handle FID.                                                     */
/* -------------------------------------------------------------------- */
        if( bHasFid && EQUAL(psFDesc->ds_columnName,pszFIDColumn) )
        {
            if( papszRow[iField] == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "NULL primary key in RecordToFeature()" );
                return NULL;
            }

            //poFeature->SetFID( atoi(papszRow[iField]) );
        }

/* -------------------------------------------------------------------- */
/*      Handle Ingres geometry                                           */
/* -------------------------------------------------------------------- */
        if( pszGeomColumn && EQUAL(psFDesc->ds_columnName,pszGeomColumn))
        {
#ifdef notdef            
            OGRGeometry *poGeometry = NULL;
            // Geometry columns will have the first 4 bytes contain the SRID.
            OGRGeometryFactory::createFromWkb(
                ((GByte *)papszRow[iField]) + 4, 
                NULL,
                &poGeometry,
                panLengths[iField] - 4 );

            if( poGeometry != NULL )
                poFeature->SetGeometryDirectly( poGeometry );
#endif
            continue;
        }


/* -------------------------------------------------------------------- */
/*      Transfer regular data fields.                                   */
/* -------------------------------------------------------------------- */
        iOGRField = poFeatureDefn->GetFieldIndex(psFDesc->ds_columnName);
        if( iOGRField < 0 )
            continue;

        switch( psFDesc->ds_dataType )
        {
          case IIAPI_CHR_TYPE:
          case IIAPI_CHA_TYPE:
          case IIAPI_LVCH_TYPE:
          case IIAPI_LTXT_TYPE:
            poFeature->SetField( iOGRField, papszRow[iField] );
            break;

          case IIAPI_VCH_TYPE:
          case IIAPI_TXT_TYPE:
            GUInt16 nLength;
            memcpy( &nLength, papszRow[iField], 2 );
            papszRow[iField][nLength+2] = '\0';
            poFeature->SetField( iOGRField, papszRow[iField]+2 );
            break;

          case IIAPI_INT_TYPE:
            if( psDV->dv_length == 4 )
            {
                GInt32 nValue;
                memcpy( &nValue, papszRow[iField], 4 );
                poFeature->SetField( iOGRField, nValue );
            }
            else if( psDV->dv_length == 2 )
            {
                GInt16 nValue;
                memcpy( &nValue, papszRow[iField], 2 );
                poFeature->SetField( iOGRField, nValue );
            }
            else if( psDV->dv_length == 1 )
            {
                GByte nValue;
                memcpy( &nValue, papszRow[iField], 1 );
                poFeature->SetField( iOGRField, nValue );
            }
            break;

          case IIAPI_FLT_TYPE:
            if( psDV->dv_length == 4 )
            {
                float fValue;
                memcpy( &fValue, papszRow[iField], 4 );
                poFeature->SetField( iOGRField, fValue );
            }
            else if( psDV->dv_length == 8 )
            {
                double dfValue;
                memcpy( &dfValue, papszRow[iField], 8 );
                poFeature->SetField( iOGRField, dfValue );
            }
            break;
            
          case IIAPI_DEC_TYPE:
          {
              IIAPI_CONVERTPARM sCParm;
              char szFormatBuf[30];

              memset( &sCParm, 0, sizeof(sCParm) );
              
              memcpy( &(sCParm.cv_srcDesc), psFDesc, 
                      sizeof(IIAPI_DESCRIPTOR) );
              memcpy( &(sCParm.cv_srcValue), psDV, 
                      sizeof(IIAPI_DATAVALUE) );

              sCParm.cv_dstDesc.ds_dataType = IIAPI_CHA_TYPE;
              sCParm.cv_dstDesc.ds_nullable = FALSE;
              sCParm.cv_dstDesc.ds_length = sizeof(szFormatBuf);

              sCParm.cv_dstValue.dv_value = szFormatBuf;

              IIapi_convertData( &sCParm );
              
              poFeature->SetField( iOGRField, szFormatBuf );
              break;
          }
        }
    }

    return poFeature;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRIngresLayer::GetNextRawFeature()

{
/* -------------------------------------------------------------------- */
/*      Do we need to establish an initial query?                       */
/* -------------------------------------------------------------------- */
    if( iNextShapeId == 0 && poResultSet == NULL )
    {
        CPLAssert( pszQueryStatement != NULL );

        poResultSet = new OGRIngresStatement( poDS->GetConn() );

        if( !poResultSet->ExecuteSQL( pszQueryStatement ) )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Fetch next record.                                              */
/* -------------------------------------------------------------------- */
    char **papszRow = poResultSet->GetRow();
    if( papszRow == NULL )
    {
        ResetReading();
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Process record.                                                 */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = RecordToFeature( papszRow );

    iNextShapeId++;

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/*                                                                      */
/*      Note that we actually override this in OGRIngresTableLayer.      */
/************************************************************************/

OGRFeature *OGRIngresLayer::GetFeature( long nFeatureId )

{
    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIngresLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else if( EQUAL(pszCap,OLCTransactions) )
        return FALSE;

	else if( EQUAL(pszCap,OLCFastGetExtent) )
		return FALSE;

    else
        return FALSE;
}



    
/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRIngresLayer::GetFIDColumn() 

{
    if( pszFIDColumn != NULL )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRIngresLayer::GetGeometryColumn() 

{
    if( pszGeomColumn != NULL )
        return pszGeomColumn;
    else
        return "";
}


/************************************************************************/
/*                         FetchSRSId()                                 */
/************************************************************************/

int OGRIngresLayer::FetchSRSId()
{
    return -1;
#ifdef notdef
    char         szCommand[1024];
    char           **papszRow;  
    
    if( hResultSet != NULL )
        ingres_free_result( hResultSet );
    hResultSet = NULL;
				
    sprintf( szCommand, 
             "SELECT srid FROM geometry_columns "
             "WHERE f_table_name = '%s'",
             pszGeomColumnTable );

    if( !ingres_query( poDS->GetConn(), szCommand ) )
        hResultSet = ingres_store_result( poDS->GetConn() );

    papszRow = NULL;
    if( hResultSet != NULL )
        papszRow = ingres_fetch_row( hResultSet );
        

    if( papszRow != NULL && papszRow[0] != NULL )
    {
        nSRSId = atoi(papszRow[0]);
    }

    // make sure to free our results
    if( hResultSet != NULL )
        ingres_free_result( hResultSet );
    hResultSet = NULL;
        
    return nSRSId;
#endif
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRIngresLayer::GetSpatialRef()

{
    if( poSRS == NULL && nSRSId > -1 )
    {
        poSRS = poDS->FetchSRS( nSRSId );
        if( poSRS != NULL )
            poSRS->Reference();
        else
            nSRSId = -1;
    }

    return poSRS;
}
