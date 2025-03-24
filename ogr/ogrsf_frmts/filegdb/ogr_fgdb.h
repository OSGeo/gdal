/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Standard includes and class definitions ArcObjects OGR driver.
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Ragi Yaser Burhum
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_FGDB_H_INCLUDED
#define OGR_FGDB_H_INCLUDED

#include <vector>
#include <set>
#include "ogrsf_frmts.h"

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
constexpr const char *pszRelationshipTypeUUID =
    "{B606A7E1-FA5B-439C-849C-6E9C2481537B}";

/* The ESRI FGDB API namespace */
using namespace FileGDBAPI;

class FGdbDriver;

/************************************************************************/
/*                           FGdbBaseLayer                              */
/************************************************************************/

class FGdbBaseLayer CPL_NON_FINAL : public OGRLayer
{
  protected:
    FGdbBaseLayer();
    virtual ~FGdbBaseLayer();

    OGRFeatureDefn *m_pFeatureDefn;
    OGRSpatialReference *m_pSRS;

    EnumRows *m_pEnumRows;

    std::vector<std::wstring>
        m_vOGRFieldToESRIField;  // OGR Field Index to ESRI Field Name Mapping
    std::vector<std::string>
        m_vOGRFieldToESRIFieldType;  // OGR Field Index to ESRI Field Type
                                     // Mapping

    bool m_suppressColumnMappingError;
    bool m_forceMulti;
    bool m_bTimeInUTC = false;

    bool OGRFeatureFromGdbRow(Row *pRow, OGRFeature **ppFeature);

    virtual void CloseGDBObjects();

  public:
    virtual OGRFeature *GetNextFeature() override;
};

/************************************************************************/
/*                            FGdbLayer                                 */
/************************************************************************/

class FGdbDataSource;

class FGdbLayer final : public FGdbBaseLayer
{
    friend class FGdbDataSource;

    bool m_bWorkaroundCrashOnCDFWithBinaryField = false;

    virtual void CloseGDBObjects() override;

#ifdef EXTENT_WORKAROUND
    OGREnvelope sLayerEnvelope;
    bool m_bLayerEnvelopeValid;
    void WorkAroundExtentProblem();
    bool UpdateRowWithGeometry(Row &row, OGRGeometry *poGeom);
#endif

    std::vector<ByteArray *> m_apoByteArrays;
    OGRErr PopulateRowWithFeature(Row &row, OGRFeature *poFeature);
    OGRErr GetRow(EnumRows &enumRows, Row &row, GIntBig nFID);

  public:
    FGdbLayer();
    virtual ~FGdbLayer();

    // Internal used by FGDB driver */
    bool Initialize(FGdbDataSource *pParentDataSource, Table *pTable,
                    const std::wstring &wstrTablePath,
                    const std::wstring &wstrType);

    // virtual const char *GetName();
    virtual const char *GetFIDColumn() override
    {
        return m_strOIDFieldName.c_str();
    }

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    Table *GetTable()
    {
        return m_pTable;
    }

    std::wstring GetTablePath() const
    {
        return m_wstrTablePath;
    }

    std::wstring GetType() const
    {
        return m_wstrType;
    }

    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce) override;

    virtual GIntBig GetFeatureCount(int bForce) override;
    virtual OGRErr SetAttributeFilter(const char *pszQuery) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_pFeatureDefn;
    }

    virtual int TestCapability(const char *) override;

    // Access the XML directly. The 2 following methods are not currently used
    // by the driver, but can be used by external code for specific purposes.
    OGRErr GetLayerXML(char **poXml);
    OGRErr GetLayerMetadataXML(char **poXmlMeta);

    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain) override;

    GDALDataset *GetDataset() override;

  protected:
    bool GDBToOGRFields(CPLXMLNode *psFields);
    bool ParseGeometryDef(const CPLXMLNode *psGeometryDef);

    static bool ParseSpatialReference(const CPLXMLNode *psSpatialRefNode,
                                      std::string *pOutWkt,
                                      std::string *pOutWKID,
                                      std::string *pOutLatestWKID);

    FGdbDataSource *m_pDS;
    Table *m_pTable;

    std::string
        m_strName;  // contains underlying FGDB table name (not catalog name)

    std::string m_strOIDFieldName;
    std::string m_strShapeFieldName;

    std::wstring m_wstrTablePath;
    std::wstring m_wstrType;  // the type: "Table" or "Feature Class"

    std::wstring m_wstrSubfields;
    std::wstring m_wstrWhereClause;

    bool m_bFilterDirty;  // optimization to avoid multiple calls to search
                          // until necessary

    bool m_bLaunderReservedKeywords;
};

/************************************************************************/
/*                         FGdbResultLayer                              */
/************************************************************************/

class FGdbResultLayer final : public FGdbBaseLayer
{
  public:
    FGdbResultLayer(FGdbDataSource *pParentDataSource, const char *pszStatement,
                    EnumRows *pEnumRows);
    virtual ~FGdbResultLayer();

    virtual void ResetReading() override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_pFeatureDefn;
    }

    virtual int TestCapability(const char *) override;

  protected:
    FGdbDataSource *m_pDS;
    CPLString osSQL;
};

/************************************************************************/
/*                           FGdbDataSource                            */
/************************************************************************/

class FGdbDatabaseConnection;
class OGRFileGDBGroup;

class FGdbDataSource final : public GDALDataset
{
    CPLString m_osFSName;
    CPLString m_osPublicName;
    std::set<OGRLayer *> m_oSetSelectLayers;
    std::shared_ptr<GDALGroup> m_poRootGroup{};
    std::map<std::string, std::unique_ptr<GDALRelationship>>
        m_osMapRelationships{};

  public:
    FGdbDataSource(bool bUseDriverMutex, FGdbDatabaseConnection *pConnection,
                   bool bUseOpenFileGDB);
    virtual ~FGdbDataSource();

    int Open(const char *pszFSName, int bUpdate, const char *pszPublicName);

    const char *GetFSName()
    {
        return m_osFSName.c_str();
    }

    int GetLayerCount() override
    {
        return static_cast<int>(m_layers.size());
    }

    OGRLayer *GetLayer(int) override;

    virtual OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                                 OGRGeometry *poSpatialFilter,
                                 const char *pszDialect) override;
    virtual void ReleaseResultSet(OGRLayer *poResultsSet) override;

    int TestCapability(const char *) override;

    const OGRFieldDomain *
    GetFieldDomain(const std::string &name) const override;
    std::vector<std::string>
    GetFieldDomainNames(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string>
    GetRelationshipNames(CSLConstList papszOptions = nullptr) const override;

    const GDALRelationship *
    GetRelationship(const std::string &name) const override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poRootGroup;
    }

    Geodatabase *GetGDB()
    {
        return m_pGeodatabase;
    }

    FGdbDatabaseConnection *GetConnection()
    {
        return m_pConnection;
    }

    GDALDriver *GetOpenFileGDBDrv()
    {
        return m_poOpenFileGDBDrv;
    }

    int HasSelectLayers()
    {
        return !m_oSetSelectLayers.empty();
    }

    int CloseInternal(int bCloseGeodatabase = FALSE);

    bool UseOpenFileGDB() const
    {
        return m_bUseOpenFileGDB;
    }

    /*
    protected:

    void EnumerateSpatialTables();
    void OpenSpatialTable( const char* pszTableName );
    */
  protected:
    bool LoadLayers(const std::wstring &parent);
    bool OpenFGDBTables(OGRFileGDBGroup *group, const std::wstring &type,
                        const std::vector<std::wstring> &layers);

    bool m_bUseDriverMutex = true;
    FGdbDatabaseConnection *m_pConnection;
    std::vector<OGRLayer *> m_layers;
    Geodatabase *m_pGeodatabase;
    GDALDriver *m_poOpenFileGDBDrv;
    std::unique_ptr<GDALDataset> m_poOpenFileGDBDS;
    bool m_bUseOpenFileGDB = false;
};

/************************************************************************/
/*                              FGdbDriver                                */
/************************************************************************/

class FGdbDatabaseConnection
{
  public:
    FGdbDatabaseConnection(const std::string &osName, Geodatabase *pGeodatabase)
        : m_osName(osName), m_pGeodatabase(pGeodatabase), m_nRefCount(1),
          m_bLocked(FALSE)
    {
    }

    std::string m_osName;
    Geodatabase *m_pGeodatabase;
    int m_nRefCount;
    int m_bLocked;

    Geodatabase *GetGDB()
    {
        return m_pGeodatabase;
    }

    void SetLocked(int bLockedIn)
    {
        m_bLocked = bLockedIn;
    }

    int GetRefCount() const
    {
        return m_nRefCount;
    }

    int IsLocked() const
    {
        return m_bLocked;
    }

    int OpenGeodatabase(const char *pszOverriddenName);
    void CloseGeodatabase();
};

class FGdbDriver final : public GDALDriver
{
  public:
    static void Release(const char *pszName);

    static CPLMutex *hMutex;
};

CPL_C_START
void CPL_DLL RegisterOGRFileGDB();
CPL_C_END

#endif /* ndef _OGR_PG_H_INCLUDED */
