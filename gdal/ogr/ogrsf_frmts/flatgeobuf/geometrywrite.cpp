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

#include "geometrywrite.h"

using namespace flatbuffers;
using namespace FlatGeobuf;

GeometryType ogr_flatgeobuf::translateOGRwkbGeometryType(OGRwkbGeometryType eGType)
{
    const auto flatType = wkbFlatten(eGType);
    GeometryType geometryType = GeometryType::Unknown;
    if (flatType >= 0 && flatType <= 17)
        geometryType = (GeometryType) flatType;
    return geometryType;
}

void ogr_flatgeobuf::writePoint(OGRPoint *p, GeometryWriteContext &gc)
{
    gc.xy.push_back(p->getX());
    gc.xy.push_back(p->getY());
    if (gc.hasZ)
        gc.z.push_back(p->getZ());
    if (gc.hasM)
        gc.m.push_back(p->getM());
}

void ogr_flatgeobuf::writeMultiPoint(OGRMultiPoint *mp, GeometryWriteContext &gc)
{
    for (int i = 0; i < mp->getNumGeometries(); i++)
        writePoint(mp->getGeometryRef(i)->toPoint(), gc);
}

uint32_t ogr_flatgeobuf::writeSimpleCurve(OGRSimpleCurve *sc, GeometryWriteContext &gc)
{
    uint32_t numPoints = sc->getNumPoints();
    const auto xyLength = gc.xy.size();
    gc.xy.resize(xyLength + (numPoints * 2));
    const auto zLength = gc.z.size();
    double *padfZOut = nullptr;
    if (gc.hasZ) {
        gc.z.resize(zLength + numPoints);
        padfZOut = gc.z.data() + zLength;
    }
    const auto mLength = gc.m.size();
    double *padfMOut = nullptr;
    if (gc.hasM) {
        gc.m.resize(mLength + numPoints);
        padfMOut = gc.m.data() + mLength;
    }
    sc->getPoints(reinterpret_cast<double*>(reinterpret_cast<OGRRawPoint *>(gc.xy.data() + xyLength)), sizeof(OGRRawPoint),
                  reinterpret_cast<double*>(reinterpret_cast<OGRRawPoint *>(gc.xy.data() + xyLength)) + 1, sizeof(OGRRawPoint),
                  padfZOut, sizeof(double),
                  padfMOut, sizeof(double));
    return numPoints;
}

void ogr_flatgeobuf::writeMultiLineString(OGRMultiLineString *mls, GeometryWriteContext &gc)
{
    uint32_t e = 0;
    const auto numGeometries = mls->getNumGeometries();
    for (int i = 0; i < numGeometries; i++)
    {
        e += writeSimpleCurve(mls->getGeometryRef(i)->toLineString(), gc);
        gc.ends.push_back(e);
    }
}

uint32_t ogr_flatgeobuf::writePolygon(OGRPolygon *p, GeometryWriteContext &gc, bool isMulti, uint32_t e)
{
    const auto exteriorRing = p->getExteriorRing();
    const auto numInteriorRings = p->getNumInteriorRings();
    e += writeSimpleCurve(exteriorRing, gc);
    if (numInteriorRings > 0 || isMulti) {
        gc.ends.push_back(e);
        for (int i = 0; i < numInteriorRings; i++)
        {
            e += writeSimpleCurve(p->getInteriorRing(i), gc);
            gc.ends.push_back(e);
        }
    }
    return e;
}

Offset<Geometry> ogr_flatgeobuf::writeMultiPolygon(FlatBufferBuilder &fbb, OGRMultiPolygon *mp, GeometryWriteContext &gc)
{
    std::vector<Offset<Geometry>> parts;
    for (int i = 0; i < mp->getNumGeometries(); i++) {
        const auto p = mp->getGeometryRef(i)->toPolygon();
        GeometryWriteContext gcPart { GeometryType::Polygon, gc.hasZ, gc.hasM };
        parts.push_back(writeGeometry(fbb, p, gcPart));
    }
    const auto geometryOffset = CreateGeometryDirect(fbb, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, gc.geometryType, &parts);
    return geometryOffset;
}

Offset<Geometry> ogr_flatgeobuf::writeCompoundCurve(FlatBufferBuilder &fbb, OGRCompoundCurve *cc, GeometryWriteContext &gc)
{
    std::vector<Offset<Geometry>> parts;
    for (int i = 0; i < cc->getNumCurves(); i++) {
        auto part = cc->getCurve(i);
        auto eGType = part->getGeometryType();
        auto geometryType = translateOGRwkbGeometryType(eGType);
        GeometryWriteContext gcPart { geometryType, gc.hasZ, gc.hasM };
        parts.push_back(writeGeometry(fbb, part, gcPart));
    }
    const auto geometryOffset = CreateGeometryDirect(fbb, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, gc.geometryType, &parts);
    return geometryOffset;
}

Offset<Geometry> ogr_flatgeobuf::writeGeometryCollection(FlatBufferBuilder &fbb, OGRGeometryCollection *ogrGC, GeometryWriteContext &gc)
{
    std::vector<Offset<Geometry>> parts;
    for (int i = 0; i < ogrGC->getNumGeometries(); i++) {
        auto part = ogrGC->getGeometryRef(i);
        auto eGType = part->getGeometryType();
        auto geometryType = translateOGRwkbGeometryType(eGType);
        GeometryWriteContext gcPart { geometryType, gc.hasZ, gc.hasM };
        parts.push_back(writeGeometry(fbb, part, gcPart));
    }
    const auto geometryOffset = CreateGeometryDirect(fbb, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, gc.geometryType, &parts);
    return geometryOffset;
}

const Offset<Geometry> ogr_flatgeobuf::writeGeometry(
    FlatBufferBuilder &fbb,
    OGRGeometry *ogrGeometry, GeometryWriteContext &gc)
{
    switch (gc.geometryType) {
        case GeometryType::MultiPolygon:
            return writeMultiPolygon(fbb, ogrGeometry->toMultiPolygon(), gc);
        case GeometryType::GeometryCollection:
            return writeGeometryCollection(fbb, ogrGeometry->toGeometryCollection(), gc);
        case GeometryType::CompoundCurve:
            return writeCompoundCurve(fbb, ogrGeometry->toCompoundCurve(), gc);
        default:
            break;
    }

    std::vector<Offset<Geometry>> parts;
    // TODO: see about generalizing this...
    switch (gc.geometryType) {
        case GeometryType::Point:
            writePoint(ogrGeometry->toPoint(), gc);
            break;
        case GeometryType::MultiPoint:
            writeMultiPoint(ogrGeometry->toMultiPoint(), gc);
            break;
        case GeometryType::LineString:
            writeSimpleCurve(ogrGeometry->toLineString(), gc);
            break;
        case GeometryType::MultiLineString:
            writeMultiLineString(ogrGeometry->toMultiLineString(), gc);
            break;
        case GeometryType::Polygon:
            writePolygon(ogrGeometry->toPolygon(), gc, false, 0);
            break;
        case GeometryType::CircularString:
            writeSimpleCurve(ogrGeometry->toCircularString(), gc);
            break;
        case GeometryType::PolyhedralSurface:
            // TODO: implement
            break;
        case GeometryType::Triangle:
            writePolygon(ogrGeometry->toTriangle(), gc, false, 0);
            break;
        case GeometryType::TIN:
            // TODO: implement
            break;
        default:
            CPLError(CE_Failure, CPLE_AppDefined, "ICreateFeature: Unknown FlatGeobuf::GeometryType %d", (int) gc.geometryType);
            return 0;
    }
    const auto pEnds = gc.ends.empty() ? nullptr : &gc.ends;
    const auto pXy = gc.xy.empty() ? nullptr : &gc.xy;
    const auto pZ = gc.z.empty() ? nullptr : &gc.z;
    const auto pM = gc.m.empty() ? nullptr : &gc.m;
    const auto geometryOffset = FlatGeobuf::CreateGeometryDirect(fbb, pEnds, pXy, pZ, pM, nullptr, nullptr, gc.geometryType, &parts);
    return geometryOffset;
}
