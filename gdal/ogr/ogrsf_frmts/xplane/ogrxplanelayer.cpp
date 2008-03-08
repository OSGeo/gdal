/******************************************************************************
 * $Id: ogrxplanelayer.cpp
 *
 * Project:  XPlane Translator
 * Purpose:  Implements OGRXPlaneLayer class.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#include "ogr_xplane.h"
#include "ogr_xplane_geo_utils.h"


/************************************************************************/
/*                            OGRXPlaneLayer()                          */
/************************************************************************/

OGRXPlaneLayer::OGRXPlaneLayer( const char* pszLayerName )

{
    nFID = 0;
    nCurrentID = 0;
    papoFeatures = NULL;

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    poFeatureDefn->Reference();
}


/************************************************************************/
/*                            ~OGRXPlaneLayer()                            */
/************************************************************************/

OGRXPlaneLayer::~OGRXPlaneLayer()

{
    int i;
    poFeatureDefn->Release();
    for(i=0;i<nFID;i++)
    {
        delete papoFeatures[i];
    }
    CPLFree(papoFeatures);
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRXPlaneLayer::ResetReading()

{
    nCurrentID = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRXPlaneLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    while(nCurrentID < nFID)
    {
        poFeature = papoFeatures[nCurrentID ++];
        CPLAssert (poFeature != NULL);

        if( (m_poFilterGeom == NULL
              || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
        {
                return poFeature->Clone();
        }
    }

    return NULL;
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature *  OGRXPlaneLayer::GetFeature( long nFID )
{
    if (nFID >= 0 && nFID < this->nFID)
    {
        return papoFeatures[nFID]->Clone();
    }
    else
    {
        return NULL;
    }
}

/************************************************************************/
/*                      GetFeatureCount()                               */
/************************************************************************/

int  OGRXPlaneLayer::GetFeatureCount( int bForce )
{
    if (m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return nFID;
    else
        return OGRLayer::GetFeatureCount( bForce ) ;
}

/************************************************************************/
/*                       TestCapability()                               */
/************************************************************************/

int  OGRXPlaneLayer::TestCapability( const char * pszCap )
{
    return FALSE;
}


/************************************************************************/
/*                       RegisterFeature()                              */
/************************************************************************/

void OGRXPlaneLayer::RegisterFeature( OGRFeature* poFeature )
{
    CPLAssert (poFeature != NULL);

    papoFeatures = (OGRFeature**) CPLRealloc(papoFeatures, (nFID + 1) * sizeof(OGRFeature*));
    papoFeatures[nFID] = poFeature;
    poFeature->SetFID( nFID );
    nFID ++;
}

