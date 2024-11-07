/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerKeyFeatures, providing access to .RT9 files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char NINE_FILE_CODE[] = "9";

static const TigerFieldInfo rt9_fields[] = {
    // fieldname    fmt  type  OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTString, 6, 10, 5, 1, 1},
    {"STATE", 'L', 'N', OFTInteger, 6, 7, 2, 1, 1},
    {"COUNTY", 'L', 'N', OFTInteger, 8, 10, 3, 1, 1},
    {"CENID", 'L', 'A', OFTString, 11, 15, 5, 1, 1},
    {"POLYID", 'R', 'N', OFTInteger, 16, 25, 10, 1, 1},
    {"SOURCE", 'L', 'A', OFTString, 26, 26, 1, 1, 1},
    {"CFCC", 'L', 'A', OFTString, 27, 29, 3, 1, 1},
    {"KGLNAME", 'L', 'A', OFTString, 30, 59, 30, 1, 1},
    {"KGLADD", 'R', 'A', OFTString, 60, 70, 11, 1, 1},
    {"KGLZIP", 'L', 'N', OFTInteger, 71, 75, 5, 1, 1},
    {"KGLZIP4", 'L', 'N', OFTInteger, 76, 79, 4, 1, 1},
    {"FEAT", 'R', 'N', OFTInteger, 80, 87, 8, 1, 1}};

static const TigerRecordInfo rt9_info = {
    rt9_fields, sizeof(rt9_fields) / sizeof(TigerFieldInfo), 88};

/************************************************************************/
/*                         TigerKeyFeatures()                         */
/************************************************************************/

TigerKeyFeatures::TigerKeyFeatures(OGRTigerDataSource *poDSIn,
                                   CPL_UNUSED const char *pszPrototypeModule)
    : TigerFileBase(&rt9_info, NINE_FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("KeyFeatures");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    /* -------------------------------------------------------------------- */
    /*      Fields from type 9 record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTInfo, poFeatureDefn);
}
