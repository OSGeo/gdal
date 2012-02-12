/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the OGRSFLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "ogr_attrind.h"
#include "swq.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                              OGRLayer()                              */
/************************************************************************/

OGRLayer::OGRLayer()

{
    m_poStyleTable = NULL;
    m_poAttrQuery = NULL;
    m_poAttrIndex = NULL;
    m_nRefCount = 0;

    m_nFeaturesRead = 0;

    m_poFilterGeom = NULL;
    m_bFilterIsEnvelope = FALSE;
}

/************************************************************************/
/*                             ~OGRLayer()                              */
/************************************************************************/

OGRLayer::~OGRLayer()

{
    if ( m_poStyleTable )
    {
        delete m_poStyleTable;
        m_poStyleTable = NULL;
    }

    if( m_poAttrIndex != NULL )
    {
        delete m_poAttrIndex;
        m_poAttrIndex = NULL;
    }

    if( m_poAttrQuery != NULL )
    {
        delete m_poAttrQuery;
        m_poAttrQuery = NULL;
    }

    if( m_poFilterGeom )
    {
        delete m_poFilterGeom;
        m_poFilterGeom = NULL;
    }
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

int OGRLayer::Reference()

{
    return ++m_nRefCount;
}

/************************************************************************/
/*                          OGR_L_Reference()                           */
/************************************************************************/

int OGR_L_Reference( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_Reference", 0 );

    return ((OGRLayer *) hLayer)->Reference();
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

int OGRLayer::Dereference()

{
    return --m_nRefCount;
}

/************************************************************************/
/*                         OGR_L_Dereference()                          */
/************************************************************************/

int OGR_L_Dereference( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_Dereference", 0 );

    return ((OGRLayer *) hLayer)->Dereference();
}

/************************************************************************/
/*                            GetRefCount()                             */
/************************************************************************/

int OGRLayer::GetRefCount() const

{
    return m_nRefCount;
}

/************************************************************************/
/*                         OGR_L_GetRefCount()                          */
/************************************************************************/

int OGR_L_GetRefCount( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetRefCount", 0 );

    return ((OGRLayer *) hLayer)->GetRefCount();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRLayer::GetFeatureCount( int bForce )

{
    OGRFeature     *poFeature;
    int            nFeatureCount = 0;

    if( !bForce )
        return -1;

    ResetReading();
    while( (poFeature = GetNextFeature()) != NULL )
    {
        nFeatureCount++;
        delete poFeature;
    }
    ResetReading();

    return nFeatureCount;
}

/************************************************************************/
/*                       OGR_L_GetFeatureCount()                        */
/************************************************************************/

int OGR_L_GetFeatureCount( OGRLayerH hLayer, int bForce )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetFeature", 0 );

    return ((OGRLayer *) hLayer)->GetFeatureCount(bForce);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
    OGRFeature  *poFeature;
    OGREnvelope oEnv;
    GBool       bExtentSet = FALSE;

    psExtent->MinX = 0.0;
    psExtent->MaxX = 0.0;
    psExtent->MinY = 0.0;
    psExtent->MaxY = 0.0;

/* -------------------------------------------------------------------- */
/*      If this layer has a none geometry type, then we can             */
/*      reasonably assume there are not extents available.              */
/* -------------------------------------------------------------------- */
    if( GetLayerDefn()->GetGeomType() == wkbNone )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      If not forced, we should avoid having to scan all the           */
/*      features and just return a failure.                             */
/* -------------------------------------------------------------------- */
    if( !bForce )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      OK, we hate to do this, but go ahead and read through all       */
/*      the features to collect geometries and build extents.           */
/* -------------------------------------------------------------------- */
    ResetReading();
    while( (poFeature = GetNextFeature()) != NULL )
    {
        OGRGeometry *poGeom = poFeature->GetGeometryRef();
        if (poGeom == NULL || poGeom->IsEmpty())
        {
            /* Do nothing */
        }
        else if (!bExtentSet)
        {
            poGeom->getEnvelope(psExtent);
            bExtentSet = TRUE;
        }
        else
        {
            poGeom->getEnvelope(&oEnv);
            if (oEnv.MinX < psExtent->MinX) 
                psExtent->MinX = oEnv.MinX;
            if (oEnv.MinY < psExtent->MinY) 
                psExtent->MinY = oEnv.MinY;
            if (oEnv.MaxX > psExtent->MaxX) 
                psExtent->MaxX = oEnv.MaxX;
            if (oEnv.MaxY > psExtent->MaxY) 
                psExtent->MaxY = oEnv.MaxY;
        }
        delete poFeature;
    }
    ResetReading();

    return (bExtentSet ? OGRERR_NONE : OGRERR_FAILURE);
}

/************************************************************************/
/*                          OGR_L_GetExtent()                           */
/************************************************************************/

OGRErr OGR_L_GetExtent( OGRLayerH hLayer, OGREnvelope *psExtent, int bForce )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetExtent", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hLayer)->GetExtent( psExtent, bForce );
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRLayer::SetAttributeFilter( const char *pszQuery )

{
/* -------------------------------------------------------------------- */
/*      Are we just clearing any existing query?                        */
/* -------------------------------------------------------------------- */
    if( pszQuery == NULL || strlen(pszQuery) == 0 )
    {
        if( m_poAttrQuery )
        {
            delete m_poAttrQuery;
            m_poAttrQuery = NULL;
            ResetReading();
        }
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Or are we installing a new query?                               */
/* -------------------------------------------------------------------- */
    OGRErr      eErr;

    if( !m_poAttrQuery )
        m_poAttrQuery = new OGRFeatureQuery();

    eErr = m_poAttrQuery->Compile( GetLayerDefn(), pszQuery );
    if( eErr != OGRERR_NONE )
    {
        delete m_poAttrQuery;
        m_poAttrQuery = NULL;
    }

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                        ContainGeomSpecialField()                     */
/************************************************************************/

static int ContainGeomSpecialField(swq_expr_node* expr,
                                   int nLayerFieldCount)
{
    if (expr->eNodeType == SNT_COLUMN)
    {
        if( expr->table_index == 0 && expr->field_index != -1 )
        {
            int nSpecialFieldIdx = expr->field_index -
                                    nLayerFieldCount;
            return nSpecialFieldIdx == SPF_OGR_GEOMETRY ||
                   nSpecialFieldIdx == SPF_OGR_GEOM_WKT ||
                   nSpecialFieldIdx == SPF_OGR_GEOM_AREA;
        }
    }
    else if (expr->eNodeType == SNT_OPERATION)
    {
        for( int i = 0; i < expr->nSubExprCount; i++ )
        {
            if (ContainGeomSpecialField(expr->papoSubExpr[i],
                                        nLayerFieldCount))
                return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                AttributeFilterEvaluationNeedsGeometry()              */
/************************************************************************/

int OGRLayer::AttributeFilterEvaluationNeedsGeometry()
{
    if( !m_poAttrQuery )
        return FALSE;

    swq_expr_node* expr = (swq_expr_node *) m_poAttrQuery->GetSWGExpr();
    int nLayerFieldCount = GetLayerDefn()->GetFieldCount();

    return ContainGeomSpecialField(expr, nLayerFieldCount);
}

/************************************************************************/
/*                      OGR_L_SetAttributeFilter()                      */
/************************************************************************/

OGRErr OGR_L_SetAttributeFilter( OGRLayerH hLayer, const char *pszQuery )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_SetAttributeFilter", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hLayer)->SetAttributeFilter( pszQuery );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRLayer::GetFeature( long nFID )

{
    OGRFeature *poFeature;

    ResetReading();
    while( (poFeature = GetNextFeature()) != NULL )
    {
        if( poFeature->GetFID() == nFID )
            return poFeature;
        else
            delete poFeature;
    }
    
    return NULL;
}

/************************************************************************/
/*                          OGR_L_GetFeature()                          */
/************************************************************************/

OGRFeatureH OGR_L_GetFeature( OGRLayerH hLayer, long nFeatureId )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetFeature", NULL );

    return (OGRFeatureH) ((OGRLayer *)hLayer)->GetFeature( nFeatureId );
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRLayer::SetNextByIndex( long nIndex )

{
    OGRFeature *poFeature;

    ResetReading();
    while( nIndex-- > 0 )
    {
        poFeature = GetNextFeature();
        if( poFeature == NULL )
            return OGRERR_FAILURE;

        delete poFeature;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGR_L_SetNextByIndex()                        */
/************************************************************************/

OGRErr OGR_L_SetNextByIndex( OGRLayerH hLayer, long nIndex )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_SetNextByIndex", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)hLayer)->SetNextByIndex( nIndex );
}

/************************************************************************/
/*                        OGR_L_GetNextFeature()                        */
/************************************************************************/

OGRFeatureH OGR_L_GetNextFeature( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetNextFeature", NULL );

    return (OGRFeatureH) ((OGRLayer *)hLayer)->GetNextFeature();
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRLayer::SetFeature( OGRFeature * )

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                          OGR_L_SetFeature()                          */
/************************************************************************/

OGRErr OGR_L_SetFeature( OGRLayerH hLayer, OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_SetFeature", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( hFeat, "OGR_L_SetFeature", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)hLayer)->SetFeature( (OGRFeature *) hFeat );
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRLayer::CreateFeature( OGRFeature * )

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_CreateFeature()                         */
/************************************************************************/

OGRErr OGR_L_CreateFeature( OGRLayerH hLayer, OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_CreateFeature", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( hFeat, "OGR_L_CreateFeature", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hLayer)->CreateFeature( (OGRFeature *) hFeat );
}

/************************************************************************/
/*                              GetInfo()                               */
/************************************************************************/

const char *OGRLayer::GetInfo( const char * pszTag )

{
    (void) pszTag;
    return NULL;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRLayer::CreateField( OGRFieldDefn * poField, int bApproxOK )

{
    (void) poField;
    (void) bApproxOK;

    CPLError( CE_Failure, CPLE_NotSupported,
              "CreateField() not supported by this layer.\n" );
              
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                         OGR_L_CreateField()                          */
/************************************************************************/

OGRErr OGR_L_CreateField( OGRLayerH hLayer, OGRFieldDefnH hField, 
                          int bApproxOK )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_CreateField", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( hField, "OGR_L_CreateField", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hLayer)->CreateField( (OGRFieldDefn *) hField, 
                                               bApproxOK );
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRLayer::DeleteField( int iField )

{
    (void) iField;

    CPLError( CE_Failure, CPLE_NotSupported,
              "DeleteField() not supported by this layer.\n" );

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                         OGR_L_DeleteField()                          */
/************************************************************************/

OGRErr OGR_L_DeleteField( OGRLayerH hLayer, int iField )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_DeleteField", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hLayer)->DeleteField( iField );
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRLayer::ReorderFields( int* panMap )

{
    (void) panMap;

    CPLError( CE_Failure, CPLE_NotSupported,
              "ReorderFields() not supported by this layer.\n" );

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                       OGR_L_ReorderFields()                          */
/************************************************************************/

OGRErr OGR_L_ReorderFields( OGRLayerH hLayer, int* panMap )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_ReorderFields", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hLayer)->ReorderFields( panMap );
}

/************************************************************************/
/*                            ReorderField()                            */
/************************************************************************/

OGRErr OGRLayer::ReorderField( int iOldFieldPos, int iNewFieldPos )

{
    OGRErr eErr;

    int nFieldCount = GetLayerDefn()->GetFieldCount();

    if (iOldFieldPos < 0 || iOldFieldPos >= nFieldCount)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }
    if (iNewFieldPos < 0 || iNewFieldPos >= nFieldCount)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }
    if (iNewFieldPos == iOldFieldPos)
        return OGRERR_NONE;

    int* panMap = (int*) CPLMalloc(sizeof(int) * nFieldCount);
    int i;
    if (iOldFieldPos < iNewFieldPos)
    {
        /* "0","1","2","3","4" (1,3) -> "0","2","3","1","4" */
        for(i=0;i<iOldFieldPos;i++)
            panMap[i] = i;
        for(;i<iNewFieldPos;i++)
            panMap[i] = i + 1;
        panMap[iNewFieldPos] = iOldFieldPos;
        for(i=iNewFieldPos+1;i<nFieldCount;i++)
            panMap[i] = i;
    }
    else
    {
        /* "0","1","2","3","4" (3,1) -> "0","3","1","2","4" */
        for(i=0;i<iNewFieldPos;i++)
            panMap[i] = i;
        panMap[iNewFieldPos] = iOldFieldPos;
        for(i=iNewFieldPos+1;i<=iOldFieldPos;i++)
            panMap[i] = i - 1;
        for(;i<nFieldCount;i++)
            panMap[i] = i;
    }

    eErr = ReorderFields(panMap);

    CPLFree(panMap);

    return eErr;
}

/************************************************************************/
/*                        OGR_L_ReorderField()                          */
/************************************************************************/

OGRErr OGR_L_ReorderField( OGRLayerH hLayer, int iOldFieldPos, int iNewFieldPos )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_ReorderField", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hLayer)->ReorderField( iOldFieldPos, iNewFieldPos );
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn,
                                 int nFlags )

{
    (void) iField;
    (void) poNewFieldDefn;
    (void) nFlags;

    CPLError( CE_Failure, CPLE_NotSupported,
              "AlterFieldDefn() not supported by this layer.\n" );

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_AlterFieldDefn()                        */
/************************************************************************/

OGRErr OGR_L_AlterFieldDefn( OGRLayerH hLayer, int iField, OGRFieldDefnH hNewFieldDefn,
                             int nFlags )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_AlterFieldDefn", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( hNewFieldDefn, "OGR_L_AlterFieldDefn", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hLayer)->AlterFieldDefn( iField, (OGRFieldDefn*) hNewFieldDefn, nFlags );
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr OGRLayer::StartTransaction()

{
    return OGRERR_NONE;
}

/************************************************************************/
/*                       OGR_L_StartTransaction()                       */
/************************************************************************/

OGRErr OGR_L_StartTransaction( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_StartTransaction", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)hLayer)->StartTransaction();
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRLayer::CommitTransaction()

{
    return OGRERR_NONE;
}

/************************************************************************/
/*                       OGR_L_CommitTransaction()                      */
/************************************************************************/

OGRErr OGR_L_CommitTransaction( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_CommitTransaction", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)hLayer)->CommitTransaction();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRLayer::RollbackTransaction()

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                     OGR_L_RollbackTransaction()                      */
/************************************************************************/

OGRErr OGR_L_RollbackTransaction( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_RollbackTransaction", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)hLayer)->RollbackTransaction();
}

/************************************************************************/
/*                         OGR_L_GetLayerDefn()                         */
/************************************************************************/

OGRFeatureDefnH OGR_L_GetLayerDefn( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetLayerDefn", NULL );

    return (OGRFeatureDefnH) ((OGRLayer *)hLayer)->GetLayerDefn();
}

/************************************************************************/
/*                        OGR_L_GetSpatialRef()                         */
/************************************************************************/

OGRSpatialReferenceH OGR_L_GetSpatialRef( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetSpatialRef", NULL );

    return (OGRSpatialReferenceH) ((OGRLayer *) hLayer)->GetSpatialRef();
}

/************************************************************************/
/*                        OGR_L_TestCapability()                        */
/************************************************************************/

int OGR_L_TestCapability( OGRLayerH hLayer, const char *pszCap )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_TestCapability", 0 );
    VALIDATE_POINTER1( pszCap, "OGR_L_TestCapability", 0 );

    return ((OGRLayer *) hLayer)->TestCapability( pszCap );
}

/************************************************************************/
/*                          GetSpatialFilter()                          */
/************************************************************************/

OGRGeometry *OGRLayer::GetSpatialFilter()

{
    return m_poFilterGeom;
}

/************************************************************************/
/*                       OGR_L_GetSpatialFilter()                       */
/************************************************************************/

OGRGeometryH OGR_L_GetSpatialFilter( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetSpatialFilter", NULL );

    return (OGRGeometryH) ((OGRLayer *) hLayer)->GetSpatialFilter();
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( InstallFilter( poGeomIn ) )
        ResetReading();
}

/************************************************************************/
/*                       OGR_L_SetSpatialFilter()                       */
/************************************************************************/

void OGR_L_SetSpatialFilter( OGRLayerH hLayer, OGRGeometryH hGeom )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetSpatialFilter" );

    ((OGRLayer *) hLayer)->SetSpatialFilter( (OGRGeometry *) hGeom );
}

/************************************************************************/
/*                        SetSpatialFilterRect()                        */
/************************************************************************/

void OGRLayer::SetSpatialFilterRect( double dfMinX, double dfMinY, 
                                     double dfMaxX, double dfMaxY )

{
    OGRLinearRing  oRing;
    OGRPolygon oPoly;

    oRing.addPoint( dfMinX, dfMinY );
    oRing.addPoint( dfMinX, dfMaxY );
    oRing.addPoint( dfMaxX, dfMaxY );
    oRing.addPoint( dfMaxX, dfMinY );
    oRing.addPoint( dfMinX, dfMinY );

    oPoly.addRing( &oRing );

    SetSpatialFilter( &oPoly );
}

/************************************************************************/
/*                     OGR_L_SetSpatialFilterRect()                     */
/************************************************************************/

void OGR_L_SetSpatialFilterRect( OGRLayerH hLayer, 
                                 double dfMinX, double dfMinY, 
                                 double dfMaxX, double dfMaxY )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetSpatialFilterRect" );

    ((OGRLayer *) hLayer)->SetSpatialFilterRect( dfMinX, dfMinY, 
                                                 dfMaxX, dfMaxY );
}

/************************************************************************/
/*                           InstallFilter()                            */
/*                                                                      */
/*      This method is only intended to be used from within             */
/*      drivers, normally from the SetSpatialFilter() method.           */
/*      It installs a filter, and also tests it to see if it is         */
/*      rectangular.  If so, it this is kept track of alongside the     */
/*      filter geometry itself so we can do cheaper comparisons in      */
/*      the FilterGeometry() call.                                      */
/*                                                                      */
/*      Returns TRUE if the newly installed filter differs in some      */
/*      way from the current one.                                       */
/************************************************************************/

int OGRLayer::InstallFilter( OGRGeometry * poFilter )

{
    if( m_poFilterGeom == NULL && poFilter == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Replace the existing filter.                                    */
/* -------------------------------------------------------------------- */
    if( m_poFilterGeom != NULL )
    {
        delete m_poFilterGeom;
        m_poFilterGeom = NULL;
    }

    if( poFilter != NULL )
        m_poFilterGeom = poFilter->clone();

    m_bFilterIsEnvelope = FALSE;

    if( m_poFilterGeom == NULL )
        return TRUE;

    if( m_poFilterGeom != NULL )
        m_poFilterGeom->getEnvelope( &m_sFilterEnvelope );

/* -------------------------------------------------------------------- */
/*      Now try to determine if the filter is really a rectangle.       */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(m_poFilterGeom->getGeometryType()) != wkbPolygon )
        return TRUE;

    OGRPolygon *poPoly = (OGRPolygon *) m_poFilterGeom;

    if( poPoly->getNumInteriorRings() != 0 )
        return TRUE;

    OGRLinearRing *poRing = poPoly->getExteriorRing();
    if (poRing == NULL)
        return TRUE;

    if( poRing->getNumPoints() > 5 || poRing->getNumPoints() < 4 )
        return TRUE;

    // If the ring has 5 points, the last should be the first. 
    if( poRing->getNumPoints() == 5 
        && ( poRing->getX(0) != poRing->getX(4)
             || poRing->getY(0) != poRing->getY(4) ) )
        return TRUE;

    // Polygon with first segment in "y" direction. 
    if( poRing->getX(0) == poRing->getX(1)
        && poRing->getY(1) == poRing->getY(2)
        && poRing->getX(2) == poRing->getX(3)
        && poRing->getY(3) == poRing->getY(0) )
        m_bFilterIsEnvelope = TRUE;

    // Polygon with first segment in "x" direction. 
    if( poRing->getY(0) == poRing->getY(1)
        && poRing->getX(1) == poRing->getX(2)
        && poRing->getY(2) == poRing->getY(3)
        && poRing->getX(3) == poRing->getX(0) )
        m_bFilterIsEnvelope = TRUE;

    return TRUE;
}

/************************************************************************/
/*                           FilterGeometry()                           */
/*                                                                      */
/*      Compare the passed in geometry to the currently installed       */
/*      filter.  Optimize for case where filter is just an              */
/*      envelope.                                                       */
/************************************************************************/

int OGRLayer::FilterGeometry( OGRGeometry *poGeometry )

{
/* -------------------------------------------------------------------- */
/*      In trivial cases of new filter or target geometry, we accept    */
/*      an intersection.  No geometry is taken to mean "the whole       */
/*      world".                                                         */
/* -------------------------------------------------------------------- */
    if( m_poFilterGeom == NULL )
        return TRUE;

    if( poGeometry == NULL )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Compute the target geometry envelope, and if there is no        */
/*      intersection between the envelopes we are sure not to have      */
/*      any intersection.                                               */
/* -------------------------------------------------------------------- */
    OGREnvelope sGeomEnv;

    poGeometry->getEnvelope( &sGeomEnv );

    if( sGeomEnv.MaxX < m_sFilterEnvelope.MinX
        || sGeomEnv.MaxY < m_sFilterEnvelope.MinY
        || m_sFilterEnvelope.MaxX < sGeomEnv.MinX
        || m_sFilterEnvelope.MaxY < sGeomEnv.MinY )
        return FALSE;


/* -------------------------------------------------------------------- */
/*      If the filter geometry is its own envelope and if the           */
/*      envelope of the geometry is inside the filter geometry,         */
/*      the geometry itself is inside the filter geometry               */
/* -------------------------------------------------------------------- */
    if( m_bFilterIsEnvelope &&
        sGeomEnv.MinX >= m_sFilterEnvelope.MinX &&
        sGeomEnv.MinY >= m_sFilterEnvelope.MinY &&
        sGeomEnv.MaxX <= m_sFilterEnvelope.MaxX &&
        sGeomEnv.MaxY <= m_sFilterEnvelope.MaxY)
    {
        return TRUE;
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      If the filter geometry is its own envelope and if the           */
/*      the geometry (line, or polygon without hole) h has at least one */
/*      point inside the filter geometry, the geometry itself is inside */
/*      the filter geometry.                                            */
/* -------------------------------------------------------------------- */
        if( m_bFilterIsEnvelope )
        {
            OGRLineString* poLS = NULL;

            switch( wkbFlatten(poGeometry->getGeometryType()) )
            {
                case wkbPolygon:
                {
                    OGRPolygon* poPoly = (OGRPolygon* )poGeometry;
                    OGRLinearRing* poRing = poPoly->getExteriorRing();
                    if (poRing != NULL && poPoly->getNumInteriorRings() == 0)
                    {
                        poLS = poRing;
                    }
                    break;
                }

                case wkbLineString:
                    poLS = (OGRLineString* )poGeometry;
                    break;

                default:
                    break;
            }

            if( poLS != NULL )
            {
                int nNumPoints = poLS->getNumPoints();
                for(int i = 0; i < nNumPoints; i++)
                {
                    double x = poLS->getX(i);
                    double y = poLS->getY(i);
                    if (x >= m_sFilterEnvelope.MinX &&
                        y >= m_sFilterEnvelope.MinY &&
                        x <= m_sFilterEnvelope.MaxX &&
                        y <= m_sFilterEnvelope.MaxY)
                    {
                        return TRUE;
                    }
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Fallback to full intersect test (using GEOS) if we still        */
/*      don't know for sure.                                            */
/* -------------------------------------------------------------------- */
        if( OGRGeometryFactory::haveGEOS() )
        {
            //CPLDebug("OGRLayer", "GEOS intersection");
            return m_poFilterGeom->Intersects( poGeometry );
        }
        else
            return TRUE;
    }
}

/************************************************************************/
/*                         OGR_L_ResetReading()                         */
/************************************************************************/

void OGR_L_ResetReading( OGRLayerH hLayer )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_ResetReading" );

    ((OGRLayer *) hLayer)->ResetReading();
}

/************************************************************************/
/*                       InitializeIndexSupport()                       */
/*                                                                      */
/*      This is only intended to be called by driver layer              */
/*      implementations but we don't make it protected so that the      */
/*      datasources can do it too if that is more appropriate.          */
/************************************************************************/

OGRErr OGRLayer::InitializeIndexSupport( const char *pszFilename )

{
    OGRErr eErr;

    if (m_poAttrIndex != NULL)
        return OGRERR_NONE;

    m_poAttrIndex = OGRCreateDefaultLayerIndex();

    eErr = m_poAttrIndex->Initialize( pszFilename, this );
    if( eErr != OGRERR_NONE )
    {
        delete m_poAttrIndex;
        m_poAttrIndex = NULL;
    }

    return eErr;
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRLayer::SyncToDisk()

{
    return OGRERR_NONE;
}

/************************************************************************/
/*                          OGR_L_SyncToDisk()                          */
/************************************************************************/

OGRErr OGR_L_SyncToDisk( OGRLayerH hDS )

{
    VALIDATE_POINTER1( hDS, "OGR_L_SyncToDisk", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hDS)->SyncToDisk();
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRLayer::DeleteFeature( long nFID )

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_DeleteFeature()                         */
/************************************************************************/

OGRErr OGR_L_DeleteFeature( OGRLayerH hDS, long nFID )

{
    VALIDATE_POINTER1( hDS, "OGR_L_DeleteFeature", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hDS)->DeleteFeature( nFID );
}

/************************************************************************/
/*                          GetFeaturesRead()                           */
/************************************************************************/

GIntBig OGRLayer::GetFeaturesRead()

{
    return m_nFeaturesRead;
}

/************************************************************************/
/*                       OGR_L_GetFeaturesRead()                        */
/************************************************************************/

GIntBig OGR_L_GetFeaturesRead( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetFeaturesRead", 0 );

    return ((OGRLayer *) hLayer)->GetFeaturesRead();
}

/************************************************************************/
/*                             GetFIDColumn                             */
/************************************************************************/

const char *OGRLayer::GetFIDColumn()

{
    return "";
}

/************************************************************************/
/*                         OGR_L_GetFIDColumn()                         */
/************************************************************************/

const char *OGR_L_GetFIDColumn( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetFIDColumn", NULL );

    return ((OGRLayer *) hLayer)->GetFIDColumn();
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRLayer::GetGeometryColumn()

{
    return "";
}

/************************************************************************/
/*                      OGR_L_GetGeometryColumn()                       */
/************************************************************************/

const char *OGR_L_GetGeometryColumn( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetGeometryColumn", NULL );

    return ((OGRLayer *) hLayer)->GetGeometryColumn();
}

/************************************************************************/
/*                            GetStyleTable()                           */
/************************************************************************/

OGRStyleTable *OGRLayer::GetStyleTable()
{
    return m_poStyleTable;
}

/************************************************************************/
/*                         SetStyleTableDirectly()                      */
/************************************************************************/

void OGRLayer::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    if ( m_poStyleTable )
        delete m_poStyleTable;
    m_poStyleTable = poStyleTable;
}

/************************************************************************/
/*                            SetStyleTable()                           */
/************************************************************************/

void OGRLayer::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if ( m_poStyleTable )
        delete m_poStyleTable;
    if ( poStyleTable )
        m_poStyleTable = poStyleTable->Clone();
}

/************************************************************************/
/*                         OGR_L_GetStyleTable()                        */
/************************************************************************/

OGRStyleTableH OGR_L_GetStyleTable( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetStyleTable", NULL );
    
    return (OGRStyleTableH) ((OGRLayer *) hLayer)->GetStyleTable( );
}

/************************************************************************/
/*                         OGR_L_SetStyleTableDirectly()                */
/************************************************************************/

void OGR_L_SetStyleTableDirectly( OGRLayerH hLayer,
                                  OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetStyleTableDirectly" );
    
    ((OGRLayer *) hLayer)->SetStyleTableDirectly( (OGRStyleTable *) hStyleTable);
}

/************************************************************************/
/*                         OGR_L_SetStyleTable()                        */
/************************************************************************/

void OGR_L_SetStyleTable( OGRLayerH hLayer,
                          OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetStyleTable" );
    VALIDATE_POINTER0( hStyleTable, "OGR_L_SetStyleTable" );
    
    ((OGRLayer *) hLayer)->SetStyleTable( (OGRStyleTable *) hStyleTable);
}

/************************************************************************/
/*                               GetName()                              */
/************************************************************************/

const char *OGRLayer::GetName()

{
    return GetLayerDefn()->GetName();
}

/************************************************************************/
/*                           OGR_L_GetName()                            */
/************************************************************************/

const char* OGR_L_GetName( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetName", "" );

    return ((OGRLayer *) hLayer)->GetName();
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

OGRwkbGeometryType OGRLayer::GetGeomType()
{
    return GetLayerDefn()->GetGeomType();
}
/************************************************************************/
/*                         OGR_L_GetGeomType()                          */
/************************************************************************/

OGRwkbGeometryType OGR_L_GetGeomType( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetGeomType", wkbUnknown );

    return ((OGRLayer *) hLayer)->GetGeomType();
}

/************************************************************************/
/*                          SetIgnoredFields()                          */
/************************************************************************/

OGRErr OGRLayer::SetIgnoredFields( const char **papszFields )
{
    OGRFeatureDefn *poDefn = GetLayerDefn();

    // first set everything as *not* ignored
    for( int iField = 0; iField < poDefn->GetFieldCount(); iField++ )
    {
        poDefn->GetFieldDefn(iField)->SetIgnored( FALSE );
    }
    poDefn->SetGeometryIgnored( FALSE );
    poDefn->SetStyleIgnored( FALSE );
    
    if ( papszFields == NULL )
        return OGRERR_NONE;

    // ignore some fields
    while ( *papszFields )
    {
        const char* pszFieldName = *papszFields;
        // check special fields
        if ( EQUAL(pszFieldName, "OGR_GEOMETRY") )
            poDefn->SetGeometryIgnored( TRUE );
        else if ( EQUAL(pszFieldName, "OGR_STYLE") )
            poDefn->SetStyleIgnored( TRUE );
        else
        {
            // check ordinary fields
            int iField = poDefn->GetFieldIndex(pszFieldName);
            if ( iField == -1 )
                return OGRERR_FAILURE;
            else
                poDefn->GetFieldDefn(iField)->SetIgnored( TRUE );
        }
        papszFields++;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       OGR_L_SetIgnoredFields()                       */
/************************************************************************/

OGRErr OGR_L_SetIgnoredFields( OGRLayerH hLayer, const char **papszFields )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_SetIgnoredFields", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *) hLayer)->SetIgnoredFields( papszFields );
}
