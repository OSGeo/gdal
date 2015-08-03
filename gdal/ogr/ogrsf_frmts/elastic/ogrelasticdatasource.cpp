/******************************************************************************
 * $Id$
 *
 * Project:  ElasticSearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

// What was this supposed to do?
// #pragma warning( disable : 4251 )

#include "ogr_elastic.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_http.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRElasticDataSource()                        */
/************************************************************************/

OGRElasticDataSource::OGRElasticDataSource() {
    papoLayers = NULL;
    nLayers = 0;
    pszName = NULL;
    pszMapping = NULL;
    pszWriteMap = NULL;
}

/************************************************************************/
/*                       ~OGRElasticDataSource()                        */
/************************************************************************/

OGRElasticDataSource::~OGRElasticDataSource() {
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
    CPLFree(pszName);
    CPLFree(pszMapping);
    CPLFree(pszWriteMap);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRElasticDataSource::TestCapability(const char * pszCap) {
    if (EQUAL(pszCap, ODsCCreateLayer) ||
        EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRElasticDataSource::GetLayer(int iLayer) {
    if (iLayer < 0 || iLayer >= nLayers)
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer * OGRElasticDataSource::ICreateLayer(const char * pszLayerName,
                                              OGRSpatialReference *poSRS,
                                              OGRwkbGeometryType eGType,
                                              char ** papszOptions)
{
    if( bOverwrite || CSLFetchBoolean(papszOptions, "OVERWRITE", FALSE) )
    {
        // If we are overwriting, then delete the current index if it exists
        DeleteIndex(CPLSPrintf("%s/%s", GetName(), pszLayerName));
    }
    
    // Create the index
    if( !UploadFile(CPLSPrintf("%s/%s", GetName(), pszLayerName), "") )
        return NULL;

    // If we have a user specified mapping, then go ahead and update it now
    const char* pszLayerMapping = pszMapping;
    if (pszLayerMapping != NULL) {
        if( !UploadFile(CPLSPrintf("%s/%s/FeatureCollection/_mapping", GetName(), pszLayerName),
                   pszLayerMapping) )
        {
            return NULL;
        }
    }
    
    OGRElasticLayer* poLayer = new OGRElasticLayer(pszName, pszLayerName, this, TRUE, papszOptions);
    nLayers++;
    papoLayers = (OGRElasticLayer **) CPLRealloc(papoLayers, nLayers * sizeof (OGRElasticLayer*));
    papoLayers[nLayers - 1] = poLayer;
    
    if( eGType != wkbNone )
    {
        const char* pszGeometryName = CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "geometry");
        OGRGeomFieldDefn oFieldDefn(pszGeometryName, eGType);
        oFieldDefn.SetSpatialRef(poSRS);
        poLayer->CreateGeomField(&oFieldDefn, FALSE);
    }

    return poLayer;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRElasticDataSource::Open(CPL_UNUSED const char * pszFilename,
                               CPL_UNUSED int bUpdateIn) {
    CPLError(CE_Failure, CPLE_NotSupported,
            "OGR/Elastic driver does not support opening a file");
    return FALSE;
}


/************************************************************************/
/*                             DeleteIndex()                            */
/************************************************************************/

void OGRElasticDataSource::DeleteIndex(const CPLString &url) {
    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "CUSTOMREQUEST", "DELETE");
    CPLHTTPResult* psResult = CPLHTTPFetch(url, papszOptions);
    CSLDestroy(papszOptions);
    if (psResult) {
        CPLHTTPDestroyResult(psResult);
    }
}

/************************************************************************/
/*                            UploadFile()                              */
/************************************************************************/

int OGRElasticDataSource::UploadFile(const CPLString &url, const CPLString &data) {
    int bRet = TRUE;
    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", data.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
            "Content-Type: application/x-javascript; charset=UTF-8");

    CPLHTTPResult* psResult = CPLHTTPFetch(url, papszOptions);
    CSLDestroy(papszOptions);
    if (psResult) {
        if( psResult->pszErrBuf != NULL ||
            (psResult->pabyData && strncmp((const char*) psResult->pabyData, "{\"error\":", strlen("{\"error\":")) == 0) )
        {
            bRet = FALSE;
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                        psResult->pabyData ? (const char*) psResult->pabyData :
                        psResult->pszErrBuf);
        }
        CPLHTTPDestroyResult(psResult);
    }
    return bRet;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRElasticDataSource::Create(const char *pszFilename,
                                 CPL_UNUSED char **papszOptions) {
    this->pszName = CPLStrdup(pszFilename);

    const char* pszMetaFile = CPLGetConfigOption("ES_META", NULL);
    const char* pszWriteMap = CPLGetConfigOption("ES_WRITEMAP", NULL);;
    this->bOverwrite = CSLTestBoolean(CPLGetConfigOption("ES_OVERWRITE", "0"));
    this->nBulkUpload = (int) CPLAtof(CPLGetConfigOption("ES_BULK", "0"));

    if (pszWriteMap != NULL) {
        this->pszWriteMap = CPLStrdup(pszWriteMap);
    }

    // Read in the meta file from disk
    if (pszMetaFile != NULL)
    {
        int fsize;
        char *fdata;
        FILE *fp;

        fp = fopen(pszMetaFile, "rb");
        if (fp != NULL) {
            fseek(fp, 0, SEEK_END);
            fsize = (int) ftell(fp);

            fdata = (char *) malloc(fsize + 1);

            fseek(fp, 0, SEEK_SET);
            if (0 == fread(fdata, fsize, 1, fp)) {
                CPLError(CE_Failure, CPLE_FileIO,
                         "OGRElasticDataSource::Create read failed.");
            }
            fdata[fsize] = 0;
            this->pszMapping = fdata;
            fclose(fp);
        }
    }

    // Do a status check to ensure that the server is valid
    CPLHTTPResult* psResult = CPLHTTPFetch(CPLSPrintf("%s/_status", pszFilename), NULL);
    int bOK = (psResult != NULL && psResult->pszErrBuf == NULL);
    if (!bOK)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                "Could not connect to server");
    }

    CPLHTTPDestroyResult(psResult);

    return bOK;
}
