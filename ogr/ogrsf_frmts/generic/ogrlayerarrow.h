/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Parts of OGRLayer dealing with Arrow C interface
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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

#endif  // OGRLAYERARROW_H_DEFINED
