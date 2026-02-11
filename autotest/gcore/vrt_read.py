#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a VRT file.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import math
import os
import shutil
import struct

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# When imported build a list of units based on the files available.


init_list = [
    ("byte.vrt", 4672),
    ("int16.vrt", 4672),
    ("uint16.vrt", 4672),
    ("int32.vrt", 4672),
    ("uint32.vrt", 4672),
    ("float32.vrt", 4672),
    ("float64.vrt", 4672),
    ("cint16.vrt", 5028),
    ("cint32.vrt", 5028),
    ("cfloat32.vrt", 5028),
    ("cfloat64.vrt", 5028),
    ("msubwinbyte.vrt", 2699),
    ("utmsmall.vrt", 50054),
    ("byte_nearest_50pct.vrt", 1192),
    ("byte_averaged_50pct.vrt", 1152),
    ("byte_nearest_200pct.vrt", 18784),
    ("byte_averaged_200pct.vrt", 18784),
]


@pytest.mark.parametrize(
    "filename,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.require_driver("VRT")
def test_vrt_open(filename, checksum):
    ut = gdaltest.GDALTest("VRT", filename, 1, checksum)
    ut.testOpen()


###############################################################################
# The VRT references a non existing TIF file


@pytest.mark.parametrize("filename", ["data/idontexist.vrt", "data/idontexist2.vrt"])
def test_vrt_read_non_existing_source(filename):

    ds = gdal.Open(filename)
    with gdal.quiet_errors():
        cs = ds.GetRasterBand(1).Checksum()
    if ds is None:
        return

    assert cs == -1

    ds.GetMetadata()
    ds.GetRasterBand(1).GetMetadata()
    ds.GetGCPs()


###############################################################################
# Test init of band data in case of cascaded VRT (ticket #2867)


def test_vrt_read_3(tmp_vsimem, tmp_path):

    driver_tif = gdal.GetDriverByName("GTIFF")

    gdal.CopyFile("data/test_mosaic.vrt", tmp_vsimem / "test_mosaic.vrt")
    gdal.CopyFile("data/test_mosaic1.vrt", tmp_vsimem / "test_mosaic1.vrt")
    gdal.CopyFile("data/test_mosaic2.vrt", tmp_vsimem / "test_mosaic2.vrt")

    output_dst = driver_tif.Create(
        tmp_vsimem / "test_mosaic1.tif", 100, 100, 3, gdal.GDT_UInt8
    )
    output_dst.GetRasterBand(1).Fill(255)
    output_dst = None

    output_dst = driver_tif.Create(
        tmp_vsimem / "test_mosaic2.tif", 100, 100, 3, gdal.GDT_UInt8
    )
    output_dst.GetRasterBand(1).Fill(127)
    output_dst = None

    ds = gdal.Open(tmp_vsimem / "test_mosaic.vrt")
    # A simple Checksum() cannot detect if the fix works or not as
    # Checksum() reads line per line, and we must use IRasterIO() on multi-line request
    data = ds.GetRasterBand(1).ReadRaster(90, 0, 20, 100)
    got = struct.unpack("B" * 20 * 100, data)
    for i in range(100):
        assert got[i * 20 + 9] == 255, "at line %d, did not find 255" % i
    ds = None


###############################################################################
# Test complex source with complex data (#3977)


def test_vrt_read_4(tmp_vsimem):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    data = np.zeros((1, 1), np.complex64)
    data[0, 0] = 1.0 + 3.0j

    drv = gdal.GetDriverByName("GTiff")
    ds = drv.Create(tmp_vsimem / "test.tif", 1, 1, 1, gdal.GDT_CFloat32)
    ds.GetRasterBand(1).WriteArray(data)
    ds = None

    complex_xml = f"""<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="CFloat32" band="1">
    <ComplexSource>
      <SourceFilename relativeToVRT="1">{tmp_vsimem}/test.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <ScaleOffset>3</ScaleOffset>
      <ScaleRatio>2</ScaleRatio>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>
"""

    ds = gdal.Open(complex_xml)
    scaleddata = ds.GetRasterBand(1).ReadAsArray()
    ds = None

    if scaleddata[0, 0].real != 5.0 or scaleddata[0, 0].imag != 9.0:
        print(
            "scaleddata[0, 0]: %f %f" % (scaleddata[0, 0].real, scaleddata[0, 0].imag)
        )
        pytest.fail("did not get expected value")


###############################################################################
# Test serializing and deserializing of various band metadata


@pytest.mark.require_driver("AAIGRID")
def test_vrt_read_5(tmp_vsimem):

    src_ds = gdal.Open("data/testserialization.asc")
    ds = gdal.GetDriverByName("VRT").CreateCopy(tmp_vsimem / "vrt_read_5.vrt", src_ds)
    src_ds = None
    ds = None

    ds = gdal.Open(tmp_vsimem / "vrt_read_5.vrt")

    gcps = ds.GetGCPs()
    assert len(gcps) == 2 and ds.GetGCPCount() == 2

    assert ds.GetGCPProjection().find("WGS 84") != -1

    ds.SetGCPs(ds.GetGCPs(), ds.GetGCPProjection())

    gcps = ds.GetGCPs()
    assert len(gcps) == 2 and ds.GetGCPCount() == 2

    assert ds.GetGCPProjection().find("WGS 84") != -1

    band = ds.GetRasterBand(1)
    assert band.GetDescription() == "MyDescription"

    assert band.GetUnitType() == "MyUnit"

    assert band.GetOffset() == 1

    assert band.GetScale() == 2

    assert band.GetRasterColorInterpretation() == gdal.GCI_PaletteIndex

    assert band.GetCategoryNames() == ["Cat1", "Cat2"]

    ct = band.GetColorTable()
    assert ct.GetColorEntry(0) == (0, 0, 0, 255)
    assert ct.GetColorEntry(1) == (1, 1, 1, 255)

    assert band.GetMaximum() == 0

    assert band.GetMinimum() == 2

    assert band.GetMetadata() == {
        "STATISTICS_MEAN": "1",
        "STATISTICS_MINIMUM": "2",
        "STATISTICS_MAXIMUM": "0",
        "STATISTICS_STDDEV": "3",
    }

    ds = None


###############################################################################
# Test GetMinimum() and GetMaximum()


def test_vrt_read_6(tmp_vsimem):

    gdal.CopyFile("data/byte.tif", tmp_vsimem / "byte.tif")
    src_ds = gdal.Open(tmp_vsimem / "byte.tif")
    mem_ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tmp_vsimem / "vrt_read_6.tif", src_ds
    )
    vrt_ds = gdal.GetDriverByName("VRT").CreateCopy(
        tmp_vsimem / "vrt_read_6.vrt", mem_ds
    )

    assert vrt_ds.GetRasterBand(1).GetMinimum() is None, "got bad minimum value"
    assert vrt_ds.GetRasterBand(1).GetMaximum() is None, "got bad maximum value"

    # Now compute source statistics
    mem_ds.GetRasterBand(1).ComputeStatistics(False)

    assert vrt_ds.GetRasterBand(1).GetMinimum() == 74, "got bad minimum value"
    assert vrt_ds.GetRasterBand(1).GetMaximum() == 255, "got bad maximum value"

    mem_ds = None
    vrt_ds = None


###############################################################################
# Test GetMinimum() and GetMaximum()


def test_vrt_read_min_max_several_sources(tmp_path):

    src1 = str(tmp_path / "left.tif")
    src2 = str(tmp_path / "right.tif")
    vrt = str(tmp_path / "tmp.vrt")
    ds = gdal.Translate(src1, "data/byte.tif", srcWin=[0, 0, 10, 20])
    ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None
    ds = gdal.Translate(src2, "data/byte.tif", srcWin=[10, 0, 10, 20])
    ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None
    ds = gdal.BuildVRT(vrt, [src1, src2])
    assert ds.GetRasterBand(1).GetMinimum() == 74
    assert ds.GetRasterBand(1).GetMaximum() == 255


###############################################################################
# Test GDALOpen() anti-recursion mechanism


def test_vrt_read_7(tmp_vsimem):

    filename = tmp_vsimem / "vrt_read_7.vrt"

    content = f"""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">{filename}</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""

    gdal.FileFromMemBuffer(filename, content)
    ds = gdal.Open(filename)
    with gdaltest.error_raised(gdal.CE_Failure, "Recursion detected"):
        assert ds.GetRasterBand(1).Checksum() == -1


###############################################################################
# Test ComputeRasterMinMax()


def test_vrt_read_8(tmp_vsimem):

    src_ds = gdal.Open("data/byte.tif")
    mem_ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tmp_vsimem / "vrt_read_8.tif", src_ds
    )
    vrt_ds = gdal.GetDriverByName("VRT").CreateCopy(
        tmp_vsimem / "vrt_read_8.vrt", mem_ds
    )

    vrt_minmax = vrt_ds.GetRasterBand(1).ComputeRasterMinMax()
    mem_minmax = mem_ds.GetRasterBand(1).ComputeRasterMinMax()

    mem_ds = None
    vrt_ds = None

    assert vrt_minmax == mem_minmax


###############################################################################
# Test ComputeStatistics()


def test_vrt_read_9(tmp_vsimem):

    src_ds = gdal.Open("data/byte.tif")
    mem_ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tmp_vsimem / "vrt_read_9.tif", src_ds
    )
    vrt_ds = gdal.GetDriverByName("VRT").CreateCopy(
        tmp_vsimem / "vrt_read_9.vrt", mem_ds
    )

    vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)
    mem_stats = mem_ds.GetRasterBand(1).ComputeStatistics(False)

    mem_ds = None
    vrt_ds = None

    assert vrt_stats == mem_stats


###############################################################################
# Test GetHistogram() & GetDefaultHistogram()


def test_vrt_read_10(tmp_vsimem):

    src_ds = gdal.Open("data/byte.tif")
    mem_ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tmp_vsimem / "vrt_read_10.tif", src_ds
    )
    vrt_ds = gdal.GetDriverByName("VRT").CreateCopy(
        tmp_vsimem / "vrt_read_10.vrt", mem_ds
    )

    vrt_hist = vrt_ds.GetRasterBand(1).GetHistogram()
    mem_hist = mem_ds.GetRasterBand(1).GetHistogram()

    mem_ds = None
    vrt_ds = None

    f = gdal.VSIFOpenL(tmp_vsimem / "vrt_read_10.vrt", "rb")
    content = gdal.VSIFReadL(1, 10000, f).decode("ascii")
    gdal.VSIFCloseL(f)

    assert vrt_hist == mem_hist

    assert "<Histograms>" in content

    # Single source optimization
    for i in range(2):
        gdal.FileFromMemBuffer(
            tmp_vsimem / "vrt_read_10.vrt",
            """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relativeToVRT="1">vrt_read_10.tif</SourceFilename>
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>""",
        )

        ds = gdal.Open(tmp_vsimem / "vrt_read_10.vrt")
        if i == 0:
            ds.GetRasterBand(1).GetDefaultHistogram()
        else:
            ds.GetRasterBand(1).GetHistogram()
        ds = None

        f = gdal.VSIFOpenL(tmp_vsimem / "vrt_read_10.vrt", "rb")
        content = gdal.VSIFReadL(1, 10000, f).decode("ascii")
        gdal.VSIFCloseL(f)

        assert "<Histograms>" in content

    # Two sources general case
    for i in range(2):
        gdal.FileFromMemBuffer(
            tmp_vsimem / "vrt_read_10.vrt",
            """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relativeToVRT="1">vrt_read_10.tif</SourceFilename>
        </SimpleSource>
        <SimpleSource>
        <SourceFilename relativeToVRT="1">vrt_read_10.tif</SourceFilename>
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>""",
        )

        ds = gdal.Open(tmp_vsimem / "vrt_read_10.vrt")
        if i == 0:
            ds.GetRasterBand(1).GetDefaultHistogram()
        else:
            ds.GetRasterBand(1).GetHistogram()
        ds = None

        f = gdal.VSIFOpenL(tmp_vsimem / "vrt_read_10.vrt", "rb")
        content = gdal.VSIFReadL(1, 10000, f).decode("ascii")
        gdal.VSIFCloseL(f)

        assert "<Histograms>" in content


###############################################################################
# Test resolving files from a symlinked vrt using relativeToVRT with an absolute symlink


def test_vrt_read_11(tmp_path):

    if not gdaltest.support_symlink():
        pytest.skip()

    os.symlink(os.path.join(os.getcwd(), "data/byte.vrt"), tmp_path / "byte.vrt")

    ds = gdal.Open(tmp_path / "byte.vrt")

    assert ds is not None


###############################################################################
# Test resolving files from a symlinked vrt using relativeToVRT
# with a relative symlink pointing to a relative symlink


def test_vrt_read_12(tmp_path):

    if not gdaltest.support_symlink():
        pytest.skip()

    try:
        os.remove("tmp/byte.vrt")
        print("Removed tmp/byte.vrt. Was not supposed to exist...")
    except OSError:
        pass

    os.symlink("../data/byte.vrt", "tmp/byte.vrt")

    ds = gdal.Open("tmp/byte.vrt")

    os.remove("tmp/byte.vrt")

    assert ds is not None


###############################################################################
# Test resolving files from a symlinked vrt using relativeToVRT with a relative symlink


def test_vrt_read_13():

    if not gdaltest.support_symlink():
        pytest.skip()

    try:
        os.remove("tmp/byte.vrt")
        print("Removed tmp/byte.vrt. Was not supposed to exist...")
    except OSError:
        pass
    try:
        os.remove("tmp/other_byte.vrt")
        print("Removed tmp/other_byte.vrt. Was not supposed to exist...")
    except OSError:
        pass

    os.symlink("../data/byte.vrt", "tmp/byte.vrt")
    os.symlink("../tmp/byte.vrt", "tmp/other_byte.vrt")

    ds = gdal.Open("tmp/other_byte.vrt")

    os.remove("tmp/other_byte.vrt")
    os.remove("tmp/byte.vrt")

    assert ds is not None


###############################################################################
# Test ComputeStatistics() when the VRT is a subwindow of the source dataset (#5468)


def test_vrt_read_14(tmp_vsimem):

    src_ds = gdal.Open("data/byte.tif")
    mem_ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tmp_vsimem / "vrt_read_14.tif", src_ds
    )
    mem_ds.FlushCache()  # hum this should not be necessary ideally
    vrt_ds = gdal.Open(f"""<VRTDataset rasterXSize="4" rasterYSize="4">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">{tmp_vsimem}/vrt_read_14.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="2" yOff="2" xSize="4" ySize="4" />
      <DstRect xOff="0" yOff="0" xSize="4" ySize="4" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)

    mem_ds = None
    vrt_ds = None

    assert vrt_stats[0] == 115.0 and vrt_stats[1] == 173.0


###############################################################################
# Test RasterIO() with resampling on SimpleSource


def test_vrt_read_15():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="9" rasterYSize="9">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 1044


###############################################################################
# Test RasterIO() with resampling on ComplexSource


def test_vrt_read_16():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="9" rasterYSize="9">
  <VRTRasterBand dataType="Byte" band="1">
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </ComplexSource>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")

    cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 1044


###############################################################################
# Test RasterIO() with resampling on AveragedSource


def test_vrt_read_17():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="9" rasterYSize="9">
  <VRTRasterBand dataType="Byte" band="1">
    <AveragedSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </AveragedSource>
  </VRTRasterBand>
</VRTDataset>""")

    # Note: AveragedSource with resampling does not give consistent results
    # depending on the RasterIO() request
    mem_ds = gdal.GetDriverByName("MEM").CreateCopy("", vrt_ds)
    cs = mem_ds.GetRasterBand(1).Checksum()
    assert cs == 799


###############################################################################
# Test that relative path is correctly VRT-in-VRT


def test_vrt_read_18():

    vrt_ds = gdal.Open("data/vrtinvrt.vrt")
    cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 4672


###############################################################################
# Test shared="0"


def test_vrt_read_19():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <AveragedSource>
      <SourceFilename relativeToVRT="0" shared="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
    </AveragedSource>
  </VRTRasterBand>
</VRTDataset>""")

    vrt2_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <AveragedSource>
      <SourceFilename relativeToVRT="0" shared="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </AveragedSource>
  </VRTRasterBand>
</VRTDataset>""")

    cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 4672

    cs = vrt2_ds.GetRasterBand(1).Checksum()
    assert cs == 4672


###############################################################################
# Test 2 level of VRT with shared="0"


def test_vrt_read_20(tmp_path):

    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    shutil.copy("data/byte.tif", tmp_path)
    for i in range(3):
        open(tmp_path / f"byte1_{i + 1}.vrt", "wt").write(
            """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relativeToVRT="1">byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""
        )
    open(tmp_path / "byte2.vrt", "wt").write(
        """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">byte1_1.vrt</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">byte1_2.vrt</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">byte1_3.vrt</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""
    )
    ret = gdaltest.runexternal(
        test_cli_utilities.get_gdalinfo_path()
        + f" -checksum {tmp_path}/byte2.vrt --config VRT_SHARED_SOURCE 0 --config GDAL_MAX_DATASET_POOL_SIZE 3"
    )
    assert "Checksum=4672" in ret


###############################################################################
# Test implicit virtual overviews


def test_vrt_read_21(tmp_vsimem):

    ds = gdal.Open("data/byte.tif")
    data = ds.ReadRaster(0, 0, 20, 20, 400, 400)
    ds = None
    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "byte.tif", 400, 400)
    ds.WriteRaster(0, 0, 400, 400, data)
    ds.BuildOverviews("NEAR", [2])
    ds = None

    gdal.FileFromMemBuffer(
        tmp_vsimem / "vrt_read_21.vrt",
        f"""<VRTDataset rasterXSize="800" rasterYSize="800">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename>{tmp_vsimem}/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="400" RasterYSize="400" DataType="Byte" BlockXSize="400" BlockYSize="1" />
      <SrcRect xOff="100" yOff="100" xSize="200" ySize="250" />
      <DstRect xOff="300" yOff="400" xSize="200" ySize="250" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""",
    )
    ds = gdal.Open(tmp_vsimem / "vrt_read_21.vrt")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    data_ds_one_band = ds.ReadRaster(0, 0, 800, 800, 400, 400)
    ds = None

    gdal.FileFromMemBuffer(
        tmp_vsimem / "vrt_read_21.vrt",
        f"""<VRTDataset rasterXSize="800" rasterYSize="800">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename>{tmp_vsimem}/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="400" RasterYSize="400" DataType="Byte" BlockXSize="400" BlockYSize="1" />
      <SrcRect xOff="100" yOff="100" xSize="200" ySize="250" />
      <DstRect xOff="300" yOff="400" xSize="200" ySize="250" />
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="400" RasterYSize="400" DataType="Byte" BlockXSize="400" BlockYSize="1" />
      <SrcRect xOff="100" yOff="100" xSize="200" ySize="250" />
      <DstRect xOff="300" yOff="400" xSize="200" ySize="250" />
      <ScaleOffset>10</ScaleOffset>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""",
    )
    ds = gdal.Open(tmp_vsimem / "vrt_read_21.vrt")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    ds = gdal.Open(tmp_vsimem / "vrt_read_21.vrt")
    ovr_band = ds.GetRasterBand(1).GetOverview(-1)
    assert ovr_band is None
    ovr_band = ds.GetRasterBand(1).GetOverview(1)
    assert ovr_band is None
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    assert ovr_band is not None
    cs = ovr_band.Checksum()
    cs2 = ds.GetRasterBand(2).GetOverview(0).Checksum()

    data = ds.ReadRaster(0, 0, 800, 800, 400, 400)

    assert data == data_ds_one_band + ds.GetRasterBand(2).ReadRaster(
        0, 0, 800, 800, 400, 400
    )

    mem_ds = gdal.GetDriverByName("MEM").Create("", 400, 400, 2)
    mem_ds.WriteRaster(0, 0, 400, 400, data)
    ref_cs = mem_ds.GetRasterBand(1).Checksum()
    ref_cs2 = mem_ds.GetRasterBand(2).Checksum()
    mem_ds = None
    assert cs == ref_cs
    assert cs2 == ref_cs2

    ds.BuildOverviews("NEAR", [2])
    expected_cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs2 = ds.GetRasterBand(2).GetOverview(0).Checksum()
    ds = None

    assert cs == expected_cs
    assert cs2 == expected_cs2


###############################################################################
# Test implicit virtual overviews


def test_vrt_read_virtual_overview_no_srcrect_dstrect(tmp_vsimem):

    ds = gdal.Open("data/byte.tif")
    data = ds.ReadRaster(0, 0, 20, 20, 400, 400)
    ds = None
    tmp_tif = str(tmp_vsimem / "tmp.tif")
    ds = gdal.GetDriverByName("GTiff").Create(tmp_tif, 400, 400)
    ds.WriteRaster(0, 0, 400, 400, data)
    ds.BuildOverviews("NEAR", [2])
    ref_cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    ds = gdal.Open(f"""<VRTDataset rasterXSize="400" rasterYSize="400">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename>{tmp_tif}</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == ref_cs


###############################################################################
# Test that we honour NBITS with SimpleSource and ComplexSource


def test_vrt_read_22(tmp_vsimem):

    ds = gdal.Open("data/byte.tif")
    data = ds.ReadRaster()
    ds = None
    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "byte.tif", 20, 20)
    ds.WriteRaster(0, 0, 20, 20, data)
    ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None

    ds = gdal.Open(f"""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <Metadata domain="IMAGE_STRUCTURE">
        <MDI key="NBITS">6</MDI>
    </Metadata>
    <SimpleSource>
      <SourceFilename>{tmp_vsimem}/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    assert ds.GetRasterBand(1).GetMinimum() == 63

    assert ds.GetRasterBand(1).GetMaximum() == 63

    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (63, 63)

    assert ds.GetRasterBand(1).ComputeStatistics(False) == [63.0, 63.0, 63.0, 0.0]

    data = ds.ReadRaster()
    got = struct.unpack("B" * 20 * 20, data)
    assert got[0] == 63

    ds = gdal.Open(f"""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <Metadata domain="IMAGE_STRUCTURE">
        <MDI key="NBITS">6</MDI>
    </Metadata>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    assert ds.GetRasterBand(1).GetMinimum() == 63

    assert ds.GetRasterBand(1).GetMaximum() == 63

    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (63, 63)

    assert ds.GetRasterBand(1).ComputeStatistics(False) == [63.0, 63.0, 63.0, 0.0]

    ds = gdal.Open(f"""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <Metadata domain="IMAGE_STRUCTURE">
        <MDI key="NBITS">6</MDI>
    </Metadata>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <ScaleOffset>10</ScaleOffset>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    assert ds.GetRasterBand(1).GetMinimum() is None

    assert ds.GetRasterBand(1).GetMaximum() is None

    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (63, 63)

    assert ds.GetRasterBand(1).ComputeStatistics(False) == [63.0, 63.0, 63.0, 0.0]


###############################################################################
# Test non-nearest resampling on a VRT exposing a nodata value but with
# an underlying dataset without nodata


def test_vrt_read_23(tmp_vsimem):

    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")

    mem_ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "vrt_read_23.tif", 2, 1)
    mem_ds.GetRasterBand(1).WriteArray(numpy.array([[0, 10]]))
    mem_ds = None
    ds = gdal.Open(f"""<VRTDataset rasterXSize="2" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <NoDataValue>0</NoDataValue>
    <SimpleSource>
      <SourceFilename>{tmp_vsimem}/vrt_read_23.tif</SourceFilename>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    got_ar = ds.GetRasterBand(1).ReadAsArray(
        0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear
    )
    assert list(got_ar[0]) == [0, 10, 10, 10]
    assert ds.ReadRaster(
        0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear
    ) == ds.GetRasterBand(1).ReadRaster(
        0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear
    )
    ds = None


def test_vrt_read_23a(tmp_vsimem):

    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")

    # Same but with nodata set on source band too
    mem_ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "vrt_read_23.tif", 2, 1)
    mem_ds.GetRasterBand(1).SetNoDataValue(0)
    mem_ds.GetRasterBand(1).WriteArray(numpy.array([[0, 10]]))
    mem_ds = None
    ds = gdal.Open(f"""<VRTDataset rasterXSize="2" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <NoDataValue>0</NoDataValue>
    <SimpleSource>
      <SourceFilename>{tmp_vsimem}/vrt_read_23.tif</SourceFilename>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    got_ar = ds.GetRasterBand(1).ReadAsArray(
        0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear
    )
    assert list(got_ar[0]) == [0, 10, 10, 10]
    assert ds.ReadRaster(
        0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear
    ) == ds.GetRasterBand(1).ReadRaster(
        0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear
    )
    ds = None


###############################################################################
# Test floating point rounding issues when the VRT does a zoom-in


def test_vrt_read_24():

    ds = gdal.Open("data/zoom_in.vrt")
    data = ds.ReadRaster(34, 5, 66, 87)
    ds = None

    ds = gdal.GetDriverByName("MEM").Create("", 66, 87)
    ds.WriteRaster(0, 0, 66, 87, data)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Please do not change the expected checksum without checking that
    # the result image has no vertical black line in the middle
    assert cs == 46612
    ds = None


###############################################################################
# Test GetDataCoverageStatus()


@pytest.mark.require_geos
def test_vrt_read_25():

    ds = gdal.Open("""<VRTDataset rasterXSize="2000" rasterYSize="200">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="1000" yOff="30" xSize="10" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="1010" yOff="30" xSize="10" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(0, 0, 20, 20)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(1005, 35, 10, 10)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(100, 100, 20, 20)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 0.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(10, 10, 20, 20)
    assert (
        flags
        == gdal.GDAL_DATA_COVERAGE_STATUS_DATA | gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY
        and pct == 25.0
    )


###############################################################################
# Test GetDataCoverageStatus() on a single source covering the whole dataset


def test_vrt_read_get_data_coverage_status_single_source(tmp_vsimem):

    tmp_gtiff = str(tmp_vsimem / "tmp.tif")
    ds = gdal.GetDriverByName("GTiff").Create(
        tmp_gtiff,
        512,
        512,
        1,
        options=["TILED=YES", "BLOCKXSIZE=256", "BLOCKYSIZE=256", "SPARSE_OK=YES"],
    )
    ds.WriteRaster(256, 256, 256, 256, b"\x01" * (256 * 256))
    ds = None

    tmp_vrt = str(tmp_vsimem / "tmp.vrt")
    gdal.Translate(tmp_vrt, tmp_gtiff, format="VRT")
    ds = gdal.Open(tmp_vrt)

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(0, 0, 512, 512)
    assert (
        flags
        == (gdal.GDAL_DATA_COVERAGE_STATUS_DATA | gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY)
        and pct == 25.0
    )

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(0, 0, 256, 256)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 0.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(256, 256, 256, 256)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(0, 256, 512, 256)
    assert (
        flags
        == (gdal.GDAL_DATA_COVERAGE_STATUS_DATA | gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY)
        and pct == 50.0
    )


###############################################################################
# Test consistency of RasterIO() with resampling, that is extracting different
# sub-windows give consistent results


def test_vrt_read_26():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="22" rasterYSize="22">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="22" ySize="22" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    full_data = vrt_ds.GetRasterBand(1).ReadRaster(0, 0, 22, 22)
    full_data = struct.unpack("B" * 22 * 22, full_data)

    partial_data = vrt_ds.GetRasterBand(1).ReadRaster(1, 1, 1, 1)
    partial_data = struct.unpack("B" * 1 * 1, partial_data)

    assert partial_data[0] == full_data[22 + 1]


###############################################################################
# Test fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1553


def test_vrt_read_27():

    gdal.Open("data/empty_gcplist.vrt")


###############################################################################
# Test fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1551


def test_vrt_read_28():

    with gdal.quiet_errors():
        ds = gdal.Open(
            '<VRTDataset rasterXSize="1 "rasterYSize="1"><VRTRasterBand band="-2147483648"><SimpleSource></SimpleSource></VRTRasterBand></VRTDataset>'
        )
    assert ds is None


###############################################################################
# Check VRT source sharing and non-sharing situations (#6939)


def test_vrt_read_29(tmp_path):

    f = open("data/byte.tif")
    lst_before = sorted(gdaltest.get_opened_files())
    if not lst_before:
        pytest.skip()
    f.close()

    gdal.Translate(tmp_path / "vrt_read_29.tif", "data/byte.tif")

    vrt_text = f"""<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename>{tmp_path}/vrt_read_29.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2">
        <SimpleSource>
        <SourceFilename>{tmp_path}/vrt_read_29.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""

    lst_before = sorted(gdaltest.get_opened_files())
    ds = gdal.Open(vrt_text)
    # Just after opening, we shouldn't have read the source
    lst = sorted(gdaltest.get_opened_files())
    assert lst == lst_before

    # Check that the 2 bands share the same source handle
    ds.GetRasterBand(1).Checksum()
    lst = sorted(gdaltest.get_opened_files())
    assert len(lst) == len(lst_before) + 1
    ds.GetRasterBand(2).Checksum()
    lst = sorted(gdaltest.get_opened_files())
    assert len(lst) == len(lst_before) + 1

    # Open a second VRT dataset handle
    ds2 = gdal.Open(vrt_text)

    # Check that it consumes an extra handle (don't share sources between
    # different VRT)
    ds2.GetRasterBand(1).Checksum()
    lst = sorted(gdaltest.get_opened_files())
    assert len(lst) == len(lst_before) + 2

    # Close first VRT dataset, and check that the handle it took on the TIFF
    # is released (https://github.com/OSGeo/gdal/issues/3253)
    ds = None
    lst = sorted(gdaltest.get_opened_files())
    assert len(lst) == len(lst_before) + 1


###############################################################################
# Check VRT reading with DatasetRasterIO


def test_vrt_read_30():

    ds = gdal.Open("""<VRTDataset rasterXSize="2" rasterYSize="2">
  <VRTRasterBand dataType="Byte" band="1">
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
  </VRTRasterBand>
</VRTDataset>""")

    data = ds.ReadRaster(
        0, 0, 2, 2, 2, 2, buf_pixel_space=3, buf_line_space=2 * 3, buf_band_space=1
    )
    got = struct.unpack("B" * 2 * 2 * 3, data)
    for i in range(2 * 2 * 3):
        assert got[i] == 0
    ds = None


###############################################################################
# Check that we take into account intermediate data type demotion


@pytest.mark.require_driver("AAIGRID")
def test_vrt_read_31(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "in.asc",
        """ncols        2
nrows        2
xllcorner    0
yllcorner    0
dx           1
dy           1
-255         1
254          256""",
    )

    ds = gdal.Translate(
        "", tmp_vsimem / "in.asc", outputType=gdal.GDT_UInt8, format="VRT"
    )

    data = ds.GetRasterBand(1).ReadRaster(0, 0, 2, 2, buf_type=gdal.GDT_Float32)
    got = struct.unpack("f" * 2 * 2, data)
    assert got == (0, 1, 254, 255)

    data = ds.ReadRaster(0, 0, 2, 2, buf_type=gdal.GDT_Float32)
    got = struct.unpack("f" * 2 * 2, data)
    assert got == (0, 1, 254, 255)

    ds = None


###############################################################################
# Test reading a VRT where the NODATA & NoDataValue are slightly below the
# minimum float value (https://github.com/OSGeo/gdal/issues/1071)


def test_vrt_float32_with_nodata_slightly_below_float_min(tmp_vsimem):

    gdal.CopyFile("data/minfloat.tif", tmp_vsimem / "minfloat.tif")
    gdal.CopyFile(
        "data/minfloat_nodata_slightly_out_of_float.vrt",
        tmp_vsimem / "minfloat_nodata_slightly_out_of_float.vrt",
    )

    ds = gdal.Open(tmp_vsimem / "minfloat_nodata_slightly_out_of_float.vrt")
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None

    vrt_content = gdal.VSIFile(
        tmp_vsimem / "minfloat_nodata_slightly_out_of_float.vrt", "rt"
    ).read()

    # Check that the values were 'normalized' when regenerating the VRT
    assert (
        "-3.402823466385289" not in vrt_content
    ), "did not get expected nodata in rewritten VRT"

    if nodata != -3.4028234663852886e38:
        print("%.17g" % nodata)
        pytest.fail("did not get expected nodata")

    assert stats == [-3.0, 5.0, 1.0, 4.0], "did not get expected stats"


###############################################################################
# Fix issue raised in https://lists.osgeo.org/pipermail/gdal-dev/2018-December/049415.html


def test_vrt_subpixel_offset():

    ds = gdal.Open("data/vrt_subpixel_offset.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4849


###############################################################################
# Check bug fix of bug fix of
# https://lists.osgeo.org/pipermail/gdal-dev/2018-December/049415.html


def test_vrt_dstsize_larger_than_source():

    ds = gdal.Open("data/dstsize_larger_than_source.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 33273


def test_vrt_invalid_srcrect():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relative="1">data/byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="-10" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""
    assert gdal.Open(vrt_text) is None


def test_vrt_invalid_dstrect():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relative="1">data/byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="1e400" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""
    assert gdal.Open(vrt_text) is None


def test_vrt_no_explicit_dataAxisToSRSAxisMapping():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <SRS>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]</SRS>
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relative="1">data/byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert ds.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [2, 1]
    ds = None


def test_vrt_explicit_dataAxisToSRSAxisMapping_1_2():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <SRS dataAxisToSRSAxisMapping="1,2">GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]</SRS>
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relative="1">data/byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert ds.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [1, 2]
    ds = None


def test_vrt_shared_no_proxy_pool():

    before = gdaltest.get_opened_files()
    vrt_text = """<VRTDataset rasterXSize="50" rasterYSize="50">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Red</ColorInterp>
    <SimpleSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Green</ColorInterp>
    <SimpleSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>2</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <ColorInterp>Blue</ColorInterp>
    <SimpleSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>3</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert ds
    assert ds.GetRasterBand(1).Checksum() == 21212
    assert ds.GetRasterBand(2).Checksum() == 21053
    assert ds.GetRasterBand(3).Checksum() == 21349
    ds = None

    after = gdaltest.get_opened_files()

    if len(before) != len(after) and (
        gdaltest.is_travis_branch("trusty_clang")
        or gdaltest.is_travis_branch("trusty_32bit")
        or gdaltest.is_travis_branch("ubuntu_1604")
    ):
        pytest.xfail("Mysterious failure")

    assert len(before) == len(after)


def test_vrt_invalid_source_band():

    vrt_text = """<VRTDataset rasterXSize="50" rasterYSize="50">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>10</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    with gdal.quiet_errors():
        assert ds.GetRasterBand(1).Checksum() == -1


@gdaltest.enable_exceptions()
def test_vrt_protocol():

    files_opened_start = gdaltest.get_opened_files()

    with pytest.raises(Exception):
        gdal.Open("vrt://")
    with pytest.raises(Exception):
        gdal.Open("vrt://i_do_not_exist")
    with pytest.raises(Exception):
        gdal.Open("vrt://i_do_not_exist?")

    ds = gdal.Open("vrt://data/byte.tif")
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672

    with pytest.raises(Exception):
        gdal.Open("vrt://data/byte.tif?foo=bar")
    with pytest.raises(Exception):
        gdal.Open("vrt://data/byte.tif?bands=foo")
    with pytest.raises(Exception):
        gdal.Open("vrt://data/byte.tif?bands=0")
    with pytest.raises(Exception):
        gdal.Open("vrt://data/byte.tif?bands=2")

    ds = gdal.Open("vrt://data/byte.tif?bands=1,mask,1")
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(2).Checksum() == 4873
    assert ds.GetRasterBand(3).Checksum() == 4672

    ds = gdal.Open("vrt://data/byte.tif?bands=1&a_srs=EPSG:3031")
    crs = ds.GetSpatialRef()
    assert crs.GetAuthorityCode(None) == "3031"

    ds = gdal.Open("vrt://data/byte.tif?a_ullr=0,10,20,0")
    geotransform = ds.GetGeoTransform()
    assert geotransform == (0.0, 1.0, 0.0, 10.0, 0.0, -0.5)

    # test #7282
    ds = gdal.Open("vrt://data/byte_with_ovr.tif?ovr=0")
    assert ds.RasterXSize == 10

    ds = gdal.Open("vrt://data/int32.tif?a_scale=2")
    assert ds.GetRasterBand(1).GetScale() == 2.0

    ds = gdal.Open("vrt://data/int32.tif?a_offset=-132")
    assert ds.GetRasterBand(1).GetOffset() == -132.0

    ds = gdal.Open("vrt://data/float32.tif?ot=Int32")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int32

    ## 5-element GCP
    ds = gdal.Open("vrt://data/float32.tif?gcp=0,0,0,10,0")
    assert ds.GetGCPCount() == 1

    ## multiple GCPs
    ds = gdal.Open("vrt://data/float32.tif?gcp=0,0,0,10,0&gcp=20,0,10,10,0")
    assert ds.GetGCPCount() == 2

    ## 4-element GCP also ok
    ds = gdal.Open("vrt://data/float32.tif?gcp=0,0,0,10&gcp=20,0,10,10")
    assert ds.GetGCPCount() == 2

    with pytest.raises(Exception, match="Invalid value for GCP"):
        gdal.Open("vrt://data/float32.tif?gcp=invalid")

    ## not compatible, or no such driver
    with pytest.raises(Exception):
        gdal.Open("vrt://data/float32.tif?if=AAIGrid,doesnotexist")

    ## compatible driver included
    ds = gdal.Open("vrt://data/float32.tif?if=AAIGrid,GTiff")
    assert ds is not None

    ## separated if not allowed
    with pytest.raises(Exception):
        gdal.Open("vrt://data/float32.tif?if=AAIGrid&if=GTiff")

    ## check exponent and scale
    ds = gdal.Open("vrt://data/float32.tif?scale=0,255,255,255")
    assert ds.GetRasterBand(1).Checksum() == 4873

    ds = gdal.Open("vrt://data/uint16_3band.vrt?scale_2=0,255,255,255")
    assert ds.GetRasterBand(2).Checksum() == 5047

    ds = gdal.Open("vrt://data/float32.tif?scale_1=0,10")
    assert ds.GetRasterBand(1).Checksum() == 4151

    with pytest.raises(Exception):
        gdal.Open("vrt://data/float32.tif?exponent=2.2")

    ds = gdal.Open("vrt://data/float32.tif?exponent=2.2&scale=0,100")
    assert ds.GetRasterBand(1).Checksum() == 400

    ds = gdal.Open("vrt://data/uint16_3band.vrt?exponent_2=2.2&scale_2=0,10,0,100")
    assert ds.GetRasterBand(2).Checksum() == 4455

    with pytest.raises(Exception):
        gdal.Open("vrt://data/float32.tif?outsize=10")

    ds = gdal.Open("vrt://data/float32.tif?outsize=10,5")
    assert ds.GetRasterBand(1).XSize == 10
    assert ds.GetRasterBand(1).YSize == 5

    ds = gdal.Open("vrt://data/float32.tif?outsize=50%,25%")
    assert ds.GetRasterBand(1).XSize == 10
    assert ds.GetRasterBand(1).YSize == 5

    with pytest.raises(Exception):
        gdal.Open("vrt://data/float32.tif?projwin=440840,441920,3750120")

    ds = gdal.Open("vrt://data/float32.tif?projwin=440840,3751080,441920,3750120")
    assert ds.GetGeoTransform()[0] == 440840.0
    assert ds.GetGeoTransform()[3] == 3751080.0
    assert ds.GetRasterBand(1).XSize == 18
    assert ds.GetRasterBand(1).YSize == 16

    ## no op, simply ignored as with gdal_translate
    ds = gdal.Open("vrt://data/float32.tif?projwin_srs=OGC:CRS84")
    assert ds is not None

    ds = gdal.Open(
        "vrt://data/float32.tif?projwin_srs=OGC:CRS84&projwin=-117.6407,33.90027,-117.6292,33.89181"
    )

    assert ds.GetGeoTransform()[0] == 440780.0
    assert ds.GetGeoTransform()[3] == 3751140.0
    assert ds.GetRasterBand(1).XSize == 19
    assert ds.GetRasterBand(1).YSize == 17

    with pytest.raises(Exception):
        gdal.Open("vrt://data/float32.tif?tr=120")

    ds = gdal.Open("vrt://data/float32.tif?tr=120,240")

    assert ds.GetGeoTransform()[0] == 440720.0
    assert ds.GetGeoTransform()[1] == 120.0
    assert ds.GetGeoTransform()[5] == -240.0
    assert ds.GetRasterBand(1).XSize == 10
    assert ds.GetRasterBand(1).YSize == 5

    ds = gdal.Open("vrt://data/float32.tif?r=bilinear&tr=120,240")
    assert struct.unpack("f", ds.ReadRaster(0, 0, 1, 1))[0] == pytest.approx(
        128.95408630371094
    )  ## check values changed via bilinear

    with pytest.raises(Exception):
        gdal.Open("vrt://data/float32.tif?srcwin=0,0,3")

    ds = gdal.Open("vrt://data/float32.tif?srcwin=2,3,8,5")
    assert ds.GetRasterBand(1).XSize == 8
    assert ds.GetRasterBand(1).YSize == 5
    assert struct.unpack("f", ds.ReadRaster(0, 0, 1, 1))[0] == pytest.approx(
        123.0
    )  ## check value is correct

    with pytest.raises(Exception):
        gdal.Open("vrt://data/float32.tif?a_gt=1,0,0,0,1")

    ds = gdal.Open("vrt://data/float32.tif?a_gt=0,1,0,0,0,1")
    gdaltest.check_geotransform(
        (0, 1, 0, 0, 0, 1),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = gdal.Open("vrt://data/float32.tif?a_nodata=-9999")
    assert ds.GetRasterBand(1).GetNoDataValue() == -9999.0

    ## multiple open options
    ds = gdal.Open(
        "vrt://data/byte_with_ovr.tif?oo=GEOREF_SOURCES=TABFILE,OVERVIEW_LEVEL=0"
    )
    gdaltest.check_geotransform(
        (0, 1, 0, 0, 0, 1),
        ds.GetGeoTransform(),
        1e-9,
    )
    assert ds.GetRasterBand(1).XSize == 10
    assert ds.GetRasterBand(1).YSize == 10

    ## separated oo instances not allowed
    with pytest.raises(Exception, match="option should be specified once"):
        gdal.Open(
            "vrt://data/byte_with_ovr.tif?oo=GEOREF_SOURCES=TABFILE&oo=OVERVIEW_LEVEL=0"
        )

    dsn_unscale = "/vsimem/scale2.tif"
    gdal.Translate(dsn_unscale, "vrt://data/byte.tif?a_scale=2")
    ds = gdal.Open("vrt://" + dsn_unscale + "?unscale=true&ot=int16")
    assert struct.unpack("h", ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1))[0] == 214

    ds = gdal.Open("vrt://data/minfloat.tif?scale=true")
    assert struct.unpack("f", ds.GetRasterBand(1).ReadRaster(2, 0, 1, 1))[
        0
    ] == pytest.approx(1.0)

    ds = gdal.Open("vrt://data/minfloat.tif?scale_2=true&bands=1,1")
    assert struct.unpack("f", ds.GetRasterBand(2).ReadRaster(2, 0, 1, 1))[
        0
    ] == pytest.approx(1.0)
    assert struct.unpack("f", ds.GetRasterBand(1).ReadRaster(2, 0, 1, 1))[
        0
    ] == pytest.approx(5.0)

    # test that 'key=value' form is used
    with pytest.raises(Exception):
        gdal.Open("vrt://data/minfloat.tif?scale")
    with pytest.raises(Exception):
        gdal.Open("vrt://data/minfloat.tif?a_ullr=0,1,1,0&unscale&")

    ds = gdal.Open("vrt://data/gcps.vrt?nogcp=true")
    assert ds.GetGCPCount() == 0

    assert gdal.Open("vrt://data/byte.tif?srcwin=0,0,20,20&eco=true&epo=true")
    assert gdal.Open("vrt://data/byte.tif?srcwin=0,0,20,21&eco=true")

    with pytest.raises(Exception):
        gdal.Open("vrt://data/byte.tif?srcwin=0,0,20,21&epo=true")
    with pytest.raises(Exception):
        gdal.Open("vrt://data/byte.tif?srcwin=20,20,1,1&epo=false&eco=true")

    ds = gdal.Open("vrt://data/tiff_with_subifds.tif?sd=2")
    assert ds.GetRasterBand(1).Checksum() == 0

    ## the component name is "1"
    ds = gdal.Open("vrt://data/tiff_with_subifds.tif?sd_name=1")
    assert ds.GetRasterBand(1).Checksum() == 35731
    with pytest.raises(Exception):
        gdal.Open("vrt://data/tiff_with_subifds.tif?sd=2&sd_name=1")
    with pytest.raises(Exception):
        gdal.Open("vrt://data/tiff_with_subifds.tif?sd=3")
    with pytest.raises(Exception):
        gdal.Open("vrt://data/tiff_with_subifds.tif?sd_name=sds")

    del ds

    files_opened_end = gdaltest.get_opened_files()

    assert files_opened_start == files_opened_end


@pytest.mark.require_driver("NetCDF")
def test_vrt_protocol_netcdf_component_name():
    ds = gdal.Open(
        "vrt://../gdrivers/data/netcdf/alldatatypes.nc?sd_name=ubyte_y2_x2_var"
    )
    assert ds.GetRasterBand(1).Checksum() == 71


@pytest.mark.require_proj(7, 2)
def test_vrt_protocol_a_coord_epoch_option():
    ds = gdal.Open("vrt://data/byte.tif?a_srs=EPSG:4326&a_coord_epoch=2021.3")
    srs = ds.GetSpatialRef()
    assert srs.IsDynamic()
    assert srs.GetCoordinateEpoch() == 2021.3


@pytest.mark.require_driver("BMP")
def test_vrt_protocol_expand_option():
    ds = gdal.Open("vrt://data/8bit_pal.bmp?expand=rgb")
    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_RedBand


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("HDF5")
def test_vrt_protocol_transpose_option():

    ds = gdal.Open("vrt://../gdrivers/data/hdf5/fwhm.h5?transpose=/MyDataField:0,1")
    assert ds.RasterXSize == 2

    ds = gdal.Open("vrt://../gdrivers/data/hdf5/fwhm.h5?transpose=/MyDataField:2,1")
    assert ds.RasterXSize == 4

    ## check exclusivity with sd_name/sd (transpose gets priority)
    with pytest.raises(
        Exception,
        match=r"'sd_name' is mutually exclusive with option 'transpose'",
    ):
        gdal.Open(
            "vrt://../gdrivers/data/hdf5/fwhm.h5?sd_name=MyDataField&transpose=/MyDataField:1,0"
        )

    with pytest.raises(
        Exception,
        match=r"'sd' is mutually exclusive with option 'transpose'",
    ):
        gdal.Open("vrt://../gdrivers/data/hdf5/fwhm.h5?sd=1&transpose=/MyDataField:1,0")


@gdaltest.enable_exceptions()
def test_vrt_protocol_block_option(tmp_vsimem):
    """Test vrt:// protocol block option for accessing natural blocks."""

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    # Create a tiled test file with known block size
    src_filename = tmp_vsimem / "block_test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(
        src_filename,
        100,
        80,
        1,
        gdal.GDT_Byte,
        options=["TILED=YES", "BLOCKXSIZE=32", "BLOCKYSIZE=32"],
    )
    # Fill with pattern: pixel value = (x + y) % 256

    data = np.zeros((80, 100), dtype=np.uint8)
    for y in range(80):
        for x in range(100):
            data[y, x] = (x + y) % 256
    ds.GetRasterBand(1).WriteArray(data)
    ds = None

    # Test block 0,0 (top-left, full 32x32)
    ds = gdal.Open(f"vrt://{src_filename}?block=0,0")
    assert ds.RasterXSize == 32
    assert ds.RasterYSize == 32
    arr = ds.GetRasterBand(1).ReadAsArray()
    assert arr[0, 0] == 0  # (0+0) % 256
    assert arr[0, 31] == 31  # (31+0) % 256
    assert arr[31, 0] == 31  # (0+31) % 256
    ds = None

    # Test block 1,0 (second column, full 32x32)
    ds = gdal.Open(f"vrt://{src_filename}?block=1,0")
    assert ds.RasterXSize == 32
    assert ds.RasterYSize == 32
    arr = ds.GetRasterBand(1).ReadAsArray()
    assert arr[0, 0] == 32  # pixel at x=32, y=0 -> (32+0) % 256
    ds = None

    # Test block 3,0 (marginal block in X direction: 100 - 3*32 = 4 pixels wide)
    ds = gdal.Open(f"vrt://{src_filename}?block=3,0")
    assert ds.RasterXSize == 4  # 100 - 96 = 4
    assert ds.RasterYSize == 32
    ds = None

    # Test block 0,2 (marginal block in Y direction: 80 - 2*32 = 16 pixels tall)
    ds = gdal.Open(f"vrt://{src_filename}?block=0,2")
    assert ds.RasterXSize == 32
    assert ds.RasterYSize == 16  # 80 - 64 = 16
    ds = None

    # Test block 3,2 (corner marginal block: 4x16)
    ds = gdal.Open(f"vrt://{src_filename}?block=3,2")
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 16
    ds = None

    # Test with bands option (should work together)
    ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "block_test_3band.tif",
        100,
        80,
        3,
        gdal.GDT_Byte,
        options=["TILED=YES", "BLOCKXSIZE=32", "BLOCKYSIZE=32"],
    )
    ds.GetRasterBand(1).Fill(10)
    ds.GetRasterBand(2).Fill(20)
    ds.GetRasterBand(3).Fill(30)
    ds = None

    ds = gdal.Open(f"vrt://{tmp_vsimem / 'block_test_3band.tif'}?block=0,0&bands=3,1")
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 32
    assert ds.RasterYSize == 32
    assert ds.GetRasterBand(1).ReadAsArray()[0, 0] == 30
    assert ds.GetRasterBand(2).ReadAsArray()[0, 0] == 10
    ds = None

    # Test error: wrong number of values
    with pytest.raises(Exception, match="must be two values"):
        gdal.Open(f"vrt://{src_filename}?block=0")

    with pytest.raises(Exception, match="must be two values"):
        gdal.Open(f"vrt://{src_filename}?block=0,0,0")

    # Test error: non-numeric values
    # Test scientific notation works (1e0 = 1)
    ds = gdal.Open(f"vrt://{src_filename}?block=1e0,0")
    assert ds.RasterXSize == 32
    ds = None

    # Test error: non-numeric values
    with pytest.raises(Exception, match="not a valid non-negative integer"):
        gdal.Open(f"vrt://{src_filename}?block=abc,0")

    with pytest.raises(Exception, match="not a valid non-negative integer"):
        gdal.Open(f"vrt://{src_filename}?block=0,xyz")

    # Test error: non-integer float values
    with pytest.raises(Exception, match="not a valid non-negative integer"):
        gdal.Open(f"vrt://{src_filename}?block=1.5,0")

    with pytest.raises(Exception, match="not a valid non-negative integer"):
        gdal.Open(f"vrt://{src_filename}?block=0,2.7")

    # Test error: negative values
    with pytest.raises(Exception, match="not a valid non-negative integer"):
        gdal.Open(f"vrt://{src_filename}?block=-1,0")

    with pytest.raises(Exception, match="not a valid non-negative integer"):
        gdal.Open(f"vrt://{src_filename}?block=0,-1")

    # Test error: out of range (max X block is 3, max Y block is 2)
    with pytest.raises(Exception, match="Invalid block indices"):
        gdal.Open(f"vrt://{src_filename}?block=4,0")

    with pytest.raises(Exception, match="Invalid block indices"):
        gdal.Open(f"vrt://{src_filename}?block=0,3")

    # Test mutual exclusivity with srcwin
    with pytest.raises(Exception, match="mutually exclusive"):
        gdal.Open(f"vrt://{src_filename}?block=0,0&srcwin=0,0,10,10")

    # Test mutual exclusivity with projwin
    with pytest.raises(Exception, match="mutually exclusive"):
        gdal.Open(f"vrt://{src_filename}?block=0,0&projwin=0,100,100,0")

    # Test mutual exclusivity with outsize
    with pytest.raises(Exception, match="mutually exclusive"):
        gdal.Open(f"vrt://{src_filename}?block=0,0&outsize=50,50")

    # Test mutual exclusivity with tr
    with pytest.raises(Exception, match="mutually exclusive"):
        gdal.Open(f"vrt://{src_filename}?block=0,0&tr=2,2")

    # Test mutual exclusivity with r (resampling)
    with pytest.raises(Exception, match="mutually exclusive"):
        gdal.Open(f"vrt://{src_filename}?block=0,0&r=bilinear")

    # Test mutual exclusivity with ovr (overview level)
    with pytest.raises(Exception, match="mutually exclusive"):
        gdal.Open(f"vrt://{src_filename}?block=0,0&ovr=0")


def test_vrt_source_no_dstrect():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>
"""
    filename = "/vsimem/out.tif"
    ds = gdal.Translate(filename, vrt_text)
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    gdal.Unlink(filename)


def test_vrt_dataset_rasterio_recursion_detection():

    gdal.FileFromMemBuffer(
        "/vsimem/test.vrt",
        """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <Overview>
        <SourceFilename relativeToVRT="0">/vsimem/test.vrt</SourceFilename>
        <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
</VRTDataset>""",
    )

    ds = gdal.Open("/vsimem/test.vrt")
    with gdal.quiet_errors():
        ds.ReadRaster(0, 0, 20, 20, 10, 10)
    gdal.Unlink("/vsimem/test.vrt")


def test_vrt_dataset_rasterio_recursion_detection_does_not_trigger():

    vrt_text = """<VRTDataset rasterXSize="50" rasterYSize="50">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Red</ColorInterp>
    <ComplexSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Green</ColorInterp>
    <ComplexSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>2</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <ColorInterp>Blue</ColorInterp>
    <ComplexSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>3</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    got_data = ds.ReadRaster(0, 0, 50, 50, 25, 25, resample_alg=gdal.GRIORA_Cubic)
    ds = gdal.Open("data/rgbsmall.tif")
    ref_data = ds.ReadRaster(0, 0, 50, 50, 25, 25, resample_alg=gdal.GRIORA_Cubic)
    assert got_data == ref_data


def test_vrt_dataset_rasterio_non_nearest_resampling_source_with_ovr(tmp_vsimem):

    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src.tif", 10, 10, 3)
    ds.GetRasterBand(1).Fill(255)
    ds.BuildOverviews("NONE", [2])
    ds.GetRasterBand(1).GetOverview(0).Fill(10)
    ds = None

    vrt_text = f"""<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Red</ColorInterp>
    <!-- two sources to avoid virtual overview to be created on the VRTRasterBand -->
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10" ySize="5" />
      <DstRect xOff="0" yOff="0" xSize="10" ySize="5" />
    </ComplexSource>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="5" xSize="10" ySize="5" />
      <DstRect xOff="0" yOff="5" xSize="10" ySize="5" />
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Green</ColorInterp>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src.tif</SourceFilename>
      <SourceBand>2</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <ColorInterp>Blue</ColorInterp>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src.tif</SourceFilename>
      <SourceBand>3</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)

    got_data = ds.ReadRaster(0, 0, 10, 10, 4, 4)
    got_data = struct.unpack("B" * 4 * 4 * 3, got_data)
    assert got_data[0] == 10

    got_data = ds.ReadRaster(0, 0, 10, 10, 4, 4, resample_alg=gdal.GRIORA_Cubic)
    got_data = struct.unpack("B" * 4 * 4 * 3, got_data)
    assert got_data[0] == 10


def test_vrt_implicit_ovr_with_hidenodatavalue(tmp_vsimem):

    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src.tif", 256, 256, 3)
    ds.GetRasterBand(1).Fill(255)
    ds.BuildOverviews("NONE", [2])
    ds.GetRasterBand(1).GetOverview(0).Fill(10)
    ds = None

    vrt_text = f"""<VRTDataset rasterXSize="256" rasterYSize="256">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Red</ColorInterp>
    <NoDataValue>5</NoDataValue>
    <HideNoDataValue>1</HideNoDataValue>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="128" ySize="128" />
      <DstRect xOff="128" yOff="128" xSize="128" ySize="128" />
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Green</ColorInterp>
    <NoDataValue>5</NoDataValue>
    <HideNoDataValue>1</HideNoDataValue>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src.tif</SourceFilename>
      <SourceBand>2</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="128" ySize="128" />
      <DstRect xOff="128" yOff="128" xSize="128" ySize="128" />
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <ColorInterp>Blue</ColorInterp>
    <NoDataValue>5</NoDataValue>
    <HideNoDataValue>1</HideNoDataValue>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src.tif</SourceFilename>
      <SourceBand>3</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="128" ySize="128" />
      <DstRect xOff="128" yOff="128" xSize="128" ySize="128" />
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    got_data = ds.ReadRaster(0, 0, 256, 256, 64, 64)
    got_data = struct.unpack("B" * 64 * 64 * 3, got_data)
    assert got_data[0] == 5
    assert got_data[32 * 64 + 32] == 10

    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 256, 256, 64, 64)
    got_data = struct.unpack("B" * 64 * 64, got_data)
    assert got_data[0] == 5
    assert got_data[32 * 64 + 32] == 10


def test_vrt_usemaskband(tmp_vsimem):

    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src1.tif", 3, 1)
    ds.GetRasterBand(1).Fill(255)
    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        ds.CreateMaskBand(0)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, b"\xff")
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src2.tif", 3, 1)
    ds.GetRasterBand(1).Fill(127)
    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        ds.CreateMaskBand(0)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(1, 0, 1, 1, b"\xff")
    ds = None

    vrt_text = f"""<VRTDataset rasterXSize="3" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src1.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src2.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
  </VRTRasterBand>
  <MaskBand>
    <VRTRasterBand dataType="Byte">
        <ComplexSource>
            <SourceFilename>{tmp_vsimem}/src1.tif</SourceFilename>
            <SourceBand>mask,1</SourceBand>
            <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
            <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
            <UseMaskBand>true</UseMaskBand>
        </ComplexSource>
        <ComplexSource>
            <SourceFilename>{tmp_vsimem}/src2.tif</SourceFilename>
            <SourceBand>mask,1</SourceBand>
            <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
            <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
            <UseMaskBand>true</UseMaskBand>
        </ComplexSource>
    </VRTRasterBand>
  </MaskBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert struct.unpack("B" * 3, ds.ReadRaster()) == (255, 127, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(1).GetMaskBand().ReadRaster()) == (
        255,
        255,
        0,
    )


def test_vrt_usemaskband_alpha(tmp_vsimem):

    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src1.tif", 3, 1, 2)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, b"\xff")
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(2).WriteRaster(0, 0, 1, 1, b"\xff")

    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src2.tif", 3, 1, 2)
    ds.GetRasterBand(1).Fill(127)
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(2).WriteRaster(1, 0, 1, 1, b"\xff")
    ds = None

    vrt_text = f"""<VRTDataset rasterXSize="3" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src1.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
    <ComplexSource>
      <SourceFilename>{tmp_vsimem}/src2.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Alpha</ColorInterp>
    <ComplexSource>
        <SourceFilename>{tmp_vsimem}/src1.tif</SourceFilename>
        <SourceBand>2</SourceBand>
        <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
        <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
        <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
    <ComplexSource>
        <SourceFilename>{tmp_vsimem}/src2.tif</SourceFilename>
        <SourceBand>2</SourceBand>
        <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
        <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
        <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert struct.unpack("B" * 3, ds.GetRasterBand(1).ReadRaster()) == (255, 127, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(2).ReadRaster()) == (255, 255, 0)


def test_vrt_check_dont_open_unneeded_source(tmp_vsimem):

    vrt = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10" ySize="10" />
      <DstRect xOff="0" yOff="0" xSize="10" ySize="10" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">i_do_not_exist.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="10" yOff="10" xSize="10" ySize="10" />
      <DstRect xOff="10" yOff="10" xSize="10" ySize="10" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""

    tmpfilename = tmp_vsimem / "tmp.vrt"
    gdal.FileFromMemBuffer(tmpfilename, vrt)

    ds = gdal.Translate("", tmpfilename, options="-of MEM -srcwin 0 0 10 10")
    assert ds is not None

    with gdal.quiet_errors():
        ds = gdal.Translate("", tmpfilename, options="-of MEM -srcwin 0 0 10.1 10.1")
    assert ds is None


def test_vrt_check_dont_open_unneeded_source_with_complex_source_nodata(tmp_vsimem):

    vrt = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <ComplexSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10" ySize="10" />
      <DstRect xOff="0" yOff="0" xSize="10" ySize="10" />
      <NODATA>0</NODATA>
    </ComplexSource>
    <ComplexSource>
      <SourceFilename relativeToVRT="1">i_do_not_exist.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="10" yOff="10" xSize="10" ySize="10" />
      <DstRect xOff="10" yOff="10" xSize="10" ySize="10" />
      <NODATA>0</NODATA>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""

    tmpfilename = tmp_vsimem / "tmp.vrt"
    gdal.FileFromMemBuffer(tmpfilename, vrt)

    ds = gdal.Translate("", tmpfilename, options="-of MEM -srcwin 0 0 10 10")
    assert ds is not None

    with gdal.quiet_errors():
        ds = gdal.Translate("", tmpfilename, options="-of MEM -srcwin 0 0 10.1 10.1")
    assert ds is None


def test_vrt_nodata_and_implicit_ovr_recursion_issue(tmp_vsimem):
    """Tests scenario https://github.com/OSGeo/gdal/issues/4620#issuecomment-938636360"""

    vrt = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <NoDataValue>0</NoDataValue>
    <ComplexSource>
      <NODATA>0</NODATA>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
  <OverviewList resampling="average">2</OverviewList>
</VRTDataset>"""

    tmpfilename = tmp_vsimem / "tmp.vrt"
    gdal.FileFromMemBuffer(tmpfilename, vrt)

    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152


def test_vrt_statistics_and_implicit_ovr_recursion_issue(tmp_vsimem):
    """Tests scenario https://github.com/OSGeo/gdal/issues/4661"""

    gdal.Translate(tmp_vsimem / "test.tif", "data/uint16.tif", width=2048)
    vrt_ds = gdal.Translate("", tmp_vsimem / "test.tif", format="VRT")
    with gdaltest.config_option("VRT_VIRTUAL_OVERVIEWS", "YES"):
        vrt_ds.BuildOverviews("NEAR", [2, 4])

    stats = vrt_ds.GetRasterBand(1).ComputeStatistics(True)  # approx stats
    assert gdal.GetLastErrorMsg() == ""
    assert stats[0] >= 74 and stats[0] <= 90

    min_max = vrt_ds.GetRasterBand(1).ComputeRasterMinMax(True)  # approx stats
    assert gdal.GetLastErrorMsg() == ""
    assert min_max[0] >= 74 and min_max[0] <= 90

    hist = vrt_ds.GetRasterBand(1).GetHistogram(True)  # approx stats
    assert gdal.GetLastErrorMsg() == ""
    assert hist is not None


def test_vrt_read_req_coordinates_almost_integer(tmp_vsimem):

    ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in.tif", 3601, 3601)
    b = (
        array.array("B", [i for i in range(255)]).tobytes() * (3601 * 3601 // 255)
        + array.array("B", [0] * (3601 * 3601 % 255)).tobytes()
    )
    ds.WriteRaster(0, 0, 3601, 3601, b)
    ds = None

    gdal.FileFromMemBuffer(
        tmp_vsimem / "in.vrt",
        """<VRTDataset rasterXSize="3601" rasterYSize="3601">
      <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
          <SourceFilename relativeToVRT="1">in.tif</SourceFilename>
          <SourceBand>1</SourceBand>
          <SrcRect xOff="0" yOff="0" xSize="3601" ySize="3601" />
          <DstRect xOff="0" yOff="0.499999999992505" xSize="1800.50000003" ySize="1800.50000003" />
        </SimpleSource>
      </VRTRasterBand>
    </VRTDataset>""",
    )

    ds = gdal.Open(tmp_vsimem / "in.vrt")
    expected = [
        [161, 163, 165, 167, 169, 171],
        [223, 225, 227, 229, 231, 233],
        [30, 32, 34, 36, 38, 40],
        [92, 94, 96, 98, 100, 102],
        [154, 156, 158, 160, 162, 164],
        [216, 218, 220, 222, 224, 226],
    ]
    assert struct.unpack(
        "B" * (6 * 6), ds.ReadRaster(1045, 1795, 6, 6)
    ) == struct.unpack(
        "B" * (6 * 6), b"".join([array.array("B", x).tobytes() for x in expected])
    )
    expected = [
        [219, 221, 223, 225, 227, 229],
        [26, 28, 30, 32, 34, 36],
        [88, 90, 92, 94, 96, 98],
        [150, 152, 154, 156, 158, 160],
        [212, 214, 216, 218, 220, 222],
        [0, 0, 0, 0, 0, 0],
    ]
    assert struct.unpack(
        "B" * (6 * 6), ds.ReadRaster(1043, 1796, 6, 6)
    ) == struct.unpack(
        "B" * (6 * 6), b"".join([array.array("B", x).tobytes() for x in expected])
    )


###############################################################################
# Test ComputeStatistics() mosaic optimization


@pytest.mark.parametrize("approx_ok,use_threads", [(False, True), (True, False)])
def test_vrt_read_compute_statistics_mosaic_optimization(
    tmp_vsimem, approx_ok, use_threads
):

    # To avoid using a byte.aux.xml file with existing statistics
    src_filename = str(tmp_vsimem / "byte.tif")
    gdal.FileFromMemBuffer(src_filename, open("data/byte.tif", "rb").read())
    src_ds = gdal.Translate("", src_filename, format="MEM")
    src_ds1 = gdal.Translate("", src_ds, options="-of MEM -srcwin 0 0 8 20")
    src_ds2 = gdal.Translate("", src_ds, options="-of MEM -srcwin 8 0 12 20")
    vrt_ds = gdal.BuildVRT("", [src_ds1, src_ds2])

    with gdaltest.config_options({"VRT_NUM_THREADS": "2" if use_threads else "0"}):
        assert vrt_ds.GetRasterBand(1).ComputeRasterMinMax(
            approx_ok
        ) == src_ds.GetRasterBand(1).ComputeRasterMinMax(approx_ok)

        def callback(pct, message, user_data):
            user_data[0] = pct
            return 1  # 1 to continue, 0 to stop

        user_data = [0]
        vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(
            approx_ok, callback=callback, callback_data=user_data
        )
        assert user_data[0] == 1.0
        assert vrt_stats == pytest.approx(
            src_ds.GetRasterBand(1).ComputeStatistics(approx_ok)
        )
    if approx_ok:
        assert (
            vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_APPROXIMATE") == "YES"
        )
    else:
        assert vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_APPROXIMATE") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_VALID_PERCENT") == "100"
    if not approx_ok:
        assert vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") == "74"
        assert vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MAXIMUM") == "255"


###############################################################################
# Test ComputeStatistics() mosaic optimization with nodata at VRT band


def test_vrt_read_compute_statistics_mosaic_optimization_nodata():

    src_ds = gdal.Translate("", gdal.Open("data/byte.tif"), format="MEM")
    src_ds1 = gdal.Translate("", src_ds, options="-of MEM -srcwin 0 0 8 20")
    # hole of 2 pixels at columns 9 and 10
    src_ds2 = gdal.Translate("", src_ds, options="-of MEM -srcwin 10 0 10 20")
    vrt_ds = gdal.BuildVRT("", [src_ds1, src_ds2], VRTNodata=10)
    vrt_materialized = gdal.Translate("", vrt_ds, format="MEM")

    assert vrt_ds.GetRasterBand(1).ComputeRasterMinMax(
        False
    ) == vrt_materialized.GetRasterBand(1).ComputeRasterMinMax(False)

    vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)
    assert vrt_stats == pytest.approx(
        vrt_materialized.GetRasterBand(1).ComputeStatistics(False)
    )
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_APPROXIMATE") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_VALID_PERCENT") == "90"


###############################################################################
# Test ComputeStatistics() mosaic optimization with nodata at VRT band, but hidden


def test_vrt_read_compute_statistics_mosaic_optimization_nodata_hidden():

    src_ds = gdal.Translate("", gdal.Open("data/byte.tif"), format="MEM")
    src_ds1 = gdal.Translate("", src_ds, options="-of MEM -srcwin 0 0 8 20")
    # hole of 2 pixels at columns 9 and 10
    src_ds2 = gdal.Translate("", src_ds, options="-of MEM -srcwin 10 0 10 20")
    vrt_ds = gdal.BuildVRT("", [src_ds1, src_ds2], VRTNodata=10, hideNodata=True)
    vrt_materialized = gdal.Translate("", vrt_ds, format="MEM")

    assert vrt_ds.GetRasterBand(1).ComputeRasterMinMax(
        False
    ) == vrt_materialized.GetRasterBand(1).ComputeRasterMinMax(False)

    vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)
    assert vrt_stats == pytest.approx(
        vrt_materialized.GetRasterBand(1).ComputeStatistics(False)
    )
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_APPROXIMATE") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_VALID_PERCENT") == "100"


###############################################################################
# Test ComputeStatistics() mosaic optimization with nodata on source bands


@pytest.mark.parametrize(
    "band_ndv,global_ndv", [(10, None), (1, None), (None, 10), (10, 10)]
)
def test_vrt_read_compute_statistics_mosaic_optimization_src_with_nodata(
    band_ndv, global_ndv
):

    src_ds = gdal.Translate("", gdal.Open("data/byte.tif"), format="MEM")
    src_ds1 = gdal.Translate(
        "", src_ds, options="-of MEM -srcwin 0 0 8 20 -scale 0 255 10 10"
    )
    src_ds2 = gdal.Translate("", src_ds, options="-of MEM -srcwin 8 0 12 20")
    vrt_ds = gdal.BuildVRT("", [src_ds1, src_ds2])
    if band_ndv:
        src_ds1.GetRasterBand(1).SetNoDataValue(band_ndv)
    if global_ndv:
        vrt_ds.GetRasterBand(1).SetNoDataValue(global_ndv)

    vrt_materialized = gdal.Translate("", vrt_ds, format="MEM")

    assert vrt_ds.GetRasterBand(1).ComputeRasterMinMax(
        False
    ) == vrt_materialized.GetRasterBand(1).ComputeRasterMinMax(False)

    vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)
    assert vrt_stats == pytest.approx(
        vrt_materialized.GetRasterBand(1).ComputeStatistics(False)
    )


###############################################################################
# Test ComputeStatistics() mosaic optimization with all at nodata


def test_vrt_read_compute_statistics_mosaic_optimization_src_with_nodata_all():

    src_ds = gdal.Translate("", gdal.Open("data/byte.tif"), format="MEM")
    src_ds1 = gdal.Translate(
        "", src_ds, options="-of MEM -srcwin 0 0 8 20 -scale 0 255 10 10"
    )
    src_ds2 = gdal.Translate(
        "", src_ds, options="-of MEM -srcwin 8 0 12 20 -scale 0 255 10 10"
    )
    vrt_ds = gdal.BuildVRT("", [src_ds1, src_ds2], VRTNodata=10)
    src_ds1.GetRasterBand(1).SetNoDataValue(10)
    src_ds2.GetRasterBand(1).SetNoDataValue(10)

    with gdal.quiet_errors():
        minmax = vrt_ds.GetRasterBand(1).ComputeRasterMinMax(False)
    assert math.isnan(minmax[0])
    assert math.isnan(minmax[1])

    with gdal.quiet_errors():
        vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)
        assert vrt_stats is None
        assert vrt_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is None


###############################################################################
# Test ComputeStatistics() mosaic optimization in a case where it shouldn't
# trigger


def test_vrt_read_compute_statistics_mosaic_optimization_not_triggered():

    src_ds = gdal.Translate("", gdal.Open("data/byte.tif"), format="MEM")
    # Overlapping sources
    src_ds1 = gdal.Translate("", src_ds, options="-of MEM -srcwin 0 0 10 20")
    src_ds2 = gdal.Translate("", src_ds, options="-of MEM -srcwin 9 0 11 20")
    vrt_ds = gdal.BuildVRT("", [src_ds1, src_ds2])

    assert (
        vrt_ds.GetRasterBand(1).ComputeRasterMinMax()
        == src_ds.GetRasterBand(1).ComputeRasterMinMax()
    )

    vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)
    assert vrt_stats == src_ds.GetRasterBand(1).ComputeStatistics(False)


###############################################################################
# Test ComputeStatistics() mosaic optimization on single source and check that
# it exactly preserves source statistics
# Note that the test unfortunately does pass even without the tweak in
# VRTSourcedRasterBand::ComputeStatistics() to exactly use the source statistics


def test_vrt_read_compute_statistics_mosaic_optimization_single_source(tmp_vsimem):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")
    rng = np.random.default_rng(0)
    N = 512
    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "in.tif", N, N, 1, gdal.GDT_Float32, options=["TILED=YES"]
    ) as ds:
        ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
        ds.WriteArray((rng.standard_normal(N * N)).reshape(N, N))
    gdal.Translate(
        tmp_vsimem / "test.tif", tmp_vsimem / "in.tif", options="-stats -co TILED=YES"
    )
    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        md_band_1 = ds.GetRasterBand(1).GetMetadata()
    gdal.BuildVRT(tmp_vsimem / "test.vrt", tmp_vsimem / "test.tif")
    with gdal.Open(tmp_vsimem / "test.vrt", gdal.GA_Update) as ds:
        ds.GetRasterBand(1).ComputeStatistics(False)
    with gdal.Open(tmp_vsimem / "test.vrt") as ds:
        assert ds.GetRasterBand(1).GetMetadata() == md_band_1
    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        assert ds.GetRasterBand(1).GetMetadata() == md_band_1


###############################################################################
# Test complex source with requesting a buffer with a type "larger" than
# the VRT data type


@pytest.mark.parametrize("obj_type", ["ds", "band"])
@pytest.mark.parametrize(
    "struct_type,gdal_type ", [("B", gdal.GDT_UInt8), ("i", gdal.GDT_Int32)]
)
def test_vrt_read_complex_source_use_band_data_type_constraint(
    obj_type, struct_type, gdal_type
):

    complex_xml = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <ScaleOffset>0</ScaleOffset>
      <ScaleRatio>1.5</ScaleRatio>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>
"""

    ds = gdal.Open(complex_xml)
    obj = ds if obj_type == "ds" else ds.GetRasterBand(1)
    scaleddata = struct.unpack(
        struct_type * (20 * 20), obj.ReadRaster(buf_type=gdal_type)
    )
    assert max(scaleddata) == 255


###############################################################################


def test_vrt_read_top_and_bottom_strips_average():

    ref_ds = gdal.Translate("", "data/vrt/top_and_bottom_strips.vrt", format="MEM")
    ds = gdal.Open("data/vrt/top_and_bottom_strips.vrt")

    args = (0, 127, 256, 129)
    kwargs = {"buf_xsize": 128, "buf_ysize": 64, "resample_alg": gdal.GRIORA_Average}

    ref_data = ref_ds.ReadRaster(*args, **kwargs)
    assert ds.GetRasterBand(1).ReadRaster(*args, **kwargs) == ref_data
    assert ds.ReadRaster(*args, **kwargs) == ref_data
    assert ref_data[0] == 128


###############################################################################


@pytest.mark.parametrize(
    "input_datatype,vrt_type,nodata,vrt_nodata,request_type",
    [
        (gdal.GDT_UInt8, "Byte", 0, 255, gdal.GDT_UInt8),
        (gdal.GDT_UInt8, "Byte", 254, 255, gdal.GDT_UInt8),
        (gdal.GDT_UInt8, "Int8", 254, 255, gdal.GDT_UInt8),
        (gdal.GDT_UInt8, "Byte", 254, 127, gdal.GDT_Int8),
        (gdal.GDT_UInt8, "UInt16", 254, 255, gdal.GDT_UInt8),
        (gdal.GDT_UInt8, "Byte", 254, 255, gdal.GDT_UInt16),
        (gdal.GDT_Int8, "Int8", 0, 127, gdal.GDT_Int8),
        (gdal.GDT_Int8, "Int16", 0, 127, gdal.GDT_Int8),
        (gdal.GDT_UInt16, "UInt16", 0, 65535, gdal.GDT_UInt16),
        (gdal.GDT_Int16, "Int16", 0, 32767, gdal.GDT_Int16),
        (gdal.GDT_UInt32, "UInt32", 0, (1 << 31) - 1, gdal.GDT_UInt32),
        (gdal.GDT_Int32, "Int32", 0, (1 << 30) - 1, gdal.GDT_Int32),
        (gdal.GDT_Int32, "Float32", 0, (1 << 30) - 1, gdal.GDT_Float64),
        (gdal.GDT_UInt64, "UInt64", 0, (1 << 63) - 1, gdal.GDT_UInt64),
        (gdal.GDT_Int64, "Int64", 0, (1 << 62) - 1, gdal.GDT_Int64),
        (gdal.GDT_Int64, "Float32", 0, (1 << 62), gdal.GDT_Int64),
        (gdal.GDT_Float32, "Float32", 0, 1.5, gdal.GDT_Float32),
        (gdal.GDT_Float32, "Float32", 0, 1.5, gdal.GDT_Float64),
        (gdal.GDT_Float32, "Float64", 0, 1.5, gdal.GDT_Float32),
        (gdal.GDT_Float32, "Float64", 0, 1.5, gdal.GDT_Float64),
        (gdal.GDT_Float64, "Float64", 0, 1.5, gdal.GDT_Float64),
        (gdal.GDT_Float64, "Float32", 0, 1.5, gdal.GDT_Float64),
    ],
)
def test_vrt_read_complex_source_nodata(
    tmp_vsimem, input_datatype, vrt_type, nodata, vrt_nodata, request_type
):
    def get_array_type(dt):
        m = {
            gdal.GDT_UInt8: "B",
            gdal.GDT_Int8: "b",
            gdal.GDT_UInt16: "H",
            gdal.GDT_Int16: "h",
            gdal.GDT_UInt32: "I",
            gdal.GDT_Int32: "i",
            gdal.GDT_UInt64: "Q",
            gdal.GDT_Int64: "q",
            gdal.GDT_Float32: "f",
            gdal.GDT_Float64: "d",
        }
        return m[dt]

    if input_datatype in (gdal.GDT_Float32, gdal.GDT_Float64):
        input_val = 1.75
        if vrt_type in ("Float32", "Float64") and request_type in (
            gdal.GDT_Float32,
            gdal.GDT_Float64,
        ):
            expected_val = input_val
        else:
            expected_val = math.round(input_val)
    else:
        input_val = 1
        expected_val = input_val

    input_data = array.array(
        get_array_type(input_datatype),
        [
            nodata,
            input_val,
            2,
            3,
            nodata,  # EOL
            4,
            nodata,
            5,
            6,
            7,  # EOL
            16,
            17,
            18,
            19,
            20,  # EOL
            8,
            9,
            nodata,
            10,
            11,  # EOL
            nodata,
            nodata,
            nodata,
            nodata,
            nodata,  # EOL
            12,
            13,
            14,
            nodata,
            15,  # EOL
        ],
    )
    input_filename = str(tmp_vsimem / "source.tif")
    ds = gdal.GetDriverByName("GTiff").Create(input_filename, 5, 6, 1, input_datatype)
    ds.WriteRaster(0, 0, 5, 6, input_data, buf_type=input_datatype)
    ds.Close()
    complex_xml = f"""<VRTDataset rasterXSize="5" rasterYSize="6">
  <VRTRasterBand dataType="{vrt_type}" band="1">
    <NoDataValue>{vrt_nodata}</NoDataValue>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">{input_filename}</SourceFilename>
      <SourceBand>1</SourceBand>
      <NODATA>{nodata}</NODATA>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>
"""
    vrt_ds = gdal.Open(complex_xml)
    got_data = vrt_ds.ReadRaster(buf_type=request_type)
    got_data = struct.unpack(get_array_type(request_type) * (5 * 6), got_data)
    assert got_data == (
        vrt_nodata,
        expected_val,
        2,
        3,
        vrt_nodata,  # EOL
        4,
        vrt_nodata,
        5,
        6,
        7,  # EOL
        16,
        17,
        18,
        19,
        20,  # EOL
        8,
        9,
        vrt_nodata,
        10,
        11,  # EOL
        vrt_nodata,
        vrt_nodata,
        vrt_nodata,
        vrt_nodata,
        vrt_nodata,  # EOL
        12,
        13,
        14,
        vrt_nodata,
        15,  # EOL
    )


###############################################################################


@pytest.mark.parametrize("data_type", [gdal.GDT_UInt8, gdal.GDT_UInt16, gdal.GDT_Int16])
def test_vrt_read_complex_source_nodata_out_of_range(tmp_vsimem, data_type):

    if data_type == gdal.GDT_UInt8:
        array_type = "B"
    elif data_type == gdal.GDT_UInt16:
        array_type = "H"
    elif data_type == gdal.GDT_Int16:
        array_type = "h"
    else:
        assert False
    input_data = array.array(array_type, [1])
    input_filename = str(tmp_vsimem / "source.tif")
    ds = gdal.GetDriverByName("GTiff").Create(input_filename, 1, 1, 1, data_type)
    ds.WriteRaster(0, 0, 1, 1, input_data, buf_type=data_type)
    ds.Close()
    vrt_type = gdal.GetDataTypeName(data_type)
    complex_xml = f"""<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="{vrt_type}" band="1">
    <NoDataValue>255</NoDataValue>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">{input_filename}</SourceFilename>
      <SourceBand>1</SourceBand>
      <NODATA>-100000000</NODATA>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>
"""
    vrt_ds = gdal.Open(complex_xml)
    got_data = vrt_ds.ReadRaster(buf_type=data_type)
    got_data = struct.unpack(array_type, got_data)
    assert got_data == (1,)


###############################################################################
# Test serialization of approximate ComputeStatistics() when there is
# external overview


def test_vrt_read_compute_statistics_approximate(tmp_vsimem):

    gtiff_filename = str(tmp_vsimem / "tmp.tif")
    gdal.Translate(
        gtiff_filename, gdal.Open("data/byte.tif"), format="GTiff", width=256
    )
    vrt_filename = str(tmp_vsimem / "tmp.vrt")
    gdal.Translate(vrt_filename, gtiff_filename)
    ds = gdal.Open(vrt_filename)
    ds.BuildOverviews("NEAR", [2])
    ds = None
    ds = gdal.Open(vrt_filename)
    ds.GetRasterBand(1).ComputeStatistics(True)
    ds = None
    ds = gdal.Open(vrt_filename)
    md = ds.GetRasterBand(1).GetMetadata()
    assert md["STATISTICS_APPROXIMATE"] == "YES"
    assert "STATISTICS_MINIMUM" in md
    assert "STATISTICS_MAXIMUM" in md
    assert "STATISTICS_MEAN" in md
    assert "STATISTICS_STDDEV" in md
    assert md["STATISTICS_VALID_PERCENT"] == "100"


###############################################################################
# Test that when generating virtual overviews we select them as close as
# possible to source ones


def test_vrt_read_virtual_overviews_match_src_overviews(tmp_vsimem):

    gtiff_filename = str(tmp_vsimem / "tmp.tif")
    ds = gdal.GetDriverByName("GTiff").Create(
        gtiff_filename, 4095, 2047, options=["SPARSE_OK=YES"]
    )
    ds.BuildOverviews("NONE", [2, 4, 8])
    band = ds.GetRasterBand(1)
    # The below assert is a pre-condition to test the behavior of the
    # "vrt://{gtiff_filename}?outsize=200%,200%". If we decided to round
    # differently overview dataset sizes in the GTiff driver, we'd need to
    # change this source dataset
    assert band.GetOverview(0).XSize == 2048
    assert band.GetOverview(0).YSize == 1024
    assert band.GetOverview(1).XSize == 1024
    assert band.GetOverview(1).YSize == 512
    ds = None

    vrt_ds = gdal.Open(f"vrt://{gtiff_filename}?outsize=200%,200%")
    assert vrt_ds.RasterXSize == 4095 * 2
    assert vrt_ds.RasterYSize == 2047 * 2
    vrt_band = vrt_ds.GetRasterBand(1)
    assert vrt_band.XSize == 4095 * 2
    assert vrt_band.YSize == 2047 * 2
    assert vrt_band.GetOverviewCount() == 3
    # We reuse the dimension of the source dataset
    assert vrt_band.GetOverview(0).XSize == 4095
    assert vrt_band.GetOverview(0).YSize == 2047
    # We reuse the dimension of the overviews of the source dataset
    assert vrt_band.GetOverview(1).XSize == 2048
    assert vrt_band.GetOverview(1).YSize == 1024
    assert vrt_band.GetOverview(2).XSize == 1024
    assert vrt_band.GetOverview(2).YSize == 512


###############################################################################
# Test multi-threaded reading


@pytest.mark.parametrize("dataset_level", [True, False])
@pytest.mark.parametrize("use_threads", [True, False])
@pytest.mark.parametrize("num_tiles", [2, 128])
def test_vrt_read_multi_threaded(tmp_vsimem, dataset_level, use_threads, num_tiles):

    width = 2048
    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", width=width, format="MEM"
    )
    assert width % num_tiles == 0
    tile_width = width // num_tiles
    tile_filenames = []
    for i in range(num_tiles):
        tile_filename = str(tmp_vsimem / ("%d.tif" % i))
        gdal.Translate(
            tile_filename, src_ds, srcWin=[i * tile_width, 0, tile_width, 1024]
        )
        tile_filenames.append(tile_filename)
    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(vrt_filename, tile_filenames)
    vrt_ds = gdal.Open(vrt_filename)

    obj = vrt_ds if dataset_level else vrt_ds.GetRasterBand(1)
    obj_ref = src_ds if dataset_level else src_ds.GetRasterBand(1)

    pcts = []

    def cbk(pct, msg, user_data):
        if pcts:
            assert pct >= pcts[-1]
        pcts.append(pct)
        return 1

    with gdal.config_options({} if use_threads else {"VRT_NUM_THREADS": "0"}):
        assert obj.ReadRaster(1, 2, 1030, 1020, callback=cbk) == obj_ref.ReadRaster(
            1, 2, 1030, 1020
        )
    assert pcts[-1] == 1.0

    assert vrt_ds.GetMetadataItem("MULTI_THREADED_RASTERIO_LAST_USED", "__DEBUG__") == (
        "1" if gdal.GetNumCPUs() >= 2 and use_threads else "0"
    )


###############################################################################
# Test multi-threaded reading


def test_vrt_read_multi_threaded_disabled_since_overlapping_sources():

    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", width=2048, format="MEM"
    )
    OVERLAP = 1
    left_ds = gdal.Translate(
        "left", src_ds, format="MEM", srcWin=[0, 0, 1024 + OVERLAP, 1024]
    )
    right_ds = gdal.Translate(
        "right", src_ds, format="MEM", srcWin=[1024, 0, 1024, 1024]
    )
    vrt_ds = gdal.BuildVRT("", [left_ds, right_ds])

    assert vrt_ds.ReadRaster(1, 2, 1030, 1020) == src_ds.ReadRaster(1, 2, 1030, 1020)

    assert (
        vrt_ds.GetMetadataItem("MULTI_THREADED_RASTERIO_LAST_USED", "__DEBUG__") == "0"
    )


###############################################################################
# Test propagation of errors from threads to main thread in multi-threaded reading


@gdaltest.enable_exceptions()
def test_vrt_read_multi_threaded_errors(tmp_vsimem):

    filename1 = str(tmp_vsimem / "tmp1.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 1, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.Close()

    filename2 = str(tmp_vsimem / "tmp2.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 1, 1)
    ds.SetGeoTransform([3, 1, 0, 49, 0, -1])
    ds.Close()

    vrt_filename = str(tmp_vsimem / "tmp.vrt")
    gdal.BuildVRT(vrt_filename, [filename1, filename2])

    gdal.Unlink(filename2)

    with gdal.Open(vrt_filename) as ds:
        with pytest.raises(Exception):
            ds.GetRasterBand(1).ReadRaster()


###############################################################################
# Test reading a VRT with a <VRTDataset> inside a <SimpleSource>


def test_vrt_read_nested_VRTDataset():

    ds = gdal.Open("data/vrt/nested_VRTDataset.vrt")
    assert ds.GetRasterBand(1).Checksum() == 4672


###############################################################################
# Test updating a VRT with a <VRTDataset> inside a <SimpleSource>


def test_vrt_update_nested_VRTDataset(tmp_vsimem):

    gdal.FileFromMemBuffer(tmp_vsimem / "byte.tif", open("data/byte.tif", "rb").read())
    gdal.Mkdir(tmp_vsimem / "vrt", 0o755)
    vrt_filename = tmp_vsimem / "vrt" / "nested_VRTDataset.vrt"
    gdal.FileFromMemBuffer(
        vrt_filename, open("data/vrt/nested_VRTDataset.vrt", "rb").read()
    )

    with gdal.Open(vrt_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672
        assert ds.GetRasterBand(1).GetMinimum() is None
        vrt_stats = ds.GetRasterBand(1).ComputeStatistics(False)
        assert vrt_stats[0] == 74

    # Check that statistics have been serialized in the VRT
    with gdal.Open(vrt_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672
        assert ds.GetRasterBand(1).GetMinimum() == 74


###############################################################################
# Test reading a VRT with a ComplexSource of type CFloat32


def test_vrt_read_cfloat32_complex_source_as_float32():

    ds = gdal.Open("data/vrt/complex_non_zero_real_zero_imag.vrt")
    assert struct.unpack("f" * 8, ds.ReadRaster()) == (1, 0, 1, 0, 1, 0, 1, 0)
    assert struct.unpack("f" * 4, ds.ReadRaster(buf_type=gdal.GDT_Float32)) == (
        1,
        1,
        1,
        1,
    )


###############################################################################
# Test reading a VRT with a ComplexSource of type Float32 (from a underlying CFloat32 source)


def test_vrt_read_float32_complex_source_from_cfloat32():

    ds = gdal.Open("data/vrt/complex_non_zero_real_zero_imag_as_float32.vrt")
    assert struct.unpack("f" * 4, ds.ReadRaster()) == (1, 1, 1, 1)


###############################################################################


def test_vrt_resampling_with_mask_and_overviews(tmp_vsimem):

    filename1 = str(tmp_vsimem / "in1.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 100, 10)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 48, 10, b"\xff" * (48 * 10))
    ds.GetRasterBand(1).WriteRaster(48, 0, 2, 10, b"\xf0" * (2 * 10))
    ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 52, 10, b"\xff" * (52 * 10))
    ds.BuildOverviews("NEAR", [2])
    ds.GetRasterBand(1).GetOverview(0).Fill(
        127
    )  # to demonstrate we ignore overviews unfortunately
    ds = None

    filename2 = str(tmp_vsimem / "in2.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 100, 10)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(48, 0, 52, 10, b"\xf0" * (52 * 10))
    ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(48, 0, 52, 10, b"\xff" * (52 * 10))
    ds.BuildOverviews("NEAR", [2])
    ds.GetRasterBand(1).GetOverview(0).Fill(
        127
    )  # to demonstrate we ignore overviews unfortunately
    ds = None

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(vrt_filename, [filename1, filename2], resampleAlg=gdal.GRIORA_Cubic)

    ds = gdal.Open(vrt_filename)
    assert (
        ds.ReadRaster(buf_xsize=10, buf_ysize=1)
        == b"\xff\xff\xff\xff\xfc\xf0\xf0\xf0\xf0\xf0"
    )
    assert (
        ds.GetRasterBand(1).GetMaskBand().ReadRaster(buf_xsize=10, buf_ysize=1)
        == b"\xff" * 10
    )

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(
        vrt_filename, [filename1, filename2], resampleAlg=gdal.GRIORA_Bilinear
    )

    ds = gdal.Open(vrt_filename)
    assert (
        ds.ReadRaster(buf_xsize=10, buf_ysize=1)
        == b"\xff\xff\xff\xff\xfb\xf1\xf0\xf0\xf0\xf0"
    )
    assert (
        ds.GetRasterBand(1).GetMaskBand().ReadRaster(buf_xsize=10, buf_ysize=1)
        == b"\xff" * 10
    )


###############################################################################


def test_vrt_resampling_with_alpha_and_overviews(tmp_vsimem):

    filename1 = str(tmp_vsimem / "in1.tif")
    ds = gdal.GetDriverByName("GTiff").Create(
        filename1, 100, 10, 2, options=["ALPHA=YES"]
    )
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 48, 10, b"\xff" * (48 * 10))
    ds.GetRasterBand(1).WriteRaster(48, 0, 2, 10, b"\xf0" * (2 * 10))
    ds.GetRasterBand(2).WriteRaster(0, 0, 52, 10, b"\xff" * (52 * 10))
    ds.BuildOverviews("NEAR", [2])
    ds.GetRasterBand(1).GetOverview(0).Fill(
        127
    )  # to demonstrate we ignore overviews unfortunately
    ds = None

    filename2 = str(tmp_vsimem / "in2.tif")
    ds = gdal.GetDriverByName("GTiff").Create(
        filename2, 100, 10, 2, options=["ALPHA=YES"]
    )
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(48, 0, 52, 10, b"\xf0" * (52 * 10))
    ds.GetRasterBand(2).WriteRaster(48, 0, 52, 10, b"\xff" * (52 * 10))
    ds.BuildOverviews("NEAR", [2])
    ds.GetRasterBand(1).GetOverview(0).Fill(
        127
    )  # to demonstrate we ignore overviews unfortunately
    ds = None

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(vrt_filename, [filename1, filename2], resampleAlg=gdal.GRIORA_Cubic)

    ds = gdal.Open(vrt_filename)
    assert ds.ReadRaster(
        buf_xsize=10, buf_ysize=1
    ) == b"\xff\xff\xff\xff\xfc\xf0\xf0\xf0\xf0\xf0" + (b"\xff" * 10)

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(
        vrt_filename, [filename1, filename2], resampleAlg=gdal.GRIORA_Bilinear
    )

    ds = gdal.Open(vrt_filename)
    assert ds.ReadRaster(
        buf_xsize=10, buf_ysize=1
    ) == b"\xff\xff\xff\xff\xfb\xf1\xf0\xf0\xf0\xf0" + (b"\xff" * 10)


###############################################################################


def test_vrt_read_CheckCompatibleForDatasetIO():

    anonymous_vrt = gdal.Translate("", "data/rgbsmall.tif", format="MEM")
    another_vrt = gdal.Translate("", anonymous_vrt, width=25, format="VRT")
    assert (
        another_vrt.GetMetadataItem("CheckCompatibleForDatasetIO()", "__DEBUG__") == "1"
    )


###############################################################################
# Fixes https://github.com/OSGeo/gdal/issues/13464


@gdaltest.enable_exceptions()
def test_vrt_read_multithreaded_non_integer_coordinates_nearest(tmp_vsimem):

    gdal.Translate(
        tmp_vsimem / "test.tif",
        "data/byte.tif",
        creationOptions={"BLOCKYSIZE": "1", "COMPRESS": "DEFLATE"},
    )
    gdal.Translate(
        tmp_vsimem / "test.vrt", tmp_vsimem / "test.tif", srcWin=[0.5, 0.5, 10, 10]
    )

    with gdal.Open(tmp_vsimem / "test.vrt") as ds:
        expected = ds.GetRasterBand(1).ReadRaster()

    with gdal.config_option("GDAL_NUM_THREADS", "2"):
        with gdal.Open(tmp_vsimem / "test.vrt") as ds:
            assert ds.GetRasterBand(1).ReadRaster() == expected


###############################################################################
# Fixes https://github.com/OSGeo/gdal/issues/13464


@gdaltest.enable_exceptions()
def test_vrt_read_multithreaded_non_integer_coordinates_nearest_two_sources(tmp_vsimem):

    gdal.Translate(
        tmp_vsimem / "test1.tif",
        "data/byte.tif",
        srcWin=[0, 0, 10, 20],
        width=502,
        height=1002,
        creationOptions={"BLOCKYSIZE": "1", "COMPRESS": "DEFLATE"},
    )
    gdal.Translate(
        tmp_vsimem / "test2.tif",
        "data/byte.tif",
        srcWin=[10, 0, 10, 20],
        width=502,
        height=1002,
        creationOptions={"BLOCKYSIZE": "1", "COMPRESS": "DEFLATE"},
    )

    ds = gdal.Open("data/byte.tif")
    gt = ds.GetGeoTransform()
    extent = [
        gt[0],
        gt[3] + ds.RasterYSize * gt[5],
        gt[0] + ds.RasterXSize * gt[1],
        gt[3],
    ]
    ds = None

    ds = gdal.Open(tmp_vsimem / "test1.tif")
    gt = ds.GetGeoTransform()
    res_big = gt[1]
    ds = None

    gdal.BuildVRT(
        tmp_vsimem / "test.vrt",
        [tmp_vsimem / "test1.tif", tmp_vsimem / "test2.tif"],
        outputBounds=[
            extent[0] + res_big / 2,
            extent[1] + res_big / 2,
            extent[2] - res_big / 2,
            extent[3] - res_big / 2,
        ],
    )

    with gdal.Open(tmp_vsimem / "test.vrt") as ds:
        expected = ds.GetRasterBand(1).ReadRaster()

    with gdal.config_option("GDAL_NUM_THREADS", "2"):
        with gdal.Open(tmp_vsimem / "test.vrt") as ds:
            assert ds.GetRasterBand(1).ReadRaster() == expected

            assert (
                ds.GetMetadataItem("MULTI_THREADED_RASTERIO_LAST_USED", "__DEBUG__")
                == "0"
            )
