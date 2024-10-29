/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSAttrReader class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "sdts_al.h"

/************************************************************************/
/* ==================================================================== */
/*                             SDTSAttrRecord                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSAttrRecord()                           */
/************************************************************************/

SDTSAttrRecord::SDTSAttrRecord() : poWholeRecord(nullptr), poATTR(nullptr)
{
}

/************************************************************************/
/*                          ~SDTSAttrRecord()                           */
/************************************************************************/

SDTSAttrRecord::~SDTSAttrRecord()

{
    if (poWholeRecord != nullptr)
        delete poWholeRecord;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void SDTSAttrRecord::Dump(FILE *fp)

{
    if (poATTR != nullptr)
        poATTR->Dump(fp);
}

/************************************************************************/
/* ==================================================================== */
/*                             SDTSAttrReader                           */
/*                                                                      */
/*      This is the class used to read a primary attribute module.      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSAttrReader()                           */
/************************************************************************/

SDTSAttrReader::SDTSAttrReader() : bIsSecondary(FALSE)
{
}

/************************************************************************/
/*                          ~SDTSAttrReader()                           */
/************************************************************************/

SDTSAttrReader::~SDTSAttrReader()
{
    Close();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void SDTSAttrReader::Close()

{
    ClearIndex();
    oDDFModule.Close();
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open the requested attr file, and prepare to start reading      */
/*      data records.                                                   */
/************************************************************************/

int SDTSAttrReader::Open(const char *pszFilename)

{
    bool bSuccess = CPL_TO_BOOL(oDDFModule.Open(pszFilename));

    if (bSuccess)
        bIsSecondary = (oDDFModule.FindFieldDefn("ATTS") != nullptr);

    return bSuccess;
}

/************************************************************************/
/*                           GetNextRecord()                            */
/************************************************************************/

DDFField *SDTSAttrReader::GetNextRecord(SDTSModId *poModId,
                                        DDFRecord **ppoRecord, int bDuplicate)

{
    /* -------------------------------------------------------------------- */
    /*      Fetch a record.                                                 */
    /* -------------------------------------------------------------------- */
    if (ppoRecord != nullptr)
        *ppoRecord = nullptr;

    if (oDDFModule.GetFP() == nullptr)
        return nullptr;

    DDFRecord *poRecord = oDDFModule.ReadRecord();

    if (poRecord == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Make a copy of the record for persistent use if requested by    */
    /*      the caller.                                                     */
    /* -------------------------------------------------------------------- */
    if (bDuplicate)
        poRecord = poRecord->Clone();

    /* -------------------------------------------------------------------- */
    /*      Find the ATTP field.                                            */
    /* -------------------------------------------------------------------- */
    DDFField *poATTP = poRecord->FindField("ATTP", 0);
    if (poATTP == nullptr)
    {
        poATTP = poRecord->FindField("ATTS", 0);
    }

    if (poATTP == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Update the module ID if required.                               */
    /* -------------------------------------------------------------------- */
    if (poModId != nullptr)
    {
        DDFField *poATPR = poRecord->FindField("ATPR");

        if (poATPR == nullptr)
            poATPR = poRecord->FindField("ATSC");

        if (poATPR != nullptr)
            poModId->Set(poATPR);
    }

    /* -------------------------------------------------------------------- */
    /*      return proper answer.                                           */
    /* -------------------------------------------------------------------- */
    if (ppoRecord != nullptr)
        *ppoRecord = poRecord;

    return poATTP;
}

/************************************************************************/
/*                         GetNextAttrRecord()                          */
/************************************************************************/

SDTSAttrRecord *SDTSAttrReader::GetNextAttrRecord()

{
    SDTSModId oModId;
    DDFRecord *poRawRecord = nullptr;

    DDFField *poATTRField = GetNextRecord(&oModId, &poRawRecord, TRUE);

    if (poATTRField == nullptr)
        return nullptr;

    SDTSAttrRecord *poAttrRecord = new SDTSAttrRecord();

    poAttrRecord->poWholeRecord = poRawRecord;
    poAttrRecord->poATTR = poATTRField;
    memcpy(&(poAttrRecord->oModId), &oModId, sizeof(SDTSModId));

    return poAttrRecord;
}
