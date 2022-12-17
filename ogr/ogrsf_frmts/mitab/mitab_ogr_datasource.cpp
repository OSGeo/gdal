/**********************************************************************
 *
 * Name:     mitab_ogr_datasource.cpp
 * Project:  MapInfo Mid/Mif, Tab ogr support
 * Language: C++
 * Purpose:  Implementation of OGRTABDataSource.
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *           and Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Stephane Villeneuve
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "mitab_ogr_driver.h"

/*=======================================================================
 *                 OGRTABDataSource
 *
 * We need one single OGRDataSource/Driver set of classes to handle all
 * the MapInfo file types.  They all deal with the IMapInfoFile abstract
 * class.
 *=====================================================================*/

/************************************************************************/
/*                         OGRTABDataSource()                           */
/************************************************************************/

OGRTABDataSource::OGRTABDataSource()
    : m_pszName(nullptr), m_pszDirectory(nullptr), m_nLayerCount(0),
      m_papoLayers(nullptr), m_papszOptions(nullptr), m_bCreateMIF(FALSE),
      m_bSingleFile(FALSE), m_bSingleLayerAlreadyCreated(FALSE),
      m_bQuickSpatialIndexMode(-1), m_nBlockSize(512)
{
}

/************************************************************************/
/*                         ~OGRTABDataSource()                          */
/************************************************************************/

OGRTABDataSource::~OGRTABDataSource()

{
    CPLFree(m_pszName);
    CPLFree(m_pszDirectory);

    for (int i = 0; i < m_nLayerCount; i++)
        delete m_papoLayers[i];

    CPLFree(m_papoLayers);
    CSLDestroy(m_papszOptions);
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new dataset (directory or file).                       */
/************************************************************************/

int OGRTABDataSource::Create(const char *pszName, char **papszOptions)

{
    CPLAssert(m_pszName == nullptr);

    m_pszName = CPLStrdup(pszName);
    m_papszOptions = CSLDuplicate(papszOptions);
    eAccess = GA_Update;

    const char *pszOpt = CSLFetchNameValue(papszOptions, "FORMAT");
    if (pszOpt != nullptr && EQUAL(pszOpt, "MIF"))
        m_bCreateMIF = TRUE;
    else if (EQUAL(CPLGetExtension(pszName), "mif") ||
             EQUAL(CPLGetExtension(pszName), "mid"))
        m_bCreateMIF = TRUE;

    if ((pszOpt = CSLFetchNameValue(papszOptions, "SPATIAL_INDEX_MODE")) !=
        nullptr)
    {
        if (EQUAL(pszOpt, "QUICK"))
            m_bQuickSpatialIndexMode = TRUE;
        else if (EQUAL(pszOpt, "OPTIMIZED"))
            m_bQuickSpatialIndexMode = FALSE;
    }

    m_nBlockSize = atoi(CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", "512"));

    // Create a new empty directory.
    VSIStatBufL sStat;

    if (strlen(CPLGetExtension(pszName)) == 0)
    {
        if (VSIStatL(pszName, &sStat) == 0)
        {
            if (!VSI_ISDIR(sStat.st_mode))
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Attempt to create dataset named %s,\n"
                         "but that is an existing file.",
                         pszName);
                return FALSE;
            }
        }
        else
        {
            if (VSIMkdir(pszName, 0755) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to create directory %s.", pszName);
                return FALSE;
            }
        }

        m_pszDirectory = CPLStrdup(pszName);
    }

    // Create a new single file.
    else
    {
        IMapInfoFile *poFile = nullptr;
        const char *pszEncoding(CSLFetchNameValue(papszOptions, "ENCODING"));
        const char *pszCharset(IMapInfoFile::EncodingToCharset(pszEncoding));

        if (m_bCreateMIF)
        {
            poFile = new MIFFile;
            if (poFile->Open(m_pszName, TABWrite, FALSE, pszCharset) != 0)
            {
                delete poFile;
                return FALSE;
            }
        }
        else
        {
            TABFile *poTabFile = new TABFile;
            if (poTabFile->Open(m_pszName, TABWrite, FALSE, m_nBlockSize,
                                pszCharset) != 0)
            {
                delete poTabFile;
                return FALSE;
            }
            poFile = poTabFile;
        }

        m_nLayerCount = 1;
        m_papoLayers = static_cast<IMapInfoFile **>(CPLMalloc(sizeof(void *)));
        m_papoLayers[0] = poFile;

        m_pszDirectory = CPLStrdup(CPLGetPath(pszName));
        m_bSingleFile = TRUE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open an existing file, or directory of files.                   */
/************************************************************************/

int OGRTABDataSource::Open(GDALOpenInfo *poOpenInfo, int bTestOpen)

{
    CPLAssert(m_pszName == nullptr);

    m_pszName = CPLStrdup(poOpenInfo->pszFilename);
    eAccess = poOpenInfo->eAccess;

    // If it is a file, try to open as a Mapinfo file.
    if (!poOpenInfo->bIsDirectory)
    {
        IMapInfoFile *poFile =
            IMapInfoFile::SmartOpen(m_pszName, GetUpdate(), bTestOpen);
        if (poFile == nullptr)
            return FALSE;

        poFile->SetDescription(poFile->GetName());

        m_nLayerCount = 1;
        m_papoLayers = static_cast<IMapInfoFile **>(CPLMalloc(sizeof(void *)));
        m_papoLayers[0] = poFile;

        m_pszDirectory = CPLStrdup(CPLGetPath(m_pszName));

        m_bSingleFile = TRUE;
        m_bSingleLayerAlreadyCreated = TRUE;
    }

    // Otherwise, we need to scan the whole directory for files
    // ending in .tab or .mif.
    else
    {
        char **papszFileList = VSIReadDir(m_pszName);

        m_pszDirectory = CPLStrdup(m_pszName);

        for (int iFile = 0;
             papszFileList != nullptr && papszFileList[iFile] != nullptr;
             iFile++)
        {
            const char *pszExtension = CPLGetExtension(papszFileList[iFile]);

            if (!EQUAL(pszExtension, "tab") && !EQUAL(pszExtension, "mif"))
                continue;

            char *pszSubFilename = CPLStrdup(
                CPLFormFilename(m_pszDirectory, papszFileList[iFile], nullptr));

            IMapInfoFile *poFile =
                IMapInfoFile::SmartOpen(pszSubFilename, GetUpdate(), bTestOpen);
            CPLFree(pszSubFilename);

            if (poFile == nullptr)
            {
                CSLDestroy(papszFileList);
                return FALSE;
            }
            poFile->SetDescription(poFile->GetName());

            m_nLayerCount++;
            m_papoLayers = static_cast<IMapInfoFile **>(
                CPLRealloc(m_papoLayers, sizeof(void *) * m_nLayerCount));
            m_papoLayers[m_nLayerCount - 1] = poFile;
        }

        CSLDestroy(papszFileList);

        if (m_nLayerCount == 0)
        {
            if (!bTestOpen)
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "No mapinfo files found in directory %s.",
                         m_pszDirectory);

            return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRTABDataSource::GetLayerCount()

{
    if (m_bSingleFile && !m_bSingleLayerAlreadyCreated)
        return 0;
    else
        return m_nLayerCount;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRTABDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= GetLayerCount())
        return nullptr;
    else
        return m_papoLayers[iLayer];
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRTABDataSource::ICreateLayer(const char *pszLayerName,
                                         OGRSpatialReference *poSRSIn,
                                         OGRwkbGeometryType /* eGeomTypeIn */,
                                         char **papszOptions)

{
    if (!GetUpdate())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create layer on read-only dataset.");
        return nullptr;
    }

    // If it is a single file mode file, then we may have already
    // instantiated the low level layer.   We would just need to
    // reset the coordinate system and (potentially) bounds.
    IMapInfoFile *poFile = nullptr;
    char *pszFullFilename = nullptr;

    const char *pszEncoding = CSLFetchNameValue(papszOptions, "ENCODING");
    const char *pszCharset(IMapInfoFile::EncodingToCharset(pszEncoding));
    const char *pszDescription(CSLFetchNameValue(papszOptions, "DESCRIPTION"));

    if (m_bSingleFile)
    {
        if (m_bSingleLayerAlreadyCreated)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Unable to create new layers in this single file dataset.");
            return nullptr;
        }

        m_bSingleLayerAlreadyCreated = TRUE;

        poFile = m_papoLayers[0];
        if (pszEncoding)
            poFile->SetCharset(pszCharset);

        if (poFile->GetFileClass() == TABFC_TABFile)
            poFile->SetMetadataItem("DESCRIPTION", pszDescription);
    }

    else
    {
        if (m_bCreateMIF)
        {
            pszFullFilename =
                CPLStrdup(CPLFormFilename(m_pszDirectory, pszLayerName, "mif"));

            poFile = new MIFFile;

            if (poFile->Open(pszFullFilename, TABWrite, FALSE, pszCharset) != 0)
            {
                CPLFree(pszFullFilename);
                delete poFile;
                return nullptr;
            }
        }
        else
        {
            pszFullFilename =
                CPLStrdup(CPLFormFilename(m_pszDirectory, pszLayerName, "tab"));

            TABFile *poTABFile = new TABFile;

            if (poTABFile->Open(pszFullFilename, TABWrite, FALSE, m_nBlockSize,
                                pszCharset) != 0)
            {
                CPLFree(pszFullFilename);
                delete poTABFile;
                return nullptr;
            }
            poFile = poTABFile;
            poFile->SetMetadataItem("DESCRIPTION", pszDescription);
        }

        m_nLayerCount++;
        m_papoLayers = static_cast<IMapInfoFile **>(
            CPLRealloc(m_papoLayers, sizeof(void *) * m_nLayerCount));
        m_papoLayers[m_nLayerCount - 1] = poFile;

        CPLFree(pszFullFilename);
    }

    poFile->SetDescription(poFile->GetName());

    // Assign the coordinate system (if provided) and set
    // reasonable bounds.
    if (poSRSIn != nullptr)
    {
        auto poSRSClone = poSRSIn->Clone();
        poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poFile->SetSpatialRef(poSRSClone);
        poSRSClone->Release();
        // SetSpatialRef() has cloned the passed geometry
        poFile->GetLayerDefn()->GetGeomFieldDefn(0)->SetSpatialRef(
            poFile->GetSpatialRef());
    }

    // Pull out the bounds if supplied
    const char *pszOpt = nullptr;
    if ((pszOpt = CSLFetchNameValue(papszOptions, "BOUNDS")) != nullptr)
    {
        double dfBounds[4];
        if (CPLsscanf(pszOpt, "%lf,%lf,%lf,%lf", &dfBounds[0], &dfBounds[1],
                      &dfBounds[2], &dfBounds[3]) != 4)
        {
            CPLError(
                CE_Failure, CPLE_IllegalArg,
                "Invalid BOUNDS parameter, expected min_x,min_y,max_x,max_y");
        }
        else
        {
            poFile->SetBounds(dfBounds[0], dfBounds[1], dfBounds[2],
                              dfBounds[3]);
        }
    }

    if (!poFile->IsBoundsSet() && !m_bCreateMIF)
    {
        if (poSRSIn != nullptr && poSRSIn->IsGeographic())
            poFile->SetBounds(-1000, -1000, 1000, 1000);
        else if (poSRSIn != nullptr && poSRSIn->IsProjected())
        {
            const double FE = poSRSIn->GetProjParm(SRS_PP_FALSE_EASTING, 0.0);
            const double FN = poSRSIn->GetProjParm(SRS_PP_FALSE_NORTHING, 0.0);
            poFile->SetBounds(-30000000 + FE, -15000000 + FN, 30000000 + FE,
                              15000000 + FN);
        }
        else
            poFile->SetBounds(-30000000, -15000000, 30000000, 15000000);
    }

    if (m_bQuickSpatialIndexMode == TRUE &&
        poFile->SetQuickSpatialIndexMode(TRUE) != 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Setting Quick Spatial Index Mode failed.");
    }
    else if (m_bQuickSpatialIndexMode == FALSE &&
             poFile->SetQuickSpatialIndexMode(FALSE) != 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Setting Normal Spatial Index Mode failed.");
    }

    return poFile;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRTABDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return GetUpdate() && (!m_bSingleFile || !m_bSingleLayerAlreadyCreated);
    else if (EQUAL(pszCap, ODsCRandomLayerWrite))
        return GetUpdate();
    else
        return FALSE;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **OGRTABDataSource::GetFileList()
{
    VSIStatBufL sStatBuf;
    CPLStringList osList;

    if (VSIStatL(m_pszName, &sStatBuf) == 0 && VSI_ISDIR(sStatBuf.st_mode))
    {
        static const char *const apszExtensions[] = {
            "mif", "mid", "tab", "map", "ind", "dat", "id", nullptr};
        char **papszDirEntries = VSIReadDir(m_pszName);

        for (int iFile = 0;
             papszDirEntries != nullptr && papszDirEntries[iFile] != nullptr;
             iFile++)
        {
            if (CSLFindString(apszExtensions,
                              CPLGetExtension(papszDirEntries[iFile])) != -1)
            {
                osList.AddString(CPLFormFilename(
                    m_pszName, papszDirEntries[iFile], nullptr));
            }
        }

        CSLDestroy(papszDirEntries);
    }
    else
    {
        static const char *const apszMIFExtensions[] = {"mif", "mid", nullptr};
        static const char *const apszTABExtensions[] = {"tab", "map", "ind",
                                                        "dat", "id",  nullptr};
        const char *const *papszExtensions = nullptr;
        if (EQUAL(CPLGetExtension(m_pszName), "mif") ||
            EQUAL(CPLGetExtension(m_pszName), "mid"))
        {
            papszExtensions = apszMIFExtensions;
        }
        else
        {
            papszExtensions = apszTABExtensions;
        }
        const char *const *papszIter = papszExtensions;
        while (*papszIter)
        {
            const char *pszFile = CPLResetExtension(m_pszName, *papszIter);
            if (VSIStatL(pszFile, &sStatBuf) != 0)
            {
                pszFile = CPLResetExtension(m_pszName,
                                            CPLString(*papszIter).toupper());
                if (VSIStatL(pszFile, &sStatBuf) != 0)
                {
                    pszFile = nullptr;
                }
            }
            if (pszFile)
                osList.AddString(pszFile);
            papszIter++;
        }
    }
    return osList.StealList();
}

/************************************************************************/
/*                            ExecuteSQL()                              */
/************************************************************************/

OGRLayer *OGRTABDataSource::ExecuteSQL(const char *pszStatement,
                                       OGRGeometry *poSpatialFilter,
                                       const char *pszDialect)
{
    char **papszTokens = CSLTokenizeString(pszStatement);
    if (CSLCount(papszTokens) == 6 && EQUAL(papszTokens[0], "CREATE") &&
        EQUAL(papszTokens[1], "INDEX") && EQUAL(papszTokens[2], "ON") &&
        EQUAL(papszTokens[4], "USING"))
    {
        IMapInfoFile *poLayer =
            dynamic_cast<IMapInfoFile *>(GetLayerByName(papszTokens[3]));
        if (poLayer == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "`%s' failed failed, no such layer as `%s'.", pszStatement,
                     papszTokens[3]);
            CSLDestroy(papszTokens);
            return nullptr;
        }
        int nFieldIdx = poLayer->GetLayerDefn()->GetFieldIndex(papszTokens[5]);
        CSLDestroy(papszTokens);
        if (nFieldIdx < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "`%s' failed, field not found.", pszStatement);
            return nullptr;
        }
        poLayer->SetFieldIndexed(nFieldIdx);
        return nullptr;
    }

    CSLDestroy(papszTokens);
    return GDALDataset::ExecuteSQL(pszStatement, poSpatialFilter, pszDialect);
}
