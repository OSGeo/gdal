/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCITableLayer class.  This class provides
 *           layer semantics on a table, but utilizing alot of machinery from
 *           the OGROCILayer base class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.8  2003/01/07 18:16:01  warmerda
 * implement spatial filtering in Oracle, re-enable index build
 *
 * Revision 1.7  2003/01/06 18:00:34  warmerda
 * Restructure geometry translation ... collections now supported.
 * Dimension is now a layer wide attribute.
 * Update dimension info in USER_SDO_GEOM_METADATA on close.
 *
 * Revision 1.6  2003/01/02 21:51:05  warmerda
 * fix quote escaping
 *
 * Revision 1.5  2002/12/29 19:43:59  warmerda
 * avoid some warnings
 *
 * Revision 1.4  2002/12/29 03:48:58  warmerda
 * fixed memory bug in CreateFeature()
 *
 * Revision 1.3  2002/12/28 20:06:31  warmerda
 * minor CreateFeature improvements
 *
 * Revision 1.2  2002/12/28 04:38:36  warmerda
 * converted to unix file conventions
 *
 * Revision 1.1  2002/12/28 04:07:27  warmerda
 * New
 *
 */

#include "ogr_oci.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static int nDiscarded = 0;
static int nHits = 0;

/************************************************************************/
/*                          OGROCITableLayer()                          */
/************************************************************************/

OGROCITableLayer::OGROCITableLayer( OGROCIDataSource *poDSIn, 
                                    const char * pszTableName,
                                    int bUpdate, int bNewLayerIn )

{
    poDS = poDSIn;

    pszQuery = NULL;
    pszWHERE = CPLStrdup( "" );
    pszQueryStatement = NULL;
    
    bUpdateAccess = bUpdate;
    bNewLayer = bNewLayerIn;
    bLaunderColumnNames = TRUE;

    iNextShapeId = 0;
    iNextFIDToWrite = 1;
    nDimension = 3;

    poFeatureDefn = ReadTableDefinition( pszTableName );
    
    ResetReading();

}

/************************************************************************/
/*                         ~OGROCITableLayer()                          */
/************************************************************************/

OGROCITableLayer::~OGROCITableLayer()

{
    if( bNewLayer )
        FinalizeNewLayer();

    CPLFree( pszQuery );
    CPLFree( pszWHERE );
}

/************************************************************************/
/*                        ReadTableDefinition()                         */
/*                                                                      */
/*      Build a schema from the named table.  Done by querying the      */
/*      catalog.                                                        */
/************************************************************************/

OGRFeatureDefn *OGROCITableLayer::ReadTableDefinition( const char * pszTable )

{
    OGROCISession      *poSession = poDS->GetSession();
    sword               nStatus;
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( pszTable );

    poDefn->Reference();

/* -------------------------------------------------------------------- */
/*      Do a DescribeAll on the table.                                  */
/* -------------------------------------------------------------------- */
    OCIParam *hAttrParam = NULL;
    OCIParam *hAttrList = NULL;

    nStatus = 
        OCIDescribeAny( poSession->hSvcCtx, poSession->hError, 
                        (dvoid *) pszTable, strlen(pszTable), OCI_OTYPE_NAME, 
                        OCI_DEFAULT, OCI_PTYPE_TABLE, poSession->hDescribe );
    if( poSession->Failed( nStatus, "OCIDescribeAny" ) )
        return poDefn;

    if( poSession->Failed( 
        OCIAttrGet( poSession->hDescribe, OCI_HTYPE_DESCRIBE, 
                    &hAttrParam, 0, OCI_ATTR_PARAM, poSession->hError ),
        "OCIAttrGet(ATTR_PARAM)") )
        return poDefn;

    if( poSession->Failed( 
        OCIAttrGet( hAttrParam, OCI_DTYPE_PARAM, &hAttrList, 0, 
                    OCI_ATTR_LIST_COLUMNS, poSession->hError ),
        "OCIAttrGet(ATTR_LIST_COLUMNS)" ) )
        return poDefn;

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    for( int iRawFld = 0; TRUE; iRawFld++ )
    {
        OGRFieldDefn    oField( "", OFTString);
        OCIParam     *hParmDesc;
        ub2          nOCIType;
        ub4          nOCILen;
        sword        nStatus;

        nStatus = OCIParamGet( hAttrList, OCI_DTYPE_PARAM,
                               poSession->hError, (dvoid**)&hParmDesc, 
                               (ub4) iRawFld+1 );
        if( nStatus != OCI_SUCCESS )
            break;

        if( poSession->GetParmInfo( hParmDesc, &oField, &nOCIType, &nOCILen )
            != CE_None )
            return poDefn;

        if( oField.GetType() == OFTBinary 
            && nOCIType == 108 )
        {
            CPLFree( pszGeomName );
            pszGeomName = CPLStrdup( oField.GetNameRef() );
            continue;
        }

        if( oField.GetType() == OFTBinary )
            continue;			

        if( EQUAL(oField.GetNameRef(),"OGR_FID") 
            && oField.GetType() == OFTInteger )
        {
            pszFIDName = CPLStrdup(oField.GetNameRef());
            continue;
        }

        poDefn->AddFieldDefn( &oField );
    }

    return poDefn;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGROCITableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( poFilterGeom != NULL )
    {
        delete poFilterGeom;
        poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
        poFilterGeom = poGeomIn->clone();

    BuildWhere();

    ResetReading();
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGROCITableLayer::BuildWhere()

{
    OGROCIStringBuf oWHERE;

    CPLFree( pszWHERE );
    pszWHERE = NULL;

    if( poFilterGeom != NULL )
    {
        OGREnvelope  sEnvelope;

        poFilterGeom->getEnvelope( &sEnvelope );

        oWHERE.Append( "WHERE sdo_filter(" );
        oWHERE.Append( pszGeomName );
        oWHERE.Append( ", MDSYS.SDO_GEOMETRY(2003,NULL,NULL," );
        oWHERE.Append( "MDSYS.SDO_ELEM_INFO_ARRAY(1,1003,3)," );
        oWHERE.Append( "MDSYS.SDO_ORDINATE_ARRAY(" );
        oWHERE.Appendf( 200, "%.16g,%.16g,%.16g,%.16g", 
                        sEnvelope.MinX, sEnvelope.MinY,
                        sEnvelope.MaxX, sEnvelope.MaxY );
        oWHERE.Append( ")), 'querytype=window') = 'TRUE' " );
    }

    if( pszQuery != NULL )
    {
        if( oWHERE.GetLast() == '\0' )
            oWHERE.Append( "WHERE " );
        else
            oWHERE.Append( "AND " );

        oWHERE.Append( pszQuery );
    }

    pszWHERE = oWHERE.StealString();
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGROCITableLayer::BuildFullQueryStatement()

{
    if( pszQueryStatement != NULL )
    {
        CPLFree( pszQueryStatement );
        pszQueryStatement = NULL;
    }

    OGROCIStringBuf oCmd;
    char *pszFields = BuildFields();

    oCmd.Append( "SELECT " );
    oCmd.Append( pszFields );
    oCmd.Append( " FROM \"" );
    oCmd.Append( poFeatureDefn->GetName() );
    oCmd.Append( "\" " );
    oCmd.Append( pszWHERE );

    pszQueryStatement = oCmd.StealString();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/*                                                                      */
/*      We override the next feature method because we know that we     */
/*      implement the attribute query within the statement and so we    */
/*      don't have to test here.   Eventually the spatial query will    */
/*      be fully tested within the statement as well.                   */
/************************************************************************/

OGRFeature *OGROCITableLayer::GetNextFeature()

{

    for( ; TRUE; )
    {
        OGRFeature	*poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
        {
            CPLDebug( "OCI", "Query complete, got %d hits, and %d discards.",
                      nHits, nDiscarded );
            nHits = 0;
            nDiscarded = 0;
            return NULL;
        }

        if( poFilterGeom == NULL
            || poFilterGeom->Intersect( poFeature->GetGeometryRef() ) )
        {
            nHits++;
            return poFeature;
        }

        if( poFilterGeom != NULL )
            nDiscarded++;

        delete poFeature;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROCITableLayer::ResetReading()

{
    nHits = 0;
    nDiscarded = 0;

    BuildFullQueryStatement();

    OGROCILayer::ResetReading();
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build list of fields to fetch, performing any required          */
/*      transformations (such as on geometry).                          */
/************************************************************************/

char *OGROCITableLayer::BuildFields()

{
    int		i;
    OGROCIStringBuf oFldList;

    if( pszGeomName )							
    {
        oFldList.Append( "\"" );
        oFldList.Append( pszGeomName );
        oFldList.Append( "\"" );
        iGeomColumn = 0;
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( oFldList.GetLast() != '\0' )
            oFldList.Append( "," );

        oFldList.Append( "\"" );
        oFldList.Append( pszName );
        oFldList.Append( "\"" );
    }

    if( pszFIDName != NULL )
    {
        iFIDColumn = poFeatureDefn->GetFieldCount();
        oFldList.Append( ",\"" );
        oFldList.Append( pszFIDName );
        oFldList.Append( "\"" );
    }

    return oFldList.StealString();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGROCITableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree( this->pszQuery );
    this->pszQuery = CPLStrdup( pszQuery );

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGROCITableLayer::SetFeature( OGRFeature *poFeature )

{
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                       TranslateElementGroup()                        */
/*                                                                      */
/*      Append one or more element groups to the existing element       */
/*      info and ordinates lists for the passed geometry.               */
/************************************************************************/

OGRErr 
OGROCITableLayer::TranslateElementGroup( OGRGeometry *poGeometry, 
                                         OGROCIStringBuf *poElemInfo,
                                         int &nLastOrdinate, 
                                         OGROCIStringBuf *poOrdinates )

{
    
    switch( wkbFlatten(poGeometry->getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = (OGRPoint *) poGeometry;

          if( poElemInfo->GetLast() != '(' )
              poElemInfo->Append( "," );
          
          poElemInfo->Appendf( 32, "%d,1,1", nLastOrdinate+1 );

          if( nLastOrdinate != 0 )
              poOrdinates->Append( "," );
          
          poOrdinates->Appendf( 64, "%.16g,%.16g", 
                                poPoint->getX(), poPoint->getY() );
          
          if( nDimension == 3 )
              poOrdinates->Appendf( 32, ",%.16g", poPoint->getZ() );

          nLastOrdinate += nDimension;

          return OGRERR_NONE;
      }

      case wkbLineString:
      {
          OGRLineString *poLine = (OGRLineString *) poGeometry;
          int  iVert;
          
          if( poElemInfo != NULL )
          {
              if( poElemInfo->GetLast() != '(' )
                  poElemInfo->Append( "," );

              poElemInfo->Appendf( 32, "%d,2,1", nLastOrdinate+1 );
          }

          for( iVert = 0; iVert < poLine->getNumPoints(); iVert++ )
          {
              if( nLastOrdinate != 0 )
                  poOrdinates->Append( "," );

              poOrdinates->Appendf( 64, "%.16g,%.16g", 
                                    poLine->getX(iVert), poLine->getY(iVert) );
          
              if( nDimension == 3 )
                  poOrdinates->Appendf( 32, ",%.16g", poLine->getZ(iVert) );

              nLastOrdinate += nDimension;
          }
          return OGRERR_NONE;
      }

      case wkbPolygon:
      {
          OGRPolygon *poPoly = (OGRPolygon *) poGeometry;
          int iRing;
          OGRErr eErr;


          for( iRing = -1; iRing < poPoly->getNumInteriorRings(); iRing++ )
          {
              OGRLinearRing *poRing;

              if( iRing == -1 )
                  poRing = poPoly->getExteriorRing();
              else
                  poRing = poPoly->getInteriorRing(iRing);

              // take care of eleminfo here.
              if( poElemInfo->GetLast() != '(' )
                  poElemInfo->Append( "," );

              if( iRing == -1 )
                  poElemInfo->Appendf( 20, "%d,1003,1", nLastOrdinate+1 );
              else
                  poElemInfo->Appendf( 20, "%d,2003,1", nLastOrdinate+1 );

              // but recurse to add the ordinates.
              eErr = TranslateElementGroup( poRing, NULL, 
                                            nLastOrdinate, poOrdinates );
              if( eErr != OGRERR_NONE )
                  return eErr;
          }

          return OGRERR_NONE;
      }

      default:
      {
          return OGRERR_FAILURE;
      }
    }
}

/************************************************************************/
/*                       TranslateToSDOGeometry()                       */
/************************************************************************/

char *OGROCITableLayer::TranslateToSDOGeometry( OGRGeometry * poGeometry )

{
    if( poGeometry == NULL )
        return CPLStrdup("NULL");

/* ==================================================================== */
/*      Handle a point geometry.                                        */
/* ==================================================================== */
    if( wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
    {
        char szResult[1024];
        OGRPoint *poPoint = (OGRPoint *) poGeometry;

        if( poGeometry->getDimension() == 2 )
            sprintf( szResult, 
                     "%s(%d,NULL,MDSYS.SDO_POINT_TYPE(%.16g,%.16g),NULL,NULL)",
                     SDO_GEOMETRY, 2001, poPoint->getX(), poPoint->getY() );
        else
            sprintf( szResult, 
                     "%s(%d,NULL,MDSYS.SDO_POINT_TYPE(%.16g,%.16g,%.16g),NULL,NULL)",
                     SDO_GEOMETRY, 2001, 
                     poPoint->getX(), poPoint->getY(), poPoint->getZ() );

        return CPLStrdup(szResult );
    }

/* ==================================================================== */
/*      Handle a line string geometry.                                  */
/* ==================================================================== */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbLineString )
    {
        int nLastOrdinate = 0;
        OGROCIStringBuf oElemInfo, oOrdinates;

        oElemInfo.Appendf( 100, "%s(%d,NULL,NULL,MDSYS.SDO_ELEM_INFO_ARRAY(",
                           SDO_GEOMETRY, nDimension * 1000 + 2 );
        oOrdinates.Append( "),MDSYS.SDO_ORDINATE_ARRAY(" );

        TranslateElementGroup( poGeometry, &oElemInfo, 
                               nLastOrdinate, &oOrdinates );

        oElemInfo.Append( oOrdinates.GetString() );
        oElemInfo.Append( "))" );

        return oElemInfo.StealString();
    }

/* ==================================================================== */
/*      Handle a polygon geometry.                                      */
/* ==================================================================== */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbPolygon )
    {
        int nLastOrdinate = 0;
        OGROCIStringBuf oElemInfo, oOrdinates;

/* -------------------------------------------------------------------- */
/*      Prepare eleminfo section.                                       */
/* -------------------------------------------------------------------- */
        oElemInfo.Appendf( 90, "%s(%d,NULL,NULL,MDSYS.SDO_ELEM_INFO_ARRAY(",
                           SDO_GEOMETRY, nDimension == 2 ? 2003 : 3003 );
        oOrdinates.Append( "),MDSYS.SDO_ORDINATE_ARRAY(" );

/* -------------------------------------------------------------------- */
/*      Translate and return.                                           */
/* -------------------------------------------------------------------- */
        TranslateElementGroup( poGeometry, &oElemInfo, 
                               nLastOrdinate, &oOrdinates );

        oElemInfo.Append( oOrdinates.GetString() );
        oElemInfo.Append( "))" );

        return oElemInfo.StealString();
    }

/* ==================================================================== */
/*      Handle a multi point geometry.                                  */
/* ==================================================================== */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint )
    {
        OGRMultiPoint *poMP = (OGRMultiPoint *) poGeometry;
        char *pszResult;
        int  iOff, iVert;

        pszResult = (char *) CPLMalloc(poMP->getNumGeometries() * 51 + 500);

        sprintf( pszResult, 
                 "%s(%d,NULL,NULL,MDSYS.SDO_ELEM_INFO_ARRAY(1,1,%d),"
                 "MDSYS.SDO_ORDINATE_ARRAY(", 
                 SDO_GEOMETRY, nDimension*1000 + 5, poMP->getNumGeometries() );
        
        iOff = strlen(pszResult);
        for( iVert = 0; iVert < poMP->getNumGeometries(); iVert++ )
        {
            OGRPoint *poPoint = (OGRPoint *)poMP->getGeometryRef( iVert );

            if( iVert != 0 )
                pszResult[iOff++] = ',';

            sprintf( pszResult + iOff, "%.16g,%.16g", 
                     poPoint->getX(), poPoint->getY() );
            iOff += strlen(pszResult+iOff);
            if( nDimension == 3 )
            {
                sprintf( pszResult + iOff, ",%.16g", poPoint->getZ() );
                iOff += strlen(pszResult+iOff);
            }
        }
        strcat( pszResult + iOff, "))" );
        return pszResult;
    }

/* ==================================================================== */
/*      Handle other geometry collections.                              */
/* ==================================================================== */
    else
    {
        int nLastOrdinate = 0;
        OGROCIStringBuf oElemInfo, oOrdinates;
        int nGType;

/* -------------------------------------------------------------------- */
/*      Identify the GType.                                             */
/* -------------------------------------------------------------------- */
        if( poGeometry->getGeometryType() == wkbMultiLineString )
            nGType = nDimension * 1000 + 6;
        else if( poGeometry->getGeometryType() == wkbMultiPolygon )
            nGType = nDimension * 1000 + 7;
        else if( poGeometry->getGeometryType() == wkbGeometryCollection )
            nGType = nDimension * 1000 + 4;
        else 
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unexpected geometry type (%d/%s) in "
                      "OGROCITableLayer::TranslateToSDOGeometry()",
                      poGeometry->getGeometryType(), 
                      poGeometry->getGeometryName() );
            return NULL;
        }

/* -------------------------------------------------------------------- */
/*      Prepare eleminfo section.                                       */
/* -------------------------------------------------------------------- */
        oElemInfo.Appendf( 90, "%s(%d,NULL,NULL,MDSYS.SDO_ELEM_INFO_ARRAY(",
                           SDO_GEOMETRY, nGType );
        oOrdinates.Append( "),MDSYS.SDO_ORDINATE_ARRAY(" );

/* -------------------------------------------------------------------- */
/*      Translate each child in turn.                                   */
/* -------------------------------------------------------------------- */
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeometry;
        int  iChild;

        for( iChild = 0; iChild < poGC->getNumGeometries(); iChild++ )
        {
            TranslateElementGroup( poGC->getGeometryRef( iChild ), &oElemInfo, 
                                   nLastOrdinate, &oOrdinates );
        }

/* -------------------------------------------------------------------- */
/*      Collect and return.                                             */
/* -------------------------------------------------------------------- */
        oElemInfo.Append( oOrdinates.GetString() );
        oElemInfo.Append( "))" );

        return oElemInfo.StealString();
    }
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGROCITableLayer::CreateFeature( OGRFeature *poFeature )

{
    OGROCISession      *poSession = poDS->GetSession();
    char		*pszCommand;
    int                 i, bNeedComma = FALSE;
    unsigned int        nCommandBufSize;;

/* -------------------------------------------------------------------- */
/*      Add extents of this geometry to the existing layer extents.     */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() != NULL )
    {
        OGREnvelope  sThisExtent;
        
        poFeature->GetGeometryRef()->getEnvelope( &sThisExtent );
        sExtent.Merge( sThisExtent );
    }

/* -------------------------------------------------------------------- */
/*      Prepare SQL statement buffer.                                   */
/* -------------------------------------------------------------------- */
    nCommandBufSize = 2000;
    pszCommand = (char *) CPLMalloc(nCommandBufSize);

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.  					*/
/* -------------------------------------------------------------------- */
    sprintf( pszCommand, "INSERT INTO \"%s\" (", poFeatureDefn->GetName() );

    if( poFeature->GetGeometryRef() != NULL )
    {
        bNeedComma = TRUE;
        strcat( pszCommand, pszGeomName );
    }
    
    if( pszFIDName != NULL )
    {
        if( bNeedComma )
            strcat( pszCommand, ", " );
        
        strcat( pszCommand, pszFIDName );
        bNeedComma = TRUE;
    }
    

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            strcat( pszCommand, ", " );

        sprintf( pszCommand + strlen(pszCommand), "\"%s\"",
                 poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
    }

    strcat( pszCommand, ") VALUES (" );

    CPLAssert( strlen(pszCommand) < nCommandBufSize );

/* -------------------------------------------------------------------- */
/*      Set the geometry                                                */
/* -------------------------------------------------------------------- */
    bNeedComma = poFeature->GetGeometryRef() != NULL;
    if( poFeature->GetGeometryRef() != NULL)
    {
        char	*pszSDO_GEOMETRY 
            = TranslateToSDOGeometry( poFeature->GetGeometryRef() );

#ifdef notdef
        // Do we need to force nSRSId to be set?
        if( nSRSId == -2 )
            GetSpatialRef();
#endif
        if( pszSDO_GEOMETRY != NULL 
            && strlen(pszCommand) + strlen(pszSDO_GEOMETRY) 
            > nCommandBufSize - 50 )
        {
            nCommandBufSize = 
                strlen(pszCommand) + strlen(pszSDO_GEOMETRY) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }

        if( pszSDO_GEOMETRY != NULL )
        {
            strcat( pszCommand, pszSDO_GEOMETRY );
            CPLFree( pszSDO_GEOMETRY );
        }
        else
            strcat( pszCommand, "NULL" );
    }

/* -------------------------------------------------------------------- */
/*      Set the FID.                                                    */
/* -------------------------------------------------------------------- */
    int nOffset = strlen(pszCommand);

    if( pszFIDName != NULL )
    {
        long  nFID;

        if( bNeedComma )
            strcat( pszCommand+nOffset, ", " );
        bNeedComma = TRUE;

        nOffset += strlen(pszCommand+nOffset);

        nFID = poFeature->GetFID();
        if( nFID == -1 )
            nFID = iNextFIDToWrite++;

        sprintf( pszCommand+nOffset, "%ld", nFID );
    }

/* -------------------------------------------------------------------- */
/*      Set the other fields.                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszStrValue = poFeature->GetFieldAsString(i);

        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            strcat( pszCommand+nOffset, ", " );
        else
            bNeedComma = TRUE;

        if( strlen(pszStrValue) + strlen(pszCommand+nOffset) + nOffset 
            > nCommandBufSize-50 )
        {
            nCommandBufSize = strlen(pszCommand) + strlen(pszStrValue) + 10000;
            pszCommand = (char *) CPLRealloc(pszCommand, nCommandBufSize );
        }
        
        if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger
            && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTReal )
        {
            int		iChar;

            /* We need to quote and escape string fields. */
            strcat( pszCommand+nOffset, "'" );

            nOffset += strlen(pszCommand+nOffset);
            
            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( pszStrValue[iChar] == '\'' )
                {
                    pszCommand[nOffset++] = '\'';
                    pszCommand[nOffset++] = pszStrValue[iChar];
                }
                else
                    pszCommand[nOffset++] = pszStrValue[iChar];
            }
            pszCommand[nOffset] = '\0';
            
            strcat( pszCommand+nOffset, "'" );
        }
        else
        {
            strcat( pszCommand+nOffset, pszStrValue );
        }
        nOffset += strlen(pszCommand+nOffset);
    }

    strcat( pszCommand+nOffset, ")" );

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    OGROCIStatement oInsert( poSession );

    if( oInsert.Execute( pszCommand ) != CE_None )
    {
        CPLFree( pszCommand );
        return OGRERR_FAILURE;
    }
    else
    {
        CPLFree( pszCommand );
        return OGRERR_NONE;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROCITableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else 
        return OGROCILayer::TestCapability( pszCap );
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGROCITableLayer::CreateField( OGRFieldDefn *poFieldIn, int bApproxOK )

{
    OGROCISession      *poSession = poDS->GetSession();
    char		szFieldType[256];
    OGRFieldDefn        oField( poFieldIn );

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char	*pszSafeName = CPLStrdup( oField.GetNameRef() );
        int	i;

        for( i = 0; pszSafeName[i] != '\0'; i++ )
        {
            pszSafeName[i] = tolower( pszSafeName[i] );
            if( pszSafeName[i] == '-' || pszSafeName[i] == '#' )
                pszSafeName[i] = '_';
        }
        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );
    }
    
/* -------------------------------------------------------------------- */
/*      Work out the PostgreSQL type.                                   */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
    {
        strcpy( szFieldType, "INTEGER" );
    }
    else if( oField.GetType() == OFTReal )
    {
        strcpy( szFieldType, "NUMBER" );
    }
    else if( oField.GetType() == OFTString )
    {
        if( oField.GetWidth() == 0 || !bPreservePrecision )
            strcpy( szFieldType, "VARCHAR(1024)" );
        else
            sprintf( szFieldType, "CHAR(%d)", oField.GetWidth() );
    }
    else if( bApproxOK )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Can't create field %s with type %s on Oracle layers.  Creating as VARCHAR.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "VARCHAR" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on Oracle layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    OGROCIStringBuf     oCommand;
    OGROCIStatement     oAddField( poSession );

    oCommand.MakeRoomFor( 40 + strlen(poFeatureDefn->GetName())
                          + strlen(oField.GetNameRef())
                          + strlen(szFieldType) );

    sprintf( oCommand.GetString(), "ALTER TABLE \"%s\" ADD \"%s\" %s", 
             poFeatureDefn->GetName(), oField.GetNameRef(), szFieldType );
    if( oAddField.Execute( oCommand.GetString() ) != CE_None )
        return OGRERR_FAILURE;

    poFeatureDefn->AddFieldDefn( &oField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGROCITableLayer::GetFeatureCount( int bForce )

{
/* -------------------------------------------------------------------- */
/*      Use a more brute force mechanism if we have a spatial query     */
/*      in play.                                                        */
/* -------------------------------------------------------------------- */
    if( poFilterGeom != NULL )
        return OGROCILayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      In theory it might be wise to cache this result, but it         */
/*      won't be trivial to work out the lifetime of the value.         */
/*      After all someone else could be adding records from another     */
/*      application when working against a database.                    */
/* -------------------------------------------------------------------- */
    OGROCISession      *poSession = poDS->GetSession();
    OGROCIStatement    oGetCount( poSession );
    char               szCommand[1024];
    char               **papszResult;

    sprintf( szCommand, "SELECT COUNT(*) FROM \"%s\" %s", 
             poFeatureDefn->GetName(), pszWHERE );

    oGetCount.Execute( szCommand );

    papszResult = oGetCount.SimpleFetchRow();

    if( CSLCount(papszResult) < 1 )
    {
        CPLDebug( "OCI", "Fast get count failed, doing hard way." );
        return OGROCILayer::GetFeatureCount( bForce );
    }
    
    return atoi(papszResult[0]);
}

/************************************************************************/
/*                            SetDimension()                            */
/************************************************************************/

void OGROCITableLayer::SetDimension( int nNewDim )

{
    nDimension = nNewDim;
}

/************************************************************************/
/*                          FinalizeNewLayer()                          */
/*                                                                      */
/*      Our main job here is to update the USER_SDO_GEOM_METADATA       */
/*      table to include the correct array of dimension object with     */
/*      the appropriate extents for this layer.  We may also do         */
/*      spatial indexing at this point.                                 */
/************************************************************************/

void OGROCITableLayer::FinalizeNewLayer()

{
    OGROCIStringBuf  sDimUpdate;
    double           dfResSize;

/* -------------------------------------------------------------------- */
/*      Prepare dimension update statement.                             */
/* -------------------------------------------------------------------- */
    sDimUpdate.Append( "UPDATE USER_SDO_GEOM_METADATA SET DIMINFO = " );
    sDimUpdate.Append( "MDSYS.SDO_DIM_ARRAY(" );

    if( sExtent.MaxX - sExtent.MinX > 400 )
        dfResSize = 0.001;
    else
        dfResSize = 0.0000001;

    sDimUpdate.Appendf(200,
                       "MDSYS.SDO_DIM_ELEMENT('X',%.16g,%.16g,%.12g)",
                       sExtent.MinX - dfResSize * 3,
                       sExtent.MaxX + dfResSize * 3,
                       dfResSize );
    sDimUpdate.Appendf(200,
                       ",MDSYS.SDO_DIM_ELEMENT('Y',%.16g,%.16g,%.12g)",
                       sExtent.MinY - dfResSize * 3,
                       sExtent.MaxY + dfResSize * 3,
                       dfResSize );

    // We choose this pretty arbitrarily under the assumption the Z 
    // is elevation in some reasonable range.  It is somewhat hard to
    // collect Z range information.
    if( nDimension == 3 )
    {
        sDimUpdate.Append(",MDSYS.SDO_DIM_ELEMENT('Z',"
                          "-100000.0,100000.0,0.002)");
    }

    sDimUpdate.Append( ")" );

    sDimUpdate.Appendf( strlen(poFeatureDefn->GetName()) + 100,
                        " WHERE table_name = '%s'", 
                        poFeatureDefn->GetName() );

/* -------------------------------------------------------------------- */
/*      Execute the metadata update.                                    */
/* -------------------------------------------------------------------- */
    OGROCIStatement oExecStatement( poDS->GetSession() );

    if( oExecStatement.Execute( sDimUpdate.GetString() ) != CE_None )
        return;

/* -------------------------------------------------------------------- */
/*      Try creating an index on the table now.  Use a simple 5         */
/*      level quadtree based index.  Would R-tree be a better default?  */
/* -------------------------------------------------------------------- */

// Disable for now, spatial index creation always seems to cause me to 
// lose my connection to the database!
    OGROCIStringBuf  sIndexCmd;

    sIndexCmd.Appendf( 10000, "CREATE INDEX %s_idx ON %s(%s) "
                       "INDEXTYPE IS MDSYS.SPATIAL_INDEX "
                       "PARAMETERS( 'SDO_LEVEL = 5' )",
                       poFeatureDefn->GetName(), 
                       poFeatureDefn->GetName(), 
                       pszGeomName );

    if( oExecStatement.Execute( sIndexCmd.GetString() ) != CE_None )
    {
        char szDropCommand[2000];
        sprintf( szDropCommand, "DROP INDEX %s_idx", poFeatureDefn->GetName());
        oExecStatement.Execute( szDropCommand );
    }
}
