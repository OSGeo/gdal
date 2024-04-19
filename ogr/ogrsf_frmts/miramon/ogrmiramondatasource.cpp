/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMiraMonDataSource class.
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

/****************************************************************************/
/*                          OGRMiraMonDataSource()                          */
/****************************************************************************/
OGRMiraMonDataSource::OGRMiraMonDataSource()
    : papoLayers(nullptr), nLayers(0), pszRootName(nullptr), pszDSName(nullptr),
      bUpdate(false)

{
    MMMap.nNumberOfLayers = 0;
    MMMap.fMMMap = nullptr;
}

/****************************************************************************/
/*                         ~OGRMiraMonDataSource()                          */
/****************************************************************************/

OGRMiraMonDataSource::~OGRMiraMonDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
    CPLFree(pszDSName);
    CPLFree(pszRootName);

    if (MMMap.fMMMap)
        VSIFCloseL(MMMap.fMMMap);
}

/****************************************************************************/
/*                                Open()                                    */
/****************************************************************************/

int OGRMiraMonDataSource::Open(const char *pszFilename, VSILFILE *fp,
                               const OGRSpatialReference *poSRS, int bUpdateIn,
                               CSLConstList papszOpenOptionsUsr)

{
    bUpdate = CPL_TO_BOOL(bUpdateIn);

    OGRMiraMonLayer *poLayer = new OGRMiraMonLayer(
        this, pszFilename, fp, poSRS, bUpdate, papszOpenOptionsUsr, &MMMap);
    if (!poLayer->bValidFile)
    {
        delete poLayer;
        return FALSE;
    }
    papoLayers = static_cast<OGRMiraMonLayer **>(CPLRealloc(
        papoLayers,
        (size_t)(sizeof(OGRMiraMonLayer *) * ((size_t)nLayers + (size_t)1))));
    papoLayers[nLayers] = poLayer;
    nLayers++;

    if (pszDSName)
    {
        const char *pszExtension = CPLGetExtension(pszDSName);
        if (!EQUAL(pszExtension, "pol") && !EQUAL(pszExtension, "arc") &&
            !EQUAL(pszExtension, "pnt"))
        {
            CPLStrlcpy(
                MMMap.pszMapName,
                CPLFormFilename(pszDSName, CPLGetBasename(pszDSName), "mmm"),
                sizeof(MMMap.pszMapName));
            if (!MMMap.nNumberOfLayers)
            {
                MMMap.fMMMap = VSIFOpenL(MMMap.pszMapName, "w+");
                if (!MMMap.fMMMap)
                {
                    // It could be an error but it is not so important
                    // to stop the process. This map is an extra element
                    // to open all layers in one click, at least in MiraMon
                    // software.
                    *MMMap.pszMapName = '\0';
                }
                else
                {
                    VSIFPrintfL(MMMap.fMMMap, "[VERSIO]\n");
                    VSIFPrintfL(MMMap.fMMMap, "Vers=2\n");
                    VSIFPrintfL(MMMap.fMMMap, "SubVers=0\n");
                    VSIFPrintfL(MMMap.fMMMap, "variant=b\n");
                    VSIFPrintfL(MMMap.fMMMap, "\n");
                    VSIFPrintfL(MMMap.fMMMap, "[DOCUMENT]\n");
                    VSIFPrintfL(MMMap.fMMMap, "Titol= %s(map)\n",
                                CPLGetBasename(poLayer->GetName()));
                    VSIFPrintfL(MMMap.fMMMap, "\n");
                }
            }
        }
        else
            *MMMap.pszMapName = '\0';
    }
    else
        *MMMap.pszMapName = '\0';

    if (pszDSName)
        CPLFree(pszDSName);
    pszDSName = CPLStrdup(pszFilename);

    return TRUE;
}

/****************************************************************************/
/*                               Create()                                   */
/*                                                                          */
/*      Create a new datasource.  This does not really do anything          */
/*      currently but save the name.                                        */
/****************************************************************************/

int OGRMiraMonDataSource::Create(const char *pszDataSetName,
                                 char ** /* papszOptions */)

{
    bUpdate = TRUE;
    pszDSName = CPLStrdup(pszDataSetName);
    pszRootName = CPLStrdup(pszDataSetName);

    return TRUE;
}

/****************************************************************************/
/*                           ICreateLayer()                                 */
/****************************************************************************/

OGRLayer *
OGRMiraMonDataSource::ICreateLayer(const char *pszLayerName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions)
{
    CPLAssert(nullptr != pszLayerName);

    const auto eType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSRS =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    // It's a seed to be able to generate a random identifier in
    // MMGenerateFileIdentifierFromMetadataFileName() function
    srand((unsigned int)time(nullptr));

    if (OGR_GT_HasM(eType))
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Measures in this layer will be ignored.");
    }

    /* -------------------------------------------------------------------- */
    /*    If the dataset has an extension, it is understood that the path   */
    /*       of the file is where to write, and the layer name is the       */
    /*       dataset name (without extension).                              */
    /* -------------------------------------------------------------------- */
    const char *pszExtension = CPLGetExtension(pszRootName);
    char *pszFullMMLayerName;
    if (EQUAL(pszExtension, "pol") || EQUAL(pszExtension, "arc") ||
        EQUAL(pszExtension, "pnt"))
    {
        char *pszMMLayerName;
        pszMMLayerName = CPLStrdup(CPLResetExtension(pszRootName, ""));
        pszMMLayerName[strlen(pszMMLayerName) - 1] = '\0';

        pszFullMMLayerName = CPLStrdup((const char *)pszMMLayerName);

        // Checking that the folder where to write exists
        const char *szDestFolder = CPLGetDirname(pszFullMMLayerName);
        if (!STARTS_WITH(szDestFolder, "/vsimem"))
        {
            VSIStatBufL sStat;
            if (VSIStatL(szDestFolder, &sStat) != 0 ||
                !VSI_ISDIR(sStat.st_mode))
            {
                CPLFree(pszMMLayerName);
                CPLFree(pszFullMMLayerName);
                CPLError(CE_Failure, CPLE_AppDefined,
                         "The folder %s does not exist.", szDestFolder);
                return nullptr;
            }
        }
        CPLFree(pszMMLayerName);
    }
    else
    {
        const char *osPath;

        osPath = pszRootName;
        pszFullMMLayerName =
            CPLStrdup(CPLFormFilename(pszRootName, pszLayerName, ""));

        /* -------------------------------------------------------------------- */
        /*      Let's create the folder if it's not already created.            */
        /*      (only the las level of the folder)                              */
        /* -------------------------------------------------------------------- */
        if (!STARTS_WITH(osPath, "/vsimem"))
        {
            VSIStatBufL sStat;
            if (VSIStatL(osPath, &sStat) != 0 || !VSI_ISDIR(sStat.st_mode))
            {
                if (VSIMkdir(osPath, 0755) != 0)
                {
                    CPLFree(pszFullMMLayerName);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to create the folder %s.", pszRootName);
                    return nullptr;
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Return open layer handle.                                       */
    /* -------------------------------------------------------------------- */
    if (Open(pszFullMMLayerName, nullptr, poSRS, TRUE, papszOptions))
    {
        CPLFree(pszFullMMLayerName);
        auto poLayer = papoLayers[nLayers - 1];
        return poLayer;
    }

    CPLFree(pszFullMMLayerName);
    return nullptr;
}

/****************************************************************************/
/*                           TestCapability()                               */
/****************************************************************************/

int OGRMiraMonDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return bUpdate;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;

    return FALSE;
}

/****************************************************************************/
/*                              GetLayer()                                  */
/****************************************************************************/

OGRLayer *OGRMiraMonDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **OGRMiraMonDataSource::GetFileList()
{
    CPLStringList oFileList;
    GetLayerCount();
    for (int i = 0; i < nLayers; i++)
    {
        OGRMiraMonLayer *poLayer = papoLayers[i];
        poLayer->AddToFileList(oFileList);
    }
    return oFileList.StealList();
}
