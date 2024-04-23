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
{
    memset(&m_MMMap, 0, sizeof(m_MMMap));
}

/****************************************************************************/
/*                         ~OGRMiraMonDataSource()                          */
/****************************************************************************/

OGRMiraMonDataSource::~OGRMiraMonDataSource()

{
    m_apoLayers.clear();

    if (m_MMMap.fMMMap)
        VSIFCloseL(m_MMMap.fMMMap);
}

/****************************************************************************/
/*                                Open()                                    */
/****************************************************************************/

bool OGRMiraMonDataSource::Open(const char *pszFilename, VSILFILE *fp,
                                const OGRSpatialReference *poSRS,
                                CSLConstList papszOpenOptionsUsr)

{
    auto poLayer = std::make_unique<OGRMiraMonLayer>(
        this, pszFilename, fp, poSRS, m_bUpdate, papszOpenOptionsUsr, &m_MMMap);
    if (!poLayer->bValidFile)
    {
        return false;
    }

    if (!m_osRootName.empty())
    {
        const char *pszExtension = CPLGetExtension(m_osRootName.c_str());
        if (!EQUAL(pszExtension, "pol") && !EQUAL(pszExtension, "arc") &&
            !EQUAL(pszExtension, "pnt"))
        {
            CPLStrlcpy(m_MMMap.pszMapName,
                       CPLFormFilename(m_osRootName.c_str(),
                                       CPLGetBasename(m_osRootName.c_str()),
                                       "mmm"),
                       sizeof(m_MMMap.pszMapName));
            if (!m_MMMap.nNumberOfLayers)
            {
                m_MMMap.fMMMap = VSIFOpenL(m_MMMap.pszMapName, "w+");
                if (!m_MMMap.fMMMap)
                {
                    // It could be an error but it is not so important
                    // to stop the process. This map is an extra element
                    // to open all layers in one click, at least in MiraMon
                    // software.
                    *m_MMMap.pszMapName = '\0';
                }
                else
                {
                    VSIFPrintfL(m_MMMap.fMMMap, "[VERSIO]\n");
                    VSIFPrintfL(m_MMMap.fMMMap, "Vers=2\n");
                    VSIFPrintfL(m_MMMap.fMMMap, "SubVers=0\n");
                    VSIFPrintfL(m_MMMap.fMMMap, "variant=b\n");
                    VSIFPrintfL(m_MMMap.fMMMap, "\n");
                    VSIFPrintfL(m_MMMap.fMMMap, "[DOCUMENT]\n");
                    VSIFPrintfL(m_MMMap.fMMMap, "Titol= %s(map)\n",
                                CPLGetBasename(poLayer->GetName()));
                    VSIFPrintfL(m_MMMap.fMMMap, "\n");
                }
            }
        }
        else
            *m_MMMap.pszMapName = '\0';
    }
    else
        *m_MMMap.pszMapName = '\0';

    m_apoLayers.emplace_back(std::move(poLayer));

    return true;
}

/****************************************************************************/
/*                               Create()                                   */
/*                                                                          */
/*      Create a new datasource.  This does not really do anything          */
/*      currently but save the name.                                        */
/****************************************************************************/

bool OGRMiraMonDataSource::Create(const char *pszDataSetName,
                                  CSLConstList /* papszOptions */)

{
    m_bUpdate = true;
    m_osRootName = pszDataSetName;

    return true;
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
    const char *pszExtension = CPLGetExtension(m_osRootName.c_str());
    std::string osFullMMLayerName;
    if (EQUAL(pszExtension, "pol") || EQUAL(pszExtension, "arc") ||
        EQUAL(pszExtension, "pnt"))
    {
        osFullMMLayerName = CPLResetExtension(m_osRootName.c_str(), "");
        if (!osFullMMLayerName.empty())
            osFullMMLayerName.pop_back();

        // Checking that the folder where to write exists
        const std::string osDestFolder =
            CPLGetDirname(osFullMMLayerName.c_str());
        if (!STARTS_WITH(osDestFolder.c_str(), "/vsimem"))
        {
            VSIStatBufL sStat;
            if (VSIStatL(osDestFolder.c_str(), &sStat) != 0 ||
                !VSI_ISDIR(sStat.st_mode))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "The folder %s does not exist.", osDestFolder.c_str());
                return nullptr;
            }
        }
    }
    else
    {
        osFullMMLayerName =
            CPLFormFilename(m_osRootName.c_str(), pszLayerName, "");

        /* -------------------------------------------------------------------- */
        /*      Let's create the folder if it's not already created.            */
        /*      (only the las level of the folder)                              */
        /* -------------------------------------------------------------------- */
        if (!STARTS_WITH(m_osRootName.c_str(), "/vsimem"))
        {
            VSIStatBufL sStat;
            if (VSIStatL(m_osRootName.c_str(), &sStat) != 0 ||
                !VSI_ISDIR(sStat.st_mode))
            {
                if (VSIMkdir(m_osRootName.c_str(), 0755) != 0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to create the folder %s.",
                             m_osRootName.c_str());
                    return nullptr;
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Return open layer handle.                                       */
    /* -------------------------------------------------------------------- */
    if (Open(osFullMMLayerName.c_str(), nullptr, poSRS, papszOptions))
    {
        return m_apoLayers.back().get();
    }

    return nullptr;
}

/****************************************************************************/
/*                           TestCapability()                               */
/****************************************************************************/

int OGRMiraMonDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return m_bUpdate;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;

    return FALSE;
}

/****************************************************************************/
/*                              GetLayer()                                  */
/****************************************************************************/

OGRLayer *OGRMiraMonDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= static_cast<int>(m_apoLayers.size()))
        return nullptr;

    return m_apoLayers[iLayer].get();
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **OGRMiraMonDataSource::GetFileList()
{
    CPLStringList oFileList;
    for (auto &poLayer : m_apoLayers)
    {
        poLayer->AddToFileList(oFileList);
    }
    return oFileList.StealList();
}
