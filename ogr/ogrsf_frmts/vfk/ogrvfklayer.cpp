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
	/* default is S-JTSK */
	const char *wktString = "PROJCS[\"S-JTSK_Krovak_East_North\","
	    "GEOGCS[\"GCS_S_JTSK\","
	    "DATUM[\"Jednotne_Trigonometricke_Site_Katastralni\","
            "SPHEROID[\"Bessel_1841\",6377397.155,299.1528128]],"
	    "PRIMEM[\"Greenwich\",0.0],"
	    "UNIT[\"Degree\",0.0174532925199433]],"
	    "PROJECTION[\"Krovak\"],"
	    "PARAMETER[\"False_Easting\",0.0],"
	    "PARAMETER[\"False_Northing\",0.0],"
	    "PARAMETER[\"Pseudo_Standard_Parallel_1\",78.5],"
	    "PARAMETER[\"Scale_Factor\",0.9999],"
	    "PARAMETER[\"Azimuth\",30.28813975277778],"
	    "PARAMETER[\"Longitude_Of_Center\",24.83333333333333],"
	    "PARAMETER[\"Latitude_Of_Center\",49.5],"
	    "PARAMETER[\"X_Scale\",-1.0],"
	    "PARAMETER[\"Y_Scale\",1.0],"
	    "PARAMETER[\"XY_Plane_Rotation\",90.0],"
	    "UNIT[\"Meter\",1.0]]";
        poSRS = new OGRSpatialReference();
	if (poSRS->importFromWkt((char **)&wktString) != OGRERR_NONE) {
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

  \return pointer to OGRGeometry
  \return NULL on error
*/
OGRGeometry *OGRVFKLayer::CreateGeometry(IVFKFeature * poVfkFeature)
{
    return poVfkFeature->GetGeometry();
}

/*!
  \brief Get spatial reference information
*/
OGRSpatialReference *OGRVFKLayer::GetSpatialRef()
{
    return poSRS;
}

/*!
  \brief Get feature count

  This method overwrites OGRLayer::GetFeatureCount(), 

  \param bForce skip (return -1)

  \return number of features
*/
int OGRVFKLayer::GetFeatureCount(int bForce)
{
    int nfeatures;

    if(!bForce)
	return -1;
    
    if (m_poFilterGeom || m_poAttrQuery)
	nfeatures = OGRLayer::GetFeatureCount(bForce);
    else
	nfeatures = poDataBlock->GetMaxFID();
    
    CPLDebug("OGR_VFK", "OGRVFKLayer::GetFeatureCount(): n=%d", nfeatures);
    
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
	// CPLDebug("OGR_VFK", "OGRVFKLayer::GetNextFeature(): fid=%d", m_iNextFeature);
	
	if (poOGRFeature)
	    return poOGRFeature;
    }
}

/*!
  \brief Get feature by fid

  \param nFID feature id (-1 for next)

  \return pointer to OGRFeature
  \return NULL not found
*/
OGRFeature *OGRVFKLayer::GetFeature(long nFID)
{
    IVFKFeature *poVFKFeature;
    
    poVFKFeature = poDataBlock->GetFeature(nFID);
    if (!poVFKFeature)
	return NULL;
    
    CPLDebug("OGR_VFK", "OGRVFKLayer::GetFeature(): fid=%ld", nFID);
    
    return GetFeature(poVFKFeature);
}

/*!
  \brief Get feature (private)
  
  \return pointer to OGRFeature
  \return NULL not found
*/
OGRFeature *OGRVFKLayer::GetFeature(IVFKFeature *poVFKFeature)
{
    OGRGeometry *poGeom;
    
    /* skip feature with unknown geometry type */
    if (poVFKFeature->GetGeometryType() == wkbUnknown)
	return NULL;
    
    /* get features geometry */
    poGeom = CreateGeometry(poVFKFeature);
    /* segfault ???
    if (poGeom != NULL)
	poGeom->assignSpatialReference(poSRS);
    */
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
