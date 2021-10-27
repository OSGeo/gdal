/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements ArcObjects OGR driver.
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
#include "aoutils.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            AODriver()                            */
/************************************************************************/
AODriver::AODriver():
OGRSFDriver(), m_licensedCheckedOut(false), m_productCode(-1), m_initialized(false)
{
}

/************************************************************************/
/*                            ~AODriver()                            */
/************************************************************************/
AODriver::~AODriver()

{
  if (m_initialized)
  {
    if (m_licensedCheckedOut)
      ShutdownDriver();

    ::CoUninitialize();
  }
}

bool AODriver::Init()
{
  if (m_initialized)
    return true;

  ::CoInitialize(NULL);
  m_initialized = true; //need to mark to un-init COM system on destruction

  m_licensedCheckedOut = InitializeDriver();
  if (!m_licensedCheckedOut)
  {
    CPLError( CE_Failure, CPLE_AppDefined, "ArcGIS License checkout failed.");

    return false;
  }
  else
  {
    m_productCode = GetInitedProductCode();
  }

  return true;
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *AODriver::GetName()

{
  return "ArcObjects";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *AODriver::Open( const char* pszFilename,
                              int bUpdate )

{
  // First check if we have to do any work.
  // In order to avoid all the COM overhead, we are going to check
  // if we have an AO prefix

  if( !STARTS_WITH_CI(pszFilename, "AO:") )
    return NULL;

  if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("AO") )
        return nullptr;

  //OK, it is our turn, let's pay the price

  if (!Init())
  {
    return NULL;
  }

  const char* pInitString = pszFilename + 3; //skip chars

  IWorkspacePtr ipWorkspace = NULL;
  OpenWorkspace(pInitString, &ipWorkspace);

  if (ipWorkspace == NULL)
    return NULL;

  AODataSource* pDS = new AODataSource();

  if(!pDS->Open( ipWorkspace, pszFilename, bUpdate ) )
  {
    delete pDS;
    return NULL;
  }

  return pDS;
}

/************************************************************************
*                     CreateDataSource()                                *
************************************************************************/

OGRDataSource* AODriver::CreateDataSource( const char * pszName,
                                           char **papszOptions)
{
  return NULL;
}

void AODriver::OpenWorkspace(std::string conn, IWorkspace** ppWorkspace)
{
  //If there are any others we want to support in the future, we just need to add them here
  // http://resources.esri.com/help/9.3/ArcGISDesktop/ArcObjects/esriGeoDatabase/IWorkspaceFactory.htm
  //
  //We can also add GUIDS based on licensing code if necessary

  const GUID* pFactories[]=
  {
    &CLSID_FileGDBWorkspaceFactory,
    &CLSID_SdeWorkspaceFactory,
    &CLSID_AccessWorkspaceFactory
  };

  CComBSTR connString(conn.c_str()); //not sure if GDAL is sending utf8 or native - check later

  HRESULT hr;

  // try to connect with every factory specified
  //

  for (int i = 0; i < (sizeof(pFactories)/sizeof(pFactories[0])); ++i)
  {
    if (pFactories[i] == NULL) // in case we have conditional factories
      continue;

    IWorkspaceFactoryPtr ipFactory(*pFactories[i]);

    VARIANT_BOOL isWorkspace = VARIANT_FALSE;

    if (FAILED(hr = ipFactory->IsWorkspace(connString, &isWorkspace)) || isWorkspace == VARIANT_FALSE)
      continue; //try next factory

    IWorkspacePtr ipWorkspace = NULL;

    if (FAILED(hr = ipFactory->OpenFromFile(connString, 0, &ipWorkspace)) || ipWorkspace == NULL)
      continue;

    *ppWorkspace = ipWorkspace.Detach();

    return;
  }

  *ppWorkspace = NULL;
}

/************************************************************************
*                           TestCapability()                           *
************************************************************************/

int AODriver::TestCapability( const char * pszCap )

{
  /*
  if (EQUAL(pszCap, ODsCCreateLayer) )
    return FALSE;
    */
  if (EQUAL(pszCap, ODsCDeleteLayer) )
    return TRUE;
  /*
  if (EQUAL(pszCap, ODrCCreateDataSource) )
    return FALSE;
  */

  return FALSE;
}

/************************************************************************
*                           RegisterOGRao()                                 *
************************************************************************/

void RegisterOGRao()

{
  if (! GDAL_CHECK_VERSION("OGR AO"))
    return;
  OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new AODriver );
}
