/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Common utility routines
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

std::vector<CPLString> GetOutputDriversFor(const char *pszDestFilename,
                                           int nFlagRasterVector)
{
    std::vector<CPLString> aoDriverList;
    char **papszList = GDALGetOutputDriversForDatasetName(
        pszDestFilename, nFlagRasterVector, /* bSingleMatch = */ false,
        /* bEmitWarning = */ false);
    for (char **papszIter = papszList; papszIter && *papszIter; ++papszIter)
        aoDriverList.push_back(*papszIter);
    CSLDestroy(papszList);
    return aoDriverList;
}

/* -------------------------------------------------------------------- */
/*                      GetOutputDriverForRaster()                      */
/* -------------------------------------------------------------------- */

CPLString GetOutputDriverForRaster(const char *pszDestFilename)
{
    char **papszList = GDALGetOutputDriversForDatasetName(
        pszDestFilename, GDAL_OF_RASTER, /* bSingleMatch = */ true,
        /* bEmitWarning = */ true);
    if (papszList)
    {
        CPLDebug("GDAL", "Using %s driver", papszList[0]);
        const std::string osRet = papszList[0];
        CSLDestroy(papszList);
        return osRet;
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
    char **papszLines =
        CSLTokenizeStringComplex(osInput.c_str(), "\r\n", FALSE, FALSE);
    std::string osSQL;
    for (char **papszIter = papszLines; papszIter && *papszIter; ++papszIter)
    {
        const char *pszLine = *papszIter;
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
    CSLDestroy(papszLines);
    return osSQL;
}

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

int ArgIsNumeric(const char *pszArg)

{
    return CPLGetValueType(pszArg) != CPL_VALUE_STRING;
}
