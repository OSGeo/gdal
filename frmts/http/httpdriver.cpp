/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of an HTTP fetching driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_error_internal.h"
#include "cpl_string.h"
#include "cpl_http.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

static std::string SanitizeDispositionFilename(const std::string &osVal)
{
    std::string osRet(osVal);
    if (!osRet.empty() && osRet[0] == '"')
    {
        const auto nEnd = osRet.find('"', 1);
        if (nEnd != std::string::npos)
            return osRet.substr(1, nEnd - 1);
    }
    return osRet;
}

/************************************************************************/
/*               HTTPFetchContentDispositionFilename()                 */
/************************************************************************/

static std::string HTTPFetchContentDispositionFilename(char **papszHeaders)
{
    char **papszIter = papszHeaders;
    while (papszIter && *papszIter)
    {
        /* For multipart, we have in raw format, but without end-of-line
         * characters */
        if (STARTS_WITH(*papszIter,
                        "Content-Disposition: attachment; filename="))
        {
            return SanitizeDispositionFilename(*papszIter + 42);
        }
        /* For single part, the headers are in KEY=VAL format, but with e-o-l
         * ... */
        else if (STARTS_WITH(*papszIter,
                             "Content-Disposition=attachment; filename="))
        {
            char *pszVal = (*papszIter + 41);
            char *pszEOL = strchr(pszVal, '\r');
            if (pszEOL)
                *pszEOL = 0;
            pszEOL = strchr(pszVal, '\n');
            if (pszEOL)
                *pszEOL = 0;
            return SanitizeDispositionFilename(pszVal);
        }
        papszIter++;
    }
    return std::string();
}

/************************************************************************/
/*                              HTTPOpen()                              */
/************************************************************************/

static GDALDataset *HTTPOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes != 0)
        return nullptr;

    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "http:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "https:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "ftp:"))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Fetch the result.                                               */
    /* -------------------------------------------------------------------- */
    CPLErrorReset();
    CPLHTTPResult *psResult = CPLHTTPFetch(poOpenInfo->pszFilename, nullptr);

    /* -------------------------------------------------------------------- */
    /*      Try to handle errors.                                           */
    /* -------------------------------------------------------------------- */
    if (psResult == nullptr || psResult->nDataLen == 0 ||
        CPLGetLastErrorNo() != 0)
    {
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a memory file from the result.                           */
    /* -------------------------------------------------------------------- */
    std::string osFilename =
        HTTPFetchContentDispositionFilename(psResult->papszHeaders);
    if (osFilename.empty())
    {
        osFilename = CPLGetFilename(poOpenInfo->pszFilename);
        /* If we have special characters, let's default to a fixed name */
        if (strchr(osFilename.c_str(), '?') || strchr(osFilename.c_str(), '&'))
            osFilename = "file.dat";
    }

    // If changing the _gdal_http_ marker, change jpgdataset.cpp that tests for it
    const CPLString osResultFilename = VSIMemGenerateHiddenFilename(
        std::string("_gdal_http_").append(osFilename).c_str());

    VSILFILE *fp = VSIFileFromMemBuffer(osResultFilename, psResult->pabyData,
                                        psResult->nDataLen, TRUE);

    if (fp == nullptr)
        return nullptr;

    VSIFCloseL(fp);

    /* -------------------------------------------------------------------- */
    /*      Steal the memory buffer from HTTP result before destroying      */
    /*      it.                                                             */
    /* -------------------------------------------------------------------- */
    psResult->pabyData = nullptr;
    psResult->nDataLen = 0;
    psResult->nDataAlloc = 0;

    CPLHTTPDestroyResult(psResult);

    CPLStringList aosOpenOptions;
    for (const char *pszStr :
         cpl::Iterate(const_cast<CSLConstList>(poOpenInfo->papszOpenOptions)))
    {
        if (STARTS_WITH_CI(pszStr, "NATIVE_DATA="))
        {
            // Avoid warning with "ogr2ogr out http://example.com/in.gpkg"
            aosOpenOptions.push_back(std::string("@").append(pszStr).c_str());
        }
        else
        {
            aosOpenOptions.push_back(pszStr);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try opening this result as a gdaldataset.                       */
    /* -------------------------------------------------------------------- */
    /* suppress errors as not all drivers support /vsimem */

    GDALDataset *poDS;
    CPLErrorAccumulator oErrorAccumulator;
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        auto oAccumulator = oErrorAccumulator.InstallForCurrentScope();
        CPL_IGNORE_RET_VAL(oAccumulator);
        poDS = GDALDataset::Open(
            osResultFilename, poOpenInfo->nOpenFlags & ~GDAL_OF_SHARED,
            poOpenInfo->papszAllowedDrivers, aosOpenOptions.List(), nullptr);
    }

    // Re-emit silenced errors if open was successful
    if (poDS)
    {
        oErrorAccumulator.ReplayErrors();
    }

    // The JP2OpenJPEG driver may need to reopen the file, hence this special
    // behavior
    if (poDS != nullptr && poDS->GetDriver() != nullptr &&
        EQUAL(poDS->GetDriver()->GetDescription(), "JP2OpenJPEG"))
    {
        poDS->MarkSuppressOnClose();
        return poDS;
    }

    /* -------------------------------------------------------------------- */
    /*      If opening it in memory didn't work, perhaps we need to         */
    /*      write to a temp file on disk?                                   */
    /* -------------------------------------------------------------------- */
    if (poDS == nullptr)
    {
        CPLString osTempFilename;

#ifdef _WIN32
        const std::string osPath =
            CPLGetPathSafe(CPLGenerateTempFilenameSafe(NULL).c_str());
#else
        const std::string osPath = "/tmp";
#endif
        osTempFilename = CPLFormFilenameSafe(
            osPath.c_str(), CPLGetFilename(osResultFilename), nullptr);
        if (CPLCopyFile(osTempFilename, osResultFilename) != 0)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to create temporary file:%s",
                     osTempFilename.c_str());
        }
        else
        {
            poDS = GDALDataset::Open(osTempFilename,
                                     poOpenInfo->nOpenFlags & ~GDAL_OF_SHARED,
                                     poOpenInfo->papszAllowedDrivers,
                                     aosOpenOptions.List(), nullptr);
            if (VSIUnlink(osTempFilename) != 0 && poDS != nullptr)
                poDS->MarkSuppressOnClose(); /* VSIUnlink() may not work on
                                                windows */
            if (poDS && strcmp(poDS->GetDescription(), osTempFilename) == 0)
                poDS->SetDescription(poOpenInfo->pszFilename);
        }
    }
    else if (strcmp(poDS->GetDescription(), osResultFilename) == 0)
        poDS->SetDescription(poOpenInfo->pszFilename);

    /* -------------------------------------------------------------------- */
    /*      Release our hold on the vsi memory file, though if it is        */
    /*      held open by a dataset it will continue to exist till that      */
    /*      lets it go.                                                     */
    /* -------------------------------------------------------------------- */
    VSIUnlink(osResultFilename);

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_HTTP()                          */
/************************************************************************/

void GDALRegister_HTTP()

{
    if (GDALGetDriverByName("HTTP") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("HTTP");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "HTTP Fetching Wrapper");

    poDriver->pfnOpen = HTTPOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
