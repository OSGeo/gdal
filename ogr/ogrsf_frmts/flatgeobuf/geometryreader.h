/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  GeometryReader class declarations.
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

#ifndef FLATGEOBUF_GEOMETRYREADER_H_INCLUDED
#define FLATGEOBUF_GEOMETRYREADER_H_INCLUDED

#include "feature_generated.h"
#include "ogr_p.h"

namespace ogr_flatgeobuf {

class GeometryReader {
    private:
        const FlatGeobuf::Geometry *m_geometry;
        const FlatGeobuf::GeometryType m_geometryType;
        const bool m_hasZ;
        const bool m_hasM;

        uint32_t m_length = 0;
        uint32_t m_offset = 0;

        OGRPoint *readPoint();
        OGRMultiPoint *readMultiPoint();
        OGRErr readSimpleCurve(OGRSimpleCurve *c);
        OGRMultiLineString *readMultiLineString();
        OGRPolygon *readPolygon();
        OGRMultiPolygon *readMultiPolygon();
        OGRGeometryCollection *readGeometryCollection();
        OGRCompoundCurve *readCompoundCurve();
        OGRCurvePolygon *readCurvePolygon();
        OGRMultiCurve *readMultiCurve();
        OGRMultiSurface *readMultiSurface();
        OGRPolyhedralSurface *readPolyhedralSurface();
        OGRTriangulatedSurface *readTIN();
        OGRTriangle *readTriangle();

        OGRGeometry *readPart(const FlatGeobuf::Geometry *part) {
            return GeometryReader(part, m_hasZ, m_hasM).read();
        }
        OGRGeometry *readPart(const FlatGeobuf::Geometry *part, const FlatGeobuf::GeometryType geometryType) {
            return GeometryReader(part, geometryType, m_hasZ, m_hasM).read();
        }

        template <class T>
        T *readSimpleCurve(const bool halfLength = false) {
            if (halfLength)
                m_length = m_length / 2;
            const auto csc = new T();
            if (readSimpleCurve(csc) != OGRERR_NONE) {
                delete csc;
                return nullptr;
            }
            return csc;
        }
    public:
        GeometryReader(
            const FlatGeobuf::Geometry *geometry,
            const FlatGeobuf::GeometryType geometryType,
            const bool hasZ,
            const bool hasM) :
            m_geometry (geometry),
            m_geometryType (geometryType),
            m_hasZ (hasZ),
            m_hasM (hasM)
            { }
        GeometryReader(
            const FlatGeobuf::Geometry *geometry,
            const bool hasZ,
            const bool hasM) :
            m_geometry (geometry),
            m_geometryType (geometry->type()),
            m_hasZ (hasZ),
            m_hasM (hasM)
            { }
        OGRGeometry *read();
};

}

#endif /* ndef FLATGEOBUF_GEOMETRYREADER_H_INCLUDED */
