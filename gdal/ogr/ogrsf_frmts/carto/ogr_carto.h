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

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include <json.h>
#ifdef __clang
#pragma clang diagnostic pop
#endif


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
                         OGRCARTOLayer(OGRCARTODataSource* poDS);
    virtual ~OGRCARTOLayer();

    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn();
    virtual OGRFeatureDefn *    GetLayerDefnInternal(json_object* poObjIn) = 0;
    virtual json_object*        FetchNewFeatures(GIntBig iNext);

    virtual const char*         GetFIDColumn() { return osFIDColName.c_str(); }

    virtual int                 TestCapability( const char * );

    int                         GetFeaturesToFetch() {
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

    virtual CPLString    GetSRS_SQL(const char* pszGeomCol);

  public:
                         OGRCARTOTableLayer(OGRCARTODataSource* poDS, const char* pszName);
    virtual ~OGRCARTOTableLayer();

    virtual const char*         GetName() { return osName.c_str(); }
    virtual OGRFeatureDefn *    GetLayerDefnInternal(json_object* poObjIn);
    virtual json_object*        FetchNewFeatures(GIntBig iNext);

    virtual GIntBig             GetFeatureCount( int bForce = TRUE );
    virtual OGRFeature         *GetFeature( GIntBig nFeatureId );

    virtual int                 TestCapability( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRErr      DeleteField( int iField );

    virtual OGRFeature  *GetNextRawFeature();

    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );
    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( GIntBig nFID );

    virtual void        SetSpatialFilter( OGRGeometry *poGeom ) { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom );
    virtual OGRErr      SetAttributeFilter( const char * );

    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce );

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

    virtual CPLString    GetSRS_SQL(const char* pszGeomCol);

  public:
                        OGRCARTOResultLayer( OGRCARTODataSource* poDS,
                                               const char * pszRawStatement );
    virtual             ~OGRCARTOResultLayer();

    virtual OGRFeatureDefn *GetLayerDefnInternal(json_object* poObjIn);
    virtual OGRFeature  *GetNextRawFeature();

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

    virtual const char* GetName() { return pszName; }

    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer*   GetLayer( int );
    virtual OGRLayer    *GetLayerByName(const char *);

    virtual int         TestCapability( const char * );

    virtual OGRLayer   *ICreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );
    virtual OGRErr      DeleteLayer(int);

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    const char*                 GetAPIURL() const;
    bool                        IsReadWrite() const { return bReadWrite; }
    bool                        DoBatchInsert() const { return bBatchInsert; }
    char**                      AddHTTPOptions();
    json_object*                RunSQL(const char* pszUnescapedSQL);
    const CPLString&            GetCurrentSchema() { return osCurrentSchema; }
    int                         FetchSRSId( OGRSpatialReference * poSRS );

    int                         IsAuthenticatedConnection() { return osAPIKey.size() != 0; }
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
