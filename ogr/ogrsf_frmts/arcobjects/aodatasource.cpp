/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements ArcObjects OGR Datasource.
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Ragi Yaser Burhum
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

#include "ogr_ao.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal.h"
#include "aoutils.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          AODataSource()                           */
/************************************************************************/

AODataSource::AODataSource():
OGRDataSource(),
m_pszName(0)
{
}

/************************************************************************/
/*                          ~AODataSource()                          */
/************************************************************************/

AODataSource::~AODataSource()
{
    CPLFree( m_pszName );

    size_t count = m_layers.size();
    for(size_t i = 0; i < count; ++i )
        delete m_layers[i];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int AODataSource::Open(IWorkspace* pWorkspace, const char * pszNewName, int bUpdate )
{
    CPLAssert( m_nLayers == 0 );

    if (bUpdate)
    {
      // Start Editing?
    }

    m_pszName = CPLStrdup( pszNewName );

    m_ipWorkspace = pWorkspace;

    HRESULT hr;

    // Anything will be fetched
    IEnumDatasetPtr ipEnumDataset;

    if (FAILED(hr = m_ipWorkspace->get_Datasets(esriDTAny, &ipEnumDataset)))
    {
      return AOErr(hr, "Failed Opening Workspace Layers");
    }

    return LoadLayers(ipEnumDataset);
}

/************************************************************************/
/*                          LoadLayers()                                */
/************************************************************************/

bool AODataSource::LoadLayers(IEnumDataset* pEnumDataset)
{
  HRESULT hr;

  pEnumDataset->Reset();

  IDatasetPtr ipDataset;

  bool errEncountered = false;

  while ((S_OK == pEnumDataset->Next(&ipDataset)) && !(ipDataset == NULL))
  {
    IFeatureDatasetPtr ipFD = ipDataset;
    if (!(ipFD == NULL))
    {
      //We are dealing with a FeatureDataset, need to get
      IEnumDatasetPtr ipEnumDatasetSubset;
      if (FAILED(hr = ipFD->get_Subsets(&ipEnumDatasetSubset)))
      {
        AOErr(hr, "Failed getting dataset subsets");
        errEncountered = true;
        continue; //skipping
      }

      if (LoadLayers(ipEnumDatasetSubset) == false)
        errEncountered = true;

      continue;
    }

    IFeatureClassPtr ipFC = ipDataset;

    if (ipFC == NULL)
      continue; //skip

    AOLayer* pLayer = new AOLayer;

    ITablePtr ipTable = ipFC;

    if (!pLayer->Initialize(ipTable))
    {
      errEncountered = true;
      continue;
    }

    m_layers.push_back(pLayer);
  }

  if (errEncountered && m_layers.empty())
    return false; //all of the ones we tried had errors
  else
    return true; //at least one worked
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr AODataSource::DeleteLayer( int iLayer )
{
  if( iLayer < 0 || iLayer >= static_cast<int>(m_layers.size()) )
    return OGRERR_FAILURE;

  // Fetch ArObject Table before deleting OGR layer object

  ITablePtr ipTable;
  m_layers[iLayer]->GetTable(&ipTable);

  std::string name = m_layers[iLayer]->GetLayerDefn()->GetName();

  // delete OGR layer
  delete m_layers[iLayer];

  m_layers.erase(m_layers.begin() + iLayer);

  IDatasetPtr ipDataset = ipTable;

  HRESULT hr;

  if (FAILED(hr = ipDataset->Delete()))
  {
    CPLError( CE_Warning, CPLE_AppDefined, "%s was not deleted however it has been closed", name.c_str());
    AOErr(hr, "Failed deleting dataset");

    return OGRERR_FAILURE;
  }
  else
    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int AODataSource::TestCapability( const char * pszCap )
{
  /*
    if( EQUAL(pszCap,ODsCCreateLayer) && bDSUpdate )
        return TRUE;
    else if( EQUAL(pszCap,ODsCDeleteLayer) && bDSUpdate )
        return TRUE;
    else
        return FALSE;
  */

  if(EQUAL(pszCap,ODsCDeleteLayer))
    return TRUE;
  else
    return FALSE;
}
//
/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *AODataSource::GetLayer( int iLayer )
{
  int count = static_cast<int>(m_layers.size());

  if( iLayer < 0 || iLayer >= count )
    return NULL;
  else
    return m_layers[iLayer];
}
