/******************************************************************************
 * $Id: ogrxplanelayer.cpp
 *
 * Project:  XPlane Translator
 * Purpose:  Implements OGRXPlaneLayer class.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_xplane_reader.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRXPlaneLayer()                          */
/************************************************************************/

OGRXPlaneLayer::OGRXPlaneLayer( const char* pszLayerName )

{
    nFID = 0;
    nFeatureArraySize = 0;
    nFeatureArrayMaxSize = 0;
    nFeatureArrayIndex = 0;
    papoFeatures = NULL;
    poDS = NULL;

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    poSRS = new OGRSpatialReference();
    poSRS->SetWellKnownGeogCS("WGS84");
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    poReader = NULL;
}


/************************************************************************/
/*                            ~OGRXPlaneLayer()                            */
/************************************************************************/

OGRXPlaneLayer::~OGRXPlaneLayer()

{
    poFeatureDefn->Release();

    poSRS->Release();

    for(int i=0;i<nFeatureArraySize;i++)
    {
        if (papoFeatures[i])
            delete papoFeatures[i];
    }
    nFeatureArraySize = 0;

    CPLFree(papoFeatures);
    papoFeatures = NULL;

    if (poReader)
    {
        delete poReader;
        poReader = NULL;
    }
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRXPlaneLayer::ResetReading()

{
    if (poReader)
    {
        for(int i=0;i<nFeatureArraySize;i++)
        {
            if (papoFeatures[i])
                delete papoFeatures[i];
        }
        nFID = 0;
        nFeatureArraySize = 0;
        poReader->Rewind();
    }
    nFeatureArrayIndex = 0;
}

/************************************************************************/
/*                            SetReader()                               */
/************************************************************************/

void OGRXPlaneLayer::SetReader(OGRXPlaneReader* poReader)
{
    if (this->poReader)
    {
        delete this->poReader;
    }
    this->poReader = poReader;
}

/************************************************************************/
/*                     AutoAdjustColumnsWidth()                         */
/************************************************************************/

void  OGRXPlaneLayer::AutoAdjustColumnsWidth()
{
    if (poReader != NULL)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AutoAdjustColumnsWidth() only supported when reading the whole file");
        return;
    }

    for(int col=0;col<poFeatureDefn->GetFieldCount();col++)
    {
        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(col);
        if (poFieldDefn->GetWidth() == 0)
        {
            if (poFieldDefn->GetType() == OFTString ||
                poFieldDefn->GetType() == OFTInteger)
            {
                int nMaxLen = 0;
                for(int i=0;i<nFeatureArraySize;i++)
                {
                    int nLen = strlen(papoFeatures[i]->GetFieldAsString(col));
                    if (nLen > nMaxLen)
                        nMaxLen = nLen;
                }
                poFieldDefn->SetWidth(nMaxLen);
            }
            else
            {
                CPLDebug("XPlane", "Field %s of layer %s is of unknown size",
                         poFieldDefn->GetNameRef(), poFeatureDefn->GetName());
            }
        }
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRXPlaneLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    if (poReader)
    {
        while(TRUE)
        {
            if ( nFeatureArrayIndex == nFeatureArraySize)
            {
                nFeatureArrayIndex = nFeatureArraySize = 0;

                if (poReader->GetNextFeature() == FALSE)
                    return NULL;
                if (nFeatureArraySize == 0)
                    return NULL;
            }

            do
            {
                poFeature = papoFeatures[nFeatureArrayIndex];
                papoFeatures[nFeatureArrayIndex] = NULL;
                nFeatureArrayIndex++;

                if( (m_poFilterGeom == NULL
                    || FilterGeometry( poFeature->GetGeometryRef() ) )
                    && (m_poAttrQuery == NULL
                        || m_poAttrQuery->Evaluate( poFeature )) )
                {
                        return poFeature;
                }

                delete poFeature;
            } while(nFeatureArrayIndex < nFeatureArraySize);
        }
    }
    else
        poDS->ReadWholeFileIfNecessary();

    while(nFeatureArrayIndex < nFeatureArraySize)
    {
        poFeature = papoFeatures[nFeatureArrayIndex ++];
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

OGRFeature *  OGRXPlaneLayer::GetFeature( GIntBig nFID )
{
    if (poReader)
        return OGRLayer::GetFeature(nFID);
    else
        poDS->ReadWholeFileIfNecessary();

    if (nFID >= 0 && nFID < nFeatureArraySize)
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

GIntBig  OGRXPlaneLayer::GetFeatureCount( int bForce )
{
    if (poReader == NULL && m_poFilterGeom == NULL && m_poAttrQuery == NULL)
    {
        poDS->ReadWholeFileIfNecessary();
        return nFeatureArraySize;
    }
    else
        return OGRLayer::GetFeatureCount( bForce ) ;
}


/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRXPlaneLayer::SetNextByIndex( GIntBig nIndex )
{
    if (poReader == NULL && m_poFilterGeom == NULL && m_poAttrQuery == NULL)
    {
        poDS->ReadWholeFileIfNecessary();
        if (nIndex < 0 || nIndex >= nFeatureArraySize)
            return OGRERR_FAILURE;

        nFeatureArrayIndex = (int)nIndex;
        return OGRERR_NONE;
    }
    else
        return OGRLayer::SetNextByIndex(nIndex);
}

/************************************************************************/
/*                       TestCapability()                               */
/************************************************************************/

int  OGRXPlaneLayer::TestCapability( const char * pszCap )
{
    if (EQUAL(pszCap,OLCFastFeatureCount) ||
        EQUAL(pszCap,OLCRandomRead) ||
        EQUAL(pszCap,OLCFastSetNextByIndex))
    {
        if (poReader == NULL && m_poFilterGeom == NULL && m_poAttrQuery == NULL)
            return TRUE;
    }
            
    return FALSE;
}


/************************************************************************/
/*                       RegisterFeature()                              */
/************************************************************************/

void OGRXPlaneLayer::RegisterFeature( OGRFeature* poFeature )
{
    CPLAssert (poFeature != NULL);

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if (poGeom)
        poGeom->assignSpatialReference( poSRS );

    if (nFeatureArraySize == nFeatureArrayMaxSize)
    {
        nFeatureArrayMaxSize = 2 * nFeatureArrayMaxSize + 1;
        papoFeatures = (OGRFeature**)CPLRealloc(papoFeatures,
                                nFeatureArrayMaxSize * sizeof(OGRFeature*));
    }
    papoFeatures[nFeatureArraySize] = poFeature;
    poFeature->SetFID( nFID );
    nFID ++;
    nFeatureArraySize ++;
}

/************************************************************************/
/*                         GetLayerDefn()                               */
/************************************************************************/

OGRFeatureDefn * OGRXPlaneLayer::GetLayerDefn()
{
    poDS->ReadWholeFileIfNecessary();
    return poFeatureDefn;
}

/************************************************************************/
/*                        SetDataSource()                               */
/************************************************************************/

void OGRXPlaneLayer::SetDataSource(OGRXPlaneDataSource* poDS)
{
    this->poDS = poDS;
}
