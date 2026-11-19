/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerOverUnder, providing access to .RTU files.
 * Author:   Mark Phillips, mbp@geomtech.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam, Mark Phillips
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char U_FILE_CODE[] = "U";

static const TigerFieldInfo rtU_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTInteger, 6, 10, 5, 1, 1},
    {"TZID", 'R', 'N', OFTInteger, 11, 20, 10, 1, 1},
    {"RTSQ", 'R', 'N', OFTInteger, 21, 21, 1, 1, 1},
    {"TLIDOV1", 'R', 'N', OFTInteger, 22, 31, 10, 1, 1},
    {"TLIDOV2", 'R', 'N', OFTInteger, 32, 41, 10, 1, 1},
    {"TLIDUN1", 'R', 'N', OFTInteger, 42, 51, 10, 1, 1},
    {"TLIDUN2", 'R', 'N', OFTInteger, 52, 61, 10, 1, 1},
    {"FRLONG", 'R', 'N', OFTInteger, 62, 71, 10, 1, 1},
    {"FRLAT", 'R', 'N', OFTInteger, 72, 80, 9, 1, 1},
};
static const TigerRecordInfo rtU_info = {
    rtU_fields, sizeof(rtU_fields) / sizeof(TigerFieldInfo), 80};

/************************************************************************/
/*                           TigerOverUnder()                           */
/************************************************************************/

TigerOverUnder::TigerOverUnder(OGRTigerDataSource *poDSIn,
                               CPL_UNUSED const char *pszPrototypeModule)
    : TigerPoint(&rtU_info, U_FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("OverUnder");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    AddFieldDefns(psRTInfo, poFeatureDefn);
}

OGRFeature *TigerOverUnder::GetFeature(int nRecordId)
{
    return TigerPoint::GetFeature(nRecordId, 62, 71, 72, 80);
}
