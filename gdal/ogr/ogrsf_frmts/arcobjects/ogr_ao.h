/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Standard includes and class definitions ArcObjects OGR driver.
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

#ifndef _OGR_AO_H_INCLUDED
#define _OGR_AO_H_INCLUDED

#include "ogrsf_frmts.h"

#include <vector>
#include "cpl_string.h"

//COM ATL Includes
#include <atlbase.h> 
#include <atlcom.h>
#include <atlctl.h>
#include <atlstr.h> //CString

using namespace ATL;

// ArcGIS COM Includes
#import "C:\Program Files (x86)\ArcGIS\com\esriSystem.olb" raw_interfaces_only, raw_native_types, no_namespace, named_guids, exclude("OLE_COLOR", "OLE_HANDLE", "VARTYPE"), rename("min", "esrimin"), rename("max", "esrimax")
#import "C:\Program Files (x86)\ArcGIS\com\esriGeometry.olb" raw_interfaces_only, raw_native_types, named_guids, exclude("ISegment")
#import "C:\Program Files (x86)\ArcGIS\com\esriGeoDatabase.olb" raw_interfaces_only, raw_native_types, no_namespace, named_guids
#import "C:\Program Files (x86)\ArcGIS\com\esriDataSourcesGDB.olb" raw_interfaces_only, raw_native_types, no_namespace, named_guids



/************************************************************************/
/*                            AOLayer                                  */
/************************************************************************/

class AODataSource;

class AOLayer : public OGRLayer
{
public:

  AOLayer();
  virtual ~AOLayer();

  bool Initialize(ITable* pTable);

  const char* GetFIDFieldName() const { return m_strOIDFieldName.c_str(); }
  const char* GetShapeFieldName() const { return m_strShapeFieldName.c_str(); }

  virtual void        ResetReading();
  virtual OGRFeature* GetNextFeature();
  virtual OGRFeature* GetFeature( long nFeatureId );

  HRESULT GetTable(ITable** ppTable);


  virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );
  virtual int         GetFeatureCount( int bForce );
  virtual OGRErr      SetAttributeFilter( const char *pszQuery );
  virtual void 	      SetSpatialFilterRect (double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);
  virtual void        SetSpatialFilter( OGRGeometry * );

/*
  virtual OGRErr      CreateField( OGRFieldDefn *poFieldIn,
  int bApproxOK );

  virtual OGRErr      SetFeature( OGRFeature *poFeature );
  virtual OGRErr      CreateFeature( OGRFeature *poFeature );
  virtual OGRErr      DeleteFeature( long nFID );
*/
   OGRFeatureDefn *    GetLayerDefn() { return m_pFeatureDefn; }

   virtual OGRSpatialReference *GetSpatialRef() { return m_pSRS; }

  virtual int         TestCapability( const char * );

protected:
    bool OGRFeatureFromAORow(IRow* pRow, OGRFeature** ppFeature);
    void SwitchToAttributeOnlyFilter();
    void SwitchToSpatialFilter();

    ITablePtr m_ipTable;
    OGRFeatureDefn* m_pFeatureDefn;
    OGRSpatialReference* m_pSRS;

    std::string m_strOIDFieldName;
    std::string m_strShapeFieldName;

    ICursorPtr m_ipCursor;
    IQueryFilterPtr m_ipQF;

    std::vector<long> m_OGRFieldToESRIField; //OGR Field Index to ESRI Field Index Mapping

    //buffers are used for avoiding constant reallocation of temp memory
    unsigned char* m_pBuffer;
    long  m_bufferSize; //in bytes
    bool  m_supressColumnMappingError;
    bool  m_forceMulti;
};

/************************************************************************/
/*                           AODataSource                            */
/************************************************************************/
class AODataSource : public OGRDataSource
{

public:
  AODataSource();
  virtual ~AODataSource();


  int         Open(IWorkspace* pWorkspace, const char *, int );
  
  const char* GetName() { return m_pszName; }
  int         GetLayerCount() { return static_cast<int>(m_layers.size()); }
  
  OGRLayer*   GetLayer( int );

  
  /*
  virtual OGRLayer* CreateLayer( const char *,
                                 OGRSpatialReference* = NULL,
                                 OGRwkbGeometryType = wkbUnknown,
                                 char** = NULL );

 */
  virtual OGRErr DeleteLayer( int );

  int TestCapability( const char * );

  /*
protected:

  void EnumerateSpatialTables();
  void OpenSpatialTable( const char* pszTableName );
*/
protected:
  bool LoadLayers(IEnumDataset* pEnumDataset);

  char* m_pszName;
  std::vector <AOLayer*> m_layers;
  IWorkspacePtr m_ipWorkspace;

};

/************************************************************************/
/*                              AODriver                                */
/************************************************************************/

class AODriver : public OGRSFDriver
{

public:
  AODriver();
  virtual ~AODriver();

  bool Init();

  const char *GetName();
  virtual OGRDataSource *Open( const char *, int );
  int TestCapability( const char * );
  virtual OGRDataSource *CreateDataSource( const char *pszName, char ** = NULL);

  void OpenWorkspace(std::string, IWorkspace** ppWorkspace);

private:
  bool m_licensedCheckedOut;
  int  m_productCode;
  bool m_initialized;
};

CPL_C_START
void CPL_DLL RegisterOGRao();
CPL_C_END

#endif /* ndef _OGR_PG_H_INCLUDED */


