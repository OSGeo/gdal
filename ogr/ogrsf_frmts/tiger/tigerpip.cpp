/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPIP, providing access to .RTP files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char P_FILE_CODE[] = "P";

static const TigerFieldInfo rtP_2002_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTInteger, 6, 10, 5, 1, 1},
    {"CENID", 'L', 'A', OFTString, 11, 15, 5, 1, 1},
    {"POLYID", 'R', 'N', OFTInteger, 16, 25, 10, 1, 1},
    {"POLYLONG", 'R', 'N', OFTInteger, 26, 35, 10, 1, 1},
    {"POLYLAT", 'R', 'N', OFTInteger, 36, 44, 9, 1, 1},
    {"WATER", 'L', 'N', OFTInteger, 45, 45, 1, 1, 1},
};
static const TigerRecordInfo rtP_2002_info = {
    rtP_2002_fields, sizeof(rtP_2002_fields) / sizeof(TigerFieldInfo), 45};

static const TigerFieldInfo rtP_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"FILE", 'L', 'N', OFTString, 6, 10, 5, 1, 1},
    {"STATE", 'L', 'N', OFTInteger, 6, 7, 2, 1, 1},
    {"COUNTY", 'L', 'N', OFTInteger, 8, 10, 3, 1, 1},
    {"CENID", 'L', 'A', OFTString, 11, 15, 5, 1, 1},
    {"POLYID", 'R', 'N', OFTInteger, 16, 25, 10, 1, 1}};
static const TigerRecordInfo rtP_info = {
    rtP_fields, sizeof(rtP_fields) / sizeof(TigerFieldInfo), 44};

/************************************************************************/
/*                              TigerPIP()                              */
/************************************************************************/

TigerPIP::TigerPIP(OGRTigerDataSource *poDSIn,
                   CPL_UNUSED const char *pszPrototypeModule)
    : TigerPoint(nullptr, P_FILE_CODE)
{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("PIP");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbPoint);

    if (poDS->GetVersion() >= TIGER_2002)
    {
        psRTInfo = &rtP_2002_info;
    }
    else
    {
        psRTInfo = &rtP_info;
    }
    AddFieldDefns(psRTInfo, poFeatureDefn);
}

OGRFeature *TigerPIP::GetFeature(int nRecordId)
{
    return TigerPoint::GetFeature(nRecordId, 26, 35, 36, 44);
}
