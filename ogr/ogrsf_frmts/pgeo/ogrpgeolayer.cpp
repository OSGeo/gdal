/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGeoLayer class, code shared between 
 *           the direct table access, and the generic SQL results.
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
 * Revision 1.6  2006/06/06 16:25:22  mloskot
 * Fixed memory 4-5 leaks in CPL ODBC and OGR drivers.
 *
 * Revision 1.5  2006/06/05 19:06:38  fwarmerdam
 * fixed problem with handling ofz for lines
 *
 * Revision 1.4  2005/09/29 19:49:48  fwarmerdam
 * dont try to translate GUID SRTEXT entries
 *
 * Revision 1.3  2005/09/21 00:55:42  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.2  2005/09/05 20:30:06  fwarmerdam
 * removed unused variable
 *
 * Revision 1.1  2005/09/05 19:34:17  fwarmerdam
 * New
 *
 */

#include "cpl_conv.h"
#include "ogr_pgeo.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRPGeoLayer()                            */
/************************************************************************/

OGRPGeoLayer::OGRPGeoLayer()

{
    poDS = NULL;

    pszGeomColumn = NULL;
    pszFIDColumn = NULL;

    poStmt = NULL;

    iNextShapeId = 0;

    poSRS = NULL;
    nSRSId = -2; // we haven't even queried the database for it yet. 
}

/************************************************************************/
/*                            ~OGRPGeoLayer()                             */
/************************************************************************/

OGRPGeoLayer::~OGRPGeoLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "PGeo", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( poStmt != NULL )
    {
        delete poStmt;
        poStmt = NULL;
    }

    if( poFeatureDefn != NULL )
    {
        poFeatureDefn->Release();
        poFeatureDefn = NULL;
    }

    CPLFree( pszGeomColumn );
    CPLFree( panFieldOrdinals ); 
    CPLFree( pszFIDColumn );

    if( poSRS != NULL )
    {
        poSRS->Release();
        poSRS = NULL;
    }
}

/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      Build feature definition from a set of column definitions       */
/*      set on a statement.  Sift out geometry and FID fields.          */
/************************************************************************/

CPLErr OGRPGeoLayer::BuildFeatureDefn( const char *pszLayerName, 
                                       CPLODBCStatement *poStmt )

{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    int    nRawColumns = poStmt->GetColCount();

    poFeatureDefn->Reference();

    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * nRawColumns );

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        OGRFieldDefn    oField( poStmt->GetColName(iCol), OFTString );

        oField.SetWidth( MAX(0,poStmt->GetColSize( iCol )) );

        if( pszGeomColumn != NULL 
            && EQUAL(poStmt->GetColName(iCol),pszGeomColumn) )
            continue;

        if( pszFIDColumn == NULL 
            && EQUAL(poStmt->GetColName(iCol),"OBJECTID") )
        {
            pszFIDColumn = CPLStrdup(poStmt->GetColName(iCol));
        }

        if( pszGeomColumn == NULL 
            && EQUAL(poStmt->GetColName(iCol),"Shape") )
        {
            pszGeomColumn = CPLStrdup(poStmt->GetColName(iCol));
            continue;
        }
        
        switch( poStmt->GetColType(iCol) )
        {
          case SQL_INTEGER:
            oField.SetType( OFTInteger );
            break;

          case SQL_BINARY:
          case SQL_VARBINARY:
          case SQL_LONGVARBINARY:
            oField.SetType( OFTBinary );
            break;

          case SQL_DECIMAL:
            oField.SetType( OFTReal );
            oField.SetPrecision( poStmt->GetColPrecision(iCol) );
            break;

          case SQL_FLOAT:
          case SQL_REAL:
          case SQL_DOUBLE:
            oField.SetType( OFTReal );
            oField.SetWidth( 0 );
            break;

          default:
            /* leave it as OFTString */;
        }

        poFeatureDefn->AddFieldDefn( &oField );
        panFieldOrdinals[poFeatureDefn->GetFieldCount() - 1] = iCol+1;
    }

    return CE_None;
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPGeoLayer::ResetReading()

{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGeoLayer::GetNextFeature()

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

OGRFeature *OGRPGeoLayer::GetNextRawFeature()

{
    if( GetStatement() == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    if( !poStmt->Fetch() )
    {
        delete poStmt;
        poStmt = NULL;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    int         iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    if( pszFIDColumn != NULL && poStmt->GetColId(pszFIDColumn) > -1 )
        poFeature->SetFID( 
            atoi(poStmt->GetColData(poStmt->GetColId(pszFIDColumn))) );
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;
    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Set the fields.                                                 */
/* -------------------------------------------------------------------- */
    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        int iSrcField = panFieldOrdinals[iField]-1;
        const char *pszValue = poStmt->GetColData( iSrcField );

        if( pszValue == NULL )
            /* no value */;
        else if( poFeature->GetFieldDefnRef(iField)->GetType() == OFTBinary )
            poFeature->SetField( iField, 
                                 poStmt->GetColDataLength(iSrcField),
                                 (GByte *) pszValue );
        else
            poFeature->SetField( iField, pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Try to extract a geometry.                                      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != NULL )
    {
        int iField = poStmt->GetColId( pszGeomColumn );
        GByte *pabyShape = (GByte *) poStmt->GetColData( iField );
        int nBytes = poStmt->GetColDataLength(iField);
        OGRGeometry *poGeom = NULL;

        if( pabyShape != NULL )
            createFromShapeBin( pabyShape, &poGeom, nBytes );

        if( poGeom != NULL )
        {
            poGeom->assignSpatialReference( poSRS );
            poFeature->SetGeometryDirectly( poGeom );
        }
    }

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPGeoLayer::GetFeature( long nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGeoLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCTransactions) )
        return FALSE;

    else 
        return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRPGeoLayer::GetSpatialRef()

{
    return poSRS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

void OGRPGeoLayer::LookupSRID( int nSRID )

{
/* -------------------------------------------------------------------- */
/*      Fetch the corresponding WKT from the SpatialRef table.          */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oStmt( poDS->GetSession() );
        
    oStmt.Appendf( "SELECT srtext FROM GDB_SpatialRefs WHERE srid = %d",
                  nSRID );

    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "'%s' failed.\n%s", 
                  oStmt.GetCommand(),
                  poDS->GetSession()->GetLastError() );
        return;
    }

    if( !oStmt.Fetch() )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "SRID %d lookup failed.\n%s", 
                  nSRID, poDS->GetSession()->GetLastError() );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Check that it isn't just a GUID.  We don't know how to          */
/*      translate those.                                                */
/* -------------------------------------------------------------------- */
    char *pszSRText = (char *) oStmt.GetColData(0);

    if( pszSRText[0] == '{' )
    {
        CPLDebug( "PGEO", "Ignoreing GUID SRTEXT: %s", pszSRText );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Turn it into an OGRSpatialReference.                            */
/* -------------------------------------------------------------------- */
    poSRS = new OGRSpatialReference();
    
    if( poSRS->importFromWkt( &pszSRText ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "importFromWKT() failed on SRS '%s'.",
                  pszSRText);
        delete poSRS;
        poSRS = NULL;
    }
    else if( poSRS->morphFromESRI() != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "morphFromESRI() failed on SRS." );
        delete poSRS;
        poSRS = NULL;
    }
    else
        nSRSId = nSRID;
}

/************************************************************************/
/*                         createFromShapeBin()                         */
/*                                                                      */
/*      Translate shapefile binary representation to an OGR             */
/*      geometry.                                                       */
/************************************************************************/

OGRErr OGRPGeoLayer::createFromShapeBin( GByte *pabyShape, 
                                         OGRGeometry **ppoGeom,
                                         int nBytes )

{
    *ppoGeom = NULL;

    if( nBytes < 1 )
        return OGRERR_FAILURE;

//    printf( "%s\n", CPLBinaryToHex( nBytes, pabyShape ) );

    int nSHPType = pabyShape[0];

/* ==================================================================== */
/*  Extract vertices for a Polygon or Arc.				*/
/* ==================================================================== */
    if( nSHPType == SHPT_POLYGON 
        || nSHPType == SHPT_ARC
        || nSHPType == SHPT_POLYGONZ
        || nSHPType == SHPT_POLYGONM
        || nSHPType == SHPT_ARCZ
        || nSHPType == SHPT_ARCM
        || nSHPType == SHPT_MULTIPATCH )
    {
	GInt32		nPoints, nParts;
	int    		i, nOffset;
        GInt32         *panPartStart;

/* -------------------------------------------------------------------- */
/*      Extract part/point count, and build vertex and part arrays      */
/*      to proper size.                                                 */
/* -------------------------------------------------------------------- */
	memcpy( &nPoints, pabyShape + 40, 4 );
	memcpy( &nParts, pabyShape + 36, 4 );

	CPL_LSBPTR32( &nPoints );
	CPL_LSBPTR32( &nParts );

        panPartStart = (GInt32 *) CPLCalloc(nParts,sizeof(GInt32));

/* -------------------------------------------------------------------- */
/*      Copy out the part array from the record.                        */
/* -------------------------------------------------------------------- */
	memcpy( panPartStart, pabyShape + 44, 4 * nParts );
	for( i = 0; i < nParts; i++ )
	{
            CPL_LSBPTR32( panPartStart + i );
	}

	nOffset = 44 + 4*nParts;

/* -------------------------------------------------------------------- */
/*      If this is a multipatch, we will also have parts types.  For    */
/*      now we ignore and skip past them.                               */
/* -------------------------------------------------------------------- */
        if( nSHPType == SHPT_MULTIPATCH )
            nOffset += 4*nParts;
        
/* -------------------------------------------------------------------- */
/*      Copy out the vertices from the record.                          */
/* -------------------------------------------------------------------- */
        double *padfX = (double *) CPLMalloc(sizeof(double)*nPoints);
        double *padfY = (double *) CPLMalloc(sizeof(double)*nPoints);
        double *padfZ = (double *) CPLCalloc(sizeof(double),nPoints);

	for( i = 0; i < nPoints; i++ )
	{
	    memcpy(padfX + i, pabyShape + nOffset + i * 16, 8 );
	    memcpy(padfY + i, pabyShape + nOffset + i * 16 + 8, 8 );
            CPL_LSBPTR64( padfX + i );
            CPL_LSBPTR64( padfY + i );
	}

        nOffset += 16*nPoints;
        
/* -------------------------------------------------------------------- */
/*      If we have a Z coordinate, collect that now.                    */
/* -------------------------------------------------------------------- */
        if( nSHPType == SHPT_POLYGONZ
            || nSHPType == SHPT_ARCZ
            || nSHPType == SHPT_MULTIPATCH )
        {
            for( i = 0; i < nPoints; i++ )
            {
                memcpy( padfZ + i, pabyShape + nOffset + 16 + i*8, 8 );
                CPL_LSBPTR64( padfZ + i );
            }

            nOffset += 16 + 8*nPoints;
        }

/* -------------------------------------------------------------------- */
/*      Build corresponding OGR objects.                                */
/* -------------------------------------------------------------------- */
        if( nSHPType == SHPT_ARC 
            || nSHPType == SHPT_ARCZ
            || nSHPType == SHPT_ARCM )
        {
/* -------------------------------------------------------------------- */
/*      Arc - As LineString                                             */
/* -------------------------------------------------------------------- */
            if( nParts == 1 )
            {
                OGRLineString *poLine = new OGRLineString();
                *ppoGeom = poLine;

                poLine->setPoints( nPoints, padfX, padfY, padfZ );
            }

/* -------------------------------------------------------------------- */
/*      Arc - As MultiLineString                                        */
/* -------------------------------------------------------------------- */
            else
            {
                OGRMultiLineString *poMulti = new OGRMultiLineString;
                *ppoGeom = poMulti;

                for( i = 0; i < nParts; i++ )
                {
                    OGRLineString *poLine = new OGRLineString;
                    int nVerticesInThisPart;

                    if( i == nParts-1 )
                        nVerticesInThisPart = nPoints - panPartStart[i];
                    else
                        nVerticesInThisPart = 
                            panPartStart[i+1] - panPartStart[i];

                    poLine->setPoints( nVerticesInThisPart, 
                                       padfX + panPartStart[i], 
                                       padfY + panPartStart[i], 
                                       padfZ + panPartStart[i] );

                    poMulti->addGeometryDirectly( poLine );
                }
            }
        } /* ARC */

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
        else if( nSHPType == SHPT_POLYGON
                 || nSHPType == SHPT_POLYGONZ
                 || nSHPType == SHPT_POLYGONM )
        {
            OGRPolygon *poMulti = new OGRPolygon;
            *ppoGeom = poMulti;

            for( i = 0; i < nParts; i++ )
            {
                OGRLinearRing *poRing = new OGRLinearRing;
                int nVerticesInThisPart;

                if( i == nParts-1 )
                    nVerticesInThisPart = nPoints - panPartStart[i];
                else
                    nVerticesInThisPart = 
                        panPartStart[i+1] - panPartStart[i];

                poRing->setPoints( nVerticesInThisPart, 
                                   padfX + panPartStart[i], 
                                   padfY + panPartStart[i], 
                                   padfZ + panPartStart[i] );

                poMulti->addRingDirectly( poRing );
            }
        } /* polygon */

/* -------------------------------------------------------------------- */
/*      Multipatch                                                      */
/* -------------------------------------------------------------------- */
        else if( nSHPType == SHPT_MULTIPATCH )
        {
            /* return to this later */
        } 

        CPLFree( panPartStart );
        CPLFree( padfX );
        CPLFree( padfY );
        CPLFree( padfZ );

        if( nSHPType == SHPT_ARC
            || nSHPType == SHPT_POLYGON )
            (*ppoGeom)->setCoordinateDimension( 2 );

        return OGRERR_NONE;
    }

/* ==================================================================== */
/*  Extract vertices for a MultiPoint.					*/
/* ==================================================================== */
    else if( nSHPType == SHPT_MULTIPOINT
             || nSHPType == SHPT_MULTIPOINTM
             || nSHPType == SHPT_MULTIPOINTZ )
    {
#ifdef notdef
	int32		nPoints;
	int    		i, nOffset;

	memcpy( &nPoints, psSHP->pabyRec + 44, 4 );
	if( bBigEndian ) SwapWord( 4, &nPoints );

	psShape->nVertices = nPoints;
        psShape->padfX = (double *) calloc(nPoints,sizeof(double));
        psShape->padfY = (double *) calloc(nPoints,sizeof(double));
        psShape->padfZ = (double *) calloc(nPoints,sizeof(double));
        psShape->padfM = (double *) calloc(nPoints,sizeof(double));

	for( i = 0; i < nPoints; i++ )
	{
	    memcpy(psShape->padfX+i, psSHP->pabyRec + 48 + 16 * i, 8 );
	    memcpy(psShape->padfY+i, psSHP->pabyRec + 48 + 16 * i + 8, 8 );

	    if( bBigEndian ) SwapWord( 8, psShape->padfX + i );
	    if( bBigEndian ) SwapWord( 8, psShape->padfY + i );
	}

        nOffset = 48 + 16*nPoints;
        
/* -------------------------------------------------------------------- */
/*	Get the X/Y bounds.						*/
/* -------------------------------------------------------------------- */
        memcpy( &(psShape->dfXMin), psSHP->pabyRec + 8 +  4, 8 );
        memcpy( &(psShape->dfYMin), psSHP->pabyRec + 8 + 12, 8 );
        memcpy( &(psShape->dfXMax), psSHP->pabyRec + 8 + 20, 8 );
        memcpy( &(psShape->dfYMax), psSHP->pabyRec + 8 + 28, 8 );

	if( bBigEndian ) SwapWord( 8, &(psShape->dfXMin) );
	if( bBigEndian ) SwapWord( 8, &(psShape->dfYMin) );
	if( bBigEndian ) SwapWord( 8, &(psShape->dfXMax) );
	if( bBigEndian ) SwapWord( 8, &(psShape->dfYMax) );

/* -------------------------------------------------------------------- */
/*      If we have a Z coordinate, collect that now.                    */
/* -------------------------------------------------------------------- */
        if( psShape->nSHPType == SHPT_MULTIPOINTZ )
        {
            memcpy( &(psShape->dfZMin), psSHP->pabyRec + nOffset, 8 );
            memcpy( &(psShape->dfZMax), psSHP->pabyRec + nOffset + 8, 8 );
            
            if( bBigEndian ) SwapWord( 8, &(psShape->dfZMin) );
            if( bBigEndian ) SwapWord( 8, &(psShape->dfZMax) );
            
            for( i = 0; i < nPoints; i++ )
            {
                memcpy( psShape->padfZ + i,
                        psSHP->pabyRec + nOffset + 16 + i*8, 8 );
                if( bBigEndian ) SwapWord( 8, psShape->padfZ + i );
            }

            nOffset += 16 + 8*nPoints;
        }

/* -------------------------------------------------------------------- */
/*      If we have a M measure value, then read it now.  We assume      */
/*      that the measure can be present for any shape if the size is    */
/*      big enough, but really it will only occur for the Z shapes      */
/*      (options), and the M shapes.                                    */
/* -------------------------------------------------------------------- */
        if( psSHP->panRecSize[hEntity]+8 >= nOffset + 16 + 8*nPoints )
        {
            memcpy( &(psShape->dfMMin), psSHP->pabyRec + nOffset, 8 );
            memcpy( &(psShape->dfMMax), psSHP->pabyRec + nOffset + 8, 8 );
            
            if( bBigEndian ) SwapWord( 8, &(psShape->dfMMin) );
            if( bBigEndian ) SwapWord( 8, &(psShape->dfMMax) );
            
            for( i = 0; i < nPoints; i++ )
            {
                memcpy( psShape->padfM + i,
                        psSHP->pabyRec + nOffset + 16 + i*8, 8 );
                if( bBigEndian ) SwapWord( 8, psShape->padfM + i );
            }
        }
#endif
    }

/* ==================================================================== */
/*      Extract vertices for a point.                                   */
/* ==================================================================== */
    else if( nSHPType == SHPT_POINT
             || nSHPType == SHPT_POINTM
             || nSHPType == SHPT_POINTZ )
    {
        int	nOffset;
        double  dfX, dfY, dfZ = 0;
        
	memcpy( &dfX, pabyShape + 4, 8 );
	memcpy( &dfY, pabyShape + 4 + 8, 8 );

        CPL_LSBPTR64( &dfX );
        CPL_LSBPTR64( &dfY );
        nOffset = 20 + 8;
        
        if( nSHPType == SHPT_POINTZ )
        {
            memcpy( &dfZ, pabyShape + 4 + 16, 8 );
            CPL_LSBPTR64( &dfZ );
        }

        *ppoGeom = new OGRPoint( dfX, dfY, dfZ );

        if( nSHPType != SHPT_POINTZ )
            (*ppoGeom)->setCoordinateDimension( 2 );

        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}
