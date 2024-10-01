/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Parts of OGRLayer dealing with Arrow C interface
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRLAYERARROW_H_DEFINED
#define OGRLAYERARROW_H_DEFINED

#include "cpl_port.h"

#include <map>
#include <string>

constexpr const char *ARROW_EXTENSION_NAME_KEY = "ARROW:extension:name";
constexpr const char *ARROW_EXTENSION_METADATA_KEY = "ARROW:extension:metadata";
constexpr const char *EXTENSION_NAME_OGC_WKB = "ogc.wkb";
constexpr const char *EXTENSION_NAME_GEOARROW_WKB = "geoarrow.wkb";
constexpr const char *EXTENSION_NAME_ARROW_JSON = "arrow.json";

std::map<std::string, std::string>
    CPL_DLL OGRParseArrowMetadata(const char *pabyMetadata);

bool CPL_DLL OGRCloneArrowArray(const struct ArrowSchema *schema,
                                const struct ArrowArray *array,
                                struct ArrowArray *out_array);

bool CPL_DLL OGRCloneArrowSchema(const struct ArrowSchema *schema,
                                 struct ArrowSchema *out_schema);

#endif  // OGRLAYERARROW_H_DEFINED
