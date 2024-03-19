/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALDriverManager class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "gdal_priv.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <set>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_compressor.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_version_full/gdal_version.h"
#include "gdal_thread_pool.h"
#include "ogr_srs_api.h"
#include "ograpispy.h"
#ifdef HAVE_XERCES
#include "ogr_xerces.h"
#endif  // HAVE_XERCES

#ifdef _MSC_VER
#ifdef MSVC_USE_VLD
#include <wchar.h>
#include <vld.h>
#endif
#endif

// FIXME: Disabled following code as it crashed on OSX CI test.
// #include <mutex>

/************************************************************************/
/* ==================================================================== */
/*                           GDALDriverManager                          */
/* ==================================================================== */
/************************************************************************/

static volatile GDALDriverManager *poDM = nullptr;
static CPLMutex *hDMMutex = nullptr;

// FIXME: Disabled following code as it crashed on OSX CI test.
// static std::mutex oDeleteMutex;

CPLMutex **GDALGetphDMMutex()
{
    return &hDMMutex;
}

/************************************************************************/
/*                        GetGDALDriverManager()                        */
/*                                                                      */
/*      A freestanding function to get the only instance of the         */
/*      GDALDriverManager.                                              */
/************************************************************************/

/**
 * \brief Fetch the global GDAL driver manager.
 *
 * This function fetches the pointer to the singleton global driver manager.
 * If the driver manager doesn't exist it is automatically created.
 *
 * @return pointer to the global driver manager.  This should not be able
 * to fail.
 */

GDALDriverManager *GetGDALDriverManager()

{
    if (poDM == nullptr)
    {
        CPLMutexHolderD(&hDMMutex);
        // cppcheck-suppress identicalInnerCondition
        if (poDM == nullptr)
            poDM = new GDALDriverManager();
    }

    CPLAssert(nullptr != poDM);

    return const_cast<GDALDriverManager *>(poDM);
}

/************************************************************************/
/*                         GDALDriverManager()                          */
/************************************************************************/

#define XSTRINGIFY(x) #x
#define STRINGIFY(x) XSTRINGIFY(x)

GDALDriverManager::GDALDriverManager()
{
    CPLAssert(poDM == nullptr);

    CPLLoadConfigOptionsFromPredefinedFiles();

    CPLHTTPSetDefaultUserAgent(
        "GDAL/" STRINGIFY(GDAL_VERSION_MAJOR) "." STRINGIFY(
            GDAL_VERSION_MINOR) "." STRINGIFY(GDAL_VERSION_REV));

/* -------------------------------------------------------------------- */
/*      We want to push a location to search for data files             */
/*      supporting GDAL/OGR such as EPSG csv files, S-57 definition     */
/*      files, and so forth.  Use the INST_DATA macro (setup at         */
/*      configure time) if available. Otherwise we don't push anything  */
/*      and we hope other mechanisms such as environment variables will */
/*      have been employed.                                             */
/* -------------------------------------------------------------------- */
#ifdef INST_DATA
    if (CPLGetConfigOption("GDAL_DATA", nullptr) != nullptr)
    {
        // This one is picked up automatically by finder initialization.
    }
    else
    {
        CPLPushFinderLocation(INST_DATA);
    }
#endif
}

/************************************************************************/
/*                         ~GDALDriverManager()                         */
/************************************************************************/

// Keep these two in sync with gdalproxypool.cpp.
void GDALDatasetPoolPreventDestroy();
void GDALDatasetPoolForceDestroy();

GDALDriverManager::~GDALDriverManager()

{
    /* -------------------------------------------------------------------- */
    /*      Cleanup any open datasets.                                      */
    /* -------------------------------------------------------------------- */

    // We have to prevent the destroying of the dataset pool during this first
    // phase, otherwise it cause crashes with a VRT B referencing a VRT A, and
    // if CloseDependentDatasets() is called first on VRT A.
    // If we didn't do this nasty trick, due to the refCountOfDisableRefCount
    // mechanism that cheats the real refcount of the dataset pool, we might
    // destroy the dataset pool too early, leading the VRT A to
    // destroy itself indirectly ... Ok, I am aware this explanation does
    // not make any sense unless you try it under a debugger ...
    // When people just manipulate "top-level" dataset handles, we luckily
    // don't need this horrible hack, but GetOpenDatasets() expose "low-level"
    // datasets, which defeat some "design" of the proxy pool.
    GDALDatasetPoolPreventDestroy();

    // First begin by requesting each remaining dataset to drop any reference
    // to other datasets.
    bool bHasDroppedRef = false;

    do
    {
        int nDSCount = 0;
        GDALDataset **papoDSList = GDALDataset::GetOpenDatasets(&nDSCount);

        // If a dataset has dropped a reference, the list might have become
        // invalid, so go out of the loop and try again with the new valid
        // list.
        bHasDroppedRef = false;
        for (int i = 0; i < nDSCount && !bHasDroppedRef; ++i)
        {
#if DEBUG_VERBOSE
            CPLDebug("GDAL", "Call CloseDependentDatasets() on %s",
                     papoDSList[i]->GetDescription());
#endif  // DEBUG_VERBOSE
            bHasDroppedRef =
                CPL_TO_BOOL(papoDSList[i]->CloseDependentDatasets());
        }
    } while (bHasDroppedRef);

    // Now let's destroy the dataset pool. Nobody should use it afterwards
    // if people have well released their dependent datasets above.
    GDALDatasetPoolForceDestroy();

    // Now close the stand-alone datasets.
    int nDSCount = 0;
    GDALDataset **papoDSList = GDALDataset::GetOpenDatasets(&nDSCount);
    for (int i = 0; i < nDSCount; ++i)
    {
        CPLDebug("GDAL", "Force close of %s (%p) in GDALDriverManager cleanup.",
                 papoDSList[i]->GetDescription(), papoDSList[i]);
        // Destroy with delete operator rather than GDALClose() to force
        // deletion of datasets with multiple reference count.
        // We could also iterate while GetOpenDatasets() returns a non NULL
        // list.
        delete papoDSList[i];
    }

    /* -------------------------------------------------------------------- */
    /*      Destroy the existing drivers.                                   */
    /* -------------------------------------------------------------------- */
    while (GetDriverCount() > 0)
    {
        GDALDriver *poDriver = GetDriver(0);

        DeregisterDriver(poDriver);
        delete poDriver;
    }

    CleanupPythonDrivers();

    GDALDestroyGlobalThreadPool();

    /* -------------------------------------------------------------------- */
    /*      Cleanup local memory.                                           */
    /* -------------------------------------------------------------------- */
    VSIFree(papoDrivers);

    /* -------------------------------------------------------------------- */
    /*      Cleanup any Proxy related memory.                               */
    /* -------------------------------------------------------------------- */
    PamCleanProxyDB();

    /* -------------------------------------------------------------------- */
    /*      Cleanup any memory allocated by the OGRSpatialReference         */
    /*      related subsystem.                                              */
    /* -------------------------------------------------------------------- */
    OSRCleanup();

    /* -------------------------------------------------------------------- */
    /*      Blow away all the finder hints paths.  We really should not     */
    /*      be doing all of them, but it is currently hard to keep track    */
    /*      of those that actually belong to us.                            */
    /* -------------------------------------------------------------------- */
    CPLFinderClean();
    CPLFreeConfig();
    CPLCleanupSharedFileMutex();

#ifdef HAVE_XERCES
    OGRCleanupXercesMutex();
#endif

#ifdef OGRAPISPY_ENABLED
    OGRAPISpyDestroyMutex();
#endif

    /* -------------------------------------------------------------------- */
    /*      Cleanup VSIFileManager.                                         */
    /* -------------------------------------------------------------------- */
    VSICleanupFileManager();
    CPLDestroyCompressorRegistry();

    /* -------------------------------------------------------------------- */
    /*      Cleanup thread local storage ... I hope the program is all      */
    /*      done with GDAL/OGR!                                             */
    /* -------------------------------------------------------------------- */
    CPLCleanupTLS();

    /* -------------------------------------------------------------------- */
    /*      Cleanup our mutex.                                              */
    /* -------------------------------------------------------------------- */
    if (hDMMutex)
    {
        CPLDestroyMutex(hDMMutex);
        hDMMutex = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup dataset list mutex.                                     */
    /* -------------------------------------------------------------------- */
    if (*GDALGetphDLMutex() != nullptr)
    {
        CPLDestroyMutex(*GDALGetphDLMutex());
        *GDALGetphDLMutex() = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup raster block mutex.                                     */
    /* -------------------------------------------------------------------- */
    GDALRasterBlock::DestroyRBMutex();

    /* -------------------------------------------------------------------- */
    /*      Cleanup gdaltransformer.cpp mutex.                              */
    /* -------------------------------------------------------------------- */
    GDALCleanupTransformDeserializerMutex();

    /* -------------------------------------------------------------------- */
    /*      Cleanup cpl_error.cpp mutex.                                    */
    /* -------------------------------------------------------------------- */
    CPLCleanupErrorMutex();

    /* -------------------------------------------------------------------- */
    /*      Cleanup CPLsetlocale mutex.                                     */
    /* -------------------------------------------------------------------- */
    CPLCleanupSetlocaleMutex();

    /* -------------------------------------------------------------------- */
    /*      Cleanup curl related stuff.                                     */
    /* -------------------------------------------------------------------- */
    CPLHTTPCleanup();

    /* -------------------------------------------------------------------- */
    /*      Cleanup the master CPL mutex, which governs the creation        */
    /*      of all other mutexes.                                           */
    /* -------------------------------------------------------------------- */
    CPLCleanupMasterMutex();

    /* -------------------------------------------------------------------- */
    /*      Ensure the global driver manager pointer is NULLed out.         */
    /* -------------------------------------------------------------------- */
    if (poDM == this)
        poDM = nullptr;
}

/************************************************************************/
/*                           GetDriverCount()                           */
/************************************************************************/

/**
 * \brief Fetch the number of registered drivers.
 *
 * This C analog to this is GDALGetDriverCount().
 *
 * @return the number of registered drivers.
 */

int GDALDriverManager::GetDriverCount() const

{
    return nDrivers;
}

//! @cond Doxygen_Suppress
int GDALDriverManager::GetDriverCount(bool bIncludeHidden) const

{
    if (!bIncludeHidden)
        return nDrivers;
    return nDrivers + static_cast<int>(m_aoHiddenDrivers.size());
}

//! @endcond

/************************************************************************/
/*                         GDALGetDriverCount()                         */
/************************************************************************/

/**
 * \brief Fetch the number of registered drivers.
 *
 * @see GDALDriverManager::GetDriverCount()
 */

int CPL_STDCALL GDALGetDriverCount()

{
    return GetGDALDriverManager()->GetDriverCount();
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

/**
 * \brief Fetch driver by index.
 *
 * This C analog to this is GDALGetDriver().
 *
 * @param iDriver the driver index from 0 to GetDriverCount()-1.
 *
 * @return the driver identified by the index or NULL if the index is invalid
 */

GDALDriver *GDALDriverManager::GetDriver(int iDriver)

{
    CPLMutexHolderD(&hDMMutex);

    return GetDriver_unlocked(iDriver);
}

//! @cond Doxygen_Suppress
GDALDriver *GDALDriverManager::GetDriver(int iDriver, bool bIncludeHidden)

{
    CPLMutexHolderD(&hDMMutex);
    if (!bIncludeHidden || iDriver < nDrivers)
        return GetDriver_unlocked(iDriver);
    if (iDriver - nDrivers < static_cast<int>(m_aoHiddenDrivers.size()))
        return m_aoHiddenDrivers[iDriver - nDrivers].get();
    return nullptr;
}

//! @endcond

/************************************************************************/
/*                           GDALGetDriver()                            */
/************************************************************************/

/**
 * \brief Fetch driver by index.
 *
 * @see GDALDriverManager::GetDriver()
 */

GDALDriverH CPL_STDCALL GDALGetDriver(int iDriver)

{
    return /* (GDALDriverH) */ GetGDALDriverManager()->GetDriver(iDriver);
}

/************************************************************************/
/*                           RegisterDriver()                           */
/************************************************************************/

/**
 * \brief Register a driver for use.
 *
 * The C analog is GDALRegisterDriver().
 *
 * Normally this method is used by format specific C callable registration
 * entry points such as GDALRegister_GTiff() rather than being called
 * directly by application level code.
 *
 * If this driver (based on the object pointer, not short name) is already
 * registered, then no change is made, and the index of the existing driver
 * is returned.  Otherwise the driver list is extended, and the new driver
 * is added at the end.
 *
 * @param poDriver the driver to register.
 *
 * @return the index of the new installed driver.
 */

int GDALDriverManager::RegisterDriver(GDALDriver *poDriver)
{
    return RegisterDriver(poDriver, /*bHidden=*/false);
}

int GDALDriverManager::RegisterDriver(GDALDriver *poDriver, bool bHidden)
{
    CPLMutexHolderD(&hDMMutex);

    /* -------------------------------------------------------------------- */
    /*      If it is already registered, just return the existing           */
    /*      index.                                                          */
    /* -------------------------------------------------------------------- */
    if (!m_bInDeferredDriverLoading &&
        GetDriverByName_unlocked(poDriver->GetDescription()) != nullptr)
    {
        for (int i = 0; i < nDrivers; ++i)
        {
            if (papoDrivers[i] == poDriver)
            {
                return i;
            }
        }

        CPLAssert(false);
    }

    if (poDriver->pfnOpen != nullptr ||
        poDriver->pfnOpenWithDriverArg != nullptr)
        poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");

    if (poDriver->pfnCreate != nullptr || poDriver->pfnCreateEx != nullptr)
        poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");

    if (poDriver->pfnCreateCopy != nullptr)
        poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");

    if (poDriver->pfnCreateMultiDimensional != nullptr)
        poDriver->SetMetadataItem(GDAL_DCAP_CREATE_MULTIDIMENSIONAL, "YES");

    // Backward compatibility for GDAL raster out-of-tree drivers:
    // If a driver hasn't explicitly set a vector capability, assume it is
    // a raster-only driver (legacy OGR drivers will have DCAP_VECTOR set before
    // calling RegisterDriver()).
    if (poDriver->GetMetadataItem(GDAL_DCAP_RASTER) == nullptr &&
        poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr &&
        poDriver->GetMetadataItem(GDAL_DCAP_GNM) == nullptr)
    {
        CPLDebug("GDAL", "Assuming DCAP_RASTER for driver %s. Please fix it.",
                 poDriver->GetDescription());
        poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    }

    if (poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST) != nullptr &&
        poDriver->pfnIdentify == nullptr &&
        poDriver->pfnIdentifyEx == nullptr &&
        !STARTS_WITH_CI(poDriver->GetDescription(), "Interlis"))
    {
        CPLDebug("GDAL",
                 "Driver %s that defines GDAL_DMD_OPENOPTIONLIST must also "
                 "implement Identify(), so that it can be used",
                 poDriver->GetDescription());
    }

    if (poDriver->pfnVectorTranslateFrom != nullptr)
        poDriver->SetMetadataItem(GDAL_DCAP_VECTOR_TRANSLATE_FROM, "YES");

    if (m_bInDeferredDriverLoading)
    {
        if (m_oMapRealDrivers.find(poDriver->GetDescription()) !=
            m_oMapRealDrivers.end())
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "RegisterDriver() in m_bInDeferredDriverLoading: %s already "
                "registered!",
                poDriver->GetDescription());
            delete poDriver;
            return -1;
        }
        m_oMapRealDrivers[poDriver->GetDescription()] =
            std::unique_ptr<GDALDriver>(poDriver);
        return -1;
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise grow the list to hold the new entry.                  */
    /* -------------------------------------------------------------------- */
    if (bHidden)
    {
        m_aoHiddenDrivers.push_back(std::unique_ptr<GDALDriver>(poDriver));
        return -1;
    }

    GDALDriver **papoNewDrivers =
        static_cast<GDALDriver **>(VSI_REALLOC_VERBOSE(
            papoDrivers, sizeof(GDALDriver *) * (nDrivers + 1)));
    if (papoNewDrivers == nullptr)
        return -1;
    papoDrivers = papoNewDrivers;

    papoDrivers[nDrivers] = poDriver;
    ++nDrivers;

    oMapNameToDrivers[CPLString(poDriver->GetDescription()).toupper()] =
        poDriver;

    int iResult = nDrivers - 1;

    return iResult;
}

/************************************************************************/
/*                         GDALRegisterDriver()                         */
/************************************************************************/

/**
 * \brief Register a driver for use.
 *
 * @see GDALDriverManager::GetRegisterDriver()
 */

int CPL_STDCALL GDALRegisterDriver(GDALDriverH hDriver)

{
    VALIDATE_POINTER1(hDriver, "GDALRegisterDriver", 0);

    return GetGDALDriverManager()->RegisterDriver(
        static_cast<GDALDriver *>(hDriver));
}

/************************************************************************/
/*                          DeregisterDriver()                          */
/************************************************************************/

/**
 * \brief Deregister the passed driver.
 *
 * If the driver isn't found no change is made.
 *
 * The C analog is GDALDeregisterDriver().
 *
 * @param poDriver the driver to deregister.
 */

void GDALDriverManager::DeregisterDriver(GDALDriver *poDriver)

{
    CPLMutexHolderD(&hDMMutex);

    int i = 0;  // Used after for.
    for (; i < nDrivers; ++i)
    {
        if (papoDrivers[i] == poDriver)
            break;
    }

    if (i == nDrivers)
        return;

    oMapNameToDrivers.erase(CPLString(poDriver->GetDescription()).toupper());
    --nDrivers;
    // Move all following drivers down by one to pack the list.
    while (i < nDrivers)
    {
        papoDrivers[i] = papoDrivers[i + 1];
        ++i;
    }
}

/************************************************************************/
/*                        GDALDeregisterDriver()                        */
/************************************************************************/

/**
 * \brief Deregister the passed driver.
 *
 * @see GDALDriverManager::GetDeregisterDriver()
 */

void CPL_STDCALL GDALDeregisterDriver(GDALDriverH hDriver)

{
    VALIDATE_POINTER0(hDriver, "GDALDeregisterDriver");

    GetGDALDriverManager()->DeregisterDriver(
        static_cast<GDALDriver *>(hDriver));
}

/************************************************************************/
/*                          GetDriverByName()                           */
/************************************************************************/

/**
 * \brief Fetch a driver based on the short name.
 *
 * The C analog is the GDALGetDriverByName() function.
 *
 * @param pszName the short name, such as GTiff, being searched for.
 *
 * @return the identified driver, or NULL if no match is found.
 */

GDALDriver *GDALDriverManager::GetDriverByName(const char *pszName)

{
    CPLMutexHolderD(&hDMMutex);

    if (m_bInDeferredDriverLoading)
    {
        return nullptr;
    }

    // Alias old name to new name
    if (EQUAL(pszName, "CartoDB"))
        pszName = "Carto";

    return GetDriverByName_unlocked(pszName);
}

/************************************************************************/
/*                        GDALGetDriverByName()                         */
/************************************************************************/

/**
 * \brief Fetch a driver based on the short name.
 *
 * @see GDALDriverManager::GetDriverByName()
 */

GDALDriverH CPL_STDCALL GDALGetDriverByName(const char *pszName)

{
    VALIDATE_POINTER1(pszName, "GDALGetDriverByName", nullptr);

    return GetGDALDriverManager()->GetDriverByName(pszName);
}

/************************************************************************/
/*                          AutoSkipDrivers()                           */
/************************************************************************/

/**
 * \brief This method unload undesirable drivers.
 *
 * All drivers specified in the comma delimited list in the GDAL_SKIP
 * environment variable) will be deregistered and destroyed.  This method
 * should normally be called after registration of standard drivers to allow
 * the user a way of unloading undesired drivers.  The GDALAllRegister()
 * function already invokes AutoSkipDrivers() at the end, so if that functions
 * is called, it should not be necessary to call this method from application
 * code.
 *
 * Note: space separator is also accepted for backward compatibility, but some
 * vector formats have spaces in their names, so it is encouraged to use comma
 * to avoid issues.
 */

void GDALDriverManager::AutoSkipDrivers()

{
    char **apapszList[2] = {nullptr, nullptr};
    const char *pszGDAL_SKIP = CPLGetConfigOption("GDAL_SKIP", nullptr);
    if (pszGDAL_SKIP != nullptr)
    {
        // Favor comma as a separator. If not found, then use space.
        const char *pszSep = (strchr(pszGDAL_SKIP, ',') != nullptr) ? "," : " ";
        apapszList[0] =
            CSLTokenizeStringComplex(pszGDAL_SKIP, pszSep, FALSE, FALSE);
    }
    const char *pszOGR_SKIP = CPLGetConfigOption("OGR_SKIP", nullptr);
    if (pszOGR_SKIP != nullptr)
    {
        // OGR has always used comma as a separator.
        apapszList[1] =
            CSLTokenizeStringComplex(pszOGR_SKIP, ",", FALSE, FALSE);
    }

    for (auto j : {0, 1})
    {
        for (int i = 0; apapszList[j] != nullptr && apapszList[j][i] != nullptr;
             ++i)
        {
            GDALDriver *const poDriver = GetDriverByName(apapszList[j][i]);

            if (poDriver == nullptr)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Unable to find driver %s to unload from GDAL_SKIP "
                         "environment variable.",
                         apapszList[j][i]);
            }
            else
            {
                CPLDebug("GDAL", "AutoSkipDriver(%s)", apapszList[j][i]);
                DeregisterDriver(poDriver);
                delete poDriver;
            }
        }
    }

    CSLDestroy(apapszList[0]);
    CSLDestroy(apapszList[1]);
}

/************************************************************************/
/*                          GetSearchPaths()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
char **GDALDriverManager::GetSearchPaths(const char *pszGDAL_DRIVER_PATH)
{
    char **papszSearchPaths = nullptr;
    CPL_IGNORE_RET_VAL(pszGDAL_DRIVER_PATH);
#ifndef GDAL_NO_AUTOLOAD
    if (pszGDAL_DRIVER_PATH != nullptr)
    {
#ifdef _WIN32
        papszSearchPaths =
            CSLTokenizeStringComplex(pszGDAL_DRIVER_PATH, ";", TRUE, FALSE);
#else
        papszSearchPaths =
            CSLTokenizeStringComplex(pszGDAL_DRIVER_PATH, ":", TRUE, FALSE);
#endif
    }
    else
    {
#ifdef INSTALL_PLUGIN_FULL_DIR
        // CMake way
        papszSearchPaths =
            CSLAddString(papszSearchPaths, INSTALL_PLUGIN_FULL_DIR);
#elif defined(GDAL_PREFIX)
        papszSearchPaths = CSLAddString(papszSearchPaths,
#ifdef MACOSX_FRAMEWORK
                                        GDAL_PREFIX "/PlugIns");
#else
                                        GDAL_PREFIX "/lib/gdalplugins");
#endif
#else
        char szExecPath[1024];

        if (CPLGetExecPath(szExecPath, sizeof(szExecPath)))
        {
            char szPluginDir[sizeof(szExecPath) + 50];
            strcpy(szPluginDir, CPLGetDirname(szExecPath));
            strcat(szPluginDir, "\\gdalplugins");
            papszSearchPaths = CSLAddString(papszSearchPaths, szPluginDir);
        }
        else
        {
            papszSearchPaths =
                CSLAddString(papszSearchPaths, "/usr/local/lib/gdalplugins");
        }
#endif

#ifdef MACOSX_FRAMEWORK
#define num2str(x) str(x)
#define str(x) #x
        papszSearchPaths = CSLAddString(
            papszSearchPaths,
            "/Library/Application Support/GDAL/" num2str(
                GDAL_VERSION_MAJOR) "." num2str(GDAL_VERSION_MINOR) "/PlugIns");
#endif
    }
#endif  // GDAL_NO_AUTOLOAD
    return papszSearchPaths;
}

//! @endcond

/************************************************************************/
/*                          LoadPlugin()                                */
/************************************************************************/

/**
 * \brief Load a single GDAL driver/plugin from shared libraries.
 *
 * This function will load a single named driver/plugin from shared libraries.
 * It searches the "driver path" for .so (or .dll) files named
 * "gdal_{name}.[so|dll|dylib]" or "ogr_{name}.[so|dll|dylib]", then tries to
 * call a function within them called GDALRegister_{name}(), or failing that
 * called GDALRegisterMe().
 *
 * \see GDALDriverManager::AutoLoadDrivers() for the rules used to determine
 * which paths are searched for plugin library files.
 */

CPLErr GDALDriverManager::LoadPlugin(const char *name)
{
#ifdef GDAL_NO_AUTOLOAD
    CPLDebug("GDAL", "GDALDriverManager::LoadPlugin() not compiled in.");
    return CE_Failure;
#else
    const char *pszGDAL_DRIVER_PATH =
        CPLGetConfigOption("GDAL_DRIVER_PATH", nullptr);
    if (pszGDAL_DRIVER_PATH == nullptr)
        pszGDAL_DRIVER_PATH = CPLGetConfigOption("OGR_DRIVER_PATH", nullptr);

    /* -------------------------------------------------------------------- */
    /*      Where should we look for stuff?                                 */
    /* -------------------------------------------------------------------- */
    const CPLStringList aosSearchPaths(GetSearchPaths(pszGDAL_DRIVER_PATH));

    /* -------------------------------------------------------------------- */
    /*      Format the ABI version specific subdirectory to look in.        */
    /* -------------------------------------------------------------------- */
    CPLString osABIVersion;

    osABIVersion.Printf("%d.%d", GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR);

    /* -------------------------------------------------------------------- */
    /*      Scan each directory looking for files matching                  */
    /*      gdal_{name}.[so|dll|dylib] or ogr_{name}.[so|dll|dylib]         */
    /* -------------------------------------------------------------------- */
    const int nSearchPaths = aosSearchPaths.size();
    for (int iDir = 0; iDir < nSearchPaths; ++iDir)
    {
        CPLString osABISpecificDir =
            CPLFormFilename(aosSearchPaths[iDir], osABIVersion, nullptr);

        VSIStatBufL sStatBuf;
        if (VSIStatL(osABISpecificDir, &sStatBuf) != 0)
            osABISpecificDir = aosSearchPaths[iDir];

        CPLString gdal_or_ogr[2] = {"gdal_", "ogr_"};
        CPLString platformExtensions[3] = {"so", "dll", "dylib"};

        for (const CPLString &prefix : gdal_or_ogr)
        {
            for (const CPLString &extension : platformExtensions)
            {
                const char *pszFilename = CPLFormFilename(
                    osABISpecificDir, CPLSPrintf("%s%s", prefix.c_str(), name),
                    extension);
                if (VSIStatL(pszFilename, &sStatBuf) != 0)
                    continue;

                CPLString osFuncName;
                if (EQUAL(prefix, "gdal_"))
                {
                    osFuncName.Printf("GDALRegister_%s", name);
                }
                else
                {
                    osFuncName.Printf("RegisterOGR%s", name);
                }
                CPLErrorReset();
                CPLPushErrorHandler(CPLQuietErrorHandler);
                void *pRegister = CPLGetSymbol(pszFilename, osFuncName);
                CPLPopErrorHandler();
                if (pRegister == nullptr)
                {
                    CPLString osLastErrorMsg(CPLGetLastErrorMsg());
                    osFuncName = "GDALRegisterMe";
                    pRegister = CPLGetSymbol(pszFilename, osFuncName);
                    if (pRegister == nullptr)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                                 osLastErrorMsg.c_str());
                        return CE_Failure;
                    }
                }
                CPLDebug("GDAL", "Registering %s using %s in %s", name,
                         osFuncName.c_str(), pszFilename);
                CPLErrorReset();
                reinterpret_cast<void (*)()>(pRegister)();
                if (CPLGetErrorCounter() > 0)
                {
                    return CE_Failure;
                }
                return CE_None;
            }
        }
    }
    CPLError(CE_Failure, CPLE_AppDefined,
             "Failed to find driver %s in configured driver paths.", name);
    return CE_Failure;
#endif  // GDAL_NO_AUTOLOAD
}

/************************************************************************/
/*                          AutoLoadDrivers()                           */
/************************************************************************/

/**
 * \brief Auto-load GDAL drivers from shared libraries.
 *
 * This function will automatically load drivers from shared libraries.  It
 * searches the "driver path" for .so (or .dll) files that start with the
 * prefix "gdal_X.so".  It then tries to load them and then tries to call a
 * function within them called GDALRegister_X() where the 'X' is the same as
 * the remainder of the shared library basename ('X' is case sensitive), or
 * failing that to call GDALRegisterMe().
 *
 * There are a few rules for the driver path.  If the GDAL_DRIVER_PATH
 * environment variable it set, it is taken to be a list of directories to
 * search separated by colons on UNIX, or semi-colons on Windows.  Otherwise
 * the /usr/local/lib/gdalplugins directory, and (if known) the
 * lib/gdalplugins subdirectory of the gdal home directory are searched on
 * UNIX and $(BINDIR)\\gdalplugins on Windows.
 *
 * Auto loading can be completely disabled by setting the GDAL_DRIVER_PATH
 * config option to "disable".
 *
 * Starting with gdal 3.5, the default search path $(prefix)/lib/gdalplugins
 * can be overridden at compile time by passing
 * -DINSTALL_PLUGIN_DIR=/another/path to cmake.
 */

void GDALDriverManager::AutoLoadDrivers()

{
#ifdef GDAL_NO_AUTOLOAD
    CPLDebug("GDAL", "GDALDriverManager::AutoLoadDrivers() not compiled in.");
#else
    const char *pszGDAL_DRIVER_PATH =
        CPLGetConfigOption("GDAL_DRIVER_PATH", nullptr);
    if (pszGDAL_DRIVER_PATH == nullptr)
        pszGDAL_DRIVER_PATH = CPLGetConfigOption("OGR_DRIVER_PATH", nullptr);

    /* -------------------------------------------------------------------- */
    /*      Allow applications to completely disable this search by         */
    /*      setting the driver path to the special string "disable".        */
    /* -------------------------------------------------------------------- */
    if (pszGDAL_DRIVER_PATH != nullptr && EQUAL(pszGDAL_DRIVER_PATH, "disable"))
    {
        CPLDebug("GDAL", "GDALDriverManager::AutoLoadDrivers() disabled.");
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Where should we look for stuff?                                 */
    /* -------------------------------------------------------------------- */
    char **papszSearchPaths = GetSearchPaths(pszGDAL_DRIVER_PATH);

    /* -------------------------------------------------------------------- */
    /*      Format the ABI version specific subdirectory to look in.        */
    /* -------------------------------------------------------------------- */
    CPLString osABIVersion;

    osABIVersion.Printf("%d.%d", GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR);

    /* -------------------------------------------------------------------- */
    /*      Scan each directory looking for files starting with gdal_       */
    /* -------------------------------------------------------------------- */
    const int nSearchPaths = CSLCount(papszSearchPaths);
    bool bFoundOnePlugin = false;
    for (int iDir = 0; iDir < nSearchPaths; ++iDir)
    {
        CPLString osABISpecificDir =
            CPLFormFilename(papszSearchPaths[iDir], osABIVersion, nullptr);

        VSIStatBufL sStatBuf;
        if (VSIStatL(osABISpecificDir, &sStatBuf) != 0)
            osABISpecificDir = papszSearchPaths[iDir];

        char **papszFiles = VSIReadDir(osABISpecificDir);
        const int nFileCount = CSLCount(papszFiles);

        for (int iFile = 0; iFile < nFileCount; ++iFile)
        {
            const char *pszExtension = CPLGetExtension(papszFiles[iFile]);

            if (!EQUAL(pszExtension, "dll") && !EQUAL(pszExtension, "so") &&
                !EQUAL(pszExtension, "dylib"))
            {
                if (strcmp(papszFiles[iFile], "drivers.ini") == 0)
                {
                    m_osDriversIniPath = CPLFormFilename(
                        osABISpecificDir, papszFiles[iFile], nullptr);
                }
                continue;
            }

            if (m_oSetPluginFileNames.find(papszFiles[iFile]) !=
                m_oSetPluginFileNames.end())
            {
                continue;
            }

            CPLString osFuncName;
            if (STARTS_WITH_CI(papszFiles[iFile], "gdal_"))
            {
                osFuncName.Printf("GDALRegister_%s",
                                  CPLGetBasename(papszFiles[iFile]) +
                                      strlen("gdal_"));
            }
            else if (STARTS_WITH_CI(papszFiles[iFile], "ogr_"))
            {
                osFuncName.Printf("RegisterOGR%s",
                                  CPLGetBasename(papszFiles[iFile]) +
                                      strlen("ogr_"));
            }
            else
                continue;

            const char *pszFilename =
                CPLFormFilename(osABISpecificDir, papszFiles[iFile], nullptr);

            CPLErrorReset();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            void *pRegister = CPLGetSymbol(pszFilename, osFuncName);
            CPLPopErrorHandler();
            if (pRegister == nullptr)
            {
                CPLString osLastErrorMsg(CPLGetLastErrorMsg());
                osFuncName = "GDALRegisterMe";
                pRegister = CPLGetSymbol(pszFilename, osFuncName);
                if (pRegister == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s",
                             osLastErrorMsg.c_str());
                }
            }

            if (pRegister != nullptr)
            {
                bFoundOnePlugin = true;
                CPLDebug("GDAL", "Auto register %s using %s.", pszFilename,
                         osFuncName.c_str());

                reinterpret_cast<void (*)()>(pRegister)();
            }
        }

        CSLDestroy(papszFiles);
    }

    CSLDestroy(papszSearchPaths);

    // No need to reorder drivers if there are no plugins
    if (!bFoundOnePlugin)
        m_osDriversIniPath.clear();

#endif  // GDAL_NO_AUTOLOAD
}

/************************************************************************/
/*                           ReorderDrivers()                           */
/************************************************************************/

/**
 * \brief Reorder drivers according to the order of the drivers.ini file.
 *
 * This function is called by GDALAllRegister(), at the end of driver loading,
 * in particular after plugin loading.
 * It will load the drivers.ini configuration file located next to plugins and
 * will use it to reorder the registration order of drivers. This can be
 * important in some situations where multiple drivers could open the same
 * dataset.
 */

void GDALDriverManager::ReorderDrivers()
{
#ifndef GDAL_NO_AUTOLOAD
    if (m_osDriversIniPath.empty())
    {
        if (m_oSetPluginFileNames.empty())
            return;

        m_osDriversIniPath = GetPluginFullPath("drivers.ini");
        if (m_osDriversIniPath.empty())
            return;
    }

    CPLMutexHolderD(&hDMMutex);

    CPLAssert(static_cast<int>(oMapNameToDrivers.size()) == nDrivers);

    VSILFILE *fp = VSIFOpenL(m_osDriversIniPath.c_str(), "rb");
    if (fp == nullptr)
        return;

    // Parse drivers.ini
    bool bInOrderSection = false;
    std::vector<std::string> aosOrderedDrivers;
    std::set<std::string> oSetOrderedDrivers;
    while (const char *pszLine = CPLReadLine2L(fp, 1024, nullptr))
    {
        if (pszLine[0] == '#')
            continue;
        int i = 0;
        while (pszLine[i] != 0 &&
               isspace(static_cast<unsigned char>(pszLine[i])))
            i++;
        if (pszLine[i] == 0)
            continue;
        if (strcmp(pszLine, "[order]") == 0)
        {
            bInOrderSection = true;
        }
        else if (pszLine[0] == '[')
        {
            bInOrderSection = false;
        }
        else if (bInOrderSection)
        {
            CPLString osUCDriverName(pszLine);
            osUCDriverName.toupper();
            if (oSetOrderedDrivers.find(osUCDriverName) !=
                oSetOrderedDrivers.end())
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Duplicated name %s in [order] section", pszLine);
            }
            else if (oMapNameToDrivers.find(osUCDriverName) !=
                     oMapNameToDrivers.end())
            {
                aosOrderedDrivers.emplace_back(pszLine);
                oSetOrderedDrivers.insert(osUCDriverName);
            }
#ifdef DEBUG_VERBOSE
            else
            {
                // Completely expected situation for "non-maximal" builds,
                // but can help diagnose bad entries in drivers.ini
                CPLDebug("GDAL",
                         "Driver %s is listed in %s but not registered.",
                         pszLine, m_osDriversIniPath.c_str());
            }
#endif
        }
    }
    VSIFCloseL(fp);

    // Find potential registered drivers not in drivers.ini, and put them in
    // their registration order in aosUnorderedDrivers
    std::vector<std::string> aosUnorderedDrivers;
    for (int i = 0; i < nDrivers; ++i)
    {
        const char *pszName = papoDrivers[i]->GetDescription();
        if (oSetOrderedDrivers.find(CPLString(pszName).toupper()) ==
            oSetOrderedDrivers.end())
        {
            // Could happen for a private plugin
            CPLDebug("GDAL",
                     "Driver %s is registered but not listed in %s. "
                     "It will be registered before other drivers.",
                     pszName, m_osDriversIniPath.c_str());
            aosUnorderedDrivers.emplace_back(pszName);
        }
    }

    // Put aosUnorderedDrivers in front of existing aosOrderedDrivers
    if (!aosUnorderedDrivers.empty())
    {
        aosUnorderedDrivers.insert(aosUnorderedDrivers.end(),
                                   aosOrderedDrivers.begin(),
                                   aosOrderedDrivers.end());
        std::swap(aosOrderedDrivers, aosUnorderedDrivers);
    }

    // Update papoDrivers[] to reflect aosOrderedDrivers order.
    CPLAssert(static_cast<int>(aosOrderedDrivers.size()) == nDrivers);
    for (int i = 0; i < nDrivers; ++i)
    {
        const auto oIter =
            oMapNameToDrivers.find(CPLString(aosOrderedDrivers[i]).toupper());
        CPLAssert(oIter != oMapNameToDrivers.end());
        papoDrivers[i] = oIter->second;
    }
#endif
}

/************************************************************************/
/*                       GDALPluginDriverProxy                          */
/************************************************************************/

/** Constructor for a plugin driver proxy.
 *
 * @param osPluginFileName Plugin filename. e.g "ogr_Parquet.so"
 */
GDALPluginDriverProxy::GDALPluginDriverProxy(
    const std::string &osPluginFileName)
    : m_osPluginFileName(osPluginFileName)
{
}

//! @cond Doxygen_Suppress
#define DEFINE_DRIVER_METHOD_GET_CALLBACK(method_name, output_type)            \
    GDALDriver::output_type GDALPluginDriverProxy::method_name()               \
    {                                                                          \
        auto poRealDriver = GetRealDriver();                                   \
        if (!poRealDriver)                                                     \
            return nullptr;                                                    \
        return poRealDriver->method_name();                                    \
    }

DEFINE_DRIVER_METHOD_GET_CALLBACK(GetOpenCallback, OpenCallback)
DEFINE_DRIVER_METHOD_GET_CALLBACK(GetCreateCallback, CreateCallback)
DEFINE_DRIVER_METHOD_GET_CALLBACK(GetCreateMultiDimensionalCallback,
                                  CreateMultiDimensionalCallback)
DEFINE_DRIVER_METHOD_GET_CALLBACK(GetCreateCopyCallback, CreateCopyCallback)
DEFINE_DRIVER_METHOD_GET_CALLBACK(GetDeleteCallback, DeleteCallback)
DEFINE_DRIVER_METHOD_GET_CALLBACK(GetRenameCallback, RenameCallback)
DEFINE_DRIVER_METHOD_GET_CALLBACK(GetCopyFilesCallback, CopyFilesCallback)

//! @endcond

char **GDALPluginDriverProxy::GetMetadata(const char *pszDomain)
{
    auto poRealDriver = GetRealDriver();
    if (!poRealDriver)
        return nullptr;
    return poRealDriver->GetMetadata(pszDomain);
}

CPLErr GDALPluginDriverProxy::SetMetadataItem(const char *pszName,
                                              const char *pszValue,
                                              const char *pszDomain)
{
    if (!pszDomain || pszDomain[0] == 0)
    {
        if (!EQUAL(pszName, GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE))
        {
            m_oSetMetadataItems.insert(pszName);
        }
    }
    return GDALDriver::SetMetadataItem(pszName, pszValue, pszDomain);
}

static const char *const apszProxyMetadataItems[] = {
    GDAL_DMD_LONGNAME,
    GDAL_DMD_EXTENSIONS,
    GDAL_DMD_EXTENSION,
    GDAL_DCAP_RASTER,
    GDAL_DCAP_MULTIDIM_RASTER,
    GDAL_DCAP_VECTOR,
    GDAL_DCAP_GNM,
    GDAL_DMD_OPENOPTIONLIST,
    GDAL_DCAP_OPEN,
    GDAL_DCAP_CREATE,
    GDAL_DCAP_CREATE_MULTIDIMENSIONAL,
    GDAL_DCAP_CREATECOPY,
    GDAL_DMD_SUBDATASETS,
    GDAL_DCAP_MULTIPLE_VECTOR_LAYERS,
    GDAL_DCAP_NONSPATIAL,
    GDAL_DMD_CONNECTION_PREFIX,
    GDAL_DCAP_VECTOR_TRANSLATE_FROM,
    GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
};

const char *GDALPluginDriverProxy::GetMetadataItem(const char *pszName,
                                                   const char *pszDomain)
{
    const auto IsListedProxyMetadataItem = [](const char *pszItem)
    {
        for (const char *pszListedItem : apszProxyMetadataItems)
        {
            if (EQUAL(pszItem, pszListedItem))
                return true;
        }
        return false;
    };

    if (!pszDomain || pszDomain[0] == 0)
    {
        if (EQUAL(pszName, "IS_NON_LOADED_PLUGIN"))
        {
            return !m_poRealDriver ? "YES" : nullptr;
        }
        else if (EQUAL(pszName, "MISSING_PLUGIN_FILENAME"))
        {
            return m_osPluginFullPath.empty() ? m_osPluginFileName.c_str()
                                              : nullptr;
        }
        else if (IsListedProxyMetadataItem(pszName))
        {
            const char *pszValue =
                GDALDriver::GetMetadataItem(pszName, pszDomain);
            if (!pszValue && EQUAL(pszName, GDAL_DMD_EXTENSION))
            {
                const char *pszOtherValue =
                    GDALDriver::GetMetadataItem(GDAL_DMD_EXTENSIONS, pszDomain);
                if (pszOtherValue && strchr(pszOtherValue, ' '))
                    return pszOtherValue;
            }
            else if (!pszValue && EQUAL(pszName, GDAL_DMD_EXTENSIONS))
            {
                return GDALDriver::GetMetadataItem(GDAL_DMD_EXTENSION,
                                                   pszDomain);
            }
            return pszValue;
        }
        else if (m_oSetMetadataItems.find(pszName) != m_oSetMetadataItems.end())
        {
            return GDALDriver::GetMetadataItem(pszName, pszDomain);
        }
    }

    auto poRealDriver = GetRealDriver();
    if (!poRealDriver)
        return nullptr;
    return poRealDriver->GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                           GetRealDriver()                            */
/************************************************************************/

GDALDriver *GDALPluginDriverProxy::GetRealDriver()
{
    // No need to take the mutex has this member variable is not modified
    // under the mutex.
    if (m_osPluginFullPath.empty())
        return nullptr;

    CPLMutexHolderD(&hDMMutex);

    if (m_poRealDriver)
        return m_poRealDriver.get();

    auto poDriverManager = GetGDALDriverManager();
    auto oIter = poDriverManager->m_oMapRealDrivers.find(GetDescription());
    if (oIter != poDriverManager->m_oMapRealDrivers.end())
    {
        m_poRealDriver = std::move(oIter->second);
        poDriverManager->m_oMapRealDrivers.erase(oIter);
    }
    else
    {
        CPLString osFuncName;
        if (STARTS_WITH(m_osPluginFileName.c_str(), "gdal_"))
        {
            osFuncName = "GDALRegister_";
            osFuncName += m_osPluginFileName.substr(
                strlen("gdal_"),
                m_osPluginFileName.find('.') - strlen("gdal_"));
        }
        else
        {
            CPLAssert(STARTS_WITH(m_osPluginFileName.c_str(), "ogr_"));
            osFuncName = "RegisterOGR";
            osFuncName += m_osPluginFileName.substr(
                strlen("ogr_"), m_osPluginFileName.find('.') - strlen("ogr_"));
        }

        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        void *pRegister = CPLGetSymbol(m_osPluginFullPath.c_str(), osFuncName);
        CPLPopErrorHandler();
        if (pRegister == nullptr)
        {
            CPLString osLastErrorMsg(CPLGetLastErrorMsg());
            osFuncName = "GDALRegisterMe";
            pRegister = CPLGetSymbol(m_osPluginFullPath.c_str(), osFuncName);
            if (pRegister == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s",
                         osLastErrorMsg.c_str());
            }
        }

        if (pRegister != nullptr)
        {
            CPLDebug("GDAL", "On-demand registering %s using %s.",
                     m_osPluginFullPath.c_str(), osFuncName.c_str());

            poDriverManager->m_bInDeferredDriverLoading = true;
            try
            {
                reinterpret_cast<void (*)()>(pRegister)();
            }
            catch (...)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s threw an exception",
                         osFuncName.c_str());
            }
            poDriverManager->m_bInDeferredDriverLoading = false;

            oIter = poDriverManager->m_oMapRealDrivers.find(GetDescription());
            if (oIter == poDriverManager->m_oMapRealDrivers.end())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Function %s of %s did not register a driver %s",
                         osFuncName.c_str(), m_osPluginFullPath.c_str(),
                         GetDescription());
            }
            else
            {
                m_poRealDriver = std::move(oIter->second);
                poDriverManager->m_oMapRealDrivers.erase(oIter);
            }
        }
    }

    if (m_poRealDriver)
    {
        pfnDelete = m_poRealDriver->pfnDelete;
        pfnRename = m_poRealDriver->pfnRename;
        pfnCopyFiles = m_poRealDriver->pfnCopyFiles;

        if (strcmp(GetDescription(), m_poRealDriver->GetDescription()) != 0)
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Driver %s has not the same name as its underlying driver (%s)",
                GetDescription(), m_poRealDriver->GetDescription());
        }

        for (const auto &osItem : m_oSetMetadataItems)
        {
            const char *pszProxyValue = GetMetadataItem(osItem.c_str());
            const char *pszRealValue =
                m_poRealDriver->GetMetadataItem(osItem.c_str());
            if (pszProxyValue &&
                (!pszRealValue || strcmp(pszProxyValue, pszRealValue) != 0))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Proxy driver %s declares %s whereas its real driver "
                         "doesn't declare it or with a different value",
                         GetDescription(), osItem.c_str());
            }
        }
        for (const char *pszListedItem : apszProxyMetadataItems)
        {
            const char *pszRealValue =
                m_poRealDriver->GetMetadataItem(pszListedItem);
            if (pszRealValue)
            {
                const char *pszProxyValue = GetMetadataItem(pszListedItem);
                if (!pszProxyValue || strcmp(pszProxyValue, pszRealValue) != 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Driver %s declares %s whereas its proxy "
                             "doesn't declare it or with a different value",
                             GetDescription(), pszListedItem);
                }
            }
        }

        const auto CheckFunctionPointer =
            [this](void *pfnFuncProxy, void *pfnFuncReal, const char *pszFunc)
        {
            if (pfnFuncReal && !pfnFuncProxy)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Driver %s declares a %s callback whereas its proxy "
                         "does not declare it",
                         GetDescription(), pszFunc);
            }
            else if (!pfnFuncReal && pfnFuncProxy)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Proxy driver %s declares a %s callback whereas the "
                         "real driver does not.",
                         GetDescription(), pszFunc);
            }
        };

        CheckFunctionPointer(
            reinterpret_cast<void *>(m_poRealDriver->pfnIdentify),
            reinterpret_cast<void *>(pfnIdentify), "pfnIdentify");

        // The real driver might provide a more accurate identification method
        if (m_poRealDriver->pfnIdentify)
            pfnIdentify = m_poRealDriver->pfnIdentify;

        CheckFunctionPointer(
            reinterpret_cast<void *>(m_poRealDriver->pfnGetSubdatasetInfoFunc),
            reinterpret_cast<void *>(pfnGetSubdatasetInfoFunc),
            "pfnGetSubdatasetInfoFunc");

        const auto CheckFunctionPointerVersusCap =
            [this](void *pfnFunc, const char *pszFunc, const char *pszItemName)
        {
            if (pfnFunc && !GetMetadataItem(pszItemName))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Driver %s declares a %s callback whereas its proxy "
                         "doest not declare %s",
                         GetDescription(), pszFunc, pszItemName);
            }
            else if (!pfnFunc && GetMetadataItem(pszItemName))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Proxy driver %s declares %s whereas the real "
                         "driver does not declare a %s callback",
                         GetDescription(), pszItemName, pszFunc);
            }
        };

        CheckFunctionPointerVersusCap(
            reinterpret_cast<void *>(m_poRealDriver->pfnOpen), "pfnOpen",
            GDAL_DCAP_OPEN);
        CheckFunctionPointerVersusCap(
            reinterpret_cast<void *>(m_poRealDriver->pfnCreate), "pfnCreate",
            GDAL_DCAP_CREATE);
        CheckFunctionPointerVersusCap(
            reinterpret_cast<void *>(m_poRealDriver->pfnCreateCopy),
            "pfnCreateCopy", GDAL_DCAP_CREATECOPY);
        CheckFunctionPointerVersusCap(
            reinterpret_cast<void *>(m_poRealDriver->pfnCreateMultiDimensional),
            "pfnCreateMultiDimensional", GDAL_DCAP_CREATE_MULTIDIMENSIONAL);
    }

    return m_poRealDriver.get();
}

/************************************************************************/
/*                        GetPluginFullPath()                           */
/************************************************************************/

std::string GDALDriverManager::GetPluginFullPath(const char *pszFilename) const
{
    if (!m_osLastTriedDirectory.empty())
    {
        const char *pszFullFilename = CPLFormFilename(
            m_osLastTriedDirectory.c_str(), pszFilename, nullptr);
        VSIStatBufL sStatBuf;
        if (VSIStatL(pszFullFilename, &sStatBuf) == 0)
        {
            return pszFullFilename;
        }
    }

    const char *pszGDAL_DRIVER_PATH =
        CPLGetConfigOption("GDAL_DRIVER_PATH", nullptr);
    if (pszGDAL_DRIVER_PATH == nullptr)
        pszGDAL_DRIVER_PATH = CPLGetConfigOption("OGR_DRIVER_PATH", nullptr);

    /* ---------------------------------------------------------------- */
    /*      Allow applications to completely disable this search by     */
    /*      setting the driver path to the special string "disable".    */
    /* ---------------------------------------------------------------- */
    if (pszGDAL_DRIVER_PATH != nullptr && EQUAL(pszGDAL_DRIVER_PATH, "disable"))
    {
        CPLDebug("GDAL", "GDALDriverManager::GetPluginFullPath() disabled.");
        return std::string();
    }

    /* ---------------------------------------------------------------- */
    /*      Where should we look for stuff?                             */
    /* ---------------------------------------------------------------- */
    const CPLStringList aosSearchPaths(
        GDALDriverManager::GetSearchPaths(pszGDAL_DRIVER_PATH));

    /* ---------------------------------------------------------------- */
    /*      Format the ABI version specific subdirectory to look in.    */
    /* ---------------------------------------------------------------- */
    CPLString osABIVersion;

    osABIVersion.Printf("%d.%d", GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR);

    /* ---------------------------------------------------------------- */
    /*      Scan each directory looking for the file of interest.       */
    /* ---------------------------------------------------------------- */
    const int nSearchPaths = aosSearchPaths.size();
    for (int iDir = 0; iDir < nSearchPaths; ++iDir)
    {
        std::string osABISpecificDir =
            CPLFormFilename(aosSearchPaths[iDir], osABIVersion, nullptr);

        VSIStatBufL sStatBuf;
        if (VSIStatL(osABISpecificDir.c_str(), &sStatBuf) != 0)
            osABISpecificDir = aosSearchPaths[iDir];

        const char *pszFullFilename =
            CPLFormFilename(osABISpecificDir.c_str(), pszFilename, nullptr);
        if (VSIStatL(pszFullFilename, &sStatBuf) == 0)
        {
            m_osLastTriedDirectory = std::move(osABISpecificDir);
            return pszFullFilename;
        }
    }

    return std::string();
}

/************************************************************************/
/*                      DeclareDeferredPluginDriver()                   */
/************************************************************************/

/** Declare a driver that will be loaded as a plugin, when actually needed.
 *
 * @param poProxyDriver Plugin driver proxy
 *
 * @since 3.9
 */
void GDALDriverManager::DeclareDeferredPluginDriver(
    GDALPluginDriverProxy *poProxyDriver)
{
    CPLMutexHolderD(&hDMMutex);

    const auto &osPluginFileName = poProxyDriver->GetPluginFileName();
    const char *pszPluginFileName = osPluginFileName.c_str();
    if ((!STARTS_WITH(pszPluginFileName, "gdal_") &&
         !STARTS_WITH(pszPluginFileName, "ogr_")) ||
        !strchr(pszPluginFileName, '.'))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid plugin filename: %s",
                 pszPluginFileName);
        return;
    }

    if (GDALGetDriverByName(poProxyDriver->GetDescription()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "DeclarePluginDriver(): trying to register %s several times",
                 poProxyDriver->GetDescription());
        delete poProxyDriver;
        return;
    }

    const std::string osFullPath = GetPluginFullPath(pszPluginFileName);
    poProxyDriver->SetPluginFullPath(osFullPath);

    if (osFullPath.empty())
    {
        CPLDebug("GDAL",
                 "Proxy driver %s *not* registered due to %s not being found",
                 poProxyDriver->GetDescription(), pszPluginFileName);
        RegisterDriver(poProxyDriver, /*bHidden=*/true);
    }
    else
    {
        //CPLDebugOnly("GDAL", "Registering proxy driver %s",
        //             poProxyDriver->GetDescription());
        RegisterDriver(poProxyDriver);
        m_oSetPluginFileNames.insert(pszPluginFileName);
    }
}

/************************************************************************/
/*                      GDALDestroyDriverManager()                      */
/************************************************************************/

/**
 * \brief Destroy the driver manager.
 *
 * Incidentally unloads all managed drivers.
 *
 * NOTE: This function is not thread safe.  It should not be called while
 * other threads are actively using GDAL.
 */

void CPL_STDCALL GDALDestroyDriverManager(void)

{
    // THREADSAFETY: We would like to lock the mutex here, but it
    // needs to be reacquired within the destructor during driver
    // deregistration.

    // FIXME: Disable following code as it crashed on OSX CI test.
    // std::lock_guard<std::mutex> oLock(oDeleteMutex);

    if (poDM != nullptr)
    {
        delete poDM;
        poDM = nullptr;
    }
}
