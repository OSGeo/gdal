#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VRTRawRasterBand support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil

import gdaltest
import pytest

from osgeo import gdal


def _vrt_has_raw_support():
    drv = gdal.GetDriverByName("VRT")
    return (
        drv is not None
        and drv.GetMetadataItem("GDAL_VRT_ENABLE_RAWRASTERBAND") == "YES"
        and gdaltest.vrt_has_open_support()
    )


pytestmark = pytest.mark.skipif(
    not _vrt_has_raw_support(),
    reason="VRT driver open missing and/or GDAL_VRT_ENABLE_RAWRASTERBAND disabled",
)


def _xmlsearch(root, nodetype, name):
    for node in root[2:]:
        if node[0] == nodetype and node[1] == name:
            return node


###############################################################################
# Verify reading from simple existing raw definition.


def test_vrtrawlink_1():

    tst = gdaltest.GDALTest("VRT", "small.vrt", 2, 12816)
    tst.testOpen()


###############################################################################
# Create a new VRT raw link via the AddBand() method.


@pytest.fixture()
def small_raw_vrt(tmp_path):

    rawlink_vrt = str(tmp_path / "rawlink.vrt")

    shutil.copyfile("data/small.raw", tmp_path / "small.raw")

    driver = gdal.GetDriverByName("VRT")
    ds = driver.Create(rawlink_vrt, 31, 35, 0)

    options = [
        "subClass=VRTRawRasterBand",
        "SourceFilename=small.raw",
        "relativeToVRT=1",
    ]

    result = ds.AddBand(gdal.GDT_UInt8, options)
    assert result == gdal.CE_None, "AddBand() returned error code"

    band = ds.GetRasterBand(1)
    chksum = band.Checksum()

    assert chksum == 12481, "Wrong checksum"

    # Force it to be written to disk.
    ds = None

    return rawlink_vrt


###############################################################################
# Confirm that the newly written file is was saved properly


def test_vrtrawlink_3(small_raw_vrt):

    with gdal.Open(small_raw_vrt) as ds:
        filelist = ds.GetFileList()
        band = ds.GetRasterBand(1)
        chksum = band.Checksum()

        assert chksum == 12481, "Wrong checksum"

        assert len(filelist) == 2, "Wrong filelist"


###############################################################################
# Add a new band, and we will test if we can write to it.


def test_vrtrawlink_4(tmp_path, small_raw_vrt):

    rawlink_dat = str(tmp_path / "rawlink.dat")

    # force creation of the file.
    open(rawlink_dat, "w").write(chr(0))

    # Add a new band pointing to this bogus file.
    options = [
        "subClass=VRTRawRasterBand",
        "SourceFilename=rawlink.dat",
        "relativeToVRT=1",
        "ImageOffset=                      100",
        "PixelOffset=3",
        "LineOffset=93",
        "ByteOrder=MSB",
    ]

    rawlink_ds = gdal.Open(small_raw_vrt, gdal.GA_Update)

    result = rawlink_ds.AddBand(gdal.GDT_UInt16, options)
    assert result == gdal.CE_None, "AddBand() returned error code"

    # write out some simple data.

    band_1 = rawlink_ds.GetRasterBand(1)
    byte_data = band_1.ReadRaster(0, 0, 31, 35)

    band = rawlink_ds.GetRasterBand(2)
    band.WriteRaster(0, 0, 31, 35, byte_data, 31, 35, gdal.GDT_UInt8)

    rawlink_ds.FlushCache()

    # Verify it seems to be right.
    chksum = band.Checksum()

    assert chksum == 12481, "Wrong checksum"

    band_1 = None
    band = None

    # Close and reopen to ensure we are getting data from disk.
    rawlink_ds = None
    rawlink_ds = gdal.Open(small_raw_vrt, gdal.GA_Update)

    band = rawlink_ds.GetRasterBand(2)
    chksum = band.Checksum()

    assert chksum == 12481, "Wrong checksum"

    # verify file length.
    statinfo = os.stat(rawlink_dat)
    assert statinfo.st_size == 3354, "data file is wrong size"


###############################################################################
# Add a new band, and check the relativeToVRT property.


def test_vrtrawlink_5(tmp_path):

    rawlink_vrt = str(tmp_path / "rawlink.vrt")

    driver = gdal.GetDriverByName("VRT")
    ds = driver.Create(rawlink_vrt, 31, 35, 0)

    # Add a new band pointing to this bogus file.
    options = [
        "subClass=VRTRawRasterBand",
        "SourceFilename=rawlink5.dat",
        "relativeToVRT=1",
        "ImageOffset=100",
        "PixelOffset=3",
        "LineOffset=93",
        "ByteOrder=MSB",
    ]

    result = ds.AddBand(gdal.GDT_UInt16, options)
    assert result == gdal.CE_None, "AddBand() returned error code"

    ds.FlushCache()

    # Close and reopen to ensure we are getting data from disk.
    ds = None
    xmlstring = open(rawlink_vrt).read()

    root = gdal.ParseXMLString(xmlstring)
    node = _xmlsearch(root, gdal.CXT_Element, "VRTRasterBand")
    node = _xmlsearch(node, gdal.CXT_Element, "SourceFilename")
    node = _xmlsearch(node, gdal.CXT_Attribute, "relativeToVRT")

    assert node is not None and node[2][1] == "1", "incorrect relativeToVRT value"

    assert (
        "<ImageOffset>100</ImageOffset>" in xmlstring
        and "<PixelOffset>3</PixelOffset>" in xmlstring
        and "<LineOffset>93</LineOffset>" in xmlstring
    )


###############################################################################
# Add a new band with relativeToVRT=1, and re-open the dataset.


def test_vrtrawlink_6(tmp_path):

    rawlink_vrt = str(tmp_path / "rawlink.vrt")

    driver = gdal.GetDriverByName("VRT")
    ds = driver.Create(rawlink_vrt, 31, 35, 0)

    # Add a new band pointing to this bogus file.
    options = [
        "subClass=VRTRawRasterBand",
        "SourceFilename=rawlink6.dat",
        "relativeToVRT=1",
        "ImageOffset=100",
        "PixelOffset=3",
        "LineOffset=93",
        "ByteOrder=MSB",
    ]

    result = ds.AddBand(gdal.GDT_UInt16, options)
    assert result == gdal.CE_None, "AddBand() returned error code"

    ds.FlushCache()

    # Close and reopen to ensure we are getting data from disk.
    ds = None

    ds = gdal.Open(rawlink_vrt)
    assert ds is not None, f'unable to open the dataset: "{rawlink_vrt}"'

    b = ds.GetRasterBand(1)
    assert b is not None, "unable to open the raster band #1"

    assert os.path.exists(
        str(tmp_path / "rawlink6.dat")
    ), 'the raw file is not in the expected location (f"{tmp_path}/rawlink6.dat")'


###############################################################################
# Add a new band with relativeToVRT=1, change directory and re-open the dataset.


def test_vrtrawlink_7(tmp_path):

    rawlink_vrt = str(tmp_path / "rawlink.vrt")

    driver = gdal.GetDriverByName("VRT")
    ds = driver.Create(rawlink_vrt, 31, 35, 0)

    # Add a new band pointing to this bogus file.
    options = [
        "subClass=VRTRawRasterBand",
        "SourceFilename=rawlink7.dat",
        "relativeToVRT=1",
        "ImageOffset=100",
        "PixelOffset=3",
        "LineOffset=93",
        "ByteOrder=MSB",
    ]

    result = ds.AddBand(gdal.GDT_UInt16, options)
    assert result == gdal.CE_None, "AddBand() returned error code"

    ds.FlushCache()

    # Close and reopen to ensure we are getting data from disk.
    ds = None

    prev_dir = os.getcwd()
    os.chdir(tmp_path)
    try:
        ds = gdal.Open("rawlink.vrt")
        assert ds is not None, 'unable to open the dataset: "rawlink.vrt"'

        b = ds.GetRasterBand(1)
        assert b is not None, "unable to open the raster band #1"
    finally:
        os.chdir(prev_dir)


###############################################################################
# Test error case (too much memory allocation)


def test_vrtrawlink_8(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "my.vrt",
        """<VRTDataset rasterXSize="200000" rasterYSize="1">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTRawRasterBand">
            <SourceFilename relativetoVRT="1">my.raw</SourceFilename>
            <ImageOffset>0</ImageOffset>
            <PixelOffset>200000</PixelOffset>
            <LineOffset>1</LineOffset>
            <ByteOrder>LSB</ByteOrder>
        </VRTRasterBand>
        </VRTDataset>""",
    )
    gdal.FileFromMemBuffer(tmp_vsimem / "my.raw", "foo")

    for _ in range(2):
        with pytest.raises(Exception, match=r".*Image file is too small.*"):
            gdal.Open(tmp_vsimem / "my.vrt")


###############################################################################
# Test error case (non-existing file)


def test_vrtrawlink_9(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "my.vrt",
        """<VRTDataset rasterXSize="200000" rasterYSize="1">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTRawRasterBand">
            <SourceFilename relativetoVRT="1">i/do/not/exist</SourceFilename>
            <ImageOffset>0</ImageOffset>
            <PixelOffset>200000</PixelOffset>
            <LineOffset>1</LineOffset>
            <ByteOrder>LSB</ByteOrder>
        </VRTRasterBand>
        </VRTDataset>""",
    )

    with pytest.raises(Exception, match=r".*Unable to open.*"):
        gdal.Open(tmp_vsimem / "my.vrt")


###############################################################################
# Test error case (invalid byte order)


def test_vrtrawlink_10(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "my.vrt",
        """<VRTDataset rasterXSize="1" rasterYSize="1">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTRawRasterBand">
            <SourceFilename relativetoVRT="1">my.raw</SourceFilename>
            <ByteOrder>invalid</ByteOrder>
        </VRTRasterBand>
        </VRTDataset>""",
    )
    gdal.FileFromMemBuffer(tmp_vsimem / "my.raw", "foo")

    with pytest.raises(Exception, match=r".*ByteOrder.*"):
        gdal.Open(tmp_vsimem / "my.vrt")


###############################################################################
#


def test_vrtrawlink_vax_order(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "my.vrt",
        """<VRTDataset rasterXSize="4" rasterYSize="3">
        <VRTRasterBand dataType="Float32" band="1" subClass="VRTRawRasterBand">
            <SourceFilename relativetoVRT="1">data/vicar/vicar_vax_float32.vic</SourceFilename>
            <ImageOffset>368</ImageOffset>
            <PixelOffset>4</PixelOffset>
            <LineOffset>16</LineOffset>
            <ByteOrder>VAX</ByteOrder>
        </VRTRasterBand>
        </VRTDataset>""",
    )
    gdal.FileFromMemBuffer(
        tmp_vsimem / "data/vicar/vicar_vax_float32.vic",
        open("data/vicar/vicar_vax_float32.vic", "rb").read(),
    )

    ds = gdal.Open(tmp_vsimem / "my.vrt")
    assert ds.GetRasterBand(1).Checksum() == 129


###############################################################################
#


def test_vrtrawlink_disabled_open():

    with gdaltest.config_option("GDAL_VRT_ENABLE_RAWRASTERBAND", "NO"):
        with pytest.raises(
            Exception, match="VRTRawRasterBand support has been disabled at run-time"
        ):
            gdal.Open("data/small.vrt")


###############################################################################
#


def test_vrtrawlink_disabled_add_band():

    with gdaltest.config_option("GDAL_VRT_ENABLE_RAWRASTERBAND", "NO"):
        driver = gdal.GetDriverByName("VRT")
        ds = driver.Create("", 31, 35, 0)

        # Add a new band pointing to this bogus file.
        options = [
            "subClass=VRTRawRasterBand",
            "SourceFilename=rawlink7.dat",
            "relativeToVRT=1",
            "ImageOffset=100",
            "PixelOffset=3",
            "LineOffset=93",
            "ByteOrder=MSB",
        ]
        with pytest.raises(
            Exception, match="VRTRawRasterBand support has been disabled at run-time"
        ):
            ds.AddBand(gdal.GDT_UInt16, options)


###############################################################################
#


def test_vrtrawlink_non_relative_filename():

    with pytest.raises(Exception, match="relativeToVRT flag is not set"):
        gdal.Open("""<VRTDataset rasterXSize="1" rasterYSize="1">
            <VRTRasterBand dataType="Byte" band="1" subClass="VRTRawRasterBand">
                <SourceFilename relativetoVRT="0">foo</SourceFilename>
            </VRTRasterBand>
            </VRTDataset>""")


###############################################################################
#


def test_vrtrawlink_relative_filename_with_dotdot():

    with gdaltest.config_option(
        "GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", "SIBLING_OR_CHILD_OF_VRT_PATH"
    ):
        with pytest.raises(
            Exception, match="it may not be a sibling or child of the VRT path"
        ):
            gdal.Open("""<VRTDataset rasterXSize="1" rasterYSize="1">
                <VRTRasterBand dataType="Byte" band="1" subClass="VRTRawRasterBand">
                    <SourceFilename relativetoVRT="1">foo/../bar</SourceFilename>
                </VRTRasterBand>
                </VRTDataset>""")


###############################################################################
#


def test_vrtrawlink_relative_filename_with_abspath():

    with gdaltest.config_option(
        "GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", "SIBLING_OR_CHILD_OF_VRT_PATH"
    ):
        with pytest.raises(
            Exception, match="is invalid because it is not relative to the VRT path "
        ):
            gdal.Open("""<VRTDataset rasterXSize="1" rasterYSize="1">
                <VRTRasterBand dataType="Byte" band="1" subClass="VRTRawRasterBand">
                    <SourceFilename relativetoVRT="1">/etc/passwd</SourceFilename>
                </VRTRasterBand>
                </VRTDataset>""")


###############################################################################
#


def test_vrtrawlink_GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE_ALL():

    with gdaltest.config_option("GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", "ALL"):
        ds = gdal.Open("""<VRTDataset rasterXSize="4" rasterYSize="3">
        <VRTRasterBand dataType="Float32" band="1" subClass="VRTRawRasterBand">
            <SourceFilename>data/vicar/vicar_vax_float32.vic</SourceFilename>
            <ImageOffset>368</ImageOffset>
            <PixelOffset>4</PixelOffset>
            <LineOffset>16</LineOffset>
            <ByteOrder>VAX</ByteOrder>
        </VRTRasterBand>
        </VRTDataset>""")
        assert ds.GetRasterBand(1).Checksum() == 129


###############################################################################
#


def test_vrtrawlink_GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE_ONLY_REMOTE_rejected():

    with gdaltest.config_option("GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", "ONLY_REMOTE"):
        with pytest.raises(Exception, match="is a local file"):
            gdal.Open("""<VRTDataset rasterXSize="4" rasterYSize="3">
        <VRTRasterBand dataType="Float32" band="1" subClass="VRTRawRasterBand">
            <SourceFilename>data/vicar/vicar_vax_float32.vic</SourceFilename>
        </VRTRasterBand>
        </VRTDataset>""")


###############################################################################
#


@pytest.mark.require_curl()
def test_vrtrawlink_GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE_ONLY_REMOTE_accepted():

    with gdaltest.config_option("GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", "ONLY_REMOTE"):
        ds = gdal.Open("""<VRTDataset rasterXSize="4" rasterYSize="3">
        <VRTRasterBand dataType="Float32" band="1" subClass="VRTRawRasterBand">
            <SourceFilename>/vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/refs/heads/release/3.11/autotest/gdrivers/data/vicar/vicar_vax_float32.vic</SourceFilename>
            <ImageOffset>368</ImageOffset>
            <PixelOffset>4</PixelOffset>
            <LineOffset>16</LineOffset>
            <ByteOrder>VAX</ByteOrder>
        </VRTRasterBand>
        </VRTDataset>""")
        assert ds.GetRasterBand(1).Checksum() == 129


###############################################################################
#


def test_vrtrawlink_GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE_paths_invalid():

    with gdaltest.config_option("GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", "invalid"):
        with pytest.raises(Exception, match="'invalid' is not an absolute path"):
            gdal.Open("""<VRTDataset rasterXSize="4" rasterYSize="3">
        <VRTRasterBand dataType="Float32" band="1" subClass="VRTRawRasterBand">
            <SourceFilename>data/vicar/vicar_vax_float32.vic</SourceFilename>
        </VRTRasterBand>
        </VRTDataset>""")


###############################################################################
#


def test_vrtrawlink_GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE_path_escape():

    with gdaltest.config_option("GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", os.getcwd()):
        with pytest.raises(
            Exception,
            match="is invalid because the presence of ../ in it may escape from the allowed path",
        ):
            gdal.Open("""<VRTDataset rasterXSize="4" rasterYSize="3">
        <VRTRasterBand dataType="Float32" band="1" subClass="VRTRawRasterBand">
            <SourceFilename>../gdrivers/data/vicar/vicar_vax_float32.vic</SourceFilename>
        </VRTRasterBand>
        </VRTDataset>""")


###############################################################################
#


def test_vrtrawlink_GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE_path_not_contained():

    with gdaltest.config_option("GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", os.getcwd()):
        with pytest.raises(
            Exception,
            match="'/etc/passwd' is invalid because it is not contained in one of the allowed path",
        ):
            gdal.Open("""<VRTDataset rasterXSize="1" rasterYSize="1">
            <VRTRasterBand dataType="Byte" band="1" subClass="VRTRawRasterBand">
                <SourceFilename>/etc/passwd</SourceFilename>
            </VRTRasterBand>
            </VRTDataset>""")


###############################################################################
#


def test_vrtrawlink_GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE_path_ok():

    with gdaltest.config_option("GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", os.getcwd()):
        ds = gdal.Open(f"""<VRTDataset rasterXSize="4" rasterYSize="3">
        <VRTRasterBand dataType="Float32" band="1" subClass="VRTRawRasterBand">
            <SourceFilename>{os.getcwd()}/data/vicar/vicar_vax_float32.vic</SourceFilename>
            <ImageOffset>368</ImageOffset>
            <PixelOffset>4</PixelOffset>
            <LineOffset>16</LineOffset>
            <ByteOrder>VAX</ByteOrder>
        </VRTRasterBand>
        </VRTDataset>""")
        assert ds.GetRasterBand(1).Checksum() == 129
