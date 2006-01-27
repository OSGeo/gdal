/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCILayer class.  This is layer semantics
 *           shared between table accessors and ExecuteSQL() result 
 *           pseudo-layers.
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
 * Revision 1.14  2006/01/27 15:47:36  fwarmerdam
 * preliminary Get{FID/Geometry}Column support
 *
 * Revision 1.13  2005/09/21 00:59:36  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.12  2005/02/22 12:57:51  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.11  2005/02/02 20:54:27  fwarmerdam
 * track m_nFeaturesRead
 *
 * Revision 1.10  2004/11/22 19:24:15  fwarmerdam
 * added support for a list of tables in the datasource name
 *
 * Revision 1.9  2004/11/22 17:19:14  fwarmerdam
 * fixed missing case for 3d multipolygons: bug 618
 *
 * Revision 1.8  2004/11/10 20:17:14  fwarmerdam
 * fixed memory leak of geometry objects
 *
 * Revision 1.7  2003/05/21 03:54:01  warmerda
 * expand tabs
 *
 * Revision 1.6  2003/01/07 21:13:35  warmerda
 * executequery() now takes query as input, getfeature moved
 *
 * Revision 1.5  2003/01/07 18:16:26  warmerda
 * fixed some egrarious errors in geometry support
 *
 * Revision 1.4  2003/01/06 17:58:50  warmerda
 * restructure geometry translation, add FID support
 *
 * Revision 1.3  2002/12/29 19:43:59  warmerda
 * avoid some warnings
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGROCILayer()                               */
/************************************************************************/

OGROCILayer::OGROCILayer()

{
    poDS = NULL;
    poStatement = NULL;

    pszQueryStatement = NULL;
    pszGeomName = NULL;
    iGeomColumn = -1;
    pszFIDName = NULL;
    iFIDColumn = -1;

    hLastGeom = NULL;
    hLastGeomInd = NULL;

    iNextShapeId = 0;
}

/************************************************************************/
/*                            ~OGROCILayer()                             */
/************************************************************************/

OGROCILayer::~OGROCILayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "OCI", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    ResetReading();

    CPLFree( pszGeomName );
    pszGeomName = NULL;

    CPLFree( pszFIDName );
    pszFIDName = NULL;

    CPLFree( pszQueryStatement );
    pszQueryStatement = NULL;

    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROCILayer::ResetReading()

{
    if( poStatement != NULL )
        delete poStatement;
    poStatement = NULL;

    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/*                                                                      */
/*      By default we implement the full spatial and attribute query    */
/*      semantics manually here.  The table query class will            */
/*      override this method and implement these inline, but the        */
/*      simple SELECT statement evaluator (OGROCISelectLayer) will      */
/*      depend us this code implementing additional spatial or          */
/*      attribute query semantics.                                      */
/************************************************************************/

OGRFeature *OGROCILayer::GetNextFeature()

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
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGROCILayer::GetNextRawFeature()

{
/* -------------------------------------------------------------------- */
/*      Do we need to establish an initial query?                       */
/* -------------------------------------------------------------------- */
    if( iNextShapeId == 0 && poStatement == NULL )
    {
        if( !ExecuteQuery(pszQueryStatement) )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Are we in some sort of error condition?                         */
/* -------------------------------------------------------------------- */
    hLastGeom = NULL;

    char **papszResult = poStatement->SimpleFetchRow();

    if( papszResult == NULL )
    {
        iNextShapeId = MAX(1,iNextShapeId);
        delete poStatement;
        poStatement = NULL;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    int         iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    poFeature->SetFID( iNextShapeId );
    iNextShapeId++;
    m_nFeaturesRead++;

    if( iFIDColumn != -1 && papszResult[iFIDColumn] != NULL )
        poFeature->SetFID( atoi(papszResult[iFIDColumn]) );

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if( papszResult[iField] != NULL )
            poFeature->SetField( iField, papszResult[iField] );
    }

/* -------------------------------------------------------------------- */
/*      Translate geometry if we have it.                               */
/* -------------------------------------------------------------------- */
    if( iGeomColumn != -1 )
    {
        poFeature->SetGeometryDirectly( TranslateGeometry() );

        OGROCISession      *poSession = poDS->GetSession();

        if( poFeature->GetGeometryRef() != NULL && hLastGeom != NULL )
            poSession->Failed( 
                OCIObjectFree(poSession->hEnv, poSession->hError, 
                              (dvoid *) hLastGeom, 
                              (ub2)OCI_OBJECTFREE_FORCE) );

        hLastGeom = NULL;
        hLastGeomInd = NULL;
    }

    nResultOffset++;

    return poFeature;
}

/************************************************************************/
/*                            ExecuteQuery()                            */
/*                                                                      */
/*      This is invoke when the first request for a feature is          */
/*      made.  It executes the query, and binds columns as needed.      */
/*      The OGROCIStatement is used for most of the work.               */
/************************************************************************/

int OGROCILayer::ExecuteQuery( const char *pszReqQuery )

{
    OGROCISession      *poSession = poDS->GetSession();

    CPLAssert( pszReqQuery != NULL );
    CPLAssert( poStatement == NULL );

/* -------------------------------------------------------------------- */
/*      Execute the query.                                              */
/* -------------------------------------------------------------------- */
    poStatement = new OGROCIStatement( poSession );
    if( poStatement->Execute( pszReqQuery ) != CE_None )
    {
        delete poStatement;
        poStatement = NULL;
        return FALSE;
    }
    nResultOffset = 0;

/* -------------------------------------------------------------------- */
/*      Do additional work binding the geometry column.                 */
/* -------------------------------------------------------------------- */
    if( iGeomColumn != -1 )
    {
        OCIDefine *hGDefine = NULL;

        if( poSession->Failed(  
            OCIDefineByPos(poStatement->GetStatement(), &hGDefine, 
                           poSession->hError,
                           (ub4) iGeomColumn+1, (dvoid *)0, (sb4)0, SQLT_NTY, 
                           (dvoid *)0, (ub2 *)0, (ub2 *)0, (ub4)OCI_DEFAULT),
            "OCIDefineByPos(geometry)") )
            return FALSE;
        
        if( poSession->Failed( 
            OCIDefineObject(hGDefine, poSession->hError, 
                            poSession->hGeometryTDO,
                            (dvoid **) &hLastGeom, (ub4 *)0, 
                            (dvoid **) &hLastGeomInd, (ub4 *)0 ),
            "OCIDefineObject") )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                         TranslateGeometry()                          */
/************************************************************************/

OGRGeometry *OGROCILayer::TranslateGeometry()

{
    OGROCISession      *poSession = poDS->GetSession();

/* -------------------------------------------------------------------- */
/*      Is the geometry NULL?                                           */
/* -------------------------------------------------------------------- */
    if( hLastGeom == NULL || hLastGeomInd == NULL 
        || hLastGeomInd->_atomic == OCI_IND_NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get the size of the sdo_elem_info and sdo_ordinates arrays.     */
/* -------------------------------------------------------------------- */
    int nElemCount, nOrdCount;

    if( poSession->Failed( 
        OCICollSize( poSession->hEnv, poSession->hError, 
                     (OCIColl *)(hLastGeom->sdo_elem_info), &nElemCount),
        "OCICollSize(sdo_elem_info)" ) )
        return NULL;

    if( poSession->Failed( 
        OCICollSize( poSession->hEnv, poSession->hError, 
                     (OCIColl *)(hLastGeom->sdo_ordinates), &nOrdCount),
        "OCICollSize(sdo_ordinates)" ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get the GType.                                                  */
/* -------------------------------------------------------------------- */
    int nGType;

    if( poSession->Failed( 
        OCINumberToInt(poSession->hError, &(hLastGeom->sdo_gtype),
                       (uword)sizeof(int), OCI_NUMBER_SIGNED,
                       (dvoid *)&nGType),
        "OCINumberToInt(GType)" ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Handle point data directly from built-in point info.            */
/* -------------------------------------------------------------------- */
    if( ORA_GTYPE_MATCH(nGType,ORA_GTYPE_POINT)
        && hLastGeomInd->sdo_point._atomic == OCI_IND_NOTNULL
        && hLastGeomInd->sdo_point.x == OCI_IND_NOTNULL
        && hLastGeomInd->sdo_point.y == OCI_IND_NOTNULL )
    {
        double     dfX, dfY, dfZ = 0.0;

        OCINumberToReal(poSession->hError, &(hLastGeom->sdo_point.x), 
                        (uword)sizeof(double), (dvoid *)&dfX);
        OCINumberToReal(poSession->hError, &(hLastGeom->sdo_point.y), 
                        (uword)sizeof(double), (dvoid *)&dfY);
        if( hLastGeomInd->sdo_point.z == OCI_IND_NOTNULL )
            OCINumberToReal(poSession->hError, &(hLastGeom->sdo_point.z), 
                            (uword)sizeof(double), (dvoid *)&dfZ);

        return new OGRPoint( dfX, dfY, dfZ );
    }

/* -------------------------------------------------------------------- */
/*      Establish the dimension.                                        */
/* -------------------------------------------------------------------- */
    int nDimension = MAX(2,(nGType / 1000));

/* -------------------------------------------------------------------- */
/*      If this is a sort of container geometry, create the             */
/*      container now.                                                  */
/* -------------------------------------------------------------------- */
    OGRGeometryCollection *poCollection = NULL;
    OGRPolygon *poPolygon = NULL;

    if( ORA_GTYPE_MATCH(nGType,ORA_GTYPE_POLYGON) )
        poPolygon = new OGRPolygon();
    else if( ORA_GTYPE_MATCH(nGType,ORA_GTYPE_COLLECTION) )
        poCollection = new OGRGeometryCollection();
    else if( ORA_GTYPE_MATCH(nGType,ORA_GTYPE_MULTIPOINT) )
        poCollection = new OGRMultiPoint();
    else if( ORA_GTYPE_MATCH(nGType,ORA_GTYPE_MULTILINESTRING) )
        poCollection = new OGRMultiLineString();
    else if( ORA_GTYPE_MATCH(nGType,ORA_GTYPE_MULTIPOLYGON) )
        poCollection = new OGRMultiPolygon();

/* ==================================================================== */
/*      Loop over the component elements.                               */
/* ==================================================================== */
    ub4   nNextStartOrdinal = 1;

    for( int iElement = 0; iElement < nElemCount; iElement += 3 )
    {
        boolean bExists;
        OCINumber *hNumber;
        ub4       nInterpretation, nEType;
        int       nStartOrdinal = nNextStartOrdinal;

/* -------------------------------------------------------------------- */
/*      Get the details about element from the elem_info array.         */
/* -------------------------------------------------------------------- */
        OCICollGetElem(poSession->hEnv, poSession->hError, 
                       (OCIColl *)(hLastGeom->sdo_elem_info), 
                       (sb4)(iElement+1), (boolean *)&bExists, 
                       (dvoid **)&hNumber, NULL );
        OCINumberToInt(poSession->hError, hNumber, (uword)sizeof(ub4), 
                       OCI_NUMBER_UNSIGNED, (dvoid *) &nEType );
        
        OCICollGetElem(poSession->hEnv, poSession->hError, 
                       (OCIColl *)(hLastGeom->sdo_elem_info), 
                       (sb4)(iElement+2), (boolean *)&bExists, 
                       (dvoid **)&hNumber, NULL );
        OCINumberToInt(poSession->hError, hNumber, (uword)sizeof(ub4), 
                       OCI_NUMBER_UNSIGNED, (dvoid *) &nInterpretation );

        if( iElement < nElemCount-3 )
        {
            OCICollGetElem(poSession->hEnv, poSession->hError, 
                           (OCIColl *)(hLastGeom->sdo_elem_info), 
                           (sb4)(iElement+3), (boolean *)&bExists, 
                           (dvoid **)&hNumber,NULL);
            OCINumberToInt(poSession->hError, hNumber, (uword)sizeof(ub4), 
                           OCI_NUMBER_UNSIGNED, (dvoid *) &nNextStartOrdinal );
        }
        else
            nNextStartOrdinal = nOrdCount+1;

/* -------------------------------------------------------------------- */
/*      Translate this element.                                         */
/* -------------------------------------------------------------------- */
        OGRGeometry *poGeom;

        poGeom = TranslateGeometryElement( nGType, nDimension, 
                                           nEType, nInterpretation,
                                           nStartOrdinal - 1, 
                                           nNextStartOrdinal - nStartOrdinal );
        if( poGeom == NULL )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Based on GType do what is appropriate.                          */
/* -------------------------------------------------------------------- */
        if( ORA_GTYPE_MATCH(nGType,ORA_GTYPE_LINESTRING) )
        {
            CPLAssert(wkbFlatten(poGeom->getGeometryType()) == wkbLineString);
            return poGeom;
        }

        else if( ORA_GTYPE_MATCH(nGType,ORA_GTYPE_POINT) )
        {
            CPLAssert(wkbFlatten(poGeom->getGeometryType()) == wkbPoint);
            return poGeom;
        }

        else if( ORA_GTYPE_MATCH(nGType,ORA_GTYPE_POLYGON) )
        {
            CPLAssert(wkbFlatten(poGeom->getGeometryType()) == wkbLineString );
            poPolygon->addRingDirectly( (OGRLinearRing *) poGeom );
        }
        else 
        {
            CPLAssert( poCollection != NULL );
            if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
            {
                int  i;
                OGRMultiPoint *poMP = (OGRMultiPoint *) poGeom;

                for( i = 0; i < poMP->getNumGeometries(); i++ )
                    poCollection->addGeometry( poMP->getGeometryRef(i) );
                delete poMP;
            }
            else if( nEType % 1000 == 3 )
            {
                /* its one poly ring, create new poly or add to existing */
                if( nEType > 999 && nEType < 2999 )
                {
                    if( poPolygon != NULL 
                        && poPolygon->getExteriorRing() != NULL )
                    {
                        poCollection->addGeometryDirectly( poPolygon );
                        poPolygon = NULL;
                    }

                    poPolygon = new OGRPolygon();
                }
                
				if( poPolygon != NULL )
                    poPolygon->addRingDirectly( (OGRLinearRing *) poGeom );
				else
				{
					CPLAssert( poPolygon != NULL );
				}
            }
            else
                poCollection->addGeometryDirectly( poGeom );
        }
    }

    if( poCollection != NULL 
        && poPolygon != NULL )
        poCollection->addGeometryDirectly( poPolygon );

/* -------------------------------------------------------------------- */
/*      Return resulting collection geometry.                           */
/* -------------------------------------------------------------------- */
    if( poCollection == NULL )
        return poPolygon;
    else
        return poCollection;
}

/************************************************************************/
/*                      TranslateGeometryElement()                      */
/************************************************************************/

OGRGeometry *
OGROCILayer::TranslateGeometryElement( int nGType, int nDimension,
                                       int nEType, int nInterpretation, 
                                       int nStartOrdinal, int nOrdCount )

{
/* -------------------------------------------------------------------- */
/*      Handle line strings.  For now we treat line strings of          */
/*      curves as if they were simple point line strings.               */
/* -------------------------------------------------------------------- */
    if( nEType == 1 && nInterpretation == 1 )
    {
        OGRPoint *poPoint = new OGRPoint();
        double dfX, dfY, dfZ = 0.0;

        GetOrdinalPoint( nStartOrdinal, nDimension, &dfX, &dfY, &dfZ );

        poPoint->setX( dfX );
        poPoint->setY( dfY );
        poPoint->setZ( dfZ );

        return poPoint;
    }

/* -------------------------------------------------------------------- */
/*      Handle line strings.  For now we treat line strings of          */
/*      curves as if they were simple point line strings.               */
/* -------------------------------------------------------------------- */
    else if( nEType == 1 )
    {
        OGRMultiPoint *poMP = new OGRMultiPoint();
        double dfX, dfY, dfZ = 0.0;
        int i;

        CPLAssert( nInterpretation == nOrdCount / nDimension );

        for( i = 0; i < nInterpretation; i++ )
        {
            GetOrdinalPoint( nStartOrdinal + i*nDimension, nDimension, 
                             &dfX, &dfY, &dfZ );

            OGRPoint *poPoint = new OGRPoint( dfX, dfY, dfZ );
            poMP->addGeometryDirectly( poPoint );
        }
        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      Handle line strings.  For now we treat line strings of          */
/*      curves as if they were simple point line strings.               */
/* -------------------------------------------------------------------- */
    else if( nEType == 2 )
    {
        OGRLineString *poLS = new OGRLineString();
        int nPointCount = nOrdCount / nDimension, i;

        poLS->setNumPoints( nPointCount );

        for( i = 0; i < nPointCount; i++ )
        {
            double dfX, dfY, dfZ = 0.0;

            GetOrdinalPoint( i*nDimension + nStartOrdinal, nDimension, 
                             &dfX, &dfY, &dfZ );
            poLS->setPoint( i, dfX, dfY, dfZ );
        }

        return poLS;
    }

/* -------------------------------------------------------------------- */
/*      Handle polygon rings.                                           */
/* -------------------------------------------------------------------- */
    else if( nEType % 1000 == 3 
             && (nInterpretation == 1 || nInterpretation == 2 ) )
    {
        OGRLinearRing *poLS = new OGRLinearRing();
        int nPointCount = nOrdCount / nDimension, i;

        poLS->setNumPoints( nPointCount );

        for( i = 0; i < nPointCount; i++ )
        {
            double dfX, dfY, dfZ = 0.0;

            GetOrdinalPoint( i*nDimension + nStartOrdinal, nDimension, 
                             &dfX, &dfY, &dfZ );
            poLS->setPoint( i, dfX, dfY, dfZ );
        }

        return poLS;
    }

/* -------------------------------------------------------------------- */
/*      Handle rectangle definitions ... translate into a linear ring.  */
/* -------------------------------------------------------------------- */
    else if( nEType % 1000 == 3 && nInterpretation == 3 )
    {
        OGRLinearRing *poLS = new OGRLinearRing();
        double dfX1, dfY1, dfZ1 = 0.0;
        double dfX2, dfY2, dfZ2 = 0.0;
        
        GetOrdinalPoint( nStartOrdinal, nDimension, 
                         &dfX1, &dfY1, &dfZ1 );
        GetOrdinalPoint( nStartOrdinal + nDimension, nDimension, 
                         &dfX2, &dfY2, &dfZ2 );
        
        poLS->setNumPoints( 5 );

        poLS->setPoint( 0, dfX1, dfY1, dfZ1 );
        poLS->setPoint( 1, dfX2, dfY1, dfZ1 );
        poLS->setPoint( 2, dfX2, dfY2, dfZ2 );
        poLS->setPoint( 3, dfX1, dfY2, dfZ2 );
        poLS->setPoint( 4, dfX1, dfY1, dfZ1 );

        return poLS;
    }

    return NULL;
}

/************************************************************************/
/*                          GetOrdinalPoint()                           */
/************************************************************************/

int OGROCILayer::GetOrdinalPoint( int iOrdinal, int nDimension,
                                  double *pdfX, double *pdfY, double *pdfZ )

{
    OGROCISession      *poSession = poDS->GetSession();
    boolean bExists;
    OCINumber *hNumber;

    OCICollGetElem( poSession->hEnv, poSession->hError,         
                    (OCIColl *)(hLastGeom->sdo_ordinates), 
                    (sb4)iOrdinal+0, (boolean *)&bExists, 
                    (dvoid **)&hNumber, NULL );
    OCINumberToReal(poSession->hError, hNumber, 
                    (uword)sizeof(double), (dvoid *)pdfX);
    OCICollGetElem( poSession->hEnv, poSession->hError,         
                    (OCIColl *)(hLastGeom->sdo_ordinates), 
                    (sb4)iOrdinal + 1, (boolean *)&bExists, 
                    (dvoid **)&hNumber, NULL );
    OCINumberToReal(poSession->hError, hNumber,
                    (uword)sizeof(double), (dvoid *)pdfY);
    if( nDimension == 3 )
    {
        OCICollGetElem( poSession->hEnv, poSession->hError,
                        (OCIColl *)(hLastGeom->sdo_ordinates), 
                        (sb4)iOrdinal + 2, (boolean *)&bExists, 
                        (dvoid **)&hNumber, NULL );
        OCINumberToReal(poSession->hError, hNumber,
                        (uword)sizeof(double), (dvoid *)pdfZ);
    }

    return TRUE;
}
                                       
/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROCILayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

    else 
        return FALSE;
}


/************************************************************************/
/*                          LookupTableSRID()                           */
/*                                                                      */
/*      Note that the table name may also be prefixed by the owner      */
/*      with a dot separator.                                           */
/************************************************************************/

int OGROCILayer::LookupTableSRID()

{
/* -------------------------------------------------------------------- */
/*      If we don't have a geometry column, there isn't much point      */
/*      in trying.                                                      */
/* -------------------------------------------------------------------- */
    if( pszGeomName == NULL )
        return -1;
    
/* -------------------------------------------------------------------- */
/*      Split out the owner if available.                               */
/* -------------------------------------------------------------------- */
    const char *pszTableName = GetLayerDefn()->GetName();
    char *pszOwner = NULL;

    if( strstr(pszTableName,".") != NULL )
    {
        pszOwner = CPLStrdup(pszTableName);
        pszTableName = strstr(pszTableName,".") + 1;

        *(strstr(pszOwner,".")) = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Build our query command.                                        */
/* -------------------------------------------------------------------- */
    OGROCIStringBuf oCommand;

    oCommand.Appendf( 1000, "SELECT SRID FROM ALL_SDO_GEOM_METADATA "
                      "WHERE TABLE_NAME = '%s' AND COLUMN_NAME = '%s'", 
                      pszTableName, pszGeomName );
    
    if( pszOwner != NULL )
    {
        oCommand.Appendf( 500, " AND OWNER = '%s'", pszOwner );
        CPLFree( pszOwner );
    }

/* -------------------------------------------------------------------- */
/*      Execute query command.                                          */
/* -------------------------------------------------------------------- */
    OGROCIStatement oGetTables( poDS->GetSession() );
    int nSRID = -1;
    
    if( oGetTables.Execute( oCommand.GetString() ) == CE_None )
    {
        char **papszRow = oGetTables.SimpleFetchRow();

        if( papszRow != NULL && papszRow[0] != NULL )
            nSRID = atoi( papszRow[0] );
    }

    return nSRID;
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGROCILayer::GetFIDColumn() 

{
    if( pszFIDName != NULL )
        return pszFIDName;
    else
        return "";
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGROCILayer::GetGeometryColumn() 

{
    if( pszGeomName != NULL )
        return pszGeomName;
    else
        return "";
}
