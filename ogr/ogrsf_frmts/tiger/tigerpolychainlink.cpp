/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPolyChainLink, providing access to .RTI files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char I_FILE_CODE[] = "I";

static const TigerFieldInfo rtI_2002_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTInteger, 6, 10, 5, 1, 1},
    {"TLID", 'R', 'N', OFTInteger, 11, 20, 10, 1, 1},
    {"TZIDS", 'R', 'N', OFTInteger, 21, 30, 10, 1, 1},
    {"TZIDE", 'R', 'N', OFTInteger, 31, 40, 10, 1, 1},
    {"CENIDL", 'L', 'A', OFTString, 41, 45, 5, 1, 1},
    {"POLYIDL", 'R', 'N', OFTInteger, 46, 55, 10, 1, 1},
    {"CENIDR", 'L', 'A', OFTString, 56, 60, 5, 1, 1},
    {"POLYIDR", 'R', 'N', OFTInteger, 61, 70, 10, 1, 1},
    {"SOURCE", 'L', 'A', OFTString, 71, 80, 10, 1, 1},
    {"FTSEG", 'L', 'A', OFTString, 81, 97, 17, 1, 1},
    {"RS_I1", 'L', 'A', OFTString, 98, 107, 10, 1, 1},
    {"RS_I2", 'L', 'A', OFTString, 108, 117, 10, 1, 1},
    {"RS_I3", 'L', 'A', OFTString, 118, 127, 10, 1, 1},
};
static const TigerRecordInfo rtI_2002_info = {
    rtI_2002_fields, sizeof(rtI_2002_fields) / sizeof(TigerFieldInfo), 127};

static const TigerFieldInfo rtI_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"TLID", 'R', 'N', OFTInteger, 6, 15, 10, 1, 1},
    {"FILE", 'L', 'N', OFTString, 16, 20, 5, 1, 1},
    {"STATE", 'L', 'N', OFTInteger, 16, 17, 2, 1, 1},
    {"COUNTY", 'L', 'N', OFTInteger, 18, 20, 3, 1, 1},
    {"RTLINK", 'L', 'A', OFTString, 21, 21, 1, 1, 1},
    {"CENIDL", 'L', 'A', OFTString, 22, 26, 5, 1, 1},
    {"POLYIDL", 'R', 'N', OFTInteger, 27, 36, 10, 1, 1},
    {"CENIDR", 'L', 'A', OFTString, 37, 41, 5, 1, 1},
    {"POLYIDR", 'R', 'N', OFTInteger, 42, 51, 10, 1, 1}};
static const TigerRecordInfo rtI_info = {
    rtI_fields, sizeof(rtI_fields) / sizeof(TigerFieldInfo), 52};

/************************************************************************/
/*                         TigerPolyChainLink()                         */
/************************************************************************/

TigerPolyChainLink::TigerPolyChainLink(
    OGRTigerDataSource *poDSIn, CPL_UNUSED const char *pszPrototypeModule)
    : TigerFileBase(nullptr, I_FILE_CODE)
{
    OGRFieldDefn oField("", OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("PolyChainLink");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    if (poDS->GetVersion() >= TIGER_2002)
    {
        psRTInfo = &rtI_2002_info;
    }
    else
    {
        psRTInfo = &rtI_info;
    }

    /* -------------------------------------------------------------------- */
    /*      Fields from type I record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTInfo, poFeatureDefn);
}
