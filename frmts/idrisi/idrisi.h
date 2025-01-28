/*****************************************************************************
 *
 * Project:  Idrisi Raster Image File Driver
 * Purpose:  Read/write Idrisi Raster Image Format RST
 * Author:   Ivan Lucena, [lucena_ivan at hotmail.com]
 *
 ******************************************************************************
 * Copyright( c ) 2006, Ivan Lucena
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef IDRISI_H_INCLUDED
#define IDRISI_H_INCLUDED

#include "cpl_error.h"
#include "ogr_spatialref.h"

CPLErr IdrisiGeoReference2Wkt(const char *pszFilename, const char *pszRefSystem,
                              const char *pszRefUnits,
                              OGRSpatialReference &oSRS);

#endif /*  IDRISI_H_INCLUDED */
