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

/************************************************************************/
/*                          OGROCITableLayer()                          */
/************************************************************************/

OGROCITableLayer::OGROCITableLayer( OGROCIDataSource *poDSIn, 
                                    const char * pszTableName,
                                    int bUpdate )

{
    poDS = poDSIn;

    pszQuery = NULL;
    pszWHERE = CPLStrdup( "" );
    pszQueryStatement = NULL;
    
    bUpdateAccess = bUpdate;

    iNextShapeId = 0;

    poFeatureDefn = ReadTableDefinition( pszTableName );
    
    ResetReading();

    bLaunderColumnNames = TRUE;
}

/************************************************************************/
/*                         ~OGROCITableLayer()                          */
/************************************************************************/

OGROCITableLayer::~OGROCITableLayer()

{
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
    char		szCommand[1024];
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
    char	szWHERE[4096];

    CPLFree( pszWHERE );
    pszWHERE = NULL;

    szWHERE[0] = '\0';
#ifdef notdef
    if( poFilterGeom != NULL && bHasPostGISGeometry )
    {
        OGREnvelope  sEnvelope;

        poFilterGeom->getEnvelope( &sEnvelope );
        sprintf( szWHERE, 
                 "WHERE %s && GeometryFromText('BOX3D(%.12f %.12f, %.12f %.12f)'::box3d,%d) ",
                 pszGeomColumn, 
                 sEnvelope.MinX, sEnvelope.MinY, 
                 sEnvelope.MaxX, sEnvelope.MaxY,
                 nSRSId );
    }
#endif

    if( pszQuery != NULL )
    {
        if( strlen(szWHERE) == 0 )
            sprintf( szWHERE, "WHERE %s ", pszQuery  );
        else
            sprintf( szWHERE+strlen(szWHERE), "AND %s ", pszQuery );
    }

    pszWHERE = CPLStrdup(szWHERE);
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

    char szCommand[6000];
    char *pszFields = BuildFields();

    sprintf( szCommand, 
             "SELECT %s FROM \"%s\" %s", 
             pszFields, poFeatureDefn->GetName(), pszWHERE );
    
    CPLFree( pszFields );

    pszQueryStatement = CPLStrdup( szCommand );
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
            return NULL;

        if( poFilterGeom == NULL
            || poFilterGeom->Intersect( poFeature->GetGeometryRef() ) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROCITableLayer::ResetReading()

{
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
    int		i, nSize;
    char	*pszFieldList;

    nSize = 25;

    if( pszGeomName )
        nSize += strlen(pszGeomName);

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        nSize += strlen(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + 4;

    pszFieldList = (char *) CPLMalloc(nSize);

    if( pszGeomName )							
    {
        sprintf( pszFieldList, "\"%s\"", pszGeomName );
        iGeomColumn = 0;
    }
    else
        pszFieldList[0] = '\0';

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        strcat( pszFieldList, "\"" );
        strcat( pszFieldList, pszName );
        strcat( pszFieldList, "\"" );
    }

    CPLAssert( (int) strlen(pszFieldList) < nSize );

    return pszFieldList;
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
                     "%s(%d,NULL,SDO_POINT(%.16g,%.16g),NULL,NULL)",
                     SDO_GEOMETRY, 2001, poPoint->getX(), poPoint->getY() );
        else
            sprintf( szResult, 
                     "%s(%d,NULL,SDO_POINT(%.16g,%.16g,%.16g),NULL,NULL)",
                     SDO_GEOMETRY, 2001, 
                     poPoint->getX(), poPoint->getY(), poPoint->getZ() );

        return CPLStrdup(szResult );
    }

/* ==================================================================== */
/*      Handle a line string geometry.                                  */
/* ==================================================================== */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbLineString )
    {
        OGRLineString *poLine = (OGRLineString *) poGeometry;
        int nDim = poGeometry->getDimension();
        char *pszResult;
        int  iOff, iVert;

        pszResult = (char *) CPLMalloc(poLine->getNumPoints() * 51 + 500);

        sprintf( pszResult, 
                 "%s(%d,NULL,NULL,MDSYS.SDO_ELEM_INFO_ARRAY(1,2,1),"
                 "MDSYS.SDO_ORDINATE_ARRAY(", 
                 SDO_GEOMETRY, nDim * 1000 + 2 );
        
        iOff = strlen(pszResult);
        for( iVert = 0; iVert < poLine->getNumPoints(); iVert++ )
        {
            if( iVert != 0 )
                pszResult[iOff++] = ',';

            sprintf( pszResult + iOff, "%.16g,%.16g", 
                     poLine->getX(iVert), poLine->getY(iVert) );
            iOff += strlen(pszResult+iOff);
            if( nDim == 3 )
            {
                sprintf( pszResult + iOff, ",%.16g", poLine->getZ(iVert) );
                iOff += strlen(pszResult+iOff);
            }
        }
        strcat( pszResult + iOff, "))" );
        return pszResult;
    }

/* ==================================================================== */
/*      Handle a polygon geometry.                                      */
/* ==================================================================== */
    else if( wkbFlatten(poGeometry->getGeometryType()) == wkbPolygon )
    {
        OGRPolygon *poPoly = (OGRPolygon *) poGeometry;
        OGRLinearRing *poRing;
        int nDim = poGeometry->getDimension();
        char *pszResult;
        int  iOff, iVert, iRing, nBufSize;

/* -------------------------------------------------------------------- */
/*      Work out buffer size required based on total vertex count       */
/*      for all rings.                                                  */
/* -------------------------------------------------------------------- */
        nBufSize = 500;
        for( iRing = -1; iRing < poPoly->getNumInteriorRings(); iRing++ )
        {
            if( iRing == -1 )
                poRing = poPoly->getExteriorRing();
            else
                poRing = poPoly->getInteriorRing(iRing);
            nBufSize += poRing->getNumPoints() * 51 + 30;
        }

/* -------------------------------------------------------------------- */
/*      Prepare base command.                                           */
/* -------------------------------------------------------------------- */
        pszResult = (char *) CPLMalloc(nBufSize);

        sprintf( pszResult, 
                 "%s(%d,NULL,NULL,MDSYS.SDO_ELEM_INFO_ARRAY(",
                 SDO_GEOMETRY, nDim == 2 ? 2003 : 3003 );
        iOff = strlen(pszResult);

/* -------------------------------------------------------------------- */
/*      Add elem_info portion data.                                     */
/* -------------------------------------------------------------------- */
        iVert = 0;
        for( iRing = -1; iRing < poPoly->getNumInteriorRings(); iRing++ )
        {
            if( iRing == -1 )
            {
                poRing = poPoly->getExteriorRing();
                sprintf(pszResult + iOff, "1,1003,1" );			
            }
            else
            {
                poRing = poPoly->getInteriorRing(iRing);
                sprintf(pszResult + iOff, ",%d,2003,1",
                        iVert * nDim + 1 );
            }
            iVert += poRing->getNumPoints();
            iOff += strlen(pszResult+iOff);
        }

/* -------------------------------------------------------------------- */
/*      Add ordinates array.                                            */
/* -------------------------------------------------------------------- */
        sprintf( pszResult+iOff, "),MDSYS.SDO_ORDINATE_ARRAY(" );
        iOff += strlen(pszResult+iOff);
        
        for( iRing = -1; iRing < poPoly->getNumInteriorRings(); iRing++ )
        {
            if( iRing == -1 )
            {
                poRing = poPoly->getExteriorRing();
            }
            else
            {
                poRing = poPoly->getInteriorRing(iRing);
            }

            for( iVert = 0; iVert < poRing->getNumPoints(); iVert++ )
            {
                CPLAssert( iOff < nBufSize - 60 );

                if( iVert != 0 || iRing != -1 )
                    pszResult[iOff++] = ',';
                
                sprintf( pszResult + iOff, "%.16g,%.16g", 
                         poRing->getX(iVert), poRing->getY(iVert) );
                iOff += strlen(pszResult+iOff);
                if( nDim == 3 )
                {
                    sprintf( pszResult + iOff, ",%.16g", poRing->getZ(iVert) );
                    iOff += strlen(pszResult+iOff);
                }
            }
        }
        strcat( pszResult + iOff, "))" );

        return pszResult;
    }

    return NULL;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGROCITableLayer::CreateFeature( OGRFeature *poFeature )

{
    OGROCISession      *poSession = poDS->GetSession();
    char		*pszCommand;
    int                 i, bNeedComma;
    unsigned int        nCommandBufSize;;
    OGRErr              eErr;

    nCommandBufSize = 2000;
    pszCommand = (char *) CPLMalloc(nCommandBufSize);

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.  					*/
/* -------------------------------------------------------------------- */
    sprintf( pszCommand, "INSERT INTO \"%s\" (", poFeatureDefn->GetName() );

    if( poFeature->GetGeometryRef() != NULL )
    {
        strcat( pszCommand, pszGeomName );
        strcat( pszCommand, " " );					
    }
    
    bNeedComma = poFeature->GetGeometryRef() != NULL;
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
/*      Set the other fields.                                           */
/* -------------------------------------------------------------------- */
    int nOffset = strlen(pszCommand);

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
                if( pszStrValue[iChar] == '\\' 
                    || pszStrValue[iChar] == '\'' )
                {
                    pszCommand[nOffset++] = '\\';
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
    char		szCommand[1024];
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
            strcpy( szFieldType, "VARCHAR" );
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
    OGROCIStatement oAddField( poSession );

    sprintf( szCommand, "ALTER TABLE \"%s\" ADD \"%s\" %s", 
             poFeatureDefn->GetName(), oField.GetNameRef(), szFieldType );
    if( oAddField.Execute( szCommand ) != CE_None )
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
