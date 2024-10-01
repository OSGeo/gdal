/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTSPointReader and SDTSRawPoint classes.
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
/*                            SDTSRawPoint                              */
/*                                                                      */
/*      This is a simple class for holding the data related with a      */
/*      point feature.                                                  */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            SDTSRawPoint()                            */
/************************************************************************/

SDTSRawPoint::SDTSRawPoint() : dfX(0.0), dfY(0.0), dfZ(0.0)
{
    nAttributes = 0;
}

/************************************************************************/
/*                           ~STDSRawPoint()                            */
/************************************************************************/

SDTSRawPoint::~SDTSRawPoint()
{
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read a record from the passed SDTSPointReader, and assign the   */
/*      values from that record to this point.  This is the bulk of     */
/*      the work in this whole file.                                    */
/************************************************************************/

int SDTSRawPoint::Read(SDTS_IREF *poIREF, DDFRecord *poRecord)

{
    /* ==================================================================== */
    /*      Loop over fields in this record, looking for those we           */
    /*      recognise, and need.                                            */
    /* ==================================================================== */
    for (int iField = 0; iField < poRecord->GetFieldCount(); iField++)
    {
        DDFField *poField = poRecord->GetField(iField);
        if (poField == nullptr)
            return FALSE;
        DDFFieldDefn *poFieldDefn = poField->GetFieldDefn();
        if (poFieldDefn == nullptr)
            return FALSE;

        const char *pszFieldName = poFieldDefn->GetName();

        if (EQUAL(pszFieldName, "PNTS"))
            oModId.Set(poField);

        else if (EQUAL(pszFieldName, "ATID"))
            ApplyATID(poField);

        else if (EQUAL(pszFieldName, "ARID"))
        {
            oAreaId.Set(poField);
        }
        else if (EQUAL(pszFieldName, "SADR"))
        {
            poIREF->GetSADR(poField, 1, &dfX, &dfY, &dfZ);
        }
    }

    return TRUE;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void SDTSRawPoint::Dump(FILE *fp)

{
    fprintf(fp, "SDTSRawPoint %s: ", oModId.GetName());

    if (oAreaId.nRecord != -1)
        fprintf(fp, " AreaId=%s", oAreaId.GetName());

    for (int i = 0; i < nAttributes; i++)
        fprintf(fp, "  ATID[%d]=%s", i, paoATID[i].GetName());

    fprintf(fp, "  Vertex = (%.2f,%.2f,%.2f)\n", dfX, dfY, dfZ);
}

/************************************************************************/
/* ==================================================================== */
/*                             SDTSPointReader                          */
/*                                                                      */
/*      This is the class used to read a point module.                  */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSPointReader()                          */
/************************************************************************/

SDTSPointReader::SDTSPointReader(SDTS_IREF *poIREFIn) : poIREF(poIREFIn)
{
}

/************************************************************************/
/*                             ~SDTSLineReader()                        */
/************************************************************************/

SDTSPointReader::~SDTSPointReader()
{
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void SDTSPointReader::Close()

{
    oDDFModule.Close();
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open the requested line file, and prepare to start reading      */
/*      data records.                                                   */
/************************************************************************/

int SDTSPointReader::Open(const char *pszFilename)

{
    return oDDFModule.Open(pszFilename);
}

/************************************************************************/
/*                            GetNextPoint()                            */
/*                                                                      */
/*      Fetch the next feature as an STDSRawPoint.                      */
/************************************************************************/

SDTSRawPoint *SDTSPointReader::GetNextPoint()

{
    /* -------------------------------------------------------------------- */
    /*      Read a record.                                                  */
    /* -------------------------------------------------------------------- */
    if (oDDFModule.GetFP() == nullptr)
        return nullptr;

    DDFRecord *poRecord = oDDFModule.ReadRecord();

    if (poRecord == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Transform into a point feature.                                 */
    /* -------------------------------------------------------------------- */
    SDTSRawPoint *poRawPoint = new SDTSRawPoint();

    if (poRawPoint->Read(poIREF, poRecord))
    {
        return poRawPoint;
    }

    delete poRawPoint;
    return nullptr;
}
