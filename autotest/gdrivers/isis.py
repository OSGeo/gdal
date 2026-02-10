#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ISIS3 formats.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2017, Hobu Inc
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import struct

import gdaltest
import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("ISIS3")

###############################################################################
# Perform simple read test on isis3 detached dataset.


def test_isis_1():
    srs = """PROJCS["Equirectangular Mars",
    GEOGCS["GCS_Mars",
        DATUM["D_Mars",
            SPHEROID["Mars_localRadius",3394813.857978216,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Equirectangular"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",184.4129944],
    PARAMETER["standard_parallel_1",-15.1470003],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1]]
"""
    gt = (
        -4766.96484375,
        10.102499961853027,
        0.0,
        -872623.625,
        0.0,
        -10.102499961853027,
    )

    tst = gdaltest.GDALTest("ISIS3", "isis3/isis3_detached.lbl", 1, 9978)
    tst.testOpen(check_prj=srs, check_gt=gt)


###############################################################################
# Perform simple read test on isis3 detached dataset.


def test_isis_2():
    srs = """PROJCS["Equirectangular mars",
    GEOGCS["GCS_mars",
        DATUM["D_mars",
            SPHEROID["mars_localRadius",3388271.702979241,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Equirectangular"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",195.92],
    PARAMETER["standard_parallel_1",-38.88],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1]]
"""
    gt = (653.132641495800044, 0.38, 0, -2298409.710162799805403, 0, -0.38)

    tst = gdaltest.GDALTest("ISIS3", "isis3/isis3_unit_test.cub", 1, 42403)
    tst.testOpen(check_prj=srs, check_gt=gt)


###############################################################################
# Perform simple read test on isis3 detached dataset with GeoTIFF image file


def test_isis_3():
    srs = """PROJCS["Equirectangular Mars",
    GEOGCS["GCS_Mars",
        DATUM["D_Mars",
            SPHEROID["Mars_localRadius",3394813.857978216,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Equirectangular"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",184.4129944],
    PARAMETER["standard_parallel_1",-15.1470003],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["metre",1]]
"""
    gt = (
        -4766.96484375,
        10.102499961853027,
        0.0,
        -872623.625,
        0.0,
        -10.102499961853027,
    )

    tst = gdaltest.GDALTest("ISIS3", "isis3/isis3_geotiff.lbl", 1, 9978)
    tst.testOpen(check_prj=srs, check_gt=gt)


# ISIS3 -> ISIS3 conversion


def test_isis_4():

    tst = gdaltest.GDALTest("ISIS3", "isis3/isis3_detached.lbl", 1, 9978)
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ds.GetMetadataDomainList() == ["", "json:ISIS3"]
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    # Couldn't be preserved, since points to dangling file
    assert "OriginalLabel" not in lbl
    assert "PositiveWest" not in lbl
    assert ds.GetRasterBand(1).GetMaskFlags() == 0
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 12220
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")

    # Preserve source Mapping group as well
    tst = gdaltest.GDALTest(
        "ISIS3", "isis3/isis3_detached.lbl", 1, 9978, options=["USE_SRC_MAPPING=YES"]
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert "PositiveWest" in lbl
    assert "Planetographic" in lbl
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")

    # Preserve source Mapping group, but with a few overrides
    tst = gdaltest.GDALTest(
        "ISIS3",
        "isis3/isis3_detached.lbl",
        1,
        9978,
        options=[
            "USE_SRC_MAPPING=YES",
            "LONGITUDE_DIRECTION=PositiveEast",
            "LATITUDE_TYPE=Planetocentric",
            "TARGET_NAME=my_label",
        ],
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert "PositiveEast" in lbl
    assert "Planetocentric" in lbl
    assert "my_label" in lbl
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Label+image creation + WRITE_BOUNDING_DEGREES=NO option


def test_isis_5():

    tst = gdaltest.GDALTest(
        "ISIS3",
        "isis3/isis3_detached.lbl",
        1,
        9978,
        options=["USE_SRC_LABEL=NO", "WRITE_BOUNDING_DEGREES=NO"],
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    assert gdal.VSIStatL("/vsimem/isis_tmp.cub") is None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert "MinimumLongitude" not in lbl
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Detached label creation and COMMENT option


def test_isis_6():

    tst = gdaltest.GDALTest(
        "ISIS3",
        "isis3/isis3_detached.lbl",
        1,
        9978,
        options=["DATA_LOCATION=EXTERNAL", "USE_SRC_LABEL=NO", "COMMENT=my comment"],
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    assert gdal.VSIStatL("/vsimem/isis_tmp.cub") is not None
    f = gdal.VSIFOpenL("/vsimem/isis_tmp.lbl", "rb")
    content = gdal.VSIFReadL(1, 10000, f).decode("ASCII")
    gdal.VSIFCloseL(f)
    assert "#my comment" in content
    assert len(content) != 10000
    ds = gdal.Open("/vsimem/isis_tmp.lbl", gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Uncompressed GeoTIFF creation


def test_isis_7():

    tst = gdaltest.GDALTest(
        "ISIS3",
        "isis3/isis3_detached.lbl",
        1,
        9978,
        options=["DATA_LOCATION=GEOTIFF", "USE_SRC_LABEL=NO"],
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    assert gdal.VSIStatL("/vsimem/isis_tmp.tif") is not None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert '"Format":"BandSequential"' in lbl
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")

    # Test GEOTIFF_AS_REGULAR_EXTERNAL = NO
    tst = gdaltest.GDALTest(
        "ISIS3",
        "isis3/isis3_detached.lbl",
        1,
        9978,
        options=[
            "DATA_LOCATION=GEOTIFF",
            "GEOTIFF_AS_REGULAR_EXTERNAL=NO",
            "USE_SRC_LABEL=NO",
        ],
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    assert gdal.VSIStatL("/vsimem/isis_tmp.tif") is not None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert '"Format":"GeoTIFF"' in lbl
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Compressed GeoTIFF creation


def test_isis_8():

    tst = gdaltest.GDALTest(
        "ISIS3",
        "isis3/isis3_detached.lbl",
        1,
        9978,
        options=[
            "DATA_LOCATION=GEOTIFF",
            "USE_SRC_LABEL=NO",
            "GEOTIFF_OPTIONS=COMPRESS=LZW",
        ],
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    assert gdal.VSIStatL("/vsimem/isis_tmp.tif") is not None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert '"Format":"GeoTIFF"' in lbl
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl", gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Tiled creation + EXTERNAL_FILENAME


def test_isis_9():

    tst = gdaltest.GDALTest(
        "ISIS3",
        "isis3/isis3_detached.lbl",
        1,
        9978,
        options=[
            "DATA_LOCATION=EXTERNAL",
            "USE_SRC_LABEL=NO",
            "TILED=YES",
            "EXTERNAL_FILENAME=/vsimem/foo.bin",
        ],
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    assert gdal.VSIStatL("/vsimem/foo.bin") is not None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert (
        '"Format":"Tile"' in lbl
        and '"TileSamples":256' in lbl
        and '"TileLines":256' in lbl
    )
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")
    assert gdal.VSIStatL("/vsimem/foo.bin") is None


# Tiled creation + regular GeoTIFF + EXTERNAL_FILENAME


def test_isis_10():

    tst = gdaltest.GDALTest(
        "ISIS3",
        "isis3/isis3_detached.lbl",
        1,
        9978,
        options=[
            "USE_SRC_LABEL=NO",
            "DATA_LOCATION=GEOTIFF",
            "TILED=YES",
            "BLOCKXSIZE=16",
            "BLOCKYSIZE=32",
            "EXTERNAL_FILENAME=/vsimem/foo.tif",
        ],
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    ds = gdal.Open("/vsimem/foo.tif")
    assert ds.GetRasterBand(1).GetBlockSize() == [16, 32]
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")
    assert gdal.VSIStatL("/vsimem/foo.tif") is None


# Tiled creation + compressed GeoTIFF


def test_isis_11():

    tst = gdaltest.GDALTest(
        "ISIS3",
        "isis3/isis3_detached.lbl",
        1,
        9978,
        options=[
            "USE_SRC_LABEL=NO",
            "DATA_LOCATION=GEOTIFF",
            "TILED=YES",
            "GEOTIFF_OPTIONS=COMPRESS=LZW",
        ],
    )
    tst.testCreateCopy(new_filename="/vsimem/isis_tmp.lbl", delete_copy=0)
    ds = gdal.Open("/vsimem/isis_tmp.tif")
    assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Multiband


def test_isis_12():

    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    gdal.Translate("/vsimem/isis_tmp.lbl", src_ds, format="ISIS3")
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        assert cs == expected_cs, (i + 1, cs, expected_cs)
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Multiband tiled


def test_isis_13():

    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    gdal.Translate(
        "/vsimem/isis_tmp.lbl",
        src_ds,
        format="ISIS3",
        creationOptions=["TILED=YES", "BLOCKXSIZE=16", "BLOCKYSIZE=32"],
    )
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        assert cs == expected_cs, (i + 1, cs, expected_cs)
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Multiband with uncompressed GeoTIFF


def test_isis_14():

    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    gdal.Translate(
        "/vsimem/isis_tmp.lbl",
        src_ds,
        format="ISIS3",
        creationOptions=["DATA_LOCATION=GEOTIFF"],
    )
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        assert cs == expected_cs, (i + 1, cs, expected_cs)
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Multiband with uncompressed tiled GeoTIFF


def test_isis_15():

    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    gdal.Translate(
        "/vsimem/isis_tmp.lbl",
        src_ds,
        format="ISIS3",
        creationOptions=[
            "DATA_LOCATION=GEOTIFF",
            "TILED=YES",
            "BLOCKXSIZE=16",
            "BLOCKYSIZE=32",
        ],
    )
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        expected_cs = src_ds.GetRasterBand(i + 1).Checksum()
        assert cs == expected_cs, (i + 1, cs, expected_cs)
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Test Create() without anything else


def test_isis_16():

    for read_before_write in [False, True]:
        for init_nd in [False, True]:
            for dt, cs, nd, options in [
                [gdal.GDT_UInt8, 0, 0, []],
                [gdal.GDT_UInt8, 0, 0, ["TILED=YES"]],
                [
                    gdal.GDT_UInt8,
                    0,
                    0,
                    ["DATA_LOCATION=GEOTIFF", "GEOTIFF_OPTIONS=COMPRESS=LZW"],
                ],
                [gdal.GDT_Int16, 65525, -32768, []],
                [gdal.GDT_UInt16, 0, 0, []],
                [gdal.GDT_Float32, 65534, -3.4028226550889045e38, []],
            ]:

                ds = gdal.GetDriverByName("ISIS3").Create(
                    "/vsimem/isis_tmp.lbl", 1, 2, 1, dt, options=options
                )
                ds.GetRasterBand(1).SetOffset(10)
                ds.GetRasterBand(1).SetScale(20)
                if read_before_write:
                    ds.GetRasterBand(1).ReadRaster()
                if init_nd:
                    ds.GetRasterBand(1).Fill(nd)
                ds = None
                ds = gdal.Open("/vsimem/isis_tmp.lbl")
                assert ds.GetRasterBand(1).Checksum() == cs, (
                    dt,
                    cs,
                    nd,
                    options,
                    init_nd,
                    ds.GetRasterBand(1).Checksum(),
                )
                assert ds.GetRasterBand(1).GetMaskFlags() == 0, (
                    dt,
                    cs,
                    nd,
                    options,
                    init_nd,
                    ds.GetRasterBand(1).GetMaskFlags(),
                )
                assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 0, (
                    dt,
                    cs,
                    nd,
                    options,
                    init_nd,
                    ds.GetRasterBand(1).GetMaskBand().Checksum(),
                )
                assert ds.GetRasterBand(1).GetOffset() == 10, (
                    dt,
                    cs,
                    nd,
                    options,
                    init_nd,
                    ds.GetRasterBand(1).GetOffset(),
                )
                assert ds.GetRasterBand(1).GetScale() == 20, (
                    dt,
                    cs,
                    nd,
                    options,
                    init_nd,
                    ds.GetRasterBand(1).GetScale(),
                )
                assert ds.GetRasterBand(1).GetNoDataValue() == nd, (
                    dt,
                    cs,
                    nd,
                    options,
                    init_nd,
                    ds.GetRasterBand(1).GetNoDataValue(),
                )
                ds = None
                gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Test create copy through Create()


def test_isis_17():

    tst = gdaltest.GDALTest("ISIS3", "isis3/isis3_detached.lbl", 1, 9978)
    tst.testCreate(vsimem=1)


# Test SRS serialization and deserialization


def test_isis_18():

    sr = osr.SpatialReference()
    sr.SetEquirectangular2(0, 1, 2, 0, 0)
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    wkt = ds.GetProjectionRef()
    ds = None
    assert osr.SpatialReference(wkt).IsSame(
        osr.SpatialReference(
            'PROJCS["Equirectangular DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME_localRadius",123455.2424988797,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Equirectangular"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",1],PARAMETER["standard_parallel_1",2],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1]]'
        )
    )

    sr = osr.SpatialReference()
    sr.SetEquirectangular2(123456, 1, 2, 987654, 3210123)
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    with gdal.quiet_errors():
        # Will warn that latitude_of_origin, false_easting and false_northing are ignored
        ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    wkt = ds.GetProjectionRef()
    ds = None
    assert osr.SpatialReference(wkt).IsSame(
        osr.SpatialReference(
            'PROJCS["Equirectangular DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME_localRadius",123455.2424988797,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Equirectangular"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",1],PARAMETER["standard_parallel_1",2],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1]]'
        )
    )

    sr = osr.SpatialReference()
    sr.SetOrthographic(1, 2, 0, 0)
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    wkt = ds.GetProjectionRef()
    ds = None
    assert osr.SpatialReference(wkt).IsSame(
        osr.SpatialReference(
            'PROJCS["Orthographic DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Orthographic"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1]]'
        )
    )

    sr = osr.SpatialReference()
    sr.SetSinusoidal(1, 0, 0)
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    wkt = ds.GetProjectionRef()
    ds = None
    assert osr.SpatialReference(wkt).IsSame(
        osr.SpatialReference(
            'PROJCS["Sinusoidal DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Sinusoidal"],PARAMETER["longitude_of_center",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1]]'
        )
    )

    sr = osr.SpatialReference()
    sr.SetMercator(0, 2, 0.9, 0, 0)
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    wkt = ds.GetProjectionRef()
    ds = None
    assert osr.SpatialReference(wkt).IsSame(
        osr.SpatialReference(
            'PROJCS["Mercator DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Mercator_1SP"],PARAMETER["central_meridian",2],PARAMETER["scale_factor",0.9],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1]]'
        )
    ), wkt

    sr = osr.SpatialReference()
    sr.SetPS(1, 2, 0.9, 0, 0)
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    wkt = ds.GetProjectionRef()
    ds = None
    assert osr.SpatialReference(wkt).IsSame(
        osr.SpatialReference(
            'PROJCS["PolarStereographic DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME_polarRadius",122838.72,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Polar_Stereographic"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["scale_factor",0.9],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1],AXIS["Easting",SOUTH],AXIS["Northing",SOUTH]]'
        )
    ), wkt

    sr = osr.SpatialReference()
    sr.SetTM(1, 2, 0.9, 0, 0)
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    wkt = ds.GetProjectionRef()
    ds = None
    assert osr.SpatialReference(wkt).IsSame(
        osr.SpatialReference(
            'PROJCS["TransverseMercator DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",1],PARAMETER["central_meridian",2],PARAMETER["scale_factor",0.9],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1]]'
        )
    )

    sr = osr.SpatialReference()
    sr.SetLCC(1, 2, 3, 4, 0, 0)
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    ds.SetProjection(sr.ExportToWkt())
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    wkt = ds.GetProjectionRef()
    ds = None
    assert osr.SpatialReference(wkt).IsSame(
        osr.SpatialReference(
            'PROJCS["LambertConformal DATUM_NAME",GEOGCS["GCS_DATUM_NAME",DATUM["D_DATUM_NAME",SPHEROID["DATUM_NAME",123456,0]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",1],PARAMETER["standard_parallel_2",2],PARAMETER["latitude_of_origin",3],PARAMETER["central_meridian",4],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1]]'
        )
    )

    sr = osr.SpatialReference()
    sr.SetEquirectangular2(0, 1, 2, 0, 0)
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/isis_tmp.lbl",
        1,
        1,
        options=[
            "LATITUDE_TYPE=Planetographic",
            "TARGET_NAME=my_target",
            "BOUNDING_DEGREES=1.5,2.5,3.5,4.5",
        ],
    )
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([1000, 1, 0, 2000, 0, -1])
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert '"TargetName":"my_target"' in lbl
    assert '"LatitudeType":"Planetographic"' in lbl
    assert '"MinimumLatitude":2.5' in lbl
    assert '"MinimumLongitude":1.5' in lbl
    assert '"MaximumLatitude":4.5' in lbl
    assert '"MaximumLongitude":3.5' in lbl
    ds = None

    sr = osr.SpatialReference()
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/isis_tmp.lbl", 100, 100, options=["LONGITUDE_DIRECTION=PositiveWest"]
    )
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([10, 1, 0, 40, 0, -1])
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert '"LongitudeDirection":"PositiveWest"' in lbl
    assert '"LongitudeDomain":180' in lbl
    assert '"MinimumLatitude":-60' in lbl
    assert '"MinimumLongitude":-110' in lbl
    assert '"MaximumLatitude":40' in lbl
    assert '"MaximumLongitude":-10' in lbl
    ds = None

    sr = osr.SpatialReference()
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 123456, 200)
    ds = gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/isis_tmp.lbl", 100, 100, options=["FORCE_360=YES"]
    )
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([-10, 1, 0, 40, 0, -1])
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert '"MinimumLatitude":-60' in lbl
    assert '"MinimumLongitude":90' in lbl
    assert '"MaximumLatitude":40' in lbl
    assert '"MaximumLongitude":350' in lbl
    assert '"UpperLeftCornerX":-21547' in lbl
    assert '"UpperLeftCornerY":86188' in lbl
    ds = None

    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Test gdal.Info() with json:ISIS3 metadata domain


def test_isis_19():

    ds = gdal.Open("data/isis3/isis3_detached.lbl")
    res = gdal.Info(ds, format="json", extraMDDomains=["json:ISIS3"])
    assert res["metadata"]["json:ISIS3"]["IsisCube"]["_type"] == "object"

    ds = gdal.Open("data/isis3/isis3_detached.lbl")
    res = gdal.Info(ds, extraMDDomains=["json:ISIS3"])
    assert "IsisCube" in res


# Test gdal.Translate() subsetting and label preservation


def test_isis_20():

    with gdal.quiet_errors():
        gdal.Translate(
            "/vsimem/isis_tmp.lbl",
            "data/isis3/isis3_detached.lbl",
            format="ISIS3",
            srcWin=[0, 0, 1, 1],
        )
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert "AMadeUpValue" in lbl
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Test gdal.Warp() and label preservation


def test_isis_21():

    with gdal.quiet_errors():
        gdal.Warp(
            "/vsimem/isis_tmp.lbl", "data/isis3/isis3_detached.lbl", format="ISIS3"
        )
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ds.GetRasterBand(1).Checksum() == 9978
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert "AMadeUpValue" in lbl
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Test source JSon use


def test_isis_22():

    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    # Invalid Json
    js = """invalid"""
    with pytest.raises(Exception):
        ds.SetMetadata([js], "json:ISIS3")
    ds = None
    gdal.Unlink("/vsimem/isis_tmp.lbl")

    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    # Invalid type for IsisCube
    js = """{ "IsisCube": 5 }"""
    ds.SetMetadata([js], "json:ISIS3")
    lbl = ds.GetMetadata_List("json:ISIS3")
    assert lbl is not None
    ds.SetMetadata([js], "json:ISIS3")
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ds is not None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")

    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    # Invalid type for IsisCube.Core
    js = """{ "IsisCube": { "_type": "object", "Core": 5 } }"""
    ds.SetMetadata([js], "json:ISIS3")
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ds is not None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")

    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    # Invalid type for IsisCube.Core.Dimensions and IsisCube.Core.Pixels
    js = """{ "IsisCube": { "_type": "object", "Core": { "_type": "object",
                                        "Dimensions": 5, "Pixels": 5 } } }"""
    ds.SetMetadata([js], "json:ISIS3")
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ds is not None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")

    ds = gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/isis_tmp.lbl", 1, 1, options=["DATA_LOCATION=EXTERNAL"]
    )
    js = """{ "IsisCube": { "foo": "bar", "bar": [ 123, 124.0, 2.5, "xyz", "anotherveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeerylooooongtext",
    234, 456, 789, 234, 567, 890, 123456789.0, 123456789.0, 123456789.0, 123456789.0, 123456789.0 ],
                         "baz" : { "value": 5, "unit": "M" }, "baw": "with space",
                         "very_long": "aveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeerylooooongtext"} }"""
    ds.SetMetadata([js], "json:ISIS3")
    ds = None

    f = gdal.VSIFOpenL("/vsimem/isis_tmp.lbl", "rb")
    assert f is not None
    content = gdal.VSIFReadL(1, 10000, f).decode("ASCII")
    gdal.VSIFCloseL(f)

    assert (
        "foo       = bar" in content
        and "  bar       = (123, 124.0, 2.5, xyz" in content
        and "               anotherveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-"
        in content
        and "               eeeeeeeeeeeeeeeeeeeeeeeerylooooongtext, 234, 456, 789, 234, 567,"
        in content
        and "               890, 123456789.0, 123456789.0, 123456789.0, 123456789.0,"
        in content
        and "               123456789.0)" in content
        and "baz       = 5 <M>" in content
        and 'baw       = "with space"' in content
        and "very_long = aveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-"
        in content
    )

    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert (
        '"foo":"bar"' in lbl
        and "123" in lbl
        and "2.5" in lbl
        and "xyz" in lbl
        and '"value":5' in lbl
        and '"unit":"M"' in lbl
        and '"baw":"with space"' in lbl
        and '"very_long":"aveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeerylooooongtext"'
        in lbl
    )
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


# Test nodata remapping


def test_isis_23():

    mem_ds = gdal.Translate("", "data/byte.tif", format="MEM")
    mem_ds.SetProjection("")
    mem_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    mem_ds.GetRasterBand(1).SetNoDataValue(74)
    ref_data = mem_ds.GetRasterBand(1).ReadRaster()
    gdal.Translate("/vsimem/isis_tmp.lbl", mem_ds, format="ISIS3")
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ref_data != ds.GetRasterBand(1).ReadRaster()
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")

    gdal.Translate(
        "/vsimem/isis_tmp.lbl",
        mem_ds,
        format="ISIS3",
        creationOptions=["DATA_LOCATION=GeoTIFF"],
    )
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ref_data != ds.GetRasterBand(1).ReadRaster()
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")

    gdal.Translate(
        "/vsimem/isis_tmp.lbl", mem_ds, format="ISIS3", creationOptions=["TILED=YES"]
    )
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    assert ref_data != ds.GetRasterBand(1).ReadRaster()
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")

    for dt in [gdal.GDT_Int16, gdal.GDT_UInt16, gdal.GDT_Float32]:
        mem_ds = gdal.Translate("", "data/byte.tif", format="MEM", outputType=dt)
        mem_ds.SetProjection("")
        mem_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
        mem_ds.GetRasterBand(1).SetNoDataValue(74)
        ref_data = mem_ds.GetRasterBand(1).ReadRaster()
        gdal.Translate("/vsimem/isis_tmp.lbl", mem_ds, format="ISIS3")
        ds = gdal.Open("/vsimem/isis_tmp.lbl")
        assert ref_data != ds.GetRasterBand(1).ReadRaster()
        ds = None
        gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


def cancel_cbk(pct, msg, user_data):
    # pylint: disable=unused-argument
    return 0


# Test error cases


@gdaltest.disable_exceptions()
def test_isis_24():

    # For DATA_LOCATION=EXTERNAL, the main filename should have a .lbl extension
    with gdal.quiet_errors():
        ds = gdal.GetDriverByName("ISIS3").Create(
            "/vsimem/error.txt", 1, 1, options=["DATA_LOCATION=EXTERNAL"]
        )
    assert ds is None

    # cannot create external filename
    with gdal.quiet_errors():
        ds = gdal.GetDriverByName("ISIS3").Create(
            "/vsimem/error.lbl",
            1,
            1,
            options=[
                "DATA_LOCATION=EXTERNAL",
                "EXTERNAL_FILENAME=/i_dont/exist/error.cub",
            ],
        )
    assert ds is None

    # no GTiff driver
    # with gdal.quiet_errors():
    #    gtiff_drv = gdal.GetDriverByName('GTiff')
    #    gtiff_drv.Deregister()
    #    ds = gdal.GetDriverByName('ISIS3').Create('/vsimem/error.lbl', 1, 1,
    #        options = ['DATA_LOCATION=GEOTIFF' ])
    #    gtiff_drv.Register()
    # if ds is not None:
    #    gdaltest.post_reason('fail')
    #    return 'fail'

    # cannot create GeoTIFF
    with gdal.quiet_errors():
        ds = gdal.GetDriverByName("ISIS3").Create(
            "/vsimem/error.lbl",
            1,
            1,
            options=[
                "DATA_LOCATION=GEOTIFF",
                "EXTERNAL_FILENAME=/i_dont/exist/error.tif",
            ],
        )
    assert ds is None
    gdal.Unlink("/vsimem/error.lbl")

    # Output file has same name as input file
    src_ds = gdal.Translate("/vsimem/out.tif", "data/byte.tif")
    with gdal.quiet_errors():
        ds = gdal.GetDriverByName("ISIS3").CreateCopy(
            "/vsimem/out.lbl", src_ds, options=["DATA_LOCATION=GEOTIFF"]
        )
    assert ds is None
    gdal.Unlink("/vsimem/out.tif")

    # Missing /vsimem/out.cub
    src_ds = gdal.Open("data/byte.tif")
    with gdal.quiet_errors():
        gdal.GetDriverByName("ISIS3").CreateCopy(
            "/vsimem/out.lbl", src_ds, options=["DATA_LOCATION=EXTERNAL"]
        )
    gdal.Unlink("/vsimem/out.cub")
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl", gdal.GA_Update)
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")

    # Missing /vsimem/out.tif
    src_ds = gdal.Open("data/byte.tif")
    with gdal.quiet_errors():
        gdal.GetDriverByName("ISIS3").CreateCopy(
            "/vsimem/out.lbl",
            src_ds,
            options=["DATA_LOCATION=GEOTIFF", "GEOTIFF_OPTIONS=COMPRESS=LZW"],
        )
    gdal.Unlink("/vsimem/out.tif")
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")

    # Invalid StartByte
    gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/out.lbl",
        1,
        1,
        options=["DATA_LOCATION=GEOTIFF", "GEOTIFF_OPTIONS=COMPRESS=LZW"],
    )
    gdal.FileFromMemBuffer(
        "/vsimem/out.lbl",
        """Object = IsisCube
  Object = Core
    StartByte = 2
    Format = GeoTIFF
    ^Core = out.tif
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""",
    )
    with gdal.quiet_errors():
        gdal.Open("/vsimem/out.lbl")
    gdal.Unlink("/vsimem/out.tif")
    with gdal.quiet_errors():
        gdal.GetDriverByName("ISIS3").Delete("/vsimem/out.lbl")

    gdal.FileFromMemBuffer("/vsimem/out.lbl", "IsisCube")
    assert gdal.IdentifyDriver("/vsimem/out.lbl") is not None
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")

    gdal.FileFromMemBuffer(
        "/vsimem/out.lbl",
        """Object = IsisCube
  Object = Core
    Format = Tile
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = Real
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""",
    )
    # Wrong tile dimensions : 0 x 0
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")

    gdal.FileFromMemBuffer(
        "/vsimem/out.lbl",
        """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 0
      Lines   = 0
      Bands   = 0
    End_Group
    Group = Pixels
      Type       = Real
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""",
    )
    # Invalid dataset dimensions : 0 x 0
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")

    gdal.FileFromMemBuffer(
        "/vsimem/out.lbl",
        """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 0
    End_Group
    Group = Pixels
      Type       = Real
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""",
    )
    # Invalid band count : 0
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")

    gdal.FileFromMemBuffer(
        "/vsimem/out.lbl",
        """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = unhandled
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""",
    )

    # unhandled format
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")

    gdal.FileFromMemBuffer(
        "/vsimem/out.lbl",
        """Object = IsisCube
  Object = Core
    Format = unhandled
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object
End""",
    )

    # bad PDL formatting
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")

    gdal.FileFromMemBuffer(
        "/vsimem/out.lbl",
        """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
  End_Object
End_Object
End""",
    )

    # missing Group = Pixels. This is actually valid. Assuming Real
    ds = gdal.Open("/vsimem/out.lbl")
    assert ds is not None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")

    gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/out.lbl",
        1,
        1,
        options=["DATA_LOCATION=GEOTIFF", "GEOTIFF_OPTIONS=COMPRESS=LZW"],
    )
    # /vsimem/out.tif has incompatible characteristics with the ones declared in the label
    gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 1, 2)
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 2, 1)
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 1, 1, 2)
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 1, 1, 1, gdal.GDT_Int16)
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")
    gdal.Unlink("/vsimem/out.tif")

    gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/out.lbl", 1, 1, options=["DATA_LOCATION=GEOTIFF"]
    )
    # /vsimem/out.tif has incompatible characteristics with the ones declared in the label
    gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 1, 2)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert gdal.GetLastErrorMsg() != ""
    gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 2, 1)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert gdal.GetLastErrorMsg() != ""
    gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 1, 1, 2)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert gdal.GetLastErrorMsg() != ""
    gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 1, 1, 1, gdal.GDT_Int16)
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert gdal.GetLastErrorMsg() != ""
    gdal.GetDriverByName("GTiff").Create(
        "/vsimem/out.tif", 1, 1, options=["COMPRESS=LZW"]
    )
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert gdal.GetLastErrorMsg() != ""
    gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 1, 1, options=["TILED=YES"])
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert gdal.GetLastErrorMsg() != ""
    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/out.tif", 1, 1)
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.SetMetadataItem("foo", "bar")
    ds = None
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/out.lbl")
    assert gdal.GetLastErrorMsg() != ""
    with gdal.quiet_errors():
        gdal.GetDriverByName("ISIS3").Delete("/vsimem/out.lbl")
    gdal.Unlink("/vsimem/out.tif")

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with gdal.quiet_errors():
        ds = gdal.GetDriverByName("ISIS3").CreateCopy(
            "/vsimem/out.lbl", mem_ds, callback=cancel_cbk
        )
    assert ds is None
    # Delete would fail since ds is None
    gdal.Unlink("/vsimem/out.lbl")


# Test CreateCopy() and scale and offset


def test_isis_25():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    mem_ds.GetRasterBand(1).SetScale(10)
    mem_ds.GetRasterBand(1).SetOffset(20)
    gdal.GetDriverByName("ISIS3").CreateCopy("/vsimem/out.lbl", mem_ds)
    ds = gdal.Open("/vsimem/out.lbl")
    assert ds.GetRasterBand(1).GetScale() == 10
    assert ds.GetRasterBand(1).GetOffset() == 20
    ds = None
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/out.lbl")


# Test objects with same name


def test_isis_26():
    gdal.FileFromMemBuffer(
        "/vsimem/in.lbl",
        """Object = IsisCube
  Object = Core
    StartByte = 1
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object

Object = Table
  Name = first_table
End_Object

Object = Table
  Name = second_table
End_Object

Object = foo
  x = A
End_Object

Object = foo
  x = B
End_Object

Object = foo
  x = C
End_Object

End""",
    )

    gdal.Translate("/vsimem/out.lbl", "/vsimem/in.lbl", format="ISIS3")

    f = gdal.VSIFOpenL("/vsimem/out.lbl", "rb")
    content = gdal.VSIFReadL(1, 10000, f).decode("ASCII")
    gdal.VSIFCloseL(f)

    assert """Object = Table
  Name = first_table
End_Object

Object = Table
  Name = second_table
End_Object

Object = foo
  x = A
End_Object

Object = foo
  x = B
End_Object

Object = foo
  x = C
End_Object
""" in content

    gdal.Unlink("/vsimem/in.lbl")
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/out.lbl")


# Test history


@pytest.mark.parametrize("src_location", ["LABEL", "EXTERNAL"])
@pytest.mark.parametrize("dst_location", ["LABEL", "EXTERNAL"])
def test_isis_27(tmp_vsimem, src_location, dst_location):

    gdal.GetDriverByName("ISIS3").Create(
        tmp_vsimem / "out.lbl", 1, 1, options=["DATA_LOCATION=" + src_location]
    )
    gdal.Translate(
        tmp_vsimem / "out2.lbl",
        tmp_vsimem / "out.lbl",
        format="ISIS3",
        creationOptions=["DATA_LOCATION=" + dst_location],
    )

    f = gdal.VSIFOpenL(tmp_vsimem / "out2.lbl", "rb")
    content = None
    if f is not None:
        content = gdal.VSIFReadL(1, 100000, f).decode("ASCII")
        gdal.VSIFCloseL(f)

    ds = gdal.Open(tmp_vsimem / "out2.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    lbl = json.loads(lbl)
    offset = lbl["History_IsisCube"]["StartByte"] - 1
    size = lbl["History_IsisCube"]["Bytes"]

    if dst_location == "EXTERNAL":
        assert lbl["Label"]["Bytes"] < 65536

        history_filename = lbl["History_IsisCube"]["^History"]
        if history_filename != "out2.History.IsisCube":
            print(src_location)
            print(dst_location)
            pytest.fail(content)

        f = gdal.VSIFOpenL(tmp_vsimem / history_filename, "rb")
        history = None
        if f is not None:
            history = gdal.VSIFReadL(1, 100000, f).decode("ASCII")
            gdal.VSIFCloseL(f)

        if offset != 0 or size != len(history):
            print(src_location)
            print(dst_location)
            pytest.fail(content)
    else:
        assert lbl["Label"]["Bytes"] >= 65536
        if offset + size != len(content):
            print(src_location, offset + size, len(content))
            pytest.fail(dst_location)
        history = content[offset:]

    if (
        not history.startswith("Object = ")
        or "FROM = out.lbl" not in history
        or "TO   = out2.lbl" not in history
        or "TO = out.lbl" not in history
    ):
        print(src_location)
        print(dst_location)
        pytest.fail(content)


def test_isis_27_bis():

    # Test GDAL_HISTORY
    gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/out.lbl", 1, 1, options=["GDAL_HISTORY=foo"]
    )
    f = gdal.VSIFOpenL("/vsimem/out.lbl", "rb")
    content = None
    if f is not None:
        content = gdal.VSIFReadL(1, 100000, f).decode("ASCII")
        gdal.VSIFCloseL(f)
    assert "foo" in content
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/out.lbl")


# Test preservation of non-pixel sections


def test_isis_28():

    gdal.FileFromMemBuffer("/vsimem/in_table", "FOO")
    gdal.FileFromMemBuffer(
        "/vsimem/in.lbl",
        """Object = IsisCube
  Object = Core
    StartByte = 1
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object

Object = Table
  Name = first_table
  StartByte = 1
  Bytes = 3
  ^Table = in_table
End_Object
End""",
    )

    ds = gdal.Open("/vsimem/in.lbl")
    fl = ds.GetFileList()
    if fl != ["/vsimem/in.lbl", "/vsimem/in_table"]:
        print(fl)
        return
    ds = None

    gdal.Translate("/vsimem/in_label.lbl", "/vsimem/in.lbl", format="ISIS3")

    for src_location in ["LABEL", "EXTERNAL"]:
        if src_location == "LABEL":
            src = "/vsimem/in_label.lbl"
        else:
            src = "/vsimem/in.lbl"
        for dst_location in ["LABEL", "EXTERNAL"]:
            gdal.Translate(
                "/vsimem/out.lbl",
                src,
                format="ISIS3",
                creationOptions=["DATA_LOCATION=" + dst_location],
            )
            f = gdal.VSIFOpenL("/vsimem/out.lbl", "rb")
            content = None
            if f is not None:
                content = gdal.VSIFReadL(1, 100000, f).decode("ASCII")
                gdal.VSIFCloseL(f)

            ds = gdal.Open("/vsimem/out.lbl")
            lbl = ds.GetMetadata_List("json:ISIS3")[0]
            lbl = json.loads(lbl)
            offset = lbl["Table_first_table"]["StartByte"] - 1
            size = lbl["Table_first_table"]["Bytes"]

            if dst_location == "EXTERNAL":
                table_filename = lbl["Table_first_table"]["^Table"]
                if table_filename != "out.Table.first_table":
                    print(src_location)
                    print(dst_location)
                    pytest.fail(content)

                f = gdal.VSIFOpenL("/vsimem/" + table_filename, "rb")
                table = None
                if f is not None:
                    table = gdal.VSIFReadL(1, 100000, f).decode("ASCII")
                    gdal.VSIFCloseL(f)

                if offset != 0 or size != 3 or size != len(table):
                    print(src_location)
                    print(dst_location)
                    pytest.fail(content)
            else:
                if offset + size != len(content):
                    print(src_location)
                    pytest.fail(dst_location)
                table = content[offset:]

            if table != "FOO":
                print(src_location)
                print(dst_location)
                pytest.fail(content)

            gdal.GetDriverByName("ISIS3").Delete("/vsimem/out.lbl")

    gdal.GetDriverByName("ISIS3").Delete("/vsimem/in_label.lbl")
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/in.lbl")


# Test complete removal of history


def test_isis_29():

    with gdal.quiet_errors():
        gdal.Translate("/vsimem/in.lbl", "data/byte.tif", format="ISIS3")

    gdal.Translate(
        "/vsimem/out.lbl",
        "/vsimem/in.lbl",
        options="-of ISIS3 -co USE_SRC_HISTORY=NO -co ADD_GDAL_HISTORY=NO",
    )

    ds = gdal.Open("/vsimem/out.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    lbl = json.loads(lbl)
    assert "History" not in lbl
    ds = None

    gdal.GetDriverByName("ISIS3").Delete("/vsimem/out.lbl")

    gdal.Translate(
        "/vsimem/out.lbl",
        "/vsimem/in.lbl",
        options="-of ISIS3 -co USE_SRC_HISTORY=NO -co ADD_GDAL_HISTORY=NO -co DATA_LOCATION=EXTERNAL",
    )

    ds = gdal.Open("/vsimem/out.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    lbl = json.loads(lbl)
    assert "History" not in lbl
    ds = None
    assert gdal.VSIStatL("/vsimem/out.History.IsisCube") is None

    gdal.GetDriverByName("ISIS3").Delete("/vsimem/out.lbl")
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/in.lbl")


# Test Fill() on a GeoTIFF file


def test_isis_30():

    ds = gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/test.lbl", 1, 1, options=["DATA_LOCATION=GEOTIFF"]
    )
    ds.GetRasterBand(1).Fill(1)
    ds = None

    ds = gdal.Open("/vsimem/test.lbl")
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.GetDriverByName("ISIS3").Delete("/vsimem/test.lbl")
    assert cs == 1


# Test correct working of block caching with a GeoTIFF file


def test_isis_31():

    with gdal.config_option("GDAL_FORCE_CACHING", "YES"):
        ds = gdal.GetDriverByName("ISIS3").Create(
            "/vsimem/test.lbl", 1, 1, options=["DATA_LOCATION=GEOTIFF"]
        )
        ds.WriteRaster(0, 0, 1, 1, struct.pack("B" * 1, 1))
        ds = None

    ds = gdal.Open("/vsimem/test.lbl")
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.GetDriverByName("ISIS3").Delete("/vsimem/test.lbl")
    assert cs == 1


###############################################################################
def test_isis3_write_utm():

    src_ds = gdal.Open("data/byte.tif")
    with gdal.quiet_errors():
        gdal.GetDriverByName("ISIS3").CreateCopy(
            "/vsimem/temp.lbl", src_ds, options=["DATA_LOCATION=EXTERNAL"]
        )
    ds = gdal.Open("/vsimem/temp.lbl")
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    f = gdal.VSIFOpenL("/vsimem/temp.lbl", "rb")
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
        gdal.VSIFCloseL(f)
    assert "MinimumLongitude   = -117.6411686" in data, data
    assert "MaximumLongitude   = -117.6281108" in data, data
    assert "MaximumLatitude    = 33.90241956" in data, data
    assert "MinimumLatitude    = 33.891530168" in data, data
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/temp.lbl")


###############################################################################
# Test bugfix https://github.com/OSGeo/gdal/issues/1510


def test_isis3_parse_list_and_write_quote_string_in_list():

    src_ds = gdal.Open("data/isis3/FC21B0037339_15142232818F1C_3bands_truncated.cub")
    gdal.GetDriverByName("ISIS3").CreateCopy(
        "/vsimem/temp.lbl", src_ds, options=["DATA_LOCATION=EXTERNAL"]
    )
    f = gdal.VSIFOpenL("/vsimem/temp.lbl", "rb")
    if f:
        data = gdal.VSIFReadL(1, 10000, f).decode("ascii")
        gdal.VSIFCloseL(f)
    assert "FilterNumber = (1, 1, 1)" in data, data
    assert "FilterName   = (Clear_F1, Clear_F1, Clear_F1)" in data, data
    assert 'Name         = ("band 1", "band 2", "band 3")' in data, data
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/temp.lbl")


###############################################################################
#


def test_isis3_bandbin_single_band():

    gdal.FileFromMemBuffer(
        "/vsimem/test.lbl",
        """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = Real
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object

  Group = BandBin
    FilterName   = "ignored"
    Name         = "My band name"
    Center       = 700 <NANOMETERS>
    Width        = 300 <NANOMETERS>
  End_Group

End_Object
End""",
    )

    ds = gdal.Open("/vsimem/test.lbl")
    assert ds
    band = ds.GetRasterBand(1)
    assert band.GetDescription() == "My band name"
    assert band.GetMetadata() == {
        "BANDWIDTH": "300.000000",
        "BANDWIDTH_UNIT": "NANOMETERS",
        "WAVELENGTH": "700.000000",
        "WAVELENGTH_UNIT": "NANOMETERS",
    }
    ds = None
    gdal.Unlink("/vsimem/test.lbl")


###############################################################################
#


def test_isis3_bandbin_multiple_bands():

    gdal.FileFromMemBuffer(
        "/vsimem/test.lbl",
        """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 2
    End_Group
    Group = Pixels
      Type       = Real
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object

  Group = BandBin
    BandSuffixName   = ("first band", "second band")
    BandSuffixUnit   = (DEGREE, DEGREE)
    BandBinCenter    = (1.0348, 1.3128)
    BandBinUnit      = MICROMETER
    Width            = (0.5, 0.6) <um>
  End_Group

End_Object
End""",
    )

    ds = gdal.Open("/vsimem/test.lbl")
    assert ds
    band = ds.GetRasterBand(1)
    assert band.GetDescription() == "first band"
    assert band.GetUnitType() == "DEGREE"
    assert band.GetMetadata() == {
        "BANDWIDTH": "0.500000",
        "BANDWIDTH_UNIT": "um",
        "WAVELENGTH": "1.034800",
        "WAVELENGTH_UNIT": "MICROMETER",
    }
    band = ds.GetRasterBand(2)
    assert band.GetDescription() == "second band"
    assert band.GetMetadata() == {
        "BANDWIDTH": "0.600000",
        "BANDWIDTH_UNIT": "um",
        "WAVELENGTH": "1.312800",
        "WAVELENGTH_UNIT": "MICROMETER",
    }
    ds = None
    gdal.Unlink("/vsimem/test.lbl")


###############################################################################
# Test that when converting from ISIS3 to other formats (PAM-enabled), the
# json:ISIS3 metadata domain is preserved.


def test_isis3_preserve_label_across_format():

    gdal.FileFromMemBuffer(
        "/vsimem/multiband.lbl",
        """Object = IsisCube
  Object = Core
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 2
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object

  Group = BandBin
    BandSuffixName   = ("first band", "second band")
    BandSuffixUnit   = (DEGREE, DEGREE)
    BandBinCenter    = (1.0, 2.0)
    BandBinUnit      = MICROMETER
    Width            = (0.5, 1.0) <um>
  End_Group

End_Object
End""",
    )
    src_ds = gdal.Open("/vsimem/multiband.lbl")

    # Copy ISIS3 to GeoTIFF
    gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/out.tif", src_ds)

    # Check GeoTIFF
    ds = gdal.Open("/vsimem/out.tif")
    assert len(ds.GetMetadataDomainList()) == 3
    assert set(ds.GetMetadataDomainList()) == set(
        ["IMAGE_STRUCTURE", "json:ISIS3", "DERIVED_SUBDATASETS"]
    )
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert lbl
    ds = None
    assert gdal.VSIStatL("/vsimem/out.tif.aux.xml") is None

    # Copy back from GeoTIFF to ISIS3
    src_ds_gtiff = gdal.Open("/vsimem/out.tif")
    gdal.GetDriverByName("ISIS3").CreateCopy("/vsimem/out.cub", src_ds_gtiff)
    assert not gdal.VSIStatL("/vsimem/out.cub.aux.xml")
    ds = gdal.Open("/vsimem/out.cub")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    # Check label preservation
    assert "BandBin" in lbl
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/out.tif")
    gdal.GetDriverByName("ISIS3").Delete("/vsimem/out.cub")

    # Copy ISIS3 to PDS4
    with gdal.quiet_errors():
        gdal.GetDriverByName("PDS4").CreateCopy("/vsimem/out.xml", src_ds)
    ds = gdal.Open("/vsimem/out.xml")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert lbl
    ds = None
    assert gdal.VSIStatL("/vsimem/out.xml.aux.xml")
    gdal.GetDriverByName("PDS4").Delete("/vsimem/out.xml")

    # Copy ISIS3 to PNG
    png_drv = gdal.GetDriverByName("PNG")
    if png_drv:
        png_drv.CreateCopy("/vsimem/out.png", src_ds)
        ds = gdal.Open("/vsimem/out.png")
        lbl = ds.GetMetadata_List("json:ISIS3")[0]
        assert lbl
        ds = None
        assert gdal.VSIStatL("/vsimem/out.png.aux.xml")
        png_drv.Delete("/vsimem/out.png")
    else:
        print("PNG driver missing")

    # Check GeoTIFF with non pure copy mode (test gdal_translate_lib)
    gdal.Translate("/vsimem/out.tif", src_ds, options="-mo FOO=BAR")
    ds = gdal.Open("/vsimem/out.tif")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert lbl
    ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/out.tif")

    # Test converting a subset of bands
    gdal.Translate("/vsimem/out.tif", src_ds, options="-b 2 -mo FOO=BAR")
    ds = gdal.Open("/vsimem/out.tif")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    lbl = json.loads(lbl)

    assert lbl["IsisCube"]["BandBin"] == json.loads("""{
      "_type":"group",
      "BandBinUnit":"MICROMETER",
      "Width":{
        "unit":"um",
        "value":[
          1.000000
        ]
      },
      "BandSuffixName":[
        "second band"
      ],
      "BandSuffixUnit":[
        "DEGREE"
      ],
      "BandBinCenter":[
        2.000000
      ]
    }""")

    assert "OriginalBandBin" in lbl["IsisCube"]
    assert lbl["IsisCube"]["OriginalBandBin"] == json.loads("""{
      "_type":"group",
      "BandSuffixName":[
        "first band",
        "second band"
      ],
      "BandSuffixUnit":[
        "DEGREE",
        "DEGREE"
      ],
      "BandBinCenter":[
        1.000000,
        2.000000
      ],
      "BandBinUnit":"MICROMETER",
      "Width":{
        "value":[
          0.500000,
          1.000000
        ],
        "unit":"um"
      }
    }""")

    ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/out.tif")

    src_ds = None
    gdal.Unlink("/vsimem/multiband.lbl")


def test_isis3_point_perspective_read():

    ds = gdal.Open("data/isis3/isis3_pointperspective.cub")
    assert (
        ds.GetSpatialRef().ExportToProj4()
        == "+proj=nsper +lat_0=-10 +lon_0=-90 +h=31603810 +x_0=0 +y_0=0 +R=3396190 +units=m +no_defs"
    )


@pytest.mark.require_proj(7)
def test_isis3_point_perspective_write():

    sr = osr.SpatialReference()
    sr.SetGeogCS("GEOG_NAME", "D_DATUM_NAME", "", 3000000, 0)
    sr.SetVerticalPerspective(1, 2, 0, 1000, 0, 0)
    ds = gdal.GetDriverByName("ISIS3").Create("/vsimem/isis_tmp.lbl", 1, 1)
    ds.SetSpatialRef(sr)
    ds.SetGeoTransform([-10, 1, 0, 40, 0, -1])
    ds = None
    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert '"CenterLatitude":1.0' in lbl
    assert '"CenterLongitude":2.0' in lbl
    assert '"Distance":3001.0' in lbl
    ds = None

    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


def test_isis3_oblique_cylindrical_read():

    ds = gdal.Open("data/isis3/isis3_obliquecylindrical.cub")
    srs = ds.GetSpatialRef()
    assert (
        srs.ExportToProj4()
        == "+proj=ob_tran +o_proj=eqc +o_lon_p=-90 +o_lat_p=180 +lon_0=0 +R=3396190 +units=m +no_defs"
    )

    pixel = ds.RasterXSize / 2.0
    line = ds.RasterYSize / 2.0
    gt = ds.GetGeoTransform()
    x = gt[0] + pixel * gt[1] + line * gt[2]
    y = gt[3] + pixel * gt[4] + line * gt[5]
    geog_srs = srs.CloneGeogCS()
    ct = osr.CoordinateTransformation(srs, geog_srs)
    lon, lat, _ = ct.TransformPoint(x, y)
    assert lon == pytest.approx(90.0)
    assert lat == pytest.approx(-45.0, 1e-2)


def test_isis3_oblique_cylindrical_write():

    src_ds = gdal.Open("data/isis3/isis3_obliquecylindrical.cub")
    ds = gdal.GetDriverByName("ISIS3").Create(
        "/vsimem/isis_tmp.lbl", src_ds.RasterXSize, src_ds.RasterYSize
    )
    ds.SetSpatialRef(src_ds.GetSpatialRef())
    ds.SetGeoTransform(src_ds.GetGeoTransform())
    src_ds = None
    ds = None

    ds = gdal.Open("/vsimem/isis_tmp.lbl")
    lbl = ds.GetMetadata_List("json:ISIS3")[0]
    assert '"PoleLongitude":0.0' in lbl
    assert '"PoleLatitude":0.0' in lbl
    assert '"PoleRotation":90.0' in lbl
    ds = None

    gdal.GetDriverByName("ISIS3").Delete("/vsimem/isis_tmp.lbl")


def test_isis_read_data(tmp_vsimem):
    gdal.FileFromMemBuffer(
        tmp_vsimem / "in.lbl",
        """Object = IsisCube
  Object = Core
    StartByte = 1
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object
End_Object

Object = Table
  Name = FirstTable
  StartByte = 1001
  Bytes = 10
End_Object

Object = Table
  Name = SecondTable
  StartByte = 1
  Bytes = 5
  ^Table = in.bin
End_Object

Object = Table
  Name = ThirdTable
  StartByte = 1
  Bytes = 5
  ^Table = not_existing.bin
End_Object

End
""",
    )

    gdal.FileFromMemBuffer(tmp_vsimem / "in.bin", b"\x01\x02\x03\x04\x05")

    f = gdal.VSIFOpenL(tmp_vsimem / "in.lbl", "rb+")
    assert f
    gdal.VSIFTruncateL(f, 1000)
    gdal.VSIFSeekL(f, 1000, 0)
    gdal.VSIFWriteL(b"\x01\x02\x03\x04\x05\x01\x02\x03\x04\x05", 10, 1, f)
    gdal.VSIFCloseL(f)

    with gdal.Open(tmp_vsimem / "in.lbl") as ds:
        got = json.loads(ds.GetMetadata("json:ISIS3")[0])
    assert "_data" in got["Table_FirstTable"]
    del got["_filename"]
    expected = {
        "IsisCube": {
            "Core": {
                "Dimensions": {
                    "Bands": 1,
                    "Lines": 1,
                    "Samples": 1,
                    "_type": "group",
                },
                "Format": "BandSequential",
                "Pixels": {
                    "Base": 0.0,
                    "ByteOrder": "Lsb",
                    "Multiplier": 1.0,
                    "Type": "UnsignedByte",
                    "_type": "group",
                },
                "StartByte": 1,
                "_type": "object",
            },
            "_type": "object",
        },
        "Table_FirstTable": {
            "Bytes": 10,
            "Name": "FirstTable",
            "StartByte": 1001,
            "_container_name": "Table",
            "_data": "01020304050102030405",
            "_type": "object",
        },
        "Table_SecondTable": {
            "Bytes": 5,
            "Name": "SecondTable",
            "StartByte": 1,
            "_container_name": "Table",
            "^Table": "in.bin",
            "_data": "0102030405",
            "_type": "object",
        },
        "Table_ThirdTable": {
            "Bytes": 5,
            "Name": "ThirdTable",
            "StartByte": 1,
            "^Table": "not_existing.bin",
            "_container_name": "Table",
            "_type": "object",
        },
    }
    assert got == expected

    with gdal.OpenEx(
        tmp_vsimem / "in.lbl", open_options=["INCLUDE_OFFLINE_CONTENT=NO"]
    ) as ds:
        got = json.loads(ds.GetMetadata("json:ISIS3")[0])
    assert "_data" not in got["Table_FirstTable"]

    with gdal.OpenEx(
        tmp_vsimem / "in.lbl", open_options=["MAX_SIZE_OFFLINE_CONTENT=5"]
    ) as ds:
        with gdaltest.error_raised(
            gdal.CE_Warning, match="Too large content reference by Table_FirstTable"
        ):
            got = json.loads(ds.GetMetadata("json:ISIS3")[0])
    assert "_data" not in got["Table_FirstTable"]
    assert "_data" in got["Table_SecondTable"]

    with gdal.Open(tmp_vsimem / "in.lbl") as ds:
        with gdaltest.error_raised(
            gdal.CE_Warning,
            match="which does not exist. Removing this section from the label",
        ):
            gdal.GetDriverByName("ISIS3").CreateCopy(tmp_vsimem / "out.lbl", ds)
    with gdal.VSIFile(tmp_vsimem / "out.lbl", "rb") as f:
        data = f.read()
        assert b"_data" not in data


def test_isis3_gdal_translate(tmp_vsimem):

    isis_filename = tmp_vsimem / "in.lbl"
    with gdal.quiet_errors():
        gdal.Translate(isis_filename, "data/byte.tif", format="ISIS3")

    gtiff_filename = tmp_vsimem / "out.tif"

    gdal.Translate(
        gtiff_filename,
        isis_filename,
        srcWin=[1, 2, 3, 4],
        width=5,
        scaleParams=[[5, 6, 7, 8]],
    )
    with gdal.Open(gtiff_filename) as ds:
        j = json.loads(ds.GetMetadata("json:ISIS3")[0])
        assert "GDALHistory" in j
        j = j["GDALHistory"]
        if "Program" in j:
            del j["Program"]
        if "ProgramPath" in j:
            del j["ProgramPath"]
        expected_j = {
            "Comment": "Part of that metadata might be invalid due to a clipping operation, a "
            "resolution change operation and a scaling operation having been performed "
            "by GDAL tools",
            "GdalVersion": gdal.VersionInfo("RELEASE_NAME"),
            "ProgramArguments": "-outsize 5 0 -srcwin 1 2 3 4 -scale 5 6 7 8",
            "_type": "object",
        }
        assert j == expected_j


def test_isis3_gdalwarp(tmp_vsimem):

    isis_filename = tmp_vsimem / "in.lbl"
    with gdal.quiet_errors():
        gdal.Translate(isis_filename, "data/byte.tif", format="ISIS3")

    gtiff_filename = tmp_vsimem / "out.tif"

    gdal.Warp(
        gtiff_filename,
        isis_filename,
        dstSRS="EPSG:4326",
    )
    with gdal.Open(gtiff_filename) as ds:
        j = json.loads(ds.GetMetadata("json:ISIS3")[0])
        assert "GDALHistory" in j
        j = j["GDALHistory"]
        if "Program" in j:
            del j["Program"]
        if "ProgramPath" in j:
            del j["ProgramPath"]
        expected_j = {
            "Comment": "Part of that metadata might be invalid due to a reprojection operation having been performed "
            "by GDAL tools",
            "GdalVersion": gdal.VersionInfo("RELEASE_NAME"),
            "ProgramArguments": "-t_srs EPSG:4326",
            "_type": "object",
        }
        assert j == expected_j


def test_isis_unit_in_array(tmp_vsimem):
    gdal.FileFromMemBuffer(
        tmp_vsimem / "in.lbl",
        """Object = IsisCube
  Object = Core
    StartByte = 1
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object

  Group = Test
    TestMultiValue   = (2 <m>, "Hello World", 3.5 <r>, "This is not suffixed by <unit>")
  End_Group

End_Object

End
""",
    )

    gdal.FileFromMemBuffer(tmp_vsimem / "in.bin", b"\x01")

    with gdal.Open(tmp_vsimem / "in.lbl") as ds:
        j = json.loads(ds.GetMetadata("json:ISIS3")[0])
        assert j["IsisCube"]["Test"]["TestMultiValue"] == [
            {"value": 2, "unit": "m"},
            "Hello World",
            {"value": 3.5, "unit": "r"},
            "This is not suffixed by <unit>",
        ]

    gdal.Translate(tmp_vsimem / "out.lbl", tmp_vsimem / "in.lbl", format="ISIS3")

    with gdal.VSIFile(tmp_vsimem / "out.lbl", "rb") as f:
        data = f.read()
    assert (
        b'TestMultiValue = (2 <m>, "Hello World", 3.5 <r>, "This is not suffixed by <unit>")'
        in data
    )


def test_isis_duplicated_keyword(tmp_vsimem):
    gdal.FileFromMemBuffer(
        tmp_vsimem / "in.lbl",
        """Object = IsisCube
  Object = Core
    StartByte = 1
    Format = BandSequential
    Group = Dimensions
      Samples = 1
      Lines   = 1
      Bands   = 1
    End_Group
    Group = Pixels
      Type       = UnsignedByte
      ByteOrder  = Lsb
      Base       = 0.0
      Multiplier = 1.0
    End_Group
  End_Object

  Group = Test
    TestRepeated1 = 1
    TestRepeated1 = 2
    TestRepeated1 = 3
    TestRepeated2 = 1 <m>
    TestRepeated2 = 2 <cm>
    TestRepeated3 = (1, 2)
    TestRepeated3 = 3
    TestRepeated4 = (1 <m>, 2)
    TestRepeated4 = 3
  End_Group

End_Object

End
""",
    )

    gdal.FileFromMemBuffer(tmp_vsimem / "in.bin", b"\x01")

    with gdal.Open(tmp_vsimem / "in.lbl") as ds:
        j = json.loads(ds.GetMetadata("json:ISIS3")[0])
        assert j["IsisCube"]["Test"]["TestRepeated1"] == {
            "values": [1, 2, 3],
        }
        assert j["IsisCube"]["Test"]["TestRepeated2"] == {
            "values": [{"value": 1, "unit": "m"}, {"value": 2, "unit": "cm"}],
        }
        assert j["IsisCube"]["Test"]["TestRepeated3"] == {
            "values": [[1, 2], 3],
        }
        assert j["IsisCube"]["Test"]["TestRepeated4"] == {
            "values": [[{"value": 1, "unit": "m"}, 2], 3],
        }

    gdal.Translate(tmp_vsimem / "out.lbl", tmp_vsimem / "in.lbl", format="ISIS3")

    with gdal.VSIFile(tmp_vsimem / "out.lbl", "rb") as f:
        data = f.read()
    assert b"TestRepeated1 = 1\n    TestRepeated1 = 2\n" in data
    assert b"TestRepeated2 = 1 <m>\n    TestRepeated2 = 2 <cm>\n" in data
    assert b"TestRepeated3 = (1, 2)\n    TestRepeated3 = 3\n" in data
    assert b"TestRepeated4 = (1 <m>, 2)\n    TestRepeated4 = 3\n" in data
