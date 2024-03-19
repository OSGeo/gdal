///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test coordinate transformations. Ported from osr/osr_ct.py.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
/*
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

#include "gdal_unit_test.h"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "gtest_include.h"

namespace
{

// Common fixture with test data
struct test_osr_ct : public ::testing::Test
{
    OGRErr err_ = OGRERR_NONE;
    OGRSpatialReferenceH srs_utm_ = nullptr;
    OGRSpatialReferenceH srs_ll_ = nullptr;
    OGRCoordinateTransformationH ct_ = nullptr;

    void SetUp() override
    {
        srs_utm_ = OSRNewSpatialReference(nullptr);
        srs_ll_ = OSRNewSpatialReference(nullptr);
        OSRSetAxisMappingStrategy(srs_utm_, OAMS_TRADITIONAL_GIS_ORDER);
        OSRSetAxisMappingStrategy(srs_ll_, OAMS_TRADITIONAL_GIS_ORDER);
    }

    void TearDown() override
    {
        OSRDestroySpatialReference(srs_utm_);
        srs_utm_ = nullptr;
        OSRDestroySpatialReference(srs_ll_);
        srs_ll_ = nullptr;
        OCTDestroyCoordinateTransformation(ct_);
        ct_ = nullptr;
    }
};

TEST_F(test_osr_ct, basic)
{
    err_ = OSRSetUTM(srs_utm_, 11, TRUE);
    ASSERT_EQ(err_, OGRERR_NONE);

    err_ = OSRSetWellKnownGeogCS(srs_utm_, "WGS84");
    ASSERT_EQ(err_, OGRERR_NONE);

    err_ = OSRSetWellKnownGeogCS(srs_ll_, "WGS84");
    ASSERT_EQ(err_, OGRERR_NONE);

    ct_ = OCTNewCoordinateTransformation(srs_ll_, srs_utm_);
    ASSERT_TRUE(nullptr != ct_);
}

// Actually perform a simple LL to UTM conversion
TEST_F(test_osr_ct, LL_to_UTM)
{
    err_ = OSRSetUTM(srs_utm_, 11, TRUE);
    ASSERT_EQ(err_, OGRERR_NONE);

    err_ = OSRSetWellKnownGeogCS(srs_utm_, "WGS84");
    ASSERT_EQ(err_, OGRERR_NONE);

    err_ = OSRSetWellKnownGeogCS(srs_ll_, "WGS84");
    ASSERT_EQ(err_, OGRERR_NONE);

    ct_ = OCTNewCoordinateTransformation(srs_ll_, srs_utm_);
    ASSERT_TRUE(nullptr != ct_);

    const int size = 1;
    double x[size] = {-117.5};
    double y[size] = {32.0};
    double z[size] = {0.0};

    ASSERT_EQ(OCTTransform(ct_, size, x, y, z), TRUE);

    EXPECT_NEAR(x[0], 452772.06, 0.01);
    EXPECT_NEAR(y[0], 3540544.89, 0.01);
    EXPECT_NEAR(z[0], 0.0, 0.01);
}

// Transform an OGR geometry.
// This is mostly aimed at ensuring that the OGRCoordinateTransformation
// target SRS isn't deleted till the output geometry which also
// uses it is deleted.
TEST_F(test_osr_ct, OGR_G_Transform)
{
    err_ = OSRSetUTM(srs_utm_, 11, TRUE);
    ASSERT_EQ(err_, OGRERR_NONE);

    err_ = OSRSetWellKnownGeogCS(srs_utm_, "WGS84");
    ASSERT_EQ(err_, OGRERR_NONE);

    err_ = OSRSetWellKnownGeogCS(srs_ll_, "WGS84");
    ASSERT_EQ(err_, OGRERR_NONE);

    ct_ = OCTNewCoordinateTransformation(srs_ll_, srs_utm_);
    ASSERT_TRUE(nullptr != ct_);

    const char *wkt = "POINT(-117.5 32.0)";
    OGRGeometryH geom = nullptr;
    err_ = OGR_G_CreateFromWkt((char **)&wkt, nullptr, &geom);
    EXPECT_EQ(OGRERR_NONE, err_);
    EXPECT_TRUE(nullptr != geom);
    if (geom)
    {
        err_ = OGR_G_Transform(geom, ct_);
        ASSERT_EQ(err_, OGRERR_NONE);

        OGRSpatialReferenceH srs = nullptr;
        srs = OGR_G_GetSpatialReference(geom);

        char *wktSrs = nullptr;
        err_ = OSRExportToPrettyWkt(srs, &wktSrs, FALSE);
        EXPECT_TRUE(nullptr != wktSrs);
        if (wktSrs)
        {
            std::string pretty(wktSrs);
            EXPECT_EQ(pretty.substr(0, 6), std::string("PROJCS"));
        }
        CPLFree(wktSrs);
        OGR_G_DestroyGeometry(geom);
    }
}

// Test OGRCoordinateTransformation::GetInverse()
TEST_F(test_osr_ct, GetInverse)
{
    OGRSpatialReference oSRSSource;
    oSRSSource.SetAxisMappingStrategy(OAMS_AUTHORITY_COMPLIANT);
    oSRSSource.importFromEPSG(4267);

    OGRSpatialReference oSRSTarget;
    oSRSTarget.SetAxisMappingStrategy(OAMS_AUTHORITY_COMPLIANT);
    oSRSTarget.importFromEPSG(4269);

    auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
        OGRCreateCoordinateTransformation(&oSRSSource, &oSRSTarget));
    ASSERT_TRUE(poCT != nullptr);
    ASSERT_TRUE(poCT->GetSourceCS() != nullptr);
    ASSERT_TRUE(poCT->GetSourceCS()->IsSame(&oSRSSource));
    ASSERT_TRUE(poCT->GetTargetCS() != nullptr);
    ASSERT_TRUE(poCT->GetTargetCS()->IsSame(&oSRSTarget));

    auto poInverse =
        std::unique_ptr<OGRCoordinateTransformation>(poCT->GetInverse());
    ASSERT_TRUE(poInverse != nullptr);
    ASSERT_TRUE(poInverse->GetSourceCS() != nullptr);
    ASSERT_TRUE(poInverse->GetSourceCS()->IsSame(&oSRSTarget));
    ASSERT_TRUE(poInverse->GetTargetCS() != nullptr);
    ASSERT_TRUE(poInverse->GetTargetCS()->IsSame(&oSRSSource));

    double x = 40;
    double y = -100;
    ASSERT_TRUE(poCT->Transform(1, &x, &y));
    // Check that the transformed point is different but not too far
    EXPECT_TRUE(fabs(x - 40) > 1e-10);
    EXPECT_TRUE(fabs(y - -100) > 1e-10);
    EXPECT_NEAR(x, 40, 1e-3);
    EXPECT_NEAR(y, -100, 1e-3);
    const double xTransformed = x;
    const double yTransformed = y;

    poCT.reset();

    // Check that the transformed point with the inverse transformation
    // matches the source
    ASSERT_TRUE(poInverse->Transform(1, &x, &y));
    EXPECT_NEAR(x, 40, 1e-8);
    EXPECT_NEAR(y, -100, 1e-8);

    auto poInvOfInv =
        std::unique_ptr<OGRCoordinateTransformation>(poInverse->GetInverse());
    ASSERT_TRUE(poInvOfInv != nullptr);
    ASSERT_TRUE(poInvOfInv->GetSourceCS() != nullptr);
    ASSERT_TRUE(poInvOfInv->GetSourceCS()->IsSame(&oSRSSource));
    ASSERT_TRUE(poInvOfInv->GetTargetCS() != nullptr);
    ASSERT_TRUE(poInvOfInv->GetTargetCS()->IsSame(&oSRSTarget));
    ASSERT_TRUE(poInvOfInv->Transform(1, &x, &y));
    // Check that the transformed point is different but not too far
    EXPECT_NEAR(x, xTransformed, 1e-8);
    EXPECT_NEAR(y, yTransformed, 1e-8);
}

// Test OGRCoordinateTransformation::GetInverse() with a specified coordinate
// operation
TEST_F(test_osr_ct, GetInverse_with_ct)
{
    OGRCoordinateTransformationOptions options;
    options.SetCoordinateOperation("+proj=affine +xoff=10", false);
    auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
        OGRCreateCoordinateTransformation(nullptr, nullptr, options));
    ASSERT_TRUE(poCT != nullptr);

    auto poInverse =
        std::unique_ptr<OGRCoordinateTransformation>(poCT->GetInverse());
    ASSERT_TRUE(poInverse != nullptr);
    ASSERT_TRUE(poInverse->GetSourceCS() == nullptr);
    ASSERT_TRUE(poInverse->GetTargetCS() == nullptr);

    poCT.reset();

    double x = 100;
    double y = 200;
    ASSERT_TRUE(poInverse->Transform(1, &x, &y));
    EXPECT_NEAR(x, 90, 1e-12);
    EXPECT_NEAR(y, 200.0, 1e-12);
}

// Test OGRCoordinateTransformation::Clone()
static void test_clone(OGRCoordinateTransformation *poCT,
                       OGRSpatialReference *poSRSSource,
                       OGRSpatialReference *poSRSTarget, const double xSrc,
                       const double ySrc)
{
    ASSERT_TRUE(poCT != nullptr);
    ASSERT_TRUE((poCT->GetSourceCS() == nullptr) == (poSRSSource == nullptr));
    if (poSRSSource != nullptr)
    {
        ASSERT_TRUE(poCT->GetSourceCS()->IsSame(poSRSSource));
    }
    ASSERT_TRUE((poCT->GetTargetCS() == nullptr) == (poSRSTarget == nullptr));
    if (poSRSTarget != nullptr)
    {
        ASSERT_TRUE(poCT->GetTargetCS()->IsSame(poSRSTarget));
    }
    double x = xSrc;
    double y = ySrc;
    ASSERT_TRUE(poCT->Transform(1, &x, &y));
    const double xTransformed = x;
    const double yTransformed = y;

    auto poClone = std::unique_ptr<OGRCoordinateTransformation>(poCT->Clone());
    ASSERT_TRUE(poClone != nullptr);
    ASSERT_TRUE((poClone->GetSourceCS() == nullptr) ==
                (poSRSSource == nullptr));
    if (poSRSSource != nullptr)
    {
        ASSERT_TRUE(poClone->GetSourceCS()->IsSame(poSRSSource));
    }
    ASSERT_TRUE((poClone->GetTargetCS() == nullptr) ==
                (poSRSTarget == nullptr));
    if (poSRSTarget != nullptr)
    {
        ASSERT_TRUE(poClone->GetTargetCS()->IsSame(poSRSTarget));
    }
    x = xSrc;
    y = ySrc;
    ASSERT_TRUE(poClone->Transform(1, &x, &y));
    EXPECT_NEAR(x, xTransformed, 1e-15);
    EXPECT_NEAR(y, yTransformed, 1e-15);
}

// Test OGRCoordinateTransformation::Clone() with usual case
TEST_F(test_osr_ct, Clone)
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

// Test OGRCoordinateTransformation::Clone() with a specified coordinate
// operation
TEST_F(test_osr_ct, Clone_with_ct)
{
    OGRCoordinateTransformationOptions options;
    options.SetCoordinateOperation("+proj=affine +xoff=10", false);
    auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
        OGRCreateCoordinateTransformation(nullptr, nullptr, options));

    test_clone(poCT.get(), nullptr, nullptr, 90, 200);
}

// Test OGRCoordinateTransformation::Clone() with WebMercator->WGS84 special
// case
TEST_F(test_osr_ct, Clone_WebMercator_to_WGS84)
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
TEST_F(test_osr_ct, OGRCoordinateTransformation_C_API)
{
    OGRSpatialReferenceH hSource = OSRNewSpatialReference(nullptr);
    OGRSpatialReferenceH hTarget = OSRNewSpatialReference(nullptr);
    EXPECT_TRUE(hSource != nullptr);
    EXPECT_TRUE(hTarget != nullptr);
    if (hSource && hTarget)
    {
        EXPECT_TRUE(OGRERR_NONE == OSRImportFromEPSG(hSource, 32637));
        EXPECT_TRUE(OGRERR_NONE == OSRSetWellKnownGeogCS(hTarget, "WGS84"));
        OGRCoordinateTransformationH hTransform =
            OCTNewCoordinateTransformation(hSource, hTarget);
        EXPECT_TRUE(hTransform != nullptr);
        if (hTransform)
        {
            OGRCoordinateTransformationH hClone = OCTClone(hTransform);
            EXPECT_TRUE(hClone != nullptr);

            OGRCoordinateTransformationH hInvTransform =
                OCTGetInverse(hTransform);
            EXPECT_TRUE(hInvTransform != nullptr);
            if (hClone && hInvTransform)
            {
                OGRSpatialReferenceH hSourceInternal =
                    OCTGetSourceCS(hTransform);
                EXPECT_TRUE(hSourceInternal != nullptr);
                OGRSpatialReferenceH hTargetInternal =
                    OCTGetTargetCS(hTransform);
                EXPECT_TRUE(hTargetInternal != nullptr);

                if (hSourceInternal)
                {
                    EXPECT_TRUE(OSRIsSame(hSource, hSourceInternal));
                }
                if (hTargetInternal)
                {
                    EXPECT_TRUE(OSRIsSame(hTarget, hTargetInternal));
                }
            }

            OCTDestroyCoordinateTransformation(hInvTransform);
            OCTDestroyCoordinateTransformation(hClone);
        }
        OCTDestroyCoordinateTransformation(hTransform);
    }
    OSRDestroySpatialReference(hSource);
    OSRDestroySpatialReference(hTarget);
}
}  // namespace
