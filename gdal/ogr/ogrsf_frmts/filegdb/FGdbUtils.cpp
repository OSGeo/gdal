/******************************************************************************
*
* Project:  OpenGIS Simple Features Reference Implementation
* Purpose:  Different utility functions used in FileGDB OGR driver.
* Author:   Ragi Yaser Burhum, ragi@burhum.com
*
******************************************************************************
* Copyright (c) 2010, Ragi Yaser Burhum
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

#include "FGdbUtils.h"
#include <algorithm>

#include "ogrpgeogeometry.h"

using std::string;

std::wstring StringToWString(const std::string& s)
{
  std::wstring temp(s.length(),L' ');
  std::copy(s.begin(), s.end(), temp.begin());
  return temp; 
}


std::string WStringToString(const std::wstring& s)
{
  //TODO: Need to see if this works on *nix port - write a more efficient - unnecessary mem copied around

  char *tempString = new char[(s.size() * 2) + 1];

  sprintf(tempString,"%ls",s.c_str());

  string returnMe = tempString;

  delete [] tempString;

  return returnMe; 
}


bool GDBErr(long hr, std::string desc)
{
  //  IErrorInfoPtr ipErrorInfo = NULL;
  //  ::GetErrorInfo(NULL, &ipErrorInfo);

  //  if (ipErrorInfo)
  //  {
  //    CComBSTR comErrDesc;
  //    ipErrorInfo->GetDescription(&comErrDesc);

  //    CW2A errMsg(comErrDesc);

  //    CPLError( CE_Failure, CPLE_AppDefined, "AO Error: %s long:%d COM_ERROR:%s", desc.c_str(), hr, errMsg );

  //    ::SetErrorInfo(NULL, NULL);
  //  }
  //  else
  //  {
  CPLError( CE_Failure, CPLE_AppDefined, "GDB Error: %s long:%ld", desc.c_str(), hr);
  //  }

  return false;
}

bool GDBToOGRGeometry(string geoType, bool hasZ, OGRwkbGeometryType* pOut)
{

  if (geoType == "esriGeometryPoint")
  {
    *pOut = hasZ? wkbPoint25D : wkbPoint;
  }
  else if (geoType == "esriGeometryMultipoint")
  {
    *pOut = hasZ? wkbMultiPoint25D : wkbMultiPoint;
  }
  else if (geoType == "esriGeometryLine")
  {
    *pOut = hasZ? wkbLineString25D : wkbLineString;
  }
  else if (geoType == "esriGeometryPolyline")
  {
    *pOut = hasZ? wkbMultiLineString25D : wkbMultiLineString;
  }
  else if (geoType == "esriGeometryPolygon")
  {
    *pOut = hasZ? wkbMultiPolygon25D : wkbMultiPolygon; // no mapping to single polygon
  }
  else
  {
    CPLError( CE_Failure, CPLE_AppDefined, "Cannot map esriGeometryType(%s) to OGRwkbGeometryType", geoType.c_str());
    return false;
  }

  return true;
}



// We could make this function far more robust by doing automatic coertion of types,
// and/or skipping fields we do not know. But our purposes this works fine

bool GDBToOGRFieldType(std::string gdbType, OGRFieldType* pOut)
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

  //            Desc                                 Name                GDB->OGR Mapped By Us?
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

  if (gdbType == "esriFieldTypeSmallInteger" ||
    gdbType == "esriFieldTypeInteger")
  {
    *pOut = OFTInteger;
    return true;
  }
  else if (gdbType == "esriFieldTypeSingle" ||
    gdbType == "esriFieldTypeDouble")
  {
    *pOut = OFTReal;
    return true;
  }
  else if (gdbType == "esriFieldTypeGUID" ||
    gdbType == "esriFieldTypeGlobalID" ||
    gdbType == "esriFieldTypeXML" ||
    gdbType == "esriFieldTypeString")
  {
    *pOut = OFTString;
    return true;
  }
  else if (gdbType == "esriFieldTypeDate")
  {
    *pOut = OFTDateTime;
    return true;
  }
  else if (gdbType == "esriFieldTypeBlob")
  {
    *pOut = OFTBinary;
    return true;
  }
  else
  {
    /* Intentionally fail at these
    esriFieldTypeOID
    esriFieldTypeGeometry
    esriFieldTypeRaster
    */
    CPLError( CE_Warning, CPLE_AppDefined, "%s", ("Cannot map field " + gdbType).c_str());

    return false;
  }
}

// TODO: this is the temporary version - we need to do a full binary import to make it work for other geometries
// only works for points - temporary
//
/*
bool GhettoGDBGeometryToOGRGeometry(bool forceMulti, FileGDBAPI::ShapeBuffer* pGdbGeometry, OGRSpatialReference* pOGRSR, OGRGeometry** ppOutGeometry)
{
  OGRGeometry* pOGRGeometry = NULL;
  
  pOGRGeometry = OGRGeometryFactory::createGeometry(wkbPoint);

  double x, y;
  memcpy(&x, pGdbGeometry->shapeBuffer + 4, sizeof(x));
  memcpy(&y, pGdbGeometry->shapeBuffer + 12, sizeof(y));

  OGRPoint* pPoint = (OGRPoint*)pOGRGeometry;
  pPoint->setX(x);
  pPoint->setY(y);
  pPoint->assignSpatialReference(pOGRSR);

  *ppOutGeometry = pOGRGeometry;

  return true;
}

bool GDBGeometryToOGRGeometry(bool forceMulti, FileGDBAPI::ShapeBuffer* pGdbGeometry, OGRSpatialReference* pOGRSR, OGRGeometry** ppOutGeometry)
{

  OGRGeometry* pOGRGeometry = NULL;
  
  OGRErr eErr = OGRGeometryFactory::createFromWkb(pGdbGeometry->shapeBuffer, pOGRSR, &pOGRGeometry, pGdbGeometry->inUseLength );
  
  if (eErr != OGRERR_NONE)
  {
    CPLError( CE_Failure, CPLE_AppDefined, "Failed attempting to import GDB WKB Geometry. OGRGeometryFactory err:%d", eErr);
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
*/

bool GDBGeometryToOGRGeometry(bool forceMulti, FileGDBAPI::ShapeBuffer* pGdbGeometry, OGRSpatialReference* pOGRSR, OGRGeometry** ppOutGeometry)
{

  OGRGeometry* pOGRGeometry = NULL;

  OGRErr eErr = OGRCreateFromShapeBin( pGdbGeometry->shapeBuffer,
                              &pOGRGeometry,
                              pGdbGeometry->inUseLength);

  //OGRErr eErr = OGRGeometryFactory::createFromWkb(pGdbGeometry->shapeBuffer, pOGRSR, &pOGRGeometry, pGdbGeometry->inUseLength );
  
  if (eErr != OGRERR_NONE)
  {
    CPLError( CE_Failure, CPLE_AppDefined, "Failed attempting to import GDB WKB Geometry. OGRGeometryFactory err:%d", eErr);
    return false;
  }

  if( pOGRGeometry != NULL )
  {
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

    if (pOGRGeometry)
        pOGRGeometry->assignSpatialReference( pOGRSR );
  }


  *ppOutGeometry = pOGRGeometry;

  return true;
}


bool GDBToOGRSpatialReference(const string & wkt, OGRSpatialReference** ppSR)
{
  
  if (wkt.size() <= 0)
  {
    CPLError( CE_Warning, CPLE_AppDefined, "ESRI Spatial Reference is NULL");
    return false;
  }

  *ppSR = new OGRSpatialReference(wkt.c_str());

  OGRErr result = (*ppSR)->morphFromESRI();

  if (result == OGRERR_NONE)
  {
    return true;
  }
  else
  {
    delete *ppSR;
    *ppSR = NULL;

    CPLError( CE_Failure, CPLE_AppDefined, "Failed morhping from ESRI Geometry: %s", wkt.c_str());

    return false;
  }
}
