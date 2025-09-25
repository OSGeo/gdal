/******************************************************************************
 *
 * Name:     gdal_drivermanager.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALDriverManager class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALDRIVERMANAGER_H_INCLUDED
#define GDALDRIVERMANAGER_H_INCLUDED

#include "cpl_port.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_majorobject.h"

#include <memory>
#include <set>
#include <vector>

class GDALDriver;
class GDALPluginDriverProxy;

/* ******************************************************************** */
/*                          GDALDriverManager                           */
/* ******************************************************************** */

/**
 * Class for managing the registration of file format drivers.
 *
 * Use GetGDALDriverManager() to fetch the global singleton instance of
 * this class.
 */

class CPL_DLL GDALDriverManager final : public GDALMajorObject
{
    int nDrivers = 0;
    GDALDriver **papoDrivers = nullptr;
    std::map<CPLString, GDALDriver *> oMapNameToDrivers{};
    std::string m_osPluginPath{};
    std::string m_osDriversIniPath{};
    mutable std::string m_osLastTriedDirectory{};
    std::set<std::string> m_oSetPluginFileNames{};
    bool m_bInDeferredDriverLoading = false;
    std::map<std::string, std::unique_ptr<GDALDriver>> m_oMapRealDrivers{};
    std::vector<std::unique_ptr<GDALDriver>> m_aoHiddenDrivers{};

    GDALDriver *GetDriver_unlocked(int iDriver)
    {
        return (iDriver >= 0 && iDriver < nDrivers) ? papoDrivers[iDriver]
                                                    : nullptr;
    }

    GDALDriver *GetDriverByName_unlocked(const char *pszName) const;

    static void CleanupPythonDrivers();

    std::string GetPluginFullPath(const char *pszFilename) const;

    int RegisterDriver(GDALDriver *, bool bHidden);

    CPL_DISALLOW_COPY_ASSIGN(GDALDriverManager)

  protected:
    friend class GDALPluginDriverProxy;
    friend GDALDatasetH CPL_STDCALL
    GDALOpenEx(const char *pszFilename, unsigned int nOpenFlags,
               const char *const *papszAllowedDrivers,
               const char *const *papszOpenOptions,
               const char *const *papszSiblingFiles);

    //! @cond Doxygen_Suppress
    static char **GetSearchPaths(const char *pszGDAL_DRIVER_PATH);
    //! @endcond

  public:
    GDALDriverManager();
    ~GDALDriverManager() override;

    int GetDriverCount(void) const;
    GDALDriver *GetDriver(int);
    GDALDriver *GetDriverByName(const char *);

    int RegisterDriver(GDALDriver *);
    void DeregisterDriver(GDALDriver *);

    // AutoLoadDrivers is a no-op if compiled with GDAL_NO_AUTOLOAD defined.
    void AutoLoadDrivers();
    void AutoSkipDrivers();
    void ReorderDrivers();
    static CPLErr LoadPlugin(const char *name);

    static void AutoLoadPythonDrivers();

    void DeclareDeferredPluginDriver(GDALPluginDriverProxy *poProxyDriver);

    //! @cond Doxygen_Suppress
    int GetDriverCount(bool bIncludeHidden) const;
    GDALDriver *GetDriver(int iDriver, bool bIncludeHidden);
    bool IsKnownDriver(const char *pszDriverName) const;
    GDALDriver *GetHiddenDriverByName(const char *pszName);
    //! @endcond
};

CPL_C_START
GDALDriverManager CPL_DLL *GetGDALDriverManager(void);
CPL_C_END

#endif
