/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  Implements GeometryReader class.
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
}

OGRPoint *GeometryReader::readPoint()
{
    const auto xy = m_geometry->xy();
    if (xy == nullptr)
        return CPLErrorInvalidPointer("XY data");
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
            return new OGRPoint { EndianScalar(aXy[offsetXy + 0]),
                                  EndianScalar(aXy[offsetXy + 1]),
                                  EndianScalar(aZ[m_offset]),
                                  EndianScalar(aM[m_offset]) };
        } else {
            return new OGRPoint { EndianScalar(aXy[offsetXy + 0]),
                                  EndianScalar(aXy[offsetXy + 1]),
                                  EndianScalar(aZ[m_offset]) };
        }
    } else if (m_hasM) {
        const auto pM = m_geometry->m();
        if (pM == nullptr)
            return CPLErrorInvalidPointer("M data");
        if (m_offset >= pM->size())
            return CPLErrorInvalidLength("M data");
        const auto aM = pM->data();
        return OGRPoint::createXYM( EndianScalar(aXy[offsetXy + 0]),
                                    EndianScalar(aXy[offsetXy + 1]),
                                    EndianScalar(aM[m_offset]) );
    } else {
        return new OGRPoint { EndianScalar(aXy[offsetXy + 0]),
                              EndianScalar(aXy[offsetXy + 1]) };
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
    auto xy = m_geometry->xy();
    if (xy == nullptr) {
        CPLErrorInvalidPointer("XY data");
        return OGRERR_CORRUPT_DATA;
    }
    if (offsetLen > xy->size() / 2)
        return CPLErrorInvalidSize("curve XY offset");
    const auto aXy = xy->data();
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
            sc->setNumPoints(m_length, false);
            for( uint32_t i = 0; i < m_length; i++ )
            {
                sc->setPoint(i,
                             EndianScalar(ogrXY[i].x),
                             EndianScalar(ogrXY[i].y),
                             EndianScalar(aZ[m_offset + i]),
                             EndianScalar(aM[m_offset + i]));
            }
#endif
        } else {
#if CPL_IS_LSB
            sc->setPoints(m_length, ogrXY, aZ + m_offset);
#else
            sc->setNumPoints(m_length, false);
            for( uint32_t i = 0; i < m_length; i++ )
            {
                sc->setPoint(i,
                             EndianScalar(ogrXY[i].x),
                             EndianScalar(ogrXY[i].y),
                             EndianScalar(aZ[m_offset + i]));
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
        sc->setNumPoints(m_length, false);
        for( uint32_t i = 0; i < m_length; i++ )
        {
            sc->setPointM(i,
                            EndianScalar(ogrXY[i].x),
                            EndianScalar(ogrXY[i].y),
                            EndianScalar(aM[m_offset + i]));
        }
#endif
    } else {
#if CPL_IS_LSB
        sc->setPoints(m_length, ogrXY);
#else
        sc->setNumPoints(m_length, false);
        for( uint32_t i = 0; i < m_length; i++ )
        {
            sc->setPoint(i,
                         EndianScalar(ogrXY[i].x),
                         EndianScalar(ogrXY[i].y));
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
        GeometryReader reader { parts->Get(i), GeometryType::Polygon, m_hasZ, m_hasM };
        mp->addGeometry(reader.read()->toPolygon());
    }
    return mp;
}

OGRGeometryCollection *GeometryReader::readGeometryCollection()
{
    auto parts = m_geometry->parts();
    auto gc = new OGRGeometryCollection();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        GeometryReader reader { parts->Get(i), m_hasZ, m_hasM };
        gc->addGeometryDirectly(reader.read());
    }
    return gc;
}

OGRCompoundCurve *GeometryReader::readCompoundCurve()
{
    auto parts = m_geometry->parts();
    auto cc = new OGRCompoundCurve();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        GeometryReader reader { parts->Get(i), m_hasZ, m_hasM };
        cc->addCurveDirectly(reader.read()->toCurve());
    }
    return cc;
}

OGRCurvePolygon *GeometryReader::readCurvePolygon()
{
    auto parts = m_geometry->parts();
    auto cp = new OGRCurvePolygon();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        GeometryReader reader { parts->Get(i), m_hasZ, m_hasM };
        cp->addRingDirectly(reader.read()->toCurve());
    }
    return cp;
}

OGRMultiCurve *GeometryReader::readMultiCurve()
{
    auto parts = m_geometry->parts();
    auto mc = new OGRMultiCurve();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        GeometryReader reader { parts->Get(i), m_hasZ, m_hasM };
        mc->addGeometryDirectly(reader.read());
    }
    return mc;
}

OGRMultiSurface *GeometryReader::readMultiSurface()
{
    auto parts = m_geometry->parts();
    auto ms = new OGRMultiSurface();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        GeometryReader reader { parts->Get(i), m_hasZ, m_hasM };
        ms->addGeometryDirectly(reader.read());
    }
    return ms;
}

OGRPolyhedralSurface *GeometryReader::readPolyhedralSurface()
{
    auto parts = m_geometry->parts();
    auto ps = new OGRPolyhedralSurface();
    for (uoffset_t i = 0; i < parts->Length(); i++) {
        GeometryReader reader { parts->Get(i), m_hasZ, m_hasM };
        ps->addGeometryDirectly(reader.read());
    }
    return ps;
}

OGRTriangulatedSurface *GeometryReader::readTIN()
{
    const auto pEnds = m_geometry->ends();
    const auto ts = new OGRTriangulatedSurface();
    if (pEnds == nullptr || pEnds->size() < 2) {
        m_length = m_length / 2;
        const auto lr = readSimpleCurve<OGRLinearRing>();
        if (lr == nullptr) {
            delete ts;
            return nullptr;
        }
        auto t = new OGRTriangle();
        t->addRingDirectly(lr);
        ts->addGeometryDirectly(t);
    } else {
        for (uint32_t i = 0; i < pEnds->size(); i++) {
            const auto e = pEnds->Get(i);
            if (e < m_offset) {
                delete ts;
                return CPLErrorInvalidLength("TIN");
            }
            m_length = e - m_offset;
            const auto lr = readSimpleCurve<OGRLinearRing>();
            m_offset = e;
            if (lr == nullptr)
                continue;
            auto t = new OGRTriangle();
            t->addRingDirectly(lr);
            ts->addGeometryDirectly(t);
        }
        if (ts->IsEmpty()) {
            delete ts;
            return nullptr;
        }
    }
    return ts;
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
        case GeometryType::PolyhedralSurface: return readPolyhedralSurface();
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
        case GeometryType::Triangle: return readTriangle();
        case GeometryType::TIN: return readTIN();
        default:
            CPLError(CE_Failure, CPLE_AppDefined, "GeometryReader::read: Unknown type %d", (int) m_geometryType);
    }
    return nullptr;
}
