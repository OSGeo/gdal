/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Geometry write functions declarations.
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

#ifndef FLATGEOBUF_GEOMETRYWRITE_H_INCLUDED
#define FLATGEOBUF_GEOMETRYWRITE_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include "feature_generated.h"

namespace ogr_flatgeobuf {

struct GeometryWriteContext {
    FlatGeobuf::GeometryType geometryType;
    bool hasZ;
    bool hasM;
    std::vector<double> xy;
    std::vector<double> z;
    std::vector<double> m;
    std::vector<uint32_t> ends;
    std::vector<uint32_t> lengths;
};

void writePoint(OGRPoint *p, GeometryWriteContext &gc);
void writeMultiPoint(OGRMultiPoint *mp, GeometryWriteContext &gc);
uint32_t writeSimpleCurve(OGRSimpleCurve *sc, GeometryWriteContext &gc);
void writeMultiLineString(OGRMultiLineString *mls, GeometryWriteContext &gc);
uint32_t writePolygon(OGRPolygon *p, GeometryWriteContext &gc, bool isMulti, uint32_t e);
void writeMultiPolygon(OGRMultiPolygon *mp, GeometryWriteContext &gc);
const flatbuffers::Offset<FlatGeobuf::Geometry> writeGeometry(
    flatbuffers::FlatBufferBuilder &fbb,
    OGRGeometry *ogrGeometry, GeometryWriteContext &gc);
}

#endif /* ndef FLATGEOBUF_GEOMETRYWRITE_H_INCLUDED */
