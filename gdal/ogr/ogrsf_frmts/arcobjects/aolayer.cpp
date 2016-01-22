/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements ArcObjects OGR layer.
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
#include "aoutils.h"
#include <strstream>

/************************************************************************/
/*                              AOLayer()                               */
/************************************************************************/
AOLayer::AOLayer():
OGRLayer(),m_pFeatureDefn(NULL),m_pSRS(NULL), m_ipQF(CLSID_QueryFilter),
m_pBuffer(NULL), m_bufferSize(0), m_supressColumnMappingError(false),m_forceMulti(false)
{
}

/************************************************************************/
/*                            ~AOLayer()                            */
/************************************************************************/

AOLayer::~AOLayer()
{
  if (m_pFeatureDefn)
    m_pFeatureDefn->Release();

  if (m_pSRS)
    m_pSRS->Release();

  if (m_pBuffer)
    delete [] m_pBuffer;
}

/************************************************************************/
/*                            Initialize()                            */
/************************************************************************/

bool AOLayer::Initialize(ITable* pTable)
{
  HRESULT hr;

  m_ipTable = pTable;

  CComBSTR temp;

  IDatasetPtr ipDataset = m_ipTable;
  if (FAILED(hr = ipDataset->get_Name(&temp)))
    return false;

  m_pFeatureDefn = new OGRFeatureDefn(CW2A(temp)); //Should I "new" an OGR smart pointer - sample says so, but it doesn't seem right
                                                   //as long as we use the same compiler & settings in both the ogr build and this
                                                   //driver, we should be OK
  m_pFeatureDefn->Reference();

  IFeatureClassPtr ipFC = m_ipTable;

  VARIANT_BOOL hasOID = VARIANT_FALSE;
  ipFC->get_HasOID(&hasOID);

  if (hasOID == VARIANT_TRUE)
  {
    ipFC->get_OIDFieldName(&temp);
    m_strOIDFieldName = CW2A(temp);
  }

  if (FAILED(hr = ipFC->get_ShapeFieldName(&temp)))
    return AOErr(hr, "No shape field found!");

  m_strShapeFieldName = CW2A(temp);

  IFieldsPtr ipFields;
  if (FAILED(hr = ipFC->get_Fields(&ipFields)))
    return AOErr(hr, "Fields not found!");

  long shapeIndex = -1;
  if (FAILED(hr = ipFields->FindField(temp, &shapeIndex)))
    return AOErr(hr, "Shape field not found!");

  IFieldPtr ipShapeField;
  if (FAILED(hr = ipFields->get_Field(shapeIndex, &ipShapeField)))
    return false;
    
  // Use GeometryDef to set OGR shapetype and Spatial Reference information
  //

  IGeometryDefPtr ipGeoDef;
  if (FAILED(hr = ipShapeField->get_GeometryDef(&ipGeoDef)))
    return false;

  OGRwkbGeometryType ogrGeoType;
  if (!AOToOGRGeometry(ipGeoDef, &ogrGeoType))
    return false;

  m_pFeatureDefn->SetGeomType(ogrGeoType);

  if (wkbFlatten(ogrGeoType) == wkbMultiLineString || wkbFlatten(ogrGeoType) == wkbMultiPoint)
    m_forceMulti = true;

  
  // Mapping of Spatial Reference will be passive about errors
  // (it is possible we won't be able to map some ESRI-specific projections)

  esriGeometry::ISpatialReferencePtr ipSR = NULL;

  if (FAILED(hr = ipGeoDef->get_SpatialReference(&ipSR)))
  {
    AOErr(hr, "Failed Fetching ESRI spatial reference");
  }
  else
  {
    if (!AOToOGRSpatialReference(ipSR, &m_pSRS))
    {
      //report error, but be passive about it
      CPLError( CE_Warning, CPLE_AppDefined, "Failed Mapping ESRI Spatial Reference");
    }
  }


  // Map fields
  //
  return AOToOGRFields(ipFields, m_pFeatureDefn, m_OGRFieldToESRIField);
}



/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void AOLayer::ResetReading()
{
  HRESULT hr;

  if (FAILED(hr = m_ipTable->Search(m_ipQF, VARIANT_TRUE, &m_ipCursor)))
    AOErr(hr, "Error Executing Query");

}

/************************************************************************/
/*                            GetTable()                            */
/************************************************************************/

HRESULT AOLayer::GetTable(ITable** ppTable)
{
  return m_ipTable.QueryInterface(IID_ITable, ppTable);
}


/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void AOLayer::SetSpatialFilter( OGRGeometry* pOGRGeom )
{
  if (pOGRGeom == NULL)
  {
    SwitchToAttributeOnlyFilter();
    return;
  }
  else
  {
    SwitchToSpatialFilter();
  }

  esriGeometry::IGeometryPtr ipGeometry = NULL;

  if ((!OGRGeometryToAOGeometry(pOGRGeom, &ipGeometry)) || ipGeometry == NULL)
  {
    // Crap! we failed and there is no return error mechanism.
    // Report Error and dismiss ogr geometry to go back to at least a working
    // state
    CPLError( CE_Failure, CPLE_AppDefined, "Could not convert OGR spatial filter geometry to ArcObjecs one. Dismissing spatial filter!");

    SwitchToAttributeOnlyFilter();
    return;
  }

  // Set Spatial Reference on AO geometry
  IGeoDatasetPtr ipGeoDataset = m_ipTable;
  esriGeometry::ISpatialReferencePtr ipSR = NULL;
  ipGeoDataset->get_SpatialReference(&ipSR);
  ipGeometry->putref_SpatialReference(ipSR);

  ISpatialFilterPtr ipSF = m_ipQF; //QI should never fail because we called SwitchToSpatialFilter

  ipSF->putref_Geometry(ipGeometry);
  
  ResetReading();
}

/************************************************************************/
/*                         SetSpatialFilterRect()                       */
/************************************************************************/

void AOLayer::SetSpatialFilterRect (double dfMinX, double dfMinY, double dfMaxX, double dfMaxY)
{
  SwitchToSpatialFilter();

  IGeoDatasetPtr ipGD = m_ipTable;
  esriGeometry::IEnvelopePtr ipEnvelope(esriGeometry::CLSID_Envelope);
  esriGeometry::ISpatialReferencePtr ipSR;

  ipGD->get_SpatialReference(&ipSR);
  ipEnvelope->putref_SpatialReference(ipSR);
  ipEnvelope->PutCoords(dfMinX, dfMinY, dfMaxX, dfMaxY);
  
  ISpatialFilterPtr ipSF(m_ipQF);
  ipSF->putref_Geometry(ipEnvelope);
  ipSF->put_SpatialRel(esriSpatialRelIntersects);

}


/************************************************************************/
/*                       SwitchToAttributeOnlyFilter()                  */
/************************************************************************/

void AOLayer::SwitchToAttributeOnlyFilter()
{
  ISpatialFilterPtr ipSF = m_ipQF;

  if (ipSF == NULL)
    return; //nothing to be done - it is an attribute filter

  //only need to preserve whereclause since it is the only thing we consume in this driver
  CComBSTR strWhereClause;
  m_ipQF->get_WhereClause(&strWhereClause);

  m_ipQF.CreateInstance(CLSID_QueryFilter); // will destroy old spatial filter
  if (strWhereClause.Length() > 0)
  {
    m_ipQF->put_WhereClause(strWhereClause);
  }
}

/************************************************************************/
/*                         SwitchToSpatialFilter()                      */
/************************************************************************/

void AOLayer::SwitchToSpatialFilter()
{
  ISpatialFilterPtr ipSF = m_ipQF;

  if (!(ipSF == NULL))
    return; //nothing to be done it is a spatial filter already

  //only need to preserve whereclause since it is the only thing we consume in this driver
  CComBSTR strWhereClause;
  m_ipQF->get_WhereClause(&strWhereClause);

  m_ipQF.CreateInstance(CLSID_SpatialFilter); // will destroy old query filter
  if (strWhereClause.Length() > 0)
  {
    m_ipQF->put_WhereClause(strWhereClause);
  }

}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr AOLayer::SetAttributeFilter( const char* pszQuery )
{

  if( pszQuery == NULL )
  {
    
    CComBSTR whereClause(_T(""));
    m_ipQF->put_WhereClause(whereClause);
    
  }
  else
  {
    CComBSTR whereClause(pszQuery);
    m_ipQF->put_WhereClause(whereClause);
  }

  ResetReading();

  return OGRERR_NONE;
}

/************************************************************************/
/*                           OGRFeatureFromAORow()                      */
/************************************************************************/

bool AOLayer::OGRFeatureFromAORow(IRow* pRow, OGRFeature** ppFeature)
{
  HRESULT hr;

  OGRFeature* pOutFeature = new OGRFeature(m_pFeatureDefn);

  /////////////////////////////////////////////////////////
  // Translate OID
  //

  long oid = -1;
  if (FAILED(hr = pRow->get_OID(&oid)))
  {
    //this should never happen
    delete pOutFeature;
    return false;
  }
  pOutFeature->SetFID(oid);

  /////////////////////////////////////////////////////////
  // Translate Geometry
  //

  IFeaturePtr ipFeature = pRow;
  esriGeometry::IGeometryPtr ipGeometry = NULL;

  if (FAILED(hr = ipFeature->get_Shape(&ipGeometry)) || ipGeometry == NULL)
  {
    delete pOutFeature;
    return AOErr(hr, "Failed retrieving shape from ArcObjects");
  }

  OGRGeometry* pOGRGeo = NULL;

  if ((!AOGeometryToOGRGeometry(m_forceMulti, ipGeometry, m_pSRS, m_pBuffer, m_bufferSize, &pOGRGeo)) || pOGRGeo == NULL)
  {
    delete pOutFeature;
    return AOErr(hr, "Failed to translate ArcObjects Geometry to OGR Geometry");
  }

  pOutFeature->SetGeometryDirectly(pOGRGeo);


  //////////////////////////////////////////////////////////
  // Map fields
  //

  CComVariant val;
  size_t mappedFieldCount = m_OGRFieldToESRIField.size();

  bool foundBadColumn = false;

  for (size_t i = 0; i < mappedFieldCount; ++i)
  {
    long index = m_OGRFieldToESRIField[i];

    if (FAILED(hr = pRow->get_Value(index, &val)))
    {
      // this should not happen
      return AOErr(hr, "Failed retrieving row value");
    }

    if (val.vt == VT_NULL)
    {
      continue; //leave as unset
    }

    // 
    // NOTE: This switch statement needs to be kept in sync with AOToOGRGeometry
    //       since we are only checking for types we mapped in that utility function

    switch (m_pFeatureDefn->GetFieldDefn(i)->GetType())
    {

    case OFTInteger:
      {
        val.ChangeType(VT_I4);
        pOutFeature->SetField(i, val.intVal);
      }
      break;

    case OFTReal:
      {
        val.ChangeType(VT_R8);
        pOutFeature->SetField(i, val.dblVal);
      }
      break;
    case OFTString:
      {
        val.ChangeType(VT_BSTR);
        pOutFeature->SetField(i, CW2A(val.bstrVal));
      }
      break;

    /* TODO: Need to get test dataset to implement these leave it as NULL for now
    case OFTBinary:
      {
      // Access as SafeArray with SafeArrayAccessData perhaps?
      }
      break;
    case OFTDateTime:
      {
      // Examine test data to figure out how to extract that
      }
      break;
      */
    default:
      {
        if (!m_supressColumnMappingError)
        {
          foundBadColumn = true;
          CPLError( CE_Warning, CPLE_AppDefined, "Row id: %d col:%d has unhandled col type (%d). Setting to NULL.", oid, i, m_pFeatureDefn->GetFieldDefn(i)->GetType());
        }
      }
    }
  }
  
  if (foundBadColumn)
    m_supressColumnMappingError = true;
  

  *ppFeature = pOutFeature;

  return true;
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* AOLayer::GetNextFeature()
{
  while (1) //want to skip errors
  {
    if (m_ipCursor == NULL)
      return NULL;

    HRESULT hr;

    IRowPtr ipRow;

    if (FAILED(hr = m_ipCursor->NextRow(&ipRow)))
    {
      AOErr(hr, "Failed fetching features");
      return NULL;
    }

    if (hr == S_FALSE || ipRow == NULL)
    {
      // It's OK, we are done fetching
      return NULL;
    }

    OGRFeature* pOGRFeature = NULL;

    if (!OGRFeatureFromAORow(ipRow,  &pOGRFeature))
    {
      long oid = -1;
      ipRow->get_OID(&oid);

      std::strstream msg;
      msg << "Failed translating ArcObjects row [" << oid << "] to OGR Feature";

      AOErr(hr, msg.str());
      
      //return NULL;
      continue; //skip feature
    }

    return pOGRFeature;
  }
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *AOLayer::GetFeature( long oid )
{
  HRESULT hr;

  IRowPtr ipRow;
  if (FAILED(hr = m_ipTable->GetRow(oid, &ipRow)))
  {
    AOErr(hr, "Failed fetching row");
    return NULL;
  }

  OGRFeature* pOGRFeature = NULL;

  if (!OGRFeatureFromAORow(ipRow,  &pOGRFeature))
  {
    AOErr(hr, "Failed translating ArcObjects row to OGR Feature");
    return NULL;
  }

  return pOGRFeature;
}


/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int AOLayer::GetFeatureCount( int bForce )
{
  HRESULT hr;

  long rowCount = -1;

  if (FAILED(hr = m_ipTable->RowCount(m_ipQF, &rowCount)))
  {
    AOErr(hr, "Failed calculating row count");

    return rowCount;
  }

  return static_cast<int>(rowCount);
}



/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr AOLayer::GetExtent (OGREnvelope* psExtent, int bForce)
{

  if (bForce) 
  {
    return OGRLayer::GetExtent( psExtent, bForce );
  }

  HRESULT hr;

  IGeoDatasetPtr ipGeoDataset = m_ipTable;
  
  esriGeometry::IEnvelopePtr ipEnv = NULL;
  if (FAILED(hr = ipGeoDataset->get_Extent(&ipEnv)) || ipEnv == NULL)
  {
    AOErr(hr, "Failed retrieving extent");
    
    return OGRERR_FAILURE;
  }

  double temp;

  ipEnv->get_XMin(&temp);
  psExtent->MinX = temp;

  ipEnv->get_YMin(&temp);
  psExtent->MinY = temp;

  ipEnv->get_XMax(&temp);
  psExtent->MaxX = temp;

  ipEnv->get_YMax(&temp);
  psExtent->MaxY = temp;

  return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int AOLayer::TestCapability( const char* pszCap )
{
    if (EQUAL(pszCap,OLCRandomRead))
        return TRUE;

    else if (EQUAL(pszCap,OLCFastFeatureCount)) 
        return TRUE;

    else if (EQUAL(pszCap,OLCFastSpatialFilter))
        return TRUE;

    else if (EQUAL(pszCap,OLCFastGetExtent))
        return TRUE;
    
    // Have not implemented this yet
    else if (EQUAL(pszCap,OLCCreateField))
        return FALSE;

    // Have not implemented this yet
    else if (EQUAL(pszCap,OLCSequentialWrite)
          || EQUAL(pszCap,OLCRandomWrite))
        return FALSE;

    else 
        return FALSE;
}
