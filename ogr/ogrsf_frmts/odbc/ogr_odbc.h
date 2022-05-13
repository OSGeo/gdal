/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/ODBC driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef OGR_ODBC_H_INCLUDED
#define OGR_ODBC_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_odbc.h"
#include "cpl_error.h"
#include <unordered_set>

/************************************************************************/
/*                            OGRODBCLayer                              */
/************************************************************************/

class OGRODBCDataSource;

class OGRODBCLayer CPL_NON_FINAL: public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    int                 m_nStatementFlags = 0;
    CPLODBCStatement   *poStmt;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;
    int                 nSRSId;

    GIntBig             iNextShapeId;

    OGRODBCDataSource    *poDS;

    int                bGeomColumnWKB;
    char               *pszGeomColumn;
    char               *pszFIDColumn;

    int                *panFieldOrdinals;

    bool                m_bEOF = false;

    CPLErr              BuildFeatureDefn( const char *pszLayerName,
                                          CPLODBCStatement *poStmt );

    virtual CPLODBCStatement *  GetStatement() { return poStmt; }

  public:
                        OGRODBCLayer();
    virtual             ~OGRODBCLayer();

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextRawFeature();
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRODBCTableLayer                          */
/************************************************************************/

class OGRODBCTableLayer final: public OGRODBCLayer
{
    char                *pszQuery;

    int                 bHaveSpatialExtents;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement() override;

    char               *pszTableName;
    char               *pszSchemaName;

  public:
    explicit            OGRODBCTableLayer( OGRODBCDataSource *, int );
                        virtual ~OGRODBCTableLayer();

    CPLErr              Initialize( const char *pszTableName,
                                    const char *pszGeomCol );

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRErr      SetAttributeFilter( const char * ) override;
#ifdef notdef
    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
#endif
    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int         TestCapability( const char * ) override;

#ifdef notdef
    // follow methods are not base class overrides
    void                SetLaunderFlag( int bFlag )
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( int bFlag )
                                { bPreservePrecision = bFlag; }
#endif
};

/************************************************************************/
/*                          OGRODBCSelectLayer                          */
/************************************************************************/

class OGRODBCSelectLayer final: public OGRODBCLayer
{
    char                *pszBaseStatement;

    void                ClearStatement();
    OGRErr              ResetStatement();

    virtual CPLODBCStatement *  GetStatement() override;

  public:
                        OGRODBCSelectLayer( OGRODBCDataSource *,
                                           CPLODBCStatement * );
                        virtual ~OGRODBCSelectLayer();

    virtual void        ResetReading() override;
    virtual GIntBig     GetFeatureCount( int ) override;

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRODBCDataSource                          */
/************************************************************************/

class OGRODBCDataSource final: public OGRDataSource
{
    OGRODBCLayer        **papoLayers;
    int                 nLayers;

    char               *pszName;

    CPLODBCSession      oSession;

    // We maintain a list of known SRID to reduce the number of trips to
    // the database to get SRSes.
    int                 nKnownSRID;
    int                *panSRID;
    OGRSpatialReference **papoSRS;

    // set of all lowercase table names. Note that this is only used when opening MDB datasources, not generic ODBC ones.
    std::unordered_set< std::string > m_aosAllLCTableNames;

    int                 m_nStatementFlags = 0;

    int                 OpenMDB(GDALOpenInfo *poOpenInfo );
    static bool         IsPrivateLayerName( const CPLString& osName );
  public:
                        OGRODBCDataSource();
                        virtual ~OGRODBCDataSource();

    int                 Open( GDALOpenInfo* poOpenInfo );
    int                 OpenTable( const char *pszTableName,
                                   const char *pszGeomCol );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;
    OGRLayer            *GetLayerByName( const char* ) override;
    bool                IsLayerPrivate( int ) const override;

    int                 TestCapability( const char * ) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    static bool         IsSupportedMsAccessFileExtension( const char* pszExtension );

    // Internal use
    CPLODBCSession     *GetSession() { return &oSession; }
};

#endif /* ndef OGR_ODBC_H_INCLUDED */
