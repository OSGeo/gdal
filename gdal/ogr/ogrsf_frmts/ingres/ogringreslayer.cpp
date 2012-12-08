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
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRIngresLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( !InstallFilter( poGeomIn ) )
        return;

    BuildWhere();

    ResetReading();
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRIngresLayer::BuildWhere()

{
    osWHERE = "";

    /* -------------------------------------------------------------------- */
    /* 				Spatial Filter											*/
    /* -------------------------------------------------------------------- */
    /* Currently use *Intersects* funtion for filtering the geometry. For a */
    /* performace consideration, It is perferable to use *MBRIntersect*     */
    if( m_poFilterGeom != NULL && osGeomColumn.size())
    { 
        if (nSRSId > 0)
        {       
            osWHERE.Printf( "INTERSECTS(%s, GEOMETRYFROMWKB( ~V , %d)) = 1",
                osGeomColumn.c_str(),
                nSRSId);
        }
        else
        {        
            osWHERE.Printf( "INTERSECTS(%s, GEOMETRYFROMWKB( ~V , SRID(%s))) = 1",
                osGeomColumn.c_str(),
                osGeomColumn.c_str());
        }
    }

    /* -------------------------------------------------------------------- */
    /*              Attribute Filter										*/
    /* -------------------------------------------------------------------- */
    if( osQuery.size() > 0 )
    {
        if( osWHERE.size() == 0 )
            osWHERE = osQuery;
        else
            osWHERE += " AND " + osQuery;
    }
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRIngresLayer::SetAttributeFilter( const char *pszQuery )

{
    osQuery = "";

    if( pszQuery != NULL )
        osQuery = pszQuery;

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
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
/*                    BindQueryGeometry                                 */
/************************************************************************/

void  OGRIngresLayer::BindQueryGeometry(OGRIngresStatement *poStatement)
{
    if (poStatement == NULL)
    {
        return;
    }
    
    GByte * pabyWKB = NULL;
    int nSize = m_poFilterGeom->WkbSize();
    pabyWKB = (GByte *) CPLMalloc(nSize);

    m_poFilterGeom->exportToWkb(wkbNDR, pabyWKB);

    poStatement->addInputParameter( IIAPI_LBYTE_TYPE, nSize, pabyWKB );
    CPLFree(pabyWKB);
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

    padfXY[0] = atof(pszNext);
    padfXY[1] = atof(pszNext + iStartY);

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
/*                         FormatDTE()									*/
/* Convert datetime string generated by OpenAPI to the format that could*/
/* be parsed by OGR.                                                    */
/************************************************************************/

int FormatDTE(const char* pszSrc, char *pszTo)
{
    /* covert to dd-mmm-yyyy hh:mm:ss, to yyyy-mm-dd hh:mm:ss */
    if (strlen(pszSrc) < 11)
    {
        return 0;
    }

    /* dd */
    memcpy(pszTo+8, pszSrc, 2 );
    /* mm */
    if (EQUALN(pszSrc+3, "jan", 3))
    {
        memcpy(pszTo+5, "01-", 3 );
    }
    else if (EQUALN(pszSrc+3, "feb", 3))
    {
        memcpy(pszTo+5, "02-", 3 );
    }
    else if (EQUALN(pszSrc+3, "mar", 3))
    {
        memcpy(pszTo+5, "03-", 3 );
    }
    else if (EQUALN(pszSrc+3, "apr", 3))
    {
        memcpy(pszTo+5, "04-", 3 );
    }
    else if (EQUALN(pszSrc+3, "may", 3))
    {
        memcpy(pszTo+5, "05-", 3 );
    }
    else if (EQUALN(pszSrc+3, "jun", 3))
    {
        memcpy(pszTo+5, "06-", 3 );
    }
    else if (EQUALN(pszSrc+3, "jul", 3))
    {
        memcpy(pszTo+5, "07-", 3 );
    }
    else if (EQUALN(pszSrc+3, "aug", 3))
    {
        memcpy(pszTo+5, "08-", 3 );
    }
    else if (EQUALN(pszSrc+3, "sep", 3))
    {
        memcpy(pszTo+5, "09-", 3 );
    }
    else if (EQUALN(pszSrc+3, "oct", 3))
    {
        memcpy(pszTo+5, "10-", 3 );
    }
    else if (EQUALN(pszSrc+3, "nov", 3))
    {
        memcpy(pszTo+5, "11-", 3 );
    }
    else if (EQUALN(pszSrc+3, "dec", 3))
    {
        memcpy(pszTo+5, "12-", 3 );
    }
    else
    {
        memcpy(pszTo+5, "01-", 3 );
    }

    /* yyyy */
    memcpy(pszTo, pszSrc+7, 4);
    *(pszTo+4) = '-';

    *(pszTo+10) = '\0';

    /* time segment */
    if (strlen(pszSrc) >= 20 && *(pszSrc+13) != ' ')
    {
        /* hh:mm:ss */
        memcpy(pszTo+10, pszSrc+11, 9);	
        *(pszTo+19) = '\0';
    }

    return 1;
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

          case IIAPI_DTE_TYPE:
          case IIAPI_DATE_TYPE:
          case IIAPI_TIME_TYPE:
              // First convert to string type then to parse
              char szFormatBuf[30];
              char szDateTime[30];
              IIAPI_CONVERTPARM sCParm;              

              memset( &sCParm, 0, sizeof(sCParm) );

              memcpy( &(sCParm.cv_srcDesc), psFDesc, 
                  sizeof(IIAPI_DESCRIPTOR) );
              memcpy( &(sCParm.cv_srcValue), psDV, 
                  sizeof(IIAPI_DATAVALUE) );

              sCParm.cv_dstDesc.ds_dataType = IIAPI_CHA_TYPE;
              sCParm.cv_dstDesc.ds_nullable = FALSE;
              sCParm.cv_dstDesc.ds_length = sizeof(szFormatBuf);
              sCParm.cv_dstDesc.ds_precision = 0;
              sCParm.cv_dstDesc.ds_scale = 0;
              sCParm.cv_dstDesc.ds_columnType = IIAPI_COL_QPARM;

              sCParm.cv_dstValue.dv_value = szFormatBuf;

              IIapi_convertData( &sCParm );

              /* need to convert 'dd-mmm-yyyy' to  
              ** 'yyyy-mm-dd' which is known to ogr  
              */
              if (!FormatDTE(szFormatBuf, szDateTime))
              {
                  break;
              }

              OGRField psDTE;

              if (!OGRParseDate(szDateTime, &psDTE, 0))
              {
                  CPLError(CE_Failure, CPLE_AppDefined, "can't Parse data time : %s", szDateTime);
              }
              else
              {
                  poFeature->SetField( iOGRField, &psDTE);
              }
              break;

          case IIAPI_BYTE_TYPE:
          case IIAPI_VBYTE_TYPE:
          case IIAPI_LBYTE_TYPE:
              // need to copy?
              poFeature->SetField( iOGRField, papszRow[iField] );
              break;

          default:
              CPLError(CE_Failure, CPLE_AppDefined, "unrecognize column type %d", psFDesc->ds_dataType);
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

        poResultSet = new OGRIngresStatement( poDS->GetTransaction() );

        /* -------------------------------------------------------------------- */
        /*              Binding Filter Geometry 								*/
        /* -------------------------------------------------------------------- */
        if (m_poFilterGeom)
        {        
            BindQueryGeometry(poResultSet);
        }

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
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

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
        OGRIngresStatement oStatement(poDS->GetTransaction());
        
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

/************************************************************************/
/*                    StartTransaction()                                */
/************************************************************************/

OGRErr      OGRIngresLayer::StartTransaction()

{
    CPLAssert(poDS);
    return poDS->StartTransaction();
}

/************************************************************************/
/*                   CommitTransaction()                                */
/************************************************************************/

OGRErr      OGRIngresLayer::CommitTransaction()

{
    CPLAssert(poDS);
    return poDS->CommitTransaction();
}

/************************************************************************/
/*                    RollbackTransaction()                             */
/************************************************************************/

OGRErr      OGRIngresLayer::RollbackTransaction()

{
    CPLAssert(poDS);
    return poDS->RollbackTransaction();
}
