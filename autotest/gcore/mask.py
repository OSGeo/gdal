#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC 15 "mask band" default functionality (nodata/alpha/etc)
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import struct

import gdaltest
import pytest

from osgeo import gdal


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Verify the checksum and flags for "all valid" case.


def test_mask_1():

    ds = gdal.Open("data/byte.tif")

    assert ds is not None, "Failed to open test dataset."

    band = ds.GetRasterBand(1)
    assert not band.IsMaskBand()

    assert band.GetMaskFlags() == gdal.GMF_ALL_VALID, "Did not get expected mask."
    assert band.GetMaskBand().IsMaskBand()

    cs = band.GetMaskBand().Checksum()
    assert cs == 4873, "Got wrong mask checksum"

    my_min, my_max, mean, stddev = band.GetMaskBand().ComputeStatistics(0)
    assert (my_min, my_max, mean, stddev) == (255, 255, 255, 0), "Got wrong mask stats"


###############################################################################
# Verify the checksum and flags for "nodata" case.


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_mask_2():

    ds = gdal.Open("data/byte.vrt")

    assert ds is not None, "Failed to open test dataset."

    band = ds.GetRasterBand(1)
    assert not band.IsMaskBand()

    assert band.GetMaskFlags() == gdal.GMF_NODATA, "Did not get expected mask."
    assert band.GetMaskBand().IsMaskBand()

    cs = band.GetMaskBand().Checksum()
    assert cs == 4209, "Got wrong mask checksum"


###############################################################################
# Verify the checksum and flags for "alpha" case.


@pytest.mark.require_driver("PNG")
def test_mask_3():

    ds = gdal.Open("data/stefan_full_rgba.png")

    assert ds is not None, "Failed to open test dataset."

    # Test first mask.

    band = ds.GetRasterBand(1)

    assert (
        band.GetMaskFlags() == gdal.GMF_ALPHA + gdal.GMF_PER_DATASET
    ), "Did not get expected mask."
    assert band.GetMaskBand().IsMaskBand()

    cs = band.GetMaskBand().Checksum()
    assert cs == 10807, "Got wrong mask checksum"

    # Verify second and third same as first.

    band_2 = ds.GetRasterBand(2)
    band_3 = ds.GetRasterBand(3)

    # We have commented the following tests as SWIG >= 1.3.37 is buggy !
    #  or str(band_2.GetMaskBand()) != str(band.GetMaskBand()) \
    #   or str(band_3.GetMaskBand()) != str(band.GetMaskBand())
    assert (
        band_2.GetMaskFlags() == band.GetMaskFlags()
        and band_3.GetMaskFlags() == band.GetMaskFlags()
    ), "Band 2 or 3 does not seem to match first mask"

    # Verify alpha has no mask.
    band = ds.GetRasterBand(4)
    assert (
        band.GetMaskFlags() == gdal.GMF_ALL_VALID
    ), "Did not get expected mask for alpha."

    cs = band.GetMaskBand().Checksum()
    assert cs == 36074, "Got wrong alpha mask checksum"


###############################################################################
# Copy a *real* masked dataset, and confirm masks copied properly.


@pytest.mark.require_driver("JPEG")
@pytest.mark.require_driver("PNM")
def test_mask_4(tmp_path):

    src_ds = gdal.Open("../gdrivers/data/jpeg/masked.jpg")

    assert src_ds is not None, "Failed to open test dataset."

    output_ppm = str(tmp_path / "mask_4.ppm")

    # NOTE: for now we copy to PNM since it does everything (overviews too)
    # externally. Should eventually test with gtiff, hfa.
    drv = gdal.GetDriverByName("PNM")
    ds = drv.CreateCopy(output_ppm, src_ds)
    src_ds = None

    # confirm we got the custom mask on the copied dataset.
    assert (
        ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    ), "did not get expected mask flags"

    msk = ds.GetRasterBand(1).GetMaskBand()
    assert msk.IsMaskBand()

    cs = msk.Checksum()
    expected_cs = 770

    assert cs == expected_cs, "Did not get expected checksum"

    msk = None
    ds = None


###############################################################################
# Create overviews for masked file, and verify the overviews have proper
# masks built for them.


@pytest.mark.require_driver("JPEG")
@pytest.mark.require_driver("PNM")
def test_mask_5(tmp_path):

    src_ds = gdal.Open("../gdrivers/data/jpeg/masked.jpg")

    output_ppm = str(tmp_path / "mask_4.ppm")
    drv = gdal.GetDriverByName("PNM")
    ds = drv.CreateCopy(output_ppm, src_ds)

    ds = gdal.Open(output_ppm, gdal.GA_Update)

    assert ds is not None, "Failed to open test dataset."

    # So that we instantiate the mask band before.
    ds.GetRasterBand(1).GetMaskFlags()

    ds.BuildOverviews(overviewlist=[2, 4])

    # confirm mask flags on overview.
    ovr = ds.GetRasterBand(1).GetOverview(1)

    assert ovr.GetMaskFlags() == gdal.GMF_PER_DATASET, "did not get expected mask flags"

    msk = ovr.GetMaskBand()
    assert msk.IsMaskBand()
    cs = msk.Checksum()
    expected_cs = 20505

    assert cs == expected_cs, "Did not get expected checksum"
    ovr = None
    msk = None
    ds = None

    # Reopen and confirm we still get same results.
    ds = gdal.Open(output_ppm)

    # confirm mask flags on overview.
    ovr = ds.GetRasterBand(1).GetOverview(1)

    assert ovr.GetMaskFlags() == gdal.GMF_PER_DATASET, "did not get expected mask flags"

    msk = ovr.GetMaskBand()
    assert msk.IsMaskBand()
    cs = msk.Checksum()
    expected_cs = 20505

    assert cs == expected_cs, "Did not get expected checksum"

    ovr = None
    msk = None
    ds = None


###############################################################################
# Test a TIFF file with 1 band and an embedded mask of 1 bit


def test_mask_6():

    with gdaltest.config_option("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "FALSE"):
        ds = gdal.Open("data/test_with_mask_1bit.tif")

        assert ds is not None, "Failed to open test dataset."

        band = ds.GetRasterBand(1)

        assert band.GetMaskFlags() == gdal.GMF_PER_DATASET, "Did not get expected mask."
        assert band.GetMaskBand().IsMaskBand()

        cs = band.GetMaskBand().Checksum()
        assert cs == 100, "Got wrong mask checksum"


###############################################################################
# Test a TIFF file with 3 bands and an embedded mask of 1 band of 1 bit


def test_mask_7():

    with gdaltest.config_option("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "FALSE"):
        ds = gdal.Open("data/test3_with_1mask_1bit.tif")

        assert ds is not None, "Failed to open test dataset."

        for i in (1, 2, 3):
            band = ds.GetRasterBand(i)

            assert (
                band.GetMaskFlags() == gdal.GMF_PER_DATASET
            ), "Did not get expected mask."
            assert band.GetMaskBand().IsMaskBand()

            cs = band.GetMaskBand().Checksum()
            assert cs == 100, "Got wrong mask checksum"


###############################################################################
# Test a TIFF file with 1 band and an embedded mask of 8 bit.
# Note : The TIFF6 specification, page 37, only allows 1 BitsPerSample && 1 SamplesPerPixel,


def test_mask_8():

    ds = gdal.Open("data/test_with_mask_8bit.tif")

    assert ds is not None, "Failed to open test dataset."

    band = ds.GetRasterBand(1)

    assert band.GetMaskFlags() == gdal.GMF_PER_DATASET, "Did not get expected mask."
    assert band.GetMaskBand().IsMaskBand()

    cs = band.GetMaskBand().Checksum()
    assert cs == 1222, "Got wrong mask checksum"


###############################################################################
# Test a TIFF file with 3 bands with an embedded mask of 1 bit with 3 bands.
# Note : The TIFF6 specification, page 37, only allows 1 BitsPerSample && 1 SamplesPerPixel,


def test_mask_9():

    ds = gdal.Open("data/test3_with_mask_1bit.tif")

    assert ds is not None, "Failed to open test dataset."

    for i in (1, 2, 3):
        band = ds.GetRasterBand(i)

        assert band.GetMaskFlags() == 0, "Did not get expected mask."
        assert band.GetMaskBand().IsMaskBand()

        cs = band.GetMaskBand().Checksum()
        assert cs == 100, "Got wrong mask checksum"


###############################################################################
# Test a TIFF file with 3 bands with an embedded mask of 8 bit with 3 bands.
# Note : The TIFF6 specification, page 37, only allows 1 BitsPerSample && 1 SamplesPerPixel,


def test_mask_10():

    ds = gdal.Open("data/test3_with_mask_8bit.tif")

    assert ds is not None, "Failed to open test dataset."

    for i in (1, 2, 3):
        band = ds.GetRasterBand(i)

        assert band.GetMaskFlags() == 0, "Did not get expected mask."
        assert band.GetMaskBand().IsMaskBand()

        cs = band.GetMaskBand().Checksum()
        assert cs == 1222, "Got wrong mask checksum"


###############################################################################
# Test a TIFF file with an overview, an embedded mask of 1 bit, and an embedded
# mask for the overview


def test_mask_11():

    with gdaltest.config_option("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "FALSE"):
        ds = gdal.Open("data/test_with_mask_1bit_and_ovr.tif")

        assert ds is not None, "Failed to open test dataset."

        band = ds.GetRasterBand(1)
        assert not band.IsMaskBand()

        # Let's fetch the mask
        assert band.GetMaskFlags() == gdal.GMF_PER_DATASET, "Did not get expected mask."
        assert band.GetMaskBand().IsMaskBand()

        cs = band.GetMaskBand().Checksum()
        assert cs == 100, "Got wrong mask checksum"

        # Let's fetch the overview
        band = ds.GetRasterBand(1).GetOverview(0)
        cs = band.Checksum()
        assert cs == 1126, "Got wrong overview checksum"

        # Let's fetch the mask of the overview
        assert band.GetMaskFlags() == gdal.GMF_PER_DATASET, "Did not get expected mask."

        cs = band.GetMaskBand().Checksum()
        assert cs == 25, "Got wrong checksum for the mask of the overview"

        # Let's fetch the overview of the mask == the mask of the overview
        band = ds.GetRasterBand(1).GetMaskBand().GetOverview(0)
        cs = band.Checksum()
        assert cs == 25, "Got wrong checksum for the overview of the mask"


###############################################################################
# Test a TIFF file with 3 bands, an overview, an embedded mask of 1 bit, and an embedded
# mask for the overview


def test_mask_12():

    with gdaltest.config_option("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "FALSE"):
        ds = gdal.Open("data/test3_with_mask_1bit_and_ovr.tif")

        assert ds is not None, "Failed to open test dataset."

        for i in (1, 2, 3):
            band = ds.GetRasterBand(i)

            # Let's fetch the mask
            assert (
                band.GetMaskFlags() == gdal.GMF_PER_DATASET
            ), "Did not get expected mask."

            cs = band.GetMaskBand().Checksum()
            assert cs == 100, "Got wrong mask checksum"

            # Let's fetch the overview
            band = ds.GetRasterBand(i).GetOverview(0)
            cs = band.Checksum()
            assert cs == 1126, "Got wrong overview checksum"

            # Let's fetch the mask of the overview
            assert (
                band.GetMaskFlags() == gdal.GMF_PER_DATASET
            ), "Did not get expected mask."

            cs = band.GetMaskBand().Checksum()
            assert cs == 25, "Got wrong checksum for the mask of the overview"

            # Let's fetch the overview of the mask == the mask of the overview
            band = ds.GetRasterBand(i).GetMaskBand().GetOverview(0)
            cs = band.Checksum()
            assert cs == 25, "Got wrong checksum for the overview of the mask"


###############################################################################
# Test creation of external TIFF mask band


def test_mask_13():

    src_ds = gdal.Open("data/byte.tif")

    assert src_ds is not None, "Failed to open test dataset."

    drv = gdal.GetDriverByName("GTiff")
    ds = drv.CreateCopy("tmp/byte_with_mask.tif", src_ds)
    src_ds = None

    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    assert ds.GetRasterBand(1).GetMaskBand().IsMaskBand()

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 0, "Got wrong checksum for the mask"

    ds.GetRasterBand(1).GetMaskBand().Fill(1)

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 400, "Got wrong checksum for the mask"

    ds = None

    try:
        os.stat("tmp/byte_with_mask.tif.msk")
    except OSError:
        pytest.fail("tmp/byte_with_mask.tif.msk is absent")

    ds = gdal.Open("tmp/byte_with_mask.tif")

    assert (
        ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    ), "wrong mask flags"

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 400, "Got wrong checksum for the mask"

    ds = None

    drv.Delete("tmp/byte_with_mask.tif")

    assert not os.path.exists("tmp/byte_with_mask.tif.msk")


###############################################################################
# Test creation of internal TIFF mask band


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_mask_14():

    src_ds = gdal.Open("data/byte.tif")

    assert src_ds is not None, "Failed to open test dataset."

    drv = gdal.GetDriverByName("GTiff")
    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        with gdal.config_option("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "FALSE"):
            ds = drv.CreateCopy("tmp/byte_with_mask.tif", src_ds)
    src_ds = None

    # The only flag value supported for internal mask is GMF_PER_DATASET
    with gdal.quiet_errors():
        ret = ds.CreateMaskBand(0)
    assert ret != 0, "Error expected"

    ret = ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    assert ret == 0, "Creation failed"
    assert ds.GetRasterBand(1).GetMaskBand().IsMaskBand()

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 0, "Got wrong checksum for the mask (1)"

    ds.GetRasterBand(1).GetMaskBand().Fill(1)

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 400, "Got wrong checksum for the mask (2)"

    # This TIFF dataset has already an internal mask band
    with gdal.quiet_errors():
        ret = ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    assert ret != 0, "Error expected"

    # This TIFF dataset has already an internal mask band
    with gdal.quiet_errors():
        ret = ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    assert ret != 0, "Error expected"

    ds = None

    assert not os.path.exists("tmp/byte_with_mask.tif.msk")

    with gdaltest.config_option("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "FALSE"):
        ds = gdal.Open("tmp/byte_with_mask.tif")

        assert (
            ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
        ), "wrong mask flags"

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 400, "Got wrong checksum for the mask (3)"

    # Test fix for #5884
    with gdaltest.SetCacheMax(0):
        out_ds = drv.CreateCopy(
            "/vsimem/byte_with_mask.tif", ds, options=["COMPRESS=JPEG"]
        )

    assert out_ds.GetRasterBand(1).Checksum() != 0
    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 400, "Got wrong checksum for the mask (4)"
    out_ds = None
    drv.Delete("/vsimem/byte_with_mask.tif")

    ds = None

    drv.Delete("tmp/byte_with_mask.tif")


###############################################################################
# Test creation of internal TIFF overview, mask band and mask band of overview


@pytest.mark.parametrize("order", [1, 2, 3, 4])
@pytest.mark.parametrize("method", ["NEAR", "AVERAGE"])
def test_mask_and_ovr(order, method):

    src_ds = gdal.Open("data/byte.tif")

    assert src_ds is not None, "Failed to open test dataset."

    drv = gdal.GetDriverByName("GTiff")
    ds = drv.CreateCopy("tmp/byte_with_ovr_and_mask.tif", src_ds)
    src_ds = None

    if order == 1:
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.BuildOverviews(method, overviewlist=[2, 4])
        with gdal.quiet_errors():
            assert (
                ds.GetRasterBand(1).GetOverview(0).CreateMaskBand(gdal.GMF_PER_DATASET)
                == gdal.CE_Failure
            )
            assert (
                ds.GetRasterBand(1).GetOverview(1).CreateMaskBand(gdal.GMF_PER_DATASET)
                == gdal.CE_Failure
            )
    elif order == 2:
        ds.BuildOverviews(method, overviewlist=[2, 4])
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetOverview(0).CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetOverview(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    elif order == 3:
        ds.BuildOverviews(method, overviewlist=[2, 4])
        ds.GetRasterBand(1).GetOverview(0).CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetOverview(1).CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    elif order == 4:
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetMaskBand().Fill(1)
        # The overview for the mask will be implicitly created and computed.
        ds.BuildOverviews(method, overviewlist=[2, 4])

    if order < 4:
        ds = None
        ds = gdal.Open("tmp/byte_with_ovr_and_mask.tif", gdal.GA_Update)
        ds.GetRasterBand(1).GetMaskBand().Fill(1)
        # The overview of the mask will be implicitly recomputed.
        ds.BuildOverviews(method, overviewlist=[2, 4])

    ds = None

    assert not os.path.exists("tmp/byte_with_ovr_and_mask.tif.msk")

    with gdaltest.config_option("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "FALSE"):
        ds = gdal.Open("tmp/byte_with_ovr_and_mask.tif")

        assert (
            ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
        ), "wrong mask flags"

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 400, "Got wrong checksum for the mask"

    cs = ds.GetRasterBand(1).GetOverview(0).GetMaskBand().Checksum()
    assert cs == 100, "Got wrong checksum for the mask of the first overview"

    cs = ds.GetRasterBand(1).GetOverview(1).GetMaskBand().Checksum()
    assert cs == 25, "Got wrong checksum for the mask of the second overview"

    ds = None

    drv.Delete("tmp/byte_with_ovr_and_mask.tif")


###############################################################################
# Test NODATA_VALUES mask


def test_mask_19():

    ds = gdal.Open("data/test_nodatavalues.tif")

    assert ds is not None, "Failed to open test dataset."

    assert (
        ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET + gdal.GMF_NODATA
    ), "did not get expected mask flags"

    msk = ds.GetRasterBand(1).GetMaskBand()
    cs = msk.Checksum()
    expected_cs = 11043

    assert cs == expected_cs, "Did not get expected checksum"

    msk = None
    ds = None


###############################################################################
# Extensive test of nodata mask for all data types


def test_mask_20():

    types = [
        gdal.GDT_UInt8,
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_UInt32,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
        gdal.GDT_CFloat32,
        gdal.GDT_CFloat64,
    ]

    nodatavalue = [1, -1, 1, -1, 1, 0.5, 0.5, 0.5, 0.5]

    drv = gdal.GetDriverByName("GTiff")
    for i, typ in enumerate(types):
        ds = drv.Create("tmp/mask20.tif", 1, 1, 1, typ)
        ds.GetRasterBand(1).Fill(nodatavalue[i])
        ds.GetRasterBand(1).SetNoDataValue(nodatavalue[i])

        assert (
            ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_NODATA
        ), "did not get expected mask flags for type %s" % gdal.GetDataTypeName(typ)

        msk = ds.GetRasterBand(1).GetMaskBand()
        assert msk.Checksum() == 0, (
            "did not get expected mask checksum for type %s : %d"
            % gdal.GetDataTypeName(typ, msk.Checksum())
        )

        msk = None
        ds = None
        drv.Delete("tmp/mask20.tif")


###############################################################################
# Extensive test of NODATA_VALUES mask for all data types


def test_mask_21():

    types = [
        gdal.GDT_UInt8,
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_UInt32,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
        gdal.GDT_CFloat32,
        gdal.GDT_CFloat64,
    ]

    nodatavalue = [1, -1, 1, -1, 1, 0.5, 0.5, 0.5, 0.5]

    drv = gdal.GetDriverByName("GTiff")
    for i, typ in enumerate(types):
        ds = drv.Create("tmp/mask21.tif", 1, 1, 3, typ)
        md = {}
        md["NODATA_VALUES"] = "%f %f %f" % (
            nodatavalue[i],
            nodatavalue[i],
            nodatavalue[i],
        )
        ds.SetMetadata(md)
        ds.GetRasterBand(1).Fill(nodatavalue[i])
        ds.GetRasterBand(2).Fill(nodatavalue[i])
        ds.GetRasterBand(3).Fill(nodatavalue[i])

        assert (
            ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET + gdal.GMF_NODATA
        ), "did not get expected mask flags for type %s" % gdal.GetDataTypeName(typ)

        msk = ds.GetRasterBand(1).GetMaskBand()
        assert msk.Checksum() == 0, (
            "did not get expected mask checksum for type %s : %d"
            % gdal.GetDataTypeName(typ, msk.Checksum())
        )

        msk = None
        ds = None
        drv.Delete("tmp/mask21.tif")


###############################################################################
# Test creation of external TIFF mask band just after Create()


def test_mask_22():

    drv = gdal.GetDriverByName("GTiff")
    ds = drv.Create("tmp/mask_22.tif", 20, 20)
    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 0, "Got wrong checksum for the mask"

    ds.GetRasterBand(1).GetMaskBand().Fill(1)

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 400, "Got wrong checksum for the mask"

    ds = None

    try:
        os.stat("tmp/mask_22.tif.msk")
    except OSError:
        pytest.fail("tmp/mask_22.tif.msk is absent")

    ds = gdal.Open("tmp/mask_22.tif")

    assert (
        ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    ), "wrong mask flags"

    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 400, "Got wrong checksum for the mask"

    ds = None

    drv.Delete("tmp/mask_22.tif")

    assert not os.path.exists("tmp/mask_22.tif.msk")


###############################################################################
# Test CreateCopy() of a dataset with a mask into a JPEG-compressed TIFF with
# internal mask (#3800)


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_mask_23():

    drv = gdal.GetDriverByName("GTiff")

    src_ds = drv.Create(
        "tmp/mask_23_src.tif", 3000, 2000, 3, options=["TILED=YES", "SPARSE_OK=YES"]
    )
    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)

    gdal.ErrorReset()
    with gdaltest.SetCacheMax(15000000):
        ds = drv.CreateCopy(
            "tmp/mask_23_dst.tif", src_ds, options=["TILED=YES", "COMPRESS=JPEG"]
        )

    del ds
    error_msg = gdal.GetLastErrorMsg()
    src_ds = None

    drv.Delete("tmp/mask_23_src.tif")
    drv.Delete("tmp/mask_23_dst.tif")

    # 'ERROR 1: TIFFRewriteDirectory:Error fetching directory count' was triggered before
    assert error_msg == ""


###############################################################################
# Test on a GDT_UInt16 RGBA (#5692)


def test_mask_24():

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/mask_24.tif",
        100,
        100,
        4,
        gdal.GDT_UInt16,
        options=["PHOTOMETRIC=RGB", "ALPHA=YES"],
    )
    ds.GetRasterBand(1).Fill(65565)
    ds.GetRasterBand(2).Fill(65565)
    ds.GetRasterBand(3).Fill(65565)
    ds.GetRasterBand(4).Fill(65565)

    assert (
        ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALPHA + gdal.GMF_PER_DATASET
    ), "Did not get expected mask."
    mask = ds.GetRasterBand(1).GetMaskBand()

    # IRasterIO() optimized case
    import struct

    assert struct.unpack("B", mask.ReadRaster(0, 0, 1, 1))[0] == 255

    # IReadBlock() code path
    blockx, blocky = mask.GetBlockSize()
    assert struct.unpack("B" * blockx * blocky, mask.ReadBlock(0, 0))[0] == 255
    mask.FlushCache()

    # Test special case where dynamics is only 0-255
    ds.GetRasterBand(4).Fill(255)
    assert struct.unpack("B", mask.ReadRaster(0, 0, 1, 1))[0] == 1

    ds = None

    gdal.Unlink("/vsimem/mask_24.tif")


###############################################################################
# Test various error conditions


def test_mask_25():

    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/mask_25.tif", 1, 1)
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    ds = None

    # No INTERNAL_MASK_FLAGS_x metadata
    gdal.GetDriverByName("GTiff").Create("/vsimem/mask_25.tif.msk", 1, 1)
    ds = gdal.Open("/vsimem/mask_25.tif")
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 3
    ds = None
    gdal.Unlink("/vsimem/mask_25.tif")
    gdal.Unlink("/vsimem/mask_25.tif.msk")

    # Per-band mask
    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/mask_25.tif", 1, 1)
    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        ds.GetRasterBand(1).CreateMaskBand(0)
    ds = None
    ds = gdal.Open("/vsimem/mask_25.tif")
    assert ds.GetRasterBand(1).GetMaskFlags() == 0
    cs = ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert cs == 0
    ds = None
    gdal.Unlink("/vsimem/mask_25.tif")
    gdal.Unlink("/vsimem/mask_25.tif.msk")

    # .msk file does not have enough bands
    gdal.GetDriverByName("GTiff").Create("/vsimem/mask_25.tif", 1, 1, 2)
    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/mask_25.tif.msk", 1, 1)
    ds.SetMetadataItem("INTERNAL_MASK_FLAGS_2", "0")
    ds = None
    ds = gdal.Open("/vsimem/mask_25.tif")
    with gdal.quiet_errors():
        assert ds.GetRasterBand(2).GetMaskFlags() == gdal.GMF_ALL_VALID
    ds = None
    gdal.Unlink("/vsimem/mask_25.tif")
    gdal.Unlink("/vsimem/mask_25.tif.msk")

    # Invalid sequences of CreateMaskBand() calls
    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/mask_25.tif", 1, 1, 2)
    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    with gdal.quiet_errors():
        with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
            assert ds.GetRasterBand(2).CreateMaskBand(0) != 0
    ds = None
    gdal.Unlink("/vsimem/mask_25.tif")
    gdal.Unlink("/vsimem/mask_25.tif.msk")

    # CreateMaskBand not supported by this dataset
    with gdal.quiet_errors():
        ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
        ds.CreateMaskBand(0)


###############################################################################
# Test on a GDT_UInt16 1band data


def test_mask_26():

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/mask_26.tif", 100, 100, 2, gdal.GDT_UInt16, options=["ALPHA=YES"]
    )
    ds.GetRasterBand(1).Fill(65565)
    ds.GetRasterBand(2).Fill(65565)

    assert (
        ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALPHA + gdal.GMF_PER_DATASET
    ), "Did not get expected mask."
    mask = ds.GetRasterBand(1).GetMaskBand()

    # IRasterIO() optimized case
    import struct

    assert struct.unpack("B", mask.ReadRaster(0, 0, 1, 1))[0] == 255

    ds = None

    gdal.Unlink("/vsimem/mask_26.tif")


###############################################################################
# Extensive test of nodata mask for all complex types using real part only


def test_mask_27():

    types = [gdal.GDT_CFloat32, gdal.GDT_CFloat64]

    nodatavalue = [0.5, 0.5]

    drv = gdal.GetDriverByName("GTiff")
    for i, typ in enumerate(types):
        ds = drv.Create("tmp/mask27.tif", 1, 1, 1, typ)
        ds.GetRasterBand(1).Fill(nodatavalue[i], 10)
        ds.GetRasterBand(1).SetNoDataValue(nodatavalue[i])

        assert (
            ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_NODATA
        ), "did not get expected mask flags for type %s" % gdal.GetDataTypeName(typ)

        msk = ds.GetRasterBand(1).GetMaskBand()
        assert msk.Checksum() == 0, (
            "did not get expected mask checksum for type %s : %d"
            % gdal.GetDataTypeName(typ, msk.Checksum())
        )

        msk = None
        ds = None
        drv.Delete("tmp/mask27.tif")


###############################################################################
# Test setting nodata after having first queried GetMaskBand()


@pytest.mark.parametrize("dt", [gdal.GDT_UInt8, gdal.GDT_Int64, gdal.GDT_UInt64])
@pytest.mark.parametrize(
    "GDAL_SIMUL_MEM_ALLOC_FAILURE_NODATA_MASK_BAND", [None, "YES", "ALWAYS"]
)
def test_mask_setting_nodata(dt, GDAL_SIMUL_MEM_ALLOC_FAILURE_NODATA_MASK_BAND):
    def set_nodata_value(ds, val):
        if dt == gdal.GDT_UInt8:
            ds.GetRasterBand(1).SetNoDataValue(val)
        elif dt == gdal.GDT_Int64:
            ds.GetRasterBand(1).SetNoDataValueAsInt64(val)
        else:
            ds.GetRasterBand(1).SetNoDataValueAsUInt64(val)

    def test():
        ds = gdal.GetDriverByName("MEM").Create("__debug__", 1, 1, 1, dt)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster() == struct.pack("B", 255)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster() == struct.pack("B", 255)
        set_nodata_value(ds, 0)
        got = ds.GetRasterBand(1).GetMaskBand().ReadRaster()
        if (
            GDAL_SIMUL_MEM_ALLOC_FAILURE_NODATA_MASK_BAND == "ALWAYS"
            and dt != gdal.GDT_UInt8
        ):
            assert got is None
            assert gdal.GetLastErrorType() == gdal.CE_Failure
        else:
            if (
                GDAL_SIMUL_MEM_ALLOC_FAILURE_NODATA_MASK_BAND == "YES"
                and dt != gdal.GDT_UInt8
            ):
                assert gdal.GetLastErrorType() == gdal.CE_Warning
            assert got == struct.pack("B", 0)
            assert ds.GetRasterBand(1).GetMaskBand().ReadRaster() == struct.pack("B", 0)
            set_nodata_value(ds, 1)
            assert ds.GetRasterBand(1).GetMaskBand().ReadRaster() == struct.pack(
                "B", 255
            )
            set_nodata_value(ds, 0)
            assert ds.GetRasterBand(1).GetMaskBand().ReadRaster() == struct.pack("B", 0)

        ds.GetRasterBand(1).DeleteNoDataValue()
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster() == struct.pack("B", 255)

    if GDAL_SIMUL_MEM_ALLOC_FAILURE_NODATA_MASK_BAND:
        with gdal.quiet_errors(), gdal.config_option(
            "GDAL_SIMUL_MEM_ALLOC_FAILURE_NODATA_MASK_BAND",
            GDAL_SIMUL_MEM_ALLOC_FAILURE_NODATA_MASK_BAND,
        ):
            test()
    else:
        test()


###############################################################################


@gdaltest.enable_exceptions()
def test_mask_write_to_all_valid_mask_band():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(
        Exception,
        match=r"GDALRasterBand::Fill\(\): attempt to write to an all-valid implicit mask band.",
    ):
        ds.GetRasterBand(1).GetMaskBand().Fill(0)
    with pytest.raises(
        Exception,
        match=r"GDALRasterBand::RasterIO\(\): attempt to write to an all-valid implicit mask band.",
    ):
        ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, b"\0")


###############################################################################


@gdaltest.enable_exceptions()
def test_mask_write_to_nodata_mask_band():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds.GetRasterBand(1).SetNoDataValue(0)
    with pytest.raises(
        Exception,
        match=r"GDALRasterBand::Fill\(\): attempt to write to a nodata implicit mask band.",
    ):
        ds.GetRasterBand(1).GetMaskBand().Fill(0)
    with pytest.raises(
        Exception,
        match=r"GDALRasterBand::RasterIO\(\): attempt to write to a nodata implicit mask band.",
    ):
        ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, b"\0")


###############################################################################


@gdaltest.enable_exceptions()
def test_mask_write_to_nodata_values_mask_band():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    ds.SetMetadataItem("NODATA_VALUES", "0 0")
    with pytest.raises(
        Exception,
        match=r"GDALRasterBand::Fill\(\): attempt to write to a nodata implicit mask band.",
    ):
        ds.GetRasterBand(1).GetMaskBand().Fill(0)
    with pytest.raises(
        Exception,
        match=r"GDALRasterBand::RasterIO\(\): attempt to write to a nodata implicit mask band.",
    ):
        ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, b"\0")
