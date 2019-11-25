/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Geometry read functions.
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

#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include "geometryread.h"
#include "cplerrors.h"
#include "ogr_flatgeobuf.h"

using namespace flatbuffers;
using namespace FlatGeobuf;
using namespace ogr_flatgeobuf;

static std::nullptr_t CPLErrorInvalidLength(const char *message) {
    CPLError(CE_Failure, CPLE_AppDefined, "Invalid length detected: %s", message);
    return nullptr;
};

OGRPoint *ogr_flatgeobuf::readPoint(GeometryReadContext &gc)
{
    const auto xy = gc.geometry->xy();
    const auto offsetXy = gc.offset * 2;
    const auto aXy = xy->data();
    if (offsetXy >= xy->size())
        return CPLErrorInvalidLength("XY data");
    if (gc.hasZ) {
        const auto z = gc.geometry->z();
        if (z == nullptr)
            return CPLErrorInvalidPointer("Z data");
        if (gc.offset >= z->size())
            return CPLErrorInvalidLength("Z data");
        const auto aZ = z->data();
        if (gc.hasM) {
            const auto pM = gc.geometry->m();
            if (pM == nullptr)
                return CPLErrorInvalidPointer("M data");
            if (gc.offset >= pM->size())
                return CPLErrorInvalidLength("M data");
            const auto aM = pM->data();
            return new OGRPoint { flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                                  flatbuffers::EndianScalar(aXy[offsetXy + 1]),
                                  flatbuffers::EndianScalar(aZ[gc.offset]),
                                  flatbuffers::EndianScalar(aM[gc.offset]) };
        } else {
            return new OGRPoint { flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                                  flatbuffers::EndianScalar(aXy[offsetXy + 1]),
                                  flatbuffers::EndianScalar(aZ[gc.offset]) };
        }
    } else if (gc.hasM) {
        const auto pM = gc.geometry->m();
        if (pM == nullptr)
            return CPLErrorInvalidPointer("M data");
        if (gc.offset >= pM->size())
            return CPLErrorInvalidLength("M data");
        const auto aM = pM->data();
        return OGRPoint::createXYM( flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                                    flatbuffers::EndianScalar(aXy[offsetXy + 1]),
                                    flatbuffers::EndianScalar(aM[gc.offset]) );
    } else {
        return new OGRPoint { flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                              flatbuffers::EndianScalar(aXy[offsetXy + 1]) };
    }
}

OGRMultiPoint *ogr_flatgeobuf::readMultiPoint(GeometryReadContext &gc)
{
    gc.length = gc.length / 2;
    if (gc.length >= feature_max_buffer_size)
        return CPLErrorInvalidLength("MultiPoint");
    const auto mp = new OGRMultiPoint();
    for (uint32_t i = 0; i < gc.length; i++) {
        gc.offset = i;
        const auto p = readPoint(gc);
        if (p == nullptr) {
            delete mp;
            return nullptr;
        }
        mp->addGeometryDirectly(p);
    }

    return mp;
}

OGRMultiLineString *ogr_flatgeobuf::readMultiLineString(GeometryReadContext &gc)
{
    const auto pEnds = gc.geometry->ends();
    if (pEnds == nullptr)
        return CPLErrorInvalidPointer("MultiLineString ends data");
    const auto mls = new OGRMultiLineString();
    gc.offset = 0;
    for (uint32_t i = 0; i < pEnds->size(); i++) {
        const auto e = pEnds->Get(i);
        if (e < gc.offset) {
            delete mls;
            return CPLErrorInvalidLength("MultiLineString");
        }
        gc.length = e - gc.offset;
        const auto ls = readSimpleCurve<OGRLineString>(gc);
        if (ls == nullptr) {
            delete mls;
            return nullptr;
        }
        mls->addGeometryDirectly(ls);
        gc.offset = e;
    }
    return mls;
}

OGRErr ogr_flatgeobuf::readSimpleCurve(GeometryReadContext &gc, OGRSimpleCurve *sc)
{
    if (gc.offset > feature_max_buffer_size || gc.length > feature_max_buffer_size - gc.offset)
        return CPLErrorInvalidSize("curve offset max");
    const uint32_t offsetLen = gc.length + gc.offset;
    if (offsetLen > gc.geometry->xy()->size() / 2)
        return CPLErrorInvalidSize("curve XY offset");
    const auto aXy = gc.geometry->xy()->data();
    const auto ogrXY = reinterpret_cast<const OGRRawPoint *>(aXy) + gc.offset;
    if (gc.hasZ) {
        const auto pZ = gc.geometry->z();
        if (pZ == nullptr) {
            CPLErrorInvalidPointer("Z data");
            return OGRERR_CORRUPT_DATA;
        }
        if (offsetLen > pZ->size())
            return CPLErrorInvalidSize("curve Z offset");
        const auto aZ = pZ->data();
        if (gc.hasM) {
            const auto pM = gc.geometry->m();
            if (pM == nullptr) {
                CPLErrorInvalidPointer("M data");
                return OGRERR_CORRUPT_DATA;
            }
            if (offsetLen > pM->size())
                return CPLErrorInvalidSize("curve M offset");
            const auto aM = pM->data();
#if CPL_IS_LSB
            sc->setPoints(gc.length, ogrXY, aZ + gc.offset, aM + gc.offset);
#else
            sc->setNumPoints(len, false);
            for( uint32_t i = 0; i < len; i++ )
            {
                sc->setPoint(i,
                             flatbuffers::EndianScalar(ogrXY[i].x),
                             flatbuffers::EndianScalar(ogrXY[i].y),
                             flatbuffers::EndianScalar(aZ[offset + i]),
                             flatbuffers::EndianScalar(aM[offset + i]));
            }
#endif
        } else {
#if CPL_IS_LSB
            sc->setPoints(gc.length, ogrXY, aZ + gc.offset);
#else
            sc->setNumPoints(len, false);
            for( uint32_t i = 0; i < len; i++ )
            {
                sc->setPoint(i,
                             flatbuffers::EndianScalar(ogrXY[i].x),
                             flatbuffers::EndianScalar(ogrXY[i].y),
                             flatbuffers::EndianScalar(aZ[offset + i]));
            }
#endif
        }
    } else if (gc.hasM) {
        const auto pM = gc.geometry->m();
        if (pM == nullptr) {
            CPLErrorInvalidPointer("M data");
            return OGRERR_CORRUPT_DATA;
        }
        if (offsetLen > pM->size())
            return CPLErrorInvalidSize("curve M offset");
        const auto aM = pM->data();
#if CPL_IS_LSB
        sc->setPointsM(gc.length, ogrXY, aM + gc.offset);
#else
            sc->setNumPoints(len, false);
            for( uint32_t i = 0; i < len; i++ )
            {
                sc->setPointM(i,
                             flatbuffers::EndianScalar(ogrXY[i].x),
                             flatbuffers::EndianScalar(ogrXY[i].y),
                             flatbuffers::EndianScalar(aM[offset + i]));
            }
#endif
    } else {
#if CPL_IS_LSB
        sc->setPoints(gc.length, ogrXY);
#else
        sc->setNumPoints(len, false);
        for( uint32_t i = 0; i < len; i++ )
        {
            sc->setPoint(i,
                         flatbuffers::EndianScalar(ogrXY[i].x),
                         flatbuffers::EndianScalar(ogrXY[i].y));
        }
#endif
    }
    return OGRERR_NONE;
}

OGRPolygon *ogr_flatgeobuf::readPolygon(GeometryReadContext &gc)
{
    const auto pEnds = gc.geometry->ends();
    const auto p = new OGRPolygon();
    if (pEnds == nullptr || pEnds->size() < 2) {
        gc.length = gc.length / 2;
        const auto lr = readSimpleCurve<OGRLinearRing>(gc);
        if (lr == nullptr) {
            delete p;
            return nullptr;
        }
        p->addRingDirectly(lr);
    } else {
        for (uint32_t i = 0; i < pEnds->size(); i++) {
            const auto e = pEnds->Get(i);
            if (e < gc.offset) {
                delete p;
                return CPLErrorInvalidLength("Polygon");
            }
            gc.length = e - gc.offset;
            const auto lr = readSimpleCurve<OGRLinearRing>(gc);
            gc.offset = e;
            if (lr == nullptr)
                continue;
            p->addRingDirectly(lr);
        }
        if (p->IsEmpty()) {
            delete p;
            return nullptr;
        }
    }
    return p;
}

OGRMultiPolygon *ogr_flatgeobuf::readMultiPolygon(GeometryReadContext &gc)
{
    auto parts = gc.geometry->parts();
    auto partsLength = parts->Length();
    const auto mp = new OGRMultiPolygon();
    for (uoffset_t i = 0; i < partsLength; i++) {
        auto part = parts->Get(i);
        GeometryReadContext gcPart { part, GeometryType::Polygon, gc.hasZ, gc.hasM };
        auto poOGRPolygon = readGeometry(gcPart)->toPolygon();
        mp->addGeometry(poOGRPolygon);
    }
    return mp;
}

static OGRTriangle *readTriangle(GeometryReadContext &gc)
{
    const auto t = new OGRTriangle();
    gc.length = gc.length / 2;
    const auto lr = readSimpleCurve<OGRLinearRing>(gc);
    if (lr == nullptr) {
        delete t;
        return nullptr;
    }
    t->addRingDirectly(lr);
    return t;
}

OGRGeometry *ogr_flatgeobuf::readGeometry(GeometryReadContext &gc)
{
    switch (gc.geometryType) {
        case GeometryType::CompoundCurve: {
            auto parts = gc.geometry->parts();
            auto partsLength = parts->Length();
            auto compoundCurve = new OGRCompoundCurve();
            for (uoffset_t i = 0; i < partsLength; i++) {
                auto part = parts->Get(i);
                auto type = part->type();
                GeometryReadContext gcPart { part, type, gc.hasZ, gc.hasM };
                auto poOGRGeometryPart = readGeometry(gcPart);
                compoundCurve->addCurveDirectly(poOGRGeometryPart->toCurve());
            }
            return compoundCurve;
        }
        case GeometryType::GeometryCollection: {
            auto parts = gc.geometry->parts();
            auto partsLength = parts->Length();
            auto geometryCollection = new OGRGeometryCollection();
            for (uoffset_t i = 0; i < partsLength; i++) {
                auto part = parts->Get(i);
                auto type = part->type();
                GeometryReadContext gcPart { part, type, gc.hasZ, gc.hasM };
                auto poOGRGeometryPart = readGeometry(gcPart);
                geometryCollection->addGeometryDirectly(poOGRGeometryPart);
            }
            return geometryCollection;
        }
        case GeometryType::MultiPolygon: return readMultiPolygon(gc);
        default: break;
    }

    const auto pXy = gc.geometry->xy();
    if (pXy == nullptr)
        return CPLErrorInvalidPointer("XY data");
    if (gc.hasZ && gc.geometry->z() == nullptr)
        return CPLErrorInvalidPointer("Z data");
    if (gc.hasM && gc.geometry->m() == nullptr)
        return CPLErrorInvalidPointer("M data");
    const auto xySize = pXy->size();
    if (xySize >= (feature_max_buffer_size / sizeof(OGRRawPoint)))
        return CPLErrorInvalidLength("XY data");
    gc.length = xySize;
    switch (gc.geometryType) {
        case GeometryType::Point: return readPoint(gc);
        case GeometryType::MultiPoint: return readMultiPoint(gc);
        case GeometryType::LineString: return readSimpleCurve<OGRLineString>(gc, true);
        case GeometryType::MultiLineString: return readMultiLineString(gc);
        case GeometryType::Polygon: return readPolygon(gc);
        case GeometryType::CircularString: return readSimpleCurve<OGRCircularString>(gc, true);
        case GeometryType::PolyhedralSurface: return readMultiPolygon(gc);
        case GeometryType::TIN: return readMultiPolygon(gc);
        case GeometryType::Triangle: return readTriangle(gc);
        default:
            CPLError(CE_Failure, CPLE_AppDefined, "readGeometry: Unknown FlatGeobuf::GeometryType %d", (int) gc.geometryType);
    }
    return nullptr;
}
