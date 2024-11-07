/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerSpatialMetadata, providing access to .RTM files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

static const char M_FILE_CODE[] = "M";

static const TigerFieldInfo rtM_fields[] = {
    // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"TLID", 'R', 'N', OFTInteger, 6, 15, 10, 1, 1},
    {"RTSQ", 'R', 'N', OFTInteger, 16, 18, 3, 1, 1},
    {"SOURCEID", 'L', 'A', OFTString, 19, 28, 10, 1, 1},
    {"ID", 'L', 'A', OFTString, 29, 46, 18, 1, 1},
    {"IDFLAG", 'R', 'A', OFTString, 47, 47, 1, 1, 1},
    {"RS-M1", 'L', 'A', OFTString, 48, 65, 18, 1, 1},
    {"RS-M2", 'L', 'A', OFTString, 66, 67, 2, 1, 1},
    {"RS-M3", 'L', 'A', OFTString, 68, 90, 23, 1, 1}};
static const TigerRecordInfo rtM_info = {
    rtM_fields, sizeof(rtM_fields) / sizeof(TigerFieldInfo), 90};

/************************************************************************/
/*                        TigerSpatialMetadata()                        */
/************************************************************************/

TigerSpatialMetadata::TigerSpatialMetadata(
    OGRTigerDataSource *poDSIn, CPL_UNUSED const char *pszPrototypeModule)
    : TigerFileBase(&rtM_info, M_FILE_CODE)

{
    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("SpatialMetadata");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    /* -------------------------------------------------------------------- */
    /*      Fields from record type H                                       */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTInfo, poFeatureDefn);
}
