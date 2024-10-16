/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPoint class.
 * Author:   Mark Phillips, mbp@geomtech.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Mark Phillips
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

#include <cinttypes>

/************************************************************************/
/*                             TigerPoint()                             */
/************************************************************************/
TigerPoint::TigerPoint(const TigerRecordInfo *psRTInfoIn,
                       const char *m_pszFileCodeIn)
    : TigerFileBase(psRTInfoIn, m_pszFileCodeIn)
{
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/
OGRFeature *TigerPoint::GetFeature(int nRecordId, int nX0, int nX1, int nY0,
                                   int nY1)
{
    char achRecord[OGR_TIGER_RECBUF_LEN];

    if (nRecordId < 0 || nRecordId >= nFeatures)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Request for out-of-range feature %d of %sP", nRecordId,
                 pszModule);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the raw record data from the file.                         */
    /* -------------------------------------------------------------------- */

    if (fpPrimary == nullptr)
        return nullptr;

    {
        const auto nOffset = static_cast<uint64_t>(nRecordId) * nRecordLength;
        if (VSIFSeekL(fpPrimary, nOffset, SEEK_SET) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed to seek to %" PRIu64 " of %sP", nOffset,
                     pszModule);
            return nullptr;
        }
    }

    // Overflow cannot happen since psRTInfo->nRecordLength is unsigned
    // char and sizeof(achRecord) == OGR_TIGER_RECBUF_LEN > 255
    if (VSIFReadL(achRecord, psRTInfo->nRecordLength, 1, fpPrimary) != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Failed to read record %d of %sP",
                 nRecordId, pszModule);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Set fields.                                                     */
    /* -------------------------------------------------------------------- */

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);

    SetFields(psRTInfo, poFeature, achRecord);

    /* -------------------------------------------------------------------- */
    /*      Set geometry                                                    */
    /* -------------------------------------------------------------------- */

    const double dfX = atoi(GetField(achRecord, nX0, nX1)) / 1000000.0;
    const double dfY = atoi(GetField(achRecord, nY0, nY1)) / 1000000.0;

    if (dfX != 0.0 || dfY != 0.0)
    {
        poFeature->SetGeometryDirectly(new OGRPoint(dfX, dfY));
    }

    return poFeature;
}
