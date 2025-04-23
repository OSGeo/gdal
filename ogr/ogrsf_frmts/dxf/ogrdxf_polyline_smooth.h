/******************************************************************************
 * File:   ogrdxf_polyline_smooth.h
 *
 * Project:  Interpolation support for smooth POLYLINE and LWPOLYLINE entities.
 * Purpose:  Definition of classes for OGR .dxf driver.
 * Author:   TJ Snider, timsn@thtree.com
 *           Ray Gardener, Daylon Graphics Ltd.
 *
 ******************************************************************************
 * Copyright (c) 2010 Daylon Graphics Ltd.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRDXF_SMOOTH_POLYLINE_H_INCLUDED
#define OGRDXF_SMOOTH_POLYLINE_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include <vector>
#include "assert.h"

class DXFSmoothPolylineVertex
{
  public:
    double x;
    double y;
    double z;
    double bulge;

    DXFSmoothPolylineVertex()
    {
        x = y = z = bulge = 0.0;
    }

    DXFSmoothPolylineVertex(double dfX, double dfY, double dfZ, double dfBulge)
    {
        set(dfX, dfY, dfZ, dfBulge);
    }

    void set(double dfX, double dfY, double dfZ, double dfBulge)
    {
        x = dfX;
        y = dfY;
        z = dfZ;
        bulge = dfBulge;
    }

    void scale(double s)
    {
        x *= s;
        y *= s;
    }

    double length() const
    {
        return (sqrt(x * x + y * y));
    }

    void normalize()
    {
        const double len = length();
        assert(len != 0.0);

        x /= len;
        y /= len;
    }

    bool shares_2D_pos(const DXFSmoothPolylineVertex &v) const
    {
        return (x == v.x && y == v.y);
    }
};

// Quiet warning from gcc (possibly https://gcc.gnu.org/bugzilla/show_bug.cgi?id=112370)
#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif
class DXFSmoothPolyline
{
    // A DXF polyline that includes vertex bulge information.
    // Call Tessellate() to convert to an OGRGeometry.
    // We treat Z as constant over the entire string; this may
    // change in the future.

  private:
    std::vector<DXFSmoothPolylineVertex> m_vertices;
    mutable bool m_blinestringstarted;
    bool m_bClosed;
    int m_dim;
    bool m_bUseMaxGapWhenTessellatingArcs;

  public:
    DXFSmoothPolyline()
        : m_blinestringstarted(false), m_bClosed(false), m_dim(2),
          m_bUseMaxGapWhenTessellatingArcs(false)
    {
    }

    OGRGeometry *Tessellate(bool bAsPolygon) const;

    size_t size() const
    {
        return m_vertices.size();
    }

    void SetSize(int n)
    {
        m_vertices.reserve(n);
    }

    void AddPoint(double dfX, double dfY, double dfZ, double dfBulge)
    {
        m_vertices.push_back(DXFSmoothPolylineVertex(dfX, dfY, dfZ, dfBulge));
    }

    void Close();

    bool IsEmpty() const
    {
        return m_vertices.empty();
    }

    void setCoordinateDimension(int n)
    {
        m_dim = n;
    }

    void SetUseMaxGapWhenTessellatingArcs(bool bVal)
    {
        m_bUseMaxGapWhenTessellatingArcs = bVal;
    }

  private:
    void EmitArc(const DXFSmoothPolylineVertex &,
                 const DXFSmoothPolylineVertex &, double radius, double len,
                 double saggita, OGRLineString *, double dfZ = 0.0) const;

    void EmitLine(const DXFSmoothPolylineVertex &,
                  const DXFSmoothPolylineVertex &, OGRLineString *) const;
};
#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic pop
#endif

#endif /* OGRDXF_SMOOTH_POLYLINE_H_INCLUDED */
