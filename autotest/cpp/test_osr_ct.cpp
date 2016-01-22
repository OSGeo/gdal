///////////////////////////////////////////////////////////////////////////////
// $Id: test_osr_ct.cpp,v 1.3 2006/12/06 15:39:13 mloskot Exp $
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test coordinate transformations. Ported from osr/osr_ct.py.
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
//  $Log: test_osr_ct.cpp,v $
//  Revision 1.3  2006/12/06 15:39:13  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////

// See Bronek Kozicki's comments posted here:
// http://lists.boost.org/Archives/boost/2005/07/89697.php
#pragma warning(disable: 4996)

#include <tut.h>
#include <tut_gdal.h>
#include <ogr_srs_api.h> // OSR
#include <ogr_api.h> // OGR
#include <cpl_error.h> // CPL
#include <algorithm>
#include <cmath>
#include <string>
    
namespace tut
{

    // Common fixture with test data
    struct test_osr_ct_data
    {
        OGRErr err_;
        OGRSpatialReferenceH srs_utm_;
        OGRSpatialReferenceH srs_ll_;
        OGRCoordinateTransformationH ct_;

        test_osr_ct_data()
            : err_(OGRERR_NONE), srs_utm_(NULL), srs_ll_(NULL), ct_(NULL)
        {
            srs_utm_ = OSRNewSpatialReference(NULL);
            srs_ll_ = OSRNewSpatialReference(NULL);
        }

        ~test_osr_ct_data()
        {
            OSRDestroySpatialReference(srs_utm_);
            OSRDestroySpatialReference(srs_ll_);
            OCTDestroyCoordinateTransformation(ct_);
        }
    };

    // Register test group
    typedef test_group<test_osr_ct_data> group;
    typedef group::object object;
    group test_osr_ct_group("OSR::CT");

    // Verify that we have PROJ.4 available
    template<>
    template<>
    void object::test<1>()
    {
        ensure("SRS UTM handle is NULL", NULL != srs_utm_);
        ensure("SRS LL handle is NULL", NULL != srs_ll_);

        err_ = OSRSetUTM(srs_utm_, 11, TRUE);
        ensure_equals("Can't set UTM zone", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_utm_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_ll_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        ct_ = OCTNewCoordinateTransformation(srs_ll_, srs_utm_);
        ensure("PROJ.4 missing, transforms not available", NULL != ct_);
    }

    // Actually perform a simple LL to UTM conversion
    template<>
    template<>
    void object::test<2>()
    {
        ensure("SRS UTM handle is NULL", NULL != srs_utm_);
        ensure("SRS LL handle is NULL", NULL != srs_ll_);

        err_ = OSRSetUTM(srs_utm_, 11, TRUE);
        ensure_equals("Can't set UTM zone", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_utm_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_ll_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        ct_ = OCTNewCoordinateTransformation(srs_ll_, srs_utm_);
        ensure("PROJ.4 missing, transforms not available", NULL != ct_);

        const int size = 1;
        double x[size] = { -117.5 };
        double y[size] = { 32.0 };
        double z[size] = { 0.0  };

        ensure_equals("OCTTransform() failed",
            OCTTransform(ct_, size, x, y, z), TRUE);

        ensure("Wrong X from LL to UTM result",
            std::fabs(x[0] - 452772.06) <= 0.01);
        ensure("Wrong Y from LL to UTM result",
            std::fabs(y[0] - 3540544.89) <= 0.01);
        ensure("Wrong Z from LL to UTM result",
            std::fabs(z[0] - 0.0) <= 0.01);
    }

    // Transform an OGR geometry.
    // This is mostly aimed at ensuring that the OGRCoordinateTransformation
    // target SRS isn't deleted till the output geometry which also
    // uses it is deleted.
    template<>
    template<>
    void object::test<3>()
    {
        ensure("SRS UTM handle is NULL", NULL != srs_utm_);
        ensure("SRS LL handle is NULL", NULL != srs_ll_);

        err_ = OSRSetUTM(srs_utm_, 11, TRUE);
        ensure_equals("Can't set UTM zone", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_utm_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_ll_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        ct_ = OCTNewCoordinateTransformation(srs_ll_, srs_utm_);
        ensure("PROJ.4 missing, transforms not available", NULL != ct_);

        const char* wkt = "POINT(-117.5 32.0)";
        OGRGeometryH geom = NULL;
        err_ = OGR_G_CreateFromWkt((char**) &wkt, NULL, &geom);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", NULL != geom);

        err_ = OGR_G_Transform(geom, ct_);
        ensure_equals("OGR_G_Transform() failed", err_, OGRERR_NONE);

        OGRSpatialReferenceH srs = NULL;
        srs = OGR_G_GetSpatialReference(geom);
        
        char* wktSrs = NULL;
        err_ = OSRExportToPrettyWkt(srs, &wktSrs, FALSE);
        ensure("Exported SRS to WKT is NULL", NULL != wktSrs);

        std::string pretty(wktSrs);
        ensure_equals("SRS output is incorrect", pretty.substr(0, 6), std::string("PROJCS"));

        OGRFree(wktSrs);
        OGR_G_DestroyGeometry(geom);
    }

} // namespace tut
