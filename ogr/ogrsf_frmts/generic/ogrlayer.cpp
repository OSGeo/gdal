/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the OGRSFLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_swq.h"
#include "ograpispy.h"

CPL_CVSID("$Id$")

struct OGRLayer::Private
{
    bool         m_bInFeatureIterator = false;
};

/************************************************************************/
/*                              OGRLayer()                              */
/************************************************************************/

OGRLayer::OGRLayer() :
    m_poPrivate(new Private()),
    m_bFilterIsEnvelope(FALSE),
    m_poFilterGeom(nullptr),
    m_pPreparedFilterGeom(nullptr),
    m_sFilterEnvelope{},
    m_iGeomFieldFilter(0),
    m_poStyleTable(nullptr),
    m_poAttrQuery(nullptr),
    m_pszAttrQueryString(nullptr),
    m_poAttrIndex(nullptr),
    m_nRefCount(0),
    m_nFeaturesRead(0)
{}

/************************************************************************/
/*                             ~OGRLayer()                              */
/************************************************************************/

OGRLayer::~OGRLayer()

{
    if( m_poStyleTable )
    {
        delete m_poStyleTable;
        m_poStyleTable = nullptr;
    }

    if( m_poAttrIndex != nullptr )
    {
        delete m_poAttrIndex;
        m_poAttrIndex = nullptr;
    }

    if( m_poAttrQuery != nullptr )
    {
        delete m_poAttrQuery;
        m_poAttrQuery = nullptr;
    }

    CPLFree( m_pszAttrQueryString );

    if( m_poFilterGeom )
    {
        delete m_poFilterGeom;
        m_poFilterGeom = nullptr;
    }

    if( m_pPreparedFilterGeom != nullptr )
    {
        OGRDestroyPreparedGeometry(m_pPreparedFilterGeom);
        m_pPreparedFilterGeom = nullptr;
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

    return OGRLayer::FromHandle(hLayer)->Reference();
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

    return OGRLayer::FromHandle(hLayer)->Dereference();
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

    return OGRLayer::FromHandle(hLayer)->GetRefCount();
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig OGRLayer::GetFeatureCount( int bForce )

{
    if( !bForce )
        return -1;

    GIntBig nFeatureCount = 0;
    for( auto&& poFeature: *this )
    {
        CPL_IGNORE_RET_VAL(poFeature.get());
        nFeatureCount ++;
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

    return OGRLayer::FromHandle(hLayer)->GetFeatureCount(bForce);
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

//! @cond Doxygen_Suppress
OGRErr OGRLayer::GetExtentInternal(int iGeomField, OGREnvelope *psExtent, int bForce )

{
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
    OGREnvelope oEnv;
    bool bExtentSet = false;

    for( auto&& poFeature: *this )
    {
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(iGeomField);
        if (poGeom == nullptr || poGeom->IsEmpty())
        {
            /* Do nothing */
        }
        else if (!bExtentSet)
        {
            poGeom->getEnvelope(psExtent);
            if( !(CPLIsNan(psExtent->MinX) || CPLIsNan(psExtent->MinY) ||
                  CPLIsNan(psExtent->MaxX) || CPLIsNan(psExtent->MaxY)) )
            {
                bExtentSet = true;
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
    }
    ResetReading();

    return bExtentSet ? OGRERR_NONE : OGRERR_FAILURE;
}
//! @endcond

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

    return OGRLayer::FromHandle(hLayer)->GetExtent( psExtent, bForce );
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

    return OGRLayer::FromHandle(hLayer)->GetExtent( iGeomField, psExtent, bForce );
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : nullptr;

/* -------------------------------------------------------------------- */
/*      Are we just clearing any existing query?                        */
/* -------------------------------------------------------------------- */
    if( pszQuery == nullptr || strlen(pszQuery) == 0 )
    {
        if( m_poAttrQuery )
        {
            delete m_poAttrQuery;
            m_poAttrQuery = nullptr;
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

    eErr = m_poAttrQuery->Compile( this, pszQuery );
    if( eErr != OGRERR_NONE )
    {
        delete m_poAttrQuery;
        m_poAttrQuery = nullptr;
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

//! @cond Doxygen_Suppress
int OGRLayer::AttributeFilterEvaluationNeedsGeometry()
{
    if( !m_poAttrQuery )
        return FALSE;

    swq_expr_node* expr = static_cast<swq_expr_node *>(
        m_poAttrQuery->GetSWQExpr());
    int nLayerFieldCount = GetLayerDefn()->GetFieldCount();

    return ContainGeomSpecialField(expr, nLayerFieldCount);
}
//! @endcond

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

    return OGRLayer::FromHandle(hLayer)->SetAttributeFilter( pszQuery );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRLayer::GetFeature( GIntBig nFID )

{
    /* Save old attribute and spatial filters */
    char* pszOldFilter = m_pszAttrQueryString ? CPLStrdup(m_pszAttrQueryString) : nullptr;
    OGRGeometry* poOldFilterGeom = ( m_poFilterGeom != nullptr ) ? m_poFilterGeom->clone() : nullptr;
    int iOldGeomFieldFilter = m_iGeomFieldFilter;
    /* Unset filters */
    SetAttributeFilter(nullptr);
    SetSpatialFilter(0, nullptr);

    OGRFeatureUniquePtr poFeature;
    for( auto&& poFeatureIter: *this )
    {
        if( poFeatureIter->GetFID() == nFID )
        {
            poFeature.swap(poFeatureIter);
            break;
        }
    }

    /* Restore filters */
    SetAttributeFilter(pszOldFilter);
    CPLFree(pszOldFilter);
    SetSpatialFilter(iOldGeomFieldFilter, poOldFilterGeom);
    delete poOldFilterGeom;

    return poFeature.release();
}

/************************************************************************/
/*                          OGR_L_GetFeature()                          */
/************************************************************************/

OGRFeatureH OGR_L_GetFeature( OGRLayerH hLayer, GIntBig nFeatureId )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetFeature", nullptr );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetFeature(hLayer, nFeatureId);
#endif

    return OGRFeature::ToHandle(
            OGRLayer::FromHandle(hLayer)->GetFeature( nFeatureId ));
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRLayer::SetNextByIndex( GIntBig nIndex )

{
    if( nIndex < 0 )
        return OGRERR_FAILURE;

    ResetReading();

    OGRFeature *poFeature = nullptr;
    while( nIndex-- > 0 )
    {
        poFeature = GetNextFeature();
        if( poFeature == nullptr )
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

    return OGRLayer::FromHandle(hLayer)->SetNextByIndex( nIndex );
}

/************************************************************************/
/*                        OGR_L_GetNextFeature()                        */
/************************************************************************/

OGRFeatureH OGR_L_GetNextFeature( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetNextFeature", nullptr );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetNextFeature(hLayer);
#endif

    return OGRFeature::ToHandle(
                OGRLayer::FromHandle(hLayer)->GetNextFeature());
}

/************************************************************************/
/*                       ConvertGeomsIfNecessary()                      */
/************************************************************************/

void OGRLayer::ConvertGeomsIfNecessary( OGRFeature *poFeature )
{
    const bool bSupportsCurve = CPL_TO_BOOL(TestCapability(OLCCurveGeometries));
    const bool bSupportsM = CPL_TO_BOOL(TestCapability(OLCMeasuredGeometries));
    if( !bSupportsCurve || !bSupportsM )
    {
        int nGeomFieldCount = GetLayerDefn()->GetGeomFieldCount();
        for(int i=0;i<nGeomFieldCount;i++)
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom != nullptr && (!bSupportsM && OGR_GT_HasM(poGeom->getGeometryType())) )
            {
                poGeom->setMeasured(FALSE);
            }
            if( poGeom != nullptr && (!bSupportsCurve && OGR_GT_IsNonLinear(poGeom->getGeometryType())) )
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
    ConvertGeomsIfNecessary(poFeature);
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

    return OGRLayer::FromHandle(hLayer)->SetFeature( OGRFeature::FromHandle(hFeat) );
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRLayer::CreateFeature( OGRFeature *poFeature )

{
    ConvertGeomsIfNecessary(poFeature);
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

    return OGRLayer::FromHandle(hLayer)->CreateFeature( OGRFeature::FromHandle(hFeat) );
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

    return OGRLayer::FromHandle(hLayer)->CreateField( OGRFieldDefn::FromHandle(hField),
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

    return OGRLayer::FromHandle(hLayer)->DeleteField( iField );
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

    return OGRLayer::FromHandle(hLayer)->ReorderFields( panMap );
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

    int* panMap = static_cast<int*>(CPLMalloc(sizeof(int) * nFieldCount));
    if (iOldFieldPos < iNewFieldPos)
    {
        /* "0","1","2","3","4" (1,3) -> "0","2","3","1","4" */
        int i = 0;  // Used after for.
        for( ; i < iOldFieldPos; i++ )
            panMap[i] = i;
        for( ; i < iNewFieldPos; i++ )
            panMap[i] = i + 1;
        panMap[iNewFieldPos] = iOldFieldPos;
        for( i = iNewFieldPos + 1; i < nFieldCount; i++ )
            panMap[i] = i;
    }
    else
    {
        /* "0","1","2","3","4" (3,1) -> "0","3","1","2","4" */
        for( int i = 0; i < iNewFieldPos; i++ )
            panMap[i] = i;
        panMap[iNewFieldPos] = iOldFieldPos;
        int i = iNewFieldPos+1;  // Used after for.
        for( ; i <= iOldFieldPos; i++ )
            panMap[i] = i - 1;
        for( ; i < nFieldCount; i++ )
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

    return OGRLayer::FromHandle(hLayer)->ReorderField( iOldFieldPos, iNewFieldPos );
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRLayer::AlterFieldDefn( int /* iField*/,
                                 OGRFieldDefn* /*poNewFieldDefn*/,
                                 int /* nFlags */ )

{
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

    return OGRLayer::FromHandle(hLayer)->AlterFieldDefn( iField,
                            OGRFieldDefn::FromHandle(hNewFieldDefn), nFlags );
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

    return OGRLayer::FromHandle(hLayer)->CreateGeomField(
        OGRGeomFieldDefn::FromHandle(hField), bApproxOK );
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

    return OGRLayer::FromHandle(hLayer)->StartTransaction();
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

    return OGRLayer::FromHandle(hLayer)->CommitTransaction();
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

    return OGRLayer::FromHandle(hLayer)->RollbackTransaction();
}

/************************************************************************/
/*                         OGR_L_GetLayerDefn()                         */
/************************************************************************/

OGRFeatureDefnH OGR_L_GetLayerDefn( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetLayerDefn", nullptr );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetLayerDefn(hLayer);
#endif

    return OGRFeatureDefn::ToHandle(
            OGRLayer::FromHandle(hLayer)->GetLayerDefn());
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

    return OGRLayer::FromHandle(hLayer)->FindFieldIndex( pszFieldName, bExactMatch );
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
        return nullptr;
}

/************************************************************************/
/*                        OGR_L_GetSpatialRef()                         */
/************************************************************************/

OGRSpatialReferenceH OGR_L_GetSpatialRef( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetSpatialRef", nullptr );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetSpatialRef(hLayer);
#endif

    return OGRSpatialReference::ToHandle(
            OGRLayer::FromHandle(hLayer)->GetSpatialRef());
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

    return OGRLayer::FromHandle(hLayer)->TestCapability( pszCap );
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
    VALIDATE_POINTER1( hLayer, "OGR_L_GetSpatialFilter", nullptr );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetSpatialFilter(hLayer);
#endif

    return OGRGeometry::ToHandle(
            OGRLayer::FromHandle(hLayer)->GetSpatialFilter());
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

    OGRLayer::FromHandle(hLayer)->SetSpatialFilter(
        OGRGeometry::FromHandle(hGeom) );
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

    OGRLayer::FromHandle(hLayer)->SetSpatialFilter( iGeomField,
                                            OGRGeometry::FromHandle(hGeom) );
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

    OGRLayer::FromHandle(hLayer)->SetSpatialFilterRect( dfMinX, dfMinY,
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

    OGRLayer::FromHandle(hLayer)->SetSpatialFilterRect( iGeomField,
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

//! @cond Doxygen_Suppress
int OGRLayer::InstallFilter( OGRGeometry * poFilter )

{
    if( m_poFilterGeom == poFilter )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Replace the existing filter.                                    */
/* -------------------------------------------------------------------- */
    if( m_poFilterGeom != nullptr )
    {
        delete m_poFilterGeom;
        m_poFilterGeom = nullptr;
    }

    if( m_pPreparedFilterGeom != nullptr )
    {
        OGRDestroyPreparedGeometry(m_pPreparedFilterGeom);
        m_pPreparedFilterGeom = nullptr;
    }

    if( poFilter != nullptr )
        m_poFilterGeom = poFilter->clone();

    m_bFilterIsEnvelope = FALSE;

    if( m_poFilterGeom == nullptr )
        return TRUE;

    m_poFilterGeom->getEnvelope( &m_sFilterEnvelope );

    /* Compile geometry filter as a prepared geometry */
    m_pPreparedFilterGeom = OGRCreatePreparedGeometry(OGRGeometry::ToHandle(m_poFilterGeom));

/* -------------------------------------------------------------------- */
/*      Now try to determine if the filter is really a rectangle.       */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(m_poFilterGeom->getGeometryType()) != wkbPolygon )
        return TRUE;

    OGRPolygon *poPoly = m_poFilterGeom->toPolygon();

    if( poPoly->getNumInteriorRings() != 0 )
        return TRUE;

    OGRLinearRing *poRing = poPoly->getExteriorRing();
    if (poRing == nullptr)
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
//! @endcond

/************************************************************************/
/*                           FilterGeometry()                           */
/*                                                                      */
/*      Compare the passed in geometry to the currently installed       */
/*      filter.  Optimize for case where filter is just an              */
/*      envelope.                                                       */
/************************************************************************/

//! @cond Doxygen_Suppress
int OGRLayer::FilterGeometry( OGRGeometry *poGeometry )

{
/* -------------------------------------------------------------------- */
/*      In trivial cases of new filter or target geometry, we accept    */
/*      an intersection.  No geometry is taken to mean "the whole       */
/*      world".                                                         */
/* -------------------------------------------------------------------- */
    if( m_poFilterGeom == nullptr )
        return TRUE;

    if( poGeometry == nullptr || poGeometry->IsEmpty() )
        return FALSE;

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
            OGRLineString* poLS = nullptr;

            switch( wkbFlatten(poGeometry->getGeometryType()) )
            {
                case wkbPolygon:
                {
                    OGRPolygon* poPoly = poGeometry->toPolygon();
                    OGRLinearRing* poRing = poPoly->getExteriorRing();
                    if (poRing != nullptr && poPoly->getNumInteriorRings() == 0)
                    {
                        poLS = poRing;
                    }
                    break;
                }

                case wkbLineString:
                    poLS = poGeometry->toLineString();
                    break;

                default:
                    break;
            }

            if( poLS != nullptr )
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
            if( m_pPreparedFilterGeom != nullptr )
                return OGRPreparedGeometryIntersects(m_pPreparedFilterGeom,
                                                     OGRGeometry::ToHandle(poGeometry));
            else
                return m_poFilterGeom->Intersects( poGeometry );
        }
        else
            return TRUE;
    }
}
//! @endcond

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

    OGRLayer::FromHandle(hLayer)->ResetReading();
}

/************************************************************************/
/*                       InitializeIndexSupport()                       */
/*                                                                      */
/*      This is only intended to be called by driver layer              */
/*      implementations but we don't make it protected so that the      */
/*      datasources can do it too if that is more appropriate.          */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRErr OGRLayer::InitializeIndexSupport( const char *pszFilename )

{
    OGRErr eErr;

    if (m_poAttrIndex != nullptr)
        return OGRERR_NONE;

    m_poAttrIndex = OGRCreateDefaultLayerIndex();

    eErr = m_poAttrIndex->Initialize( pszFilename, this );
    if( eErr != OGRERR_NONE )
    {
        delete m_poAttrIndex;
        m_poAttrIndex = nullptr;
    }

    return eErr;
}
//! @endcond

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

    return OGRLayer::FromHandle(hLayer)->SyncToDisk();
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

    return OGRLayer::FromHandle(hLayer)->DeleteFeature( nFID );
}

/************************************************************************/
/*                          GetFeaturesRead()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
GIntBig OGRLayer::GetFeaturesRead()

{
    return m_nFeaturesRead;
}
//! @endcond

/************************************************************************/
/*                       OGR_L_GetFeaturesRead()                        */
/************************************************************************/

GIntBig OGR_L_GetFeaturesRead( OGRLayerH hLayer )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_GetFeaturesRead", 0 );

    return OGRLayer::FromHandle(hLayer)->GetFeaturesRead();
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
    VALIDATE_POINTER1( hLayer, "OGR_L_GetFIDColumn", nullptr );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetFIDColumn(hLayer);
#endif

    return OGRLayer::FromHandle(hLayer)->GetFIDColumn();
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
    VALIDATE_POINTER1( hLayer, "OGR_L_GetGeometryColumn", nullptr );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_L_GetGeometryColumn(hLayer);
#endif

    return OGRLayer::FromHandle(hLayer)->GetGeometryColumn();
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
    VALIDATE_POINTER1( hLayer, "OGR_L_GetStyleTable", nullptr );

    return reinterpret_cast<OGRStyleTableH>(
        OGRLayer::FromHandle(hLayer)->GetStyleTable( ));
}

/************************************************************************/
/*                         OGR_L_SetStyleTableDirectly()                */
/************************************************************************/

void OGR_L_SetStyleTableDirectly( OGRLayerH hLayer,
                                  OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetStyleTableDirectly" );

    OGRLayer::FromHandle(hLayer)->SetStyleTableDirectly(
        reinterpret_cast<OGRStyleTable *>(hStyleTable) );
}

/************************************************************************/
/*                         OGR_L_SetStyleTable()                        */
/************************************************************************/

void OGR_L_SetStyleTable( OGRLayerH hLayer,
                          OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hLayer, "OGR_L_SetStyleTable" );
    VALIDATE_POINTER0( hStyleTable, "OGR_L_SetStyleTable" );

    OGRLayer::FromHandle(hLayer)->SetStyleTable(
        reinterpret_cast<OGRStyleTable *>(hStyleTable) );
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

    return OGRLayer::FromHandle(hLayer)->GetName();
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

OGRwkbGeometryType OGRLayer::GetGeomType()
{
    OGRFeatureDefn* poLayerDefn = GetLayerDefn();
    if( poLayerDefn == nullptr )
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

    OGRwkbGeometryType eType = OGRLayer::FromHandle(hLayer)->GetGeomType();
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
    for( int iField = 0; iField < poDefn->GetGeomFieldCount(); iField++ )
    {
        poDefn->GetGeomFieldDefn(iField)->SetIgnored( FALSE );
    }
    poDefn->SetStyleIgnored( FALSE );

    if ( papszFields == nullptr )
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

    return OGRLayer::FromHandle(hLayer)->SetIgnoredFields( papszFields );
}

/************************************************************************/
/*                             Rename()                                 */
/************************************************************************/

/** Rename layer.
 *
 * This operation is implemented only by layers that expose the OLCRename capability,
 * and drivers that expose the GDAL_DCAP_RENAME_LAYERS capability
 *
 * This operation will fail if a layer with the new name already exists.
 *
 * On success, GetDescription() and GetLayerDefn()->GetName() will return
 * pszNewName.
 *
 * Renaming the layer may interrupt current feature iteration.
 *
 * @param pszNewName New layer name. Must not be NULL.
 * @return OGRERR_NONE in case of success
 *
 * @since GDAL 3.5
 */
OGRErr OGRLayer::Rename( CPL_UNUSED const char* pszNewName )
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Rename() not supported by this layer.");

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                           OGR_L_Rename()                             */
/************************************************************************/

/** Rename layer.
 *
 * This operation is implemented only by layers that expose the OLCRename capability,
 * and drivers that expose the GDAL_DCAP_RENAME_LAYERS capability
 *
 * This operation will fail if a layer with the new name already exists.
 *
 * On success, GetDescription() and GetLayerDefn()->GetName() will return
 * pszNewName.
 *
 * Renaming the layer may interrupt current feature iteration.
 *
 * @param hLayer     Layer to rename.
 * @param pszNewName New layer name. Must not be NULL.
 * @return OGRERR_NONE in case of success
 *
 * @since GDAL 3.5
 */
OGRErr OGR_L_Rename( OGRLayerH hLayer, const char* pszNewName )

{
    VALIDATE_POINTER1( hLayer, "OGR_L_Rename", OGRERR_INVALID_HANDLE );
    VALIDATE_POINTER1( pszNewName, "OGR_L_Rename", OGRERR_FAILURE );

    return OGRLayer::FromHandle(hLayer)->Rename( pszNewName );
}

/************************************************************************/
/*         helper functions for layer overlay methods                   */
/************************************************************************/

static
OGRErr clone_spatial_filter(OGRLayer *pLayer, OGRGeometry **ppGeometry)
{
    OGRErr ret = OGRERR_NONE;
    OGRGeometry *g = pLayer->GetSpatialFilter();
    *ppGeometry = g ? g->clone() : nullptr;
    return ret;
}

static
OGRErr create_field_map(OGRFeatureDefn *poDefn, int **map)
{
    OGRErr ret = OGRERR_NONE;
    int n = poDefn->GetFieldCount();
    if (n > 0) {
        *map = static_cast<int*>(VSI_MALLOC_VERBOSE(sizeof(int) * n));
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
                         bool combined,
                         const char* const* papszOptions)
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnResult = pLayerResult->GetLayerDefn();
    const char* pszInputPrefix = CSLFetchNameValue(papszOptions, "INPUT_PREFIX");
    const char* pszMethodPrefix = CSLFetchNameValue(papszOptions, "METHOD_PREFIX");
    int bSkipFailures = CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    if (poDefnResult->GetFieldCount() > 0) {
        // the user has defined the schema of the output layer
        if( mapInput )
        {
            for( int iField = 0; iField < poDefnInput->GetFieldCount(); iField++ ) {
                CPLString osName(poDefnInput->GetFieldDefn(iField)->GetNameRef());
                if( pszInputPrefix != nullptr )
                    osName = pszInputPrefix + osName;
                mapInput[iField] = poDefnResult->GetFieldIndex(osName);
            }
        }
        if (!mapMethod) return ret;
        // cppcheck-suppress nullPointer
        for( int iField = 0; iField < poDefnMethod->GetFieldCount(); iField++ ) {
            // cppcheck-suppress nullPointer
            CPLString osName(poDefnMethod->GetFieldDefn(iField)->GetNameRef());
            if( pszMethodPrefix != nullptr )
                osName = pszMethodPrefix + osName;
            mapMethod[iField] = poDefnResult->GetFieldIndex(osName);
        }
    } else {
        // use schema from the input layer or from input and method layers
        int nFieldsInput = poDefnInput->GetFieldCount();
        for( int iField = 0; iField < nFieldsInput; iField++ ) {
            OGRFieldDefn oFieldDefn(poDefnInput->GetFieldDefn(iField));
            if( pszInputPrefix != nullptr )
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
            if( mapInput )
                mapInput[iField] = iField;
        }
        if (!combined) return ret;
        if (!mapMethod) return ret;
        if (!poDefnMethod) return ret;
        for( int iField = 0; iField < poDefnMethod->GetFieldCount(); iField++ ) {
            OGRFieldDefn oFieldDefn(poDefnMethod->GetFieldDefn(iField));
            if( pszMethodPrefix != nullptr )
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
    if (!geom) return nullptr;
    if (pGeometryExistingFilter) {
        if (!geom->Intersects(pGeometryExistingFilter)) return nullptr;
        OGRGeometry *intersection = geom->Intersection(pGeometryExistingFilter);
        if (intersection) {
            pLayer->SetSpatialFilter(intersection);
            delete intersection;
        } else
            return nullptr;
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
 * The recognized list of options is:
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * <li>PRETEST_CONTAINMENT=YES/NO. Set to YES to pretest the
 *     containment of features of method layer within the features of
 *     this layer. This will speed up the method significantly in some
 *     cases. Requires that the prepared geometries are in effect.
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is to add
 *     but only if the result layer has an unknown geometry type.
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
 * @note The first geometry field is always used.
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
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    OGREnvelope sEnvelopeMethod;
    GBool bEnvelopeSet;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CPLTestBool(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));
    int bUsePreparedGeometries = CPLTestBool(CSLFetchNameValueDef(papszOptions, "USE_PREPARED_GEOMETRIES", "YES"));
    if (bUsePreparedGeometries) bUsePreparedGeometries = OGRHasPreparedGeometrySupport();
    int bPretestContainment = CPLTestBool(CSLFetchNameValueDef(papszOptions, "PRETEST_CONTAINMENT", "NO"));
    int bKeepLowerDimGeom = CPLTestBool(CSLFetchNameValueDef(papszOptions, "KEEP_LOWER_DIMENSION_GEOMETRIES", "YES"));

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
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, true, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();
    bEnvelopeSet = pLayerMethod->GetExtent(&sEnvelopeMethod, 1) == OGRERR_NONE;
    if (bKeepLowerDimGeom) {
        // require that the result layer is of geom type unknown
        if (pLayerResult->GetGeomType() != wkbUnknown) {
            CPLDebug("OGR", "Resetting KEEP_LOWER_DIMENSION_GEOMETRIES to NO since the result layer does not allow it.");
            bKeepLowerDimGeom = FALSE;
        }
    }

    for( auto&& x: this ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
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
                    continue;
                }
            } else {
                continue;
            }
        }

        // set up the filter for method layer
        CPLErrorReset();
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None) {
            if (!bSkipFailures) {
                ret = OGRERR_FAILURE;
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom) {
            continue;
        }

        OGRPreparedGeometryUniquePtr x_prepared_geom;
        if (bUsePreparedGeometries) {
            x_prepared_geom.reset(OGRCreatePreparedGeometry(OGRGeometry::ToHandle(x_geom)));
            if (!x_prepared_geom) {
                goto done;
            }
        }

        for( auto&& y: pLayerMethod ) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) continue;
            OGRGeometryUniquePtr z_geom;

            if (x_prepared_geom) {
                CPLErrorReset();
                ret = OGRERR_NONE;
                if (bPretestContainment && OGRPreparedGeometryContains(x_prepared_geom.get(), OGRGeometry::ToHandle(y_geom)))
                {
                    if (CPLGetLastErrorType() == CE_None)
                        z_geom.reset(y_geom->clone());
                }
                else if (!(OGRPreparedGeometryIntersects(x_prepared_geom.get(), OGRGeometry::ToHandle(y_geom))))
                {
                    if (CPLGetLastErrorType() == CE_None) {
                        continue;
                    }
                }
                if (CPLGetLastErrorType() != CE_None) {
                    if (!bSkipFailures) {
                        ret = OGRERR_FAILURE;
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                        continue;
                    }
                }
            }
            if (!z_geom) {
                CPLErrorReset();
                z_geom.reset(x_geom->Intersection(y_geom));
                if (CPLGetLastErrorType() != CE_None || z_geom == nullptr) {
                    if (!bSkipFailures) {
                        ret = OGRERR_FAILURE;
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                        continue;
                    }
                }
                if (z_geom->IsEmpty() ||
                    (!bKeepLowerDimGeom &&
                     (x_geom->getDimension() == y_geom->getDimension() &&
                      z_geom->getDimension() < x_geom->getDimension())))
                {
                    continue;
                }
            }
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            z->SetFieldsFrom(y.get(), mapMethod);
            if (bPromoteToMulti)
                z_geom.reset(promote_to_multi(z_geom.release()));
            z->SetGeometryDirectly(z_geom.release());
            ret = pLayerResult->CreateFeature(z.get());

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
 *     feature could not be inserted or a GEOS call failed.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * <li>PRETEST_CONTAINMENT=YES/NO. Set to YES to pretest the
 *     containment of features of method layer within the features of
 *     this layer. This will speed up the method significantly in some
 *     cases. Requires that the prepared geometries are in effect.
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is to add
 *     but only if the result layer has an unknown geometry type.
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
 * @note The first geometry field is always used.
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

    return OGRLayer::FromHandle(pLayerInput)->Intersection(
        OGRLayer::FromHandle(pLayerMethod),
        OGRLayer::FromHandle(pLayerResult),
        papszOptions, pfnProgress, pProgressArg );
}

/************************************************************************/
/*                              Union()                                 */
/************************************************************************/

/**
 * \brief Union of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are either in the input layer, in the method layer, or in
 * both. The features in the result layer have attributes from both
 * input and method layers. For features which represent areas that
 * are only in the input or in the method layer the respective
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
 *     feature could not be inserted or a GEOS call failed.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is to add
 *     but only if the result layer has an unknown geometry type.
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
 * @note The first geometry field is always used.
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
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    OGRGeometry *pGeometryInputFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE)) + static_cast<double>(pLayerMethod->GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CPLTestBool(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));
    int bUsePreparedGeometries = CPLTestBool(CSLFetchNameValueDef(papszOptions, "USE_PREPARED_GEOMETRIES", "YES"));
    if (bUsePreparedGeometries) bUsePreparedGeometries = OGRHasPreparedGeometrySupport();
    int bKeepLowerDimGeom = CPLTestBool(CSLFetchNameValueDef(papszOptions, "KEEP_LOWER_DIMENSION_GEOMETRIES", "YES"));

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
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, true, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();
    if (bKeepLowerDimGeom) {
        // require that the result layer is of geom type unknown
        if (pLayerResult->GetGeomType() != wkbUnknown) {
            CPLDebug("OGR", "Resetting KEEP_LOWER_DIMENSION_GEOMETRIES to NO since the result layer does not allow it.");
            bKeepLowerDimGeom = FALSE;
        }
    }

    // add features based on input layer
    for( auto&& x: this ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None) {
            if (!bSkipFailures) {
                ret = OGRERR_FAILURE;
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom) {
            continue;
        }

        OGRPreparedGeometryUniquePtr x_prepared_geom;
        if (bUsePreparedGeometries) {
            x_prepared_geom.reset(OGRCreatePreparedGeometry(OGRGeometry::ToHandle(x_geom)));
            if (!x_prepared_geom) {
                goto done;
            }
        }

        OGRGeometryUniquePtr x_geom_diff(x_geom->clone()); // this will be the geometry of the result feature
        for( auto&& y: pLayerMethod) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) { continue;}

            CPLErrorReset();
            if (x_prepared_geom && !(OGRPreparedGeometryIntersects(x_prepared_geom.get(), OGRGeometry::ToHandle(y_geom)))) {
                if (CPLGetLastErrorType() == CE_None) {
                    continue;
                }
            }
            if (CPLGetLastErrorType() != CE_None) {
                if (!bSkipFailures) {
                    ret = OGRERR_FAILURE;
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }

            CPLErrorReset();
            OGRGeometryUniquePtr poIntersection(x_geom->Intersection(y_geom));
            if (CPLGetLastErrorType() != CE_None || poIntersection == nullptr) {
                if (!bSkipFailures) {
                    ret = OGRERR_FAILURE;
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                    continue;
                }
            }
            if( poIntersection->IsEmpty() ||
                (!bKeepLowerDimGeom &&
                 (x_geom->getDimension() == y_geom->getDimension() &&
                  poIntersection->getDimension() < x_geom->getDimension())) )
            {
                // ok
            }
            else
            {
                OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
                z->SetFieldsFrom(x.get(), mapInput);
                z->SetFieldsFrom(y.get(), mapMethod);
                if( bPromoteToMulti )
                    poIntersection.reset(promote_to_multi(poIntersection.release()));
                z->SetGeometryDirectly(poIntersection.release());

                if (x_geom_diff) {
                    CPLErrorReset();
                    OGRGeometryUniquePtr x_geom_diff_new(x_geom_diff->Difference(y_geom));
                    if (CPLGetLastErrorType() != CE_None || x_geom_diff_new == nullptr) {
                        if (!bSkipFailures) {
                            ret = OGRERR_FAILURE;
                            goto done;
                        } else {
                            CPLErrorReset();
                        }
                    } else {
                        x_geom_diff.swap(x_geom_diff_new);
                    }
                }

                ret = pLayerResult->CreateFeature(z.get());
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
        x_prepared_geom.reset();

        if( x_geom_diff == nullptr || x_geom_diff->IsEmpty() )
        {
            // ok
        }
        else
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if( bPromoteToMulti )
                x_geom_diff.reset(promote_to_multi(x_geom_diff.release()));
            z->SetGeometryDirectly(x_geom_diff.release());
            ret = pLayerResult->CreateFeature(z.get());
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
    for( auto&& x: pLayerMethod ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on input layer
        CPLErrorReset();
        OGRGeometry *x_geom = set_filter_from(this, pGeometryInputFilter, x.get());
        if (CPLGetLastErrorType() != CE_None) {
            if (!bSkipFailures) {
                ret = OGRERR_FAILURE;
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom) {
            continue;
        }

        OGRGeometryUniquePtr x_geom_diff(x_geom->clone()); // this will be the geometry of the result feature
        for( auto&& y: this ) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) { continue;}

            if (x_geom_diff) {
                CPLErrorReset();
                OGRGeometryUniquePtr x_geom_diff_new(x_geom_diff->Difference(y_geom));
                if (CPLGetLastErrorType() != CE_None || x_geom_diff_new == nullptr) {
                    if (!bSkipFailures) {
                        ret = OGRERR_FAILURE;
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                } else {
                    x_geom_diff.swap(x_geom_diff_new);
                }
            }
        }

        if( x_geom_diff == nullptr || x_geom_diff->IsEmpty() )
        {
            // ok
        }
        else
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapMethod);
            if( bPromoteToMulti )
                x_geom_diff.reset(promote_to_multi(x_geom_diff.release()));
            z->SetGeometryDirectly(x_geom_diff.release());
            ret = pLayerResult->CreateFeature(z.get());
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
 * that are in either in the input layer, in the method layer, or in
 * both. The features in the result layer have attributes from both
 * input and method layers. For features which represent areas that
 * are only in the input or in the method layer the respective
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
 *     feature could not be inserted or a GEOS call failed.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is to add
 *     but only if the result layer has an unknown geometry type.
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
 * @note The first geometry field is always used.
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

    return OGRLayer::FromHandle(pLayerInput)->Union(
        OGRLayer::FromHandle(pLayerMethod),
        OGRLayer::FromHandle(pLayerResult),
        papszOptions, pfnProgress, pProgressArg );
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
 *     feature could not be inserted or a GEOS call failed.
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
 * @note The first geometry field is always used.
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
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    OGRGeometry *pGeometryInputFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE)) + static_cast<double>(pLayerMethod->GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CPLTestBool(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

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
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, true, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // add features based on input layer
    for( auto&& x: this ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None) {
            if (!bSkipFailures) {
                ret = OGRERR_FAILURE;
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom) {
            continue;
        }

        OGRGeometryUniquePtr geom(x_geom->clone()); // this will be the geometry of the result feature
        for( auto&& y: pLayerMethod ) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) {continue;}
            if (geom) {
                CPLErrorReset();
                OGRGeometryUniquePtr geom_new(geom->Difference(y_geom));
                if (CPLGetLastErrorType() != CE_None || geom_new == nullptr) {
                    if (!bSkipFailures) {
                        ret = OGRERR_FAILURE;
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                } else {
                    geom.swap(geom_new);
                }
            }
            if (geom && geom->IsEmpty()) break;
        }

        if (geom && !geom->IsEmpty()) {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if( bPromoteToMulti )
                geom.reset(promote_to_multi(geom.release()));
            z->SetGeometryDirectly(geom.release());
            ret = pLayerResult->CreateFeature(z.get());
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
    for( auto&& x: pLayerMethod ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on input layer
        CPLErrorReset();
        OGRGeometry *x_geom = set_filter_from(this, pGeometryInputFilter, x.get());
        if (CPLGetLastErrorType() != CE_None) {
            if (!bSkipFailures) {
                ret = OGRERR_FAILURE;
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom) {
            continue;
        }

        OGRGeometryUniquePtr geom(x_geom->clone()); // this will be the geometry of the result feature
        for( auto&& y: this ) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) continue;
            if (geom) {
                CPLErrorReset();
                OGRGeometryUniquePtr geom_new(geom->Difference(y_geom));
                if (CPLGetLastErrorType() != CE_None || geom_new == nullptr) {
                    if (!bSkipFailures) {
                        ret = OGRERR_FAILURE;
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                } else {
                    geom.swap(geom_new);
                }
            }
            if (geom == nullptr || geom->IsEmpty()) break;
        }

        if (geom && !geom->IsEmpty()) {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapMethod);
            if( bPromoteToMulti )
                geom.reset(promote_to_multi(geom.release()));
            z->SetGeometryDirectly(geom.release());
            ret = pLayerResult->CreateFeature(z.get());
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
 *     feature could not be inserted or a GEOS call failed.
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
 * @note The first geometry field is always used.
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

    return OGRLayer::FromHandle(pLayerInput)->SymDifference(
        OGRLayer::FromHandle(pLayerMethod),
        OGRLayer::FromHandle(pLayerResult),
        papszOptions, pfnProgress, pProgressArg );
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
 *     feature could not be inserted or a GEOS call failed.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is to add
 *     but only if the result layer has an unknown geometry type.
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
 * @note The first geometry field is always used.
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
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CPLTestBool(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));
    int bUsePreparedGeometries = CPLTestBool(CSLFetchNameValueDef(papszOptions, "USE_PREPARED_GEOMETRIES", "YES"));
    if (bUsePreparedGeometries) bUsePreparedGeometries = OGRHasPreparedGeometrySupport();
    int bKeepLowerDimGeom = CPLTestBool(CSLFetchNameValueDef(papszOptions, "KEEP_LOWER_DIMENSION_GEOMETRIES", "YES"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }
    if (bKeepLowerDimGeom) {
        // require that the result layer is of geom type unknown
        if (pLayerResult->GetGeomType() != wkbUnknown) {
            CPLDebug("OGR", "Resetting KEEP_LOWER_DIMENSION_GEOMETRIES to NO since the result layer does not allow it.");
            bKeepLowerDimGeom = FALSE;
        }
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, true, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // split the features in input layer to the result layer
    for( auto&& x: this ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None) {
            if (!bSkipFailures) {
                ret = OGRERR_FAILURE;
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom) {
            continue;
        }

        OGRPreparedGeometryUniquePtr x_prepared_geom;
        if (bUsePreparedGeometries) {
            x_prepared_geom.reset(OGRCreatePreparedGeometry(OGRGeometry::ToHandle(x_geom)));
            if (!x_prepared_geom) {
                goto done;
            }
        }

        OGRGeometryUniquePtr x_geom_diff(x_geom->clone()); // this will be the geometry of the result feature
        for( auto&& y: pLayerMethod ) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
                continue;

            CPLErrorReset();
            if (x_prepared_geom && !(OGRPreparedGeometryIntersects(x_prepared_geom.get(), OGRGeometry::ToHandle(y_geom)))) {
                if (CPLGetLastErrorType() == CE_None) {
                    continue;
                }
            }
            if (CPLGetLastErrorType() != CE_None) {
                if (!bSkipFailures) {
                    ret = OGRERR_FAILURE;
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }

            CPLErrorReset();
            OGRGeometryUniquePtr poIntersection(x_geom->Intersection(y_geom));
            if (CPLGetLastErrorType() != CE_None || poIntersection == nullptr) {
                if (!bSkipFailures) {
                    ret = OGRERR_FAILURE;
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
            else if( poIntersection->IsEmpty() ||
                     (!bKeepLowerDimGeom &&
                      (x_geom->getDimension() == y_geom->getDimension() &&
                       poIntersection->getDimension() < x_geom->getDimension())) )
            {
                /* ok*/
            }
            else
            {
                OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
                z->SetFieldsFrom(x.get(), mapInput);
                z->SetFieldsFrom(y.get(), mapMethod);
                if( bPromoteToMulti )
                    poIntersection.reset(promote_to_multi(poIntersection.release()));
                z->SetGeometryDirectly(poIntersection.release());
                if (x_geom_diff) {
                    CPLErrorReset();
                    OGRGeometryUniquePtr x_geom_diff_new(x_geom_diff->Difference(y_geom));
                    if (CPLGetLastErrorType() != CE_None || x_geom_diff_new == nullptr) {
                        if (!bSkipFailures) {
                            ret = OGRERR_FAILURE;
                            goto done;
                        } else {
                            CPLErrorReset();
                        }
                    } else {
                        x_geom_diff.swap(x_geom_diff_new);
                    }
                }
                ret = pLayerResult->CreateFeature(z.get());
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

        x_prepared_geom.reset();

        if( x_geom_diff == nullptr || x_geom_diff->IsEmpty() )
        {
            /* ok */
        }
        else
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if( bPromoteToMulti )
                x_geom_diff.reset(promote_to_multi(x_geom_diff.release()));
            z->SetGeometryDirectly(x_geom_diff.release());
            ret = pLayerResult->CreateFeature(z.get());
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
 *     feature could not be inserted or a GEOS call failed.
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is to add
 *     but only if the result layer has an unknown geometry type.
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
 * @note The first geometry field is always used.
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

    return OGRLayer::FromHandle(pLayerInput)->Identity(
        OGRLayer::FromHandle(pLayerMethod),
        OGRLayer::FromHandle(pLayerResult),
        papszOptions, pfnProgress, pProgressArg );
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
 *     feature could not be inserted or a GEOS call failed.
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
 * @note The first geometry field is always used.
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
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE)) + static_cast<double>(pLayerMethod->GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CPLTestBool(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

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
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput, mapMethod, false, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // add clipped features from the input layer
    for( auto&& x: this ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None) {
            if (!bSkipFailures) {
                ret = OGRERR_FAILURE;
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom) {
            continue;
        }

        OGRGeometryUniquePtr x_geom_diff(x_geom->clone()); //this will be the geometry of a result feature
        for( auto&& y: pLayerMethod ) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) continue;
            if (x_geom_diff) {
                CPLErrorReset();
                OGRGeometryUniquePtr x_geom_diff_new(x_geom_diff->Difference(y_geom));
                if (CPLGetLastErrorType() != CE_None || x_geom_diff_new == nullptr) {
                    if (!bSkipFailures) {
                        ret = OGRERR_FAILURE;
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                } else {
                    x_geom_diff.swap(x_geom_diff_new);
                }
            }
        }

        if( x_geom_diff == nullptr || x_geom_diff->IsEmpty() )
        {
            /* ok */
        }
        else
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if( bPromoteToMulti )
                x_geom_diff.reset(promote_to_multi(x_geom_diff.release()));
            z->SetGeometryDirectly(x_geom_diff.release());
            ret = pLayerResult->CreateFeature(z.get());
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
    for( auto&& y: pLayerMethod ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        OGRGeometry *y_geom = y->StealGeometry();
        if (!y_geom) continue;
        OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
        if (mapMethod) z->SetFieldsFrom(y.get(), mapMethod);
        z->SetGeometryDirectly(y_geom);
        ret = pLayerResult->CreateFeature(z.get());
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
 *     feature could not be inserted or a GEOS call failed.
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
 * @note The first geometry field is always used.
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

    return OGRLayer::FromHandle(pLayerInput)->Update(
        OGRLayer::FromHandle(pLayerMethod),
        OGRLayer::FromHandle(pLayerResult),
        papszOptions, pfnProgress, pProgressArg );
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
 *     feature could not be inserted or a GEOS call failed.
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
 * @note The first geometry field is always used.
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
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CPLTestBool(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, nullptr, mapInput, nullptr, false, papszOptions);
    if (ret != OGRERR_NONE) goto done;

    poDefnResult = pLayerResult->GetLayerDefn();
    for( auto&& x: this ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None) {
            if (!bSkipFailures) {
                ret = OGRERR_FAILURE;
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom) {
            continue;
        }

        OGRGeometryUniquePtr geom; // this will be the geometry of the result feature
        // incrementally add area from y to geom
        for( auto&& y: pLayerMethod ) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) continue;
            if (!geom) {
                geom.reset(y_geom->clone());
            } else {
                CPLErrorReset();
                OGRGeometryUniquePtr geom_new(geom->Union(y_geom));
                if (CPLGetLastErrorType() != CE_None || geom_new == nullptr) {
                    if (!bSkipFailures) {
                        ret = OGRERR_FAILURE;
                        goto done;
                    } else {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                } else {
                    geom.swap(geom_new);
                }
            }
        }

        // possibly add a new feature with area x intersection sum of y
        if (geom) {
            CPLErrorReset();
            OGRGeometryUniquePtr poIntersection(x_geom->Intersection(geom.get()));
            if (CPLGetLastErrorType() != CE_None || poIntersection == nullptr) {
                if (!bSkipFailures) {
                    ret = OGRERR_FAILURE;
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
            else if( !poIntersection->IsEmpty() )
            {
                OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
                z->SetFieldsFrom(x.get(), mapInput);
                if( bPromoteToMulti )
                    poIntersection.reset(promote_to_multi(poIntersection.release()));
                z->SetGeometryDirectly(poIntersection.release());
                ret = pLayerResult->CreateFeature(z.get());
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
 *     feature could not be inserted or a GEOS call failed.
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
 * @note The first geometry field is always used.
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

    return OGRLayer::FromHandle(pLayerInput)->Clip(
        OGRLayer::FromHandle(pLayerMethod),
        OGRLayer::FromHandle(pLayerResult),
        papszOptions, pfnProgress, pProgressArg );
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
 *     feature could not be inserted or a GEOS call failed.
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
 * @note The first geometry field is always used.
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
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    int bSkipFailures = CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    int bPromoteToMulti = CPLTestBool(CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS()) {
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE) goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE) goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, nullptr, mapInput, nullptr, false, papszOptions);
    if (ret != OGRERR_NONE) goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    for( auto&& x: this ) {

        if (pfnProgress) {
            double p = progress_counter/progress_max;
            if (p > progress_ticker) {
                if (!pfnProgress(p, "", pProgressArg)) {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on the method layer
        CPLErrorReset();
        OGRGeometry *x_geom = set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None) {
            if (!bSkipFailures) {
                ret = OGRERR_FAILURE;
                goto done;
            } else {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom) {
            continue;
        }

        OGRGeometryUniquePtr geom(x_geom->clone()); // this will be the geometry of the result feature
        // incrementally erase y from geom
        for( auto&& y: pLayerMethod ) {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom) continue;
            CPLErrorReset();
            OGRGeometryUniquePtr geom_new(geom->Difference(y_geom));
            if (CPLGetLastErrorType() != CE_None || geom_new == nullptr) {
                if (!bSkipFailures) {
                    ret = OGRERR_FAILURE;
                    goto done;
                } else {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            } else {
                geom.swap(geom_new);
                if (geom->IsEmpty())
                {
                    break;
                }
            }
        }

        // add a new feature if there is remaining area
        if (!geom->IsEmpty()) {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if( bPromoteToMulti )
                geom.reset(promote_to_multi(geom.release()));
            z->SetGeometryDirectly(geom.release());
            ret = pLayerResult->CreateFeature(z.get());
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
 *     feature could not be inserted or a GEOS call failed.
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
 * @note The first geometry field is always used.
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

    return OGRLayer::FromHandle(pLayerInput)->Erase(
        OGRLayer::FromHandle(pLayerMethod),
        OGRLayer::FromHandle(pLayerResult),
        papszOptions, pfnProgress, pProgressArg );
}

/************************************************************************/
/*                  OGRLayer::FeatureIterator::Private                  */
/************************************************************************/

struct OGRLayer::FeatureIterator::Private
{
    CPL_DISALLOW_COPY_ASSIGN(Private)
    Private() = default;

    OGRFeatureUniquePtr m_poFeature{};
    OGRLayer* m_poLayer = nullptr;
    bool m_bError = false;
    bool m_bEOF = true;
};

/************************************************************************/
/*                OGRLayer::FeatureIterator::FeatureIterator()          */
/************************************************************************/

OGRLayer::FeatureIterator::FeatureIterator(OGRLayer* poLayer, bool bStart):
    m_poPrivate(new OGRLayer::FeatureIterator::Private())
{
    m_poPrivate->m_poLayer = poLayer;
    if( bStart )
    {
        if( m_poPrivate->m_poLayer->m_poPrivate->m_bInFeatureIterator )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                        "Only one feature iterator can be "
                        "active at a time");
            m_poPrivate->m_bError = true;
        }
        else
        {
            m_poPrivate->m_poLayer->ResetReading();
            m_poPrivate->m_poFeature.reset(m_poPrivate->m_poLayer->GetNextFeature());
            m_poPrivate->m_bEOF = m_poPrivate->m_poFeature == nullptr;
            m_poPrivate->m_poLayer->m_poPrivate->m_bInFeatureIterator = true;
        }
    }
}

/************************************************************************/
/*               ~OGRLayer::FeatureIterator::FeatureIterator()          */
/************************************************************************/

OGRLayer::FeatureIterator::~FeatureIterator()
{
    if( !m_poPrivate->m_bError && m_poPrivate->m_poLayer)
        m_poPrivate->m_poLayer->m_poPrivate->m_bInFeatureIterator = false;
}

/************************************************************************/
/*                              operator*()                             */
/************************************************************************/

OGRFeatureUniquePtr& OGRLayer::FeatureIterator::operator*()
{
    return m_poPrivate->m_poFeature;
}

/************************************************************************/
/*                              operator++()                            */
/************************************************************************/

OGRLayer::FeatureIterator& OGRLayer::FeatureIterator::operator++()
{
    m_poPrivate->m_poFeature.reset(m_poPrivate-> m_poLayer->GetNextFeature());
    m_poPrivate->m_bEOF = m_poPrivate->m_poFeature == nullptr;
    return *this;
}

/************************************************************************/
/*                             operator!=()                             */
/************************************************************************/

bool OGRLayer::FeatureIterator::operator!=(const OGRLayer::FeatureIterator& it) const
{
    return m_poPrivate->m_bEOF != it.m_poPrivate->m_bEOF;
}

/************************************************************************/
/*                                 begin()                              */
/************************************************************************/

OGRLayer::FeatureIterator OGRLayer::begin()
{
    return {this, true};
}

/************************************************************************/
/*                                  end()                               */
/************************************************************************/

OGRLayer::FeatureIterator OGRLayer::end()
{
    return {this, false};
}
