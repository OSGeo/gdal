/******************************************************************************
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

CPL_CVSID("$Id$")

/*!
  \brief OGRVFKLayer constructor

  \param pszName layer name
  \param poSRSIn spatial reference
  \param eReqType WKB geometry type
  \param poDSIn  data source where to register OGR layer
*/
OGRVFKLayer::OGRVFKLayer( const char *pszName,
                          OGRSpatialReference *poSRSIn,
                          OGRwkbGeometryType eReqType,
                          OGRVFKDataSource *poDSIn ) :
    poSRS(poSRSIn == nullptr ? new OGRSpatialReference() : poSRSIn->Clone()),
    poFeatureDefn(new OGRFeatureDefn(pszName)),
    poDataBlock(poDSIn->GetReader()->GetDataBlock(pszName)),
    m_iNextFeature(0)
{
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    if( poSRSIn == nullptr ) {
        // Default is S-JTSK (EPSG: 5514).
        if( poSRS->importFromEPSG(5514) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = nullptr;
        }
    }

    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(eReqType);
}

/*!
  \brief OGRVFKLayer() destructor
*/
OGRVFKLayer::~OGRVFKLayer()
{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( poSRS )
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
    /* note that 'nfeatures' is 0 when data are not read from DB */
    int nfeatures = (int)poDataBlock->GetFeatureCount();
    if( m_poFilterGeom || m_poAttrQuery || nfeatures < 1 )
    {
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
    /* loop till we find and translate a feature meeting all our
       requirements
    */
    if ( m_iNextFeature < 1 &&
        m_poFilterGeom == nullptr &&
        m_poAttrQuery == nullptr ) {
        /* sequential feature properties access only supported when no
        filter enabled */
        poDataBlock->LoadProperties();
    }
    while( true ) {
        IVFKFeature* poVFKFeature = poDataBlock->GetNextFeature();
        if (!poVFKFeature) {
            /* clean loaded feature properties for a next run */
            poDataBlock->CleanProperties();
            return nullptr;
        }

        /* skip feature with unknown geometry type */
        if (poVFKFeature->GetGeometryType() == wkbUnknown)
            continue;

        OGRFeature* poOGRFeature = GetFeature(poVFKFeature);
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
    IVFKFeature *poVFKFeature = poDataBlock->GetFeature(nFID);

    if (!poVFKFeature)
        return nullptr;

    /* clean loaded feature properties (sequential access not
       finished) */
    if ( m_iNextFeature > 0 ) {
        ResetReading();
        poDataBlock->CleanProperties();
    }

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
    /* skip feature with unknown geometry type */
    if (poVFKFeature->GetGeometryType() == wkbUnknown)
        return nullptr;

    /* get features geometry */
    OGRGeometry *poGeom = CreateGeometry(poVFKFeature);
    if (poGeom != nullptr)
        poGeom->assignSpatialReference(poSRS);

    /* does it satisfy the spatial query, if there is one? */
    if (m_poFilterGeom != nullptr && poGeom && !FilterGeometry(poGeom)) {
        return nullptr;
    }

    /* convert the whole feature into an OGRFeature */
    OGRFeature *poOGRFeature = new OGRFeature(GetLayerDefn());
    poOGRFeature->SetFID(poVFKFeature->GetFID());
    poVFKFeature->LoadProperties(poOGRFeature);

    /* test against the attribute query */
    if (m_poAttrQuery != nullptr &&
        !m_poAttrQuery->Evaluate(poOGRFeature)) {
        delete poOGRFeature;
        return nullptr;
    }

    if (poGeom)
        poOGRFeature->SetGeometryDirectly(poGeom->clone());

    m_iNextFeature++;

    return poOGRFeature;
}
