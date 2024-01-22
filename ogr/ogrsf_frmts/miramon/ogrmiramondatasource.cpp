/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API to create a MiraMon layer
 * Author:   Abel Pau
 ******************************************************************************
 * Copyright (c) 2024, Xavier Pons
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

#include "ogrmiramon.h"

/************************************************************************/
/*                          OGRMiraMonDataSource()                          */
/************************************************************************/

OGRMiraMonDataSource::OGRMiraMonDataSource()
    : papoLayers(nullptr), nLayers(0), pszDSName(nullptr), bUpdate(false)
{
}

/************************************************************************/
/*                         ~OGRMiraMonDataSource()                          */
/************************************************************************/

OGRMiraMonDataSource::~OGRMiraMonDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
    CPLFree(pszDSName);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRMiraMonDataSource::Open(const char *pszFilename, VSILFILE *fp,
                           const OGRSpatialReference *poSRS, int bUpdateIn,
                            char **papszOpenOptionsUsr)

{
    bUpdate = CPL_TO_BOOL(bUpdateIn);

    OGRMiraMonLayer *poLayer = new OGRMiraMonLayer(pszFilename, fp, poSRS,
                bUpdate, papszOpenOptionsUsr);
    if (!poLayer->bValidFile)
    {
        delete poLayer;
        return FALSE;
    }

    papoLayers = static_cast<OGRMiraMonLayer **>(
        CPLRealloc(papoLayers, (size_t)(sizeof(OGRMiraMonLayer *) *
            ((size_t)nLayers + (size_t)1))));
    papoLayers[nLayers] = poLayer;
    nLayers++;

    CPLFree(pszDSName);
    pszDSName = CPLStrdup(pszFilename);

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new datasource.  This does not really do anything      */
/*      currently but save the name.                                    */
/************************************************************************/

int OGRMiraMonDataSource::Create(const char *pszDataSetName,
    char ** /* papszOptions */)

{
    pszDSName = CPLStrdup(pszDataSetName);

    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRMiraMonDataSource::ICreateLayer(const char *pszLayerName,
                                         const OGRSpatialReference *poSRS,
                                         OGRwkbGeometryType eType,
                                         char **papszOptions)
{
    CPLAssert(nullptr != pszLayerName);

    const char *osPath;
    CPLString osFilename(pszDSName);
    char *pszMMLayerName;
    const char *pszFullMMLayerName;
    const char *pszFlags = "wb+";

    if (osFilename == "/dev/stdout")
        osFilename = "/vsistdout";

    if (STARTS_WITH(osFilename, "/vsistdout"))
        pszFlags = "wb";

    // If the dataset has an extension, we understand that the path
    // of the file is where to write, and the layer name is the
    // dataset name (without extension).
    const char *pszExtension = CPLGetExtension(pszDSName);
    if(EQUAL(pszExtension, "pol") ||
        EQUAL(pszExtension, "arc") ||
        EQUAL(pszExtension, "pnt"))
    {
        pszMMLayerName = CPLStrdup(CPLResetExtension(pszDSName, ""));
        pszMMLayerName[strlen(pszMMLayerName)-1]='\0';

        pszFullMMLayerName = (const char *)pszMMLayerName;
        osPath = CPLGetPath(pszDSName);
    }
    else
    {
        pszMMLayerName = CPLStrdup(pszLayerName);
        osPath = pszDSName;
        pszFullMMLayerName = CPLFormFilename(pszDSName, pszLayerName, NULL);
    }

    // Let's create the folder if it's not already created.
    if (VSIMkdirRecursive(osPath,  0777) !=0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to create directory %s.",
                 pszDSName);
        return nullptr;
    }    
    
    /* -------------------------------------------------------------------- */
    /*      Return open layer handle.                                       */
    /* -------------------------------------------------------------------- */
    if (Open(pszFullMMLayerName, NULL, poSRS, TRUE,papszOptions))
    {
        CPLFree(pszMMLayerName);
        auto poLayer = papoLayers[nLayers - 1];
        return poLayer;
    }

    CPLFree(pszMMLayerName);
    return nullptr;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMiraMonDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;
    else if (EQUAL(pszCap, OLCFastFeatureCount))
        return TRUE; 

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRMiraMonDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}


