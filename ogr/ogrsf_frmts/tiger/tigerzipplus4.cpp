/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerZipPlus4, providing access to .RTZ files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char Z_FILE_CODE[] = "Z";

static const TigerFieldInfo rtZ_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"TLID", 'R', 'N', OFTInteger, 6, 15, 10, 1, 1},
    {"RTSQ", 'R', 'N', OFTInteger, 16, 18, 3, 1, 1},
    {"ZIP4L", 'L', 'N', OFTInteger, 19, 22, 4, 1, 1},
    {"ZIP4R", 'L', 'N', OFTInteger, 23, 26, 4, 1, 1}};
static const TigerRecordInfo rtZ_info = {
    rtZ_fields, sizeof(rtZ_fields) / sizeof(TigerFieldInfo), 26};

/************************************************************************/
/*                           TigerZipPlus4()                            */
/************************************************************************/

TigerZipPlus4::TigerZipPlus4(OGRTigerDataSource *poDSIn,
                             CPL_UNUSED const char *pszPrototypeModule)
    : TigerFileBase(&rtZ_info, Z_FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("ZipPlus4");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    /* -------------------------------------------------------------------- */
    /*      Fields from type Z record.                                      */
    /* -------------------------------------------------------------------- */
    AddFieldDefns(psRTInfo, poFeatureDefn);
}
