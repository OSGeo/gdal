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
    for (int i = 1; i < argc; i++)
    {
        if (EQUAL(argv[i], "--config") && i + 2 < argc)
        {
            CPLSetConfigOption(argv[i + 1], argv[i + 2]);

            i += 2;
        }
        else if (EQUAL(argv[i], "--debug") && i + 1 < argc)
        {
            CPLSetConfigOption("CPL_DEBUG", argv[i + 1]);
            i += 1;
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
/*                      GDALRemoveSQLComments()                         */
/************************************************************************/

std::string GDALRemoveSQLComments(const std::string &osInput)
{
    const CPLStringList aosLines(
        CSLTokenizeStringComplex(osInput.c_str(), "\r\n", FALSE, FALSE));
    std::string osSQL;
    for (const char *pszLine : aosLines)
    {
        char chQuote = 0;
        int i = 0;
        for (; pszLine[i] != '\0'; ++i)
        {
            if (chQuote)
            {
                if (pszLine[i] == chQuote)
                {
                    if (pszLine[i + 1] == chQuote)
                    {
                        i++;
                    }
                    else
                    {
                        chQuote = 0;
                    }
                }
            }
            else if (pszLine[i] == '\'' || pszLine[i] == '"')
            {
                chQuote = pszLine[i];
            }
            else if (pszLine[i] == '-' && pszLine[i + 1] == '-')
            {
                break;
            }
        }
        if (i > 0)
        {
            osSQL.append(pszLine, i);
        }
        osSQL += ' ';
    }
    return osSQL;
}

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

int ArgIsNumeric(const char *pszArg)

{
    return CPLGetValueType(pszArg) != CPL_VALUE_STRING;
}
