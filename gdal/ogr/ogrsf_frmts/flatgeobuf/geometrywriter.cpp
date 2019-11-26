/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Geometry write functions.
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

#include "geometrywriter.h"

using namespace flatbuffers;
using namespace FlatGeobuf;
using namespace ogr_flatgeobuf;

GeometryType GeometryWriter::translateOGRwkbGeometryType(OGRwkbGeometryType eGType)
{
    const auto flatType = wkbFlatten(eGType);
    GeometryType geometryType = GeometryType::Unknown;
    if (flatType >= 0 && flatType <= 17)
        geometryType = (GeometryType) flatType;
    return geometryType;
}

void GeometryWriter::writePoint(OGRPoint *p)
{
    m_xy.push_back(p->getX());
    m_xy.push_back(p->getY());
    if (m_hasZ)
        m_z.push_back(p->getZ());
    if (m_hasM)
        m_m.push_back(p->getM());
}

void GeometryWriter::writeMultiPoint(OGRMultiPoint *mp)
{
    for (int i = 0; i < mp->getNumGeometries(); i++)
        writePoint(mp->getGeometryRef(i)->toPoint());
}

uint32_t GeometryWriter::writeSimpleCurve(OGRSimpleCurve *sc)
{
    uint32_t numPoints = sc->getNumPoints();
    const auto xyLength = m_xy.size();
    m_xy.resize(xyLength + (numPoints * 2));
    const auto zLength = m_z.size();
    double *padfZOut = nullptr;
    if (m_hasZ) {
        m_z.resize(zLength + numPoints);
        padfZOut = m_z.data() + zLength;
    }
    const auto mLength = m_m.size();
    double *padfMOut = nullptr;
    if (m_hasM) {
        m_m.resize(mLength + numPoints);
        padfMOut = m_m.data() + mLength;
    }
    sc->getPoints(reinterpret_cast<double*>(reinterpret_cast<OGRRawPoint *>(m_xy.data() + xyLength)), sizeof(OGRRawPoint),
                  reinterpret_cast<double*>(reinterpret_cast<OGRRawPoint *>(m_xy.data() + xyLength)) + 1, sizeof(OGRRawPoint),
                  padfZOut, sizeof(double),
                  padfMOut, sizeof(double));
    return numPoints;
}

void GeometryWriter::writeMultiLineString(OGRMultiLineString *mls)
{
    uint32_t e = 0;
    const auto numGeometries = mls->getNumGeometries();
    for (int i = 0; i < numGeometries; i++)
        m_ends.push_back(e += writeSimpleCurve(mls->getGeometryRef(i)->toLineString()));
}

void GeometryWriter::writePolygon(OGRPolygon *p)
{
    const auto exteriorRing = p->getExteriorRing();
    const auto numInteriorRings = p->getNumInteriorRings();
    uint32_t e = writeSimpleCurve(exteriorRing);
    // NOTE: do not have to write ends if only exterior ring
    if (numInteriorRings > 0) {
        m_ends.push_back(e);
        for (int i = 0; i < numInteriorRings; i++)
            m_ends.push_back(e += writeSimpleCurve(p->getInteriorRing(i)));
    }
}

Offset<Geometry> GeometryWriter::writeMultiPolygon(OGRMultiPolygon *mp)
{
    std::vector<Offset<Geometry>> parts;
    for (int i = 0; i < mp->getNumGeometries(); i++) {
        const auto part = mp->getGeometryRef(i)->toPolygon();
        GeometryWriter writer { m_fbb, part, GeometryType::Polygon, m_hasZ, m_hasM };
        parts.push_back(writer.write());
    }
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, m_geometryType, &parts);
}

Offset<Geometry> GeometryWriter::writeGeometryCollection(OGRGeometryCollection *gc)
{
    std::vector<Offset<Geometry>> parts;
    for (int i = 0; i < gc->getNumGeometries(); i++) {
        auto part = gc->getGeometryRef(i);
        GeometryWriter writer { m_fbb, part, m_hasZ, m_hasM };
        parts.push_back(writer.write());
    }
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, m_geometryType, &parts);
}

Offset<Geometry> GeometryWriter::writeCompoundCurve(OGRCompoundCurve *cc)
{
    std::vector<Offset<Geometry>> parts;
    for (int i = 0; i < cc->getNumCurves(); i++) {
        const auto part = cc->getCurve(i);
        GeometryWriter writer { m_fbb, part, m_hasZ, m_hasM };
        parts.push_back(writer.write());
    }
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, m_geometryType, &parts);
}

Offset<Geometry> GeometryWriter::writeCurvePolygon(OGRCurvePolygon *cp)
{
    std::vector<OGRCurve *> curves;
    std::vector<Offset<Geometry>> parts;
    curves.push_back(cp->getExteriorRingCurve());
    for (int i = 0; i < cp->getNumInteriorRings(); i++)
        curves.push_back(cp->getInteriorRingCurve(i));
    for (auto curve : curves) {
        GeometryWriter writer { m_fbb, curve, m_hasZ, m_hasM };
        parts.push_back(writer.write());
    }
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, m_geometryType, &parts);
}

Offset<Geometry> GeometryWriter::writePolyhedralSurface(OGRPolyhedralSurface *p)
{
    std::vector<Offset<Geometry>> parts;
    for (int i = 0; i < p->getNumGeometries(); i++) {
        auto part = p->getGeometryRef(i);
        GeometryWriter writer { m_fbb, part, m_hasZ, m_hasM };
        parts.push_back(writer.write());
    }
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, m_geometryType, &parts);
}

void GeometryWriter::writeTIN(OGRTriangulatedSurface *ts)
{
    auto numGeometries = ts->getNumGeometries();
    if (numGeometries == 1) {
        auto lr = ts->getGeometryRef(0)->toTriangle()->getExteriorRing();
        writeSimpleCurve(lr);
        return;
    }
    uint32_t e = 0;
    for (int i = 0; i < numGeometries; i++) {
        auto lr = ts->getGeometryRef(i)->toTriangle()->getExteriorRing();
        m_ends.push_back(e += writeSimpleCurve(lr));
    }
}

const Offset<Geometry> GeometryWriter::write()
{
    switch (m_geometryType) {
        case GeometryType::Point:
            writePoint(m_ogrGeometry->toPoint()); break;
        case GeometryType::MultiPoint:
            writeMultiPoint(m_ogrGeometry->toMultiPoint()); break;
        case GeometryType::LineString:
            writeSimpleCurve(m_ogrGeometry->toLineString()); break;
        case GeometryType::MultiLineString:
            writeMultiLineString(m_ogrGeometry->toMultiLineString()); break;
        case GeometryType::Polygon:
            writePolygon(m_ogrGeometry->toPolygon()); break;
        case GeometryType::MultiPolygon:
            return writeMultiPolygon(m_ogrGeometry->toMultiPolygon());
        case GeometryType::GeometryCollection:
            return writeGeometryCollection(m_ogrGeometry->toGeometryCollection());
        case GeometryType::CircularString:
            writeSimpleCurve(m_ogrGeometry->toCircularString()); break;
        case GeometryType::CompoundCurve:
            return writeCompoundCurve(m_ogrGeometry->toCompoundCurve());
        case GeometryType::CurvePolygon:
            return writeCurvePolygon(m_ogrGeometry->toCurvePolygon());
        case GeometryType::MultiCurve:
            return writeGeometryCollection(m_ogrGeometry->toMultiCurve());
        case GeometryType::MultiSurface:
            return writeGeometryCollection(m_ogrGeometry->toMultiSurface());
        case GeometryType::PolyhedralSurface:
            return writePolyhedralSurface(m_ogrGeometry->toPolyhedralSurface());
        case GeometryType::Triangle:
            writePolygon(m_ogrGeometry->toTriangle()); break;
        case GeometryType::TIN:
            writeTIN(m_ogrGeometry->toTriangulatedSurface()); break;
        default:
            CPLError(CE_Failure, CPLE_AppDefined, "GeometryWriter::write: Unknown type %d", (int) m_geometryType);
            return 0;
    }
    // TODO: only write m_geometryType if needed (heterogenous collections, depth > 0, feature specific type)
    return FlatGeobuf::CreateGeometryDirect(m_fbb, &m_ends, &m_xy, &m_z, &m_m, nullptr, nullptr, m_geometryType);
}
