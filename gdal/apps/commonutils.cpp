/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Common utility routines
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$")

/* -------------------------------------------------------------------- */
/*                   DoesDriverHandleExtension()                        */
/* -------------------------------------------------------------------- */

static bool DoesDriverHandleExtension( GDALDriverH hDriver, const char* pszExt )
{
    bool bRet = false;
    const char* pszDriverExtensions =
        GDALGetMetadataItem( hDriver, GDAL_DMD_EXTENSIONS, NULL );
    if( pszDriverExtensions )
    {
        char** papszTokens = CSLTokenizeString( pszDriverExtensions );
        for( int j = 0; papszTokens[j]; j++ )
        {
            if( EQUAL(pszExt, papszTokens[j]) )
            {
                bRet = true;
                break;
            }
        }
        CSLDestroy(papszTokens);
    }
    return bRet;
}

/* -------------------------------------------------------------------- */
/*                      CheckExtensionConsistency()                     */
/*                                                                      */
/*      Check that the target file extension is consistent with the     */
/*      requested driver. Actually, we only warn in cases where the     */
/*      inconsistency is blatant (use of an extension declared by one   */
/*      or several drivers, and not by the selected one)                */
/* -------------------------------------------------------------------- */

void CheckExtensionConsistency(const char* pszDestFilename,
                               const char* pszDriverName)
{

    CPLString osExt = CPLGetExtension(pszDestFilename);
    if( !osExt.empty() )
    {
        GDALDriverH hThisDrv = GDALGetDriverByName(pszDriverName);
        if( hThisDrv != NULL && DoesDriverHandleExtension(hThisDrv, osExt) )
            return;

        const int nDriverCount = GDALGetDriverCount();
        CPLString osConflictingDriverList;
        for( int i = 0; i < nDriverCount; i++ )
        {
            GDALDriverH hDriver = GDALGetDriver(i);
            if( hDriver != hThisDrv &&
                DoesDriverHandleExtension(hDriver, osExt) )
            {
                if (!osConflictingDriverList.empty() )
                    osConflictingDriverList += ", ";
                osConflictingDriverList += GDALGetDriverShortName(hDriver);
            }
        }
        if (!osConflictingDriverList.empty() )
        {
            fprintf(
                stderr,
                "Warning: The target file has a '%s' extension, "
                "which is normally used by the %s driver%s, "
                "but the requested output driver is %s. "
                "Is it really what you want?\n",
                osExt.c_str(),
                osConflictingDriverList.c_str(),
                strchr(osConflictingDriverList.c_str(), ',') ? "s" : "",
                pszDriverName);
        }
    }
}

/* -------------------------------------------------------------------- */
/*                        EarlySetConfigOptions()                       */
/* -------------------------------------------------------------------- */

void EarlySetConfigOptions( int argc, char ** argv )
{
    // Must process some config options before GDALAllRegister() or
    // OGRRegisterAll(), but we can't call GDALGeneralCmdLineProcessor() or
    // OGRGeneralCmdLineProcessor(), because it needs the drivers to be
    // registered for the --format or --formats options.
    for( int i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"--config") && i + 2 < argc &&
            (EQUAL(argv[i + 1], "GDAL_SKIP") ||
             EQUAL(argv[i + 1], "GDAL_DRIVER_PATH") ||
             EQUAL(argv[i + 1], "OGR_SKIP") ||
             EQUAL(argv[i + 1], "OGR_DRIVER_PATH")) )
        {
            CPLSetConfigOption( argv[i+1], argv[i+2] );

            i += 2;
        }
        else if( EQUAL(argv[i],"--debug") && i + 1 < argc )
        {
            CPLSetConfigOption( "CPL_DEBUG", argv[i+1] );
            i += 1;
        }
    }
}
