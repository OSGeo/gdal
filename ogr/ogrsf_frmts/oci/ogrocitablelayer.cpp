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
 * Revision 1.14  2003/01/14 15:11:00  warmerda
 * Added layer creation options caching on layer.
 * Set SRID in spatial query.
 * Support user override of DIMINFO bounds in FinalizeNewLayer().
 * Support user override of indexing options, or disabling of indexing.
 *
 * Revision 1.13  2003/01/13 13:50:13  warmerda
 * dont quote table names, it doesnt seem to work with userid.tablename
 *
 * Revision 1.12  2003/01/10 22:31:53  warmerda
 * no longer encode ordinates into SQL statement when creating features
 *
 * Revision 1.11  2003/01/09 21:19:12  warmerda
 * improved data type support, get/set precision
 *
 * Revision 1.10  2003/01/07 22:24:35  warmerda
 * added SRS support
 *
 * Revision 1.9  2003/01/07 21:14:20  warmerda
 * implement GetFeature() and SetFeature()
 *
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
                                    const char * pszGeomColIn,
                                    int nSRIDIn, int bUpdate, int bNewLayerIn )

{
    poDS = poDSIn;

    pszQuery = NULL;
    pszWHERE = CPLStrdup( "" );
    pszQueryStatement = NULL;
    papszOptions = NULL;
    
    bUpdateAccess = bUpdate;
    bNewLayer = bNewLayerIn;
    bLaunderColumnNames = TRUE;

    iNextShapeId = 0;
    iNextFIDToWrite = 1;
    nDimension = 3;

    bValidTable = FALSE;

    poFeatureDefn = ReadTableDefinition( pszTableName );
    
    CPLFree( pszGeomName );
    pszGeomName = CPLStrdup( pszGeomColIn );

    nSRID = nSRIDIn;
    poSRS = poDSIn->FetchSRS( nSRID );
    if( poSRS != NULL )
        poSRS->Reference();

    nOrdinalCount = 0;
    nOrdinalMax = 0;
    padfOrdinals = NULL;

    hOrdVARRAY = NULL;

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
    CSLDestroy( papszOptions );

    if( poSRS != NULL && poSRS->Dereference() == 0 )
        delete poSRS;

    CPLFree( padfOrdinals );
}

/************************************************************************/
/*                             SetOptions()                             */
/*                                                                      */
/*      Set layer creation or other options.                            */
/************************************************************************/

void OGROCITableLayer::SetOptions( char **papszOptionsIn )

{
    CSLDestroy( papszOptions );
    papszOptions = CSLDuplicate( papszOptionsIn );
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

    bValidTable = TRUE;

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
        oWHERE.Append( ", MDSYS.SDO_GEOMETRY(2003," );
        if( nSRID == -1 )
            oWHERE.Append( "NULL" );
        else
            oWHERE.Appendf( 15, "%d", nSRID );
        oWHERE.Append( ",NULL," );
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
    oCmd.Append( " FROM " );
    oCmd.Append( poFeatureDefn->GetName() );
    oCmd.Append( " " );
    oCmd.Append( pszWHERE );

    pszQueryStatement = oCmd.StealString();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGROCITableLayer::GetFeature( long nFeatureId )

{

/* -------------------------------------------------------------------- */
/*      If we don't have an FID column scan for the desired feature.    */
/* -------------------------------------------------------------------- */
    if( pszFIDName == NULL )
        return OGROCILayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Clear any existing query.                                       */
/* -------------------------------------------------------------------- */
    ResetReading();

/* -------------------------------------------------------------------- */
/*      Build query for this specific feature.                          */
/* -------------------------------------------------------------------- */
    OGROCIStringBuf oCmd;
    char *pszFields = BuildFields();

    oCmd.Append( "SELECT " );
    oCmd.Append( pszFields );
    oCmd.Append( " FROM " );
    oCmd.Append( poFeatureDefn->GetName() );
    oCmd.Append( " " );
    oCmd.Appendf( 50+strlen(pszFIDName), 
                  " WHERE \"%s\" = %ld ", 
                  pszFIDName, nFeatureId );

/* -------------------------------------------------------------------- */
/*      Execute the statement.                                          */
/* -------------------------------------------------------------------- */
    if( !ExecuteQuery( oCmd.GetString() ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get the feature.                                                */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature;

    poFeature = GetNextRawFeature();
    
    if( poFeature != NULL && poFeature->GetGeometryRef() != NULL )
        poFeature->GetGeometryRef()->assignSpatialReference( poSRS );

/* -------------------------------------------------------------------- */
/*      Cleanup the statement.                                          */
/* -------------------------------------------------------------------- */
    ResetReading();

/* -------------------------------------------------------------------- */
/*      verify the FID.                                                 */
/* -------------------------------------------------------------------- */
    if( poFeature != NULL && poFeature->GetFID() != nFeatureId )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OGROCITableLayer::GetFeature(%d) ... query returned feature %d instead!",
                  nFeatureId, poFeature->GetFID() );
        delete poFeature;
        return NULL;
    }
    else
        return poFeature;
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
            if( poFeature->GetGeometryRef() != NULL )
                poFeature->GetGeometryRef()->assignSpatialReference( poSRS );
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
/*                                                                      */
/*      We implement SetFeature() by deleting the existing row (if      */
/*      it exists), and then using CreateFeature() to write it out      */
/*      tot he table normally.  CreateFeature() will preserve the       */
/*      existing FID if possible.                                       */
/************************************************************************/

OGRErr OGROCITableLayer::SetFeature( OGRFeature *poFeature )

{
/* -------------------------------------------------------------------- */
/*      Do some validation.                                             */
/* -------------------------------------------------------------------- */
    if( pszFIDName == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OGROCITableLayer::SetFeature(%d) failed because there is "
                  "no apparent FID column on table %s.",
                  poFeature->GetFID(), 
                  poFeatureDefn->GetName() );

        return OGRERR_FAILURE;
    }

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OGROCITableLayer::SetFeature(%d) failed because the feature "
                  "has no FID!", poFeature->GetFID() );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Prepare the delete command, and execute.  We don't check the    */
/*      error result of the execute, since attempting to Set a          */
/*      non-existing feature may be OK.                                 */
/* -------------------------------------------------------------------- */
    OGROCIStringBuf     oCmdText;
    OGROCIStatement     oCmdStatement( poDS->GetSession() );

    oCmdText.Appendf( strlen(poFeatureDefn->GetName())+strlen(pszFIDName)+100,
                      "DELETE FROM %s WHERE \"%s\" = %d",
                      poFeatureDefn->GetName(), 
                      pszFIDName, 
                      poFeature->GetFID() );

    oCmdStatement.Execute( oCmdText.GetString() );

    return CreateFeature( poFeature );
}

/************************************************************************/
/*                            PushOrdinal()                             */
/************************************************************************/

void OGROCITableLayer::PushOrdinal( double dfOrd )

{
    if( nOrdinalCount == nOrdinalMax )
    {
        nOrdinalMax = nOrdinalMax * 2 + 100;
        padfOrdinals = (double *) CPLRealloc(padfOrdinals, 
                                             sizeof(double) * nOrdinalMax);
    }
    
    padfOrdinals[nOrdinalCount++] = dfOrd;
}

/************************************************************************/
/*                       TranslateElementGroup()                        */
/*                                                                      */
/*      Append one or more element groups to the existing element       */
/*      info and ordinates lists for the passed geometry.               */
/************************************************************************/

OGRErr 
OGROCITableLayer::TranslateElementGroup( OGRGeometry *poGeometry, 
                                         OGROCIStringBuf *poElemInfo )

{
    switch( wkbFlatten(poGeometry->getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = (OGRPoint *) poGeometry;

          if( poElemInfo->GetLast() != '(' )
              poElemInfo->Append( "," );
          
          poElemInfo->Appendf( 32, "%d,1,1", nOrdinalCount+1 );

          PushOrdinal( poPoint->getX() );
          PushOrdinal( poPoint->getY() );
          if( nDimension == 3 )
              PushOrdinal( poPoint->getZ() );

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

              poElemInfo->Appendf( 32, "%d,2,1", nOrdinalCount+1 );
          }

          for( iVert = 0; iVert < poLine->getNumPoints(); iVert++ )
          {
              PushOrdinal( poLine->getX(iVert) );
              PushOrdinal( poLine->getY(iVert) );
              if( nDimension == 3 )
                  PushOrdinal( poLine->getZ(iVert) );
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
                  poElemInfo->Appendf( 20, "%d,1003,1", nOrdinalCount+1 );
              else
                  poElemInfo->Appendf( 20, "%d,2003,1", nOrdinalCount+1 );

              // but recurse to add the ordinates.
              eErr = TranslateElementGroup( poRing, NULL );
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
    nOrdinalCount = 0;

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
        OGROCIStringBuf oElemInfo;

        oElemInfo.Appendf( 100, "%s(%d,NULL,NULL,MDSYS.SDO_ELEM_INFO_ARRAY(",
                           SDO_GEOMETRY, nDimension * 1000 + 2 );

        TranslateElementGroup( poGeometry, &oElemInfo );

        oElemInfo.Append( "),:ordinates)" );
        return oElemInfo.StealString();
    }

/* ==================================================================== */
/*      Handle a polygon geometry.                                      */
/* ==================================================================== */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbPolygon )
    {
        OGROCIStringBuf oElemInfo;

/* -------------------------------------------------------------------- */
/*      Prepare eleminfo section.                                       */
/* -------------------------------------------------------------------- */
        oElemInfo.Appendf( 90, "%s(%d,NULL,NULL,MDSYS.SDO_ELEM_INFO_ARRAY(",
                           SDO_GEOMETRY, nDimension == 2 ? 2003 : 3003 );

/* -------------------------------------------------------------------- */
/*      Translate and return.                                           */
/* -------------------------------------------------------------------- */
        TranslateElementGroup( poGeometry, &oElemInfo );

        oElemInfo.Append( "),:ordinates)" );

        return oElemInfo.StealString();
    }

/* ==================================================================== */
/*      Handle a multi point geometry.                                  */
/* ==================================================================== */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPoint )
    {
        OGRMultiPoint *poMP = (OGRMultiPoint *) poGeometry;
        char *pszResult;
        int  iVert;

        pszResult = (char *) CPLMalloc(500);

        sprintf( pszResult, 
                 "%s(%d,NULL,NULL,MDSYS.SDO_ELEM_INFO_ARRAY(1,1,%d),"
                 ":ordinates)", 
                 SDO_GEOMETRY, nDimension*1000 + 5, poMP->getNumGeometries() );
        
        for( iVert = 0; iVert < poMP->getNumGeometries(); iVert++ )
        {
            OGRPoint *poPoint = (OGRPoint *)poMP->getGeometryRef( iVert );

            PushOrdinal( poPoint->getX() );
            PushOrdinal( poPoint->getY() );
            if( nDimension == 3 )
                PushOrdinal( poPoint->getZ() );
        }
        return pszResult;
    }

/* ==================================================================== */
/*      Handle other geometry collections.                              */
/* ==================================================================== */
    else
    {
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
            TranslateElementGroup( poGC->getGeometryRef(iChild), &oElemInfo );

/* -------------------------------------------------------------------- */
/*      Collect and return.                                             */
/* -------------------------------------------------------------------- */
        oElemInfo.Append( "),:ordinates)" );

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
    sprintf( pszCommand, "INSERT INTO %s (", poFeatureDefn->GetName() );

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
/*      Bind and translate the ordinates if we have some.               */
/* -------------------------------------------------------------------- */
    OGROCIStatement oInsert( poSession );
    int  bHaveOrdinates = strstr(pszCommand,":ordinates") != NULL;

    if( oInsert.Prepare( pszCommand ) != CE_None )
    {
        CPLFree( pszCommand );
        return OGRERR_FAILURE;
    }

    CPLFree( pszCommand );
    if( bHaveOrdinates )
    {
        OCIBind *hBindOrd = NULL;
        int i;
        OCINumber oci_number; 

        // Create or clear VARRAY 
        if( hOrdVARRAY == NULL )
        {
            if( poSession->Failed(
                OCIObjectNew( poSession->hEnv, poSession->hError, 
                              poSession->hSvcCtx, OCI_TYPECODE_VARRAY,
                              poSession->hOrdinatesTDO, (dvoid *)NULL, 
                              OCI_DURATION_SESSION,
                              FALSE, (dvoid **)&hOrdVARRAY),
                "OCIObjectNew(hOrdVARRAY)") )
                return OGRERR_FAILURE;
        }
        else
        {
            sb4  nOldCount;

            OCICollSize( poSession->hEnv, poSession->hError, 
                         hOrdVARRAY, &nOldCount );
            OCICollTrim( poSession->hEnv, poSession->hError, 
                         nOldCount, hOrdVARRAY );
        }

        // Prepare the VARRAY of ordinate values. 
	for (i = 0; i < nOrdinalCount; i++)
	{
            if( poSession->Failed( 
                OCINumberFromReal( poSession->hError, 
                                   (dvoid *) (padfOrdinals + i),
                                   (uword)sizeof(double),
                                   &oci_number),
                "OCINumberFromReal") )
                return OGRERR_FAILURE;

            if( poSession->Failed( 
                OCICollAppend( poSession->hEnv, poSession->hError,
                               (dvoid *) &oci_number,
                               (dvoid *)0, hOrdVARRAY),
                "OCICollAppend") )
                return OGRERR_FAILURE;
	}

        // Do the binding.
        if( poSession->Failed( 
            OCIBindByName( oInsert.GetStatement(), &hBindOrd, 
                           poSession->hError,
                           (text *) ":ordinates", (sb4) -1, (dvoid *) 0, 
                           (sb4) 0, SQLT_NTY, (dvoid *)0, (ub2 *)0, 
                           (ub2 *)0, (ub4)0, (ub4 *)0, 
                           (ub4)OCI_DEFAULT),
            "OCIBindByName(:ordinates)") )
            return OGRERR_FAILURE;

        if( poSession->Failed(
            OCIBindObject( hBindOrd, poSession->hError, 
                           poSession->hOrdinatesTDO,
                           (dvoid **)&hOrdVARRAY, (ub4 *)0, 
                           (dvoid **)0, (ub4 *)0),
            "OCIBindObject(:ordinates)" ) )
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    if( oInsert.Execute( NULL ) != CE_None )
        return OGRERR_FAILURE;
    else
        return OGRERR_NONE;
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

        poSession->CleanName( pszSafeName );
        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );
    }
    
/* -------------------------------------------------------------------- */
/*      Work out the PostgreSQL type.                                   */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
    {
        if( bPreservePrecision && oField.GetWidth() != 0 )
            sprintf( szFieldType, "NUMBER(%d)", oField.GetWidth() );
        else
            strcpy( szFieldType, "INTEGER" );
    }
    else if( oField.GetType() == OFTReal )
    {
        if( bPreservePrecision && oField.GetWidth() != 0 )
            sprintf( szFieldType, "NUMBER(%d,%d)", 
                     oField.GetWidth(), oField.GetPrecision() );
        else
            strcpy( szFieldType, "FLOAT(126)" );
    }
    else if( oField.GetType() == OFTString )
    {
        if( oField.GetWidth() == 0 || !bPreservePrecision )
            strcpy( szFieldType, "VARCHAR(2048)" );
        else
            sprintf( szFieldType, "CHAR(%d)", oField.GetWidth() );
    }
    else if( bApproxOK )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Can't create field %s with type %s on Oracle layers.  Creating as VARCHAR.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "VARCHAR(2048)" );
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

    sprintf( oCommand.GetString(), "ALTER TABLE %s ADD \"%s\" %s", 
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

    sprintf( szCommand, "SELECT COUNT(*) FROM %s %s", 
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
/*                            ParseDIMINFO()                            */
/************************************************************************/

void OGROCITableLayer::ParseDIMINFO( const char *pszOptionName, 
                                     double *pdfMin, 
                                     double *pdfMax,
                                     double *pdfRes )

{
    const char *pszUserDIMINFO;
    char **papszTokens;

    pszUserDIMINFO = CSLFetchNameValue( papszOptions, pszOptionName );
    if( pszUserDIMINFO == NULL )
        return;

    papszTokens = 
        CSLTokenizeStringComplex( pszUserDIMINFO, ",", FALSE, FALSE );
    if( CSLCount(papszTokens) != 3 )
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Ignoring %s, it does not contain three comma separated values.", 
                  pszOptionName );
        return;
    }

    *pdfMin = atof(papszTokens[0]);
    *pdfMax = atof(papszTokens[1]);
    *pdfRes = atof(papszTokens[2]);

    CSLDestroy( papszTokens );
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

/* -------------------------------------------------------------------- */
/*      If the dimensions are degenerate (all zeros) then we assume     */
/*      there were no geometries, and we don't bother setting the       */
/*      dimensions.                                                     */
/* -------------------------------------------------------------------- */
    if( sExtent.MaxX == 0 && sExtent.MinX == 0
        && sExtent.MaxY == 0 && sExtent.MinY == 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Layer %s appears to have no geometry ... not setting SDO DIMINFO metadata.", 
                  poFeatureDefn->GetName() );
        return;
                  
    }

/* -------------------------------------------------------------------- */
/*      Establish the extents and resolution to use.                    */
/* -------------------------------------------------------------------- */
    double           dfResSize;
    double           dfXMin, dfXMax, dfXRes;
    double           dfYMin, dfYMax, dfYRes;
    double           dfZMin, dfZMax, dfZRes;

    if( sExtent.MaxX - sExtent.MinX > 400 )
        dfResSize = 0.001;
    else
        dfResSize = 0.0000001;

    dfXMin = sExtent.MinX - dfResSize * 3;
    dfXMax = sExtent.MaxX + dfResSize * 3;
    dfXRes = dfResSize;
    ParseDIMINFO( "DIMINFO_X", &dfXMin, &dfXMax, &dfXRes );
    
    dfYMin = sExtent.MinY - dfResSize * 3;
    dfYMax = sExtent.MaxY + dfResSize * 3;
    dfYRes = dfResSize;
    ParseDIMINFO( "DIMINFO_Y", &dfYMin, &dfYMax, &dfYRes );
    
    dfZMin = -100000.0;
    dfZMax = 100000.0;
    dfZRes = 0.002;
    ParseDIMINFO( "DIMINFO_Z", &dfZMin, &dfZMax, &dfZRes );
    
/* -------------------------------------------------------------------- */
/*      Prepare dimension update statement.                             */
/* -------------------------------------------------------------------- */
    sDimUpdate.Append( "UPDATE USER_SDO_GEOM_METADATA SET DIMINFO = " );
    sDimUpdate.Append( "MDSYS.SDO_DIM_ARRAY(" );

    sDimUpdate.Appendf(200,
                       "MDSYS.SDO_DIM_ELEMENT('X',%.16g,%.16g,%.12g)",
                       dfXMin, dfXMax, dfXRes );
    sDimUpdate.Appendf(200,
                       ",MDSYS.SDO_DIM_ELEMENT('Y',%.16g,%.16g,%.12g)",
                       dfYMin, dfYMax, dfYRes );

    if( nDimension == 3 )
    {
        sDimUpdate.Appendf(200,
                           ",MDSYS.SDO_DIM_ELEMENT('Z',%.16g,%.16g,%.12g)",
                           dfZMin, dfZMax, dfZRes );
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
/*      If the user has disabled INDEX support then don't create the    */
/*      index.                                                          */
/* -------------------------------------------------------------------- */
    if( !CSLFetchBoolean( papszOptions, "INDEX", TRUE ) )
        return;

/* -------------------------------------------------------------------- */
/*      Establish an index name.  For some reason Oracle 8.1.7 does     */
/*      not support spatial index names longer than 18 characters so    */
/*      we magic up an index name if it would be too long.              */
/* -------------------------------------------------------------------- */
    char  szIndexName[20];

    if( strlen(poFeatureDefn->GetName()) < 15 )
        sprintf( szIndexName, "%s_idx", poFeatureDefn->GetName() );
    else if( strlen(poFeatureDefn->GetName()) < 17 )
        sprintf( szIndexName, "%si", poFeatureDefn->GetName() );
    else
    {
        int i, nHash = 0;
        const char *pszSrcName = poFeatureDefn->GetName();

        for( i = 0; pszSrcName[i] != '\0'; i++ )
            nHash = (nHash + i * pszSrcName[i]) % 987651;
        
        sprintf( szIndexName, "OSI_%d", nHash );
    }

    poDS->GetSession()->CleanName( szIndexName );

/* -------------------------------------------------------------------- */
/*      Try creating an index on the table now.  Use a simple 5         */
/*      level quadtree based index.  Would R-tree be a better default?  */
/* -------------------------------------------------------------------- */

// Disable for now, spatial index creation always seems to cause me to 
// lose my connection to the database!
    OGROCIStringBuf  sIndexCmd;

    sIndexCmd.Appendf( 10000, "CREATE INDEX \"%s\" ON %s(\"%s\") "
                       "INDEXTYPE IS MDSYS.SPATIAL_INDEX ",
                       szIndexName, 
                       poFeatureDefn->GetName(), 
                       pszGeomName );

    if( CSLFetchNameValue( papszOptions, "INDEX_PARAMETERS" ) != NULL )
    {
        sIndexCmd.Append( " PARAMETERS( '" );
        sIndexCmd.Append( CSLFetchNameValue(papszOptions,"INDEX_PARAMETERS") );
        sIndexCmd.Append( "' )" );
    }

    if( oExecStatement.Execute( sIndexCmd.GetString() ) != CE_None )
    {
        char szDropCommand[2000];
        sprintf( szDropCommand, "DROP INDEX \"%s\"", szIndexName );
        oExecStatement.Execute( szDropCommand );
    }
}
