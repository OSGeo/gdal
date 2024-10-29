/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerTLIDRange, providing access to .RTR files.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char R_FILE_CODE[] = "R";

static const TigerFieldInfo rtR_2002_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTInteger, 6, 10, 5, 1, 1},
    {"CENID", 'L', 'A', OFTString, 11, 15, 5, 1, 1},
    {"TLMAXID", 'R', 'N', OFTInteger, 16, 25, 10, 1, 1},
    {"TLMINID", 'R', 'N', OFTInteger, 26, 35, 10, 1, 1},
    {"TLIGHID", 'R', 'N', OFTInteger, 36, 45, 10, 1, 1},
    {"TZMAXID", 'R', 'N', OFTInteger, 46, 55, 10, 1, 1},
    {"TZMINID", 'R', 'N', OFTInteger, 56, 65, 10, 1, 1},
    {"TZHIGHID", 'R', 'N', OFTInteger, 66, 75, 10, 1, 1},
    {"FILLER", 'L', 'A', OFTString, 76, 76, 1, 1, 1},
};
static const TigerRecordInfo rtR_2002_info = {
    rtR_2002_fields, sizeof(rtR_2002_fields) / sizeof(TigerFieldInfo), 76};

static const TigerFieldInfo rtR_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTString, 6, 10, 5, 1, 1},
    {"STATE", 'L', 'N', OFTInteger, 6, 7, 2, 1, 1},
    {"COUNTY", 'L', 'N', OFTInteger, 8, 10, 3, 1, 1},
    {"CENID", 'L', 'A', OFTString, 11, 15, 5, 1, 1},
    {"MAXID", 'R', 'N', OFTInteger, 16, 25, 10, 1, 1},
    {"MINID", 'R', 'N', OFTInteger, 26, 35, 10, 1, 1},
    {"HIGHID", 'R', 'N', OFTInteger, 36, 45, 10, 1, 1}};

static const TigerRecordInfo rtR_info = {
    rtR_fields, sizeof(rtR_fields) / sizeof(TigerFieldInfo), 46};

/************************************************************************/
/*                           TigerTLIDRange()                           */
/************************************************************************/

TigerTLIDRange::TigerTLIDRange(OGRTigerDataSource *poDSIn,
                               CPL_UNUSED const char *pszPrototypeModule)
    : TigerFileBase(nullptr, R_FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("TLIDRange");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    if (poDS->GetVersion() >= TIGER_2002)
    {
        psRTInfo = &rtR_2002_info;
    }
    else
    {
        psRTInfo = &rtR_info;
    }

    /* -------------------------------------------------------------------- */
    /*      Fields from type R record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTInfo, poFeatureDefn);
}
