/******************************************************************************
 * $Id $
 *
 * Project:  ElasticSearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
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

#ifndef _OGR_ELASTIC_H_INCLUDED
#define _OGR_ELASTIC_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "cpl_hash_set.h"

class OGRElasticDataSource;

/************************************************************************/
/*                             OGRElasticLayer                              */

/************************************************************************/

class OGRElasticLayer : public OGRLayer {
    OGRFeatureDefn* poFeatureDefn;
    OGRSpatialReference* poSRS;
    OGRElasticDataSource* poDS;
    CPLString sIndex;
    void* pAttributes;
    char* pszLayerName;

public:
    OGRElasticLayer(const char *pszFilename,
            const char* layerName,
            OGRElasticDataSource* poDS,
            OGRSpatialReference *poSRSIn,
            int bWriteMode = FALSE);
    ~OGRElasticLayer();

    void ResetReading();
    OGRFeature * GetNextFeature();

    OGRErr CreateFeature(OGRFeature *poFeature);
    OGRErr CreateField(OGRFieldDefn *poField, int bApproxOK);

    OGRFeatureDefn * GetLayerDefn();

    int TestCapability(const char *);

    OGRSpatialReference *GetSpatialRef();

    int GetFeatureCount(int bForce);

    void PushIndex();
    CPLString BuildMap();
};

/************************************************************************/
/*                           OGRElasticDataSource                           */

/************************************************************************/

class OGRElasticDataSource : public OGRDataSource {
    char* pszName;
    char* pszServer;

    OGRElasticLayer** papoLayers;
    int nLayers;

public:
    OGRElasticDataSource();
    ~OGRElasticDataSource();

    int Open(const char * pszFilename,
            int bUpdate);

    int Create(const char *pszFilename,
            char **papszOptions);

    const char* GetName() {
        return pszName;
    }

    int GetLayerCount() {
        return nLayers;
    }
    OGRLayer* GetLayer(int);

    OGRLayer * CreateLayer(const char * pszLayerName,
            OGRSpatialReference *poSRS,
            OGRwkbGeometryType eType,
            char ** papszOptions);

    int TestCapability(const char *);

    void UploadFile(const CPLString &url, const CPLString &data);
    void DeleteIndex(const CPLString &url);

    int bOverwrite;
    int nBulkUpload;
    char* pszWriteMap;
    char* pszMapping;
};

/************************************************************************/
/*                             OGRElasticDriver                             */

/************************************************************************/

class OGRElasticDriver : public OGRSFDriver {
public:
    ~OGRElasticDriver();

    const char* GetName();
    OGRDataSource* Open(const char *, int);
    OGRDataSource* CreateDataSource(const char * pszName, char **papszOptions);
    int TestCapability(const char *);

};


#endif /* ndef _OGR_Elastic_H_INCLUDED */
