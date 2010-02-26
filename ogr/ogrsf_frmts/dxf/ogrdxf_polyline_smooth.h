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


#ifndef __OGRDXF_SMOOTH_POLYLINE_H__
#define __OGRDXF_SMOOTH_POLYLINE_H__

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include <vector>
#include "assert.h"

#ifndef M_PI
    #define M_PI        3.14159265358979323846  /* pi */
#endif


class DXFSmoothPolylineVertex
{
    public:
        double  x, y, z, bulge;


        DXFSmoothPolylineVertex()
        {
            x = y = z = bulge = 0.0;
        }


        DXFSmoothPolylineVertex(double dfX, double dfY, double dfZ, double dfBulge)
        {
            this->set(dfX, dfY, dfZ, dfBulge);
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
            return (sqrt(x*x + y*y));
        }


        void normalize()
        {
            const double len = this->length();
            assert(len != 0.0);

            x /= len;
            y /= len;
        }


        bool shares_2D_pos(const DXFSmoothPolylineVertex& v) const
        {
            return (x == v.x && y == v.y);
        }

};


class DXFSmoothPolyline
{
    // A DXF polyline that includes vertex bulge information.
    // Call Tesselate() to convert to an OGRGeometry.
    // We treat Z as constant over the entire string; this may
    // change in the future.

    private:

        std::vector<DXFSmoothPolylineVertex>    m_vertices;
        mutable bool                            m_blinestringstarted;
        bool                                    m_bClosed;
		int										m_dim;

       
    public:
        DXFSmoothPolyline()
        {
            m_bClosed = false;
			m_dim = 2;
        }

        OGRGeometry* Tesselate() const;

        void SetSize(int n) { m_vertices.reserve(n); }

        void AddPoint(double dfX, double dfY, double dfZ, double dfBulge)
        {
            m_vertices.push_back(DXFSmoothPolylineVertex(dfX, dfY, dfZ, dfBulge));
        }

        void Close();

        bool IsEmpty() const { return m_vertices.empty(); }

        bool HasConstantZ(double&) const;

	    void setCoordinateDimension(int n) { m_dim = n; }



    private:
        void EmitArc(const DXFSmoothPolylineVertex&, const DXFSmoothPolylineVertex&,
                double radius, double len, double saggita, 
                OGRLineString*, double dfZ = 0.0) const;

        void EmitLine(const DXFSmoothPolylineVertex&, const DXFSmoothPolylineVertex&, 
            OGRLineString*, bool bConstantZ, double dfZ) const;
};

#endif  /* __OGRDXF_SMOOTH_POLYLINE_H__ */

