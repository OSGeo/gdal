/******************************************************************************
 * $Id$
 *
 * Project:  EDIGEO Translator
 * Purpose:  Implements OGREDIGEOLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_edigeo.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGREDIGEOLayer()                            */
/************************************************************************/

OGREDIGEOLayer::OGREDIGEOLayer( OGREDIGEODataSource* poDS,
                                const char* pszName, OGRwkbGeometryType eType,
                                OGRSpatialReference* poSRS )

{
    this->poDS = poDS;
    nNextFID = 0;

    this->poSRS = poSRS;
    if (poSRS)
        poSRS->Reference();

    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eType );
}

/************************************************************************/
/*                          ~OGREDIGEOLayer()                           */
/************************************************************************/

OGREDIGEOLayer::~OGREDIGEOLayer()

{
    for(int i=0;i<(int)aosFeatures.size();i++)
        delete aosFeatures[i];

    poFeatureDefn->Release();

    if (poSRS)
        poSRS->Release();
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGREDIGEOLayer::ResetReading()

{
    nNextFID = 0;
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGREDIGEOLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    while(TRUE)
    {
        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGREDIGEOLayer::GetNextRawFeature()
{
    if (nNextFID < (int)aosFeatures.size())
    {
        OGRFeature* poFeature = aosFeatures[nNextFID]->Clone();
        nNextFID++;
        return poFeature;
    }
    else
        return NULL;
}

/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature * OGREDIGEOLayer::GetFeature(long nFID)
{
    if (nFID >= 0 && nFID < (int)aosFeatures.size())
        return aosFeatures[nFID]->Clone();
    else
        return NULL;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGREDIGEOLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    else if (EQUAL(pszCap, OLCRandomRead))
        return TRUE;

    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return poDS->HasUTF8ContentOnly();

    return FALSE;
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGREDIGEOLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    /*if (poDS->bExtentValid)
    {
        psExtent->MinX = poDS->dfMinX;
        psExtent->MinY = poDS->dfMinY;
        psExtent->MaxX = poDS->dfMaxX;
        psExtent->MaxY = poDS->dfMaxY;
        return OGRERR_NONE;
    }*/

    return OGRLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGREDIGEOLayer::GetFeatureCount( int bForce )
{
    if (m_poFilterGeom != NULL || m_poAttrQuery != NULL)
        return OGRLayer::GetFeatureCount(bForce);

    return (int)aosFeatures.size();
}

/************************************************************************/
/*                             AddFeature()                             */
/************************************************************************/

void OGREDIGEOLayer::AddFeature(OGRFeature* poFeature)
{
    poFeature->SetFID(aosFeatures.size());
    aosFeatures.push_back(poFeature);
}

/************************************************************************/
/*                         GetAttributeIndex()                          */
/************************************************************************/

int OGREDIGEOLayer::GetAttributeIndex(const CPLString& osRID)
{
    std::map<CPLString,int>::iterator itAttrIndex =
                                            mapAttributeToIndex.find(osRID);
    if (itAttrIndex != mapAttributeToIndex.end())
        return itAttrIndex->second;
    else
        return -1;
}

/************************************************************************/
/*                           AddFieldDefn()                             */
/************************************************************************/

void OGREDIGEOLayer::AddFieldDefn(const CPLString& osName,
                                  OGRFieldType eType,
                                  const CPLString& osRID)
{
    if (osRID.size() != 0)
        mapAttributeToIndex[osRID] = poFeatureDefn->GetFieldCount();

    OGRFieldDefn oFieldDefn(osName, eType);
    poFeatureDefn->AddFieldDefn(&oFieldDefn);
}
