/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "vsikerchunk.h"

#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"

/************************************************************************/
/*                    VSIInstallKerchunkFileSystems()                   */
/************************************************************************/

void VSIInstallKerchunkFileSystems()
{
    VSIInstallKerchunkJSONRefFileSystem();
    VSIInstallKerchunkParquetRefFileSystem();
}

/************************************************************************/
/*                    VSIKerchunkFileSystemsCleanCache()                */
/************************************************************************/

void VSIKerchunkFileSystemsCleanCache()
{
    VSIKerchunkParquetRefFileSystemCleanCache();
}

/************************************************************************/
/*                    VSIKerchunkMorphURIToVSIPath()                    */
/************************************************************************/

std::string VSIKerchunkMorphURIToVSIPath(const std::string &osURI,
                                         const std::string &osRootDirname)
{
    static const struct
    {
        const char *pszFSSpecPrefix;
        const char *pszVSIPrefix;
    } substitutions[] = {
        {"s3://", "/vsis3/"},
        {"gs://", "/vsigs/"},
        {"http://", "/vsicurl/http://"},
        {"https://", "/vsicurl/https://"},
    };

    for (const auto &substitution : substitutions)
    {
        if (STARTS_WITH(osURI.c_str(), substitution.pszFSSpecPrefix))
        {
            return std::string(substitution.pszVSIPrefix)
                .append(osURI.c_str() + strlen(substitution.pszFSSpecPrefix));
        }
    }

    if (CPLIsFilenameRelative(osURI.c_str()))
    {
        return CPLFormFilenameSafe(osRootDirname.c_str(), osURI.c_str(),
                                   nullptr);
    }
    else if (VSIIsLocal(osURI.c_str()) && !VSIIsLocal(osRootDirname.c_str()))
    {
        const char *pszVal = CPLGetConfigOption(
            "GDAL_ALLOW_REMOTE_RESOURCE_TO_ACCESS_LOCAL_FILE", nullptr);
        if (pszVal == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Remote resource '%s' tries to access local file '%s'. "
                     "This is disabled by default. Set the "
                     "GDAL_ALLOW_REMOTE_RESOURCE_TO_ACCESS_LOCAL_FILE "
                     "configuration option to YES to allow that.",
                     osRootDirname.c_str(), osURI.c_str());
            return std::string();
        }
        else if (!CPLTestBool(pszVal))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Remote resource '%s' tries to access local file '%s'.",
                     osRootDirname.c_str(), osURI.c_str());
            return std::string();
        }
    }

    return osURI;
}
