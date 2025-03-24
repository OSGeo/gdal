/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBTableLayer class, access to an existing table
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_IDB_H_INCLUDED_
#define OGR_IDB_H_INCLUDED_

#include "ogrsf_frmts.h"
#include "cpl_error.h"
#include "idb_headers.h"

/************************************************************************/
/*                            OGRIDBLayer                              */
/************************************************************************/

class OGRIDBDataSource;

class OGRIDBLayer CPL_NON_FINAL : public OGRLayer
{
  protected:
    OGRFeatureDefn *poFeatureDefn;

    ITCursor *m_poCurr;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int nSRSId;

    int iNextShapeId;

    OGRIDBDataSource *poDS;

    int bGeomColumnWKB;
    char *pszGeomColumn;
    char *pszFIDColumn;

    CPLErr BuildFeatureDefn(const char *pszLayerName, ITCursor *poCurr);

    virtual ITCursor *GetQuery()
    {
        return m_poCurr;
    }

  public:
    OGRIDBLayer();
    virtual ~OGRIDBLayer();

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int TestCapability(const char *) override;
};

/************************************************************************/
/*                           OGRIDBTableLayer                          */
/************************************************************************/

class OGRIDBTableLayer final : public OGRIDBLayer
{
    int bUpdateAccess;

    char *pszQuery;

    int bHaveSpatialExtents;

    void ClearQuery();
    OGRErr ResetQuery();

    virtual ITCursor *GetQuery() override;

  public:
    explicit OGRIDBTableLayer(OGRIDBDataSource *);
    virtual ~OGRIDBTableLayer();

    CPLErr Initialize(const char *pszTableName, const char *pszGeomCol,
                      int bUpdate);

    virtual void ResetReading() override;
    virtual GIntBig GetFeatureCount(int) override;

    virtual OGRErr SetAttributeFilter(const char *) override;
    virtual OGRErr ISetFeature(OGRFeature *poFeature) override;
    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;

#if 0
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
#endif
    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int TestCapability(const char *) override;
};

/************************************************************************/
/*                          OGRIDBSelectLayer                          */
/************************************************************************/

class OGRIDBSelectLayer final : public OGRIDBLayer
{
    char *pszBaseQuery;

    void ClearQuery();
    OGRErr ResetQuery();

    virtual ITCursor *GetQuery() override;

  public:
    OGRIDBSelectLayer(OGRIDBDataSource *, ITCursor *);
    virtual ~OGRIDBSelectLayer();

    virtual void ResetReading() override;
    virtual GIntBig GetFeatureCount(int) override;

    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce) override;

    virtual int TestCapability(const char *) override;
};

/************************************************************************/
/*                           OGRIDBDataSource                          */
/************************************************************************/

class OGRIDBDataSource final : public GDALDataset
{
    OGRIDBLayer **papoLayers;
    int nLayers;

    int bDSUpdate;
    ITConnection *poConn;

  public:
    OGRIDBDataSource();
    virtual ~OGRIDBDataSource();

    int Open(const char *, int bUpdate, int bTestOpen);
    int OpenTable(const char *pszTableName, const char *pszGeomCol,
                  int bUpdate);

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;

    int TestCapability(const char *) override;

    virtual OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                                 OGRGeometry *poSpatialFilter,
                                 const char *pszDialect) override;
    virtual void ReleaseResultSet(OGRLayer *poLayer) override;

    // Internal use
    ITConnection *GetConnection()
    {
        return poConn;
    }
};

ITCallbackResult IDBErrorHandler(const ITErrorManager &err, void *userdata,
                                 long errorlevel);

#endif /* ndef _OGR_idb_H_INCLUDED_ */
