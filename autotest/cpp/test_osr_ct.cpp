///////////////////////////////////////////////////////////////////////////////
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

#include "gdal_unit_test.h"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"

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
            : err_(OGRERR_NONE), srs_utm_(nullptr), srs_ll_(nullptr), ct_(nullptr)
        {
            srs_utm_ = OSRNewSpatialReference(nullptr);
            srs_ll_ = OSRNewSpatialReference(nullptr);
            OSRSetAxisMappingStrategy(srs_utm_, OAMS_TRADITIONAL_GIS_ORDER);
            OSRSetAxisMappingStrategy(srs_ll_, OAMS_TRADITIONAL_GIS_ORDER);
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
        ensure("SRS UTM handle is NULL", nullptr != srs_utm_);
        ensure("SRS LL handle is NULL", nullptr != srs_ll_);

        err_ = OSRSetUTM(srs_utm_, 11, TRUE);
        ensure_equals("Can't set UTM zone", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_utm_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_ll_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        ct_ = OCTNewCoordinateTransformation(srs_ll_, srs_utm_);
        ensure("PROJ.4 missing, transforms not available", nullptr != ct_);
    }

    // Actually perform a simple LL to UTM conversion
    template<>
    template<>
    void object::test<2>()
    {
        ensure("SRS UTM handle is NULL", nullptr != srs_utm_);
        ensure("SRS LL handle is NULL", nullptr != srs_ll_);

        err_ = OSRSetUTM(srs_utm_, 11, TRUE);
        ensure_equals("Can't set UTM zone", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_utm_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_ll_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        ct_ = OCTNewCoordinateTransformation(srs_ll_, srs_utm_);
        ensure("PROJ.4 missing, transforms not available", nullptr != ct_);

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
        ensure("SRS UTM handle is NULL", nullptr != srs_utm_);
        ensure("SRS LL handle is NULL", nullptr != srs_ll_);

        err_ = OSRSetUTM(srs_utm_, 11, TRUE);
        ensure_equals("Can't set UTM zone", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_utm_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        err_ = OSRSetWellKnownGeogCS(srs_ll_, "WGS84");
        ensure_equals("Can't set GeogCS", err_, OGRERR_NONE);

        ct_ = OCTNewCoordinateTransformation(srs_ll_, srs_utm_);
        ensure("PROJ.4 missing, transforms not available", nullptr != ct_);

        const char* wkt = "POINT(-117.5 32.0)";
        OGRGeometryH geom = nullptr;
        err_ = OGR_G_CreateFromWkt((char**) &wkt, nullptr, &geom);
        ensure_equals("Can't import geometry from WKT", OGRERR_NONE, err_);
        ensure("Can't create geometry", nullptr != geom);

        err_ = OGR_G_Transform(geom, ct_);
        ensure_equals("OGR_G_Transform() failed", err_, OGRERR_NONE);

        OGRSpatialReferenceH srs = nullptr;
        srs = OGR_G_GetSpatialReference(geom);

        char* wktSrs = nullptr;
        err_ = OSRExportToPrettyWkt(srs, &wktSrs, FALSE);
        ensure("Exported SRS to WKT is NULL", nullptr != wktSrs);

        std::string pretty(wktSrs);
        ensure_equals("SRS output is incorrect", pretty.substr(0, 6), std::string("PROJCS"));

        CPLFree(wktSrs);
        OGR_G_DestroyGeometry(geom);
    }

    // Test OGRCoordinateTransformation::GetInverse()
    template<>
    template<>
    void object::test<4>()
    {
        OGRSpatialReference oSRSSource;
        oSRSSource.importFromEPSG(4267);

        OGRSpatialReference oSRSTarget;
        oSRSTarget.importFromEPSG(4269);

        auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
            OGRCreateCoordinateTransformation(&oSRSSource, &oSRSTarget));
        ensure( poCT != nullptr );
        ensure( poCT->GetSourceCS() != nullptr );
        ensure( poCT->GetSourceCS()->IsSame(&oSRSSource) );
        ensure( poCT->GetTargetCS() != nullptr );
        ensure( poCT->GetTargetCS()->IsSame(&oSRSTarget) );

        auto poInverse = std::unique_ptr<OGRCoordinateTransformation>(
            poCT->GetInverse());
        ensure( poInverse != nullptr );
        ensure( poInverse->GetSourceCS() != nullptr );
        ensure( poInverse->GetSourceCS()->IsSame(&oSRSTarget) );
        ensure( poInverse->GetTargetCS() != nullptr );
        ensure( poInverse->GetTargetCS()->IsSame(&oSRSSource) );

        double x = 44;
        double y = -60;
        ensure( poCT->Transform(1, &x, &y) );
        // Check that the transformed point is different but not too far
        ensure( fabs(x - 44) > 1e-10 );
        ensure( fabs(y - -60) > 1e-10 );
        ensure( fabs(x - 44) < 1e-3 );
        ensure( fabs(y - -60) < 1e-3 );
        const double xTransformed = x;
        const double yTransformed = y;

        poCT.reset();

        // Check that the transformed point with the inverse transformation
        // matches the source
        ensure( poInverse->Transform(1, &x, &y) );
        ensure_approx_equals( x, 44.0 );
        ensure_approx_equals( y, -60.0 );

        auto poInvOfInv =std::unique_ptr<OGRCoordinateTransformation>(
            poInverse->GetInverse());
        ensure( poInvOfInv != nullptr );
        ensure( poInvOfInv->GetSourceCS() != nullptr );
        ensure( poInvOfInv->GetSourceCS()->IsSame(&oSRSSource) );
        ensure( poInvOfInv->GetTargetCS() != nullptr );
        ensure( poInvOfInv->GetTargetCS()->IsSame(&oSRSTarget) );
        ensure( poInvOfInv->Transform(1, &x, &y) );
        // Check that the transformed point is different but not too far
        ensure_approx_equals( x, xTransformed );
        ensure_approx_equals( y, yTransformed );
    }

    // Test OGRCoordinateTransformation::GetInverse() with a specified coordinate operation
    template<>
    template<>
    void object::test<5>()
    {
        OGRCoordinateTransformationOptions options;
        options.SetCoordinateOperation("+proj=affine +xoff=10", false);
        auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
            OGRCreateCoordinateTransformation(nullptr, nullptr, options));
        ensure( poCT != nullptr );

        auto poInverse = std::unique_ptr<OGRCoordinateTransformation>(
            poCT->GetInverse());
        ensure( poInverse != nullptr );
        ensure( poInverse->GetSourceCS() == nullptr );
        ensure( poInverse->GetTargetCS() == nullptr );

        poCT.reset();

        double x = 100;
        double y = 200;
        ensure( poInverse->Transform(1, &x, &y) );
        ensure_approx_equals( x, 90.0 );
        ensure_approx_equals( y, 200.0 );
    }

    // Test OGRCoordinateTransformation::Clone()
    static void test_clone(OGRCoordinateTransformation* poCT,
                           OGRSpatialReference* poSRSSource,
                           OGRSpatialReference* poSRSTarget,
                           const double xSrc, const double ySrc)
    {
        ensure(poCT != nullptr);
        ensure((poCT->GetSourceCS() == nullptr) ==
               (poSRSSource == nullptr) );
        if(poSRSSource != nullptr)
        {
            ensure(poCT->GetSourceCS()->IsSame(poSRSSource));
        }
        ensure((poCT->GetTargetCS() == nullptr) ==
               (poSRSTarget == nullptr));
        if(poSRSTarget != nullptr)
        {
            ensure(poCT->GetTargetCS()->IsSame(poSRSTarget));
        }
        double x = xSrc;
        double y = ySrc;
        ensure(poCT->Transform(1, &x, &y));
        const double xTransformed = x;
        const double yTransformed = y;

        auto poClone =std::unique_ptr<OGRCoordinateTransformation>(
            poCT->Clone());
        ensure(poClone != nullptr );
        ensure((poClone->GetSourceCS() == nullptr) ==
               (poSRSSource == nullptr));
        if(poSRSSource != nullptr)
        {
            ensure(poClone->GetSourceCS()->IsSame(poSRSSource));
        }
        ensure((poClone->GetTargetCS() == nullptr) ==
               (poSRSTarget == nullptr));
        if(poSRSTarget != nullptr)
        {
            ensure(poClone->GetTargetCS()->IsSame(poSRSTarget));
        }
        x = xSrc;
        y = ySrc;
        ensure(poClone->Transform(1, &x, &y));
        ensure(fabs(x - xTransformed) < 1e-15);
        ensure(fabs(y - yTransformed) < 1e-15);
    }

    // Test OGRCoordinateTransformation::Clone() with usual case
    template<>
    template<>
    void object::test<6>()
    {
        OGRSpatialReference oSRSSource;
        oSRSSource.importFromEPSG(4267);
        oSRSSource.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRSpatialReference oSRSTarget;
        oSRSTarget.importFromEPSG(4269);
        oSRSTarget.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
            OGRCreateCoordinateTransformation(&oSRSSource, &oSRSTarget));

        test_clone(poCT.get(), &oSRSSource, &oSRSTarget, 44, -60);
    }

    // Test OGRCoordinateTransformation::Clone() with a specified coordinate operation
    template<>
    template<>
    void object::test<7>()
    {
        OGRCoordinateTransformationOptions options;
        options.SetCoordinateOperation("+proj=affine +xoff=10", false);
        auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
            OGRCreateCoordinateTransformation(nullptr, nullptr, options));

        test_clone(poCT.get(), nullptr, nullptr, 90, 200);
    }
    // Test OGRCoordinateTransformation::Clone() with WebMercator->WGS84 special case
    template<>
    template<>
    void object::test<8>()
    {
        OGRSpatialReference oSRSSource;
        oSRSSource.importFromEPSG(3857);
        oSRSSource.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRSpatialReference oSRSTarget;
        oSRSTarget.SetWellKnownGeogCS("WGS84");
        oSRSTarget.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
            OGRCreateCoordinateTransformation(&oSRSSource, &oSRSTarget));

        test_clone(poCT.get(), &oSRSSource, &oSRSTarget, 44, -60);
    }

    // Test OGRCoordinateTransformation in pure "C" API
    // OCTClone/OCTGetSourceCS/OCTGetTargetCS/OCTGetInverse
    template<>
    template<>
    void object::test<9>()
    {
        OGRSpatialReferenceH hSource = OSRNewSpatialReference(nullptr);
        OGRSpatialReferenceH hTarget = OSRNewSpatialReference(nullptr);
        ensure(hSource != nullptr);
        ensure(hTarget != nullptr);
        ensure(OGRERR_NONE == OSRImportFromEPSG(hSource, 32637));
        ensure(OGRERR_NONE == OSRSetWellKnownGeogCS(hTarget, "WGS84"));
        OGRCoordinateTransformationH hTransform =
            OCTNewCoordinateTransformation(hSource, hTarget);
        ensure(hTransform != nullptr);

        OGRCoordinateTransformationH hClone = OCTClone(hTransform);
        ensure(hClone != nullptr);

        OGRCoordinateTransformationH hInvTransform =
            OCTGetInverse(hTransform);
        ensure(hInvTransform != nullptr);

        OGRSpatialReferenceH hSourceInternal = OCTGetSourceCS(hTransform);
        ensure(hSourceInternal != nullptr);
        OGRSpatialReferenceH hTargetInternal = OCTGetTargetCS(hTransform);
        ensure(hTargetInternal != nullptr);

        ensure(OSRIsSame(hSource, hSourceInternal));
        ensure(OSRIsSame(hTarget, hTargetInternal));

        OCTDestroyCoordinateTransformation(hInvTransform);
        OCTDestroyCoordinateTransformation(hClone);
        OCTDestroyCoordinateTransformation(hTransform);
        OSRDestroySpatialReference(hSource);
        OSRDestroySpatialReference(hTarget);
    }
} // namespace tut
