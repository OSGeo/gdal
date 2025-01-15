/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements FileGDB OGR driver.
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *           Paul Ramsey, pramsey at cleverelephant.ca
 *
 ******************************************************************************
 * Copyright (c) 2010, Ragi Yaser Burhum
 * Copyright (c) 2011, Paul Ramsey <pramsey at cleverelephant.ca>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_fgdb.h"
#include "cpl_conv.h"
#include "FGdbUtils.h"
#include "cpl_multiproc.h"
#include "ogrmutexeddatasource.h"
#include "FGdbDriverCore.h"

extern "C" void RegisterOGRFileGDB();

static std::map<CPLString, FGdbDatabaseConnection *> *poMapConnections =
    nullptr;
CPLMutex *FGdbDriver::hMutex = nullptr;

/************************************************************************/
/*                     OGRFileGDBDriverUnload()                         */
/************************************************************************/

static void OGRFileGDBDriverUnload(GDALDriver *)
{
    if (poMapConnections && !poMapConnections->empty())
        CPLDebug("FileGDB", "Remaining %d connections. Bug?",
                 (int)poMapConnections->size());
    if (FGdbDriver::hMutex != nullptr)
        CPLDestroyMutex(FGdbDriver::hMutex);
    FGdbDriver::hMutex = nullptr;
    delete poMapConnections;
    poMapConnections = nullptr;
}

/************************************************************************/
/*                      OGRFileGDBDriverOpen()                          */
/************************************************************************/

static GDALDataset *OGRFileGDBDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess == GA_Update)
        return nullptr;

    const char *pszFilename = poOpenInfo->pszFilename;
    // @MAY_USE_OPENFILEGDB may be set to NO by the OpenFileGDB driver in its
    // Open() method when it detects that a dataset includes compressed tables
    // (.cdf), and thus calls the FileGDB driver to make it handle such
    // datasets. As the FileGDB driver would call, by default, OpenFileGDB for
    // assistance to get some information, OpenFileGDB needs to instruct it not
    // to do so to avoid a OpenFileGDB -> FileGDB -> OpenFileGDB cycle.
    const bool bUseOpenFileGDB = CPLTestBool(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "@MAY_USE_OPENFILEGDB", "YES"));

    if (bUseOpenFileGDB)
    {
        if (OGRFileGDBDriverIdentifyInternal(poOpenInfo, pszFilename) ==
            GDAL_IDENTIFY_FALSE)
            return nullptr;

        // If this is a raster-only GDB, do not try to open it, to be consistent
        // with OpenFileGDB behavior.
        const char *const apszOpenFileGDBDriver[] = {"OpenFileGDB", nullptr};
        auto poOpenFileGDBDS = std::unique_ptr<GDALDataset>(
            GDALDataset::Open(pszFilename, GDAL_OF_RASTER,
                              apszOpenFileGDBDriver, nullptr, nullptr));
        if (poOpenFileGDBDS)
        {
            poOpenFileGDBDS.reset();
            poOpenFileGDBDS = std::unique_ptr<GDALDataset>(
                GDALDataset::Open(pszFilename, GDAL_OF_VECTOR,
                                  apszOpenFileGDBDriver, nullptr, nullptr));
            if (!poOpenFileGDBDS)
                return nullptr;
        }
    }

    long hr;

    CPLMutexHolderD(&FGdbDriver::hMutex);
    if (poMapConnections == nullptr)
        poMapConnections = new std::map<CPLString, FGdbDatabaseConnection *>();

    FGdbDatabaseConnection *pConnection = (*poMapConnections)[pszFilename];
    if (pConnection != nullptr)
    {
        pConnection->m_nRefCount++;
        CPLDebug("FileGDB", "ref_count of %s = %d now", pszFilename,
                 pConnection->m_nRefCount);
    }
    else
    {
        Geodatabase *pGeoDatabase = new Geodatabase;
        hr = ::OpenGeodatabase(StringToWString(pszFilename), *pGeoDatabase);

        if (FAILED(hr))
        {
            delete pGeoDatabase;

            if (OGRGetDriverByName("OpenFileGDB") != nullptr)
            {
                std::wstring fgdb_error_desc_w;
                std::string fgdb_error_desc("Unknown error");
                fgdbError er;
                er = FileGDBAPI::ErrorInfo::GetErrorDescription(
                    static_cast<fgdbError>(hr), fgdb_error_desc_w);
                if (er == S_OK)
                {
                    fgdb_error_desc = WStringToString(fgdb_error_desc_w);
                }
                CPLDebug("FileGDB",
                         "Cannot open %s with FileGDB driver: %s. Failing "
                         "silently so OpenFileGDB can be tried",
                         pszFilename, fgdb_error_desc.c_str());
            }
            else
            {
                GDBErr(hr, "Failed to open Geodatabase");
            }
            poMapConnections->erase(pszFilename);
            return nullptr;
        }

        CPLDebug("FileGDB", "Really opening %s", pszFilename);
        pConnection = new FGdbDatabaseConnection(pszFilename, pGeoDatabase);
        (*poMapConnections)[pszFilename] = pConnection;
    }

    FGdbDataSource *pDS;

    pDS = new FGdbDataSource(true, pConnection, bUseOpenFileGDB);

    if (!pDS->Open(pszFilename, /*bUpdate = */ FALSE, nullptr))
    {
        delete pDS;
        return nullptr;
    }
    else
    {
        auto poMutexedDS =
            new OGRMutexedDataSource(pDS, TRUE, FGdbDriver::hMutex, TRUE);
        return poMutexedDS;
    }
}

/***********************************************************************/
/*                    OGRFileGDBDriverCreate()                         */
/***********************************************************************/

static GDALDataset *
OGRFileGDBDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                       CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                       CPL_UNUSED GDALDataType eDT, char **papszOptions)
{
    auto poOpenFileGDBDriver =
        GetGDALDriverManager()->GetDriverByName("OpenFileGDB");
    if (poOpenFileGDBDriver)
    {
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "FileGDB driver has no longer any direct creation capabilities. "
            "Forwarding to OpenFileGDB driver");
        return poOpenFileGDBDriver->Create(pszName, 0, 0, 0, GDT_Unknown,
                                           papszOptions);
    }
    return nullptr;
}

/***********************************************************************/
/*                            Release()                                */
/***********************************************************************/

void FGdbDriver::Release(const char *pszName)
{
    CPLMutexHolderOptionalLockD(FGdbDriver::hMutex);

    if (poMapConnections == nullptr)
        poMapConnections = new std::map<CPLString, FGdbDatabaseConnection *>();

    FGdbDatabaseConnection *pConnection = (*poMapConnections)[pszName];
    if (pConnection != nullptr)
    {
        pConnection->m_nRefCount--;
        CPLDebug("FileGDB", "ref_count of %s = %d now", pszName,
                 pConnection->m_nRefCount);
        if (pConnection->m_nRefCount == 0)
        {
            pConnection->CloseGeodatabase();
            delete pConnection;
            poMapConnections->erase(pszName);
        }
    }
}

/***********************************************************************/
/*                         CloseGeodatabase()                          */
/***********************************************************************/

void FGdbDatabaseConnection::CloseGeodatabase()
{
    if (m_pGeodatabase != nullptr)
    {
        CPLDebug("FileGDB", "Really closing %s now", m_osName.c_str());
        ::CloseGeodatabase(*m_pGeodatabase);
        delete m_pGeodatabase;
        m_pGeodatabase = nullptr;
    }
}

/***********************************************************************/
/*                         OpenGeodatabase()                           */
/***********************************************************************/

int FGdbDatabaseConnection::OpenGeodatabase(const char *pszFSName)
{
    m_pGeodatabase = new Geodatabase;
    long hr = ::OpenGeodatabase(StringToWString(CPLString(pszFSName)),
                                *m_pGeodatabase);
    if (FAILED(hr))
    {
        delete m_pGeodatabase;
        m_pGeodatabase = nullptr;
        return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                     OGRFileGDBDeleteDataSource()                     */
/************************************************************************/

static CPLErr OGRFileGDBDeleteDataSource(const char *pszDataSource)
{
    CPLMutexHolderD(&FGdbDriver::hMutex);

    std::wstring wstr = StringToWString(pszDataSource);

    long hr = 0;

    if (S_OK != (hr = ::DeleteGeodatabase(wstr)))
    {
        GDBErr(hr, "Failed to delete Geodatabase");
        return CE_Failure;
    }

    return CE_None;
}

/***********************************************************************/
/*                       RegisterOGRFileGDB()                          */
/***********************************************************************/

void RegisterOGRFileGDB()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    if (!GDAL_CHECK_VERSION("OGR FGDB"))
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRFileGDBDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRFileGDBDriverOpen;
    poDriver->pfnCreate = OGRFileGDBDriverCreate;
    poDriver->pfnDelete = OGRFileGDBDeleteDataSource;
    poDriver->pfnUnloadDriver = OGRFileGDBDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
