/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Common utility routines
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "commonutils.h"

#include <cstdio>
#include <cstring>

#include <string>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal.h"

/* -------------------------------------------------------------------- */
/*                         GetOutputDriversFor()                        */
/* -------------------------------------------------------------------- */

std::vector<std::string> GetOutputDriversFor(const char *pszDestFilename,
                                             int nFlagRasterVector)
{
    return CPLStringList(GDALGetOutputDriversForDatasetName(
        pszDestFilename, nFlagRasterVector, /* bSingleMatch = */ false,
        /* bEmitWarning = */ false));
}

/* -------------------------------------------------------------------- */
/*                      GetOutputDriverForRaster()                      */
/* -------------------------------------------------------------------- */

CPLString GetOutputDriverForRaster(const char *pszDestFilename)
{
    const CPLStringList aosList(GDALGetOutputDriversForDatasetName(
        pszDestFilename, GDAL_OF_RASTER, /* bSingleMatch = */ true,
        /* bEmitWarning = */ true));
    if (!aosList.empty())
    {
        CPLDebug("GDAL", "Using %s driver", aosList[0]);
        return aosList[0];
    }
    return CPLString();
}

/* -------------------------------------------------------------------- */
/*                        EarlySetConfigOptions()                       */
/* -------------------------------------------------------------------- */

void EarlySetConfigOptions(int argc, char **argv)
{
    // Must process some config options before GDALAllRegister() or
    // OGRRegisterAll(), but we can't call GDALGeneralCmdLineProcessor() or
    // OGRGeneralCmdLineProcessor(), because it needs the drivers to be
    // registered for the --format or --formats options.

    // Start with --debug, so that "my_command --config UNKNOWN_CONFIG_OPTION --debug on"
    // detects and warns about a unknown config option.
    for (int i = 1; i < argc; i++)
    {
        if (EQUAL(argv[i], "--config") && i + 1 < argc)
        {
            const char *pszArg = argv[i + 1];
            if (strchr(pszArg, '=') != nullptr)
            {
                char *pszKey = nullptr;
                const char *pszValue = CPLParseNameValue(pszArg, &pszKey);
                if (pszKey && EQUAL(pszKey, "CPL_DEBUG") && pszValue)
                {
                    CPLSetConfigOption(pszKey, pszValue);
                }
                CPLFree(pszKey);
                ++i;
            }
            else
            {
                if (i + 2 >= argc)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "--config option given without a key and value "
                             "argument.");
                    return;
                }

                if (EQUAL(argv[i + 1], "CPL_DEBUG"))
                {
                    CPLSetConfigOption(argv[i + 1], argv[i + 2]);
                }

                i += 2;
            }
        }
        else if (EQUAL(argv[i], "--debug") && i + 1 < argc)
        {
            CPLSetConfigOption("CPL_DEBUG", argv[i + 1]);
            i += 1;
        }
    }
    for (int i = 1; i < argc; i++)
    {
        if (EQUAL(argv[i], "--config") && i + 1 < argc)
        {
            const char *pszArg = argv[i + 1];
            if (strchr(pszArg, '=') != nullptr)
            {
                char *pszKey = nullptr;
                const char *pszValue = CPLParseNameValue(pszArg, &pszKey);
                if (pszKey && !EQUAL(pszKey, "CPL_DEBUG") && pszValue)
                {
                    CPLSetConfigOption(pszKey, pszValue);
                }
                CPLFree(pszKey);
                ++i;
            }
            else
            {
                if (i + 2 >= argc)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "--config option given without a key and value "
                             "argument.");
                    return;
                }

                if (!EQUAL(argv[i + 1], "CPL_DEBUG"))
                {
                    CPLSetConfigOption(argv[i + 1], argv[i + 2]);
                }

                i += 2;
            }
        }
    }
}

/************************************************************************/
/*                          GDALRemoveBOM()                             */
/************************************************************************/

/* Remove potential UTF-8 BOM from data (must be NUL terminated) */
void GDALRemoveBOM(GByte *pabyData)
{
    if (pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF)
    {
        memmove(pabyData, pabyData + 3,
                strlen(reinterpret_cast<char *>(pabyData) + 3) + 1);
    }
}

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

int ArgIsNumeric(const char *pszArg)

{
    return CPLGetValueType(pszArg) != CPL_VALUE_STRING;
}
