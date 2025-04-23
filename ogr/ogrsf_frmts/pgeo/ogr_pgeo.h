/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for Personal Geodatabase driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_ODBC_H_INCLUDED
#define OGR_ODBC_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_odbc.h"
#include "cpl_error.h"

#include <unordered_set>

/************************************************************************/
/*                            OGRPGeoLayer                              */
/************************************************************************/

class OGRPGeoDataSource;

constexpr const char *pszRelationshipTypeUUID =
    "{B606A7E1-FA5B-439C-849C6E9C2481537B}";

class OGRPGeoLayer CPL_NON_FINAL : public OGRLayer
{
  protected:
    OGRFeatureDefn *poFeatureDefn;

    int m_nStatementFlags = 0;

    CPLODBCStatement *poStmt;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int nSRSId;

    GIntBig iNextShapeId;

    OGRPGeoDataSource *poDS;

    char *pszGeomColumn;
    char *pszFIDColumn;

    int *panFieldOrdinals;

    bool m_bEOF = false;

    CPLErr BuildFeatureDefn(const char *pszLayerName, CPLODBCStatement *poStmt);

    virtual CPLODBCStatement *GetStatement()
    {
        return poStmt;
    }

    void LookupSRID(int);

  public:
    OGRPGeoLayer();
    virtual ~OGRPGeoLayer();

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual int TestCapability(const char *) override;

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;
};

/************************************************************************/
/*                           OGRPGeoTableLayer                            */
/************************************************************************/

class OGRPGeoTableLayer final : public OGRPGeoLayer
{
    char *pszQuery;

    void ClearStatement();
    OGRErr ResetStatement();

    virtual CPLODBCStatement *GetStatement() override;

    OGREnvelope sExtent;
    std::string m_osDefinition;
    std::string m_osDocumentation;

  public:
    explicit OGRPGeoTableLayer(OGRPGeoDataSource *, int);
    virtual ~OGRPGeoTableLayer();

    CPLErr Initialize(const char *pszTableName, const char *pszGeomCol,
                      int nShapeType, double dfExtentLeft, double dfExtentRight,
                      double dfExtentBottom, double dfExtentTop, int nSRID,
                      int bHasZ, int nHasM);

    virtual void ResetReading() override;
    virtual GIntBig GetFeatureCount(int) override;

    virtual OGRErr SetAttributeFilter(const char *) override;
    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    virtual int TestCapability(const char *) override;

    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce) override;

    const std::string &GetXMLDefinition()
    {
        return m_osDefinition;
    }

    const std::string &GetXMLDocumentation()
    {
        return m_osDocumentation;
    }
};

/************************************************************************/
/*                          OGRPGeoSelectLayer                          */
/************************************************************************/

class OGRPGeoSelectLayer final : public OGRPGeoLayer
{
    char *pszBaseStatement;

    void ClearStatement();
    OGRErr ResetStatement();

    virtual CPLODBCStatement *GetStatement() override;

  public:
    OGRPGeoSelectLayer(OGRPGeoDataSource *, CPLODBCStatement *);
    virtual ~OGRPGeoSelectLayer();

    virtual void ResetReading() override;
    virtual GIntBig GetFeatureCount(int) override;

    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    virtual int TestCapability(const char *) override;
};

/************************************************************************/
/*                           OGRPGeoDataSource                          */
/************************************************************************/

class OGRPGeoDataSource final : public GDALDataset
{
    OGRPGeoLayer **papoLayers;
    int nLayers;

    mutable CPLODBCSession oSession;

    std::unordered_set<std::string> m_aosAllLCTableNames;
    bool m_bHasGdbItemsTable = false;

    std::vector<std::unique_ptr<OGRLayer>> m_apoInvisibleLayers;

    std::map<std::string, std::unique_ptr<GDALRelationship>>
        m_osMapRelationships{};

#ifndef _WIN32
    mutable bool m_COUNT_STAR_state_known = false;
    mutable bool m_COUNT_STAR_working = false;
#endif

    int m_nStatementFlags = 0;

    static bool IsPrivateLayerName(const CPLString &osName);

  public:
    OGRPGeoDataSource();
    virtual ~OGRPGeoDataSource();

    int Open(GDALOpenInfo *poOpenInfo);
    int OpenTable(const char *pszTableName, const char *pszGeomCol,
                  int bUpdate);

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;
    OGRLayer *GetLayerByName(const char *) override;
    bool IsLayerPrivate(int) const override;

    int TestCapability(const char *) override;

    virtual OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                                 OGRGeometry *poSpatialFilter,
                                 const char *pszDialect) override;
    virtual void ReleaseResultSet(OGRLayer *poLayer) override;

    std::vector<std::string>
    GetRelationshipNames(CSLConstList papszOptions = nullptr) const override;

    const GDALRelationship *
    GetRelationship(const std::string &name) const override;

    // Internal use
    CPLODBCSession *GetSession()
    {
        return &oSession;
    }

    bool CountStarWorking() const;

    bool HasGdbItemsTable() const
    {
        return m_bHasGdbItemsTable;
    }
};

#endif /* ndef _OGR_PGeo_H_INCLUDED */
