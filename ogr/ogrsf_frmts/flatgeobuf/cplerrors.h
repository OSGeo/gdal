/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Common CPLError helpers.
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2019, Björn Harrtell <bjorn at wololo dot org>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef FLATGEOBUF_CPLERRORS_H_INCLUDED
#define FLATGEOBUF_CPLERRORS_H_INCLUDED

#include "ogr_p.h"

namespace ogr_flatgeobuf
{

static std::nullptr_t CPLErrorInvalidPointer(const char *message)
{
    CPLError(CE_Failure, CPLE_AppDefined, "Unexpected nullptr: %s", message);
    return nullptr;
}

static OGRErr CPLErrorInvalidSize(const char *message)
{
    CPLError(CE_Failure, CPLE_AppDefined, "Invalid size detected: %s", message);
    return OGRERR_CORRUPT_DATA;
}

}  // namespace ogr_flatgeobuf

#endif /* ndef FLATGEOBUF_CPLERRORS_H_INCLUDED */
