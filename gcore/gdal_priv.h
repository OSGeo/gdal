/******************************************************************************
 *
 * Name:     gdal_priv.h
 * Project:  GDAL Core
 * Purpose:  GDAL Core C++/Private declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_PRIV_H_INCLUDED
#define GDAL_PRIV_H_INCLUDED

/**
 * \file gdal_priv.h
 *
 * C++ GDAL entry points.
 */

/* -------------------------------------------------------------------- */
/*      Pull in the public declarations.  This gets the C apis, and     */
/*      also various constants.  However, we will still get to          */
/*      provide the real class definitions for the GDAL classes.        */
/* -------------------------------------------------------------------- */

#include "gdal.h"
#include "gdal_frmts.h"
#include "gdalsubdatasetinfo.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_atomic_ops.h"

#include <stdarg.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "ogr_core.h"
#include "ogr_feature.h"

#include "gdal_multidomainmetadata.h"
#include "gdal_majorobject.h"
#include "gdal_defaultoverviews.h"
#include "gdal_openinfo.h"
#include "gdal_gcp.h"
#include "gdal_geotransform.h"
#include "gdal_dataset.h"
#include "gdal_rasterblock.h"
#include "gdal_colortable.h"
#include "gdal_rasterband.h"
#include "gdal_maskbands.h"
#include "gdal_driver.h"
#include "gdal_drivermanager.h"
#include "gdal_asyncreader.h"
#include "gdal_multidim.h"
#include "gdal_relationship.h"
#include "gdal_cpp_functions.h"

#endif /* ndef GDAL_PRIV_H_INCLUDED */
