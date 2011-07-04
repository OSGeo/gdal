/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Common utility routines
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault
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
/*      Check that the target file extension is consistant with the     */
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
            const char* pszDriverExtension = 
                GDALGetMetadataItem( hDriver, GDAL_DMD_EXTENSION, NULL );   
            if (pszDriverExtension && EQUAL(pszDestExtension, pszDriverExtension))
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
