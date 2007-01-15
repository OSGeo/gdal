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
 ****************************************************************************/

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "ogr_attrind.h"

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

/* -------------------------------------------------------------------- */
/*      If this layer has a none geometry type, then we can             */
/*      reasonably assume there are not extents available.              */
/* -------------------------------------------------------------------- */
    if( GetLayerDefn()->GetGeomType() == wkbNone )
    {
        psExtent->MinX = 0.0;
        psExtent->MaxX = 0.0;
        psExtent->MinY = 0.0;
        psExtent->MaxY = 0.0;
        
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
        OGRGeometry *poGeom = poFeature->GetGeometryRef();
        if (poGeom && !bExtentSet)
        {
            poGeom->getEnvelope(psExtent);
            bExtentSet = TRUE;
        }
        else if (poGeom)
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
/*                      OGR_L_SetAttributeFilter()                      */
/************************************************************************/

OGRErr OGR_L_SetAttributeFilter( OGRLayerH hLayer, const char *pszQuery )

{
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
    return ((OGRLayer *)hLayer)->SetNextByIndex( nIndex );
}

/************************************************************************/
/*                        OGR_L_GetNextFeature()                        */
/************************************************************************/

OGRFeatureH OGR_L_GetNextFeature( OGRLayerH hLayer )

{
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
    return ((OGRLayer *) hLayer)->CreateField( (OGRFieldDefn *) hField, 
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
    return ((OGRLayer *)hLayer)->RollbackTransaction();
}

/************************************************************************/
/*                         OGR_L_GetLayerDefn()                         */
/************************************************************************/

OGRFeatureDefnH OGR_L_GetLayerDefn( OGRLayerH hLayer )

{
    return (OGRFeatureDefnH) ((OGRLayer *)hLayer)->GetLayerDefn();
}

/************************************************************************/
/*                        OGR_L_GetSpatialRef()                         */
/************************************************************************/

OGRSpatialReferenceH OGR_L_GetSpatialRef( OGRLayerH hLayer )

{
    return (OGRSpatialReferenceH) ((OGRLayer *) hLayer)->GetSpatialRef();
}

/************************************************************************/
/*                        OGR_L_TestCapability()                        */
/************************************************************************/

int OGR_L_TestCapability( OGRLayerH hLayer, const char *pszCap )

{
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
/*      Fallback to full intersect test (using GEOS) if we still        */
/*      don't know for sure.                                            */
/* -------------------------------------------------------------------- */
    if( m_bFilterIsEnvelope )
        return TRUE;
    else
    {
        if( OGRGeometryFactory::haveGEOS() )
            return m_poFilterGeom->Intersects( poGeometry );
        else
            return TRUE;
    }
}

/************************************************************************/
/*                         OGR_L_ResetReading()                         */
/************************************************************************/

void OGR_L_ResetReading( OGRLayerH hLayer )

{
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
    return ((OGRLayer *) hLayer)->GetGeometryColumn();
}

