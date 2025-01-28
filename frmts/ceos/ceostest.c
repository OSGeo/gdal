/******************************************************************************
 *
 * Project:  CEOS Translator
 * Purpose:  Test mainline.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ceosopen.h"

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main(int nArgc, char **papszArgv)

{
    const char *pszFilename;
    FILE *fp;
    CEOSRecord *psRecord;
    int nPosition = 0;

    if (nArgc > 1)
        pszFilename = papszArgv[1];
    else
        pszFilename = "imag_01.dat";

    fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "Can't open %s at all.\n", pszFilename);
        exit(1);
    }

    while (!VSIFEofL(fp) && (psRecord = CEOSReadRecord(fp)) != NULL)
    {
        printf("%9d:%4d:%8x:%d\n", nPosition, psRecord->nRecordNum,
               psRecord->nRecordType, psRecord->nLength);
        CEOSDestroyRecord(psRecord);

        nPosition = (int)VSIFTellL(fp);
    }
    VSIFCloseL(fp);

    exit(0);
}
