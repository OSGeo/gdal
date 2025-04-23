/******************************************************************************
 * $Id: iso19115_srs.cpp 35951 2016-10-26 16:53:54Z goatbar $
 *
 * Project:  BAG Driver
 * Purpose:  Implements code to parse ISO 19115 metadata to extract a
 *           spatial reference system.  Eventually intended to be made
 *           a method on OGRSpatialReference.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef ISO19155_SRS_H_INCLUDED_
#define ISO19155_SRS_H_INCLUDED_

#include "cpl_port.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

OGRErr OGR_SRS_ImportFromISO19115(OGRSpatialReference *poThis,
                                  const char *pszISOXML);

#endif  // ISO19155_SRS_H_INCLUDED_
