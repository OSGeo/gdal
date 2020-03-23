/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Common CPLError helpers.
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2019, Björn Harrtell <bjorn at wololo dot org>
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

#ifndef FLATGEOBUF_CPLERRORS_H_INCLUDED
#define FLATGEOBUF_CPLERRORS_H_INCLUDED

#include "ogr_p.h"

namespace ogr_flatgeobuf {

static std::nullptr_t CPLErrorInvalidPointer(const char *message) {
    CPLError(CE_Failure, CPLE_AppDefined, "Unexpected nullptr: %s", message);
    return nullptr;
}

static OGRErr CPLErrorInvalidSize(const char *message) {
    CPLError(CE_Failure, CPLE_AppDefined, "Invalid size detected: %s", message);
    return OGRERR_CORRUPT_DATA;
}

}

#endif /* ndef FLATGEOBUF_CPLERRORS_H_INCLUDED */
