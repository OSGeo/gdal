/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerLandmarks, providing access to .RT7 files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char SEVEN_FILE_CODE[] = "7";

static const TigerFieldInfo rt7_2002_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTInteger, 6, 10, 5, 1, 1},
    {"LAND", 'R', 'N', OFTInteger, 11, 20, 10, 1, 1},
    {"SOURCE", 'L', 'A', OFTString, 21, 21, 1, 1, 1},
    {"CFCC", 'L', 'A', OFTString, 22, 24, 3, 1, 1},
    {"LANAME", 'L', 'A', OFTString, 25, 54, 30, 1, 1},
    {"LALONG", 'R', 'N', OFTInteger, 55, 64, 10, 1, 1},
    {"LALAT", 'R', 'N', OFTInteger, 65, 73, 9, 1, 1},
    {"FILLER", 'L', 'A', OFTString, 74, 74, 1, 1, 1},
};
static const TigerRecordInfo rt7_2002_info = {
    rt7_2002_fields, sizeof(rt7_2002_fields) / sizeof(TigerFieldInfo), 74};

static const TigerFieldInfo rt7_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTString, 6, 10, 5, 1, 0},
    {"STATE", 'L', 'N', OFTInteger, 6, 7, 2, 1, 1},
    {"COUNTY", 'L', 'N', OFTInteger, 8, 10, 3, 1, 1},
    {"LAND", 'R', 'N', OFTInteger, 11, 20, 10, 1, 1},
    {"SOURCE", 'L', 'A', OFTString, 21, 21, 1, 1, 1},
    {"CFCC", 'L', 'A', OFTString, 22, 24, 3, 1, 1},
    {"LANAME", 'L', 'A', OFTString, 25, 54, 30, 1, 1}};
static const TigerRecordInfo rt7_info = {
    rt7_fields, sizeof(rt7_fields) / sizeof(TigerFieldInfo), 74};

/************************************************************************/
/*                            TigerLandmarks()                          */
/************************************************************************/

TigerLandmarks::TigerLandmarks(OGRTigerDataSource *poDSIn,
                               CPL_UNUSED const char *pszPrototypeModule)
    : TigerPoint(nullptr, SEVEN_FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("Landmarks");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbPoint);

    if (poDS->GetVersion() >= TIGER_2002)
    {
        psRTInfo = &rt7_2002_info;
    }
    else
    {
        psRTInfo = &rt7_info;
    }

    AddFieldDefns(psRTInfo, poFeatureDefn);
}

OGRFeature *TigerLandmarks::GetFeature(int nRecordId)
{
    return TigerPoint::GetFeature(nRecordId, 55, 64, 65, 73);
}
