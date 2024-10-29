#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GeoRaster Testing.
# Author:   Ivan Lucena <ivan.lucena@pmldnet.com>
#
###############################################################################
# Copyright (c) 2008, Ivan Lucena <ivan.lucena@pmldnet.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

from osgeo import gdal, ogr

pytestmark = [
    pytest.mark.skipif(
        "OCI_DSNAME" not in os.environ, reason="no OCI_DSNAME in environment"
    ),
    pytest.mark.require_driver("GeoRaster"),
]


###############################################################################
#


def get_connection_str():

    oci_dsname = os.environ.get("OCI_DSNAME")

    if oci_dsname is None:
        # TODO: Spelling - informe?
        return "<error: informe ORACLE connection>"
    return "geor:" + oci_dsname[oci_dsname.index(":") + 1 :]


###############################################################################
#


def test_georaster_init():

    gdaltest.oci_ds = None

    gdaltest.georasterDriver = gdal.GetDriverByName("GeoRaster")

    gdaltest.oci_ds = ogr.Open(os.environ["OCI_DSNAME"])

    if gdaltest.oci_ds is None:
        pytest.skip()

    with gdal.quiet_errors():
        rs = gdaltest.oci_ds.ExecuteSQL("select owner from all_sdo_geor_sysdata")

    err_msg = gdal.GetLastErrorMsg()

    if rs is not None:
        gdaltest.oci_ds.ReleaseResultSet(rs)
        rs = None

    if err_msg != "":
        gdaltest.oci_ds = None
        pytest.skip(
            "ALL_SDO_GEOR_SYSDATA inaccessible, " "likely georaster unavailable."
        )


###############################################################################
#


def test_georaster_byte():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/byte.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1001, sdo_geor.init('GDAL_TEST_RDT',1001))",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 4672, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_int16():
    if gdaltest.oci_ds is None:
        pytest.skip()

    get_connection_str()

    ds_src = gdal.Open("data/int16.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1002, sdo_geor.init('GDAL_TEST_RDT',1002))",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 4672, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_int32():
    if gdaltest.oci_ds is None:
        pytest.skip()

    get_connection_str()

    ds_src = gdal.Open("data/int32.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1003, sdo_geor.init('GDAL_TEST_RDT',1003))",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 4672, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_rgb_b1():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/rgbsmall.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1004, sdo_geor.init('GDAL_TEST_RDT',1004))",
            "BLOCKBSIZE=1",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 21212, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_rgb_b2():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/rgbsmall.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1005, sdo_geor.init('GDAL_TEST_RDT',1005))",
            "BLOCKBSIZE=2",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 21212, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_rgb_b3_bsq():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/rgbsmall.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1006, sdo_geor.init('GDAL_TEST_RDT',1006))",
            "BLOCKBSIZE=3",
            "INTERLEAVE=BSQ",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 21212, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_rgb_b3_bip():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/rgbsmall.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1007, sdo_geor.init('GDAL_TEST_RDT',1007))",
            "BLOCKBSIZE=3",
            "INTERLEAVE=BIP",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 21212, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_rgb_b3_bil():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/rgbsmall.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1008, sdo_geor.init('GDAL_TEST_RDT',1008))",
            "BLOCKBSIZE=3",
            "INTERLEAVE=BIL",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 21212, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_byte_deflate():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/byte.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1009, sdo_geor.init('GDAL_TEST_RDT',1009))",
            "COMPRESS=DEFLATE",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 4672, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_rgb_deflate_b3():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/rgbsmall.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1010, sdo_geor.init('GDAL_TEST_RDT',1010))",
            "COMPRESS=DEFLATE",
            "BLOCKBSIZE=3",
            "INTERLEAVE=PIXEL",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 21212, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_1bit():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/byte.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1011, sdo_geor.init('GDAL_TEST_RDT',1011))",
            "NBITS=1",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 252, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_2bit():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/byte.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1012, sdo_geor.init('GDAL_TEST_RDT',1012))",
            "NBITS=2",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 718, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_4bit():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/byte.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1013, sdo_geor.init('GDAL_TEST_RDT',1013))",
            "NBITS=4",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 2578, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_genstats():
    if gdaltest.oci_ds is None:
        pytest.skip()

    ds_src = gdal.Open("data/byte.tif")

    ds = gdaltest.georasterDriver.CreateCopy(
        get_connection_str() + ",GDAL_TEST,RASTER",
        ds_src,
        1,
        [
            "DESCRIPTION=(id number, raster sdo_georaster)",
            "INSERT=(1014, sdo_geor.init('GDAL_TEST_RDT',1014))",
            "NBITS=4",
            "GENSTATS=TRUE",
        ],
    )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest("GeoRaster", ds_name, 1, 2578, filename_absolute=1)

    tst.testOpen()


###############################################################################
#


def test_georaster_cleanup():
    if gdaltest.oci_ds is None:
        pytest.skip()

    gdaltest.oci_ds.ExecuteSQL("drop table GDAL_TEST")
    gdaltest.oci_ds.ExecuteSQL("drop table GDAL_TEST_RDT")

    gdaltest.oci_ds.Close()
    gdaltest.oci_ds = None


###############################################################################
#
