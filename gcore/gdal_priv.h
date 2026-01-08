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
 * This file is legacy since GDAL 3.12, but will be kept at least in the whole
 * GDAL 3.x series. Applications using the C++ API and that target only
 * GDAL >= 3.12 should rather use the new gdal_cpp.h header, or the finer
 * grain headers it includes.
 *
 * C++ GDAL entry points (legacy).
 */

/* -------------------------------------------------------------------- */
/*      Pull in the public declarations.  This gets the C apis, and     */
/*      also various constants.  However, we will still get to          */
/*      provide the real class definitions for the GDAL classes.        */
/* -------------------------------------------------------------------- */

#if !defined(GDAL_COMPILATION) &&                                              \
    !defined(GDAL_PRIV_SKIP_OTHER_GDAL_HEADERS) && !defined(GDAL_4_0_COMPAT)
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdalsubdatasetinfo.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#endif

#if !defined(GDAL_COMPILATION) && !defined(GDAL_PRIV_SKIP_STANDARD_HEADERS) && \
    !defined(GDAL_4_0_COMPAT)
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
#endif

#include "gdal_raster_cpp.h"
#include "gdal_multidim_cpp.h"

#endif /* ndef GDAL_PRIV_H_INCLUDED */
