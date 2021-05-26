/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Different utility functions used in ArcObjects OGR driver.
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

#include "aoutils.h"

CPL_CVSID("$Id$")

bool AOErr(HRESULT hr, std::string desc)
{
  IErrorInfoPtr ipErrorInfo = NULL;
  ::GetErrorInfo(NULL, &ipErrorInfo);

  if (ipErrorInfo)
  {
    CComBSTR comErrDesc;
    ipErrorInfo->GetDescription(&comErrDesc);

    CW2A errMsg(comErrDesc);

    CPLError( CE_Failure, CPLE_AppDefined, "AO Error: %s HRESULT:%d COM_ERROR:%s", desc.c_str(), hr, errMsg );

    ::SetErrorInfo(NULL, NULL);
  }
  else
  {
    CPLError( CE_Failure, CPLE_AppDefined, "AO Error: %s HRESULT:%d", desc.c_str(), hr);
  }

  return false;
}

bool AOToOGRGeometry(IGeometryDef* pGeoDef, OGRwkbGeometryType* pOut)
{
  esriGeometry::esriGeometryType geo;
  VARIANT_BOOL hasZ;

  pGeoDef->get_GeometryType(&geo);
  pGeoDef->get_HasZ(&hasZ);

  switch (geo)
  {
    case esriGeometry::esriGeometryPoint:      *pOut = hasZ == VARIANT_TRUE? wkbPoint25D      : wkbPoint;                break;
    case esriGeometry::esriGeometryMultipoint: *pOut = hasZ == VARIANT_TRUE? wkbMultiPoint25D : wkbMultiPoint;           break;
    case esriGeometry::esriGeometryLine:       *pOut = hasZ == VARIANT_TRUE? wkbLineString25D : wkbLineString;           break;
    case esriGeometry::esriGeometryPolyline:   *pOut = hasZ == VARIANT_TRUE? wkbMultiLineString25D : wkbMultiLineString; break;
    case esriGeometry::esriGeometryPolygon:    *pOut = hasZ == VARIANT_TRUE? wkbMultiPolygon25D : wkbMultiPolygon;    break;// no mapping to single polygon

    default:
      {
        CPLError( CE_Failure, CPLE_AppDefined, "Cannot map esriGeometryType(%d) to OGRwkbGeometryType", geo);
        return false;
      }
  }

  return true;
}

bool AOToOGRFields(IFields* pFields, OGRFeatureDefn* pOGRFeatureDef, std::vector<long> & ogrToESRIFieldMapping)
{
  HRESULT hr;

  long fieldCount;
  if (FAILED(hr = pFields->get_FieldCount(&fieldCount)))
    return false;

  ogrToESRIFieldMapping.clear();

  for (long i = 0; i < fieldCount; ++i)
  {

    IFieldPtr ipField;
    if (FAILED(hr = pFields->get_Field(i, &ipField)))
      return AOErr(hr, "Error getting field");

    CComBSTR name;
    if (FAILED(hr = ipField->get_Name(&name)))
      return AOErr(hr, "Could not get field name");

    esriFieldType fieldType;
    if (FAILED(hr = ipField->get_Type(&fieldType)))
      return AOErr(hr, "Error getting field type");

    //skip these
    if (fieldType == esriFieldTypeOID || fieldType == esriFieldTypeGeometry)
      continue;

    OGRFieldType ogrType;
    if (!AOToOGRFieldType(fieldType, &ogrType))
    {
      // field cannot be mapped, skipping it
      CPLError( CE_Warning, CPLE_AppDefined, "Skipping field %s", CW2A(name) );
      continue;
    }

    OGRFieldDefn fieldTemplate( CW2A(name), ogrType);
    pOGRFeatureDef->AddFieldDefn( &fieldTemplate );

    ogrToESRIFieldMapping.push_back(i);
  }

  CPLAssert(ogrToESRIFieldMapping.size() == pOGRFeatureDef->GetFieldCount());

  return true;
}

// We could make this function far more robust by doing automatic coercion of
// types, and/or skipping fields we do not know. But, for our purposes, this
// works fine.

bool AOToOGRFieldType(esriFieldType aoType, OGRFieldType* pOut)
{
  /*
  ESRI types
    esriFieldTypeSmallInteger = 0,
    esriFieldTypeInteger = 1,
    esriFieldTypeSingle = 2,
    esriFieldTypeDouble = 3,
    esriFieldTypeString = 4,
    esriFieldTypeDate = 5,
    esriFieldTypeOID = 6,
    esriFieldTypeGeometry = 7,
    esriFieldTypeBlob = 8,
    esriFieldTypeRaster = 9,
    esriFieldTypeGUID = 10,
    esriFieldTypeGlobalID = 11,
    esriFieldTypeXML = 12
  */

  //OGR Types

  //            Desc                                 Name                AO->OGR Mapped By Us?
  /** Simple 32bit integer *///                   OFTInteger = 0,             YES
  /** List of 32bit integers *///                 OFTIntegerList = 1,         NO
  /** Double Precision floating point *///        OFTReal = 2,                YES
  /** List of doubles *///                        OFTRealList = 3,            NO
  /** String of ASCII chars *///                  OFTString = 4,              YES
  /** Array of strings *///                       OFTStringList = 5,          NO
  /** deprecated *///                             OFTWideString = 6,          NO
  /** deprecated *///                             OFTWideStringList = 7,      NO
  /** Raw Binary data *///                        OFTBinary = 8,              YES
  /** Date *///                                   OFTDate = 9,                NO
  /** Time *///                                   OFTTime = 10,               NO
  /** Date and Time *///                          OFTDateTime = 11            YES

  switch (aoType)
  {
  case esriFieldTypeSmallInteger:
  case esriFieldTypeInteger:
    {
      *pOut = OFTInteger;
      return true;
    }
  case esriFieldTypeSingle:
  case esriFieldTypeDouble:
    {
      *pOut = OFTReal;
      return true;
    }
  case esriFieldTypeGUID:
  case esriFieldTypeGlobalID:
  case esriFieldTypeXML:
  case esriFieldTypeString:
    {
      *pOut = OFTString;
      return true;
    }
  case esriFieldTypeDate:
    {
      *pOut = OFTDateTime;
      return true;
    }
  case esriFieldTypeBlob:
    {
      *pOut = OFTBinary;
      return true;
    }
  default:
    {
      /* Intentionally fail at these
        esriFieldTypeOID
        esriFieldTypeGeometry
        esriFieldTypeRaster
      */
      return false;
    }
  }
}

bool AOGeometryToOGRGeometry(bool forceMulti, esriGeometry::IGeometry* pInAOGeo, OGRSpatialReference* pOGRSR, unsigned char* & pInOutWorkingBuffer, long & inOutBufferSize, OGRGeometry** ppOutGeometry)
{
  HRESULT hr;

  esriGeometry::IWkbPtr ipWkb = pInAOGeo;

  long reqSize = 0;

  if (FAILED(hr = ipWkb->get_WkbSize(&reqSize)))
  {
    AOErr(hr, "Error getting Wkb buffer size");
    return false;
  }

  if (reqSize > inOutBufferSize)
  {
    // resize working buffer
    delete [] pInOutWorkingBuffer;
    pInOutWorkingBuffer = new unsigned char[reqSize];
    inOutBufferSize = reqSize;
  }

  if (FAILED(hr = ipWkb->ExportToWkb(&reqSize, pInOutWorkingBuffer)))
  {
    AOErr(hr, "Error exporting to WKB buffer");
    return false;
  }

  OGRGeometry* pOGRGeometry = NULL;
  OGRErr eErr = OGRGeometryFactory::createFromWkb(pInOutWorkingBuffer, pOGRSR, &pOGRGeometry, reqSize);
  if (eErr != OGRERR_NONE)
  {
    CPLError( CE_Failure, CPLE_AppDefined, "Failed attempting to import ArcGIS WKB Geometry. OGRGeometryFactory err:%d", eErr);
    return false;
  }

  // force geometries to multi if requested

  // If it is a polygon, force to MultiPolygon since we always produce multipolygons
  if (wkbFlatten(pOGRGeometry->getGeometryType()) == wkbPolygon)
  {
    pOGRGeometry = OGRGeometryFactory::forceToMultiPolygon(pOGRGeometry);
  }
  else if (forceMulti)
  {
    if (wkbFlatten(pOGRGeometry->getGeometryType()) == wkbLineString)
    {
      pOGRGeometry = OGRGeometryFactory::forceToMultiLineString(pOGRGeometry);
    }
    else if (wkbFlatten(pOGRGeometry->getGeometryType()) == wkbPoint)
    {
      pOGRGeometry = OGRGeometryFactory::forceToMultiPoint(pOGRGeometry);
    }
  }

  *ppOutGeometry = pOGRGeometry;

  return true;
}

bool AOToOGRSpatialReference(esriGeometry::ISpatialReference* pSR, OGRSpatialReference** ppSR)
{
  HRESULT hr;

  if (pSR == NULL)
  {
    CPLError( CE_Warning, CPLE_AppDefined, "ESRI Spatial Reference is NULL");
    return false;
  }

  esriGeometry::IESRISpatialReferenceGEN2Ptr ipSRGen = pSR;

  long bufferSize = 0;
  if (FAILED(hr = ipSRGen->get_ESRISpatialReferenceSize(&bufferSize)) || bufferSize == 0)
    return false; //should never happen

  BSTR buffer = ::SysAllocStringLen(NULL,bufferSize);

  if (FAILED(hr = ipSRGen->ExportToESRISpatialReference2(&buffer, &bufferSize)))
  {
    ::SysFreeString(buffer);

    return AOErr(hr, "Failed to export ESRI string");
  }

  CW2A strESRIWKT(buffer);

  ::SysFreeString(buffer);

  if ( strESRIWKT[0] == '\0' )
  {
    CPLError( CE_Warning, CPLE_AppDefined, "ESRI Spatial Reference is NULL");
    return false;
  }

  *ppSR = new OGRSpatialReference();
  (*ppSR)->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

  OGRErr result = (*ppSR)->importFromWkt(strESRIWKT);

  if (result == OGRERR_NONE)
  {
    return true;
  }
  else
  {
    delete *ppSR;
    *ppSR = NULL;

    CPLError( CE_Failure, CPLE_AppDefined,
              "Failed morphing from ESRI Geometry: %s", strESRIWKT);

    return false;
  }
}

bool OGRGeometryToAOGeometry(OGRGeometry* pOGRGeom, esriGeometry::IGeometry** ppGeometry)
{
  HRESULT hr;

  *ppGeometry = NULL;

  long wkbSize = static_cast<long>(pOGRGeom->WkbSize());
  GByte* pWKB = (GByte *) CPLMalloc(wkbSize);

  if( pOGRGeom->exportToWkb( wkbNDR, pWKB ) != OGRERR_NONE )
  {
    CPLFree (pWKB);
    CPLError( CE_Failure, CPLE_AppDefined, "Could not export OGR geometry to WKB");
    return false;
  }

  long bytesRead;
  esriGeometry::IGeometryFactoryPtr ipGeomFact(esriGeometry::CLSID_GeometryEnvironment);
  hr = ipGeomFact->CreateGeometryFromWkb(&bytesRead, pWKB, ppGeometry);

  CPLFree (pWKB);

  if (FAILED(hr))
  {
    return AOErr(hr, "Failed translating OGR geometry to ESRI Geometry");
  }

  return true;
}

// Attempt to checkout a license from the top down
bool InitializeDriver(esriLicenseExtensionCode license)
{
  IAoInitializePtr ipInit(CLSID_AoInitialize);

  if (license == 0)
  {
    // Try to init as engine, then engineGeoDB, then ArcView,
    //    then ArcEditor, then ArcInfo
    if (!InitAttemptWithoutExtension(esriLicenseProductCodeEngine))
      if (!InitAttemptWithoutExtension(esriLicenseProductCodeArcView))
        if (!InitAttemptWithoutExtension(esriLicenseProductCodeArcEditor))
          if (!InitAttemptWithoutExtension(esriLicenseProductCodeArcInfo))
          {
            // No appropriate license is available

            CPLError( CE_Failure, CPLE_AppDefined, "ArcGIS License checkout failed.");
            return false;
          }

          return true;
  }

  // Try to init as engine, then engineGeoDB, then ArcView,
  //    then ArcEditor, then ArcInfo
  if (!InitAttemptWithExtension(esriLicenseProductCodeEngine,license))
    if (!InitAttemptWithExtension(esriLicenseProductCodeArcView, license))
      if (!InitAttemptWithExtension(esriLicenseProductCodeArcEditor, license))
        if (!InitAttemptWithExtension(esriLicenseProductCodeArcInfo, license))
        {
          // No appropriate license is available
          CPLError( CE_Failure, CPLE_AppDefined, "ArcGIS License checkout failed.");
          return false;
        }

        return true;
}

// Attempt to initialize without an extension
bool InitAttemptWithoutExtension(esriLicenseProductCode product)
{
  IAoInitializePtr ipInit(CLSID_AoInitialize);

  esriLicenseStatus status = esriLicenseFailure;
  ipInit->Initialize(product, &status);
  return status == esriLicenseCheckedOut;
}

// Attempt to initialize with an extension
bool InitAttemptWithExtension(esriLicenseProductCode product,
                              esriLicenseExtensionCode extension)
{
  IAoInitializePtr ipInit(CLSID_AoInitialize);

  esriLicenseStatus licenseStatus = esriLicenseFailure;
  ipInit->IsExtensionCodeAvailable(product, extension, &licenseStatus);
  if (licenseStatus == esriLicenseAvailable)
  {
    ipInit->Initialize(product, &licenseStatus);
    if (licenseStatus == esriLicenseCheckedOut)
      ipInit->CheckOutExtension(extension, &licenseStatus);
  }
  return licenseStatus == esriLicenseCheckedOut;
}

// Shutdown the driver and check-in the license if needed.
HRESULT ShutdownDriver(esriLicenseExtensionCode license)
{
  HRESULT hr;

  // Scope ipInit so released before AoUninitialize call
  {
    IAoInitializePtr ipInit(CLSID_AoInitialize);
    esriLicenseStatus status;
    if (license != NULL)
    {
      hr = ipInit->CheckInExtension(license, &status);
      if (FAILED(hr) || status != esriLicenseCheckedIn)
        CPLError( CE_Failure, CPLE_AppDefined, "License checkin failed.");
    }
    hr = ipInit->Shutdown();
  }

  return hr;
}

int GetInitedProductCode()
{
  HRESULT hr;
  IAoInitializePtr ipAO(CLSID_AoInitialize);
  esriLicenseProductCode code;
  if (FAILED(hr = ipAO->InitializedProduct(&code)))
    return -1;

  return static_cast<int>(code);
}
