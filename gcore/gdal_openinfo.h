/******************************************************************************
 *
 * Name:     gdal_openinfo.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALOpenInfo class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALOPENINFO_H_INCLUDED
#define GDALOPENINFO_H_INCLUDED

#include "cpl_port.h"

#include "gdal.h"

struct VSIVirtualHandle;

/* ******************************************************************** */
/*                             GDALOpenInfo                             */
/* ******************************************************************** */

/** Class for dataset open functions. */
class CPL_DLL GDALOpenInfo
{
    bool bHasGotSiblingFiles = false;
    char **papszSiblingFiles = nullptr;
    int nHeaderBytesTried = 0;

    void Init(const char *const *papszSiblingFilesIn,
              std::unique_ptr<VSIVirtualHandle> poFile);

  public:
    GDALOpenInfo(const char *pszFile, int nOpenFlagsIn,
                 const char *const *papszSiblingFilesIn = nullptr);
    GDALOpenInfo(const char *pszFile, int nOpenFlagsIn,
                 std::unique_ptr<VSIVirtualHandle> poFile);
    ~GDALOpenInfo();

    /** Filename */
    char *pszFilename = nullptr;

    /** Result of CPLGetExtension(pszFilename); */
    std::string osExtension{};

    /** Open options */
    char **papszOpenOptions = nullptr;

    /** Access flag */
    GDALAccess eAccess = GA_ReadOnly;
    /** Open flags */
    int nOpenFlags = 0;

    /** Whether stat()'ing the file was successful */
    bool bStatOK = false;
    /** Whether the file is a directory */
    bool bIsDirectory = false;

    /** Pointer to the file */
    VSILFILE *fpL = nullptr;

    /** Number of bytes in pabyHeader */
    int nHeaderBytes = 0;
    /** Buffer with first bytes of the file */
    GByte *pabyHeader = nullptr;

    /** Allowed drivers (NULL for all) */
    const char *const *papszAllowedDrivers = nullptr;

    int TryToIngest(int nBytes);
    char **GetSiblingFiles();
    char **StealSiblingFiles();
    bool AreSiblingFilesLoaded() const;

    bool IsSingleAllowedDriver(const char *pszDriverName) const;

    /** Return whether the extension of the file is equal to pszExt, using
     * case-insensitive comparison.
     * @since 3.11 */
    inline bool IsExtensionEqualToCI(const char *pszExt) const
    {
        return EQUAL(osExtension.c_str(), pszExt);
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALOpenInfo)
};

#endif
