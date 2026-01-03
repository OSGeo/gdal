/******************************************************************************
 * Project:  GDAL Core
 * Purpose:  Test including all public C headers as extern C from .cpp file
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_atomic_ops.h"
#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_list.h"
#include "cpl_minixml.h"
#include "cpl_port.h"
#include "cpl_progress.h"
#include "cpl_quad_tree.h"
#include "cpl_vsi.h"
#include "gdal_alg.h"
#include "gdal_version.h"
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_srs_api.h"

int main()
{
    return 0;
}
