/******************************************************************************
*
* Project:  OpenGIS Simple Features Reference Implementation
* Purpose:  Implements FileGDB OGR layer.
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

#include "ogr_fgdb.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "FGdbUtils.h"
#include "cpl_minixml.h" // the only way right now to extract schema information

using std::string;
using std::wstring;

/************************************************************************/
/*                              FGdbLayer()                               */
/************************************************************************/
FGdbLayer::FGdbLayer():
OGRLayer(), m_pDS(NULL),m_pSRS(NULL),m_wstrSubfields(L"*"),m_pOGRFilterGeometry(NULL),m_pEnumRows(NULL),  m_bFilterDirty(true),
m_supressColumnMappingError(false),m_forceMulti(false)
{
  m_pEnumRows = new EnumRows;
}

/************************************************************************/
/*                            ~FGdbLayer()                            */
/************************************************************************/

FGdbLayer::~FGdbLayer()
{
  if (m_pFeatureDefn)
    m_pFeatureDefn->Release();

  if (m_pSRS)
    m_pSRS->Release();

  if (m_pEnumRows)
    delete m_pEnumRows;

  // NOTE: never delete m_pDS - the memory doesn't belong to us

  //TODO: check if we need to close the table or if the destructor takes care of closing
  //      as it should
  if (m_pTable)
    delete m_pTable;

  if (m_pOGRFilterGeometry)
  {
    OGRGeometryFactory::destroyGeometry(m_pOGRFilterGeometry);
  }

}

/*************************************************************************/
/*                            Initialize()                               */
/* Has ownership of the table as soon as it is called.                   */
/************************************************************************/

bool FGdbLayer::Initialize(FGdbDataSource* pParentDataSource, Table* pTable, std::wstring wstrTablePath)
{
  long hr;

  m_pDS = pParentDataSource; // we never assume ownership of the parent - so our destructor should not delete
  
  m_pTable = pTable;

  m_wstrTablePath = wstrTablePath;

  wstring wstrQueryName;
  if (FAILED(hr = pParentDataSource->GetGDB()->GetQueryName(wstrTablePath, wstrQueryName)))
    return GDBErr(hr, "Failed at getting underlying table name for " + WStringToString(wstrTablePath));

  m_strName = WStringToString(wstrQueryName);

  m_pFeatureDefn = new OGRFeatureDefn(m_strName.c_str()); //TODO: Should I "new" an OGR smart pointer - sample says so, but it doesn't seem right
  //as long as we use the same compiler & settings in both the ogr build and this
  //driver, we should be OK
  m_pFeatureDefn->Reference();

  string tableDef;
  if (FAILED(hr = m_pTable->GetDefinition(tableDef)))
    return GDBErr(hr, "Failed at getting table definition for " + WStringToString(wstrTablePath));

  bool abort = false;

  // extract schema information from table
  CPLXMLNode *psRoot = CPLParseXMLString( tableDef.c_str() );

  if (psRoot == NULL)
  {
    CPLError( CE_Failure, CPLE_AppDefined, "%s", ("Failed parsing GDB Table Schema XML for " + m_strName).c_str());
    return false;
  }

  CPLXMLNode *pDataElementNode = psRoot->psNext; // Move to next field which should be DataElement

  if( pDataElementNode != NULL 
    && pDataElementNode->psChild != NULL
    && pDataElementNode->eType == CXT_Element
    && EQUAL(pDataElementNode->pszValue,"DataElement") )
  {
    CPLXMLNode *psNode;

    for( psNode = pDataElementNode->psChild;
      psNode != NULL;
      psNode = psNode->psNext )
    {
      if( psNode->eType == CXT_Element && psNode->psChild != NULL )
      {
        if (EQUAL(psNode->pszValue,"OIDFieldName") )
        {
          char* pszUnescaped = CPLUnescapeString(psNode->psChild->pszValue, NULL, CPLES_XML);
          m_strOIDFieldName = pszUnescaped;
          CPLFree(pszUnescaped);
        }
        else if (EQUAL(psNode->pszValue,"ShapeFieldName") )
        {
          char* pszUnescaped = CPLUnescapeString(psNode->psChild->pszValue, NULL, CPLES_XML);
          m_strShapeFieldName = pszUnescaped;
          CPLFree(pszUnescaped);
        }
        else if (EQUAL(psNode->pszValue,"Fields") )
        {
          if (!GDBToOGRFields(psNode))
          {
            abort = true;
            break;
          }
        }
      }
    }
  }
  else
  {
    CPLError( CE_Failure, CPLE_AppDefined, "%s", ("Failed parsing GDB Table Schema XML (DataElement) for " + m_strName).c_str());
    return false;
  }
  CPLDestroyXMLNode( psRoot );

  if (m_strShapeFieldName.length() <= 0)
  {
    CPLError( CE_Failure, CPLE_AppDefined, "%s", "Attempting to open non-spatial table");
    
    return false;
  }

  if (abort)
    return false;

  return true; //AOToOGRFields(ipFields, m_pFeatureDefn, m_vOGRFieldToESRIField);
}

bool FGdbLayer::ParseGeometryDef(CPLXMLNode* psRoot)
{
  CPLXMLNode *psGeometryDefItem;

  string geometryType;
  bool hasZ = false;
  string wkt;

  for (psGeometryDefItem = psRoot->psChild;
    psGeometryDefItem != NULL;
    psGeometryDefItem = psGeometryDefItem->psNext )
  {
    //loop through all "GeometryDef" elements
    //

    if (psGeometryDefItem->eType == CXT_Element && 
        psGeometryDefItem->psChild != NULL)
    {
      if (EQUAL(psGeometryDefItem->pszValue,"GeometryType"))
      {
        char* pszUnescaped = CPLUnescapeString(psGeometryDefItem->psChild->pszValue, NULL, CPLES_XML);
        
        geometryType = pszUnescaped;
        
        CPLFree(pszUnescaped);
      }
      else if (EQUAL(psGeometryDefItem->pszValue,"SpatialReference"))
      {
        ParseSpatialReference(psGeometryDefItem, &wkt); // we don't check for success because it
                                                        // may not be there
      }
      /* No M support in OGR yet 
      else if (EQUAL(psFieldNode->pszValue,"HasM")
      {
        char* pszUnescaped = CPLUnescapeString(psNode->psChild->pszValue, NULL, CPLES_XML);
        
        if (!strcmp(szUnescaped, "true"))
          hasM = true;
        
        CPLFree(pszUnescaped);
      }
      */
      else if (EQUAL(psGeometryDefItem->pszValue,"HasZ"))
      {
        char* pszUnescaped = CPLUnescapeString(psGeometryDefItem->psChild->pszValue, NULL, CPLES_XML);
        
        if (!strcmp(pszUnescaped, "true"))
          hasZ = true;
        
        CPLFree(pszUnescaped);
      }
    }

  }
       
  OGRwkbGeometryType ogrGeoType;
  if (!GDBToOGRGeometry(geometryType, hasZ, &ogrGeoType))
    return false;

  m_pFeatureDefn->SetGeomType(ogrGeoType);

  if (wkbFlatten(ogrGeoType) == wkbMultiLineString || wkbFlatten(ogrGeoType) == wkbMultiPoint)
    m_forceMulti = true;


  if (wkt.length() > 0)
  {
    if (!GDBToOGRSpatialReference(wkt, &m_pSRS))
    {
      //report error, but be passive about it
      CPLError( CE_Warning, CPLE_AppDefined, "Failed Mapping ESRI Spatial Reference");
    }
  }
  else
  {
    //report error, but be passive about it
    CPLError( CE_Warning, CPLE_AppDefined, "Empty Spatial Reference");
  }

  return true;
}

bool FGdbLayer::ParseSpatialReference(CPLXMLNode* psSpatialRefNode, string* pOutWkt)
{
  *pOutWkt = "";

  CPLXMLNode* psSRItemNode;

  for( psSRItemNode = psSpatialRefNode->psChild;
    psSRItemNode != NULL;
    psSRItemNode = psSRItemNode->psNext )
  {
    //loop through all "Field" elements
    //

    if( psSRItemNode->eType == CXT_Element && 
        psSRItemNode->psChild != NULL && 
        EQUAL(psSRItemNode->pszValue,"WKT"))
    {

      char* pszUnescaped = CPLUnescapeString(psSRItemNode->psChild->pszValue, NULL, CPLES_XML);
      *pOutWkt = pszUnescaped;
      CPLFree(pszUnescaped);

      return true; // found what I was looking for
    }
  }

  return false;
}

bool FGdbLayer::GDBToOGRFields(CPLXMLNode* psRoot)
{
  m_vOGRFieldToESRIField.clear();

  if (psRoot->psChild == NULL || psRoot->psChild->psNext == NULL)
  {
    CPLError( CE_Failure, CPLE_AppDefined, "Unrecognized GDB XML Schema");

    return false;
  }

  psRoot = psRoot->psChild->psNext; //change root to "FieldArray"

  //CPLAssert(ogrToESRIFieldMapping.size() == pOGRFeatureDef->GetFieldCount());

  CPLXMLNode* psFieldNode;

  for( psFieldNode = psRoot->psChild;
    psFieldNode != NULL;
    psFieldNode = psFieldNode->psNext )
  {
    //loop through all "Field" elements
    //

    if( psFieldNode->eType == CXT_Element && psFieldNode->psChild != NULL && EQUAL(psFieldNode->pszValue,"Field"))
    {

      CPLXMLNode* psFieldItemNode;
      std::string fieldName;
      std::string fieldType;

      // loop through all items in Field element
      //

      for( psFieldItemNode = psFieldNode->psChild;
        psFieldItemNode != NULL;
        psFieldItemNode = psFieldItemNode->psNext )
      {
        if (psFieldItemNode->eType == CXT_Element)
        {

          if (EQUAL(psFieldItemNode->pszValue,"Name"))
          {
            char* pszUnescaped = CPLUnescapeString(psFieldItemNode->psChild->pszValue, NULL, CPLES_XML);
            fieldName = pszUnescaped;
            CPLFree(pszUnescaped);
          }
          else if (EQUAL(psFieldItemNode->pszValue,"Type") )
          {
            char* pszUnescaped = CPLUnescapeString(psFieldItemNode->psChild->pszValue, NULL, CPLES_XML);
            fieldType = pszUnescaped;
            CPLFree(pszUnescaped);
          }
          else if (EQUAL(psFieldItemNode->pszValue,"GeometryDef") )
          {
            if (!ParseGeometryDef(psFieldItemNode))
              return false; // if we failed parsing the GeometryDef, we are done!
          }
        }
      }


      ///////////////////////////////////////////////////////////////////
      // At this point we have parsed everything about the current field


      if (fieldType == "esriFieldTypeGeometry")
      {
        m_strShapeFieldName = fieldName;

        continue; // finish here for special field - don't add as OGR fielddef 
      }
      else if (fieldType == "esriFieldTypeOID")
      {
        //m_strOIDFieldName = fieldName; // already set by this point

        continue; // finish here for special field - don't add as OGR fielddef
      }

      OGRFieldType ogrType;
      if (!GDBToOGRFieldType(fieldType, &ogrType))
      {
        // field cannot be mapped, skipping further processing
        CPLError( CE_Warning, CPLE_AppDefined, "Skipping field: [%s] type: [%s] ", 
          fieldName.c_str(), fieldType.c_str() );
        continue;
      }


      //TODO: Optimization - modify m_wstrSubFields so it only fetches fields that are mapped

      OGRFieldDefn fieldTemplate( fieldName.c_str(), ogrType);
      m_pFeatureDefn->AddFieldDefn( &fieldTemplate );

      m_vOGRFieldToESRIField.push_back(StringToWString(fieldName));

    }
  }

  return true;
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void FGdbLayer::ResetReading()
{
  long hr;

  if (m_pOGRFilterGeometry && !m_pOGRFilterGeometry->IsEmpty())
  {
    // Search spatial
    // As of beta1, FileGDB only supports bbox searched, if we have GEOS installed,
    // we can do the rest ourselves.

    OGREnvelope ogrEnv;

    m_pOGRFilterGeometry->getEnvelope(&ogrEnv);

    //spatial query
    FileGDBAPI::Envelope env(ogrEnv.MinX, ogrEnv.MaxX, ogrEnv.MinY, ogrEnv.MaxY); 
    
    if FAILED(hr = m_pTable->Search(m_wstrSubfields, m_wstrWhereClause, env, true, *m_pEnumRows))
      GDBErr(hr, "Failed Searching");

    m_bFilterDirty = false;

    return;
  }

  // Search non-spatial

  if FAILED(hr = m_pTable->Search(m_wstrSubfields, m_wstrWhereClause, true, *m_pEnumRows))
    GDBErr(hr, "Failed Searching");

  m_bFilterDirty = false;
  
}

/************************************************************************/
/*                            GetTable()                                */
/************************************************************************/

long FGdbLayer::GetTable(Table** ppTable)
{
  *ppTable = m_pTable;
  return S_OK;
}


/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void FGdbLayer::SetSpatialFilter( OGRGeometry* pOGRGeom )
{
  if (m_pOGRFilterGeometry)
  {
    OGRGeometryFactory::destroyGeometry(m_pOGRFilterGeometry);
    m_pOGRFilterGeometry = NULL;
  }

  if (pOGRGeom == NULL || pOGRGeom->IsEmpty())
  {
    m_bFilterDirty = true;

    return;
  }

  m_pOGRFilterGeometry = pOGRGeom->clone();

  m_pOGRFilterGeometry->transformTo(m_pSRS);

  m_bFilterDirty = true;
}

/************************************************************************/
/*                         SetSpatialFilterRect()                       */
/************************************************************************/

void FGdbLayer::SetSpatialFilterRect (double dfMinX, double dfMinY, double dfMaxX, double dfMaxY)
{

  //TODO: can optimize this by changing how the filter gets generated -
  //this will work for now

  OGRGeometry* pTemp = OGRGeometryFactory::createGeometry(wkbPolygon);

  pTemp->assignSpatialReference(m_pSRS);

  OGRLinearRing ring;

  ring.addPoint( dfMinX, dfMinY );
  ring.addPoint( dfMinX, dfMaxY );
  ring.addPoint( dfMaxX, dfMaxY );
  ring.addPoint( dfMaxX, dfMinY );
  ring.addPoint( dfMinX, dfMinY );
  ((OGRPolygon *) pTemp)->addRing( &ring );

  SetSpatialFilter(pTemp);

  OGRGeometryFactory::destroyGeometry(pTemp);

}


/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr FGdbLayer::SetAttributeFilter( const char* pszQuery )
{
  m_wstrWhereClause = StringToWString( (pszQuery != NULL) ? pszQuery : "" );

  m_bFilterDirty = true;

  return OGRERR_NONE;
}

/************************************************************************/
/*                           OGRFeatureFromGdbRow()                      */
/************************************************************************/

bool FGdbLayer::OGRFeatureFromGdbRow(Row* pRow, OGRFeature** ppFeature)
{
  long hr;

  OGRFeature* pOutFeature = new OGRFeature(m_pFeatureDefn);

  /////////////////////////////////////////////////////////
  // Translate OID
  //

  int32 oid = -1;
  if (FAILED(hr = pRow->GetOID(oid)))
  {
    //this should never happen
    delete pOutFeature;
    return false;
  }
  pOutFeature->SetFID(oid);


  /////////////////////////////////////////////////////////
  // Translate Geometry
  //

  ShapeBuffer gdbGeometry;
  if (FAILED(hr = pRow->GetGeometry(gdbGeometry)))
  {
    delete pOutFeature;
    return GDBErr(hr, "Failed retrieving shape for row " + string(CPLSPrintf("%d", (int)oid)));
  }

  OGRGeometry* pOGRGeo = NULL;

  if ((!GDBGeometryToOGRGeometry(m_forceMulti, &gdbGeometry, m_pSRS, &pOGRGeo)) || pOGRGeo == NULL)
  {
    delete pOutFeature;
    return GDBErr(hr, "Failed to translate FileGDB Geometry to OGR Geometry for row " + string(CPLSPrintf("%d", (int)oid)));
  }

  pOutFeature->SetGeometryDirectly(pOGRGeo);


  //////////////////////////////////////////////////////////
  // Map fields
  //


  size_t mappedFieldCount = m_vOGRFieldToESRIField.size();

  bool foundBadColumn = false;

  for (size_t i = 0; i < mappedFieldCount; ++i)
  {
    const wstring & wstrFieldName = m_vOGRFieldToESRIField[i];

    bool isNull = false;

    if (FAILED(hr = pRow->IsNull(wstrFieldName, isNull)))
    {
      GDBErr(hr, "Failed to determine NULL status from column " + WStringToString(wstrFieldName));
      foundBadColumn = true;
      continue;
    }

    if (isNull)
    {
      continue; //leave as unset
    }

    // 
    // NOTE: This switch statement needs to be kept in sync with GDBToOGRFieldType utility function
    //       since we are only checking for types we mapped in that utility function

    switch (m_pFeatureDefn->GetFieldDefn(i)->GetType())
    {

    case OFTInteger:
      {
        int32 val;

        if (FAILED(hr = pRow->GetInteger(wstrFieldName, val)))
        {
          GDBErr(hr, "Failed to determine value for column " + WStringToString(wstrFieldName));
          foundBadColumn = true;
          continue;
        }

        pOutFeature->SetField(i, (int)val);
      }
      break;

    case OFTReal:
      {
        double val;

        if (FAILED(hr = pRow->GetDouble(wstrFieldName, val)))
        {
          GDBErr(hr, "Failed to determine value for column " + WStringToString(wstrFieldName));
          foundBadColumn = true;
          continue;
        }

        pOutFeature->SetField(i, val);
      }
      break;
    case OFTString:
      {
        wstring val;

        if (FAILED(hr = pRow->GetString(wstrFieldName, val)))
        {
          GDBErr(hr, "Failed to determine value for column " + WStringToString(wstrFieldName));
          foundBadColumn = true;
          continue;
        }

        pOutFeature->SetField(i, WStringToString(val).c_str());
      }
      break;

      /* TODO: Need to get test dataset to implement these leave it as NULL for now
      case OFTBinary:
      {
      
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
          CPLError( CE_Warning, CPLE_AppDefined, "Row id: %d col:%d has unhandled col type (%d). Setting to NULL.", (int)oid, i, m_pFeatureDefn->GetFieldDefn(i)->GetType());
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

OGRFeature* FGdbLayer::GetNextFeature()
{
  if (m_bFilterDirty)
    ResetReading();


  while (1) //want to skip errors
  {
    if (m_pEnumRows == NULL)
      return NULL;

    long hr;

    Row row;

    if (FAILED(hr = m_pEnumRows->Next(row)))
    {
      GDBErr(hr, "Failed fetching features");
      return NULL;
    }

    if (hr != S_OK)
    {
      // It's OK, we are done fetching - failure is catched by FAILED macro
      return NULL;
    }

    OGRFeature* pOGRFeature = NULL;

    if (!OGRFeatureFromGdbRow(&row,  &pOGRFeature))
    {
      int32 oid = -1;
      row.GetOID(oid);

      GDBErr(hr, CPLSPrintf("Failed translating ArcObjects row [%d] to OGR Feature", oid));

      //return NULL;
      continue; //skip feature
    }

    return pOGRFeature;
  }
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *FGdbLayer::GetFeature( long oid )
{
  // do query to fetch individual row

  long           hr;
  Row            row;
  EnumRows       enumRows;
  CPLString      osQuery;

  osQuery.Printf("%s = %ld", m_strOIDFieldName.c_str(), oid);

  if (FAILED(hr = m_pTable->Search(m_wstrSubfields, StringToWString(osQuery.c_str()), true, enumRows)))
  {
    GDBErr(hr, "Failed fetching row ");
    return NULL;
  }

  if (FAILED(hr = enumRows.Next(row)))
  {
    GDBErr(hr, "Failed fetching row ");
    return NULL;
  }

  if (hr != S_OK)
    return NULL; //none found - but no failure


  OGRFeature* pOGRFeature = NULL;

  if (!OGRFeatureFromGdbRow(&row,  &pOGRFeature))
  {
    GDBErr(hr, "Failed translating ArcObjects row to OGR Feature");
    return NULL;
  }

  return pOGRFeature;
}


/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int FGdbLayer::GetFeatureCount( int bForce )
{

  long           hr;
  int32          rowCount = 0;

  if (m_pOGRFilterGeometry != NULL || m_wstrWhereClause.size() != 0)
      return OGRLayer::GetFeatureCount(bForce);

  if (FAILED(hr = m_pTable->GetRowCount(rowCount)))
  {
    GDBErr(hr, "Failed counting rows");
    return 0;
  }

#if 0
  Row            row;
  EnumRows       enumRows;

  if (FAILED(hr = m_pTable->Search(StringToWString(m_strOIDFieldName), L"", true, enumRows)))
  {
    GDBErr(hr, "Failed counting rows");
    return -1;
  }

  while (S_OK == (hr = enumRows.Next(row)))
    ++rowCount;

  if (FAILED(hr))
  {
    GDBErr(hr, "Failed counting rows (during fetch)");
    return -1;
  }
#endif

  return static_cast<int>(rowCount);
}



/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr FGdbLayer::GetExtent (OGREnvelope* psExtent, int bForce)
{
  if (m_pOGRFilterGeometry != NULL || m_wstrWhereClause.size() != 0)
      return OGRLayer::GetExtent(psExtent, bForce);

  long hr;
  Envelope envelope;
  if (FAILED(hr = m_pTable->GetExtent(envelope)))
  {
    GDBErr(hr, "Failed fetching extent");
    return OGRERR_FAILURE;
  }

  psExtent->MinX = envelope.xMin;
  psExtent->MinY = envelope.yMin;
  psExtent->MaxX = envelope.xMax;
  psExtent->MaxY = envelope.yMax;

  return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int FGdbLayer::TestCapability( const char* pszCap )
{
  if (EQUAL(pszCap,OLCRandomRead))
    return TRUE;

  else if (EQUAL(pszCap,OLCFastFeatureCount)) 
    return m_pOGRFilterGeometry == NULL && m_wstrWhereClause.size() == 0;

  else if (EQUAL(pszCap,OLCFastSpatialFilter))
    return TRUE;

  else if (EQUAL(pszCap,OLCFastGetExtent))
    return m_pOGRFilterGeometry == NULL && m_wstrWhereClause.size() == 0;

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
