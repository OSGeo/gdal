/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  GeometryWriter class declarations.
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

#ifndef FLATGEOBUF_GEOMETRYWRITER_H_INCLUDED
#define FLATGEOBUF_GEOMETRYWRITER_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include "feature_generated.h"

namespace ogr_flatgeobuf {

class GeometryWriter {
    private:
        flatbuffers::FlatBufferBuilder &m_fbb;
        const OGRGeometry *m_ogrGeometry;
        FlatGeobuf::GeometryType m_geometryType;
        const bool m_hasZ;
        const bool m_hasM;
        std::vector<double> m_xy;
        std::vector<double> m_z;
        std::vector<double> m_m;
        std::vector<uint32_t> m_ends;

        void writePoint(const OGRPoint *p);
        void writeMultiPoint(const OGRMultiPoint *mp);
        uint32_t writeSimpleCurve(const OGRSimpleCurve *sc);
        void writeMultiLineString(const OGRMultiLineString *mls);
        void writePolygon(const OGRPolygon *p);
        const flatbuffers::Offset<FlatGeobuf::Geometry> writeMultiPolygon(const OGRMultiPolygon *mp, int depth);
        const flatbuffers::Offset<FlatGeobuf::Geometry> writeGeometryCollection(const OGRGeometryCollection *gc, int depth);
        const flatbuffers::Offset<FlatGeobuf::Geometry> writeCompoundCurve(const OGRCompoundCurve *cc, int depth);
        const flatbuffers::Offset<FlatGeobuf::Geometry> writeCurvePolygon(const OGRCurvePolygon *cp, int depth);
        const flatbuffers::Offset<FlatGeobuf::Geometry> writePolyhedralSurface(const OGRPolyhedralSurface *p, int depth);
        void writeTIN(const OGRTriangulatedSurface *p);

    public:
        GeometryWriter(
            flatbuffers::FlatBufferBuilder &fbb,
            const  OGRGeometry *ogrGeometry,
            const FlatGeobuf::GeometryType geometryType,
            const bool hasZ,
            const bool hasM) :
            m_fbb (fbb),
            m_ogrGeometry (ogrGeometry),
            m_geometryType (geometryType),
            m_hasZ (hasZ),
            m_hasM (hasM)
            { }
        GeometryWriter(
            flatbuffers::FlatBufferBuilder &fbb,
            const OGRGeometry *ogrGeometry,
            const bool hasZ,
            const bool hasM) :
            m_fbb (fbb),
            m_ogrGeometry (ogrGeometry),
            m_geometryType (GeometryWriter::translateOGRwkbGeometryType(ogrGeometry->getGeometryType())),
            m_hasZ (hasZ),
            m_hasM (hasM)
            { }
        const flatbuffers::Offset<FlatGeobuf::Geometry> write(int depth);
        static FlatGeobuf::GeometryType translateOGRwkbGeometryType(const OGRwkbGeometryType eGType);
};

}

#endif /* ndef FLATGEOBUF_GEOMETRYWRITER_H_INCLUDED */
