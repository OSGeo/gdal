/******************************************************************************
 * Project:  WFS Reader
 * Purpose:  WFS filtering
 * Purpose:  Implements OGR SQL into OGC Filter translation.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRWFSFILTER_H_INCLUDED
#define OGRWFSFILTER_H_INCLUDED

#include "cpl_string.h"
#include "gdal_priv.h"
#include "ogr_feature.h"
#include "ogrsf_frmts.h"
#include "ogr_swq.h"

CPLString CPL_DLL WFS_TurnSQLFilterToOGCFilter(
    const swq_expr_node *poExpr, GDALDataset *poDS, OGRFeatureDefn *poFDefn,
    int nVersion, int bPropertyIsNotEqualToSupported, int bUseFeatureId,
    int bGmlObjectIdNeedsGMLPrefix, const char *pszNSPrefix,
    int *pbOutNeedsNullCheck);
swq_custom_func_registrar CPL_DLL *WFSGetCustomFuncRegistrar();

#endif
