/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definition of classes and functions used by SQLite and GPKG drivers
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_SQLITE_BASE_H_INCLUDED
#define OGR_SQLITE_BASE_H_INCLUDED

#include "cpl_vsi.h"
#include "cpl_string.h"
#include "gdal_pam.h"
#include "ogrsf_frmts.h"

#include <sqlite3.h>

#include <map>
#include <utility>
#include <vector>

/************************************************************************/
/*      Format used to store geometry data in the database.             */
/************************************************************************/

enum OGRSQLiteGeomFormat
{
    OSGF_None = 0,
    OSGF_WKT = 1,
    OSGF_WKB = 2,
    OSGF_FGF = 3,
    OSGF_SpatiaLite = 4
};

/************************************************************************/
/*                        OGRSQLiteGeomFieldDefn                        */
/************************************************************************/

class OGRSQLiteGeomFieldDefn final : public OGRGeomFieldDefn
{
    public:
        OGRSQLiteGeomFieldDefn( const char* pszNameIn, int iGeomColIn ) :
            OGRGeomFieldDefn(pszNameIn, wkbUnknown), nSRSId(-1),
            iCol(iGeomColIn), bTriedAsSpatiaLite(FALSE), eGeomFormat(OSGF_None),
            bCachedExtentIsValid(FALSE), bHasSpatialIndex(FALSE),
            bHasCheckedSpatialIndexTable(FALSE)
            {
            }

        int nSRSId;
        int iCol; /* ordinal of geometry field in SQL statement */
        int bTriedAsSpatiaLite;
        OGRSQLiteGeomFormat eGeomFormat;
        OGREnvelope         oCachedExtent;
        int                 bCachedExtentIsValid;
        int                 bHasSpatialIndex;
        int                 bHasCheckedSpatialIndexTable;
        std::vector< std::pair<CPLString,CPLString> > aosDisabledTriggers;
};

/************************************************************************/
/*                        OGRSQLiteFeatureDefn                          */
/************************************************************************/

class OGRSQLiteFeatureDefn final : public OGRFeatureDefn
{
    public:
        explicit OGRSQLiteFeatureDefn( const char * pszName = nullptr ) :
            OGRFeatureDefn(pszName)
        {
            SetGeomType(wkbNone);
        }

        OGRSQLiteGeomFieldDefn* myGetGeomFieldDefn(int i)
        {
            return (OGRSQLiteGeomFieldDefn*) GetGeomFieldDefn(i);
        }
};

/************************************************************************/
/*                       IOGRSQLiteGetSpatialWhere                      */
/************************************************************************/

class IOGRSQLiteGetSpatialWhere
{
  public:
    virtual              ~IOGRSQLiteGetSpatialWhere() {}

    virtual int           HasFastSpatialFilter(int iGeomCol) = 0;
    virtual CPLString     GetSpatialWhere(int iGeomCol,
                                          OGRGeometry* poFilterGeom) = 0;
};

/************************************************************************/
/*                       OGRSQLiteBaseDataSource                        */
/************************************************************************/

/* Used by both OGRSQLiteDataSource and OGRGeoPackageDataSource */
class OGRSQLiteBaseDataSource CPL_NON_FINAL: public GDALPamDataset
{
  protected:
    char               *m_pszFilename = nullptr;
    bool                m_bCallUndeclareFileNotToOpen = false;

    sqlite3             *hDB = nullptr;

    sqlite3_vfs*        pMyVFS = nullptr;

    VSILFILE*           fpMainFile = nullptr; /* Set by the VFS layer when it opens the DB */
                                    /* Must *NOT* be closed by the datasource explicitly. */

    int                 OpenOrCreateDB(int flags, int bRegisterOGR2SQLiteExtensions);
    bool                SetSynchronous();
    bool                SetCacheSize();

    void                CloseDB();

    std::map<CPLString, OGREnvelope> oMapSQLEnvelope{};

    void               *hSpatialiteCtxt = nullptr;
    bool                InitNewSpatialite();
    void                FinishNewSpatialite();

    int                 bUserTransactionActive = FALSE;
    int                 nSoftTransactionLevel = 0;

    OGRErr              DoTransactionCommand(const char* pszCommand);

  public:
                        OGRSQLiteBaseDataSource();
                        virtual ~OGRSQLiteBaseDataSource();

    sqlite3            *GetDB() { return hDB; }
    inline bool         GetUpdate() const { return eAccess == GA_Update; }

    void                NotifyFileOpened (const char* pszFilename,
                                          VSILFILE* fp);

    const OGREnvelope*  GetEnvelopeFromSQL(const CPLString& osSQL);
    void                SetEnvelopeForSQL(const CPLString& osSQL, const OGREnvelope& oEnvelope);

    virtual std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> GetLayerWithGetSpatialWhereByName( const char* pszName ) = 0;

    virtual OGRErr     AbortSQL() override;

    virtual OGRErr      StartTransaction(int bForce = FALSE) override;
    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    virtual int         TestCapability( const char * ) override;

    virtual void        *GetInternalHandle( const char * ) override;

    OGRErr              SoftStartTransaction();
    OGRErr              SoftCommitTransaction();
    OGRErr              SoftRollbackTransaction();
};

/************************************************************************/
/*                         IOGRSQLiteSelectLayer                        */
/************************************************************************/

class IOGRSQLiteSelectLayer
{
    public:
        virtual                     ~IOGRSQLiteSelectLayer() {}

        virtual char*&               GetAttrQueryString() = 0;
        virtual OGRFeatureQuery*&    GetFeatureQuery() = 0;
        virtual OGRGeometry*&        GetFilterGeom() = 0;
        virtual int&                 GetIGeomFieldFilter() = 0;
        virtual OGRSpatialReference* GetSpatialRef() = 0;
        virtual OGRFeatureDefn      *GetLayerDefn() = 0;
        virtual int                  InstallFilter( OGRGeometry * ) = 0;
        virtual int                  HasReadFeature() = 0;
        virtual void                 BaseResetReading() = 0;
        virtual OGRFeature          *BaseGetNextFeature() = 0;
        virtual OGRErr               BaseSetAttributeFilter(const char* pszQuery) = 0;
        virtual GIntBig              BaseGetFeatureCount(int bForce) = 0;
        virtual int                  BaseTestCapability( const char * ) = 0;
        virtual OGRErr               BaseGetExtent(OGREnvelope *psExtent, int bForce) = 0;
        virtual OGRErr               BaseGetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) = 0;
};

/************************************************************************/
/*                   OGRSQLiteSelectLayerCommonBehaviour                */
/************************************************************************/

class OGRSQLiteSelectLayerCommonBehaviour
{
    OGRSQLiteBaseDataSource *poDS;
    IOGRSQLiteSelectLayer   *poLayer;

    CPLString           osSQLBase;

    int                 bEmptyLayer;
    int                 bAllowResetReadingEvenIfIndexAtZero;
    int                 bSpatialFilterInSQL;

    std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> GetBaseLayer(size_t& i);
    int                 BuildSQL();

  public:

    CPLString           osSQLCurrent;

        OGRSQLiteSelectLayerCommonBehaviour(OGRSQLiteBaseDataSource* poDS,
                                            IOGRSQLiteSelectLayer* poBaseLayer,
                                            CPLString osSQL,
                                            int bEmptyLayer);

    void        ResetReading();
    OGRFeature *GetNextFeature();
    GIntBig     GetFeatureCount( int );
    void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    OGRErr      SetAttributeFilter( const char * );
    int         TestCapability( const char * );
    OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce);
};

/************************************************************************/
/*                   OGRSQLiteSingleFeatureLayer                        */
/************************************************************************/

class OGRSQLiteSingleFeatureLayer final : public OGRLayer
{
  private:
    int                 nVal;
    char               *pszVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
                        OGRSQLiteSingleFeatureLayer( const char* pszLayerName,
                                                     int nVal );
                        OGRSQLiteSingleFeatureLayer( const char* pszLayerName,
                                                     const char *pszVal );
                        virtual ~OGRSQLiteSingleFeatureLayer();

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeatureDefn *GetLayerDefn() override;
    virtual int         TestCapability( const char * ) override;
};

/************************************************************************/
/* Functions                                                            */
/************************************************************************/

OGRErr       OGRSQLiteGetSpatialiteGeometryHeader( const GByte *pabyData,
                                                    int nBytes,
                                                    int* pnSRID,
                                                    OGRwkbGeometryType* peType,
                                                    bool* pbIsEmpty,
                                                    double* pdfMinX,
                                                    double* pdfMinY,
                                                    double* pdfMaxX,
                                                    double* pdfMaxY );
// CPL_DLL just for spatialite_geom_import_fuzzer
OGRErr CPL_DLL OGRSQLiteImportSpatiaLiteGeometry( const GByte *, int,
                                                OGRGeometry **,
                                                int *pnSRID = nullptr);
OGRErr       OGRSQLiteExportSpatiaLiteGeometry( const OGRGeometry *,
                                                  GInt32, OGRwkbByteOrder,
                                                  int, int bUseComprGeom, GByte **, int * );
#endif // OGR_SQLITE_BASE_H_INCLUDED
