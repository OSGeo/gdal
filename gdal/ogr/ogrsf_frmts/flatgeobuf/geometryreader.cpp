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

#include "geometryreader.h"
#include "cplerrors.h"
#include "ogr_flatgeobuf.h"

using namespace flatbuffers;
using namespace FlatGeobuf;
using namespace ogr_flatgeobuf;

static std::nullptr_t CPLErrorInvalidLength(const char *message) {
    CPLError(CE_Failure, CPLE_AppDefined, "Invalid length detected: %s", message);
    return nullptr;
};

OGRPoint *GeometryReader::readPoint()
{
    const auto xy = m_geometry->xy();
    const auto offsetXy = m_offset * 2;
    const auto aXy = xy->data();
    if (offsetXy >= xy->size())
        return CPLErrorInvalidLength("XY data");
    if (m_hasZ) {
        const auto z = m_geometry->z();
        if (z == nullptr)
            return CPLErrorInvalidPointer("Z data");
        if (m_offset >= z->size())
            return CPLErrorInvalidLength("Z data");
        const auto aZ = z->data();
        if (m_hasM) {
            const auto pM = m_geometry->m();
            if (pM == nullptr)
                return CPLErrorInvalidPointer("M data");
            if (m_offset >= pM->size())
                return CPLErrorInvalidLength("M data");
            const auto aM = pM->data();
            return new OGRPoint { flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                                  flatbuffers::EndianScalar(aXy[offsetXy + 1]),
                                  flatbuffers::EndianScalar(aZ[m_offset]),
                                  flatbuffers::EndianScalar(aM[m_offset]) };
        } else {
            return new OGRPoint { flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                                  flatbuffers::EndianScalar(aXy[offsetXy + 1]),
                                  flatbuffers::EndianScalar(aZ[m_offset]) };
        }
    } else if (m_hasM) {
        const auto pM = m_geometry->m();
        if (pM == nullptr)
            return CPLErrorInvalidPointer("M data");
        if (m_offset >= pM->size())
            return CPLErrorInvalidLength("M data");
        const auto aM = pM->data();
        return OGRPoint::createXYM( flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                                    flatbuffers::EndianScalar(aXy[offsetXy + 1]),
                                    flatbuffers::EndianScalar(aM[m_offset]) );
    } else {
        return new OGRPoint { flatbuffers::EndianScalar(aXy[offsetXy + 0]),
                              flatbuffers::EndianScalar(aXy[offsetXy + 1]) };
    }
}

OGRMultiPoint *GeometryReader::readMultiPoint()
{
    m_length = m_length / 2;
    if (m_length >= feature_max_buffer_size)
        return CPLErrorInvalidLength("MultiPoint");
    const auto mp = new OGRMultiPoint();
    for (uint32_t i = 0; i < m_length; i++) {
        m_offset = i;
        const auto p = readPoint();
        if (p == nullptr) {
            delete mp;
            return nullptr;
        }
        mp->addGeometryDirectly(p);
    }

    return mp;
}

OGRMultiLineString *GeometryReader::readMultiLineString()
{
    const auto pEnds = m_geometry->ends();
    if (pEnds == nullptr)
        return CPLErrorInvalidPointer("MultiLineString ends data");
    const auto mls = new OGRMultiLineString();
    m_offset = 0;
    for (uint32_t i = 0; i < pEnds->size(); i++) {
        const auto e = pEnds->Get(i);
        if (e < m_offset) {
            delete mls;
            return CPLErrorInvalidLength("MultiLineString");
        }
        m_length = e - m_offset;
        const auto ls = readSimpleCurve<OGRLineString>();
        if (ls == nullptr) {
            delete mls;
            return nullptr;
        }
        mls->addGeometryDirectly(ls);
        m_offset = e;
    }
    return mls;
}

OGRErr GeometryReader::readSimpleCurve(OGRSimpleCurve *sc)
{
    if (m_offset > feature_max_buffer_size || m_length > feature_max_buffer_size - m_offset)
        return CPLErrorInvalidSize("curve offset max");
    const uint32_t offsetLen = m_length + m_offset;
    if (offsetLen > m_geometry->xy()->size() / 2)
        return CPLErrorInvalidSize("curve XY offset");
    const auto aXy = m_geometry->xy()->data();
    const auto ogrXY = reinterpret_cast<const OGRRawPoint *>(aXy) + m_offset;
    if (m_hasZ) {
        const auto pZ = m_geometry->z();
        if (pZ == nullptr) {
            CPLErrorInvalidPointer("Z data");
            return OGRERR_CORRUPT_DATA;
        }
        if (offsetLen > pZ->size())
            return CPLErrorInvalidSize("curve Z offset");
        const auto aZ = pZ->data();
        if (m_hasM) {
            const auto pM = m_geometry->m();
            if (pM == nullptr) {
                CPLErrorInvalidPointer("M data");
                return OGRERR_CORRUPT_DATA;
            }
            if (offsetLen > pM->size())
                return CPLErrorInvalidSize("curve M offset");
            const auto aM = pM->data();
#if CPL_IS_LSB
            sc->setPoints(m_length, ogrXY, aZ + m_offset, aM + m_offset);
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
            sc->setPoints(m_length, ogrXY, aZ + m_offset);
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
    } else if (m_hasM) {
        const auto pM = m_geometry->m();
        if (pM == nullptr) {
            CPLErrorInvalidPointer("M data");
            return OGRERR_CORRUPT_DATA;
        }
        if (offsetLen > pM->size())
            return CPLErrorInvalidSize("curve M offset");
        const auto aM = pM->data();
#if CPL_IS_LSB
        sc->setPointsM(m_length, ogrXY, aM + m_offset);
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
        sc->setPoints(m_length, ogrXY);
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

OGRPolygon *GeometryReader::readPolygon()
{
    const auto pEnds = m_geometry->ends();
    const auto p = new OGRPolygon();
    if (pEnds == nullptr || pEnds->size() < 2) {
        m_length = m_length / 2;
        const auto lr = readSimpleCurve<OGRLinearRing>();
        if (lr == nullptr) {
            delete p;
            return nullptr;
        }
        p->addRingDirectly(lr);
    } else {
        for (uint32_t i = 0; i < pEnds->size(); i++) {
            const auto e = pEnds->Get(i);
            if (e < m_offset) {
                delete p;
                return CPLErrorInvalidLength("Polygon");
            }
            m_length = e - m_offset;
            const auto lr = readSimpleCurve<OGRLinearRing>();
            m_offset = e;
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

OGRMultiPolygon *GeometryReader::readMultiPolygon()
{
    auto parts = m_geometry->parts();
    auto partsLength = parts->Length();
    const auto mp = new OGRMultiPolygon();
    for (uoffset_t i = 0; i < partsLength; i++) {
        auto part = parts->Get(i);
        GeometryReader reader { part, GeometryType::Polygon, m_hasZ, m_hasM };
        auto p = reader.read()->toPolygon();
        mp->addGeometry(p);
    }
    return mp;
}

OGRGeometryCollection *GeometryReader::readGeometryCollection()
{
    auto parts = m_geometry->parts();
    auto gc = new OGRGeometryCollection();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        auto part = parts->Get(i);
        GeometryReader reader { part, m_hasZ, m_hasM };
        auto poOGRGeometryPart = reader.read();
        gc->addGeometryDirectly(poOGRGeometryPart);
    }
    return gc;
}

OGRCompoundCurve *GeometryReader::readCompoundCurve()
{
    auto parts = m_geometry->parts();
    auto cc = new OGRCompoundCurve();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        auto part = parts->Get(i);
        GeometryReader reader { part, m_hasZ, m_hasM };
        auto poOGRGeometryPart = reader.read();
        cc->addCurveDirectly(poOGRGeometryPart->toCurve());
    }
    return cc;
}

OGRCurvePolygon *GeometryReader::readCurvePolygon()
{
    auto parts = m_geometry->parts();
    auto cp = new OGRCurvePolygon();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        auto part = parts->Get(i);
        GeometryReader reader { part, m_hasZ, m_hasM };
        cp->addRingDirectly(reader.read()->toCurve());
    }
    return cp;
}

OGRMultiCurve *GeometryReader::readMultiCurve()
{
    auto parts = m_geometry->parts();
    auto mc = new OGRMultiCurve();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        auto part = parts->Get(i);
        GeometryReader reader { part, m_hasZ, m_hasM };
        auto poOGRGeometryPart = reader.read();
        mc->addGeometryDirectly(poOGRGeometryPart);
    }
    return mc;
}

OGRMultiSurface *GeometryReader::readMultiSurface()
{
    auto parts = m_geometry->parts();
    auto ms = new OGRMultiSurface();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        auto part = parts->Get(i);
        GeometryReader reader { part, m_hasZ, m_hasM };
        auto poOGRGeometryPart = reader.read();
        ms->addGeometryDirectly(poOGRGeometryPart);
    }
    return ms;
}

OGRTriangle *GeometryReader::readTriangle()
{
    const auto t = new OGRTriangle();
    m_length = m_length / 2;
    const auto lr = readSimpleCurve<OGRLinearRing>();
    if (lr == nullptr) {
        delete t;
        return nullptr;
    }
    t->addRingDirectly(lr);
    return t;
}

OGRGeometry *GeometryReader::read()
{
    // nested types
    switch (m_geometryType) {
        case GeometryType::GeometryCollection: return readGeometryCollection();
        case GeometryType::MultiPolygon: return readMultiPolygon();
        case GeometryType::CompoundCurve: return readCompoundCurve();
        case GeometryType::CurvePolygon: return readCurvePolygon();
        case GeometryType::MultiCurve: return readMultiCurve();
        case GeometryType::MultiSurface: return readMultiSurface();
        default: break;
    }

    // if not nested must have geometry data
    const auto pXy = m_geometry->xy();
    if (pXy == nullptr)
        return CPLErrorInvalidPointer("XY data");
    if (m_hasZ && m_geometry->z() == nullptr)
        return CPLErrorInvalidPointer("Z data");
    if (m_hasM && m_geometry->m() == nullptr)
        return CPLErrorInvalidPointer("M data");
    const auto xySize = pXy->size();
    if (xySize >= (feature_max_buffer_size / sizeof(OGRRawPoint)))
        return CPLErrorInvalidLength("XY data");
    m_length = xySize;

    switch (m_geometryType) {
        case GeometryType::Point: return readPoint();
        case GeometryType::MultiPoint: return readMultiPoint();
        case GeometryType::LineString: return readSimpleCurve<OGRLineString>(true);
        case GeometryType::MultiLineString: return readMultiLineString();
        case GeometryType::Polygon: return readPolygon();
        case GeometryType::CircularString: return readSimpleCurve<OGRCircularString>(true);
        //case GeometryType::PolyhedralSurface: return readMultiPolygon();
        //case GeometryType::TIN: return readMultiPolygon();
        case GeometryType::Triangle: return readTriangle();
        default:
            CPLError(CE_Failure, CPLE_AppDefined, "GeometryReader::read: Unknown type %d", (int) m_geometryType);
    }
    return nullptr;
}
