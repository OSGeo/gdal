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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.21  2005/01/03 22:17:00  fwarmerdam
 * added OGRLayer::SetSpatialFilterRect()
 *
 * Revision 1.20  2004/08/17 15:41:31  warmerda
 * dont compute extents for wkbNone layers
 *
 * Revision 1.19  2003/10/09 15:30:07  warmerda
 * added OGRLayer::DeleteFeature() support
 *
 * Revision 1.18  2003/05/28 19:18:04  warmerda
 * fixup argument names for docs
 *
 * Revision 1.17  2003/05/21 04:54:29  warmerda
 * avoid warnings about unused formal parameters and possibly uninit variables
 *
 * Revision 1.16  2003/04/22 19:36:04  warmerda
 * Added SyncToDisk
 *
 * Revision 1.15  2003/03/19 20:35:05  warmerda
 * added reference counting support
 *
 * Revision 1.14  2003/03/04 17:45:16  warmerda
 * Ensure m_poAttrIndex is initialized.
 *
 * Revision 1.13  2003/03/04 05:48:05  warmerda
 * added index initialize support
 *
 * Revision 1.12  2002/09/26 18:16:19  warmerda
 * added C entry points
 *
 * Revision 1.11  2002/03/27 21:25:25  warmerda
 * added working default implementation of GetFeature()
 *
 * Revision 1.10  2001/11/15 21:19:21  warmerda
 * added transaction semantics
 *
 * Revision 1.9  2001/10/02 14:16:21  warmerda
 * fix handling of case where a query is being cleared
 *
 * Revision 1.8  2001/07/19 18:26:42  warmerda
 * expand tabs
 *
 * Revision 1.7  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.6  2001/06/19 15:53:49  warmerda
 * Added attribute query support
 *
 * Revision 1.5  2001/03/15 04:01:43  danmo
 * Added OGRLayer::GetExtent()
 *
 * Revision 1.4  1999/11/04 21:10:30  warmerda
 * Added CreateField() method.
 *
 * Revision 1.3  1999/07/27 00:51:39  warmerda
 * added GetInfo(), fixed args for other methods
 *
 * Revision 1.2  1999/07/26 13:59:06  warmerda
 * added feature writing api
 *
 * Revision 1.1  1999/07/08 20:05:13  warmerda
 * New
 *
 */

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
}

/************************************************************************/
/*                             ~OGRLayer()                              */
/************************************************************************/

OGRLayer::~OGRLayer()

{
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
/*                       OGR_L_GetSpatialFilter()                       */
/************************************************************************/

OGRGeometryH OGR_L_GetSpatialFilter( OGRLayerH hLayer )

{
    return (OGRGeometryH) ((OGRLayer *) hLayer)->GetSpatialFilter();
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

