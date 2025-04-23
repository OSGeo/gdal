/******************************************************************************
 *
 * Project:  FlatGeobuf driver
 * Purpose:  GeometryReader class declarations.
 * Author:   Björn Harrtell <bjorn at wololo dot org>
 *
 ******************************************************************************
 * Copyright (c) 2018-2019, Björn Harrtell <bjorn at wololo dot org>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef FLATGEOBUF_GEOMETRYREADER_H_INCLUDED
#define FLATGEOBUF_GEOMETRYREADER_H_INCLUDED

#include "feature_generated.h"
#include "ogr_p.h"

namespace ogr_flatgeobuf
{

class GeometryReader
{
  private:
    const FlatGeobuf::Geometry *m_geometry;
    const FlatGeobuf::GeometryType m_geometryType;
    const bool m_hasZ;
    const bool m_hasM;

    const double *m_xy = nullptr;
    uint32_t m_xylength = 0;
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

    OGRGeometry *readPart(const FlatGeobuf::Geometry *part)
    {
        return GeometryReader(part, m_hasZ, m_hasM).read();
    }

    OGRGeometry *readPart(const FlatGeobuf::Geometry *part,
                          const FlatGeobuf::GeometryType geometryType)
    {
        return GeometryReader(part, geometryType, m_hasZ, m_hasM).read();
    }

    template <class T> T *readSimpleCurve(const bool halfLength = false)
    {
        if (halfLength)
            m_length = m_length / 2;
        const auto csc = new T();
        if (readSimpleCurve(csc) != OGRERR_NONE)
        {
            delete csc;
            return nullptr;
        }
        return csc;
    }

  public:
    GeometryReader(const FlatGeobuf::Geometry *geometry,
                   const FlatGeobuf::GeometryType geometryType, const bool hasZ,
                   const bool hasM)
        : m_geometry(geometry), m_geometryType(geometryType), m_hasZ(hasZ),
          m_hasM(hasM)
    {
    }

    GeometryReader(const FlatGeobuf::Geometry *geometry, const bool hasZ,
                   const bool hasM)
        : m_geometry(geometry), m_geometryType(geometry->type()), m_hasZ(hasZ),
          m_hasM(hasM)
    {
    }

    OGRGeometry *read();
};

}  // namespace ogr_flatgeobuf

#endif /* ndef FLATGEOBUF_GEOMETRYREADER_H_INCLUDED */
