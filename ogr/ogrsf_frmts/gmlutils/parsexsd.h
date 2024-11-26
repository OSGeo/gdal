/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLParseXSD()
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef PARSEXSD_H_INCLUDED
#define PARSEXSD_H_INCLUDED

#include "cpl_port.h"

#include <vector>
#include "gmlfeature.h"

bool CPL_DLL GMLParseXSD(const char *pszFile, bool bUseSchemaImports,
                         std::vector<GMLFeatureClass *> &aosClasses,
                         bool &bFullyUnderstood);

#endif  // PARSEXSD_H_INCLUDED
