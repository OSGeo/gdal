///////////////////////////////////////////////////////////////////////////////
// $Id: tut_gdal.cpp,v 1.3 2006/12/06 15:39:13 mloskot Exp $
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  TUT: C++ Unit Test Framework extensions for GDAL Test Suite
// Author:   Mateusz Loskot <mateusz@loskot.net>
// 
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
//  
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
// 
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
///////////////////////////////////////////////////////////////////////////////
//
//  $Log: tut_gdal.cpp,v $
//  Revision 1.3  2006/12/06 15:39:13  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////
#include <tut.h> // TUT
#include <tut_gdal.h>
#include <ogr_api.h> // GDAL
#include <algorithm> // C++
#include <cmath>
#include <sstream>
#include <string>

namespace tut
{

void ensure_equal_geometries(OGRGeometryH lhs, OGRGeometryH rhs, double tolerance)
{
    // Test raw pointers
    ensure("First geometry is NULL", NULL != lhs);
    ensure("Second geometry is NULL", NULL != rhs);
    ensure("Passed the same pointers to geometry", lhs != rhs);

    // Test basic properties
    ensure_equals("Geometry names do not match",
        std::string(OGR_G_GetGeometryName(lhs)), std::string(OGR_G_GetGeometryName(rhs)));

    ensure_equals("Sub-geometry counts do not match",
        OGR_G_GetGeometryCount(lhs), OGR_G_GetGeometryCount(rhs));

    ensure_equals("Point counts do not match",
        OGR_G_GetPointCount(lhs), OGR_G_GetPointCount(rhs));

    if (OGR_G_GetGeometryCount(lhs) > 0)
    {
        // Test sub-geometries recursively
        const int count = OGR_G_GetGeometryCount(lhs);
        for (int i = 0; i < count; ++i)
        {
            ensure_equal_geometries(OGR_G_GetGeometryRef(lhs, i),
                                    OGR_G_GetGeometryRef(rhs, i),
                                    tolerance);
        }
    }
    else
    {
        // Test geometry points
        const std::size_t csize = 3;
        double a[csize] = { 0 };
        double b[csize] = { 0 };
        double d[csize] = { 0 };
        double dmax = 0;

        const int count = OGR_G_GetPointCount(lhs);
        for (int i = 0; i < count; ++i)
        {
            OGR_G_GetPoint(lhs, i, &a[0], &a[1], &a[2]);
            OGR_G_GetPoint(rhs, i, &b[0], &b[1], &b[2]);

            // Test vertices
            for (std::size_t c = 0; c < csize; ++c)
            {
                d[c] = std::fabs(a[c] - b[c]);
            }

            const double* pos = std::max_element(d, d + csize);
            dmax = *pos;

            std::ostringstream os;
            os << "Error in vertex " << i << " off by " << dmax;

            ensure(os.str(), dmax < tolerance);
        }
    }
}

} // } // namespace tut
