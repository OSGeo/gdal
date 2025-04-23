/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 *
 * cpl_serv.h
 *
 * This include file derived and simplified from the GDAL Common Portability
 * Library.
 */

#ifndef CPL_SERV_H_INCLUDED
#define CPL_SERV_H_INCLUDED

/* ==================================================================== */
/*	Standard include files.						*/
/* ==================================================================== */

#include "geo_config.h"

#include "cpl_port.h"
#include "cpl_string.h"

#define GTIFAtof CPLAtof
#define GTIFStrtod CPLStrtod
/*
 * Define an auxiliary symbol to help us to find when the internal cpl_serv.h
 * is used instead of the external one from the geotiff package.
 */
#define CPL_SERV_H_INTERNAL 1

#endif /* ndef CPL_SERV_H_INCLUDED */
