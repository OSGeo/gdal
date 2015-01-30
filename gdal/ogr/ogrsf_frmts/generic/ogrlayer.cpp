/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the OGRSFLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ograpispy.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                              OGRLayer()                              */
/************************************************************************/

OGRLayer::OGRLayer()

{
    m_poStyleTable = NULL;
    m_poAttrQuery = NULL;
    m_pszAttrQueryString = NULL;
    m_poAttrIndex = NULL;
    m_nRefCount = 0;

    m_nFeaturesRead = 0;

    m_poFilterGeom = NULL;
    m_bFilterIsEnvelope = FALSE;
    m_pPreparedFilterGeom = NULL;
    m_iGeomFieldFilter = 0;
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

    CPLFree( m_pszAttrQueryString );

    if( m_poFilterGeom )
    {
        delete m_poFilterGeom;
        m_poFilterGeom = NULL;
    }

    if( m_pPreparedFilterGeom != NULL )
    {
        OGRDestroyPreparedGeometry(m_pPreparedFilterGeom);
        m_pPreparedFilterGeom = NULL;
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
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig OGRLayer::GetFeatureCount( int bForce )

{
    OGRFeature     *poFeature;
    GIntBig         nFeatureCount = 0;

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
/*                      OGR_L_GetFeatureCount()                         */
/************************************************************************/

GIntBig OGR_L_GetFeatureCount( OGRLayerH hLayer, int bForce )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetFeatureCount", 0 );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetFeatureCount(hLayer, bForce);
#endif

    return ((OGRLayer *) hLayer)->GetFeatureCount(bForce);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
    return GetExtentInternal(0, psExtent, bForce);
}

OGRErr OGRLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce )

{
    if( iGeomField == 0 )
        return GetExtent(psExtent, bForce);
    else
        return GetExtentInternal(iGeomField, psExtent, bForce);
}

OGRErr OGRLayer::GetExtentInternal(int iGeomField, OGREnvelope *psExtent, int bForce )

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
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

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
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(iGeomField);
        if (poGeom == NULL || poGeom->IsEmpty())
        {
            /* Do nothing */
        }
        else if (!bExtentSet)
        {
            poGeom->getEnvelope(psExtent);
            if( !(CPLIsNan(psExtent->MinX) || CPLIsNan(psExtent->MinY) ||
                  CPLIsNan(psExtent->MaxX) || CPLIsNan(psExtent->MaxY)) )
            {
                bExtentSet = TRUE;
            }
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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetExtent(hLayer, bForce);
#endif

    return ((OGRLayer *) hLayer)->GetExtent( psExtent, bForce );
}

/************************************************************************/
/*                         OGR_L_GetExtentEx()                          */
/************************************************************************/

OGRErr OGR_L_GetExtentEx( OGRLayerH hLayer, int iGeomField,
                          OGREnvelope *psExtent, int bForce )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetExtentEx", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetExtentEx(hLayer, iGeomField, bForce);
#endif

    return ((OGRLayer *) hLayer)->GetExtent( iGeomField, psExtent, bForce );
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_SetAttributeFilter(hLayer, pszQuery);
#endif

    return ((OGRLayer *) hLayer)->SetAttributeFilter( pszQuery );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRLayer::GetFeature( GIntBig nFID )

{
    OGRFeature *poFeature;

    /* Save old attribute and spatial filters */
    char* pszOldFilter = m_pszAttrQueryString ? CPLStrdup(m_pszAttrQueryString) : NULL;
    OGRGeometry* poOldFilterGeom = ( m_poFilterGeom != NULL ) ? m_poFilterGeom->clone() : NULL;
    int iOldGeomFieldFilter = m_iGeomFieldFilter;
    /* Unset filters */
    SetAttributeFilter(NULL);
    SetSpatialFilter(0, NULL);

    ResetReading();
    while( (poFeature = GetNextFeature()) != NULL )
    {
        if( poFeature->GetFID() == nFID )
            break;
        else
            delete poFeature;
    }
    
    /* Restore filters */
    SetAttributeFilter(pszOldFilter);
    CPLFree(pszOldFilter);
    SetSpatialFilter(iOldGeomFieldFilter, poOldFilterGeom);
    delete poOldFilterGeom;
    
    return poFeature;
}

/************************************************************************/
/*                          OGR_L_GetFeature()                          */
/************************************************************************/

OGRFeatureH OGR_L_GetFeature( OGRLayerH hLayer, GIntBig nFeatureId )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetFeature", NULL );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetFeature(hLayer, nFeatureId);
#endif

    return (OGRFeatureH) ((OGRLayer *)hLayer)->GetFeature( nFeatureId );
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRLayer::SetNextByIndex( GIntBig nIndex )

{
    OGRFeature *poFeature;

    if( nIndex < 0 )
        return OGRERR_FAILURE;

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

OGRErr OGR_L_SetNextByIndex( OGRLayerH hLayer, GIntBig nIndex )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_SetNextByIndex", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_SetNextByIndex(hLayer, nIndex);
#endif

    return ((OGRLayer *)hLayer)->SetNextByIndex( nIndex );
}

/************************************************************************/
/*                        OGR_L_GetNextFeature()                        */
/************************************************************************/

OGRFeatureH OGR_L_GetNextFeature( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetNextFeature", NULL );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetNextFeature(hLayer);
#endif

    return (OGRFeatureH) ((OGRLayer *)hLayer)->GetNextFeature();
}

/************************************************************************/
/*                    ConvertNonLinearGeomsIfNecessary()                */
/************************************************************************/

void OGRLayer::ConvertNonLinearGeomsIfNecessary( OGRFeature *poFeature )
{
    if( !TestCapability(OLCCurveGeometries) )
    {
        int nGeomFieldCount = GetLayerDefn()->GetGeomFieldCount();
        for(int i=0;i<nGeomFieldCount;i++)
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom != NULL && OGR_GT_IsNonLinear(poGeom->getGeometryType()) )
            {
                OGRwkbGeometryType eTargetType = OGR_GT_GetLinear(poGeom->getGeometryType());
                poFeature->SetGeomFieldDirectly(i,
                    OGRGeometryFactory::forceTo(poFeature->StealGeometry(i), eTargetType));
            }
        }
    }
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRLayer::SetFeature( OGRFeature *poFeature )

{
    ConvertNonLinearGeomsIfNecessary(poFeature);
    return ISetFeature(poFeature);
}

/************************************************************************/
/*                             ISetFeature()                            */
/************************************************************************/

OGRErr OGRLayer::ISetFeature( OGRFeature * )

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_SetFeature(hLayer, hFeat);
#endif

    return ((OGRLayer *)hLayer)->SetFeature( (OGRFeature *) hFeat );
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRLayer::CreateFeature( OGRFeature *poFeature )

{
    ConvertNonLinearGeomsIfNecessary(poFeature);
    return ICreateFeature(poFeature);
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRLayer::ICreateFeature( OGRFeature * )

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_CreateFeature(hLayer, hFeat);
#endif

    return ((OGRLayer *) hLayer)->CreateFeature( (OGRFeature *) hFeat );
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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_CreateField(hLayer, hField, bApproxOK);
#endif

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_DeleteField(hLayer, iField);
#endif

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_ReorderFields(hLayer, panMap);
#endif

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_ReorderField(hLayer, iOldFieldPos, iNewFieldPos);
#endif

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_AlterFieldDefn(hLayer, iField, hNewFieldDefn, nFlags);
#endif

    return ((OGRLayer *) hLayer)->AlterFieldDefn( iField, (OGRFieldDefn*) hNewFieldDefn, nFlags );
}

/************************************************************************/
/*                         CreateGeomField()                            */
/************************************************************************/

OGRErr OGRLayer::CreateGeomField( OGRGeomFieldDefn * poField, int bApproxOK )

{
    (void) poField;
    (void) bApproxOK;

    CPLError( CE_Failure, CPLE_NotSupported,
              "CreateGeomField() not supported by this layer.\n" );
              
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_CreateGeomField()                       */
/************************************************************************/

OGRErr OGR_L_CreateGeomField( OGRLayerH hLayer, OGRGeomFieldDefnH hField, 
                              int bApproxOK )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_CreateGeomField", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( hField, "OGR_L_CreateGeomField", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_CreateGeomField(hLayer, hField, bApproxOK);
#endif

    return ((OGRLayer *) hLayer)->CreateGeomField( (OGRGeomFieldDefn *) hField, 
                                                   bApproxOK );
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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_StartTransaction(hLayer);
#endif

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_CommitTransaction(hLayer);
#endif

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_RollbackTransaction(hLayer);
#endif

    return ((OGRLayer *)hLayer)->RollbackTransaction();
}

/************************************************************************/
/*                         OGR_L_GetLayerDefn()                         */
/************************************************************************/

OGRFeatureDefnH OGR_L_GetLayerDefn( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetLayerDefn", NULL );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetLayerDefn(hLayer);
#endif

    return (OGRFeatureDefnH) ((OGRLayer *)hLayer)->GetLayerDefn();
}

/************************************************************************/
/*                         OGR_L_FindFieldIndex()                       */
/************************************************************************/

int OGR_L_FindFieldIndex( OGRLayerH hLayer, const char *pszFieldName, int bExactMatch )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_FindFieldIndex", -1 );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_FindFieldIndex(hLayer, pszFieldName, bExactMatch);
#endif

    return ((OGRLayer *)hLayer)->FindFieldIndex( pszFieldName, bExactMatch );
}

/************************************************************************/
/*                           FindFieldIndex()                           */
/************************************************************************/

int OGRLayer::FindFieldIndex( const char *pszFieldName, CPL_UNUSED int bExactMatch )
{
    return GetLayerDefn()->GetFieldIndex( pszFieldName );
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRLayer::GetSpatialRef()
{ 
    if( GetLayerDefn()->GetGeomFieldCount() > 0 )
        return GetLayerDefn()->GetGeomFieldDefn(0)->GetSpatialRef();
    else
        return NULL;
}

/************************************************************************/
/*                        OGR_L_GetSpatialRef()                         */
/************************************************************************/

OGRSpatialReferenceH OGR_L_GetSpatialRef( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetSpatialRef", NULL );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetSpatialRef(hLayer);
#endif

    return (OGRSpatialReferenceH) ((OGRLayer *) hLayer)->GetSpatialRef();
}

/************************************************************************/
/*                        OGR_L_TestCapability()                        */
/************************************************************************/

int OGR_L_TestCapability( OGRLayerH hLayer, const char *pszCap )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_TestCapability", 0 );
    VALIDATE_POINTER1( pszCap, "OGR_L_TestCapability", 0 );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_TestCapability(hLayer, pszCap);
#endif

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetSpatialFilter(hLayer);
#endif

    return (OGRGeometryH) ((OGRLayer *) hLayer)->GetSpatialFilter();
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    m_iGeomFieldFilter = 0;
    if( InstallFilter( poGeomIn ) )
        ResetReading();
}


void OGRLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField == 0 )
    {
        m_iGeomFieldFilter = iGeomField;
        SetSpatialFilter( poGeomIn );
    }
    else
    {
        if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
            return;
        }

        m_iGeomFieldFilter = iGeomField;
        if( InstallFilter( poGeomIn ) )
            ResetReading();
    }
}

/************************************************************************/
/*                       OGR_L_SetSpatialFilter()                       */
/************************************************************************/

void OGR_L_SetSpatialFilter( OGRLayerH hLayer, OGRGeometryH hGeom )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetSpatialFilter" );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_SetSpatialFilter(hLayer, hGeom);
#endif

    ((OGRLayer *) hLayer)->SetSpatialFilter( (OGRGeometry *) hGeom );
}

/************************************************************************/
/*                      OGR_L_SetSpatialFilterEx()                      */
/************************************************************************/

void OGR_L_SetSpatialFilterEx( OGRLayerH hLayer, int iGeomField, 
                               OGRGeometryH hGeom )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetSpatialFilterEx" );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_SetSpatialFilterEx(hLayer, iGeomField, hGeom);
#endif

    ((OGRLayer *) hLayer)->SetSpatialFilter( iGeomField, (OGRGeometry *) hGeom );
}
/************************************************************************/
/*                        SetSpatialFilterRect()                        */
/************************************************************************/

void OGRLayer::SetSpatialFilterRect( double dfMinX, double dfMinY, 
                                     double dfMaxX, double dfMaxY )

{
    SetSpatialFilterRect( 0, dfMinX, dfMinY, dfMaxX, dfMaxY );
}


void OGRLayer::SetSpatialFilterRect( int iGeomField, 
                                     double dfMinX, double dfMinY, 
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

    if( iGeomField == 0 )
        /* for drivers that only overload SetSpatialFilter(OGRGeometry*) */
        SetSpatialFilter( &oPoly );
    else
        SetSpatialFilter( iGeomField, &oPoly );
}

/************************************************************************/
/*                     OGR_L_SetSpatialFilterRect()                     */
/************************************************************************/

void OGR_L_SetSpatialFilterRect( OGRLayerH hLayer, 
                                 double dfMinX, double dfMinY, 
                                 double dfMaxX, double dfMaxY )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetSpatialFilterRect" );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_SetSpatialFilterRect(hLayer, dfMinX, dfMinY, dfMaxX, dfMaxY);
#endif

    ((OGRLayer *) hLayer)->SetSpatialFilterRect( dfMinX, dfMinY, 
                                                 dfMaxX, dfMaxY );
}

/************************************************************************/
/*                    OGR_L_SetSpatialFilterRectEx()                    */
/************************************************************************/

void OGR_L_SetSpatialFilterRectEx( OGRLayerH hLayer,
                                   int iGeomField,
                                   double dfMinX, double dfMinY, 
                                   double dfMaxX, double dfMaxY )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetSpatialFilterRectEx" );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_SetSpatialFilterRectEx(hLayer, iGeomField, dfMinX, dfMinY, dfMaxX, dfMaxY);
#endif

    ((OGRLayer *) hLayer)->SetSpatialFilterRect( iGeomField,
                                                 dfMinX, dfMinY, 
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

    if( m_pPreparedFilterGeom != NULL )
    {
        OGRDestroyPreparedGeometry(m_pPreparedFilterGeom);
        m_pPreparedFilterGeom = NULL;
    }

    if( poFilter != NULL )
        m_poFilterGeom = poFilter->clone();

    m_bFilterIsEnvelope = FALSE;

    if( m_poFilterGeom == NULL )
        return TRUE;

    if( m_poFilterGeom != NULL )
        m_poFilterGeom->getEnvelope( &m_sFilterEnvelope );

    /* Compile geometry filter as a prepared geometry */
    m_pPreparedFilterGeom = OGRCreatePreparedGeometry(m_poFilterGeom);

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
            if( m_pPreparedFilterGeom != NULL )
                return OGRPreparedGeometryIntersects(m_pPreparedFilterGeom,
                                                     poGeometry);
            else
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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_ResetReading(hLayer);
#endif

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

OGRErr OGR_L_SyncToDisk( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_SyncToDisk", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_SyncToDisk(hLayer);
#endif

    return ((OGRLayer *) hLayer)->SyncToDisk();
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRLayer::DeleteFeature( CPL_UNUSED GIntBig nFID )
{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_DeleteFeature()                         */
/************************************************************************/

OGRErr OGR_L_DeleteFeature( OGRLayerH hLayer, GIntBig nFID )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_DeleteFeature", OGRERR_INVALID_HANDLE );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_DeleteFeature(hLayer, nFID);
#endif

    return ((OGRLayer *) hLayer)->DeleteFeature( nFID );
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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetFIDColumn(hLayer);
#endif

    return ((OGRLayer *) hLayer)->GetFIDColumn();
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRLayer::GetGeometryColumn()

{
    if( GetLayerDefn()->GetGeomFieldCount() > 0 )
        return GetLayerDefn()->GetGeomFieldDefn(0)->GetNameRef();
    else
        return "";
}

/************************************************************************/
/*                      OGR_L_GetGeometryColumn()                       */
/************************************************************************/

const char *OGR_L_GetGeometryColumn( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetGeometryColumn", NULL );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetGeometryColumn(hLayer);
#endif

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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetName(hLayer);
#endif

    return ((OGRLayer *) hLayer)->GetName();
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

OGRwkbGeometryType OGRLayer::GetGeomType()
{
    OGRFeatureDefn* poLayerDefn = GetLayerDefn();
    if( poLayerDefn == NULL )
    {
        CPLDebug("OGR", "GetLayerType() returns NULL !");
        return wkbUnknown;
    }
    return poLayerDefn->GetGeomType();
}

/************************************************************************/
/*                         OGR_L_GetGeomType()                          */
/************************************************************************/

OGRwkbGeometryType OGR_L_GetGeomType( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetGeomType", wkbUnknown );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetGeomType(hLayer);
#endif

    OGRwkbGeometryType eType = ((OGRLayer *) hLayer)->GetGeomType();
    if( OGR_GT_IsNonLinear(eType) && !OGRGetNonLinearGeometriesEnabledFlag() )
    {
        eType = OGR_GT_GetLinear(eType);
    }
    return eType;
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
            {
                // check geometry field
                iField = poDefn->GetGeomFieldIndex(pszFieldName);
                if ( iField == -1 )
                {
                    return OGRERR_FAILURE;
                }
                else
                    poDefn->GetGeomFieldDefn(iField)->SetIgnored( TRUE );
            }
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

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_SetIgnoredFields(hLayer, papszFields);
#endif

    return ((OGRLayer *) hLayer)->SetIgnoredFields( papszFields );
}

/************************************************************************/
/*         helper functions for layer overlay methods                   */
/************************************************************************/

static
OGRErr clone_spatial_filter(OGRLayer *pLayer, OGRGeometry **ppGeometry)
{
    OGRErr ret = OGRERR_NONE;
    OGRGeometry *g = pLayer->GetSpatialFilter();
    *ppGeometry = g ? g->clone() : NULL;
    return ret;
}

static
OGRErr create_field_map(OGRFeatureDefn *poDefn, int **map)
{
    OGRErr ret = OGRERR_NONE;
    int n = poDefn->GetFieldCount();
    if (n > 0) {
        *map = (int*)VSIMalloc(sizeof(int) * n);
        if (!(*map)) return OGRERR_NOT_ENOUGH_MEMORY;
        for(int i=0;i<n;i++)
            (*map)[i] = -1;
    }
    return ret;
}

static
OGRErr set_result_schema(OGRLayer *pLayerResult,
                         OGRFeatureDefn *poDefnInput, 
                         OGRFeatureDefn *poDefnMethod,
                         int *mapInput,
                         int *mapMethod,
                         int combined,
                         char** papszOptions)
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnResult = pLayerResult->GetLayerDefn();
    const char* pszInputPrefix = CSLFetchNameValue(papszOptions, "INPUT_PREFIX");
    const char* pszMethodPrefix = CSLFetchNameValue(papszOptions, "METHOD_PREFIX");
    int bSkipFailures = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    if (poDefnResult->GetFieldCount() > 0) {
        // the user has defined the schema of the output layer
        for( int iField = 0; iField < poDefnInput->GetFieldCount(); iField++ ) {
            CPLString osName(poDefnInput->GetFieldDefn(iField)->GetNameRef());
            if( pszInputPrefix != NULL )
                osName = pszInputPrefix + osName;
            mapInput[iField] = poDefnResult->GetFieldIndex(osName);
        }
        if (!mapMethod) return ret;
        for( int iField = 0; iField < poDefnMethod->GetFieldCount(); iField++ ) {
            CPLString osName(poDefnMethod->GetFieldDefn(iField)->GetNameRef());
            if( pszMethodPrefix != NULL )
                osName = pszMethodPrefix + osName;
            mapMethod[iField] = poDefnResult->GetFieldIndex(osName);
        }
    } else {
        // use schema from the input layer or from input and method layers
        int nFieldsInput = poDefnInput->GetFieldCount();
        for( int iField = 0; iField < nFieldsInput; iField++ ) {
            OGRFieldDefn oFieldDefn(poDefnInput->GetFieldDefn(iField));
            if( pszInputPrefix != NULL )
                oFieldDefn.SetName(CPLSPrintf("%s%s", pszInputPrefix, oFieldDefn.GetNameRef()));
            ret = pLayerResult->CreateField(&oFieldDefn);
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) 
                    return ret;
                else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
            mapInput[iField] = iField;
        }
        if (!combined) return ret;
        if (!mapMethod) return ret;
        for( int iField = 0; iField < poDefnMethod->GetFieldCount(); iField++ ) {
            OGRFieldDefn oFieldDefn(poDefnMethod->GetFieldDefn(iField));
            if( pszMethodPrefix != NULL )
                oFieldDefn.SetName(CPLSPrintf("%s%s", pszMethodPrefix, oFieldDefn.GetNameRef()));
            ret = pLayerResult->CreateField(&oFieldDefn);
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) 
                    return ret;
                else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
            mapMethod[iField] = nFieldsInput+iField;
        }
    }
    return ret;
}

static
OGRGeometry *set_filter_from(OGRLayer *pLayer, OGRGeometry *pGeometryExistingFilter, OGRFeature *pFeature)
{
    OGRGeometry *geom = pFeature->GetGeometryRef();
    if (!geom) return NULL;
    if (pGeometryExistingFilter) {
        if (!geom->Intersects(pGeometryExistingFilter)) return NULL;
        OGRGeometry *intersection = geom->Intersection(pGeometryExistingFilter);
        pLayer->SetSpatialFilter(intersection);
        if (intersection) delete intersection;
    } else {
        pLayer->SetSpatialFilter(geom);
    }
    return geom;
}

static OGRGeometry* promote_to_multi(OGRGeometry* poGeom)
{
    OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    if( eType == wkbPolygon )
        return OGRGeometryFactory::forceToMultiPolygon(poGeom);
    else if( eType == wkbLineString )
        return OGRGeometryFactory::forceToMultiLineString(poGeom);
    else
        return poGeom;
}

/************************************************************************/
/*                          Intersection()                              */
/************************************************************************/
/**
 * \brief Intersection of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are common between features in the input layer and in the
 * method layer. The features in the result layer have attributes from
 * both input and method layers. The schema of the result layer can be
 * set by the user or, if it is empty, is initialized to contain all
 * fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This method is the same as the C function OGR_L_Intersection().
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Intersection( OGRLayer *pLayerMethod, 
                               OGRLayer *pLayerResult, 
                               char** papszOptions, 
                               GDALProgressFunc pfnProgress, 
                               void * pProgressArg )
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = NULL;
    OGRGeometry *pGeometryMethodFilter = NULL;
    int *mapInput = NULL;
    int *mapMethod = NULL;
    OGREnvelope sEnvelopeMethod;
    GBool bEnvelopeSet;
    double progress_max = GetFeatureCount(0);
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, 1, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();
    bEnvelopeSet = pLayerMethod->GetExtent(&sEnvelopeMethod, 1) == OGRERR_NONE;

    ResetReading();
    while (OGRFeature *x = GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete x;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // is it worth to proceed?
        if (bEnvelopeSet) {
            OGRGeometry *x_geom = x->GetGeometryRef();
            if (x_geom) {
                OGREnvelope x_env;
                x_geom->getEnvelope(&x_env);
                if (x_env.MaxX < sEnvelopeMethod.MinX 
                    || x_env.MaxY < sEnvelopeMethod.MinY
                    || sEnvelopeMethod.MaxX < x_env.MinX
                    || sEnvelopeMethod.MaxY < x_env.MinY) {
                    delete x;
                    continue;
                }
            } else {
                delete x;
                continue;
            }
        }

        // set up the filter for method layer
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x);
        if (!x_geom) {
            delete x;
            continue;
        }

        pLayerMethod->ResetReading();
        while (OGRFeature *y = pLayerMethod->GetNextFeature()) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {delete y; continue;}
            OGRGeometry* poIntersection = x_geom->Intersection(y_geom);
            if( poIntersection == NULL || poIntersection->IsEmpty() ||
                (x_geom->getDimension() == 2 &&
                y_geom->getDimension() == 2 &&
                poIntersection->getDimension() < 2) )
            {
                delete poIntersection;
                delete y;
            }
            else
            {
                OGRFeature *z = new OGRFeature(poDefnResult);
                z->SetFieldsFrom(x, mapInput);
                z->SetFieldsFrom(y, mapMethod);
                if( bPromoteToMulti )
                    poIntersection = promote_to_multi(poIntersection);
                z->SetGeometryDirectly(poIntersection);
                delete y;
                ret = pLayerResult->CreateFeature(z);
                delete z;
                if (ret != OGRERR_NONE) {
                    if (!bSkipFailures) {
                        delete x; 
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
            }
        }

        delete x;
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg)) {
      CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
      ret = OGRERR_FAILURE;
      goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter) delete pGeometryMethodFilter;
    if (mapInput) VSIFree(mapInput);
    if (mapMethod) VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                       OGR_L_Intersection()                           */
/************************************************************************/
/**
 * \brief Intersection of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are common between features in the input layer and in the
 * method layer. The features in the result layer have attributes from
 * both input and method layers. The schema of the result layer can be
 * set by the user or, if it is empty, is initialized to contain all
 * fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Intersection().
 * 
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Intersection( OGRLayerH pLayerInput, 
                           OGRLayerH pLayerMethod, 
                           OGRLayerH pLayerResult, 
                           char** papszOptions, 
                           GDALProgressFunc pfnProgress, 
                           void * pProgressArg )

{
    VALIDATE_POINTER1( pLayerInput, "OGR_L_Intersection", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerMethod, "OGR_L_Intersection", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerResult, "OGR_L_Intersection", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)pLayerInput)->Intersection( (OGRLayer *)pLayerMethod, (OGRLayer *)pLayerResult, papszOptions, pfnProgress, pProgressArg );
}

/************************************************************************/
/*                              Union()                                 */
/************************************************************************/

/**
 * \brief Union of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are in either in the input layer or in the method layer. The
 * features in the result layer have attributes from both input and
 * method layers. For features which represent areas that are only in
 * the input or in the method layer the respective attributes have
 * undefined values. The schema of the result layer can be set by the
 * user or, if it is empty, is initialized to contain all fields in
 * the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This method is the same as the C function OGR_L_Union().
 * 
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Union( OGRLayer *pLayerMethod, 
                        OGRLayer *pLayerResult, 
                        char** papszOptions, 
                        GDALProgressFunc pfnProgress, 
                        void * pProgressArg )
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = NULL;
    OGRGeometry *pGeometryMethodFilter = NULL;
    OGRGeometry *pGeometryInputFilter = NULL;
    int *mapInput = NULL;
    int *mapMethod = NULL;
    double progress_max = GetFeatureCount(0) + pLayerMethod->GetFeatureCount(0);
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(this, &pGeometryInputFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, 1, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // add features based on input layer
    ResetReading();
    while (OGRFeature *x = GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete x;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x);
        if (!x_geom) {
            delete x; 
            continue;
        }
        
        OGRGeometry *x_geom_diff = x_geom->clone(); // this will be the geometry of the result feature
        pLayerMethod->ResetReading();
        while (OGRFeature *y = pLayerMethod->GetNextFeature()) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {delete y; continue;}
            OGRGeometry *poIntersection = x_geom->Intersection(y_geom);
            if( poIntersection == NULL || poIntersection->IsEmpty() ||
                (x_geom->getDimension() == 2 &&
                y_geom->getDimension() == 2 &&
                poIntersection->getDimension() < 2) )
            {
                delete poIntersection;
                delete y;
            }
            else
            {
                OGRFeature *z = new OGRFeature(poDefnResult);
                z->SetFieldsFrom(x, mapInput);
                z->SetFieldsFrom(y, mapMethod);
                if( bPromoteToMulti )
                    poIntersection = promote_to_multi(poIntersection);
                z->SetGeometryDirectly(poIntersection);
                OGRGeometry *x_geom_diff_new = x_geom_diff ? x_geom_diff->Difference(y_geom) : NULL;
                if (x_geom_diff) delete x_geom_diff;
                x_geom_diff = x_geom_diff_new;
                delete y;
                ret = pLayerResult->CreateFeature(z);
                delete z;
                if (ret != OGRERR_NONE) {
                    if (!bSkipFailures) {
                        delete x;
                        if (x_geom_diff)
                            delete x_geom_diff;
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
            }
        }

        if( x_geom_diff == NULL || x_geom_diff->IsEmpty() )
        {
            delete x_geom_diff;
            delete x;
        }
        else
        {
            OGRFeature *z = new OGRFeature(poDefnResult);
            z->SetFieldsFrom(x, mapInput);
            if( bPromoteToMulti )
                x_geom_diff = promote_to_multi(x_geom_diff);
            z->SetGeometryDirectly(x_geom_diff);
            delete x;
            ret = pLayerResult->CreateFeature(z);
            delete z;
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) {
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }

    // restore filter on method layer and add features based on it
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    pLayerMethod->ResetReading();
    while (OGRFeature *x = pLayerMethod->GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete x;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on input layer
        OGRGeometry *x_geom = set_filter_from(this, pGeometryInputFilter, x);
        if (!x_geom) {
            delete x; 
            continue;
        }
        
        OGRGeometry *x_geom_diff = x_geom->clone(); // this will be the geometry of the result feature
        ResetReading();
        while (OGRFeature *y = GetNextFeature()) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {delete y; continue;}
            OGRGeometry *x_geom_diff_new = x_geom_diff ? x_geom_diff->Difference(y_geom) : NULL;
            if (x_geom_diff) delete x_geom_diff;
            x_geom_diff = x_geom_diff_new;
            delete y;
        }

        if( x_geom_diff == NULL || x_geom_diff->IsEmpty() )
        {
            delete x_geom_diff;
            delete x;
        }
        else
        {
            OGRFeature *z = new OGRFeature(poDefnResult);
            z->SetFieldsFrom(x, mapMethod);
            if( bPromoteToMulti )
                x_geom_diff = promote_to_multi(x_geom_diff);
            z->SetGeometryDirectly(x_geom_diff);
            delete x;
            ret = pLayerResult->CreateFeature(z);
            delete z;
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) {
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg)) {
      CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
      ret = OGRERR_FAILURE;
      goto done;
    }
done:
    // release resources
    SetSpatialFilter(pGeometryInputFilter);
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter) delete pGeometryMethodFilter;
    if (pGeometryInputFilter) delete pGeometryInputFilter;
    if (mapInput) VSIFree(mapInput);
    if (mapMethod) VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                           OGR_L_Union()                              */
/************************************************************************/

/**
 * \brief Union of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are in either in the input layer or in the method layer. The
 * features in the result layer have attributes from both input and
 * method layers. For features which represent areas that are only in
 * the input or in the method layer the respective attributes have
 * undefined values. The schema of the result layer can be set by the
 * user or, if it is empty, is initialized to contain all fields in
 * the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Union().
 * 
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10 
 */

OGRErr OGR_L_Union( OGRLayerH pLayerInput, 
                    OGRLayerH pLayerMethod, 
                    OGRLayerH pLayerResult, 
                    char** papszOptions, 
                    GDALProgressFunc pfnProgress, 
                    void * pProgressArg )

{
    VALIDATE_POINTER1( pLayerInput, "OGR_L_Union", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerMethod, "OGR_L_Union", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerResult, "OGR_L_Union", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)pLayerInput)->Union( (OGRLayer *)pLayerMethod, (OGRLayer *)pLayerResult, papszOptions, pfnProgress, pProgressArg );
}

/************************************************************************/
/*                          SymDifference()                             */
/************************************************************************/

/**
 * \brief Symmetrical difference of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are in either in the input layer or in the method layer but
 * not in both. The features in the result layer have attributes from
 * both input and method layers. For features which represent areas
 * that are only in the input or in the method layer the respective
 * attributes have undefined values. The schema of the result layer
 * can be set by the user or, if it is empty, is initialized to
 * contain all fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This method is the same as the C function OGR_L_SymDifference().
 * 
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::SymDifference( OGRLayer *pLayerMethod, 
                                OGRLayer *pLayerResult, 
                                char** papszOptions, 
                                GDALProgressFunc pfnProgress, 
                                void * pProgressArg )
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = NULL;
    OGRGeometry *pGeometryMethodFilter = NULL;
    OGRGeometry *pGeometryInputFilter = NULL;
    int *mapInput = NULL;
    int *mapMethod = NULL;
    double progress_max = GetFeatureCount(0) + pLayerMethod->GetFeatureCount(0);
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(this, &pGeometryInputFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, 1, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // add features based on input layer
    ResetReading();
    while (OGRFeature *x = GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete x;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x);
        if (!x_geom) {
            delete x; 
            continue;
        }
        
        OGRGeometry *geom = x_geom->clone(); // this will be the geometry of the result feature
        pLayerMethod->ResetReading();
        while (OGRFeature *y = pLayerMethod->GetNextFeature()) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {delete y; continue;}
            OGRGeometry *geom_new = geom ? geom->Difference(y_geom) : NULL;
            if (geom) delete geom;
            geom = geom_new;
            delete y;
            if (geom && geom->IsEmpty()) break;
        }

        OGRFeature *z = NULL;
        if (geom && !geom->IsEmpty()) {
            z = new OGRFeature(poDefnResult);
            z->SetFieldsFrom(x, mapInput);
            if( bPromoteToMulti )
                geom = promote_to_multi(geom);
            z->SetGeometryDirectly(geom);
        } else {
            if (geom) delete geom;
        }
        delete x;
        if (z) {
            ret = pLayerResult->CreateFeature(z);
            delete z;
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) {
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }

    // restore filter on method layer and add features based on it
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    pLayerMethod->ResetReading();
    while (OGRFeature *x = pLayerMethod->GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete x;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on input layer
        OGRGeometry *x_geom = set_filter_from(this, pGeometryInputFilter, x);
        if (!x_geom) {
            delete x; 
            continue;
        }
        
        OGRGeometry *geom = x_geom->clone(); // this will be the geometry of the result feature
        ResetReading();
        while (OGRFeature *y = GetNextFeature()) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {delete y; continue;}
            OGRGeometry *geom_new = geom ? geom->Difference(y_geom) : NULL;
            if (geom) delete geom;
            geom = geom_new;
            delete y;
            if (geom == NULL || geom->IsEmpty()) break;
        }

        OGRFeature *z = NULL;
        if (geom && !geom->IsEmpty()) {
            z = new OGRFeature(poDefnResult);
            z->SetFieldsFrom(x, mapMethod);
            if( bPromoteToMulti )
                geom = promote_to_multi(geom);
            z->SetGeometryDirectly(geom);
        } else {
            if (geom) delete geom;
        }
        delete x;
        if (z) {
            ret = pLayerResult->CreateFeature(z);
            delete z;
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) {
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg)) {
      CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
      ret = OGRERR_FAILURE;
      goto done;
    }
done:
    // release resources
    SetSpatialFilter(pGeometryInputFilter);
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter) delete pGeometryMethodFilter;
    if (pGeometryInputFilter) delete pGeometryInputFilter;
    if (mapInput) VSIFree(mapInput);
    if (mapMethod) VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                        OGR_L_SymDifference()                         */
/************************************************************************/

/**
 * \brief Symmetrical difference of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are in either in the input layer or in the method layer but
 * not in both. The features in the result layer have attributes from
 * both input and method layers. For features which represent areas
 * that are only in the input or in the method layer the respective
 * attributes have undefined values. The schema of the result layer
 * can be set by the user or, if it is empty, is initialized to
 * contain all fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::SymDifference().
 *
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_SymDifference( OGRLayerH pLayerInput, 
                            OGRLayerH pLayerMethod, 
                            OGRLayerH pLayerResult, 
                            char** papszOptions, 
                            GDALProgressFunc pfnProgress, 
                            void * pProgressArg )

{
    VALIDATE_POINTER1( pLayerInput, "OGR_L_SymDifference", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerMethod, "OGR_L_SymDifference", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerResult, "OGR_L_SymDifference", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)pLayerInput)->SymDifference( (OGRLayer *)pLayerMethod, (OGRLayer *)pLayerResult, papszOptions, pfnProgress, pProgressArg );
}

/************************************************************************/
/*                            Identity()                                */
/************************************************************************/

/**
 * \brief Identify the features of this layer with the ones from the
 * identity layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer. The features in the result layer have
 * attributes from both input and method layers. The schema of the
 * result layer can be set by the user or, if it is empty, is
 * initialized to contain all fields in input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This method is the same as the C function OGR_L_Identity().
 * 
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Identity( OGRLayer *pLayerMethod, 
                           OGRLayer *pLayerResult, 
                           char** papszOptions, 
                           GDALProgressFunc pfnProgress, 
                           void * pProgressArg )
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = NULL;
    OGRGeometry *pGeometryMethodFilter = NULL;
    int *mapInput = NULL;
    int *mapMethod = NULL;
    double progress_max = GetFeatureCount(0);
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, 1, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // split the features in input layer to the result layer
    ResetReading();
    while (OGRFeature *x = GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete x;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x);
        if (!x_geom) {
            delete x; 
            continue;
        }
        
        OGRGeometry *x_geom_diff = x_geom->clone(); // this will be the geometry of the result feature
        pLayerMethod->ResetReading();
        while (OGRFeature *y = pLayerMethod->GetNextFeature()) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {delete y; continue;}
            OGRGeometry* poIntersection = x_geom->Intersection(y_geom);
            if( poIntersection == NULL || poIntersection->IsEmpty() ||
                (x_geom->getDimension() == 2 &&
                y_geom->getDimension() == 2 &&
                poIntersection->getDimension() < 2) )
            {
                delete poIntersection;
                delete y;
            }
            else
            {
                OGRFeature *z = new OGRFeature(poDefnResult);
                z->SetFieldsFrom(x, mapInput);
                z->SetFieldsFrom(y, mapMethod);
                if( bPromoteToMulti )
                    poIntersection = promote_to_multi(poIntersection);
                z->SetGeometryDirectly(poIntersection);
                OGRGeometry *x_geom_diff_new = x_geom_diff ? x_geom_diff->Difference(y_geom) : NULL;
                if (x_geom_diff) delete x_geom_diff;
                x_geom_diff = x_geom_diff_new;
                delete y;
                ret = pLayerResult->CreateFeature(z);
                delete z;
                if (ret != OGRERR_NONE) {
                    if (!bSkipFailures) {
                        delete x;
                        delete x_geom_diff;
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
            }
        }

        if( x_geom_diff == NULL || x_geom_diff->IsEmpty() )
        {
            delete x_geom_diff;
            delete x;
        }
        else
        {
            OGRFeature *z = new OGRFeature(poDefnResult);
            z->SetFieldsFrom(x, mapInput);
            if( bPromoteToMulti )
                x_geom_diff = promote_to_multi(x_geom_diff);
            z->SetGeometryDirectly(x_geom_diff);
            delete x;
            ret = pLayerResult->CreateFeature(z);
            delete z;
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) {
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg)) {
      CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
      ret = OGRERR_FAILURE;
      goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter) delete pGeometryMethodFilter;
    if (mapInput) VSIFree(mapInput);
    if (mapMethod) VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                         OGR_L_Identity()                             */
/************************************************************************/

/**
 * \brief Identify the features of this layer with the ones from the
 * identity layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer. The features in the result layer have
 * attributes from both input and method layers. The schema of the
 * result layer can be set by the user or, if it is empty, is
 * initialized to contain all fields in input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Identity().
 * 
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Identity( OGRLayerH pLayerInput, 
                       OGRLayerH pLayerMethod, 
                       OGRLayerH pLayerResult, 
                       char** papszOptions, 
                       GDALProgressFunc pfnProgress, 
                       void * pProgressArg )

{
    VALIDATE_POINTER1( pLayerInput, "OGR_L_Identity", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerMethod, "OGR_L_Identity", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerResult, "OGR_L_Identity", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)pLayerInput)->Identity( (OGRLayer *)pLayerMethod, (OGRLayer *)pLayerResult, papszOptions, pfnProgress, pProgressArg );
}

/************************************************************************/
/*                             Update()                                 */
/************************************************************************/

/**
 * \brief Update this layer with features from the update layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are either in the input layer or in the method layer. The
 * features in the result layer have areas of the features of the
 * method layer or those ares of the features of the input layer that
 * are not covered by the method layer. The features of the result
 * layer get their attributes from the input layer. The schema of the
 * result layer can be set by the user or, if it is empty, is
 * initialized to contain all fields in the input layer.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in the method layer, then
 * the attribute in the result feature the originates from the method
 * layer will get the value from the feature of the method layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This method is the same as the C function OGR_L_Update().
 * 
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Update( OGRLayer *pLayerMethod, 
                         OGRLayer *pLayerResult, 
                         char** papszOptions, 
                         GDALProgressFunc pfnProgress, 
                         void * pProgressArg )
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = NULL;
    OGRGeometry *pGeometryMethodFilter = NULL;
    int *mapInput = NULL;
    int *mapMethod = NULL;
    double progress_max = GetFeatureCount(0) + pLayerMethod->GetFeatureCount(0);
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, 0, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // add clipped features from the input layer
    ResetReading();
    while (OGRFeature *x = GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete x;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x);
        if (!x_geom) {
            delete x; 
            continue;
        }
        
        OGRGeometry *x_geom_diff = x_geom->clone(); //this will be the geometry of a result feature
        pLayerMethod->ResetReading();
        while (OGRFeature *y = pLayerMethod->GetNextFeature()) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {delete y; continue;}
            OGRGeometry *x_geom_diff_new = x_geom_diff ? x_geom_diff->Difference(y_geom) : NULL;
            if (x_geom_diff) delete x_geom_diff;
            x_geom_diff = x_geom_diff_new;
            delete y;
        }

        if( x_geom_diff == NULL || x_geom_diff->IsEmpty() )
        {
            delete x_geom_diff;
            delete x;
        }
        else
        {
            OGRFeature *z = new OGRFeature(poDefnResult);
            z->SetFieldsFrom(x, mapInput);
            if( bPromoteToMulti )
                x_geom_diff = promote_to_multi(x_geom_diff);
            z->SetGeometryDirectly(x_geom_diff);
            delete x;
            ret = pLayerResult->CreateFeature(z);
            delete z;
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) {
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }

    // restore the original filter and add features from the update layer
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    pLayerMethod->ResetReading();
    while (OGRFeature *y = pLayerMethod->GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete y;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        OGRGeometry *y_geom = y->GetGeometryRef();
        if (!y_geom) {delete y; continue;}
        OGRFeature *z = new OGRFeature(poDefnResult);
        if (mapMethod) z->SetFieldsFrom(y, mapMethod);
        z->SetGeometry(y_geom);
        delete y;
        ret = pLayerResult->CreateFeature(z);
        delete z;
        if (ret != OGRERR_NONE) {
            if (!bSkipFailures) {
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg)) {
      CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
      ret = OGRERR_FAILURE;
      goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter) delete pGeometryMethodFilter;
    if (mapInput) VSIFree(mapInput);
    if (mapMethod) VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                          OGR_L_Update()                              */
/************************************************************************/

/**
 * \brief Update this layer with features from the update layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are either in the input layer or in the method layer. The
 * features in the result layer have areas of the features of the
 * method layer or those ares of the features of the input layer that
 * are not covered by the method layer. The features of the result
 * layer get their attributes from the input layer. The schema of the
 * result layer can be set by the user or, if it is empty, is
 * initialized to contain all fields in the input layer.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in the method layer, then
 * the attribute in the result feature the originates from the method
 * layer will get the value from the feature of the method layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Update().
 * 
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Update( OGRLayerH pLayerInput, 
                     OGRLayerH pLayerMethod, 
                     OGRLayerH pLayerResult, 
                     char** papszOptions, 
                     GDALProgressFunc pfnProgress, 
                     void * pProgressArg )

{
    VALIDATE_POINTER1( pLayerInput, "OGR_L_Update", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerMethod, "OGR_L_Update", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerResult, "OGR_L_Update", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)pLayerInput)->Update( (OGRLayer *)pLayerMethod, (OGRLayer *)pLayerResult, papszOptions, pfnProgress, pProgressArg );
}

/************************************************************************/
/*                              Clip()                                  */
/************************************************************************/

/**
 * \brief Clip off areas that are not covered by the method layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer and in the method layer. The features
 * in the result layer have the (possibly clipped) areas of features
 * in the input layer and the attributes from the same features. The
 * schema of the result layer can be set by the user or, if it is
 * empty, is initialized to contain all fields in the input layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This method is the same as the C function OGR_L_Clip().
 * 
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Clip( OGRLayer *pLayerMethod, 
                       OGRLayer *pLayerResult, 
                       char** papszOptions, 
                       GDALProgressFunc pfnProgress, 
                       void * pProgressArg )
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnResult = NULL;
    OGRGeometry *pGeometryMethodFilter = NULL;
    int *mapInput = NULL;
    double progress_max = GetFeatureCount(0);
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, NULL, mapInput, NULL, 0, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    
    poDefnResult = pLayerResult->GetLayerDefn();
    ResetReading();
    while (OGRFeature *x = GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete x;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x);
        if (!x_geom) {
            delete x; 
            continue;
        }
        
        OGRGeometry *geom = NULL; // this will be the geometry of the result feature 
        pLayerMethod->ResetReading();
        // incrementally add area from y to geom
        while (OGRFeature *y = pLayerMethod->GetNextFeature()) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {delete y; continue;}
            if (!geom) {
                geom = y_geom->clone();
            } else {
                OGRGeometry *geom_new = geom->Union(y_geom);
                delete geom;
                geom = geom_new;
            }
            delete y;
        }

        // possibly add a new feature with area x intersection sum of y
        OGRFeature *z = NULL;
        if (geom) {
            OGRGeometry* poIntersection = x_geom->Intersection(geom);
            if( poIntersection != NULL && !poIntersection->IsEmpty() )
            {
                z = new OGRFeature(poDefnResult);
                z->SetFieldsFrom(x, mapInput);
                if( bPromoteToMulti )
                    poIntersection = promote_to_multi(poIntersection);
                z->SetGeometryDirectly(poIntersection);
            }
            else
                delete poIntersection;
            delete geom;
        }
        delete x;
        if (z) {
            if (z->GetGeometryRef() != NULL && !z->GetGeometryRef()->IsEmpty())
                ret = pLayerResult->CreateFeature(z);
            delete z;
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) {
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg)) {
      CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
      ret = OGRERR_FAILURE;
      goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter) delete pGeometryMethodFilter;
    if (mapInput) VSIFree(mapInput);
    return ret;
}

/************************************************************************/
/*                           OGR_L_Clip()                               */
/************************************************************************/

/**
 * \brief Clip off areas that are not covered by the method layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer and in the method layer. The features
 * in the result layer have the (possibly clipped) areas of features
 * in the input layer and the attributes from the same features. The
 * schema of the result layer can be set by the user or, if it is
 * empty, is initialized to contain all fields in the input layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Clip().
 * 
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Clip( OGRLayerH pLayerInput, 
                   OGRLayerH pLayerMethod, 
                   OGRLayerH pLayerResult, 
                   char** papszOptions, 
                   GDALProgressFunc pfnProgress, 
                   void * pProgressArg )

{
    VALIDATE_POINTER1( pLayerInput, "OGR_L_Clip", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerMethod, "OGR_L_Clip", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerResult, "OGR_L_Clip", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)pLayerInput)->Clip( (OGRLayer *)pLayerMethod, (OGRLayer *)pLayerResult, papszOptions, pfnProgress, pProgressArg );
}

/************************************************************************/
/*                              Erase()                                 */
/************************************************************************/

/**
 * \brief Remove areas that are covered by the method layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer but not in the method layer. The
 * features in the result layer have attributes from the input
 * layer. The schema of the result layer can be set by the user or, if
 * it is empty, is initialized to contain all fields in the input
 * layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This method is the same as the C function OGR_L_Erase().
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Erase( OGRLayer *pLayerMethod, 
                        OGRLayer *pLayerResult, 
                        char** papszOptions, 
                        GDALProgressFunc pfnProgress, 
                        void * pProgressArg )
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnResult = NULL;
    OGRGeometry *pGeometryMethodFilter = NULL;
    int *mapInput = NULL;
    double progress_max = GetFeatureCount(0);
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, NULL, mapInput, NULL, 0, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    ResetReading();
    while (OGRFeature *x = GetNextFeature()) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    delete x;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on the method layer
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x);
        if (!x_geom) {
            delete x; 
            continue;
        }

        OGRGeometry *geom = NULL; // this will be the geometry of the result feature
        pLayerMethod->ResetReading();
        // incrementally add area from y to geom
        while (OGRFeature *y = pLayerMethod->GetNextFeature()) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {delete y; continue;}
            if (!geom) {
                geom = y_geom->clone();
            } else {
                OGRGeometry *geom_new = geom->Union(y_geom);
                delete geom;
                geom = geom_new;
            }
            delete y;
        }

        // possibly add a new feature with area x minus sum of y
        OGRFeature *z = NULL;
        if (geom) {
            OGRGeometry* x_geom_diff = x_geom->Difference(geom);
            if( x_geom_diff != NULL && !x_geom_diff->IsEmpty() )
            {
                z = new OGRFeature(poDefnResult);
                z->SetFieldsFrom(x, mapInput);
                if( bPromoteToMulti )
                    x_geom_diff = promote_to_multi(x_geom_diff);
                z->SetGeometryDirectly(x_geom_diff);
            }
            else
                delete x_geom_diff;
            delete geom;
        }
        delete x;
        if (z) {
            if (z->GetGeometryRef() != NULL && !z->GetGeometryRef()->IsEmpty())
                ret = pLayerResult->CreateFeature(z);
            delete z;
            if (ret != OGRERR_NONE) {
                if (!bSkipFailures) {
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg)) {
      CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
      ret = OGRERR_FAILURE;
      goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter) delete pGeometryMethodFilter;
    if (mapInput) VSIFree(mapInput);
    return ret;
}

/************************************************************************/
/*                           OGR_L_Erase()                              */
/************************************************************************/

/**
 * \brief Remove areas that are covered by the method layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer but not in the method layer. The
 * features in the result layer have attributes from the input
 * layer. The schema of the result layer can be set by the user or, if
 * it is empty, is initialized to contain all fields in the input
 * layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Erase().
 * 
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Erase( OGRLayerH pLayerInput, 
                    OGRLayerH pLayerMethod, 
                    OGRLayerH pLayerResult, 
                    char** papszOptions, 
                    GDALProgressFunc pfnProgress, 
                    void * pProgressArg )

{
    VALIDATE_POINTER1( pLayerInput, "OGR_L_Erase", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerMethod, "OGR_L_Erase", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pLayerResult, "OGR_L_Erase", OGRERR_INVALID_HANDLE );

    return ((OGRLayer *)pLayerInput)->Erase( (OGRLayer *)pLayerMethod, (OGRLayer *)pLayerResult, papszOptions, pfnProgress, pProgressArg );
}
