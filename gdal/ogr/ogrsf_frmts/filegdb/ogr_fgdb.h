/******************************************************************************
* $Id$
*
* Project:  OpenGIS Simple Features Reference Implementation
* Purpose:  Standard includes and class definitions ArcObjects OGR driver.
* Author:   Ragi Yaser Burhum, ragi@burhum.com
*
******************************************************************************
* Copyright (c) 2009, Ragi Yaser Burhum
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_FGDB_H_INCLUDED
#define OGR_FGDB_H_INCLUDED

#ifdef DEBUG_BOOL
#define DO_NOT_USE_DEBUG_BOOL
#endif

#include <vector>
#include <set>
#include "ogrsf_frmts.h"
#include "ogremulatedtransaction.h"

/* GDAL string utilities */
#include "cpl_string.h"

/* GDAL XML handler */
#include "cpl_minixml.h"

/* FGDB API headers through our own inclusion file */
#include "filegdbsdk_headers.h"

/* Workaround needed for Linux, at least for FileGDB API 1.1 (#4455) */
#if defined(__linux__)
#define EXTENT_WORKAROUND
#endif

/************************************************************************
* Default layer creation options
*/

#define FGDB_FEATURE_DATASET "";
#define FGDB_GEOMETRY_NAME "SHAPE"
#define FGDB_OID_NAME "OBJECTID"


/* The ESRI FGDB API namespace */
using namespace FileGDBAPI;

class FGdbDriver;

/************************************************************************/
/*                           FGdbBaseLayer                              */
/************************************************************************/

class FGdbBaseLayer : public OGRLayer
{
protected:

  FGdbBaseLayer();
  virtual ~FGdbBaseLayer();

  OGRFeatureDefn* m_pFeatureDefn;
  OGRSpatialReference* m_pSRS;

  EnumRows*    m_pEnumRows;

  std::vector<std::wstring> m_vOGRFieldToESRIField; //OGR Field Index to ESRI Field Name Mapping
  std::vector<std::string> m_vOGRFieldToESRIFieldType; //OGR Field Index to ESRI Field Type Mapping

  bool  m_suppressColumnMappingError;
  bool  m_forceMulti;

  bool OGRFeatureFromGdbRow(Row* pRow, OGRFeature** ppFeature);

  virtual void       CloseGDBObjects();

public:
          virtual OGRFeature* GetNextFeature();
};

/************************************************************************/
/*                            FGdbLayer                                 */
/************************************************************************/

class FGdbDataSource;

class FGdbLayer : public FGdbBaseLayer
{
  friend class FGdbDataSource;

  int                 m_bBulkLoadAllowed;
  int                 m_bBulkLoadInProgress;

  virtual void        CloseGDBObjects();
  int                 EditIndexesForFIDHack(const char* pszRadixTablename);
  int                 EditGDBTablX(const CPLString& osGDBTablX,
                                   const CPLString& osNewGDBTablX);
  int                 EditATXOrSPX(const CPLString& osIndex);
  int                 EditATXOrSPX( VSILFILE* fp,
                                    int nThisPage,
                                    int& nLastPageVisited,
                                    int nDepth,
                                    int nSizeIndexedValue,
                                    GByte* pabyLastIndexedValue,
                                    int& bIndexedValueIsValid,
                                    int& nFirstIndexAtThisValue,
                                    std::vector<int>& anPagesAtThisValue,
                                    int& bSortThisValue,
                                    int& bInvalidateIndex );

  void                StartBulkLoad();
  void                EndBulkLoad();

#ifdef EXTENT_WORKAROUND
  bool                m_bLayerJustCreated;
  OGREnvelope         sLayerEnvelope;
  bool                m_bLayerEnvelopeValid;
  void                WorkAroundExtentProblem();
  bool                UpdateRowWithGeometry(Row& row, OGRGeometry* poGeom);
#endif

  std::vector<ByteArray*> m_apoByteArrays;
  OGRErr              PopulateRowWithFeature( Row& row, OGRFeature *poFeature );
  OGRErr              GetRow( EnumRows& enumRows, Row& row, GIntBig nFID );

  char              **m_papszOptions;

  int                 m_bCreateMultipatch;

  std::map<int,int>   m_oMapOGRFIDToFGDBFID;
  std::map<int,int>   m_oMapFGDBFIDToOGRFID;
  int                 m_nResyncThreshold;
  void                ResyncIDs();

  int                 m_bSymlinkFlag;
  int                 CreateRealCopy();

  char*               CreateFieldDefn(OGRFieldDefn& oField,
                                      int bApproxOK,
                                      std::string& fieldname_clean,
                                      std::string& gdbFieldType);

public:

  FGdbLayer();
  virtual ~FGdbLayer();

  // Internal used by FGDB driver */
  bool Initialize(FGdbDataSource* pParentDataSource, Table* pTable, std::wstring wstrTablePath, std::wstring wstrType);
  bool Create(FGdbDataSource* pParentDataSource, const char * pszLayerName, OGRSpatialReference *poSRS, OGRwkbGeometryType eType, char ** papszOptions);
  bool CreateFeatureDataset(FGdbDataSource* pParentDataSource, std::string feature_dataset_name, OGRSpatialReference* poSRS, char** papszOptions );

  // virtual const char *GetName();
  virtual const char* GetFIDColumn() { return m_strOIDFieldName.c_str(); }

  virtual void        ResetReading();
  virtual OGRFeature* GetNextFeature();
  virtual OGRFeature* GetFeature( GIntBig nFeatureId );

  Table* GetTable() { return m_pTable; }

  std::wstring GetTablePath() const { return m_wstrTablePath; }
  std::wstring GetType() const { return m_wstrType; }

  virtual OGRErr      CreateField( OGRFieldDefn *poField, int bApproxOK );
  virtual OGRErr      DeleteField( int iFieldToDelete );
#ifdef AlterFieldDefn_implemented_but_not_working
  virtual OGRErr      AlterFieldDefn( int iFieldToAlter, OGRFieldDefn* poNewFieldDefn, int nFlags );
#endif

  virtual OGRErr      ICreateFeature( OGRFeature *poFeature );
  virtual OGRErr      ISetFeature( OGRFeature *poFeature );
  virtual OGRErr      DeleteFeature( GIntBig nFID );

  virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );
  virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

  virtual GIntBig     GetFeatureCount( int bForce );
  virtual OGRErr      SetAttributeFilter( const char *pszQuery );

  virtual void        SetSpatialFilter( OGRGeometry * );
  virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom )
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

//  virtual OGRErr        StartTransaction( );
//  virtual OGRErr        CommitTransaction( );
//  virtual OGRErr        RollbackTransaction( );

  OGRFeatureDefn *    GetLayerDefn() { return m_pFeatureDefn; }

  virtual int         TestCapability( const char * );

  // Access the XML directly. The 2 following methods are not currently used by the driver, but
  // can be used by external code for specific purposes.
  OGRErr              GetLayerXML ( char **poXml );
  OGRErr              GetLayerMetadataXML ( char **poXmlMeta );

  void                SetSymlinkFlag() { m_bSymlinkFlag = TRUE; }

  virtual const char* GetMetadataItem(const char* pszName, const char* pszDomain);

protected:

  bool GDBToOGRFields(CPLXMLNode* psFields);
  bool ParseGeometryDef(CPLXMLNode* psGeometryDef);
  bool ParseSpatialReference(CPLXMLNode* psSpatialRefNode, std::string* pOutWkt,
                             std::string* pOutWKID, std::string* pOutLatestWKID);

  FGdbDataSource* m_pDS;
  Table* m_pTable;

  std::string m_strName; //contains underlying FGDB table name (not catalog name)

  std::string m_strOIDFieldName;
  std::string m_strShapeFieldName;

  std::wstring m_wstrTablePath;
  std::wstring m_wstrType; // the type: "Table" or "Feature Class"

  std::wstring m_wstrSubfields;
  std::wstring m_wstrWhereClause;
  OGRGeometry* m_pOGRFilterGeometry;

  bool        m_bFilterDirty; //optimization to avoid multiple calls to search until necessary

  bool  m_bLaunderReservedKeywords;

};

/************************************************************************/
/*                         FGdbResultLayer                              */
/************************************************************************/

class FGdbResultLayer : public FGdbBaseLayer
{
public:

  FGdbResultLayer(FGdbDataSource* pParentDataSource, const char* pszStatement, EnumRows* pEnumRows);
  virtual ~FGdbResultLayer();

  virtual void        ResetReading();

  OGRFeatureDefn *    GetLayerDefn() { return m_pFeatureDefn; }

  virtual int         TestCapability( const char * );

protected:

  FGdbDataSource* m_pDS;
  CPLString       osSQL;
};

/************************************************************************/
/*                           FGdbDataSource                            */
/************************************************************************/

class FGdbDatabaseConnection;

class FGdbDataSource : public OGRDataSource
{
  CPLString             m_osFSName;
  CPLString             m_osPublicName;
  std::set<OGRLayer*>   m_oSetSelectLayers;

  int        FixIndexes();
  int        bPerLayerCopyingForTransaction;

public:
  FGdbDataSource(FGdbDriver* poDriver, FGdbDatabaseConnection* pConnection);
  virtual ~FGdbDataSource();

  int         Open(const char* pszFSName, int bUpdate,
                   const char* pszPublicName);

  const char* GetName() { return m_osPublicName.c_str(); }
  const char* GetFSName() { return m_osFSName.c_str(); }

  int         GetLayerCount() { return static_cast<int>(m_layers.size()); }

  OGRLayer*   GetLayer( int );

  virtual OGRLayer* ICreateLayer( const char *, OGRSpatialReference* = NULL, OGRwkbGeometryType = wkbUnknown, char** = NULL );

  virtual OGRErr DeleteLayer( int );

  virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                  OGRGeometry *poSpatialFilter,
                                  const char *pszDialect );
  virtual void        ReleaseResultSet( OGRLayer * poResultsSet );

  int TestCapability( const char * );

  Geodatabase* GetGDB() { return m_pGeodatabase; }
  bool         GetUpdate() { return m_bUpdate; }
  FGdbDatabaseConnection* GetConnection() { return m_pConnection; }

  GDALDriver* GetOpenFileGDBDrv() { return m_poOpenFileGDBDrv; }
  int         HasSelectLayers() { return m_oSetSelectLayers.size() != 0; }

  int         Close(int bCloseGeodatabase = FALSE);
  int         ReOpen();


  int         HasPerLayerCopyingForTransaction();
  void        SetPerLayerCopyingForTransaction(int bFlag) { bPerLayerCopyingForTransaction = bFlag; }
  void        SetSymlinkFlagOnAllLayers();

  /*
  protected:

  void EnumerateSpatialTables();
  void OpenSpatialTable( const char* pszTableName );
  */
protected:
  bool LoadLayers(const std::wstring & parent);
  bool OpenFGDBTables(const std::wstring &type,
                      const std::vector<std::wstring> &layers);

  FGdbDriver* m_poDriver;
  FGdbDatabaseConnection* m_pConnection;
  std::vector <FGdbLayer*> m_layers;
  Geodatabase* m_pGeodatabase;
  bool m_bUpdate;
  GDALDriver* m_poOpenFileGDBDrv;
};

/************************************************************************/
/*                              FGdbDriver                                */
/************************************************************************/

class FGdbDatabaseConnection
{
public:
    FGdbDatabaseConnection(CPLString osName, Geodatabase* pGeodatabase) :
        m_osName(osName), m_pGeodatabase(pGeodatabase), m_nRefCount(1), m_bLocked(FALSE),
        m_bFIDHackInProgress(FALSE) {}

    CPLString    m_osName;
    Geodatabase* m_pGeodatabase;
    int          m_nRefCount;
    int          m_bLocked;
    int          m_bFIDHackInProgress;

    Geodatabase* GetGDB() { return m_pGeodatabase; }
    void         SetLocked(int bLockedIn) { m_bLocked = bLockedIn; }
    int          GetRefCount() const { return m_nRefCount; }
    int          IsLocked() const { return m_bLocked; }

    int          IsFIDHackInProgress() const { return m_bFIDHackInProgress; }
    void         SetFIDHackInProgress(int bFlag) { m_bFIDHackInProgress = bFlag; }
    int          OpenGeodatabase(const char* pszOverriddenName);
    void         CloseGeodatabase();
};

class FGdbDriver : public OGRSFDriver, public IOGRTransactionBehaviour
{
  std::map<CPLString, FGdbDatabaseConnection*> oMapConnections;
  CPLMutex* hMutex;

public:
  FGdbDriver();
  virtual ~FGdbDriver();

  virtual const char *GetName();
  virtual OGRDataSource *Open( const char *, int );
  virtual int TestCapability( const char * );
  virtual OGRDataSource *CreateDataSource( const char *pszName, char ** = NULL);
  virtual OGRErr DeleteDataSource( const char *pszDataSource );

  /* From IOGRTransactionBehaviour */
  virtual OGRErr StartTransaction(OGRDataSource*& poDSInOut, int& bOutHasReopenedDS);
  virtual OGRErr CommitTransaction(OGRDataSource*& poDSInOut, int& bOutHasReopenedDS);
  virtual OGRErr RollbackTransaction(OGRDataSource*& poDSInOut, int& bOutHasReopenedDS);

  void Release(const char* pszName);
  CPLMutex* GetMutex() { return hMutex; }

private:

};

CPL_C_START
void CPL_DLL RegisterOGRFileGDB();
CPL_C_END

#endif /* ndef _OGR_PG_H_INCLUDED */


