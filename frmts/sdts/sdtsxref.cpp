/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTS_XREF class for reading XREF module.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "sdts_al.h"

/************************************************************************/
/*                             SDTS_XREF()                              */
/************************************************************************/

SDTS_XREF::SDTS_XREF()
    : pszSystemName(CPLStrdup("")), pszDatum(CPLStrdup("")), nZone(0)
{
}

/************************************************************************/
/*                             ~SDTS_XREF()                             */
/************************************************************************/

SDTS_XREF::~SDTS_XREF()
{
    CPLFree(pszSystemName);
    CPLFree(pszDatum);
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read the named file to initialize this structure.               */
/************************************************************************/

int SDTS_XREF::Read(const char *pszFilename)

{
    /* -------------------------------------------------------------------- */
    /*      Open the file, and read the header.                             */
    /* -------------------------------------------------------------------- */
    DDFModule oXREFFile;
    if (!oXREFFile.Open(pszFilename))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Read the first record, and verify that this is an XREF record.  */
    /* -------------------------------------------------------------------- */
    DDFRecord *poRecord = oXREFFile.ReadRecord();
    if (poRecord == nullptr)
        return FALSE;

    if (poRecord->GetStringSubfield("XREF", 0, "MODN", 0) == nullptr)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Read fields of interest.                                        */
    /* -------------------------------------------------------------------- */

    CPLFree(pszSystemName);
    pszSystemName =
        CPLStrdup(poRecord->GetStringSubfield("XREF", 0, "RSNM", 0));

    CPLFree(pszDatum);
    pszDatum = CPLStrdup(poRecord->GetStringSubfield("XREF", 0, "HDAT", 0));

    nZone = poRecord->GetIntSubfield("XREF", 0, "ZONE", 0);

    return TRUE;
}
