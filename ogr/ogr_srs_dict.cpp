/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implement importFromDict() method to read a WKT SRS from a
 *           coordinate system dictionary in a simple text format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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

#include "cpl_port.h"
#include "ogr_spatialref.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"

/************************************************************************/
/*                           importFromDict()                           */
/************************************************************************/

/**
 * Read SRS from WKT dictionary.
 *
 * This method will attempt to find the indicated coordinate system identity
 * in the indicated dictionary file.  If found, the WKT representation is
 * imported and used to initialize this OGRSpatialReference.
 *
 * More complete information on the format of the dictionary files can
 * be found in the epsg.wkt file in the GDAL data tree.  The dictionary
 * files are searched for in the "GDAL" domain using CPLFindFile().  Normally
 * this results in searching /usr/local/share/gdal or somewhere similar.
 *
 * This method is the same as the C function OSRImportFromDict().
 *
 * @param pszDictFile the name of the dictionary file to load.
 *
 * @param pszCode the code to lookup in the dictionary.
 *
 * @return OGRERR_NONE on success, or OGRERR_SRS_UNSUPPORTED if the code isn't
 * found, and OGRERR_SRS_FAILURE if something more dramatic goes wrong.
 */

OGRErr OGRSpatialReference::importFromDict(const char *pszDictFile,
                                           const char *pszCode)

{
    CPLString osWKT(lookupInDict(pszDictFile, pszCode));
    if (osWKT.empty())
        return OGRERR_UNSUPPORTED_SRS;

    OGRErr eErr = importFromWkt(osWKT);
    if (eErr == OGRERR_NONE && strstr(pszDictFile, "esri_") == nullptr)
    {
        morphFromESRI();
    }

    return eErr;
}

/************************************************************************/
/*                          lookupInDict()                              */
/************************************************************************/

CPLString OGRSpatialReference::lookupInDict(const char *pszDictFile,
                                            const char *pszCode)

{
    /* -------------------------------------------------------------------- */
    /*      Find and open file.                                             */
    /* -------------------------------------------------------------------- */
    CPLString osDictFile(pszDictFile);
    const char *pszFilename = CPLFindFile("gdal", pszDictFile);
    if (pszFilename == nullptr)
        return CPLString();

    VSILFILE *fp = VSIFOpenL(pszFilename, "rb");
    if (fp == nullptr)
        return CPLString();

    /* -------------------------------------------------------------------- */
    /*      Process lines.                                                  */
    /* -------------------------------------------------------------------- */
    CPLString osWKT;
    const char *pszLine = nullptr;

    while ((pszLine = CPLReadLineL(fp)) != nullptr)

    {
        if (pszLine[0] == '#')
            continue;

        if (STARTS_WITH_CI(pszLine, "include "))
        {
            osWKT = lookupInDict(pszLine + 8, pszCode);
            if (!osWKT.empty())
                break;
            continue;
        }

        if (strstr(pszLine, ",") == nullptr)
            continue;

        if (EQUALN(pszLine, pszCode, strlen(pszCode)) &&
            pszLine[strlen(pszCode)] == ',')
        {
            osWKT = pszLine + strlen(pszCode) + 1;
            break;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    VSIFCloseL(fp);

    return osWKT;
}

/************************************************************************/
/*                         OSRImportFromDict()                          */
/************************************************************************/

/**
 * Read SRS from WKT dictionary.
 *
 * This method will attempt to find the indicated coordinate system identity
 * in the indicated dictionary file.  If found, the WKT representation is
 * imported and used to initialize this OGRSpatialReference.
 *
 * More complete information on the format of the dictionary files can
 * be found in the epsg.wkt file in the GDAL data tree.  The dictionary
 * files are searched for in the "GDAL" domain using CPLFindFile().  Normally
 * this results in searching /usr/local/share/gdal or somewhere similar.
 *
 * This method is the same as the C++ method
 * OGRSpatialReference::importFromDict().
 *
 * @param hSRS spatial reference system handle.
 *
 * @param pszDictFile the name of the dictionary file to load.
 *
 * @param pszCode the code to lookup in the dictionary.
 *
 * @return OGRERR_NONE on success, or OGRERR_SRS_UNSUPPORTED if the code isn't
 * found, and OGRERR_SRS_FAILURE if something more dramatic goes wrong.
 */

OGRErr OSRImportFromDict(OGRSpatialReferenceH hSRS, const char *pszDictFile,
                         const char *pszCode)

{
    VALIDATE_POINTER1(hSRS, "OSRImportFromDict", OGRERR_FAILURE);

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->importFromDict(
        pszDictFile, pszCode);
}
