/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerIDHistory, providing access to .RTH files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char FILE_CODE[] = "H";

static const TigerFieldInfo rtH_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTString, 6, 10, 5, 1, 1},
    {"STATE", 'L', 'N', OFTInteger, 6, 7, 2, 1, 1},
    {"COUNTY", 'L', 'N', OFTInteger, 8, 10, 3, 1, 1},
    {"TLID", 'R', 'N', OFTInteger, 11, 20, 10, 1, 1},
    {"HIST", 'L', 'A', OFTString, 21, 21, 1, 1, 1},
    {"SOURCE", 'L', 'A', OFTString, 22, 22, 1, 1, 1},
    {"TLIDFR1", 'R', 'N', OFTInteger, 23, 32, 10, 1, 1},
    {"TLIDFR2", 'R', 'N', OFTInteger, 33, 42, 10, 1, 1},
    {"TLIDTO1", 'R', 'N', OFTInteger, 43, 52, 10, 1, 1},
    {"TLIDTO2", 'R', 'N', OFTInteger, 53, 62, 10, 1, 1}};
static const TigerRecordInfo rtH_info = {
    rtH_fields, sizeof(rtH_fields) / sizeof(TigerFieldInfo), 62};

/************************************************************************/
/*                           TigerIDHistory()                           */
/************************************************************************/

TigerIDHistory::TigerIDHistory(OGRTigerDataSource *poDSIn,
                               CPL_UNUSED const char *pszPrototypeModule)
    : TigerFileBase(&rtH_info, FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("IDHistory");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    /* -------------------------------------------------------------------- */
    /*      Fields from record type H                                       */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTInfo, poFeatureDefn);
}
