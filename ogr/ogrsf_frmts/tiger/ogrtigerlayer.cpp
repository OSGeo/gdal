/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements OGRTigerLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ogr_tiger.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRTigerLayer()                            */
/*                                                                      */
/*      Note that the OGRTigerLayer assumes ownership of the passed     */
/*      OGRFeatureDefn object.                                          */
/************************************************************************/

OGRTigerLayer::OGRTigerLayer( OGRTigerDataSource *poDSIn,
                              TigerFileBase * poReaderIn ) :
    poReader(poReaderIn),
    poDS(poDSIn),
    nFeatureCount(0),
    panModuleFCount(nullptr),
    panModuleOffset(nullptr),
    iLastFeatureId(0),
    iLastModule(-1)
{
/* -------------------------------------------------------------------- */
/*      Setup module feature counts.                                    */
/* -------------------------------------------------------------------- */
    panModuleFCount = (int *)
        CPLCalloc(poDS->GetModuleCount(),sizeof(int));
    panModuleOffset = (int *)
        CPLCalloc(poDS->GetModuleCount()+1,sizeof(int));

    nFeatureCount = 0;

    for( int iModule = 0; iModule < poDS->GetModuleCount(); iModule++ )
    {
        if( poReader->SetModule( poDS->GetModule(iModule) ) )
            panModuleFCount[iModule] = poReader->GetFeatureCount();
        else
            panModuleFCount[iModule] = 0;

        panModuleOffset[iModule] = nFeatureCount;
        nFeatureCount += panModuleFCount[iModule];
    }

    // this entry is just to make range comparisons easy without worrying
    // about falling off the end of the array.
    panModuleOffset[poDS->GetModuleCount()] = nFeatureCount;

    poReader->SetModule( nullptr );
}

/************************************************************************/
/*                           ~OGRTigerLayer()                           */
/************************************************************************/

OGRTigerLayer::~OGRTigerLayer()

{
    if( m_nFeaturesRead > 0 && poReader->GetFeatureDefn() != nullptr )
    {
        CPLDebug( "TIGER", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead,
                  poReader->GetFeatureDefn()->GetName() );
    }

    delete poReader;

    CPLFree( panModuleFCount );
    CPLFree( panModuleOffset );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRTigerLayer::ResetReading()

{
    iLastFeatureId = 0;
    iLastModule = -1;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRTigerLayer::GetFeature( GIntBig nFeatureId )

{
    if( nFeatureId < 1 || nFeatureId > nFeatureCount )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we don't have the current module open for the requested      */
/*      data, then open it now.                                         */
/* -------------------------------------------------------------------- */
    if( iLastModule == -1
        || nFeatureId <= panModuleOffset[iLastModule]
        || nFeatureId > panModuleOffset[iLastModule+1] )
    {
        for( iLastModule = 0;
             iLastModule < poDS->GetModuleCount()
                 && nFeatureId > panModuleOffset[iLastModule+1];
             iLastModule++ ) {}

        if( !poReader->SetModule( poDS->GetModule(iLastModule) ) )
        {
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Fetch the feature associated with the record.                   */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature =
        poReader->GetFeature( (int)nFeatureId-panModuleOffset[iLastModule]-1 );

    if( poFeature != nullptr )
    {
        poFeature->SetFID( nFeatureId );

        if( poFeature->GetGeometryRef() != nullptr )
            poFeature->GetGeometryRef()->assignSpatialReference(
                poDS->DSGetSpatialRef() );

        poFeature->SetField( 0, poReader->GetShortModule() );

        m_nFeaturesRead++;
    }

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRTigerLayer::GetNextFeature()

{
/* -------------------------------------------------------------------- */
/*      Read features till we find one that satisfies our current       */
/*      spatial criteria.                                               */
/* -------------------------------------------------------------------- */
    while( iLastFeatureId < nFeatureCount )
    {
        OGRFeature      *poFeature = GetFeature( ++iLastFeatureId );

        if( poFeature == nullptr )
            break;

        if( (m_poFilterGeom == nullptr
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }

    return nullptr;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRTigerLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *OGRTigerLayer::GetLayerDefn()

{
    OGRFeatureDefn* poFDefn = poReader->GetFeatureDefn();
    if( poFDefn != nullptr )
    {
        if( poFDefn->GetGeomFieldCount() > 0 )
            poFDefn->GetGeomFieldDefn(0)->SetSpatialRef(poDS->DSGetSpatialRef());
    }
    return poFDefn;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRTigerLayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
        return nFeatureCount;
    else
        return OGRLayer::GetFeatureCount( bForce );
}
