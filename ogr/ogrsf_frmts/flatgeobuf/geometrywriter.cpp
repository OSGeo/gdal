/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Implements GeometryWriter class.
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

GeometryType
GeometryWriter::translateOGRwkbGeometryType(const OGRwkbGeometryType eGType)
{
    const auto flatType = wkbFlatten(eGType);
    GeometryType geometryType = GeometryType::Unknown;
    if (flatType <= 17)
        geometryType = (GeometryType)flatType;
    return geometryType;
}

void GeometryWriter::writePoint(const OGRPoint *p)
{
    m_xy.push_back(p->getX());
    m_xy.push_back(p->getY());
    if (m_hasZ)
        m_z.push_back(p->getZ());
    if (m_hasM)
        m_m.push_back(p->getM());
}

void GeometryWriter::writeMultiPoint(const OGRMultiPoint *mp)
{
    for (const auto part : *mp)
        if (!part->IsEmpty())
            writePoint(part);
}

uint32_t GeometryWriter::writeSimpleCurve(const OGRSimpleCurve *sc)
{
    uint32_t numPoints = sc->getNumPoints();
    const auto xyLength = m_xy.size();
    m_xy.resize(xyLength + (numPoints * 2));
    const auto zLength = m_z.size();
    double *padfZOut = nullptr;
    if (m_hasZ)
    {
        m_z.resize(zLength + numPoints);
        padfZOut = m_z.data() + zLength;
    }
    const auto mLength = m_m.size();
    double *padfMOut = nullptr;
    if (m_hasM)
    {
        m_m.resize(mLength + numPoints);
        padfMOut = m_m.data() + mLength;
    }
    sc->getPoints(reinterpret_cast<double *>(
                      reinterpret_cast<OGRRawPoint *>(m_xy.data() + xyLength)),
                  sizeof(OGRRawPoint),
                  reinterpret_cast<double *>(
                      reinterpret_cast<OGRRawPoint *>(m_xy.data() + xyLength)) +
                      1,
                  sizeof(OGRRawPoint), padfZOut, sizeof(double), padfMOut,
                  sizeof(double));
    return numPoints;
}

void GeometryWriter::writeMultiLineString(const OGRMultiLineString *mls)
{
    uint32_t e = 0;
    for (const auto part : *mls)
        if (!part->IsEmpty())
            m_ends.push_back(e += writeSimpleCurve(part));
}

void GeometryWriter::writePolygon(const OGRPolygon *p)
{
    const auto exteriorRing = p->getExteriorRing();
    const auto numInteriorRings = p->getNumInteriorRings();
    uint32_t e = writeSimpleCurve(exteriorRing);
    // NOTE: should not write ends if only exterior ring
    if (numInteriorRings > 0)
    {
        m_ends.push_back(e);
        for (int i = 0; i < numInteriorRings; i++)
            m_ends.push_back(e += writeSimpleCurve(p->getInteriorRing(i)));
    }
}

const Offset<Geometry>
GeometryWriter::writeMultiPolygon(const OGRMultiPolygon *mp, int depth)
{
    std::vector<Offset<Geometry>> parts;
    for (const auto part : *mp)
        if (!part->IsEmpty())
            parts.push_back(writePart(part, GeometryType::Polygon, depth + 1));
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, m_geometryType, &parts);
}

const Offset<Geometry>
GeometryWriter::writeGeometryCollection(const OGRGeometryCollection *gc,
                                        int depth)
{
    std::vector<Offset<Geometry>> parts;
    for (const auto part : *gc)
        if (!part->IsEmpty())
            parts.push_back(writePart(part, depth + 1));
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, m_geometryType, &parts);
}

const Offset<Geometry>
GeometryWriter::writeCompoundCurve(const OGRCompoundCurve *cc, int depth)
{
    std::vector<Offset<Geometry>> parts;
    for (const auto curve : *cc)
        parts.push_back(writePart(curve, depth + 1));
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, m_geometryType, &parts);
}

const Offset<Geometry>
GeometryWriter::writeCurvePolygon(const OGRCurvePolygon *cp, int depth)
{
    std::vector<Offset<Geometry>> parts;
    for (const auto curve : *cp)
        parts.push_back(writePart(curve, depth + 1));
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, m_geometryType, &parts);
}

const Offset<Geometry>
GeometryWriter::writePolyhedralSurface(const OGRPolyhedralSurface *p, int depth)
{
    std::vector<Offset<Geometry>> parts;
    for (const auto part : *p)
        parts.push_back(writePart(part, depth + 1));
    return CreateGeometryDirect(m_fbb, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, m_geometryType, &parts);
}

void GeometryWriter::writeTIN(const OGRTriangulatedSurface *ts)
{
    const auto numGeometries = ts->getNumGeometries();
    if (numGeometries == 1)
        return (void)writeSimpleCurve(ts->getGeometryRef(0)->getExteriorRing());
    uint32_t e = 0;
    for (const auto part : *ts)
        m_ends.push_back(e += writeSimpleCurve(part->getExteriorRing()));
}

const Offset<Geometry> GeometryWriter::write(int depth)
{
    bool unknownGeometryType = false;
    if (depth == 0 && m_geometryType == GeometryType::Unknown)
    {
        m_geometryType =
            translateOGRwkbGeometryType(m_ogrGeometry->getGeometryType());
        unknownGeometryType = true;
    }
    switch (m_geometryType)
    {
        case GeometryType::Point:
            writePoint(m_ogrGeometry->toPoint());
            break;
        case GeometryType::MultiPoint:
            writeMultiPoint(m_ogrGeometry->toMultiPoint());
            break;
        case GeometryType::LineString:
            writeSimpleCurve(m_ogrGeometry->toLineString());
            break;
        case GeometryType::MultiLineString:
            writeMultiLineString(m_ogrGeometry->toMultiLineString());
            break;
        case GeometryType::Polygon:
            writePolygon(m_ogrGeometry->toPolygon());
            break;
        case GeometryType::MultiPolygon:
            return writeMultiPolygon(m_ogrGeometry->toMultiPolygon(), depth);
        case GeometryType::GeometryCollection:
            return writeGeometryCollection(
                m_ogrGeometry->toGeometryCollection(), depth);
        case GeometryType::CircularString:
            writeSimpleCurve(m_ogrGeometry->toCircularString());
            break;
        case GeometryType::CompoundCurve:
            return writeCompoundCurve(m_ogrGeometry->toCompoundCurve(), depth);
        case GeometryType::CurvePolygon:
            return writeCurvePolygon(m_ogrGeometry->toCurvePolygon(), depth);
        case GeometryType::MultiCurve:
            return writeGeometryCollection(m_ogrGeometry->toMultiCurve(),
                                           depth);
        case GeometryType::MultiSurface:
            return writeGeometryCollection(m_ogrGeometry->toMultiSurface(),
                                           depth);
        case GeometryType::PolyhedralSurface:
            return writePolyhedralSurface(m_ogrGeometry->toPolyhedralSurface(),
                                          depth);
        case GeometryType::Triangle:
            writePolygon(m_ogrGeometry->toTriangle());
            break;
        case GeometryType::TIN:
            writeTIN(m_ogrGeometry->toTriangulatedSurface());
            break;
        default:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GeometryWriter::write: Unknown type %d",
                     (int)m_geometryType);
            return 0;
    }
    const auto pEnds = m_ends.empty() ? nullptr : &m_ends;
    const auto pXy = m_xy.empty() ? nullptr : &m_xy;
    const auto pZ = m_z.empty() ? nullptr : &m_z;
    const auto pM = m_m.empty() ? nullptr : &m_m;
    const auto geometryType = depth > 0 || unknownGeometryType
                                  ? m_geometryType
                                  : GeometryType::Unknown;
    return FlatGeobuf::CreateGeometryDirect(m_fbb, pEnds, pXy, pZ, pM, nullptr,
                                            nullptr, geometryType);
}
