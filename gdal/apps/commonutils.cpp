/******************************************************************************
 * $Id$
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
#include "cpl_string.h"
#include "gdal.h"

CPL_CVSID("$Id$");

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

    char* pszDestExtension = CPLStrdup(CPLGetExtension(pszDestFilename));
    if (pszDestExtension[0] != '\0')
    {
        int nDriverCount = GDALGetDriverCount();
        CPLString osConflictingDriverList;
        for(int i=0;i<nDriverCount;i++)
        {
            GDALDriverH hDriver = GDALGetDriver(i);
            const char* pszDriverExtensions = 
                GDALGetMetadataItem( hDriver, GDAL_DMD_EXTENSIONS, NULL );
            if( pszDriverExtensions )
            {
                char** papszTokens = CSLTokenizeString( pszDriverExtensions );
                for(int j=0; papszTokens[j]; j++)
                {
                    const char* pszDriverExtension = papszTokens[j];
                    if (EQUAL(pszDestExtension, pszDriverExtension))
                    {
                        if (GDALGetDriverByName(pszDriverName) != hDriver)
                        {
                            if (osConflictingDriverList.size())
                                osConflictingDriverList += ", ";
                            osConflictingDriverList += GDALGetDriverShortName(hDriver);
                        }
                        else
                        {
                            /* If the request driver allows the used extension, then */
                            /* just stop iterating now */
                            osConflictingDriverList = "";
                            break;
                        }
                    }
                }
                CSLDestroy(papszTokens);
            }
        }
        if (osConflictingDriverList.size())
        {
            fprintf(stderr,
                    "Warning: The target file has a '%s' extension, which is normally used by the %s driver%s,\n"
                    "but the requested output driver is %s. Is it really what you want ?\n",
                    pszDestExtension,
                    osConflictingDriverList.c_str(),
                    strchr(osConflictingDriverList.c_str(), ',') ? "s" : "",
                    pszDriverName);
        }
    }

    CPLFree(pszDestExtension);
}

/* -------------------------------------------------------------------- */
/*                        EarlySetConfigOptions()                       */
/* -------------------------------------------------------------------- */

void EarlySetConfigOptions( int argc, char ** argv )
{
    /* Must process some config options before GDALAllRegister() or OGRRegisterAll(), */
    /* but we can't call GDALGeneralCmdLineProcessor() or OGRGeneralCmdLineProcessor(), */
    /* because it needs the drivers to be registered for the --format or --formats options */
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
