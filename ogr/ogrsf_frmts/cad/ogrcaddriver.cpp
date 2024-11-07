/*******************************************************************************
 *  Project: OGR CAD Driver
 *  Purpose: Implements driver based on libopencad
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, polimax@mail.ru
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016, NextGIS
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/
#include "ogr_cad.h"
#include "vsilfileio.h"
#include "ogrcaddrivercore.h"

/************************************************************************/
/*                           OGRCADDriverOpen()                         */
/************************************************************************/

static GDALDataset *OGRCADDriverOpen(GDALOpenInfo *poOpenInfo)
{
    long nSubRasterLayer = -1;
    long nSubRasterFID = -1;

    CADFileIO *pFileIO;
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "CAD:"))
    {
        char **papszTokens =
            CSLTokenizeString2(poOpenInfo->pszFilename, ":", 0);
        int nTokens = CSLCount(papszTokens);
        if (nTokens < 4)
        {
            CSLDestroy(papszTokens);
            return nullptr;
        }

        CPLString osFilename;
        for (int i = 1; i < nTokens - 2; ++i)
        {
            if (osFilename.empty())
                osFilename += ":";
            osFilename += papszTokens[i];
        }

        pFileIO = new VSILFileIO(osFilename);
        nSubRasterLayer = atol(papszTokens[nTokens - 2]);
        nSubRasterFID = atol(papszTokens[nTokens - 1]);

        CSLDestroy(papszTokens);
    }
    else
    {
        pFileIO = new VSILFileIO(poOpenInfo->pszFilename);
    }

    if (IdentifyCADFile(pFileIO, false) == FALSE)
    {
        delete pFileIO;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The CAD driver does not support update access to existing"
                 " datasets.\n");
        delete pFileIO;
        return nullptr;
    }

    GDALCADDataset *poDS = new GDALCADDataset();
    if (!poDS->Open(poOpenInfo, pFileIO, nSubRasterLayer, nSubRasterFID))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRCAD()                           */
/************************************************************************/

void RegisterOGRCAD()
{
    if (GDALGetDriverByName(DRIVER_NAME) == nullptr)
    {
        auto poDriver = new GDALDriver();
        OGRCADDriverSetCommonMetadata(poDriver);

        poDriver->pfnOpen = OGRCADDriverOpen;

        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}

CPLString CADRecode(const CPLString &sString, int CADEncoding)
{
    const char *const apszSource[] = {/* 0 UNDEFINED */ "",
                                      /* 1 ASCII */ "US-ASCII",
                                      /* 2 8859_1 */ "ISO-8859-1",
                                      /* 3 8859_2 */ "ISO-8859-2",
                                      /* 4 UNDEFINED */ "",
                                      /* 5 8859_4 */ "ISO-8859-4",
                                      /* 6 8859_5 */ "ISO-8859-5",
                                      /* 7 8859_6 */ "ISO-8859-6",
                                      /* 8 8859_7 */ "ISO-8859-7",
                                      /* 9 8859_8 */ "ISO-8859-8",
                                      /* 10 8859_9 */ "ISO-8859-9",
                                      /* 11 DOS437 */ "CP437",
                                      /* 12 DOS850 */ "CP850",
                                      /* 13 DOS852 */ "CP852",
                                      /* 14 DOS855 */ "CP855",
                                      /* 15 DOS857 */ "CP857",
                                      /* 16 DOS860 */ "CP860",
                                      /* 17 DOS861 */ "CP861",
                                      /* 18 DOS863 */ "CP863",
                                      /* 19 DOS864 */ "CP864",
                                      /* 20 DOS865 */ "CP865",
                                      /* 21 DOS869 */ "CP869",
                                      /* 22 DOS932 */ "CP932",
                                      /* 23 MACINTOSH */ "MACINTOSH",
                                      /* 24 BIG5 */ "BIG5",
                                      /* 25 KSC5601 */ "CP949",
                                      /* 26 JOHAB */ "JOHAB",
                                      /* 27 DOS866 */ "CP866",
                                      /* 28 ANSI_1250 */ "CP1250",
                                      /* 29 ANSI_1251 */ "CP1251",
                                      /* 30 ANSI_1252 */ "CP1252",
                                      /* 31 GB2312 */ "GB2312",
                                      /* 32 ANSI_1253 */ "CP1253",
                                      /* 33 ANSI_1254 */ "CP1254",
                                      /* 34 ANSI_1255 */ "CP1255",
                                      /* 35 ANSI_1256 */ "CP1256",
                                      /* 36 ANSI_1257 */ "CP1257",
                                      /* 37 ANSI_874 */ "CP874",
                                      /* 38 ANSI_932 */ "CP932",
                                      /* 39 ANSI_936 */ "CP936",
                                      /* 40 ANSI_949 */ "CP949",
                                      /* 41 ANSI_950 */ "CP950",
                                      /* 42 ANSI_1361 */ "CP1361",
                                      /* 43 ANSI_1200 */ "UTF-16",
                                      /* 44 ANSI_1258 */ "CP1258"};

    if (CADEncoding > 0 &&
        CADEncoding < static_cast<int>(CPL_ARRAYSIZE(apszSource)) &&
        CADEncoding != 4)
    {
        char *pszRecoded =
            CPLRecode(sString, apszSource[CADEncoding], CPL_ENC_UTF8);
        CPLString soRecoded(pszRecoded);
        CPLFree(pszRecoded);
        return soRecoded;
    }
    CPLError(CE_Failure, CPLE_NotSupported,
             "CADRecode() function does not support provided CADEncoding.");
    return CPLString("");
}
