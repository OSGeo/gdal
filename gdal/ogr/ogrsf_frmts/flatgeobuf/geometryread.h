/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Geometry read functions declarations.
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

#ifndef FLATGEOBUF_GEOMETRYREAD_H_INCLUDED
#define FLATGEOBUF_GEOMETRYREAD_H_INCLUDED

#include "feature_generated.h"

namespace ogr_flatgeobuf {

struct GeometryReadContext {
    const FlatGeobuf::Geometry *geometry;
    FlatGeobuf::GeometryType geometryType;
    bool hasZ;
    bool hasM;
    uint32_t length;
    uint32_t offset;
};

OGRPoint *readPoint(GeometryReadContext &gc);
OGRMultiPoint *readMultiPoint(GeometryReadContext &gc);
OGRErr readSimpleCurve(GeometryReadContext &gc, OGRSimpleCurve *c);
OGRMultiLineString *readMultiLineString(GeometryReadContext &gc);
OGRPolygon *readPolygon(GeometryReadContext &gc);
OGRMultiPolygon *readMultiPolygon(GeometryReadContext &gc);
OGRGeometry *readGeometry(GeometryReadContext &gc);
OGRGeometry *readGeometry(const FlatGeobuf::Feature *feature, GeometryReadContext &gc);

template <class T>
T *readSimpleCurve(GeometryReadContext &gc)
{
    const auto csc = new T();
    if (readSimpleCurve(gc, csc) != OGRERR_NONE) {
        delete csc;
        return nullptr;
    }
    return csc;
};

}

#endif /* ndef FLATGEOBUF_GEOMETRYREAD_H_INCLUDED */
