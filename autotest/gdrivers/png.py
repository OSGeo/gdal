#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for PNG driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import os

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("PNG")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Read test of simple byte reference data.


def test_png_1():

    tst = gdaltest.GDALTest("PNG", "png/test.png", 1, 57921)
    tst.testOpen()


###############################################################################
# Test lossless copying.


def test_png_2():

    tst = gdaltest.GDALTest("PNG", "png/test.png", 1, 57921)

    tst.testCreateCopy()


###############################################################################
# Verify the geotransform, colormap, and nodata setting for test file.


def test_png_3():

    ds = gdal.Open("data/png/test.png")
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    assert (
        cm.GetCount() == 16
        and cm.GetColorEntry(0) == (255, 255, 255, 0)
        and cm.GetColorEntry(1) == (255, 255, 208, 255)
    ), "Wrong colormap entries"

    cm = None

    assert int(ds.GetRasterBand(1).GetNoDataValue()) == 0, "Wrong nodata value."

    # This geotransform test is also verifying the fix for bug 1414, as
    # the world file is in a mixture of numeric representations for the
    # numbers.  (mixture of "," and "." in file)

    gt_expected = (700000.305, 0.38, 0.01, 4287500.695, -0.01, -0.38)

    gt = ds.GetGeoTransform()
    for i in range(6):
        if gt[i] != pytest.approx(gt_expected[i], abs=0.0001):
            print("expected:", gt_expected)
            print("got:", gt)
            pytest.fail("Mixed locale world file read improperly.")


###############################################################################
# Test RGB mode creation and reading.


def test_png_4():

    tst = gdaltest.GDALTest("PNG", "rgbsmall.tif", 3, 21349)

    tst.testCreateCopy()


###############################################################################
# Test RGBA 16bit read support.


def test_png_5():

    tst = gdaltest.GDALTest("PNG", "png/rgba16.png", 3, 1815)
    tst.testOpen()


###############################################################################
# Test RGBA 16bit mode creation and reading.


def test_png_6():

    tst = gdaltest.GDALTest("PNG", "png/rgba16.png", 4, 4873)

    tst.testCreateCopy()


###############################################################################
# Test RGB NODATA_VALUES metadata write (and read) support.
# This is handled via the tRNS block in PNG.


def test_png_7():

    drv = gdal.GetDriverByName("PNG")
    srcds = gdal.Open("data/png/tbbn2c16.png")

    dstds = drv.CreateCopy("tmp/png7.png", srcds)
    srcds = None

    dstds = gdal.Open("tmp/png7.png")
    md = dstds.GetMetadata()
    dstds = None

    assert md["NODATA_VALUES"] == "32639 32639 32639", "NODATA_VALUES wrong"

    dstds = None

    drv.Delete("tmp/png7.png")


###############################################################################
# Test PNG file with broken IDAT chunk. This poor man test of clean
# recovery from errors caused by reading broken file..


def test_png_8(tmp_path):

    drv = gdal.GetDriverByName("PNG")
    ds_src = gdal.Open("data/png/idat_broken.png")

    md = ds_src.GetMetadata()
    assert not md, "metadata list not expected"

    # Number of bands has been preserved
    assert ds_src.RasterCount == 4, "wrong number of bands"

    # No reading is performed, so we expect valid reference
    b = ds_src.GetRasterBand(1)
    assert b is not None, "band 1 is missing"

    # We're not interested in returned value but internal state of GDAL.
    with gdal.quiet_errors():
        b.ComputeBandStats()
        err = gdal.GetLastErrorNo()

    assert err != 0, "error condition expected"

    with gdal.quiet_errors():
        ds_dst = drv.CreateCopy(tmp_path / "idat_broken.png", ds_src)
        err = gdal.GetLastErrorNo()
    ds_src = None

    assert err != 0, "error condition expected"

    assert ds_dst is None, "dataset not expected"


###############################################################################
# Test creating an in memory copy.


def test_png_9():

    tst = gdaltest.GDALTest("PNG", "byte.tif", 1, 4672)

    tst.testCreateCopy(vsimem=1)


###############################################################################
# Test writing to /vsistdout/


def test_png_10():

    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("PNG").CreateCopy(
        "/vsistdout_redirect//vsimem/tmp.png", src_ds
    )
    assert ds.GetRasterBand(1).Checksum() == 0
    src_ds = None
    ds = None

    ds = gdal.Open("/vsimem/tmp.png")
    assert ds is not None
    assert ds.GetRasterBand(1).Checksum() == 4672

    gdal.Unlink("/vsimem/tmp.png")


###############################################################################
# Test CreateCopy() interruption


def test_png_11():

    tst = gdaltest.GDALTest("PNG", "byte.tif", 1, 4672)

    tst.testCreateCopy(vsimem=1, interrupt_during_copy=True)
    gdal.Unlink("/vsimem/byte.tif.tst")


###############################################################################
# Test optimized IRasterIO


def test_png_12():
    ds = gdal.Open("../gcore/data/stefan_full_rgba.png")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]

    # Band interleaved
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    tmp_ds = gdal.GetDriverByName("Mem").Create(
        "", ds.RasterXSize, ds.RasterYSize, ds.RasterCount
    )
    tmp_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data)
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert cs == got_cs

    # Pixel interleaved
    data = ds.ReadRaster(
        0,
        0,
        ds.RasterXSize,
        ds.RasterYSize,
        buf_pixel_space=ds.RasterCount,
        buf_band_space=1,
    )
    tmp_ds = gdal.GetDriverByName("Mem").Create(
        "", ds.RasterXSize, ds.RasterYSize, ds.RasterCount
    )
    tmp_ds.WriteRaster(
        0,
        0,
        ds.RasterXSize,
        ds.RasterYSize,
        data,
        buf_pixel_space=ds.RasterCount,
        buf_band_space=1,
    )
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert cs == got_cs

    # Pixel interleaved with padding
    data = ds.ReadRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize, buf_pixel_space=5, buf_band_space=1
    )
    tmp_ds = gdal.GetDriverByName("Mem").Create(
        "", ds.RasterXSize, ds.RasterYSize, ds.RasterCount
    )
    tmp_ds.WriteRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize, data, buf_pixel_space=5, buf_band_space=1
    )
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert cs == got_cs


###############################################################################
# Test metadata


def test_png_13():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadataItem("foo", "bar")
    src_ds.SetMetadataItem("COPYRIGHT", "copyright value")
    src_ds.SetMetadataItem("DESCRIPTION", "will be overridden by creation option")
    out_ds = gdal.GetDriverByName("PNG").CreateCopy(
        "/vsimem/tmp.png",
        src_ds,
        options=["WRITE_METADATA_AS_TEXT=YES", "DESCRIPTION=my desc"],
    )
    md = out_ds.GetMetadata()
    assert (
        len(md) == 3
        and md["foo"] == "bar"
        and md["Copyright"] == "copyright value"
        and md["Description"] == "my desc"
    )
    out_ds = None
    # check that no PAM file is created
    assert gdal.VSIStatL("/vsimem/tmp.png.aux.xml") != 0
    gdal.Unlink("/vsimem/tmp.png")


###############################################################################
# Test support for nbits < 8


def test_png_14():

    src_ds = gdal.Open("../gcore/data/oddsize1bit.tif")
    expected_cs = src_ds.GetRasterBand(1).Checksum()
    gdal.GetDriverByName("PNG").CreateCopy("/vsimem/tmp.png", src_ds)
    out_ds = gdal.Open("/vsimem/tmp.png")
    cs = out_ds.GetRasterBand(1).Checksum()
    nbits = out_ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE")
    gdal.Unlink("/vsimem/tmp.png")

    assert cs == expected_cs

    assert nbits == "1"

    # check that no PAM file is created
    assert gdal.VSIStatL("/vsimem/tmp.png.aux.xml") != 0

    # Test explicit NBITS
    gdal.GetDriverByName("PNG").CreateCopy(
        "/vsimem/tmp.png", src_ds, options=["NBITS=2"]
    )
    out_ds = gdal.Open("/vsimem/tmp.png")
    nbits = out_ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE")
    gdal.Unlink("/vsimem/tmp.png")
    assert nbits == "2"

    # Test (wrong) explicit NBITS
    with gdal.quiet_errors():
        gdal.GetDriverByName("PNG").CreateCopy(
            "/vsimem/tmp.png", src_ds, options=["NBITS=7"]
        )
    out_ds = gdal.Open("/vsimem/tmp.png")
    nbits = out_ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE")
    gdal.Unlink("/vsimem/tmp.png")
    assert nbits is None


###############################################################################
# Test whole image decompression optimization


@pytest.mark.parametrize(
    "options",
    [{}, {"GDAL_PNG_WHOLE_IMAGE_OPTIM": "NO"}, {"GDAL_PNG_SINGLE_BLOCK": "NO"}],
)
@pytest.mark.parametrize("nbands", [1, 2, 3, 4])
@pytest.mark.parametrize("xsize,ysize", [(7, 8), (513, 5)])
def test_png_whole_image_optim(options, nbands, xsize, ysize):

    filename = "/vsimem/test.png"
    src_ds = gdal.GetDriverByName("MEM").Create("", xsize, ysize, nbands)
    src_ds.WriteRaster(
        0,
        0,
        src_ds.RasterXSize,
        src_ds.RasterYSize,
        array.array("B", [i % 256 for i in range(xsize * ysize * nbands)]),
    )
    gdal.GetDriverByName("PNG").CreateCopy(filename, src_ds)
    with gdaltest.config_options(options):
        ds = gdal.Open(filename)
        for i in range(nbands):
            assert (
                ds.GetRasterBand(i + 1).ReadRaster()
                == src_ds.GetRasterBand(i + 1).ReadRaster()
            )

        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()
        assert ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()

        if nbands >= 2:
            ds = gdal.Open(filename)
            assert (
                ds.GetRasterBand(2).ReadRaster() == src_ds.GetRasterBand(2).ReadRaster()
            )
            assert (
                ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()
            )

        ds = gdal.Open(filename)
        blockxsize, blockysize = ds.GetRasterBand(1).GetBlockSize()
        assert ds.GetRasterBand(1).ReadBlock(0, 0) == src_ds.GetRasterBand(
            1
        ).ReadRaster(0, 0, blockxsize, blockysize)

        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).ReadRaster(2, 3, 4, 2) == src_ds.GetRasterBand(
            1
        ).ReadRaster(2, 3, 4, 2)

        ds = gdal.Open(filename)
        assert ds.ReadRaster() == src_ds.ReadRaster()

        ds = gdal.Open(filename)
        assert ds.ReadRaster(2, 3, 4, 2) == src_ds.ReadRaster(2, 3, 4, 2)

        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).ReadRaster(
            buf_type=gdal.GDT_UInt16
        ) == src_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt16)

        ds = gdal.Open(filename)
        assert ds.ReadRaster(buf_type=gdal.GDT_UInt16) == src_ds.ReadRaster(
            buf_type=gdal.GDT_UInt16
        )

    gdal.Unlink(filename)


###############################################################################


def test_png_background_color_gray(tmp_vsimem):

    filename = tmp_vsimem / "out.png"

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadataItem("BACKGROUND_COLOR", "123")

    gdal.GetDriverByName("PNG").CreateCopy(filename, src_ds)
    gdal.Unlink(str(filename) + ".aux.xml")
    with gdal.Open(filename) as ds:
        assert ds.GetMetadataItem("BACKGROUND_COLOR") == "123"


###############################################################################


def test_png_background_color_gray_alpha(tmp_vsimem):

    filename = tmp_vsimem / "out.png"

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.SetMetadataItem("BACKGROUND_COLOR", "123")

    gdal.GetDriverByName("PNG").CreateCopy(filename, src_ds)
    gdal.Unlink(str(filename) + ".aux.xml")
    with gdal.Open(filename) as ds:
        assert ds.GetMetadataItem("BACKGROUND_COLOR") == "123"


###############################################################################


def test_png_background_color_index(tmp_vsimem):

    filename = tmp_vsimem / "out.png"

    ct_ds = gdal.Open("data/png/test.png")
    ct = ct_ds.GetRasterBand(1).GetRasterColorTable()

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.GetRasterBand(1).SetColorTable(ct)
    src_ds.SetMetadataItem("BACKGROUND_COLOR", "3")

    gdal.GetDriverByName("PNG").CreateCopy(filename, src_ds)
    gdal.Unlink(str(filename) + ".aux.xml")
    with gdal.Open(filename) as ds:
        assert ds.GetMetadataItem("BACKGROUND_COLOR") == "3"


###############################################################################


def test_png_background_color_rgb(tmp_vsimem):

    filename = tmp_vsimem / "out.png"

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)
    src_ds.SetMetadataItem("BACKGROUND_COLOR", "123,234,67")

    gdal.GetDriverByName("PNG").CreateCopy(filename, src_ds)
    gdal.Unlink(str(filename) + ".aux.xml")
    with gdal.Open(filename) as ds:
        assert ds.GetMetadataItem("BACKGROUND_COLOR") == "123,234,67"


###############################################################################


def test_png_background_color_rgba(tmp_vsimem):

    filename = tmp_vsimem / "out.png"

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 4)
    src_ds.SetMetadataItem("BACKGROUND_COLOR", "123,234,67")

    gdal.GetDriverByName("PNG").CreateCopy(filename, src_ds)
    gdal.Unlink(str(filename) + ".aux.xml")
    with gdal.Open(filename) as ds:
        assert ds.GetMetadataItem("BACKGROUND_COLOR") == "123,234,67"


###############################################################################
def test_png_copy_mdd():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadataItem("FOO", "BAR")
    src_ds.SetMetadataItem("BAR", "BAZ", "OTHER_DOMAIN")
    src_ds.SetMetadataItem("should_not", "be_copied", "IMAGE_STRUCTURE")

    filename = "/vsimem/test_png_copy_mdd.png"

    gdal.GetDriverByName("PNG").CreateCopy(filename, src_ds)
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(["", "DERIVED_SUBDATASETS"])
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {}
    ds = None

    gdal.GetDriverByName("PNG").CreateCopy(
        filename, src_ds, options=["COPY_SRC_MDD=NO"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {}
    ds = None

    gdal.GetDriverByName("PNG").CreateCopy(
        filename, src_ds, options=["COPY_SRC_MDD=YES"]
    )
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(
        ["", "DERIVED_SUBDATASETS", "OTHER_DOMAIN"]
    )
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    gdal.GetDriverByName("PNG").CreateCopy(
        filename, src_ds, options=["SRC_MDD=OTHER_DOMAIN"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    gdal.GetDriverByName("PNG").CreateCopy(
        filename, src_ds, options=["SRC_MDD=", "SRC_MDD=OTHER_DOMAIN"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Read test of 16-bit interlaced


def test_png_read_interlace_16_bit():

    ds = gdal.Open("data/png/uint16_interlaced.png")
    assert ds.GetRasterBand(1).Checksum() == 4672


###############################################################################


def test_png_create_copy_only_visible_at_close_time(tmp_path):

    src_ds = gdal.Open("data/byte.tif")
    out_filename = tmp_path / "tmp.png"

    def my_callback(pct, msg, user_data):
        if pct < 1:
            assert gdal.VSIStatL(out_filename) is None
        return True

    drv = gdal.GetDriverByName("PNG")
    assert drv.GetMetadataItem(gdal.DCAP_CREATE_ONLY_VISIBLE_AT_CLOSE_TIME) == "YES"
    drv.CreateCopy(
        out_filename,
        src_ds,
        options=["@CREATE_ONLY_VISIBLE_AT_CLOSE_TIME=YES"],
        callback=my_callback,
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("zlevel", [-1, 0, 9, 10])
def test_png_create_zlevel(tmp_vsimem, zlevel):

    src_ds = gdal.Open("data/byte.tif")
    if zlevel < 0 or zlevel > 9:
        with pytest.raises(Exception, match="Illegal ZLEVEL value"):
            gdal.GetDriverByName("PNG").CreateCopy(
                tmp_vsimem / "out.png", src_ds, options=[f"ZLEVEL={zlevel}"]
            )
    else:
        gdal.GetDriverByName("PNG").CreateCopy(
            tmp_vsimem / "out.png", src_ds, options=[f"ZLEVEL={zlevel}"]
        )


###############################################################################


def test_png_close(tmp_path):

    ds = gdal.GetDriverByName("PNG").CreateCopy(
        tmp_path / "out.png", gdal.Open("data/byte.tif")
    )
    ds.Close()
    os.remove(tmp_path / "out.png")
