/******************************************************************************
 * $Id: ogr_gft.h 25483 2013-01-10 17:06:59Z warmerdam $
 *
 * Project:  Google Maps Engine (GME) API Driver
 * Purpose:  GME driver declarations
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *           (derived from GFT driver by Even)
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGR_GME_H_INCLUDED
#define _OGR_GME_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogrgeojsonreader.h"
#include "cpl_http.h"

#include <map>
#include <vector>

#include <json.h>

/************************************************************************/
/*                             OGRGMELayer                              */
/************************************************************************/
class OGRGMEDataSource;

class OGRGMELayer : public OGRLayer
{
    OGRGMEDataSource* poDS;

    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;

    int                iGeometryField;

    CPLString          osTableName;
    CPLString          osTableId;
    std::map<int, CPLString> oMapIdToGMEKey;
    std::vector<OGRFeature *> oListOfUpdatedFeatures;
    std::vector<OGRFeature *> oListOfInsertedFeatures;
    CPLString          osGeomColumnName;

    CPLString          osWhere;
    CPLString          osSelect;

    json_object*       current_feature_page;
    json_object*       current_features_array;
    int                index_in_page;
    unsigned int       iBatchPatchSize;
    bool               bDirty;

    void               GetPageOfFeatures();

    void               BuildWhere(void);

    int                FetchDescribe();

    OGRFeature        *GetNextRawFeature();
    void               GetPageOfFEatures();
    void               BatchPatch();
    void               BatchInsert();
    void               BatchRequest(const char *osMethod, std::vector<OGRFeature *> &oListOfFeatures);

  public:
    OGRGMELayer(OGRGMEDataSource* poDS, const char* pszTableId);
    ~OGRGMELayer();

    virtual void       ResetReading();
    void               SetBatchPatchSize(unsigned int iSize);

    virtual OGRFeatureDefn *    GetLayerDefn();

    virtual const char *GetName() { return osTableName.c_str(); }

    virtual OGRFeature *GetNextFeature();

    virtual const char *GetGeometryColumn() { return osGeomColumnName; }

    virtual int         TestCapability( const char * );

    virtual OGRErr      SetAttributeFilter( const char * pszWhere );

    virtual OGRErr      SetIgnoredFields(const char ** papszFields );

    virtual OGRErr      SyncToDisk();

    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
};

/************************************************************************/
/*                           OGRGMEDataSource                           */
/************************************************************************/

class OGRGMEDataSource : public OGRDataSource
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
    CPLString           osSelect;
    CPLString           osWhere;

    void                DeleteLayer( const char *pszLayerName );

    int                 bMustCleanPersistant;
    int                 nRetries;

  public:
                        OGRGMEDataSource();
                        ~OGRGMEDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char* GetName() { return pszName; }

    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer*   GetLayer( int );

    virtual int         TestCapability( const char * );

    CPLHTTPResult*      MakeRequest(const char *pszRequest,
                                    const char *pszMoreOptions = NULL);
    CPLHTTPResult*      PostRequest(const char *pszRequest,
                                    const char *pszBody);
    const CPLString&    GetAccessToken() const { return osAccessToken;}
    const char*         GetAPIURL() const;
    int                 IsReadWrite() const { return bReadWrite; }
    void                AddHTTPOptions(CPLStringList &oOptions);
    void                AddHTTPPostOptions(CPLStringList &oOptions);
    json_object*        Parse( const char* pszText );
    const char*         GetJSONString(json_object *parent, 
                                      const char *field_name,
                                      const char *default_value = NULL);

};

/************************************************************************/
/*                             OGRGMEDriver                             */
/************************************************************************/

class OGRGMEDriver : public OGRSFDriver
{
  public:
                ~OGRGMEDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual OGRDataSource*      CreateDataSource( const char * pszName,
                                                  char **papszOptions );
    virtual int                 TestCapability( const char * );
};

#endif /* ndef _OGR_GME_H_INCLUDED */
