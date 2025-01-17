/******************************************************************************
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57FileCollector() function.  This function collects
 *           a list of S-57 data files based on the contents of a directory,
 *           catalog file, or direct reference to an S-57 file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "s57.h"

/************************************************************************/
/*                          S57FileCollector()                          */
/************************************************************************/

char **S57FileCollector(const char *pszDataset)

{
    /* -------------------------------------------------------------------- */
    /*      Stat the dataset, and fail if it isn't a file or directory.     */
    /* -------------------------------------------------------------------- */
    VSIStatBuf sStatBuf;
    if (CPLStat(pszDataset, &sStatBuf))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No S-57 files found, %s\nisn't a directory or a file.\n",
                 pszDataset);

        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      We handle directories by scanning for all S-57 data files in    */
    /*      them, but not for catalogs.                                     */
    /* -------------------------------------------------------------------- */
    char **papszRetList = nullptr;

    if (VSI_ISDIR(sStatBuf.st_mode))
    {
        char **papszDirFiles = VSIReadDir(pszDataset);
        DDFModule oModule;

        for (int iFile = 0;
             papszDirFiles != nullptr && papszDirFiles[iFile] != nullptr;
             iFile++)
        {
            char *pszFullFile = CPLStrdup(
                CPLFormFilenameSafe(pszDataset, papszDirFiles[iFile], nullptr)
                    .c_str());

            // Add to list if it is an S-57 _data_ file.
            if (VSIStat(pszFullFile, &sStatBuf) == 0 &&
                VSI_ISREG(sStatBuf.st_mode) && oModule.Open(pszFullFile, TRUE))
            {
                if (oModule.FindFieldDefn("DSID") != nullptr)
                    papszRetList = CSLAddString(papszRetList, pszFullFile);
            }

            CPLFree(pszFullFile);
        }

        return papszRetList;
    }

    /* -------------------------------------------------------------------- */
    /*      If this is a regular file, but not a catalog just return it.    */
    /*      Note that the caller may still open it and fail.                */
    /* -------------------------------------------------------------------- */
    DDFModule oModule;

    if (!oModule.Open(pszDataset))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The file %s isn't an S-57 data file, or catalog.\n",
                 pszDataset);

        return nullptr;
    }

    DDFRecord *poRecord = oModule.ReadRecord();
    if (poRecord == nullptr)
        return nullptr;

    if (poRecord->FindField("CATD") == nullptr ||
        oModule.FindFieldDefn("CATD")->FindSubfieldDefn("IMPL") == nullptr)
    {
        papszRetList = CSLAddString(papszRetList, pszDataset);
        return papszRetList;
    }

    /* -------------------------------------------------------------------- */
    /*      We presumably have a catalog.  It contains paths to files       */
    /*      that generally lack the ENC_ROOT component.  Try to find the    */
    /*      correct name for the ENC_ROOT directory if available and        */
    /*      build a base path for our purposes.                             */
    /* -------------------------------------------------------------------- */
    char *pszCatDir = CPLStrdup(CPLGetPathSafe(pszDataset).c_str());
    char *pszRootDir = nullptr;

    if (CPLStat(CPLFormFilenameSafe(pszCatDir, "ENC_ROOT", nullptr).c_str(),
                &sStatBuf) == 0 &&
        VSI_ISDIR(sStatBuf.st_mode))
    {
        pszRootDir = CPLStrdup(
            CPLFormFilenameSafe(pszCatDir, "ENC_ROOT", nullptr).c_str());
    }
    else if (CPLStat(
                 CPLFormFilenameSafe(pszCatDir, "enc_root", nullptr).c_str(),
                 &sStatBuf) == 0 &&
             VSI_ISDIR(sStatBuf.st_mode))
    {
        pszRootDir = CPLStrdup(
            CPLFormFilenameSafe(pszCatDir, "enc_root", nullptr).c_str());
    }

    if (pszRootDir)
        CPLDebug("S57", "Found root directory to be %s.", pszRootDir);

    /* -------------------------------------------------------------------- */
    /*      We have a catalog.  Scan it for data files, those with an       */
    /*      IMPL of BIN.  Is there be a better way of testing               */
    /*      whether a file is a data file or another catalog file?          */
    /* -------------------------------------------------------------------- */
    for (; poRecord != nullptr; poRecord = oModule.ReadRecord())
    {
        if (poRecord->FindField("CATD") != nullptr &&
            EQUAL(poRecord->GetStringSubfield("CATD", 0, "IMPL", 0), "BIN"))
        {
            const char *pszFile =
                poRecord->GetStringSubfield("CATD", 0, "FILE", 0);

            // Often there is an extra ENC_ROOT in the path, try finding
            // this file.

            std::string osWholePath =
                CPLFormFilenameSafe(pszCatDir, pszFile, nullptr);
            if (CPLStat(osWholePath.c_str(), &sStatBuf) != 0 &&
                pszRootDir != nullptr)
            {
                osWholePath = CPLFormFilenameSafe(pszRootDir, pszFile, nullptr);
            }

            if (CPLStat(osWholePath.c_str(), &sStatBuf) != 0)
            {
                CPLError(CE_Warning, CPLE_OpenFailed,
                         "Can't find file %s from catalog %s.", pszFile,
                         pszDataset);
                continue;
            }

            papszRetList = CSLAddString(papszRetList, osWholePath.c_str());
            CPLDebug("S57", "Got path %s from CATALOG.", osWholePath.c_str());
        }
    }

    CPLFree(pszCatDir);
    CPLFree(pszRootDir);

    return papszRetList;
}
