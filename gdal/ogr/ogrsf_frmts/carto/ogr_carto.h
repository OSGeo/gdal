/******************************************************************************
 * $Id$
 *
 * Project:  CARTO Translator
 * Purpose:  Definition of classes for OGR Carto driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_CARTO_H_INCLUDED
#define OGR_CARTO_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_http.h"

#include <vector>

#include "ogr_json_header.h"


json_object* OGRCARTOGetSingleRow(json_object* poObj);
CPLString OGRCARTOEscapeIdentifier(const char* pszStr);
CPLString OGRCARTOEscapeLiteral(const char* pszStr);

/************************************************************************/
/*                      OGRCartoGeomFieldDefn                         */
/************************************************************************/

class OGRCartoGeomFieldDefn: public OGRGeomFieldDefn
{
    public:
        int nSRID;

        OGRCartoGeomFieldDefn(const char* pszNameIn, OGRwkbGeometryType eType) :
                OGRGeomFieldDefn(pszNameIn, eType), nSRID(0)
        {
        }
};

/************************************************************************/
/*                           OGRCARTOLayer                            */
/************************************************************************/
class OGRCARTODataSource;

class OGRCARTOLayer : public OGRLayer
{
protected:
    OGRCARTODataSource* poDS;

    OGRFeatureDefn      *poFeatureDefn;
    CPLString            osBaseSQL;
    CPLString            osFIDColName;

    bool                 bEOF;
    int                  nFetchedObjects;
    int                  iNextInFetchedObjects;
    GIntBig              iNext;
    json_object         *poCachedObj;

    virtual OGRFeature  *GetNextRawFeature();
    OGRFeature          *BuildFeature(json_object* poRowObj);

    void                 EstablishLayerDefn(const char* pszLayerName,
                                            json_object* poObjIn);
    OGRSpatialReference *GetSRS(const char* pszGeomCol, int *pnSRID);
    virtual CPLString    GetSRS_SQL(const char* pszGeomCol) = 0;

  public:
    explicit OGRCARTOLayer(OGRCARTODataSource* poDS);
    virtual ~OGRCARTOLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature *        GetNextFeature() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override;
    virtual OGRFeatureDefn *    GetLayerDefnInternal(json_object* poObjIn) = 0;
    virtual json_object*        FetchNewFeatures(GIntBig iNext);

    virtual const char*         GetFIDColumn() override { return osFIDColName.c_str(); }

    virtual int                 TestCapability( const char * ) override;

    static int                         GetFeaturesToFetch() {
        return atoi(CPLGetConfigOption("CARTO_PAGE_SIZE",
                        CPLGetConfigOption("CARTODB_PAGE_SIZE", "500"))); }
};

typedef enum
{
    INSERT_UNINIT,
    INSERT_SINGLE_FEATURE,
    INSERT_MULTIPLE_FEATURE
} InsertState;

/************************************************************************/
/*                        OGRCARTOTableLayer                          */
/************************************************************************/

class OGRCARTOTableLayer : public OGRCARTOLayer
{
    CPLString           osName;
    CPLString           osQuery;
    CPLString           osWHERE;
    CPLString           osSELECTWithoutWHERE;

    bool                bLaunderColumnNames;

    bool                bInDeferredInsert;
    InsertState         eDeferredInsertState;
    CPLString           osDeferredInsertSQL;
    GIntBig             nNextFID;

    bool                bDeferredCreation;
    bool                bCartodbfy;
    int                 nMaxChunkSize;

    void                BuildWhere();

    virtual CPLString    GetSRS_SQL(const char* pszGeomCol) override;

  public:
                         OGRCARTOTableLayer(OGRCARTODataSource* poDS, const char* pszName);
    virtual ~OGRCARTOTableLayer();

    virtual const char*         GetName() override { return osName.c_str(); }
    virtual OGRFeatureDefn *    GetLayerDefnInternal(json_object* poObjIn) override;
    virtual json_object*        FetchNewFeatures(GIntBig iNext) override;

    virtual GIntBig             GetFeatureCount( int bForce = TRUE ) override;
    virtual OGRFeature         *GetFeature( GIntBig nFeatureId ) override;

    virtual int                 TestCapability( const char * ) override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;

    virtual OGRErr      DeleteField( int iField ) override;

    virtual OGRFeature  *GetNextRawFeature() override;

    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;

    virtual void        SetSpatialFilter( OGRGeometry *poGeom ) override { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override;
    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) override { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce ) override;

    void                SetLaunderFlag( bool bFlag )
        { bLaunderColumnNames = bFlag; }
    void                SetDeferredCreation( OGRwkbGeometryType eGType,
                                             OGRSpatialReference* poSRS,
                                             bool bGeomNullable,
                                             bool bCartodbfy);
    OGRErr              RunDeferredCreationIfNecessary();
    bool                GetDeferredCreation() const
        { return bDeferredCreation; }
    void                CancelDeferredCreation()
        { bDeferredCreation = false; bCartodbfy = false; }

    OGRErr              FlushDeferredInsert(bool bReset = true);
    void                RunDeferredCartofy();
};

/************************************************************************/
/*                       OGRCARTOResultLayer                          */
/************************************************************************/

class OGRCARTOResultLayer : public OGRCARTOLayer
{
    OGRFeature          *poFirstFeature;

    virtual CPLString    GetSRS_SQL(const char* pszGeomCol) override;

  public:
                        OGRCARTOResultLayer( OGRCARTODataSource* poDS,
                                               const char * pszRawStatement );
    virtual             ~OGRCARTOResultLayer();

    virtual OGRFeatureDefn *GetLayerDefnInternal(json_object* poObjIn) override;
    virtual OGRFeature  *GetNextRawFeature() override;

    bool                IsOK();
};

/************************************************************************/
/*                           OGRCARTODataSource                       */
/************************************************************************/

class OGRCARTODataSource : public OGRDataSource
{
    char*               pszName;
    char*               pszAccount;

    OGRCARTOTableLayer**  papoLayers;
    int                 nLayers;

    bool                bReadWrite;
    bool                bBatchInsert;

    bool                bUseHTTPS;

    CPLString           osAPIKey;

    bool                bMustCleanPersistent;

    CPLString           osCurrentSchema;

    int                 bHasOGRMetadataFunction;

    int                 nPostGISMajor;
    int                 nPostGISMinor;

  public:
                        OGRCARTODataSource();
    virtual ~OGRCARTODataSource();

    int                 Open( const char * pszFilename,
                              char** papszOpenOptions,
                              int bUpdate );

    virtual const char* GetName() override { return pszName; }

    virtual int         GetLayerCount() override { return nLayers; }
    virtual OGRLayer*   GetLayer( int ) override;
    virtual OGRLayer    *GetLayerByName(const char *) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRLayer   *ICreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL ) override;
    virtual OGRErr      DeleteLayer(int) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    const char*                 GetAPIURL() const;
    bool                        IsReadWrite() const { return bReadWrite; }
    bool                        DoBatchInsert() const { return bBatchInsert; }
    char**                      AddHTTPOptions();
    json_object*                RunSQL(const char* pszUnescapedSQL);
    const CPLString&            GetCurrentSchema() { return osCurrentSchema; }
    static int                         FetchSRSId( OGRSpatialReference * poSRS );

    int                         IsAuthenticatedConnection() { return !osAPIKey.empty(); }
    int                         HasOGRMetadataFunction() { return bHasOGRMetadataFunction; }
    void                        SetOGRMetadataFunction(int bFlag) { bHasOGRMetadataFunction = bFlag; }

    OGRLayer *                  ExecuteSQLInternal(
        const char *pszSQLCommand,
        OGRGeometry *poSpatialFilter = NULL,
        const char *pszDialect = NULL,
        bool bRunDeferredActions = false );

    int                         GetPostGISMajor() const { return nPostGISMajor; }
    int                         GetPostGISMinor() const { return nPostGISMinor; }
};

#endif /* ndef OGR_CARTO_H_INCLUDED */
