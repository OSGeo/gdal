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
/*                              ParseXY()                               */
/************************************************************************/

static int ParseXY( const char **ppszNext, double *padfXY )

{
    int iStartY;
    const char *pszNext = *ppszNext;

    for( iStartY = 0; ; iStartY++ )
    {
        if( pszNext[iStartY] == '\0' )
            return FALSE;

        if( pszNext[iStartY] == ',' )
        {
            iStartY++;
            break;
        }
    }

    padfXY[0] = CPLAtof(pszNext);
    padfXY[1] = CPLAtof(pszNext + iStartY);

    int iEnd;

    for( iEnd = iStartY;
         pszNext[iEnd] != ')';
         iEnd++ )
    {
        if( pszNext[iEnd] == '\0' )
            return FALSE;
    }
    
    *ppszNext += iEnd;

    return TRUE;
}

/************************************************************************/
/*                         TranslateGeometry()                          */
/*                                                                      */
/*      This currently only supports "old style" ingres geometry in     */
/*      text format.  Essentially tuple lists of vertices.              */
/************************************************************************/

OGRGeometry *OGRIngresLayer::TranslateGeometry( const char *pszGeom )

{
    OGRGeometry *poGeom = NULL;

/* -------------------------------------------------------------------- */
/*      Parse the tuple list into an array of x/y vertices.  The        */
/*      input may look like "(2,3)" or "((2,3),(4,5),...)".  Extra      */
/*      spaces may occur between tokens.                                */
/* -------------------------------------------------------------------- */
    double *padfXY = NULL;
    int    nVertMax = 0, nVertCount = 0;
    int    nDepth = 0;
    const char *pszNext = pszGeom;

    while( *pszNext != '\0' )
    {
        while( *pszNext == ' ' )
            pszNext++;

        if( *pszNext == '(' )
        {
            pszNext++;
            nDepth++;
            continue;
        }

        if( *pszNext == ')' )
        {
            pszNext++;
            CPLAssert( nDepth == 1 );
            nDepth--;
            break;
        }

        if( *pszNext == ',' )
        {
            pszNext++;
            CPLAssert( nDepth == 1 );
            continue;
        }

        if( nVertCount == nVertMax )
        {
            nVertMax = nVertMax * 2 + 1;
            padfXY = (double *) 
                CPLRealloc(padfXY, sizeof(double) * nVertMax * 2 );
        }

        if( !ParseXY( &pszNext, padfXY + nVertCount*2 ) )
        {
            CPLDebug( "INGRES", "Error parsing geometry: %s", 
                      pszGeom );
            CPLFree( padfXY );
            return NULL;
        }
        
        CPLAssert( *pszNext == ')' );
        nVertCount++;
        pszNext++;
        nDepth--;

        while( *pszNext == ' ' )
            pszNext++;
    }

    CPLAssert( nDepth == 0 );

/* -------------------------------------------------------------------- */
/*      Handle Box/IBox.                                                */
/* -------------------------------------------------------------------- */
    if( EQUAL(osIngresGeomType,"BOX")
        || EQUAL(osIngresGeomType,"IBOX") )
    {
        CPLAssert( nVertCount == 2 );

        OGRLinearRing *poRing = new OGRLinearRing();
        poRing->addPoint( padfXY[0], padfXY[1] );
        poRing->addPoint( padfXY[2], padfXY[1] );
        poRing->addPoint( padfXY[2], padfXY[3] );
        poRing->addPoint( padfXY[0], padfXY[3] );
        poRing->addPoint( padfXY[0], padfXY[1] );

        OGRPolygon *poPolygon = new OGRPolygon();
        poPolygon->addRingDirectly( poRing );

        poGeom = poPolygon;
    }

/* -------------------------------------------------------------------- */
/*      Handle Point/IPoint                                             */
/* -------------------------------------------------------------------- */
    else if( EQUAL(osIngresGeomType,"POINT")
             || EQUAL(osIngresGeomType,"IPOINT") )
    {
        CPLAssert( nVertCount == 1 );

        poGeom = new OGRPoint( padfXY[0], padfXY[1] );
    }

/* -------------------------------------------------------------------- */
/*      Handle various linestring types.                                */
/* -------------------------------------------------------------------- */
    else if( EQUAL(osIngresGeomType,"LSEG")
             || EQUAL(osIngresGeomType,"ILSEG")
             || EQUAL(osIngresGeomType,"LINE")
             || EQUAL(osIngresGeomType,"LONG LINE")
             || EQUAL(osIngresGeomType,"ILINE") )
    {
        OGRLineString *poLine = new OGRLineString();
        int iVert;

        poLine->setNumPoints( nVertCount );
        for( iVert = 0; iVert < nVertCount; iVert++ )
            poLine->setPoint( iVert, padfXY[iVert*2+0], padfXY[iVert*2+1] );

        poGeom = poLine;
    }

/* -------------------------------------------------------------------- */
/*      Handle Polygon/IPolygon/LongPolygon.                            */
/* -------------------------------------------------------------------- */
    else if( EQUAL(osIngresGeomType,"POLYGON")
             || EQUAL(osIngresGeomType,"IPOLYGON")
             || EQUAL(osIngresGeomType,"LONG POLYGON") )
    {
        OGRLinearRing *poLine = new OGRLinearRing();
        int iVert;

        poLine->setNumPoints( nVertCount );
        for( iVert = 0; iVert < nVertCount; iVert++ )
            poLine->setPoint( iVert, padfXY[iVert*2+0], padfXY[iVert*2+1] );

        // INGRES polygons are implicitly closed, but OGR expects explicit
        if( poLine->getX(nVertCount-1) != poLine->getX(0)
            || poLine->getY(nVertCount-1) != poLine->getY(0) )
            poLine->addPoint( poLine->getX(0), poLine->getY(0) );

        OGRPolygon *poPolygon = new OGRPolygon();
        poPolygon->addRingDirectly( poLine );
        poGeom = poPolygon;
    }

    return poGeom;
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
        if( osFIDColumn.size() 
            && EQUAL(psFDesc->ds_columnName,osFIDColumn) 
            && psFDesc->ds_dataType == IIAPI_INT_TYPE 
            && psDV->dv_length == 4 )
        {
            if( papszRow[iField] == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "NULL primary key in RecordToFeature()" );
                return NULL;
            }

            GInt32 nValue;
            memcpy( &nValue, papszRow[iField], 4 );
            poFeature->SetFID( nValue );
        }

/* -------------------------------------------------------------------- */
/*      Handle Ingres geometry                                           */
/* -------------------------------------------------------------------- */
        if( osGeomColumn.size() 
            && EQUAL(psFDesc->ds_columnName,osGeomColumn))
        {
        	if( poDS->IsNewIngres() )
        	{
        		OGRGeometry *poGeometry = NULL;
        		unsigned char *pszWKB = (unsigned char *) papszRow[iField];

//        		OGRGeometryFactory::createFromWkt(&pszWKT, NULL, &poGeometry);
        		OGRGeometryFactory::createFromWkb(pszWKB, NULL, &poGeometry, -1);

        		poFeature->SetGeometryDirectly(poGeometry);
        	}
        	else
        	{
        		poFeature->SetGeometryDirectly(
        			TranslateGeometry( papszRow[iField] ) );
        	}
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
            if( psDV->dv_length == 8 )
            {
                GIntBig nValue;
                memcpy( &nValue, papszRow[iField], 8 );
                poFeature->SetField( iOGRField, (int) nValue );
            }
            else if( psDV->dv_length == 4 )
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
        CPLAssert( osQueryStatement.size() != 0 );

        poDS->EstablishActiveLayer( this );

        poResultSet = new OGRIngresStatement( poDS->GetConn() );

        if( !poResultSet->ExecuteSQL( osQueryStatement ) )
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
        return FALSE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

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
    return osFIDColumn;
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRIngresLayer::GetGeometryColumn() 

{
    return osGeomColumn;
}


/************************************************************************/
/*                         FetchSRSId()                                 */
/************************************************************************/

int OGRIngresLayer::FetchSRSId(OGRFeatureDefn *poDefn)
{
/* -------------------------------------------------------------------- */
/*      We only support srses in the new ingres geospatial implementation.*/
/* -------------------------------------------------------------------- */
    if( !poDS->IsNewIngres() )
    {
        nSRSId = -1;
    }

/* -------------------------------------------------------------------- */
/*      If we haven't queried for the srs id yet, do so now.            */
/* -------------------------------------------------------------------- */
    if( nSRSId == -2 )
    {
        char         szCommand[1024];
        char           **papszRow;
        OGRIngresStatement oStatement(poDS->GetConn());
        
        sprintf( szCommand, 
                 "SELECT srid FROM geometry_columns "
                 "WHERE f_table_name = '%s' AND f_geometry_column = '%s'",
                 poDefn->GetName(),
                 GetGeometryColumn());
        
        oStatement.ExecuteSQL(szCommand);
        
        papszRow = oStatement.GetRow();
        
        if( papszRow != NULL && papszRow[0] != NULL )
        {
            nSRSId = *((II_INT4 *) papszRow[0]);
        }
    }

    return nSRSId;
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
