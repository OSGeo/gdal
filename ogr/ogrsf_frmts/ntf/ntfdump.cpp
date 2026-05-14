/******************************************************************************
 *
 * Project:  NTF Translator
 * Purpose:  Simple test harness.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ntf.h"
#include "cpl_vsi.h"
#include "cpl_string.h"

static void NTFDump(const char *pszFile, char **papszOptions);
static void NTFCount(const char *pszFile);

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main(int argc, char **argv)

{
    const char *pszMode = "-d";
    char **papszOptions = NULL;

    if (argc == 1)
        printf("Usage: ntfdump [-s n] [-g] [-d] [-c] [-codelist] files\n");

    for (int i = 1; i < argc; i++)
    {
        if (EQUAL(argv[i], "-g"))
            papszOptions = CSLSetNameValue(papszOptions, "FORCE_GENERIC", "ON");
        else if (EQUAL(argv[i], "-s"))
        {
            papszOptions =
                CSLSetNameValue(papszOptions, "DEM_SAMPLE", argv[++i]);
        }
        else if (EQUAL(argv[i], "-codelist"))
        {
            papszOptions = CSLSetNameValue(papszOptions, "CODELIST", "ON");
        }
        else if (argv[i][0] == '-')
            pszMode = argv[i];
        else if (EQUAL(pszMode, "-d"))
            NTFDump(argv[i], papszOptions);
        else if (EQUAL(pszMode, "-c"))
            NTFCount(argv[i]);
    }

    return 0;
}

/************************************************************************/
/*                              NTFCount()                              */
/************************************************************************/

static void NTFCount(const char *pszFile)

{
    FILE *fp = VSIFOpen(pszFile, "r");
    if (fp == NULL)
        return;

    int anCount[100] = {};

    NTFRecord *poRecord = NULL;
    do
    {
        if (poRecord != NULL)
            delete poRecord;

        poRecord = new NTFRecord(fp);
        anCount[poRecord->GetType()]++;
    } while (poRecord->GetType() != 99);

    VSIFClose(fp);

    printf("\nReporting on: %s\n", pszFile);
    for (int i = 0; i < 100; i++)
    {
        if (anCount[i] > 0)
            printf("Found %d records of type %d\n", anCount[i], i);
    }
}

/************************************************************************/
/*                              NTFDump()                               */
/************************************************************************/

static void NTFDump(const char *pszFile, char **papszOptions)

{
    OGRNTFDataSource oDS;

    oDS.SetOptionList(papszOptions);

    if (!oDS.Open(pszFile))
        return;

    OGRFeature *poFeature = NULL;
    while ((poFeature = oDS.GetNextFeature()) != NULL)
    {
        printf("-------------------------------------\n");
        poFeature->DumpReadable(stdout);
        delete poFeature;
    }
}
