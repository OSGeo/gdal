/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Return the path of the "gdal" binary
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_spawn.h"
#include "cpl_vsi_virtual.h"
#include "gdal.h"
#include "gdalgetgdalpath.h"

#include <cassert>

/************************************************************************/
/*                         GDALGetGDALPath()                            */
/************************************************************************/

/** Return the path of the "gdal" binary, or an empty string if it cannot be
 * found.
 *
 * The GDAL_PATH configuration option may be set to point to the directory where
 * the GDAL binary is located.
 */
std::string GDALGetGDALPath()
{
    const char *pszGDAL_PATH = CPLGetConfigOption("GDAL_PATH", nullptr);
    if (pszGDAL_PATH)
    {
        VSIStatBufL sStat;
        for (const char *pszProgramName : {"gdal"
#ifdef _WIN32
                                           ,
                                           "gdal.exe"
#endif
             })
        {
            std::string osPath =
                CPLFormFilenameSafe(pszGDAL_PATH, pszProgramName, nullptr);
            if (VSIStatL(osPath.c_str(), &sStat) == 0)
                return osPath;
        }
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No 'gdal' binary can be found in '%s'", pszGDAL_PATH);
        return std::string();
    }

    constexpr int MAXPATH_SIZE = 4096;
    std::string osPath;
    osPath.resize(MAXPATH_SIZE);
    if (CPLGetExecPath(osPath.data(), MAXPATH_SIZE))
    {
        osPath.resize(strlen(osPath.c_str()));
        if (!cpl::ends_with(osPath, "/gdal") &&
            !cpl::ends_with(osPath, "\\gdal") &&
            !cpl::ends_with(osPath, "\\gdal.exe"))
        {
            osPath.clear();
#if defined(__linux) && !defined(STATIC_BUILD)
            const CPLStringList aosLines(CSLLoad("/proc/self/maps"));
            for (const char *pszLine : aosLines)
            {
                const char *pszLibName = strstr(pszLine, "/libgdal.so.");
                while (pszLibName &&
                       static_cast<size_t>(pszLibName - pszLine) > 1 &&
                       pszLibName[-1] != ' ')
                {
                    --pszLibName;
                }
                if (pszLibName &&
                    static_cast<size_t>(pszLibName - pszLine) > 1 &&
                    pszLibName[0] == '/')
                {
                    const std::string osPathOfGDALLib =
                        CPLGetDirnameSafe(pszLibName);
                    std::string osBinFilename = CPLFormFilenameSafe(
                        CPLGetDirnameSafe(osPathOfGDALLib.c_str()).c_str(),
                        "bin/gdal", nullptr);
                    VSIStatBufL sStat;
                    if (VSIStatL(osBinFilename.c_str(), &sStat) == 0)
                    {
                        // Case if pszLibName=/usr/lib/libgdal.so.xxx
                        osPath = std::move(osBinFilename);
                    }
                    else
                    {
                        osBinFilename = CPLFormFilenameSafe(
                            CPLGetDirnameSafe(
                                CPLGetDirnameSafe(osPathOfGDALLib.c_str())
                                    .c_str())
                                .c_str(),
                            "bin/gdal", nullptr);
                        if (VSIStatL(osBinFilename.c_str(), &sStat) == 0)
                        {
                            // Case if pszLibName=/usr/lib/yyyyy/libgdal.so.xxx
                            osPath = std::move(osBinFilename);
                        }
                        else
                        {
                            osBinFilename = CPLFormFilenameSafe(
                                osPathOfGDALLib.c_str(), "apps/gdal", nullptr);
                            if (VSIStatL(osBinFilename.c_str(), &sStat) == 0)
                            {
                                // Case if pszLibName=/path/to/build_dir/libgdal.so.xxx
                                osPath = std::move(osBinFilename);
                            }
                        }
                    }
                }
            }
#endif
        }
        if (!osPath.empty())
        {
            CPLDebug("GDAL", "gdal binary found at '%s'", osPath.c_str());
        }
    }
    if (osPath.empty())
    {
        // Try to locate from the path
#ifdef _WIN32
        osPath = "gdal.exe";
#else
        osPath = "gdal";
#endif
    }

    const char *const apszArgv[] = {osPath.c_str(), "--version", nullptr};
    const std::string osTmpFilenameVersion =
        VSIMemGenerateHiddenFilename(nullptr);
    auto fpOut = std::unique_ptr<VSIVirtualHandle>(
        VSIFOpenL(osTmpFilenameVersion.c_str(), "wb+"));
    VSIUnlink(osTmpFilenameVersion.c_str());
    CPLAssert(fpOut);
    CPLSpawn(apszArgv, nullptr, fpOut.get(), /* bDisplayErr = */ false);
    const auto nPos = fpOut->Tell();
    std::string osVersion;
    osVersion.resize(128);
    if (nPos > 0 && nPos < osVersion.size())
    {
        osVersion.resize(static_cast<size_t>(nPos));
        fpOut->Seek(0, SEEK_SET);
        fpOut->Read(osVersion.data(), 1, osVersion.size());
        for (const char ch : {'\n', '\r'})
        {
            if (!osVersion.empty() && osVersion.back() == ch)
            {
                osVersion.pop_back();
            }
        }
        if (osVersion == GDALVersionInfo(""))
        {
            return osPath;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'%s --version' returned '%s', whereas '%s' "
                     "expected. Make sure the gdal binary corresponding "
                     "to the version of the libgdal of the current "
                     "process is in the PATH environment variable",
                     osPath.c_str(), osVersion.c_str(), GDALVersionInfo(""));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not find 'gdal' binary. Make sure it is in the "
                 "PATH environment variable.");
    }
    return std::string();
}
