/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Definition of classes for OGR Google Fusion Tables driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_GFT_H_INCLUDED
#define OGR_GFT_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_http.h"

#include <vector>

/************************************************************************/
/*                             OGRGFTLayer                              */
/************************************************************************/
class OGRGFTDataSource;

class OGRGFTLayer : public OGRLayer
{
protected:
    OGRGFTDataSource* poDS;

    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;

    int                nNextInSeq;

    int                iGeometryField;
    int                iLatitudeField;
    int                iLongitudeField;
    int                bHiddenGeometryField;

    OGRFeature *       GetNextRawFeature();

    int                nOffset;
    int                bEOF;
    virtual int                FetchNextRows() = 0;

    std::vector<CPLString> aosRows;

    int                 bFirstTokenIsFID;
    OGRFeature*         BuildFeatureFromSQL(const char* pszLine);

    static CPLString    LaunderColName(const char* pszColName);

    void                SetGeomFieldName();

  public:
    explicit              OGRGFTLayer(OGRGFTDataSource* poDS);
                         virtual ~OGRGFTLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature *        GetNextFeature() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override;

    virtual int                 TestCapability( const char * ) override;

    virtual OGRErr              SetNextByIndex( GIntBig nIndex ) override;

    static const char *         GetDefaultGeometryColumnName() { return "geometry"; }

    static int                  ParseCSVResponse(char* pszLine,
                                                 std::vector<CPLString>& aosRes);
    static CPLString            PatchSQL(const char* pszSQL);

    int                         GetGeometryFieldIndex() { return iGeometryField; }
    int                         GetLatitudeFieldIndex() { return iLatitudeField; }
    int                         GetLongitudeFieldIndex() { return iLongitudeField; }

    static int                  GetFeaturesToFetch() { return atoi(CPLGetConfigOption("GFT_PAGE_SIZE", "500")); }
};

/************************************************************************/
/*                         OGRGFTTableLayer                             */
/************************************************************************/

class OGRGFTTableLayer : public OGRGFTLayer
{
    CPLString         osTableName;
    CPLString         osTableId;
    CPLString         osGeomColumnName;

    int                bHasTriedCreateTable;
    void               CreateTableIfNecessary();

    CPLString           osWHERE;
    CPLString           osQuery;

    void                BuildWhere();

    CPLString          osTransaction;
    int                bInTransaction;
    int                nFeaturesInTransaction;

    int                FetchDescribe();
    virtual int        FetchNextRows() override;

    OGRwkbGeometryType eGTypeForCreation;

    std::vector<CPLString>  aosColumnInternalName;

    public:
            OGRGFTTableLayer( OGRGFTDataSource* poDS,
                              const char* pszTableName,
                              const char* pszTableId = "",
                              const char* pszGeomColumnName = "" );
            virtual ~OGRGFTTableLayer();

    virtual void                ResetReading() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override;

    virtual const char *        GetName() override { return osTableName.c_str(); }
    virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;

    virtual OGRFeature *        GetFeature( GIntBig nFID ) override;

    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;

    virtual OGRErr      StartTransaction() override;
    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    const CPLString&            GetTableId() const { return osTableId; }

    virtual int                 TestCapability( const char * ) override;

    void                SetGeometryType(OGRwkbGeometryType eGType);
};

/************************************************************************/
/*                        OGRGFTResultLayer                             */
/************************************************************************/

class OGRGFTResultLayer : public OGRGFTLayer
{
    CPLString   osSQL;
    int         bGotAllRows;

    virtual int                FetchNextRows() override;

    public:
            OGRGFTResultLayer(OGRGFTDataSource* poDS,
                              const char* pszSQL);
            virtual ~OGRGFTResultLayer();

    virtual void                ResetReading() override;

    int     RunSQL();
};

/************************************************************************/
/*                           OGRGFTDataSource                           */
/************************************************************************/

class OGRGFTDataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

    int                 bReadWrite;

    int                 bUseHTTPS;

    CPLString           osAuth;
    CPLString           osAccessToken;
    CPLString           osRefreshToken;
    CPLString           osAPIKey;

    void                DeleteLayer( const char *pszLayerName );

    int                 bMustCleanPersistent;

    static CPLStringList ParseSimpleJson(const char *pszJSon);

  public:
                        OGRGFTDataSource();
                        virtual ~OGRGFTDataSource();

    int                 Open( const char * pszFilename,
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

    virtual OGRLayer*  ExecuteSQL( const char *pszSQLCommand,
                                   OGRGeometry *poSpatialFilter,
                                   const char *pszDialect ) override;
    virtual void       ReleaseResultSet( OGRLayer * poLayer ) override;

    const CPLString&            GetAccessToken() const { return osAccessToken;}
    const char*                 GetAPIURL() const;
    int                         IsReadWrite() const { return bReadWrite; }
    char**                      AddHTTPOptions(char** papszOptions = NULL);
    CPLHTTPResult*              RunSQL(const char* pszUnescapedSQL);
};

/************************************************************************/
/*                             OGRGFTDriver                             */
/************************************************************************/

class OGRGFTDriver : public OGRSFDriver
{
  public:
                virtual ~OGRGFTDriver();

    virtual const char*         GetName() override;
    virtual OGRDataSource*      Open( const char *, int ) override;
    virtual OGRDataSource*      CreateDataSource( const char * pszName,
                                                  char **papszOptions ) override;
    virtual int                 TestCapability( const char * ) override;
};

char **OGRGFTCSVSplitLine( const char *pszString, char chDelimiter );
char* OGRGFTGotoNextLine(char* pszData);

#endif /* ndef OGR_GFT_H_INCLUDED */
