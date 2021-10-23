/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGRNASLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "ogr_nas.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRNASLayer()                              */
/************************************************************************/

OGRNASLayer::OGRNASLayer( const char * pszName,
                          OGRNASDataSource *poDSIn ) :
    poFeatureDefn(new OGRFeatureDefn(
        pszName + (STARTS_WITH_CI(pszName, "ogr:") ? 4 : 0))),
    iNextNASId(0),
    poDS(poDSIn),
    // Readers should get the corresponding GMLFeatureClass and cache it.
    poFClass(poDS->GetReader()->GetClass( pszName ))
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
}

/************************************************************************/
/*                           ~OGRNASLayer()                           */
/************************************************************************/

OGRNASLayer::~OGRNASLayer()

{
    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRNASLayer::ResetReading()

{
    iNextNASId = 0;
    poDS->GetReader()->ResetReading();
    if (poFClass)
        poDS->GetReader()->SetFilteredClassName(poFClass->GetElementName());
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNASLayer::GetNextFeature()

{
    GMLFeature  *poNASFeature = nullptr;

    if( iNextNASId == 0 )
        ResetReading();

/* ==================================================================== */
/*      Loop till we find and translate a feature meeting all our       */
/*      requirements.                                                   */
/* ==================================================================== */
    while( true )
    {
/* -------------------------------------------------------------------- */
/*      Cleanup last feature, and get a new raw nas feature.            */
/* -------------------------------------------------------------------- */
        delete poNASFeature;
        poNASFeature = poDS->GetReader()->NextFeature();
        if( poNASFeature == nullptr )
            return nullptr;

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
        const CPLXMLNode* const * papsGeometry =
            poNASFeature->GetGeometryList();

        std::vector < OGRGeometry * > poGeom( poNASFeature->GetGeometryCount() );

        bool bErrored = false, bFiltered = false;
        CPLString osLastErrorMsg;
        for( int iGeom = 0; iGeom < poNASFeature->GetGeometryCount(); ++iGeom ) {
            if ( papsGeometry[iGeom] == nullptr )
            {
                poGeom[iGeom] = nullptr;
            }
            else
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);

                poGeom[iGeom] = (OGRGeometry*) OGR_G_CreateFromGMLTree(papsGeometry[iGeom]);
                CPLPopErrorHandler();
                if( poGeom[iGeom] == nullptr )
                    osLastErrorMsg = CPLGetLastErrorMsg();
                poGeom[iGeom] = NASReader::ConvertGeometry(poGeom[iGeom]);
                poGeom[iGeom] = OGRGeometryFactory::forceTo(poGeom[iGeom], GetGeomType());
                // poGeom->dumpReadable( 0, "NAS: " );

                if( poGeom[iGeom] == nullptr )
                    bErrored = true;
            }

            bFiltered = m_poFilterGeom != nullptr && !FilterGeometry( poGeom[iGeom] );
            if( bErrored || bFiltered )
            {
                while (iGeom > 0)
                    delete poGeom[--iGeom];
                poGeom.clear();

                break;
            }
        }

        if( bErrored ) {

            CPLString osGMLId;
            if( poFClass->GetPropertyIndex("gml_id") == 0 )
            {
                const GMLProperty *psGMLProperty =
                    poNASFeature->GetProperty( 0 );
                if( psGMLProperty && psGMLProperty->nSubProperties == 1 )
                {
                    osGMLId.Printf("(gml_id=%s) ",
                            psGMLProperty->papszSubProperties[0]);
                }
            }

            delete poNASFeature;
            poNASFeature = nullptr;

            const bool bGoOn = CPLTestBool(
                    CPLGetConfigOption("NAS_SKIP_CORRUPTED_FEATURES", "NO"));
            CPLError(bGoOn ? CE_Warning : CE_Failure, CPLE_AppDefined,
                    "Geometry of feature %d %scannot be parsed: %s%s",
                    iNextNASId, osGMLId.c_str(), osLastErrorMsg.c_str(),
                    bGoOn ? ". Skipping to next feature.":
                    ". You may set the NAS_SKIP_CORRUPTED_FEATURES "
                    "configuration option to YES to skip to the next "
                    "feature");
            if( bGoOn )
                continue;

            return nullptr;
        }

        if( bFiltered )
            continue;

/* -------------------------------------------------------------------- */
/*      Convert the whole feature into an OGRFeature.                   */
/* -------------------------------------------------------------------- */
        OGRFeature *poOGRFeature = new OGRFeature( GetLayerDefn() );

        poOGRFeature->SetFID( iNextNASId );

        for( int iField = 0; iField < poFClass->GetPropertyCount(); iField++ )
        {
            const GMLProperty *psGMLProperty =
                poNASFeature->GetProperty( iField );
            if( psGMLProperty == nullptr || psGMLProperty->nSubProperties == 0 )
                continue;

            switch( poFClass->GetProperty(iField)->GetType()  )
            {
              case GMLPT_Real:
              {
                  poOGRFeature->SetField(
                      iField, CPLAtof(psGMLProperty->papszSubProperties[0]) );
              }
              break;

              case GMLPT_IntegerList:
              {
                  int nCount = psGMLProperty->nSubProperties;
                  int *panIntList = static_cast<int *>(
                      CPLMalloc(sizeof(int) * nCount ) );

                  for( int i = 0; i < nCount; i++ )
                      panIntList[i] =
                          atoi(psGMLProperty->papszSubProperties[i]);

                  poOGRFeature->SetField( iField, nCount, panIntList );
                  CPLFree( panIntList );
              }
              break;

              case GMLPT_RealList:
              {
                  int nCount = psGMLProperty->nSubProperties;
                  double *padfList = static_cast<double *>(
                      CPLMalloc(sizeof(double)*nCount) );

                  for( int i = 0; i < nCount; i++ )
                      padfList[i] =
                          CPLAtof(psGMLProperty->papszSubProperties[i]);

                  poOGRFeature->SetField( iField, nCount, padfList );
                  CPLFree( padfList );
              }
              break;

              case GMLPT_StringList:
              {
                  poOGRFeature->SetField(
                      iField, psGMLProperty->papszSubProperties );
              }
              break;

              default:
                poOGRFeature->SetField(
                    iField, psGMLProperty->papszSubProperties[0] );
                break;
            }
        }

        for ( int iGeom = 0; iGeom < poNASFeature->GetGeometryCount(); ++iGeom ) {
            poOGRFeature->SetGeomFieldDirectly(iGeom, poGeom[iGeom]);
            poGeom[iGeom] = nullptr;
        }
        poGeom.clear();

/* -------------------------------------------------------------------- */
/*      Test against the attribute query.                               */
/* -------------------------------------------------------------------- */
        if( m_poAttrQuery != nullptr
            && !m_poAttrQuery->Evaluate( poOGRFeature ) )
        {
            delete poOGRFeature;
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Wow, we got our desired feature. Return it.                     */
/* -------------------------------------------------------------------- */
        delete poNASFeature;

        return poOGRFeature;
    }

    return nullptr;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRNASLayer::GetFeatureCount( int bForce )

{
    if( poFClass == nullptr )
        return 0;

    if( m_poFilterGeom != nullptr || m_poAttrQuery != nullptr )
        return OGRLayer::GetFeatureCount( bForce );

    return poFClass->GetFeatureCount();
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRNASLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
    double dfXMin = 0.0;
    double dfXMax = 0.0;
    double dfYMin = 0.0;
    double dfYMax = 0.0;

    if( poFClass != nullptr &&
        poFClass->GetExtents( &dfXMin, &dfXMax, &dfYMin, &dfYMax ) )
    {
        psExtent->MinX = dfXMin;
        psExtent->MaxX = dfXMax;
        psExtent->MinY = dfYMin;
        psExtent->MaxY = dfYMax;

        return OGRERR_NONE;
    }

    return OGRLayer::GetExtent( psExtent, bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNASLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        if( poFClass == nullptr )
            return FALSE;

        double dfXMin = 0.0;
        double dfXMax = 0.0;
        double dfYMin = 0.0;
        double dfYMax = 0.0;

        return poFClass->GetExtents( &dfXMin, &dfXMax, &dfYMin, &dfYMax );
    }

    if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
        if( poFClass == nullptr
            || m_poFilterGeom != nullptr
            || m_poAttrQuery != nullptr )
            return FALSE;

        return poFClass->GetFeatureCount() != -1;
    }

    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

    return FALSE;
}
