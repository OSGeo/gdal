/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements RasterLite2 support class.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 *
 * CREDITS: The RasterLite2 module has been completely funded by:
 * Regione Toscana - Settore Sistema Informativo Territoriale ed
 * Ambientale (GDAL/RasterLite2 driver)
 * CIG: 644544015A
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef RASTERLITE_HEADER_H
#define RASTERLITE_HEADER_H

#include "cpl_port.h"

#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) &&               \
     !defined(_MSC_VER))
#pragma GCC system_header
#endif

#ifdef HAVE_RASTERLITE2
#include "rasterlite2/rasterlite2.h"
#endif

#endif  // RASTERLITE_HEADER_H
