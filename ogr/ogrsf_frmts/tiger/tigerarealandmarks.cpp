/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerAreaLandmarks, providing access to .RT8 files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char EIGHT_FILE_CODE[] = "8";

static const TigerFieldInfo rt8_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTString, 6, 10, 5, 1, 1},
    {"STATE", 'L', 'N', OFTInteger, 6, 7, 2, 1, 1},
    {"COUNTY", 'L', 'N', OFTInteger, 8, 10, 3, 1, 1},
    {"CENID", 'L', 'A', OFTString, 11, 15, 5, 1, 1},
    {"POLYID", 'R', 'N', OFTInteger, 16, 25, 10, 1, 1},
    {"LAND", 'R', 'N', OFTInteger, 26, 35, 10, 1, 1}};

static const TigerRecordInfo rt8_info = {
    rt8_fields, sizeof(rt8_fields) / sizeof(TigerFieldInfo), 36};

/************************************************************************/
/*                         TigerAreaLandmarks()                         */
/************************************************************************/

TigerAreaLandmarks::TigerAreaLandmarks(
    OGRTigerDataSource *poDSIn, CPL_UNUSED const char *pszPrototypeModule)
    : TigerFileBase(&rt8_info, EIGHT_FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("AreaLandmarks");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    /* -------------------------------------------------------------------- */
    /*      Fields from type 8 record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTInfo, poFeatureDefn);
}
