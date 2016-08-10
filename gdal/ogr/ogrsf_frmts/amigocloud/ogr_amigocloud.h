/******************************************************************************
 * $Id$
 *
 * Project:  AMIGOCLOUD Translator
 * Purpose:  Definition of classes for OGR AmigoCloud driver.
 * Author:   Victor Chernetsky, <victor at amigocloud dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Victor Chernetsky, <victor at amigocloud dot com>
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

#ifndef OGR_AMIGOCLOUD_H_INCLUDED
#define OGR_AMIGOCLOUD_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_http.h"

#include <vector>
#include <string>
#include <json.h>
#include <cpl_hash_set.h>
#include <cstdlib>

json_object* OGRAMIGOCLOUDGetSingleRow(json_object* poObj);
CPLString OGRAMIGOCLOUDEscapeIdentifier(const char* pszStr);
CPLString OGRAMIGOCLOUDEscapeLiteral(const char* pszStr);

/************************************************************************/
/*                      OGRAmigoCloudGeomFieldDefn                         */
/************************************************************************/

class OGRAmigoCloudGeomFieldDefn: public OGRGeomFieldDefn
{
    public:
        int nSRID;

        OGRAmigoCloudGeomFieldDefn(const char* pszNameIn, OGRwkbGeometryType eType) :
                OGRGeomFieldDefn(pszNameIn, eType), nSRID(0)
        {
        }
};

class OGRAmigoCloudFID
{
    public:
        GIntBig iIndex;
        GIntBig iFID;
        std::string osAmigoId;

        OGRAmigoCloudFID(const std::string &amigo_id, GIntBig index)
        {
            iIndex = index;
            OGRAmigoCloudFID::osAmigoId = amigo_id.c_str();
            iFID = std::abs((long)CPLHashSetHashStr(amigo_id.c_str()));
        }

        OGRAmigoCloudFID()
        {
            iIndex=0;
            iFID=0;
        }

        OGRAmigoCloudFID(const OGRAmigoCloudFID& fid)
        {
            iIndex = fid.iIndex;
            iFID = fid.iFID;
            osAmigoId = fid.osAmigoId.c_str();
        }
};

/************************************************************************/
/*                           OGRAmigoCloudLayer                            */
/************************************************************************/
class OGRAmigoCloudDataSource;

class OGRAmigoCloudLayer : public OGRLayer
{
    protected:
        OGRAmigoCloudDataSource* poDS;

        OGRFeatureDefn      *poFeatureDefn;
        CPLString            osBaseSQL;
        CPLString            osFIDColName;

        int                  bEOF;
        int                  nFetchedObjects;
        int                  iNextInFetchedObjects;
        GIntBig              iNext;
        json_object         *poCachedObj;

        std::map<GIntBig, OGRAmigoCloudFID>  mFIDs;

        virtual OGRFeature  *GetNextRawFeature();
        OGRFeature          *BuildFeature(json_object* poRowObj);

        void                 EstablishLayerDefn(const char* pszLayerName,
                                                json_object* poObjIn);
        OGRSpatialReference *GetSRS(const char* pszGeomCol, int *pnSRID);
        virtual CPLString    GetSRS_SQL(const char* pszGeomCol) = 0;

    public:
         OGRAmigoCloudLayer(OGRAmigoCloudDataSource* poDS);
        ~OGRAmigoCloudLayer();

        virtual void                ResetReading();
        virtual OGRFeature *        GetNextFeature();

        virtual OGRFeatureDefn *    GetLayerDefn();
        virtual OGRFeatureDefn *    GetLayerDefnInternal(json_object* poObjIn) = 0;
        virtual json_object*        FetchNewFeatures(GIntBig iNext);

        virtual const char*         GetFIDColumn() { return osFIDColName.c_str(); }

        virtual int                 TestCapability( const char * );

        int                         GetFeaturesToFetch() { return atoi(CPLGetConfigOption("AMIGOCLOUD_PAGE_SIZE", "500")); }
};


/************************************************************************/
/*                        OGRAmigoCloudTableLayer                          */
/************************************************************************/

class OGRAmigoCloudTableLayer : public OGRAmigoCloudLayer
{
    CPLString           osTableName;
    CPLString           osDatasetId;
    CPLString           osQuery;
    CPLString           osWHERE;
    CPLString           osSELECTWithoutWHERE;

    std::vector<std::string> vsDeferredInsertChangesets;
    GIntBig             nNextFID;

    int                 bDeferredCreation;
    int                 nMaxChunkSize;

    void                BuildWhere();

    virtual CPLString    GetSRS_SQL(const char* pszGeomCol);

    public:
         OGRAmigoCloudTableLayer(OGRAmigoCloudDataSource* poDS, const char* pszName);
        ~OGRAmigoCloudTableLayer();

        virtual const char        *GetName() { return osTableName.c_str(); }
                const char        *GetDatasetId() { return osDatasetId.c_str(); }
        virtual OGRFeatureDefn    *GetLayerDefnInternal(json_object* poObjIn);
        virtual json_object       *FetchNewFeatures(GIntBig iNext);

        virtual GIntBig             GetFeatureCount( int bForce = TRUE );
        virtual OGRFeature         *GetFeature( GIntBig nFeatureId );

        virtual int                 TestCapability( const char * );

        virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                         int bApproxOK = TRUE );

        virtual OGRFeature  *GetNextRawFeature();

        virtual OGRErr      ICreateFeature( OGRFeature *poFeature );
        virtual OGRErr      ISetFeature( OGRFeature *poFeature );
        virtual OGRErr      DeleteFeature( GIntBig nFID );

        virtual void        SetSpatialFilter( OGRGeometry *poGeom ) { SetSpatialFilter(0, poGeom); }
        virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom );
        virtual OGRErr      SetAttributeFilter( const char * );

        virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) { return GetExtent(0, psExtent, bForce); }
        virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce );

        void                SetDeferredCreation(OGRwkbGeometryType eGType,
                                   OGRSpatialReference *poSRS,
                                   int bGeomNullable);

        CPLString           GetAmigoCloudType(OGRFieldDefn& oField);

        OGRErr              RunDeferredCreationIfNecessary();
        int                 GetDeferredCreation() const { return bDeferredCreation; }
        void                CancelDeferredCreation() { bDeferredCreation = FALSE; }

        void                FlushDeferredInsert();
        bool                IsDatasetExists();
};

/************************************************************************/
/*                       OGRAmigoCloudResultLayer                          */
/************************************************************************/

class OGRAmigoCloudResultLayer : public OGRAmigoCloudLayer
{
        OGRFeature          *poFirstFeature;

        virtual CPLString    GetSRS_SQL(const char* pszGeomCol);

    public:
        OGRAmigoCloudResultLayer( OGRAmigoCloudDataSource* poDS,
                                               const char * pszRawStatement );
        virtual             ~OGRAmigoCloudResultLayer();

        virtual OGRFeatureDefn *GetLayerDefnInternal(json_object* poObjIn);
        virtual OGRFeature  *GetNextRawFeature();

        int                 IsOK();
};

/************************************************************************/
/*                           OGRAmigoCloudDataSource                       */
/************************************************************************/

class OGRAmigoCloudDataSource : public OGRDataSource
{
        char*               pszName;
        char*               pszProjetctId;

        OGRAmigoCloudTableLayer**  papoLayers;
        int                 nLayers;
        int                 bReadWrite;

        int                 bUseHTTPS;

        CPLString           osAPIKey;

        int                 bMustCleanPersistent;

        CPLString           osCurrentSchema;

        int                 bHasOGRMetadataFunction;

    public:
        OGRAmigoCloudDataSource();
        ~OGRAmigoCloudDataSource();

        int                 Open( const char * pszFilename,
                                  char** papszOpenOptions,
                                  int bUpdate );

        virtual const char* GetName() { return pszName; }

        virtual int         GetLayerCount() { return nLayers; }
        virtual OGRLayer   *GetLayer( int );
        virtual OGRLayer   *GetLayerByName(const char *);

        virtual int         TestCapability( const char * );

        virtual OGRLayer   *ICreateLayer( const char *pszName,
                                         OGRSpatialReference *poSpatialRef = NULL,
                                         OGRwkbGeometryType eGType = wkbUnknown,
                                         char ** papszOptions = NULL );
        virtual OGRErr      DeleteLayer(int);

        virtual OGRLayer   *ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect );
        virtual void        ReleaseResultSet( OGRLayer * poLayer );

        const char*                 GetAPIURL() const;
        int                         IsReadWrite() const { return bReadWrite; }
        const char*                 GetProjetcId() { return pszProjetctId;}
        char**                      AddHTTPOptions();
        json_object*                RunPOST(const char*pszURL, const char *pszPostData, const char *pszHeaders="HEADERS=Content-Type: application/json");
        json_object*                RunGET(const char*pszURL);
        json_object*                RunDELETE(const char*pszURL);
        json_object*                RunSQL(const char* pszUnescapedSQL);
        const CPLString&            GetCurrentSchema() { return osCurrentSchema; }
        int                         FetchSRSId( OGRSpatialReference * poSRS );

        int                         IsAuthenticatedConnection() { return osAPIKey.size() != 0; }
        int                         HasOGRMetadataFunction() { return bHasOGRMetadataFunction; }
        void                        SetOGRMetadataFunction(int bFlag) { bHasOGRMetadataFunction = bFlag; }

        OGRLayer *                  ExecuteSQLInternal( const char *pszSQLCommand,
                                                        OGRGeometry *poSpatialFilter = NULL,
                                                        const char *pszDialect = NULL,
                                                        int bRunDeferredActions = FALSE );
};

#endif /* ndef OGR_AMIGOCLOUD_H_INCLUDED */
