/******************************************************************************
 * $Id: ogrnaslayer.cpp 10645 2007-01-18 02:22:39Z warmerdam $
 *
 * Project:  OGR
 * Purpose:  Implements OGRNASLayer class.
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
 ****************************************************************************/

#include "ogr_nas.h"
#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"

CPL_CVSID("$Id: ogrnaslayer.cpp 10645 2007-01-18 02:22:39Z warmerdam $");

/************************************************************************/
/*                           OGRNASLayer()                              */
/************************************************************************/

OGRNASLayer::OGRNASLayer( const char * pszName,
                          OGRSpatialReference *poSRSIn,
                          OGRwkbGeometryType eReqType,
                          OGRNASDataSource *poDSIn )

{
    if( poSRSIn == NULL )
        poSRS = NULL;
    else
        poSRS = poSRSIn->Clone();
    
    iNextNASId = 0;
    nTotalNASCount = -1;
    
    poDS = poDSIn;

    if ( EQUALN(pszName, "ogr:", 4) )
      poFeatureDefn = new OGRFeatureDefn( pszName+4 );
    else
      poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eReqType );

/* -------------------------------------------------------------------- */
/*      Reader's should get the corresponding NASFeatureClass and       */
/*      cache it.                                                       */
/* -------------------------------------------------------------------- */
    poFClass = poDS->GetReader()->GetClass( pszName );
}

/************************************************************************/
/*                           ~OGRNASLayer()                           */
/************************************************************************/

OGRNASLayer::~OGRNASLayer()

{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( poSRS != NULL )
        poSRS->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRNASLayer::ResetReading()

{
    iNextNASId = 0;
    poDS->GetReader()->ResetReading();
    if (poFClass)
        poDS->GetReader()->SetFilteredClassName(poFClass->GetName());
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNASLayer::GetNextFeature()

{
    GMLFeature  *poNASFeature = NULL;
    OGRGeometry *poGeom = NULL;

    if( iNextNASId == 0 )
        ResetReading();

/* ==================================================================== */
/*      Loop till we find and translate a feature meeting all our       */
/*      requirements.                                                   */
/* ==================================================================== */
    while( TRUE )
    {
/* -------------------------------------------------------------------- */
/*      Cleanup last feature, and get a new raw nas feature.            */
/* -------------------------------------------------------------------- */
        delete poNASFeature;
        delete poGeom;

        poNASFeature = NULL;
        poGeom = NULL;

        poNASFeature = poDS->GetReader()->NextFeature();
        if( poNASFeature == NULL )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Is it of the proper feature class?                              */
/* -------------------------------------------------------------------- */

        // We count reading low level NAS features as a feature read for
        // work checking purposes, though at least we didn't necessary
        // have to turn it into an OGRFeature.
        m_nFeaturesRead++;

        if( poNASFeature->GetClass() != poFClass )
            continue;

        iNextNASId++;

/* -------------------------------------------------------------------- */
/*      Does it satisfy the spatial query, if there is one?             */
/* -------------------------------------------------------------------- */
        const CPLXMLNode* const * papsGeometry = poNASFeature->GetGeometryList();
        if (papsGeometry[0] != NULL)
        {
            poGeom = (OGRGeometry*) OGR_G_CreateFromGMLTree(papsGeometry[0]);

            // We assume the createFromNAS() function would have already
            // reported the error. 
            if( poGeom == NULL )
            {
                delete poNASFeature;
                return NULL;
            }
            
            if( m_poFilterGeom != NULL && !FilterGeometry( poGeom ) )
                continue;
        }
        
/* -------------------------------------------------------------------- */
/*      Convert the whole feature into an OGRFeature.                   */
/* -------------------------------------------------------------------- */
        int iField;
        OGRFeature *poOGRFeature = new OGRFeature( GetLayerDefn() );

        poOGRFeature->SetFID( iNextNASId );

        for( iField = 0; iField < poFClass->GetPropertyCount(); iField++ )
        {
            const GMLProperty *psGMLProperty = poNASFeature->GetProperty( iField );
            if( psGMLProperty == NULL || psGMLProperty->nSubProperties == 0 )
                continue;

            switch( poFClass->GetProperty(iField)->GetType()  )
            {
              case GMLPT_Real:
              {
                  poOGRFeature->SetField( iField, CPLAtof(psGMLProperty->papszSubProperties[0]) );
              }
              break;

              case GMLPT_IntegerList:
              {
                  int nCount = psGMLProperty->nSubProperties;
                  int *panIntList = (int *) CPLMalloc(sizeof(int) * nCount );
                  int i;

                  for( i = 0; i < nCount; i++ )
                      panIntList[i] = atoi(psGMLProperty->papszSubProperties[i]);

                  poOGRFeature->SetField( iField, nCount, panIntList );
                  CPLFree( panIntList );
              }
              break;

              case GMLPT_RealList:
              {
                  int nCount = psGMLProperty->nSubProperties;
                  double *padfList = (double *)CPLMalloc(sizeof(double)*nCount);
                  int i;

                  for( i = 0; i < nCount; i++ )
                      padfList[i] = CPLAtof(psGMLProperty->papszSubProperties[i]);

                  poOGRFeature->SetField( iField, nCount, padfList );
                  CPLFree( padfList );
              }
              break;

              case GMLPT_StringList:
              {
                  poOGRFeature->SetField( iField, psGMLProperty->papszSubProperties );
              }
              break;

              default:
                poOGRFeature->SetField( iField, psGMLProperty->papszSubProperties[0] );
                break;
            }
        }

/* -------------------------------------------------------------------- */
/*      Test against the attribute query.                               */
/* -------------------------------------------------------------------- */
        if( m_poAttrQuery != NULL
            && !m_poAttrQuery->Evaluate( poOGRFeature ) )
        {
            delete poOGRFeature;
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Wow, we got our desired feature. Return it.                     */
/* -------------------------------------------------------------------- */
        delete poNASFeature;

        poOGRFeature->SetGeometryDirectly( poGeom );

        return poOGRFeature;
    }

    return NULL;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRNASLayer::GetFeatureCount( int bForce )

{
    if( poFClass == NULL )
        return 0;

    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return poFClass->GetFeatureCount();
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRNASLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
    double dfXMin, dfXMax, dfYMin, dfYMax;

    if( poFClass != NULL && 
        poFClass->GetExtents( &dfXMin, &dfXMax, &dfYMin, &dfYMax ) )
    {
        psExtent->MinX = dfXMin;
        psExtent->MaxX = dfXMax;
        psExtent->MinY = dfYMin;
        psExtent->MaxY = dfYMax;

        return OGRERR_NONE;
    }
    else 
        return OGRLayer::GetExtent( psExtent, bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNASLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        double  dfXMin, dfXMax, dfYMin, dfYMax;

        if( poFClass == NULL )
            return FALSE;

        return poFClass->GetExtents( &dfXMin, &dfXMax, &dfYMin, &dfYMax );
    }

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
        if( poFClass == NULL 
            || m_poFilterGeom != NULL 
            || m_poAttrQuery != NULL )
            return FALSE;

        return poFClass->GetFeatureCount() != -1;
    }

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

    else 
        return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRNASLayer::GetSpatialRef()

{
    return poSRS;
}

