/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerZeroCellID, providing access to .RTT files.
 * Author:   Mark Phillips, mbp@geomtech.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam, Mark Phillips
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char T_FILE_CODE[] = "T";

static const TigerFieldInfo rtT_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTInteger, 6, 10, 5, 1, 1},
    {"TZID", 'R', 'N', OFTInteger, 11, 20, 10, 1, 1},
    {"SOURCE", 'L', 'A', OFTString, 21, 30, 10, 1, 1},
    {"FTRP", 'L', 'A', OFTString, 31, 47, 17, 1, 1}};
static const TigerRecordInfo rtT_info = {
    rtT_fields, sizeof(rtT_fields) / sizeof(TigerFieldInfo), 47};

/************************************************************************/
/*                           TigerZeroCellID()                           */
/************************************************************************/

TigerZeroCellID::TigerZeroCellID(OGRTigerDataSource *poDSIn,
                                 CPL_UNUSED const char *pszPrototypeModule)
    : TigerFileBase(&rtT_info, T_FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("ZeroCellID");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    /* -------------------------------------------------------------------- */
    /*      Fields from type T record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTInfo, poFeatureDefn);
}
