/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Declarations for MySQL OGR Driver Classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Author:   Howard Butler, hobu@hobu.net
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_MYSQL_H_INCLUDED
#define OGR_MYSQL_H_INCLUDED

// Include cpl_port.h before mysql stuff to avoid issues with CPLIsFinite()
// See https://trac.osgeo.org/gdal/ticket/6899
#include "cpl_port.h"

#ifdef _MSC_VER
#pragma warning(push)
// 'my_alignment_imp<0x02>' : structure was padded due to __declspec(align())
#pragma warning(disable : 4324)
// nonstandard extension used : nameless struct/union
#pragma warning(disable : 4201)
// nonstandard extension used : redefined extern to static
#pragma warning(disable : 4211)
// warning C4005: 'HAVE_STRUCT_TIMESPEC': macro redefinition
#pragma warning(disable : 4005)
#endif

#include <mysql.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/* my_global.h from mysql 5.1 declares the min and max macros. */
/* This conflicts with templates in g++-4.3.2 header files. Grrr */
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef bool
#undef bool
#endif

#include "ogrsf_frmts.h"

#include <map>

class OGRMySQLDataSource;

/************************************************************************/
/*                        OGRMySQLGeomFieldDefn                         */
/************************************************************************/

class OGRMySQLGeomFieldDefn final : public OGRGeomFieldDefn
{
    OGRMySQLGeomFieldDefn(const OGRMySQLGeomFieldDefn &) = delete;
    OGRMySQLGeomFieldDefn &operator=(const OGRMySQLGeomFieldDefn &) = delete;

  protected:
    OGRMySQLDataSource *poDS;

  public:
    OGRMySQLGeomFieldDefn(OGRMySQLDataSource *poDSIn, const char *pszFieldName)
        : OGRGeomFieldDefn(pszFieldName, wkbUnknown), poDS(poDSIn)
    {
    }

    const OGRSpatialReference *GetSpatialRef() const override;

    void UnsetDataSource()
    {
        poDS = nullptr;
    }

    mutable int nSRSId = -1;
};

/************************************************************************/
/*                            OGRMySQLLayer                             */
/************************************************************************/

class OGRMySQLLayer CPL_NON_FINAL : public OGRLayer
{
  protected:
    OGRMySQLDataSource *poDS;

    OGRFeatureDefn *poFeatureDefn = nullptr;

    // Layer srid.
    int nSRSId = -2;  // we haven't even queried the database for it yet.

    GIntBig iNextShapeId = 0;

    char *pszQueryStatement = nullptr;

    int nResultOffset = 0;

    char *pszGeomColumn = nullptr;
    char *pszGeomColumnTable = nullptr;
    int nGeomType = 0;

    int bHasFid = FALSE;
    char *pszFIDColumn = nullptr;

    MYSQL_RES *hResultSet = nullptr;
    bool m_bEOF = false;

    int FetchSRSId();

  public:
    explicit OGRMySQLLayer(OGRMySQLDataSource *poDSIn);
    ~OGRMySQLLayer() override;

    void ResetReading() override;

    OGRFeature *GetNextFeature() override;

    OGRFeature *GetFeature(GIntBig nFeatureId) override;

    using OGRLayer::GetLayerDefn;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return poFeatureDefn;
    }

    const char *GetFIDColumn() const override;

    /* custom methods */
    virtual OGRFeature *RecordToFeature(char **papszRow, unsigned long *);
    virtual OGRFeature *GetNextRawFeature();

    GDALDataset *GetDataset() override;
};

/************************************************************************/
/*                          OGRMySQLTableLayer                          */
/************************************************************************/

class OGRMySQLTableLayer final : public OGRMySQLLayer
{
    int bUpdateAccess;

    OGRFeatureDefn *ReadTableDefinition(const char *);

    void BuildWhere();
    char *BuildFields();
    void BuildFullQueryStatement();

    char *pszQuery;
    char *pszWHERE;

    int bLaunderColumnNames;
    int bPreservePrecision;

  public:
    OGRMySQLTableLayer(OGRMySQLDataSource *, const char *pszName, int bUpdate,
                       int nSRSId = -2);
    ~OGRMySQLTableLayer() override;

    OGRErr Initialize(const char *pszTableName);

    OGRFeature *GetFeature(GIntBig nFeatureId) override;
    void ResetReading() override;
    GIntBig GetFeatureCount(int) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRErr SetAttributeFilter(const char *) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr DeleteFeature(GIntBig nFID) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;

    void SetLaunderFlag(int bFlag)
    {
        bLaunderColumnNames = bFlag;
    }

    void SetPrecisionFlag(int bFlag)
    {
        bPreservePrecision = bFlag;
    }

    int TestCapability(const char *) const override;

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;
};

/************************************************************************/
/*                         OGRMySQLResultLayer                          */
/************************************************************************/

class OGRMySQLResultLayer final : public OGRMySQLLayer
{
    void BuildFullQueryStatement();

    char *pszRawStatement;

  public:
    OGRMySQLResultLayer(OGRMySQLDataSource *, const char *pszRawStatement,
                        MYSQL_RES *hResultSetIn);
    ~OGRMySQLResultLayer() override;

    OGRFeatureDefn *ReadResultDefinition();

    void ResetReading() override;
    GIntBig GetFeatureCount(int) override;

    int TestCapability(const char *) const override;
};

/************************************************************************/
/*                          OGRMySQLDataSource                          */
/************************************************************************/

class OGRMySQLDataSource final : public GDALDataset
{
    OGRMySQLLayer **papoLayers;
    int nLayers;

    int bDSUpdate;

    MYSQL *hConn;

    OGRErr DeleteLayer(int iLayer) override;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    std::map<int,
             std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser>>
        m_oSRSCache{};

    OGRMySQLLayer *poLongResultLayer;

    bool m_bIsMariaDB = false;
    int m_nMajor = 0;
    int m_nMinor = 0;

  public:
    OGRMySQLDataSource();
    ~OGRMySQLDataSource() override;

    MYSQL *GetConn()
    {
        return hConn;
    }

    int FetchSRSId(const OGRSpatialReference *poSRS);

    const OGRSpatialReference *FetchSRS(int nSRSId);

    OGRErr InitializeMetadataTables();
    OGRErr UpdateMetadataTables(const char *pszLayerName,
                                OGRwkbGeometryType eType,
                                const char *pszGeomColumnName,
                                const int nSRSId);

    int Open(const char *, char **papszOpenOptions, int bUpdate);
    int OpenTable(const char *, int bUpdate);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *) const override;

    OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                         OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
    void ReleaseResultSet(OGRLayer *poLayer) override;

    // nonstandard

    void ReportError(const char * = nullptr);

    static char *LaunderName(const char *);

    void RequestLongResult(OGRMySQLLayer *);
    void InterruptLongResult();

    bool IsMariaDB() const
    {
        return m_bIsMariaDB;
    }

    int GetMajorVersion() const
    {
        return m_nMajor;
    }

    int GetUnknownSRID() const;
};

std::string OGRMySQLEscapeLiteral(const char *pszLiteral);

#endif /* ndef OGR_MYSQL_H_INCLUDED */
