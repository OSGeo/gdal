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
FGdbTransactionManager *FGdbDriver::m_poTransactionManager = nullptr;

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
    delete FGdbDriver::m_poTransactionManager;
    FGdbDriver::m_poTransactionManager = nullptr;
    delete poMapConnections;
    poMapConnections = nullptr;
}

/************************************************************************/
/*                      OGRFileGDBDriverOpen()                          */
/************************************************************************/

static GDALDataset *OGRFileGDBDriverOpen(GDALOpenInfo *poOpenInfo)
{
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

    const bool bUpdate = poOpenInfo->eAccess == GA_Update;
    long hr;

    CPLMutexHolderD(&FGdbDriver::hMutex);
    if (poMapConnections == nullptr)
        poMapConnections = new std::map<CPLString, FGdbDatabaseConnection *>();

    FGdbDatabaseConnection *pConnection = (*poMapConnections)[pszFilename];
    if (pConnection != nullptr)
    {
        if (pConnection->IsFIDHackInProgress())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot open geodatabase at the moment since it is in "
                     "'FID hack mode'");
            return nullptr;
        }

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

            if (OGRGetDriverByName("OpenFileGDB") != nullptr &&
                bUpdate == FALSE)
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

    if (!pDS->Open(pszFilename, bUpdate, nullptr))
    {
        delete pDS;
        return nullptr;
    }
    else
    {
        OGRMutexedDataSource *poMutexedDS =
            new OGRMutexedDataSource(pDS, TRUE, FGdbDriver::hMutex, TRUE);
        if (bUpdate)
            return OGRCreateEmulatedTransactionDataSourceWrapper(
                poMutexedDS, FGdbDriver::GetTransactionManager(), TRUE, FALSE);
        else
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
    long hr;
    Geodatabase *pGeodatabase;
    std::wstring wconn = StringToWString(pszName);
    int bUpdate = TRUE;  // If we're creating, we must be writing.
    VSIStatBuf stat;

    CPLMutexHolderD(&FGdbDriver::hMutex);

    /* We don't support options yet, so warn if they send us some */
    if (papszOptions)
    {
        /* TODO: warning, ignoring options */
    }

    /* Only accept names of form "filename.gdb" and */
    /* also .gdb.zip to be able to return FGDB with MapServer OGR output (#4199)
     */
    const char *pszExt = CPLGetExtension(pszName);
    if (!(EQUAL(pszExt, "gdb") || EQUAL(pszExt, "zip")))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FGDB data source name must use 'gdb' extension.\n");
        return nullptr;
    }

    /* Don't try to create on top of something already there */
    if (CPLStat(pszName, &stat) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s already exists.\n", pszName);
        return nullptr;
    }

    /* Try to create the geodatabase */
    pGeodatabase =
        new Geodatabase;  // Create on heap so we can store it in the Datasource
    hr = CreateGeodatabase(wconn, *pGeodatabase);

    /* Handle creation errors */
    if (S_OK != hr)
    {
        const char *errstr = "Error creating geodatabase (%s).\n";
        if (hr == -2147220653)
            errstr = "File already exists (%s).\n";
        delete pGeodatabase;
        CPLError(CE_Failure, CPLE_AppDefined, errstr, pszName);
        return nullptr;
    }

    FGdbDatabaseConnection *pConnection =
        new FGdbDatabaseConnection(pszName, pGeodatabase);

    if (poMapConnections == nullptr)
        poMapConnections = new std::map<CPLString, FGdbDatabaseConnection *>();
    (*poMapConnections)[pszName] = pConnection;

    /* Ready to embed the Geodatabase in an OGR Datasource */
    FGdbDataSource *pDS = new FGdbDataSource(true, pConnection, true);
    if (!pDS->Open(pszName, bUpdate, nullptr))
    {
        delete pDS;
        return nullptr;
    }
    else
        return OGRCreateEmulatedTransactionDataSourceWrapper(
            new OGRMutexedDataSource(pDS, TRUE, FGdbDriver::hMutex, TRUE),
            FGdbDriver::GetTransactionManager(), TRUE, FALSE);
}

/************************************************************************/
/*                           StartTransaction()                         */
/************************************************************************/

OGRErr FGdbTransactionManager::StartTransaction(OGRDataSource *&poDSInOut,
                                                int &bOutHasReopenedDS)
{
    CPLMutexHolderOptionalLockD(FGdbDriver::hMutex);

    bOutHasReopenedDS = FALSE;

    OGRMutexedDataSource *poMutexedDS = (OGRMutexedDataSource *)poDSInOut;
    FGdbDataSource *poDS = (FGdbDataSource *)poMutexedDS->GetBaseDataSource();
    if (!poDS->GetUpdate())
        return OGRERR_FAILURE;
    FGdbDatabaseConnection *pConnection = poDS->GetConnection();
    if (pConnection->GetRefCount() != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot start transaction as database is opened in another "
                 "connection");
        return OGRERR_FAILURE;
    }
    if (pConnection->IsLocked())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Transaction is already in progress");
        return OGRERR_FAILURE;
    }

    bOutHasReopenedDS = TRUE;

    CPLString osName(poMutexedDS->GetName());
    CPLString osNameOri(osName);
    if (osName.back() == '/' || osName.back() == '\\')
        osName.resize(osName.size() - 1);

#ifndef _WIN32
    int bPerLayerCopyingForTransaction =
        poDS->HasPerLayerCopyingForTransaction();
#endif

    pConnection->m_nRefCount++;
    delete poDSInOut;
    poDSInOut = nullptr;
    poMutexedDS = nullptr;
    poDS = nullptr;

    pConnection->CloseGeodatabase();

    const CPLString osEditedName(CPLString(osName).append(".ogredited"));

    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPL_IGNORE_RET_VAL(CPLUnlinkTree(osEditedName));
    CPLPopErrorHandler();

    OGRErr eErr = OGRERR_NONE;

    CPLString osDatabaseToReopen;
#ifndef _WIN32
    if (bPerLayerCopyingForTransaction)
    {
        int bError = FALSE;

        if (VSIMkdir(osEditedName, 0755) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create directory '%s'.", osEditedName.c_str());
            bError = TRUE;
        }

        // Only copy a0000000X.Y files with X >= 1 && X <= 8, gdb and timestamps
        // and symlink others
        char **papszFiles = VSIReadDir(osName);
        for (char **papszIter = papszFiles; !bError && *papszIter; ++papszIter)
        {
            if (strcmp(*papszIter, ".") == 0 || strcmp(*papszIter, "..") == 0)
                continue;
            if (((*papszIter)[0] == 'a' && atoi((*papszIter) + 1) >= 1 &&
                 atoi((*papszIter) + 1) <= 8) ||
                EQUAL(*papszIter, "gdb") || EQUAL(*papszIter, "timestamps"))
            {
                if (CPLCopyFile(
                        CPLFormFilename(osEditedName, *papszIter, nullptr),
                        CPLFormFilename(osName, *papszIter, nullptr)) != 0)
                {
                    bError = TRUE;
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot copy %s",
                             *papszIter);
                }
            }
            else
            {
                CPLString osSourceFile;
                if (CPLIsFilenameRelative(osName))
                    osSourceFile = CPLFormFilename(
                        CPLSPrintf("../%s", CPLGetFilename(osName.c_str())),
                        *papszIter, nullptr);
                else
                    osSourceFile = osName;
                if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE1") ||
                    CPLSymlink(osSourceFile,
                               CPLFormFilename(osEditedName.c_str(), *papszIter,
                                               nullptr),
                               nullptr) != 0)
                {
                    bError = TRUE;
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot symlink %s",
                             *papszIter);
                }
            }
        }
        CSLDestroy(papszFiles);

        if (bError)
        {
            eErr = OGRERR_FAILURE;
            osDatabaseToReopen = osName;
        }
        else
            osDatabaseToReopen = osEditedName;
    }
    else
#endif
    {
        if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE1") ||
            CPLCopyTree(osEditedName, osName) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot backup geodatabase");
            eErr = OGRERR_FAILURE;
            osDatabaseToReopen = osName;
        }
        else
            osDatabaseToReopen = osEditedName;
    }

    pConnection->m_pGeodatabase = new Geodatabase;
    long hr = ::OpenGeodatabase(StringToWString(osDatabaseToReopen),
                                *(pConnection->m_pGeodatabase));
    if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE2") || FAILED(hr))
    {
        delete pConnection->m_pGeodatabase;
        pConnection->m_pGeodatabase = nullptr;
        FGdbDriver::Release(osName);
        GDBErr(hr, CPLSPrintf("Failed to open %s. Dataset should be closed",
                              osDatabaseToReopen.c_str()));

        return OGRERR_FAILURE;
    }

    FGdbDataSource *pDS = new FGdbDataSource(true, pConnection, true);
    pDS->Open(osDatabaseToReopen, TRUE, osNameOri);

#ifndef _WIN32
    if (eErr == OGRERR_NONE && bPerLayerCopyingForTransaction)
    {
        pDS->SetPerLayerCopyingForTransaction(bPerLayerCopyingForTransaction);
        pDS->SetSymlinkFlagOnAllLayers();
    }
#endif

    poDSInOut = new OGRMutexedDataSource(pDS, TRUE, FGdbDriver::hMutex, TRUE);

    if (eErr == OGRERR_NONE)
        pConnection->SetLocked(TRUE);
    return eErr;
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

OGRErr FGdbTransactionManager::CommitTransaction(OGRDataSource *&poDSInOut,
                                                 int &bOutHasReopenedDS)
{
    CPLMutexHolderOptionalLockD(FGdbDriver::hMutex);

    bOutHasReopenedDS = FALSE;

    OGRMutexedDataSource *poMutexedDS = (OGRMutexedDataSource *)poDSInOut;
    FGdbDataSource *poDS = (FGdbDataSource *)poMutexedDS->GetBaseDataSource();
    FGdbDatabaseConnection *pConnection = poDS->GetConnection();
    if (!pConnection->IsLocked())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "No transaction in progress");
        return OGRERR_FAILURE;
    }

    bOutHasReopenedDS = TRUE;

    CPLString osName(poMutexedDS->GetName());
    CPLString osNameOri(osName);
    if (osName.back() == '/' || osName.back() == '\\')
        osName.resize(osName.size() - 1);

#ifndef _WIN32
    int bPerLayerCopyingForTransaction =
        poDS->HasPerLayerCopyingForTransaction();
#endif

    pConnection->m_nRefCount++;
    delete poDSInOut;
    poDSInOut = nullptr;
    poMutexedDS = nullptr;
    poDS = nullptr;

    pConnection->CloseGeodatabase();

    CPLString osEditedName(osName);
    osEditedName += ".ogredited";

#ifndef _WIN32
    if (bPerLayerCopyingForTransaction)
    {
        int bError = FALSE;
        char **papszFiles;
        std::vector<CPLString> aosTmpFilesToClean;

        // Check for files present in original copy that are not in edited copy
        // That is to say deleted layers
        papszFiles = VSIReadDir(osName);
        for (char **papszIter = papszFiles; !bError && *papszIter; ++papszIter)
        {
            if (strcmp(*papszIter, ".") == 0 || strcmp(*papszIter, "..") == 0)
                continue;
            VSIStatBufL sStat;
            if ((*papszIter)[0] == 'a' &&
                VSIStatL(CPLFormFilename(osEditedName, *papszIter, nullptr),
                         &sStat) != 0)
            {
                if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE1") ||
                    VSIRename(CPLFormFilename(osName, *papszIter, nullptr),
                              CPLFormFilename(osName, *papszIter, "tmp")) != 0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot rename %s to %s",
                             CPLFormFilename(osName, *papszIter, nullptr),
                             CPLFormFilename(osName, *papszIter, "tmp"));
                    bError = TRUE;
                }
                else
                    aosTmpFilesToClean.push_back(
                        CPLFormFilename(osName, *papszIter, "tmp"));
            }
        }
        CSLDestroy(papszFiles);

        // Move modified files from edited directory to main directory
        papszFiles = VSIReadDir(osEditedName);
        for (char **papszIter = papszFiles; !bError && *papszIter; ++papszIter)
        {
            if (strcmp(*papszIter, ".") == 0 || strcmp(*papszIter, "..") == 0)
                continue;
            struct stat sStat;
            if (lstat(CPLFormFilename(osEditedName, *papszIter, nullptr),
                      &sStat) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot stat %s",
                         CPLFormFilename(osEditedName, *papszIter, nullptr));
                bError = TRUE;
            }
            else if (!S_ISLNK(sStat.st_mode))
            {
                // If there was such a file in original directory, first rename
                // it as a temporary file
                if (lstat(CPLFormFilename(osName, *papszIter, nullptr),
                          &sStat) == 0)
                {
                    if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""),
                              "CASE2") ||
                        VSIRename(CPLFormFilename(osName, *papszIter, nullptr),
                                  CPLFormFilename(osName, *papszIter, "tmp")) !=
                            0)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot rename %s to %s",
                                 CPLFormFilename(osName, *papszIter, nullptr),
                                 CPLFormFilename(osName, *papszIter, "tmp"));
                        bError = TRUE;
                    }
                    else
                        aosTmpFilesToClean.push_back(
                            CPLFormFilename(osName, *papszIter, "tmp"));
                }
                if (!bError)
                {
                    if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""),
                              "CASE3") ||
                        CPLMoveFile(
                            CPLFormFilename(osName, *papszIter, nullptr),
                            CPLFormFilename(osEditedName, *papszIter,
                                            nullptr)) != 0)
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined, "Cannot move %s to %s",
                            CPLFormFilename(osEditedName, *papszIter, nullptr),
                            CPLFormFilename(osName, *papszIter, nullptr));
                        bError = TRUE;
                    }
                    else
                        CPLDebug(
                            "FileGDB", "Move %s to %s",
                            CPLFormFilename(osEditedName, *papszIter, nullptr),
                            CPLFormFilename(osName, *papszIter, nullptr));
                }
            }
        }
        CSLDestroy(papszFiles);

        if (!bError)
        {
            for (size_t i = 0; i < aosTmpFilesToClean.size(); i++)
            {
                if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE4") ||
                    VSIUnlink(aosTmpFilesToClean[i]) != 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot remove %s. Manual cleanup required",
                             aosTmpFilesToClean[i].c_str());
                }
            }
        }

        if (bError)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "An error occurred while moving files from %s back to %s. "
                "Manual cleaning must be done and dataset should be closed",
                osEditedName.c_str(), osName.c_str());
            pConnection->SetLocked(FALSE);
            FGdbDriver::Release(osName);
            return OGRERR_FAILURE;
        }
        else if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE5") ||
                 CPLUnlinkTree(osEditedName) != 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot remove %s. Manual cleanup required",
                     osEditedName.c_str());
        }
    }
    else
#endif
    {
        CPLString osTmpName(osName);
        osTmpName += ".ogrtmp";

        /* Install the backup copy as the main database in 3 steps : */
        /* first rename the main directory  in .tmp */
        /* then rename the edited copy under regular name */
        /* and finally dispose the .tmp directory */
        /* That way there's no risk definitely losing data */
        if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE1") ||
            VSIRename(osName, osTmpName) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot rename %s to %s. Edited database during "
                     "transaction is in %s"
                     "Dataset should be closed",
                     osName.c_str(), osTmpName.c_str(), osEditedName.c_str());
            pConnection->SetLocked(FALSE);
            FGdbDriver::Release(osName);
            return OGRERR_FAILURE;
        }

        if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE2") ||
            VSIRename(osEditedName, osName) != 0)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Cannot rename %s to %s. The original geodatabase is in '%s'. "
                "Dataset should be closed",
                osEditedName.c_str(), osName.c_str(), osTmpName.c_str());
            pConnection->SetLocked(FALSE);
            FGdbDriver::Release(osName);
            return OGRERR_FAILURE;
        }

        if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE3") ||
            CPLUnlinkTree(osTmpName) != 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot remove %s. Manual cleanup required",
                     osTmpName.c_str());
        }
    }

    pConnection->m_pGeodatabase = new Geodatabase;
    long hr = ::OpenGeodatabase(StringToWString(osName),
                                *(pConnection->m_pGeodatabase));
    if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE_REOPEN") ||
        FAILED(hr))
    {
        delete pConnection->m_pGeodatabase;
        pConnection->m_pGeodatabase = nullptr;
        pConnection->SetLocked(FALSE);
        FGdbDriver::Release(osName);
        GDBErr(hr, "Failed to re-open Geodatabase. Dataset should be closed");
        return OGRERR_FAILURE;
    }

    FGdbDataSource *pDS = new FGdbDataSource(true, pConnection, true);
    pDS->Open(osNameOri, TRUE, nullptr);
    // pDS->SetPerLayerCopyingForTransaction(bPerLayerCopyingForTransaction);
    poDSInOut = new OGRMutexedDataSource(pDS, TRUE, FGdbDriver::hMutex, TRUE);

    pConnection->SetLocked(FALSE);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           RollbackTransaction()                      */
/************************************************************************/

OGRErr FGdbTransactionManager::RollbackTransaction(OGRDataSource *&poDSInOut,
                                                   int &bOutHasReopenedDS)
{
    CPLMutexHolderOptionalLockD(FGdbDriver::hMutex);

    bOutHasReopenedDS = FALSE;

    OGRMutexedDataSource *poMutexedDS = (OGRMutexedDataSource *)poDSInOut;
    FGdbDataSource *poDS = (FGdbDataSource *)poMutexedDS->GetBaseDataSource();
    FGdbDatabaseConnection *pConnection = poDS->GetConnection();
    if (!pConnection->IsLocked())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "No transaction in progress");
        return OGRERR_FAILURE;
    }

    bOutHasReopenedDS = TRUE;

    CPLString osName(poMutexedDS->GetName());
    CPLString osNameOri(osName);
    if (osName.back() == '/' || osName.back() == '\\')
        osName.resize(osName.size() - 1);

    // int bPerLayerCopyingForTransaction =
    // poDS->HasPerLayerCopyingForTransaction();

    pConnection->m_nRefCount++;
    delete poDSInOut;
    poDSInOut = nullptr;
    poMutexedDS = nullptr;
    poDS = nullptr;

    pConnection->CloseGeodatabase();

    CPLString osEditedName(osName);
    osEditedName += ".ogredited";

    OGRErr eErr = OGRERR_NONE;
    if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE1") ||
        CPLUnlinkTree(osEditedName) != 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Cannot remove %s. Manual cleanup required",
                 osEditedName.c_str());
        eErr = OGRERR_FAILURE;
    }

    pConnection->m_pGeodatabase = new Geodatabase;
    long hr = ::OpenGeodatabase(StringToWString(osName),
                                *(pConnection->m_pGeodatabase));
    if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE2") || FAILED(hr))
    {
        delete pConnection->m_pGeodatabase;
        pConnection->m_pGeodatabase = nullptr;
        pConnection->SetLocked(FALSE);
        FGdbDriver::Release(osName);
        GDBErr(hr, "Failed to re-open Geodatabase. Dataset should be closed");
        return OGRERR_FAILURE;
    }

    FGdbDataSource *pDS = new FGdbDataSource(true, pConnection, true);
    pDS->Open(osNameOri, TRUE, nullptr);
    // pDS->SetPerLayerCopyingForTransaction(bPerLayerCopyingForTransaction);
    poDSInOut = new OGRMutexedDataSource(pDS, TRUE, FGdbDriver::hMutex, TRUE);

    pConnection->SetLocked(FALSE);

    return eErr;
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
/*                       GetTransactionManager()                       */
/***********************************************************************/

FGdbTransactionManager *FGdbDriver::GetTransactionManager()
{
    CPLMutexHolderD(&FGdbDriver::hMutex);
    if (m_poTransactionManager == nullptr)
        m_poTransactionManager = new FGdbTransactionManager();
    return m_poTransactionManager;
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
