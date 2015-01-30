/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVFKLayer class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include "ogr_vfk.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/*!
  \brief OGRVFKLayer constructor

  \param pszName layer name
  \param poSRSIn spatial reference
  \param eReqType WKB geometry type
  \param poDSIn  data source where to registrate OGR layer
*/
OGRVFKLayer::OGRVFKLayer(const char *pszName,
                         OGRSpatialReference *poSRSIn,
                         OGRwkbGeometryType eReqType,
                         OGRVFKDataSource *poDSIn)
{
    /* set spatial reference */
    if( poSRSIn == NULL ) {
        /* default is S-JTSK (EPSG: 5514) */
        poSRS = new OGRSpatialReference();
        if (poSRS->importFromEPSG(5514) != OGRERR_NONE) {
            delete poSRS;
            poSRS = NULL;
        }
    }
    else {
        poSRS = poSRSIn->Clone();
    }

    /* layer datasource */
    poDS = poDSIn;

    /* feature definition */
    poFeatureDefn = new OGRFeatureDefn(pszName);
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(eReqType);

    /* data block reference */
    poDataBlock = poDS->GetReader()->GetDataBlock(pszName);
}

/*!
  \brief OGRVFKLayer() destructor
*/
OGRVFKLayer::~OGRVFKLayer()
{
    if(poFeatureDefn)
        poFeatureDefn->Release();

    if(poSRS)
        poSRS->Release();
}

/*!
  \brief Test capability (random access, etc.)

  \param pszCap capability name
*/
int OGRVFKLayer::TestCapability(const char * pszCap)
{
    if (EQUAL(pszCap, OLCRandomRead)) {
        return TRUE; /* ? */
    }
    
    return FALSE;
}

/*!
  \brief Reset reading
  
  \todo To be implemented
*/
void OGRVFKLayer::ResetReading()
{
    m_iNextFeature = 0;
    poDataBlock->ResetReading();
}

/*!
  \brief Create geometry from VFKFeature

  \param poVfkFeature pointer to VFKFeature

  \return pointer to OGRGeometry or NULL on error
*/
OGRGeometry *OGRVFKLayer::CreateGeometry(IVFKFeature * poVfkFeature)
{
    return poVfkFeature->GetGeometry();
}

/*!
  \brief Get feature count

  This method overwrites OGRLayer::GetFeatureCount(),

  \param bForce skip (return -1)

  \return number of features
*/
GIntBig OGRVFKLayer::GetFeatureCount(CPL_UNUSED int bForce)
{
    int nfeatures;

    /* note that 'nfeatures' is 0 when data are not read from DB */
    nfeatures = (int)poDataBlock->GetFeatureCount();
    if (m_poFilterGeom || m_poAttrQuery || nfeatures < 1) {
        /* force real feature count */
        nfeatures = (int)OGRLayer::GetFeatureCount();
    }

    CPLDebug("OGR-VFK", "OGRVFKLayer::GetFeatureCount(): name=%s -> n=%d",
             GetName(), nfeatures);

    return nfeatures;
}

/*!
  \brief Get next feature

  \return pointer to OGRFeature instance
*/
OGRFeature *OGRVFKLayer::GetNextFeature()
{
    VFKFeature  *poVFKFeature;
    
    OGRFeature  *poOGRFeature;
    OGRGeometry *poOGRGeom;
    
    poOGRFeature = NULL;
    poOGRGeom    = NULL;
    
    /* loop till we find and translate a feature meeting all our
       requirements
    */
    while (TRUE) {
        /* cleanup last feature, and get a new raw vfk feature */
        if (poOGRGeom != NULL) {
            delete poOGRGeom;
            poOGRGeom = NULL;
        }
        
        poVFKFeature = (VFKFeature *) poDataBlock->GetNextFeature();
        if (!poVFKFeature)
            return NULL;        
        
        /* skip feature with unknown geometry type */
        if (poVFKFeature->GetGeometryType() == wkbUnknown)
            continue;
        
        poOGRFeature = GetFeature(poVFKFeature);
        if (poOGRFeature)
            return poOGRFeature;
    }
}

/*!
  \brief Get feature by fid

  \param nFID feature id (-1 for next)

  \return pointer to OGRFeature or NULL not found
*/
OGRFeature *OGRVFKLayer::GetFeature(GIntBig nFID)
{
    IVFKFeature *poVFKFeature;

    poVFKFeature = poDataBlock->GetFeature(nFID);
    
    if (!poVFKFeature)
        return NULL;

    CPLAssert(nFID == poVFKFeature->GetFID());
    CPLDebug("OGR-VFK", "OGRVFKLayer::GetFeature(): name=%s fid=" CPL_FRMT_GIB, GetName(), nFID);
    
    return GetFeature(poVFKFeature);
}

/*!
  \brief Get feature (private)
  
  \return pointer to OGRFeature or NULL not found
*/
OGRFeature *OGRVFKLayer::GetFeature(IVFKFeature *poVFKFeature)
{
    OGRGeometry *poGeom;
    
    /* skip feature with unknown geometry type */
    if (poVFKFeature->GetGeometryType() == wkbUnknown)
        return NULL;
    
    /* get features geometry */
    poGeom = CreateGeometry(poVFKFeature);
    if (poGeom != NULL)
        poGeom->assignSpatialReference(poSRS);
    
    /* does it satisfy the spatial query, if there is one? */
    if (m_poFilterGeom != NULL && poGeom && !FilterGeometry(poGeom)) {
        return NULL;
    }
    
    /* convert the whole feature into an OGRFeature */
    OGRFeature *poOGRFeature = new OGRFeature(GetLayerDefn());
    poOGRFeature->SetFID(poVFKFeature->GetFID());
    // poOGRFeature->SetFID(++m_iNextFeature);
    
    poVFKFeature->LoadProperties(poOGRFeature);
    
    /* test against the attribute query */
    if (m_poAttrQuery != NULL &&
        !m_poAttrQuery->Evaluate(poOGRFeature)) {
        delete poOGRFeature;
        return NULL;
    }
    
    if (poGeom)
        poOGRFeature->SetGeometryDirectly(poGeom->clone());
    
    return poOGRFeature;
}
