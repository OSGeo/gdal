#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalwarp
# Author:   Faza Mahamood <fazamhd @ gmail dot com>
#
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
# Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import collections
import json
import shutil
import struct

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

###############################################################################
# Simple test


def test_gdalwarp_lib_1(tmp_path):

    ds1 = gdal.Open("../gcore/data/byte.tif")
    dstDS = gdal.Warp(tmp_path / "testgdalwarp1.tif", ds1)

    assert dstDS.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    dstDS = None


###############################################################################
# Test -of option


def test_gdalwarp_lib_2(tmp_path):

    ds1 = gdal.Open("../gcore/data/byte.tif")
    dstDS = gdal.Warp(
        tmp_path / "testgdalwarp2.tif".encode("ascii").decode("ascii"),
        [ds1],
        format="GTiff",
    )

    assert dstDS.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    dstDS = None


###############################################################################
# Test -ot option


def test_gdalwarp_lib_3():

    ds1 = gdal.Open("../gcore/data/byte.tif")
    dstDS = gdal.Warp("", ds1, format="MEM", outputType=gdal.GDT_Int16)

    assert dstDS.GetRasterBand(1).DataType == gdal.GDT_Int16, "Bad data type"

    assert dstDS.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    dstDS = None


###############################################################################
# Test -t_srs option


def test_gdalwarp_lib_4():

    ds1 = gdal.Open("../gcore/data/byte.tif")
    dstDS = gdal.Warp("", ds1, format="MEM", dstSRS="EPSG:32611")

    assert dstDS.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    dstDS = None


###############################################################################
# Test warping from GCPs without any explicit option


@pytest.fixture(scope="module")
def testgdalwarp_gcp_tif(tmp_path_factory):

    testgdalwarp_gcp_tif_fname = tmp_path_factory.mktemp("tmp") / "testgdalwarp_gcp.tif"

    ds = gdal.Open("../gcore/data/byte.tif")
    gcpList = [
        gdal.GCP(440720.000, 3751320.000, 0, 0, 0),
        gdal.GCP(441920.000, 3751320.000, 0, 20, 0),
        gdal.GCP(441920.000, 3750120.000, 0, 20, 20),
        gdal.GCP(440720.000, 3750120.000, 0, 0, 20),
    ]
    ds1 = gdal.Translate(
        testgdalwarp_gcp_tif_fname, ds, outputSRS="EPSG:26711", GCPs=gcpList
    )
    del ds1

    yield testgdalwarp_gcp_tif_fname


def test_gdalwarp_lib_5(testgdalwarp_gcp_tif):

    ds = gdal.Open("../gcore/data/byte.tif")

    ds1 = gdal.Open(testgdalwarp_gcp_tif)

    dstDS = gdal.Warp("", ds1, format="MEM", tps=True)

    assert dstDS.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(ds.GetGeoTransform(), dstDS.GetGeoTransform(), 1e-9)

    dstDS = None


###############################################################################
# Test warping from GCPs with -tps


def test_gdalwarp_lib_6(testgdalwarp_gcp_tif):

    ds1 = gdal.Open(testgdalwarp_gcp_tif)
    dstDS = gdal.Warp("", ds1, format="MEM", tps=True)

    assert dstDS.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        dstDS.GetGeoTransform(),
        1e-9,
    )

    dstDS = None


###############################################################################
# Test warping from GCPs with SRC_METHOD=GCP_POLYNOMIAL and MAX_GCP_ORDER=-1


def test_gdalwarp_lib_6_bis(testgdalwarp_gcp_tif):

    ds1 = gdal.Open(testgdalwarp_gcp_tif)
    dstDS = gdal.Warp(
        "",
        ds1,
        format="MEM",
        transformerOptions=["SRC_METHOD=GCP_POLYNOMIAL", "MAX_GCP_ORDER=-1"],
    )

    assert dstDS.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        dstDS.GetGeoTransform(),
        1e-9,
    )

    dstDS = None


###############################################################################
# Test -tr


def test_gdalwarp_lib_7(testgdalwarp_gcp_tif):

    ds1 = gdal.Open(testgdalwarp_gcp_tif)
    dstDS = gdal.Warp("", [ds1], format="MEM", xRes=120, yRes=120)
    assert dstDS is not None

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    gdaltest.check_geotransform(expected_gt, dstDS.GetGeoTransform(), 1e-9)

    dstDS = None


###############################################################################
# Test -ts


def test_gdalwarp_lib_8(testgdalwarp_gcp_tif):

    ds1 = gdal.Open(testgdalwarp_gcp_tif)
    dstDS = gdal.Warp("", [ds1], format="MEM", width=10, height=10)
    assert dstDS is not None

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    gdaltest.check_geotransform(expected_gt, dstDS.GetGeoTransform(), 1e-9)

    dstDS = None


###############################################################################
# Test -te


def test_gdalwarp_lib_9():

    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        outputBounds=[440720.000, 3750120.000, 441920.000, 3751320.000],
    )

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test -rn


def test_gdalwarp_lib_10():

    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        width=40,
        height=40,
        resampleAlg=gdal.GRA_NearestNeighbour,
    )

    assert ds.GetRasterBand(1).Checksum() == 18784, "Bad checksum"

    ds = None


###############################################################################
# Test -rb


def test_gdalwarp_lib_11():

    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        width=40,
        height=40,
        resampleAlg=gdal.GRA_Bilinear,
    )

    ref_ds = gdal.Open("ref_data/testgdalwarp11.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        pytest.fail("Image too different from reference")

    ref_ds = None
    ds = None


###############################################################################
# Test -rc


def test_gdalwarp_lib_12():

    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        width=40,
        height=40,
        resampleAlg=gdal.GRA_Cubic,
    )

    ref_ds = gdal.Open("ref_data/testgdalwarp12.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        pytest.fail("Image too different from reference")

    ref_ds = None
    ds = None


###############################################################################
# Test -rcs


def test_gdalwarp_lib_13():

    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        width=40,
        height=40,
        resampleAlg=gdal.GRA_CubicSpline,
    )

    ref_ds = gdal.Open("ref_data/testgdalwarp13.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        pytest.fail("Image too different from reference")

    ref_ds = None
    ds = None


###############################################################################
# Test -r lanczos


def test_gdalwarp_lib_14():

    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        width=40,
        height=40,
        resampleAlg=gdal.GRA_Lanczos,
    )

    ref_ds = gdal.Open("ref_data/testgdalwarp14.tif")
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        pytest.fail("Image too different from reference")

    ref_ds = None
    ds = None


###############################################################################
# Test parsing all resampling methods


@pytest.mark.parametrize(
    "resampleAlg,resampleAlgStr",
    [
        (gdal.GRA_NearestNeighbour, "near"),
        (gdal.GRA_Cubic, "cubic"),
        (gdal.GRA_CubicSpline, "cubicspline"),
        (gdal.GRA_Lanczos, "lanczos"),
        (gdal.GRA_Average, "average"),
        (gdal.GRA_RMS, "rms"),
        (gdal.GRA_Mode, "mode"),
        (gdal.GRA_Max, "max"),
        (gdal.GRA_Min, "min"),
        (gdal.GRA_Med, "med"),
        (gdal.GRA_Q1, "q1"),
        (gdal.GRA_Q3, "q3"),
        (gdal.GRA_Sum, "sum"),
    ],
)
def test_gdalwarp_lib_resampling_methods(resampleAlg, resampleAlgStr):

    option_list = gdal.WarpOptions(
        resampleAlg=resampleAlg, options="__RETURN_OPTION_LIST__"
    )
    assert option_list == ["-r", resampleAlgStr]
    assert (
        gdal.Warp(
            "",
            "../gcore/data/byte.tif",
            format="MEM",
            width=2,
            height=2,
            resampleAlg=resampleAlg,
        )
        is not None
    )


###############################################################################
# Test -dstnodata


def test_gdalwarp_lib_15(testgdalwarp_gcp_tif):

    ds = gdal.Warp(
        "", testgdalwarp_gcp_tif, format="MEM", dstSRS="EPSG:32610", dstNodata=1
    )

    assert ds.GetRasterBand(1).GetNoDataValue() == 1, "Bad nodata value"

    assert ds.GetRasterBand(1).Checksum() in (4523, 4547)  # 4547 with HPGN grids

    ds = None


###############################################################################
# Test -of VRT which is a special case


def test_gdalwarp_lib_16(tmp_vsimem, testgdalwarp_gcp_tif):

    ds = gdal.Warp(
        tmp_vsimem / "test_gdalwarp_lib_16.vrt", [testgdalwarp_gcp_tif], format="VRT"
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None

    # Cannot write file
    with pytest.raises(Exception):
        gdal.Warp(
            "/i_dont/exist/test_gdalwarp_lib_16.vrt",
            testgdalwarp_gcp_tif,
            format="VRT",
        )


###############################################################################
# Test -dstalpha


def test_gdalwarp_lib_17():

    ds = gdal.Warp("", "../gcore/data/rgbsmall.tif", format="MEM", dstAlpha=True)
    assert ds is not None

    assert ds.GetRasterBand(4) is not None, "No alpha band generated"

    ds = None


###############################################################################
# Test -et 0 which is a special case


def test_gdalwarp_lib_19(testgdalwarp_gcp_tif):

    ds = gdal.Warp("", testgdalwarp_gcp_tif, format="MEM", errorThreshold=0)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test invalid value of -et


def test_gdalwarp_lib_invalid_et(testgdalwarp_gcp_tif):

    with gdaltest.enable_exceptions():
        with pytest.raises(Exception, match="Failed to parse"):
            gdal.Warp(
                "", "../gcore/data/byte.tif", format="MEM", errorThreshold="minimal"
            )


###############################################################################
# Test cutline from OGR datasource.


@pytest.mark.require_driver("CSV")
def test_gdalwarp_lib_cutline():

    ds = gdal.Warp(
        "",
        "../gcore/data/utmsmall.tif",
        format="MEM",
        cutlineDSName="data/cutline.vrt",
        cutlineLayer="cutline",
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19139, "Bad checksum"

    ds = None


###############################################################################
# Test cutline from OGR datasource with cutlineSRS


@pytest.mark.require_driver("CSV")
def test_gdalwarp_lib_cutline_with_cutline_srs():

    ds = gdal.Warp(
        "",
        "../gcore/data/utmsmall.tif",
        format="MEM",
        cutlineDSName="data/cutline.csv",
        cutlineLayer="cutline",
        cutlineSRS="EPSG:26711",
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19139, "Bad checksum"

    ds = None


###############################################################################
# Test cutline from WKT


def test_gdalwarp_lib_cutline_WKT():

    ds = gdal.Warp(
        "",
        "../gcore/data/utmsmall.tif",
        format="MEM",
        cutlineWKT="POLYGON ((445125 3748212,442222 3748212,442222 3750366,445125 3750366,445125 3748212))",
        cutlineSRS="EPSG:26711",
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19139, "Bad checksum"

    ds = None


###############################################################################
# Test cutline with sourceCRS != targetCRS and targetCRS == cutlineCRS


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("GPKG")
def test_gdalwarp_lib_cutline_reprojection(tmp_vsimem):

    cutline_filename = tmp_vsimem / "cutline.gpkg"

    gdal.VectorTranslate(
        cutline_filename, "data/cutline.vrt", dstSRS="EPSG:4267", reproject=True
    )

    ds = gdal.Warp(
        "",
        "../gcore/data/utmsmall.tif",
        format="MEM",
        dstSRS="EPSG:4267",
        cutlineDSName=cutline_filename,
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18883, "Bad checksum"

    ds = None


###############################################################################
# Test cutline with sourceCRS != targetCRS and targetCRS == cutlineCRS and coordinateOperation


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("GPKG")
def test_gdalwarp_lib_cutline_reprojection_and_coordinate_operation(tmp_vsimem):

    cutline_filename = tmp_vsimem / "cutline.gpkg"

    ct = "+proj=pipeline +step +proj=affine +xoff=10000 +step +inv +proj=utm +zone=11 +ellps=clrk66 +step +proj=unitconvert +xy_in=rad +xy_out=deg +step +proj=axisswap +order=2,1"
    gdal.VectorTranslate(
        cutline_filename,
        "data/cutline.vrt",
        dstSRS="EPSG:4267",
        coordinateOperation=ct,
        reproject=True,
    )

    ds = gdal.Warp(
        "",
        "../gcore/data/utmsmall.tif",
        format="MEM",
        dstSRS="EPSG:4267",
        cutlineDSName=cutline_filename,
        coordinateOperation=ct,
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18914, "Bad checksum"

    ds = None


###############################################################################
# Test cutline from PostGIS (mostly to check that we open the dataset in
# vector mode, and not in raster mode, which would cause the PostGISRaster
# driver to be used)


@pytest.mark.require_driver("PostgreSQL")
@pytest.mark.skipif(
    gdal.GetConfigOption("OGR_PG_CONNECTION_STRING", None) is None,
    reason="OGR_PG_CONNECTION_STRING not defined",
)
def test_gdalwarp_lib_cutline_postgis():

    postgis_ds_name = "PG:" + gdal.GetConfigOption("OGR_PG_CONNECTION_STRING", None)
    cutline_ds = ogr.Open(postgis_ds_name, update=1)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(26711)
    cutline_lyr = cutline_ds.CreateLayer("cutline", srs=srs)
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((400000 3000000,400000 4000000,500000 4000000,500000 3000000,400000 3000000))"
        )
    )
    cutline_lyr.CreateFeature(f)
    cutline_ds = None

    try:
        ds = gdal.Warp(
            "",
            "../gcore/data/byte.tif",
            format="MEM",
            cutlineDSName=postgis_ds_name,
            cutlineSQL="SELECT * FROM cutline",
        )
        assert ds is not None
        assert ds.GetRasterBand(1).Checksum() == 4672
    finally:
        cutline_ds = ogr.Open(postgis_ds_name, update=1)
        cutline_ds.ExecuteSQL("DELLAYER:cutline")


###############################################################################
# Test cutline whose extent is larger than the source data


@pytest.mark.parametrize(
    "options", [{}, {"GDALWARP_SKIP_CUTLINE_CONTAINMENT_TEST": "YES"}]
)
def test_gdalwarp_lib_cutline_larger_source_dataset(tmp_vsimem, options):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(26711)
    cutline_ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "cutline.shp"
    )
    cutline_lyr = cutline_ds.CreateLayer("cutline", srs=srs)
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((400000 3000000,400000 4000000,500000 4000000,500000 3000000,400000 3000000))"
        )
    )
    cutline_lyr.CreateFeature(f)
    cutline_ds = None

    with gdaltest.config_options(options):
        ds = gdal.Warp(
            "",
            "../gcore/data/byte.tif",
            format="MEM",
            cutlineDSName=tmp_vsimem / "cutline.shp",
            cutlineLayer="cutline",
        )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = None


###############################################################################
# Test cutline with ALL_TOUCHED enabled.


@pytest.mark.require_driver("CSV")
def test_gdalwarp_lib_23():

    ds = gdal.Warp(
        "",
        "../gcore/data/utmsmall.tif",
        format="MEM",
        warpOptions=["CUTLINE_ALL_TOUCHED=TRUE"],
        cutlineDSName="data/cutline.vrt",
        cutlineLayer="cutline",
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 20123, "Bad checksum"

    ds = None


###############################################################################
# Test -tap


def test_gdalwarp_lib_32():

    with pytest.raises(Exception, match="-tap option cannot be used without using -tr"):
        gdal.Warp("", "../gcore/data/byte.tif", format="MEM", targetAlignedPixels=True)

    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        targetAlignedPixels=True,
        xRes=100,
        yRes=50,
    )
    assert ds is not None

    expected_gt = (440700.0, 100.0, 0.0, 3751300.0, 0.0, -50.0)
    got_gt = ds.GetGeoTransform()
    gdaltest.check_geotransform(expected_gt, got_gt, 1e-9)

    assert (
        ds.RasterXSize == 12 and ds.RasterYSize == 24
    ), "Wrong raster dimensions : %d x %d" % (ds.RasterXSize, ds.RasterYSize)

    ds = None


###############################################################################
# Test -tap, -tr and -te


def test_gdalwarp_lib_tap_tr_te(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(
        src_filename, 10980, 10980, 1, options=["SPARSE_OK=YES"]
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([600000, 10, 0, 5700000, 0, -10])

    ds = gdal.Warp(
        "",
        src_ds,
        format="VRT",
        targetAlignedPixels=True,
        xRes=10,
        yRes=10,
        outputBounds=[599800.0, 5590200.0, 709800.0, 5700000.0],
    )
    assert ds is not None
    assert ds.GetGeoTransform() == pytest.approx(
        (599800.0, 10.0, 0.0, 5700000.0, 0.0, -10.0)
    )


###############################################################################
# Test warping multiple sources


def test_gdalwarp_lib_34():

    srcds1 = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", srcWin=[0, 0, 10, 20]
    )
    srcds2 = gdal.Translate(
        "", "../gcore/data/byte.tif", format="MEM", srcWin=[10, 0, 10, 20]
    )
    ds = gdal.Warp("", [srcds1, srcds2], format="MEM")

    cs = ds.GetRasterBand(1).Checksum()
    gt = ds.GetGeoTransform()
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize
    ds = None

    assert xsize == 20 and ysize == 20, "bad dimensions"

    assert cs == 4672, "bad checksum"

    expected_gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-5), "bad gt"


###############################################################################
# Test -te_srs


def test_gdalwarp_lib_45():

    ds = gdal.Warp(
        "",
        ["../gcore/data/byte.tif"],
        format="MEM",
        outputBounds=[
            -117.641087629972,
            33.8915301685897,
            -117.628190189534,
            33.9024195619201,
        ],
        outputBoundsSRS="EPSG:4267",
    )
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = None


###############################################################################
# Test -crop_to_cutline


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("GeoJSON")
def test_gdalwarp_lib_46(tmp_vsimem):

    ds = gdal.Warp(
        "",
        ["../gcore/data/utmsmall.tif"],
        format="MEM",
        cutlineDSName="data/cutline.vrt",
        cropToCutline=True,
    )
    assert ds.GetRasterBand(1).Checksum() == 18837, "Bad checksum"

    ds = None

    # Precisely test output raster bounds in no raster reprojection ccase

    src_ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        outputBounds=[2, 49, 3, 48],
        outputSRS="EPSG:4326",
    )

    cutlineDSName = tmp_vsimem / "test_gdalwarp_lib_46.json"
    cutline_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer("cutline")
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((2.13 48.13,2.83 48.13,2.83 48.83,2.13 48.83,2.13 48.13))"
        )
    )
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    # No CUTLINE_ALL_TOUCHED: the extent should be smaller than the cutline
    ds = gdal.Warp(
        "", src_ds, format="MEM", cutlineDSName=cutlineDSName, cropToCutline=True
    )
    got_gt = ds.GetGeoTransform()
    expected_gt = (2.15, 0.05, 0.0, 48.8, 0.0, -0.05)
    assert max([abs(got_gt[i] - expected_gt[i]) for i in range(6)]) <= 1e-8
    assert ds.RasterXSize == 13 and ds.RasterYSize == 13

    # Same but with CUTLINE_ALL_TOUCHED=YES: the extent should be larger
    # than the cutline
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        cutlineDSName=cutlineDSName,
        cropToCutline=True,
        warpOptions=["CUTLINE_ALL_TOUCHED=YES"],
    )
    got_gt = ds.GetGeoTransform()
    expected_gt = (2.1, 0.05, 0.0, 48.85, 0.0, -0.05)
    assert max([abs(got_gt[i] - expected_gt[i]) for i in range(6)]) <= 1e-8
    assert ds.RasterXSize == 15 and ds.RasterYSize == 15

    # Test numeric stability when the cutline is exactly on pixel boundaries
    cutline_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer("cutline")
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((2.15 48.15,2.85 48.15,2.85 48.85,2.15 48.85,2.15 48.15))"
        )
    )
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    for warpOptions in [[], ["CUTLINE_ALL_TOUCHED=YES"]]:
        ds = gdal.Warp(
            "",
            src_ds,
            format="MEM",
            cutlineDSName=cutlineDSName,
            cropToCutline=True,
            warpOptions=warpOptions,
        )
        got_gt = ds.GetGeoTransform()
        expected_gt = (2.15, 0.05, 0.0, 48.85, 0.0, -0.05)
        assert max([abs(got_gt[i] - expected_gt[i]) for i in range(6)]) <= 1e-8
        assert ds.RasterXSize == 14 and ds.RasterYSize == 14

    gdal.Unlink(cutlineDSName)


###############################################################################
# Test -crop_to_cutline -tr X Y -wo CUTLINE_ALL_TOUCHED=YES (fixes for #1360)


@pytest.mark.require_driver("GeoJSON")
def test_gdalwarp_lib_cutline_all_touched_single_pixel(tmp_vsimem):

    cutlineDSName = (
        tmp_vsimem / "test_gdalwarp_lib_cutline_all_touched_single_pixel.json"
    )
    cutline_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer("cutline")
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((2.15 48.15,2.15000001 48.15000001,2.15 48.15000001,2.15 48.15))"
        )
    )
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    src_ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        outputBounds=[2, 49, 3, 48],
        outputSRS="EPSG:4326",
    )

    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        cutlineDSName=cutlineDSName,
        cropToCutline=True,
        warpOptions=["CUTLINE_ALL_TOUCHED=YES"],
        xRes=0.001,
        yRes=0.001,
    )
    got_gt = ds.GetGeoTransform()
    expected_gt = (2.15, 0.001, 0.0, 48.151, 0.0, -0.001)
    assert max([abs(got_gt[i] - expected_gt[i]) for i in range(6)]) <= 1e-8, got_gt
    assert ds.RasterXSize == 1 and ds.RasterYSize == 1

    gdal.Unlink(cutlineDSName)


###############################################################################
# Test -crop_to_cutline where the geometry is very close to pixel boundaries
# (#7226)s


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("GeoJSON")
def test_gdalwarp_lib_crop_to_cutline_slightly_shifted_wrt_pixel_boundaries(tmp_vsimem):

    cutlineDSName = (
        tmp_vsimem / "test_gdalwarp_lib_crop_to_cutline_close_to_pixel_boundaries.json"
    )
    cutline_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(cutlineDSName)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(26711)
    cutline_lyr = cutline_ds.CreateLayer("cutline", srs=srs)
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON((440720.001 3751320.001,440720.001 3750120.001,441920.001 3750120.001,441920.001 3751320.001,440720.001 3751320.001))"
        )
    )
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    src_ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Warp(
        "", src_ds, format="MEM", cutlineDSName=cutlineDSName, cropToCutline=True
    )
    assert ds.RasterXSize == src_ds.RasterXSize
    assert ds.RasterYSize == src_ds.RasterYSize
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()

    src_ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        cutlineDSName=cutlineDSName,
        cropToCutline=True,
        warpOptions=["CUTLINE_ALL_TOUCHED=YES"],
    )
    assert ds.RasterXSize == src_ds.RasterXSize
    assert ds.RasterYSize == src_ds.RasterYSize
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()

    gdal.Unlink(cutlineDSName)


###############################################################################
# Test callback


def mycallback(pct, msg, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    return 1


def test_gdalwarp_lib_100():

    tab = [0]
    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        callback=mycallback,
        callback_data=tab,
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    assert tab[0] == 1.0, "Bad percentage"

    ds = None


###############################################################################
# Test with color table


def test_gdalwarp_lib_101():

    ds = gdal.Warp("", "../gdrivers/data/small_world_pct.tif", format="MEM")
    assert ds.GetRasterBand(1).GetColorTable() is not None, "Did not get color table"


###############################################################################
# Test with a dataset with no bands


def test_gdalwarp_lib_102():

    no_band_ds = gdal.GetDriverByName("MEM").Create("no band", 1, 1, 0)
    with pytest.raises(Exception):
        gdal.Warp(
            "", ["../gdrivers/data/small_world_pct.tif", no_band_ds], format="MEM"
        )


###############################################################################
# Test failed transformer


def test_gdalwarp_lib_103():

    with pytest.raises(Exception):
        gdal.Warp(
            "",
            [
                "../gdrivers/data/small_world_pct.tif",
                "../gcore/data/stefan_full_rgba.tif",
            ],
            format="MEM",
        )


###############################################################################
# Test no usable source image


def test_gdalwarp_lib_104():

    with pytest.raises(Exception):
        gdal.Warp("", [], format="MEM")


###############################################################################
# Used to be a failure in GDALSuggestedWarpOutput2 with proj 4.9.3


def test_gdalwarp_lib_105():

    gdal.Warp(
        "",
        ["../gdrivers/data/small_world_pct.tif", "../gcore/data/byte.tif"],
        format="MEM",
        dstSRS="EPSG:32645",
        width=100,
        height=100,
    )


###############################################################################
# Test failure in creation


def test_gdalwarp_lib_106():

    with pytest.raises(Exception):
        gdal.Warp(
            "/not_existing_dir/not_existing_file",
            ["../gdrivers/data/small_world_pct.tif", "../gcore/data/byte.tif"],
        )


###############################################################################
# Test forced width only


def test_gdalwarp_lib_107():

    ds = gdal.Warp("", "../gcore/data/byte.tif", format="MEM", width=20)
    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"


###############################################################################
# Test forced height only


def test_gdalwarp_lib_108():

    ds = gdal.Warp("", "../gcore/data/byte.tif", format="MEM", height=20)
    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"


###############################################################################
# Test wrong cutline name


def test_gdalwarp_lib_109():

    with pytest.raises(Exception):
        gdal.Warp(
            "", "../gcore/data/byte.tif", format="MEM", cutlineDSName="/does/not/exist"
        )


###############################################################################
# Test wrong cutline layer name


def test_gdalwarp_lib_110():

    with pytest.raises(Exception):
        gdal.Warp(
            "",
            "../gcore/data/byte.tif",
            format="MEM",
            cutlineDSName="data/cutline.vrt",
            cutlineLayer="wrong_name",
        )


###############################################################################
# Test cutline SQL


@pytest.mark.require_driver("CSV")
def test_gdalwarp_lib_111():

    ds = gdal.Warp(
        "",
        "../gcore/data/utmsmall.tif",
        format="MEM",
        cutlineDSName="data/cutline.vrt",
        cutlineSQL="SELECT * FROM cutline",
        cutlineWhere="1 = 1",
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19139, "Bad checksum"

    ds = None


###############################################################################
# Test cutline without geometry


def test_gdalwarp_lib_112(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "cutline.shp"
    )
    lyr = ds.CreateLayer("cutline")
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None
    ds = None
    with pytest.raises(Exception):
        gdal.Warp(
            "",
            "../gcore/data/utmsmall.tif",
            format="MEM",
            cutlineDSName=tmp_vsimem / "cutline.shp",
            cutlineSQL="SELECT * FROM cutline",
        )


###############################################################################
# Test cutline with non polygon geometry


def test_gdalwarp_lib_113(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "cutline.shp"
    )
    lyr = ds.CreateLayer("cutline")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(f)
    f = None
    ds = None
    with pytest.raises(Exception):
        gdal.Warp(
            "",
            "../gcore/data/utmsmall.tif",
            format="MEM",
            cutlineDSName=tmp_vsimem / "cutline.shp",
        )


###############################################################################
# Test cutline without feature


def test_gdalwarp_lib_114(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "cutline.shp"
    )
    ds.CreateLayer("cutline")
    ds = None
    with pytest.raises(Exception):
        gdal.Warp(
            "",
            "../gcore/data/utmsmall.tif",
            format="MEM",
            cutlineDSName=tmp_vsimem / "cutline.shp",
        )


###############################################################################
# Test source dataset without band


def test_gdalwarp_lib_115():

    no_band_ds = gdal.GetDriverByName("MEM").Create("no band", 1, 1, 0)
    out_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)

    with pytest.raises(Exception):
        gdal.Warp(
            out_ds, no_band_ds, cutlineDSName="data/cutline.vrt", cutlineLayer="cutline"
        )


###############################################################################
# Test failed cropToCutline due to invalid SRC_SRS


def test_gdalwarp_lib_116():

    with pytest.raises(Exception):
        gdal.Warp(
            "",
            "../gcore/data/utmsmall.tif",
            format="MEM",
            cutlineDSName="data/cutline.vrt",
            cutlineLayer="cutline",
            cropToCutline=True,
            transformerOptions=["SRC_SRS=invalid"],
        )


###############################################################################
# Test failed cropToCutline due to invalid DST_SRS


def test_gdalwarp_lib_117():

    with pytest.raises(Exception):
        gdal.Warp(
            "",
            "../gcore/data/utmsmall.tif",
            format="MEM",
            cutlineDSName="data/cutline.vrt",
            cutlineLayer="cutline",
            cropToCutline=True,
            transformerOptions=["DST_SRS=invalid"],
        )


###############################################################################
# Test failed cropToCutline due to no source raster


def test_gdalwarp_lib_118():

    with pytest.raises(Exception):
        gdal.Warp(
            "",
            [],
            format="MEM",
            cutlineDSName="data/cutline.vrt",
            cutlineLayer="cutline",
            cropToCutline=True,
        )


###############################################################################
# Test failed cropToCutline due to source raster without projection


def test_gdalwarp_lib_119():

    no_proj_ds = gdal.GetDriverByName("MEM").Create("no_proj_ds", 1, 1)
    with pytest.raises(Exception):
        gdal.Warp(
            "",
            no_proj_ds,
            format="MEM",
            cutlineDSName="data/cutline.vrt",
            cutlineLayer="cutline",
            cropToCutline=True,
        )


###############################################################################
# Test failed cropToCutline due to source raster with dummy projection


def test_gdalwarp_lib_120():

    dummy_proj_ds = gdal.GetDriverByName("MEM").Create("no_proj_ds", 1, 1)
    dummy_proj_ds.SetProjection("dummy")
    with pytest.raises(Exception):
        gdal.Warp(
            "",
            dummy_proj_ds,
            format="MEM",
            cutlineDSName="data/cutline.vrt",
            cutlineLayer="cutline",
            cropToCutline=True,
        )


###############################################################################
# Test internal wrappers


def test_gdalwarp_lib_121():

    # No option
    with pytest.raises(Exception):
        gdal.wrapper_GDALWarpDestName("", [], None)

    # Will create an implicit options structure
    with pytest.raises(Exception):
        gdal.wrapper_GDALWarpDestName("", [], None, gdal.TermProgress_nocb)

    # Null dest name
    with gdal.quiet_errors():
        with pytest.raises(Exception):
            gdal.wrapper_GDALWarpDestName(None, [], None)

    # No option
    gdal.wrapper_GDALWarpDestDS(gdal.GetDriverByName("MEM").Create("", 1, 1), [], None)

    # Will create an implicit options structure
    gdal.wrapper_GDALWarpDestDS(
        gdal.GetDriverByName("MEM").Create("", 1, 1),
        [],
        None,
        gdal.TermProgress_nocb,
    )


###############################################################################
# Test unnamed output VRT


def test_gdalwarp_lib_122():

    ds = gdal.Warp("", "../gcore/data/byte.tif", format="VRT")
    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"


###############################################################################
# Test failure during warping


def test_gdalwarp_lib_123():

    with pytest.raises(Exception):
        gdal.Warp("", "../gcore/data/byte_truncated.tif", format="MEM")


###############################################################################
# Test warping to dataset with existing nodata


def test_gdalwarp_lib_124():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.SetGeoTransform([10, 1, 0, 10, 0, -1])
    src_ds.GetRasterBand(1).SetNoDataValue(12)
    src_ds.GetRasterBand(1).Fill(12)

    out_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    out_ds.SetGeoTransform([10, 1, 0, 10, 0, -1])
    out_ds.GetRasterBand(1).SetNoDataValue(21)
    out_ds.GetRasterBand(1).Fill(21)
    expected_cs = out_ds.GetRasterBand(1).Checksum()

    gdal.Warp(out_ds, src_ds)

    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs, "Bad checksum"


###############################################################################
# Test that statistics are not propagated


def test_gdalwarp_lib_125():

    for i in range(3):

        src_ds_1 = gdal.GetDriverByName("MEM").Create("", 2, 2)
        src_ds_1.SetGeoTransform([10, 1, 0, 10, 0, -1])
        if i == 1 or i == 3:
            src_ds_1.GetRasterBand(1).SetMetadataItem("STATISTICS_MINIMUM", "5")

        src_ds_2 = gdal.GetDriverByName("MEM").Create("", 2, 2)
        src_ds_2.SetGeoTransform([10, 1, 0, 10, 0, -1])
        if i == 2 or i == 3:
            src_ds_2.GetRasterBand(1).SetMetadataItem("STATISTICS_MINIMUM", "5")

        out_ds = gdal.Warp("", [src_ds_1, src_ds_2], format="MEM")

        assert out_ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is None, i


###############################################################################
# Test cutline with invalid geometry


@pytest.mark.require_geos
def test_gdalwarp_lib_126(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "cutline.shp"
    )
    lyr = ds.CreateLayer("cutline")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POLYGON((0 0,1 1,0 1,1 0,0 0))")
    )  # Self intersecting
    lyr.CreateFeature(f)
    f = None
    ds = None
    with pytest.raises(Exception):
        gdal.Warp(
            "",
            "../gcore/data/utmsmall.tif",
            format="MEM",
            cutlineDSName=tmp_vsimem / "cutline.shp",
        )


###############################################################################
# Test -srcnodata (#6315)


def test_gdalwarp_lib_127():

    ds = gdal.Warp("", "../gcore/data/byte.tif", format="MEM", srcNodata=1)
    assert ds.GetRasterBand(1).GetNoDataValue() == 1, "bad nodata value"
    assert ds.GetRasterBand(1).Checksum() == 4672, "bad checksum"


@pytest.mark.parametrize("srcNodata", [float("-inf"), -1])
def test_gdalwarp_lib_srcnodata(srcNodata):

    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        srcNodata=srcNodata,
        outputType=gdal.GDT_Float32,
    )
    assert ds.GetRasterBand(1).GetNoDataValue() == srcNodata, "bad nodata value"
    assert ds.GetRasterBand(1).Checksum() == 4672, "bad checksum"


@pytest.mark.parametrize("dstNodata", [float("-inf"), -1])
def test_gdalwarp_lib_dstnodata(dstNodata):

    ds = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        dstNodata=dstNodata,
        outputType=gdal.GDT_Float32,
    )
    assert ds.GetRasterBand(1).GetNoDataValue() == dstNodata, "bad nodata value"
    assert ds.GetRasterBand(1).Checksum() == 4672, "bad checksum"


###############################################################################
# Test automatic densification of cutline (#6375)


@pytest.mark.require_driver("GeoJSON")
def test_gdalwarp_lib_128(tmp_vsimem):

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1177, 4719)
    rpc = [
        "HEIGHT_OFF=109",
        "LINE_NUM_COEFF=-0.001245683 -0.09427649 -1.006342 -1.954469e-05 0.001033926 2.020534e-08 -3.845472e-07 -0.002075817 0.0005520694 0 -4.642442e-06 -3.271793e-06 2.705977e-05 -7.634384e-07 -2.132832e-05 -3.248862e-05 -8.17894e-06 -3.678094e-07 2.002032e-06 3.693162e-08",
        "LONG_OFF=7.1477",
        "SAMP_DEN_COEFF=1 0.01415176 -0.003715018 -0.001205632 -0.0007738299 4.057763e-05 -1.649126e-05 0.0001453584 0.0001628194 -7.354731e-05 4.821444e-07 -4.927701e-06 -1.942371e-05 -2.817499e-06 1.946396e-06 3.04243e-06 2.362282e-07 -2.5371e-07 -1.36993e-07 1.132432e-07",
        "LINE_SCALE=2360",
        "SAMP_NUM_COEFF=0.04337163 1.775948 -0.87108 0.007425391 0.01783631 0.0004057179 -0.000184695 -0.04257537 -0.01127869 -1.531228e-06 1.017961e-05 0.000572344 -0.0002941 -0.0001301705 -0.0003289546 5.394918e-05 6.388447e-05 -4.038289e-06 -7.525785e-06 -5.431241e-07",
        "LONG_SCALE=0.8383",
        "SAMP_SCALE=593",
        "SAMP_OFF=589",
        "LAT_SCALE=1.4127",
        "LAT_OFF=33.8992",
        "LINE_OFF=2359",
        "LINE_DEN_COEFF=1 0.0007273139 -0.0006006867 -4.272095e-07 2.578717e-05 4.718479e-06 -2.116976e-06 -1.347805e-05 -2.209958e-05 8.131258e-06 -7.290143e-08 5.105109e-08 -7.353388e-07 0 2.131142e-06 9.697701e-08 1.237039e-08 7.153246e-08 6.758015e-08 5.811124e-08",
        "HEIGHT_SCALE=96.3",
    ]
    mem_ds.SetMetadata(rpc, "RPC")
    mem_ds.GetRasterBand(1).Fill(255)

    cutlineDSName = tmp_vsimem / "test_gdalwarp_lib_128.json"
    cutline_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer("cutline")
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((7.2151 32.51930,7.214316 32.58116,7.216043 32.59476,7.21666 32.5193,7.2151 32.51930))"
        )
    )
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    # Default is GDALWARP_DENSIFY_CUTLINE=YES
    ds = gdal.Warp(
        "",
        mem_ds,
        format="MEM",
        cutlineDSName=cutlineDSName,
        dstSRS="EPSG:4326",
        outputBounds=[7.2, 32.52, 7.217, 32.59],
        xRes=0.000226555,
        yRes=0.000226555,
        transformerOptions=["RPC_DEM=data/test_gdalwarp_lib_128_dem.tif"],
    )
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 4248, "bad checksum"

    # Below steps depend on GEOS
    if not ogrtest.have_geos():
        gdal.Unlink(cutlineDSName)
        return

    with gdal.config_option("GDALWARP_DENSIFY_CUTLINE", "ONLY_IF_INVALID"):
        ds = gdal.Warp(
            "",
            mem_ds,
            format="MEM",
            cutlineDSName=cutlineDSName,
            dstSRS="EPSG:4326",
            outputBounds=[7.2, 32.52, 7.217, 32.59],
            xRes=0.000226555,
            yRes=0.000226555,
            transformerOptions=["RPC_DEM=data/test_gdalwarp_lib_128_dem.tif"],
        )
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 4248, "bad checksum"

    with gdal.config_option("GDALWARP_DENSIFY_CUTLINE", "NO"), pytest.raises(Exception):
        gdal.Warp(
            "",
            mem_ds,
            format="MEM",
            cutlineDSName=cutlineDSName,
            dstSRS="EPSG:4326",
            outputBounds=[7.2, 32.52, 7.217, 32.59],
            xRes=0.000226555,
            yRes=0.000226555,
            transformerOptions=["RPC_DEM=data/test_gdalwarp_lib_128_dem.tif"],
        )

    gdal.Unlink(cutlineDSName)


###############################################################################
# Test automatic densification of cutline, but with initial guess leading
# to an invalid geometry (#6375)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.require_geos
@pytest.mark.require_driver("GeoJSON")
def test_gdalwarp_lib_129(tmp_vsimem):

    mem_ds = gdal.GetDriverByName("MEM").Create("", 1000, 2000)
    rpc = [
        "HEIGHT_OFF=1767",
        "LINE_NUM_COEFF=0.0004430579 -0.06200816 -1.007087 1.614683e-05 0.0009263463 -1.003745e-07 -2.346893e-06 -0.001179024 -0.0007413534 0 9.41488e-08 -4.566652e-07 2.895947e-05 -2.925327e-07 -2.308839e-05 -1.502702e-05 -4.775127e-06 0 4.290483e-07 2.850458e-08",
        "LONG_OFF=-.2282",
        "SAMP_DEN_COEFF=1 -0.01907542 0.01651069 -0.001340671 -0.0005495095 -1.072863e-05 -1.157626e-05 0.0003737224 0.0002712591 -0.0001363199 3.614417e-08 3.584749e-06 9.175671e-06 2.661593e-06 -1.045511e-05 -1.293648e-06 -2.769964e-06 5.931109e-07 -1.018687e-07 2.366109e-07",
        "LINE_SCALE=11886",
        "SAMP_NUM_COEFF=0.007334337 1.737166 -0.7954719 -0.004635387 -0.007478255 0.0006381186 -0.0003313475 0.0002313095 -0.002883101 -1.625925e-06 -6.409095e-06 -0.000403506 -0.0004441055 -0.0002360882 8.940442e-06 -0.0001780485 0.0001081517 -6.592931e-06 2.642496e-06 6.316508e-07",
        "LONG_SCALE=0.6996",
        "SAMP_SCALE=2945",
        "SAMP_OFF=2926",
        "LAT_SCALE=1.4116",
        "LAT_OFF=.4344",
        "LINE_OFF=-115",
        "LINE_DEN_COEFF=1 0.0008882352 -0.0002437686 -2.380782e-06 2.69128e-05 0 2.144654e-07 -2.093549e-05 -7.055149e-06 4.740057e-06 0 -1.588607e-08 -1.397592e-05 0 -7.717698e-07 6.505002e-06 0 -1.225041e-08 3.608499e-08 -4.463376e-08",
        "HEIGHT_SCALE=1024",
    ]

    mem_ds.SetMetadata(rpc, "RPC")
    mem_ds.GetRasterBand(1).Fill(255)

    cutlineDSName = tmp_vsimem / "test_gdalwarp_lib_129.json"
    cutline_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer("cutline")
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((-0.873086 0.511332,-0.626502 0.507654,-0.630715 0.282053,-0.876863 0.285693,-0.873086 0.511332))"
        )
    )
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    ds = gdal.Warp(
        "",
        mem_ds,
        format="MEM",
        cutlineDSName=cutlineDSName,
        dstSRS="EPSG:4326",
        outputBounds=[-1, 0, 0, 1],
        xRes=0.01,
        yRes=0.01,
        transformerOptions=["RPC_DEM=data/test_gdalwarp_lib_129_dem.vrt"],
    )
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 399, "bad checksum"

    gdal.Unlink(cutlineDSName)


###############################################################################
# Test automatic detection and setting of alpha channel, and setting RGB on
# GTiff output


def test_gdalwarp_lib_130(tmp_vsimem):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "test_gdalwarp_lib_130.tif", 1, 1, 5, options=["PHOTOMETRIC=RGB"]
    )
    src_ds.SetGeoTransform([100, 1, 0, 200, 0, -1])
    src_ds.GetRasterBand(5).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(2).Fill(2)
    src_ds.GetRasterBand(3).Fill(3)
    src_ds.GetRasterBand(4).Fill(4)
    src_ds.GetRasterBand(5).Fill(255)

    ds = gdal.Warp(tmp_vsimem / "test_gdalwarp_lib_130_dst.tif", src_ds)
    assert (
        ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    ), "bad color interpretation"
    assert (
        ds.GetRasterBand(5).GetColorInterpretation() == gdal.GCI_AlphaBand
    ), "bad color interpretation"
    expected_val = [1, 2, 3, 4, 255]
    for i in range(5):
        data = struct.unpack("B" * 1, ds.GetRasterBand(i + 1).ReadRaster())[0]
        assert data == expected_val[i], "bad checksum"

    # Wrap onto existing file
    for i in range(5):
        ds.GetRasterBand(i + 1).Fill(0)
    gdal.Warp(ds, src_ds)
    for i in range(5):
        data = struct.unpack("B" * 1, ds.GetRasterBand(i + 1).ReadRaster())[0]
        assert data == expected_val[i], "bad checksum"

    src_ds = None
    ds = None

    assert (
        gdal.VSIStatL(tmp_vsimem / "test_gdalwarp_lib_130_dst.tif.aux.xml") is None
    ), "got PAM file"


###############################################################################
# Test -nosrcalpha


def test_gdalwarp_lib_131(tmp_vsimem):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "test_gdalwarp_lib_131.tif", 1, 1, 2
    )
    src_ds.SetGeoTransform([100, 1, 0, 200, 0, -1])
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(2).Fill(0)

    ds = gdal.Warp(
        tmp_vsimem / "test_gdalwarp_lib_131_dst.tif", src_ds, options="-nosrcalpha"
    )
    expected_val = [1, 0]
    for i in range(2):
        data = struct.unpack("B" * 1, ds.GetRasterBand(i + 1).ReadRaster())[0]
        assert data == expected_val[i], "bad checksum"
    src_ds = None
    ds = None


###############################################################################
# Test that alpha blending works by warping onto an existing dataset
# with alpha > 0 and < 255


@pytest.mark.parametrize("dt", [gdal.GDT_UInt8, gdal.GDT_Float32])
def test_gdalwarp_lib_132(tmp_vsimem, dt):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "test_gdalwarp_lib_132.tif", 33, 1, 2, dt
    )
    src_ds.SetGeoTransform([100, 1, 0, 200, 0, -1])
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)

    ds = gdal.Translate(tmp_vsimem / "test_gdalwarp_lib_132_dst.tif", src_ds)
    dst_grey = 60
    dst_alpha = 100
    ds.GetRasterBand(1).Fill(dst_grey)
    ds.GetRasterBand(2).Fill(dst_alpha)

    src_grey = 170
    src_alpha = 200
    src_ds.GetRasterBand(1).Fill(src_grey)
    src_ds.GetRasterBand(2).Fill(src_alpha)
    gdal.Warp(ds, src_ds)
    expected_alpha = int(src_alpha + dst_alpha * (255 - src_alpha) / 255.0 + 0.5)
    expected_grey = int(
        (src_grey * src_alpha + dst_grey * dst_alpha * (255 - src_alpha) / 255.0)
        / expected_alpha
        + 0.5
    )
    expected_val = [expected_grey, expected_alpha]
    for i in range(2):
        for x in range(33):
            data = struct.unpack(
                "B" * 1,
                ds.GetRasterBand(i + 1).ReadRaster(i, 0, 1, 1, buf_type=gdal.GDT_UInt8),
            )[0]
            if data != pytest.approx(expected_val[i], abs=1):
                print(dt)
                print(x)
                pytest.fail("bad checksum")
    ds = None

    src_ds = None


###############################################################################
# Test cutline with multiple touching polygons


def test_gdalwarp_lib_133(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "test_gdalwarp_lib_133.shp"
    )
    lyr = ds.CreateLayer("cutline")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,1 0,1 1,0 1,0 0))"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((1 0,2 0,2 1,1 1,1 0))"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    src_ds = gdal.GetDriverByName("MEM").Create("", 4, 1)
    src_ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.Warp(
        "", src_ds, format="MEM", cutlineDSName=tmp_vsimem / "test_gdalwarp_lib_133.shp"
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 5, "Bad checksum"

    ds = None


###############################################################################
# Test SRC_METHOD=NO_GEOTRANSFORM and DST_METHOD=NO_GEOTRANSFORM (#6721)


def test_gdalwarp_lib_134(tmp_vsimem):

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_vsimem / "test_gdalwarp_lib_134.shp"
    )
    lyr = ds.CreateLayer("cutline")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((2 2,2 18,18 18,18 2,2 2))"))
    lyr.CreateFeature(f)
    f = None
    ds = None

    src_src_ds = gdal.Open("../gcore/data/byte.tif")
    src_ds = gdal.GetDriverByName("MEM").Create("", 20, 20)
    src_ds.GetRasterBand(1).WriteRaster(
        0, 0, 20, 20, src_src_ds.GetRasterBand(1).ReadRaster()
    )
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        transformerOptions={
            "SRC_METHOD": "NO_GEOTRANSFORM",
            "DST_METHOD": "NO_GEOTRANSFORM",
        },
        outputBounds=[1, 2, 4, 6],
    )
    assert ds is not None

    assert ds.GetRasterBand(1).ReadRaster() == src_src_ds.GetRasterBand(1).ReadRaster(
        1, 2, 4 - 1, 6 - 2
    ), "Bad checksum"

    ds = None

    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM", "DST_METHOD=NO_GEOTRANSFORM"],
        cutlineDSName=tmp_vsimem / "test_gdalwarp_lib_134.shp",
        cropToCutline=True,
    )
    assert ds is not None

    assert ds.GetRasterBand(1).ReadRaster() == src_src_ds.GetRasterBand(1).ReadRaster(
        2, 2, 16, 16
    ), "Bad checksum"

    ds = None

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource(
        tmp_vsimem / "test_gdalwarp_lib_134.shp"
    )


###############################################################################
# Test vertical datum shift


@pytest.fixture()
def gdalwarp_135_grid_gtx(tmp_path):

    grid_gtx = str(tmp_path / "grid.gtx")

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")

    grid_ds = gdal.GetDriverByName("GTX").Create(grid_gtx, 3, 3, 1, gdal.GDT_Float32)
    grid_ds.SetProjection(sr.ExportToWkt())
    grid_ds.SetGeoTransform([-180 - 90, 180, 0, 90 + 45, 0, -90])
    grid_ds.GetRasterBand(1).Fill(20)
    grid_ds = None

    return grid_gtx


@pytest.fixture()
def gdalwarp_135_grid2_gtx(tmp_path):

    grid2_gtx = str(tmp_path / "grid2.gtx")

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")

    grid_ds = gdal.GetDriverByName("GTX").Create(grid2_gtx, 3, 3, 1, gdal.GDT_Float32)
    grid_ds.SetProjection(sr.ExportToWkt())
    grid_ds.SetGeoTransform([-180 - 90, 180, 0, 90 + 45, 0, -90])
    grid_ds.GetRasterBand(1).Fill(5)
    grid_ds = None

    return grid2_gtx


@pytest.fixture()
def gdalwarp_135_src_ds(tmp_path):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    src_ds.GetRasterBand(1).Fill(100)

    return src_ds


@pytest.fixture()
def gdalwarp_135_src_ds_longlat(tmp_path):

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")

    src_ds_longlat = gdal.GetDriverByName("MEM").Create("", 2, 1)
    src_ds_longlat.SetProjection(sr.ExportToWkt())
    src_ds_longlat.SetGeoTransform([-180, 180, 0, 90, 0, -180])
    src_ds_longlat.GetRasterBand(1).Fill(100)

    return src_ds_longlat


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135(gdalwarp_135_src_ds, gdalwarp_135_grid_gtx):

    # Forward transform
    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds,
        format="MEM",
        srcSRS=f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
        dstSRS="EPSG:4979",
    )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_novshift(gdalwarp_135_src_ds, gdalwarp_135_grid_gtx):

    with gdaltest.error_raised(gdal.CE_None):
        ds = gdal.Warp(
            "",
            gdalwarp_135_src_ds,
            options=f'-f MEM -s_srs "+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs" -t_srs EPSG:4979 -novshift',
        )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135a(gdalwarp_135_src_ds_longlat, gdalwarp_135_grid_gtx):

    # Forward transform, longlat

    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds_longlat,
        format="MEM",
        srcSRS=f"+proj=longlat +datum=WGS84 +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
        dstSRS="EPSG:4979",
    )
    assert ds.GetGeoTransform() == (-180, 180, 0, 90, 0, -180)
    data = struct.unpack("B" * 2, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135b(gdalwarp_135_src_ds, gdalwarp_135_grid_gtx):

    # Inverse transform
    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds,
        format="MEM",
        srcSRS="+proj=utm +zone=31 +datum=WGS84 +units=m +no_defs",
        dstSRS=f"+proj=longlat +datum=WGS84 +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
    )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 80, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135c(gdalwarp_135_src_ds_longlat, gdalwarp_135_grid_gtx):

    # Inverse transform, longlat
    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds_longlat,
        format="MEM",
        srcSRS="EPSG:4979",
        dstSRS=f"+proj=longlat +datum=WGS84 +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
    )
    assert ds.GetGeoTransform() == (-180, 180, 0, 90, 0, -180)
    data = struct.unpack("B" * 2, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 80, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135d(
    gdalwarp_135_src_ds, gdalwarp_135_grid_gtx, gdalwarp_135_grid2_gtx
):

    # Both transforms
    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds,
        format="MEM",
        srcSRS=f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
        dstSRS=f"+proj=longlat +datum=WGS84 +geoidgrids={gdalwarp_135_grid2_gtx} +vunits=m +no_defs",
    )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 115, "Bad value"


def test_gdalwarp_lib_135e(gdalwarp_135_src_ds):

    # Both transforms, but none of them have geoidgrids
    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds,
        format="MEM",
        srcSRS="EPSG:32631+5730",
        dstSRS="EPSG:4326+5621",
    )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135f(gdalwarp_135_src_ds, gdalwarp_135_grid_gtx):

    # Both transforms being a no-op
    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds,
        format="MEM",
        srcSRS=f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
        dstSRS=f"+proj=longlat +datum=WGS84 +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
    )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135g(
    gdalwarp_135_src_ds, gdalwarp_135_grid_gtx, gdalwarp_135_grid2_gtx
):
    # Both transforms to anonymous VRT
    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds,
        format="VRT",
        srcSRS=f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
        dstSRS=f"+proj=longlat +datum=WGS84 +geoidgrids={gdalwarp_135_grid2_gtx} +vunits=m +no_defs",
    )
    gdalwarp_135_src_ds = None  # drop the ref to src_ds before for fun
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 115, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135h(gdalwarp_135_grid_gtx, gdalwarp_135_grid2_gtx):
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    src_ds.GetRasterBand(1).Fill(100)

    # Target CRS in us-ft
    ds = gdal.Warp(
        "",
        src_ds,
        format="VRT",
        outputType=gdal.GDT_Float32,
        srcSRS=f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
        dstSRS=f"+proj=longlat +datum=WGS84 +geoidgrids={gdalwarp_135_grid2_gtx} +vunits=us-ft +no_defs",
    )
    data = struct.unpack("f" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == pytest.approx(115 / (1200.0 / 3937)), "Bad value"


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135i(
    gdalwarp_135_src_ds, gdalwarp_135_grid_gtx, gdalwarp_135_grid2_gtx, tmp_path
):
    # Both transforms to regular VRT
    gdal.GetDriverByName("GTiff").CreateCopy(f"{tmp_path}/dem.tif", gdalwarp_135_src_ds)
    gdal.Warp(
        f"{tmp_path}/tmp.vrt",
        f"{tmp_path}/dem.tif",
        format="VRT",
        srcSRS=f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs",
        dstSRS=f"+proj=longlat +datum=WGS84 +geoidgrids={gdalwarp_135_grid2_gtx} +vunits=m +no_defs",
    )
    ds = gdal.Open(f"{tmp_path}/tmp.vrt")
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    ds = None
    assert data == 115, "Bad value"


def test_gdalwarp_lib_135j(gdalwarp_135_src_ds):
    # Missing grid in forward path, but this is OK
    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds,
        format="MEM",
        srcSRS="+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=@i_dont_exist.tif +vunits=m +no_defs",
        dstSRS="EPSG:4979",
    )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, "Bad value"


def test_gdalwarp_lib_135k(gdalwarp_135_src_ds):
    # Missing grid in inverse path but this is OK
    ds = gdal.Warp(
        "",
        gdalwarp_135_src_ds,
        format="MEM",
        srcSRS="+proj=utm +zone=31 +datum=WGS84 +units=m +no_defs",
        dstSRS="+proj=longlat +datum=WGS84 +geoidgrids=@i_dont_exist.tif +vunits=m +no_defs",
    )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135m(gdalwarp_135_grid_gtx):
    # Forward transform with explicit m unit
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    sr = osr.SpatialReference()
    sr.ImportFromProj4(
        f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs"
    )
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(100)
    src_ds.GetRasterBand(1).SetUnitType("m")

    ds = gdal.Warp("", src_ds, format="MEM", dstSRS="EPSG:4979")
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135n(gdalwarp_135_grid_gtx):
    # Forward transform with explicit ft unit
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    sr = osr.SpatialReference()
    sr.ImportFromProj4(
        f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs"
    )
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(100 / 0.3048)
    src_ds.GetRasterBand(1).SetUnitType("ft")

    ds = gdal.Warp(
        "", src_ds, format="MEM", dstSRS="EPSG:4979", outputType=gdal.GDT_UInt8
    )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135o(gdalwarp_135_grid_gtx):
    # Forward transform with explicit US survey foot unit
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    sr = osr.SpatialReference()
    sr.ImportFromProj4(
        f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs"
    )
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(100 / 0.3048006096012192)
    src_ds.GetRasterBand(1).SetUnitType("US survey foot")

    ds = gdal.Warp(
        "", src_ds, format="MEM", dstSRS="EPSG:4979", outputType=gdal.GDT_UInt8
    )
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135p(gdalwarp_135_grid_gtx):
    # Forward transform with explicit unhandled unit
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    sr = osr.SpatialReference()
    sr.ImportFromProj4(
        f"+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={gdalwarp_135_grid_gtx} +vunits=m +no_defs"
    )
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(100)
    src_ds.GetRasterBand(1).SetUnitType("unhandled")

    with gdal.quiet_errors():
        ds = gdal.Warp("", src_ds, format="MEM", dstSRS="EPSG:4979")
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, "Bad value"


def test_gdalwarp_lib_135q():
    # Transform to same CRS (implicit)
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    sr = osr.SpatialReference()
    sr.SetFromUserInput("EPSG:6597+6360")
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(100)

    ds = gdal.Warp("", src_ds, format="MEM")
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, "Bad value"


def test_gdalwarp_lib_135r():
    # Transform to same CRS (explicit)
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    sr = osr.SpatialReference()
    sr.SetFromUserInput("EPSG:6597+6360")
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(100)

    ds = gdal.Warp("", src_ds, format="MEM", dstSRS="EPSG:6597+6360")
    data = struct.unpack("B" * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, "Bad value"


@pytest.mark.require_driver("GTX")
def test_gdalwarp_lib_135s(tmp_path):
    empty_grid_gtx = str(tmp_path / "empty_grid.gtx")

    grid_ds = gdal.GetDriverByName("GTX").Create(
        empty_grid_gtx, 3, 3, 1, gdal.GDT_Float32
    )
    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")
    grid_ds.SetProjection(sr.ExportToWkt())
    grid_ds.SetGeoTransform([-180 - 90, 180, 0, 90 + 45, 0, -90])
    grid_ds.GetRasterBand(1).Fill(-88.8888)
    grid_ds = None

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    src_ds.GetRasterBand(1).Fill(100)

    if osr.GetPROJVersionMajor() >= 8:
        # Test missing shift values in area of interest
        with pytest.raises(Exception):
            gdal.Warp(
                "",
                src_ds,
                format="MEM",
                srcSRS="+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids={empty_grid_gtx} +vunits=m +no_defs",
                dstSRS="EPSG:4979",
            )


###############################################################################
# Test error code path linked with failed warper initialization


@pytest.mark.parametrize("fmt", ("MEM", "VRT"))
def test_gdalwarp_lib_136(fmt):

    with pytest.raises(Exception):
        gdal.Warp(
            "",
            "../gcore/data/utmsmall.tif",
            format=fmt,
            warpOptions=["CUTLINE=invalid"],
        )


###############################################################################
# Test warping two input datasets with different SRS, with no explicit target SRS


def test_gdalwarp_lib_several_sources_with_different_srs_no_explicit_target_srs():
    src_ds = gdal.Open("../gcore/data/byte.tif")
    src_ds_32611_left = gdal.Translate(
        "", src_ds, format="MEM", srcWin=[0, 0, 10, 20], outputSRS="EPSG:32611"
    )
    src_ds_32611_right = gdal.Translate(
        "", src_ds, format="MEM", srcWin=[10, 0, 10, 20], outputSRS="EPSG:32611"
    )
    src_ds_4326_right = gdal.Warp(
        "", src_ds_32611_right, format="MEM", dstSRS="EPSG:4326"
    )
    out_ds = gdal.Warp("", [src_ds_4326_right, src_ds_32611_left], format="MEM")
    assert out_ds is not None
    assert out_ds.RasterXSize == 23
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 5048


###############################################################################
# Test fix for https://trac.osgeo.org/gdal/ticket/7243


def test_gdalwarp_lib_touching_dateline():

    src_ds = gdal.GetDriverByName("MEM").Create("", 100, 100)
    src_ds.SetGeoTransform([-2050000, 500, 0, 2100000, 0, -500])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3411)
    src_ds.SetProjection(sr.ExportToWkt())
    out_ds = gdal.Warp("", src_ds, dstSRS="EPSG:4326", format="MEM")
    assert out_ds.RasterXSize == 319


###############################################################################
# Test fix for https://trac.osgeo.org/gdal/ticket/7245


@pytest.mark.require_driver("netCDF")
@pytest.mark.parametrize("frmt", ("NC", "NC2", "NC4"))
def test_gdalwarp_lib_override_default_output_nodata(frmt, tmp_path):

    drv = gdal.GetDriverByName("netCDF")
    creationoptionlist = drv.GetMetadataItem("DMD_CREATIONOPTIONLIST")

    if f"<Value>{frmt}</Value>" not in creationoptionlist:
        pytest.skip(f"netCDF format {frmt} not available")

    result_nc = str(tmp_path / "out.nc")

    gdal.Warp(
        result_nc,
        "../gcore/data/byte.tif",
        srcNodata=255,
        format="netCDF",
        creationOptions=["FORMAT=" + frmt],
    )
    ds = gdal.Open(result_nc)
    assert ds.GetRasterBand(1).GetNoDataValue() == 255, frmt
    assert ds.GetProjection() != "", frmt
    ds = None


###############################################################################
# Test automatting setting (or not) of SKIP_NOSOURCE=YES


@pytest.mark.parametrize(
    "options,checksum",
    [
        ("-wo SKIP_NOSOURCE=NO", 41500),
        ("", 41500),
        ("-wo INIT_DEST=0", 41500),
        ("-wo INIT_DEST=NO_DATA -dstnodata 0", 41500),
        ("-dstnodata 0", 41500),
        ("-dstnodata 1", 51132),
        ("-dstnodata 1 -wo INIT_DEST=NO_DATA", 51132),
        ("-dstnodata 1 -wo INIT_DEST=1", 51132),
        ("-dstnodata 127 -wo INIT_DEST=0", 41500),
    ],
)
@pytest.mark.parametrize("output_format", ("GTiff", "MEM"))
def test_gdalwarp_lib_auto_skip_nosource(tmp_vsimem, options, checksum, output_format):

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    src_ds = gdal.GetDriverByName("MEM").Create("", 1000, 500)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src_ds.SetProjection(sr.ExportToWkt())

    tmpfilename = tmp_vsimem / "test_gdalwarp_lib_auto_skip_nosource.tif"

    out_ds = gdal.Warp(
        tmpfilename,
        src_ds,
        options=f"-te 1.5 48 3.5 49.5 -wm 100000 -of {output_format} {options}",
    )
    cs = out_ds.GetRasterBand(1).Checksum()

    assert cs == checksum, options


def test_gdalwarp_lib_auto_skip_nosource_2(tmp_vsimem):

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    # Test with 2 input datasets
    src_ds1 = gdal.GetDriverByName("MEM").Create("", 500, 500)
    src_ds1.GetRasterBand(1).Fill(255)
    src_ds1.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src_ds1.SetProjection(sr.ExportToWkt())

    src_ds2 = gdal.GetDriverByName("MEM").Create("", 500, 500)
    src_ds2.GetRasterBand(1).Fill(255)
    src_ds2.SetGeoTransform([2.5, 0.001, 0, 49, 0, -0.001])
    src_ds2.SetProjection(sr.ExportToWkt())

    out_ds = gdal.Warp(
        tmp_vsimem / "out.tif",
        [src_ds1, src_ds2],
        options="-te 1.5 48 3.5 49.5 -wm 100000 -of GTiff ",
    )
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 41500


###############################################################################
# Test warping a full EPSG:4326 extent to +proj=ortho
# (https://github.com/OSGeo/gdal/issues/862)


def test_gdalwarp_lib_to_ortho(tmp_path):

    result_tif = str(tmp_path / "out.tif")

    out_ds = gdal.Warp(
        result_tif,
        "../gdrivers/data/small_world.tif",
        options='-of MEM -t_srs "+proj=ortho +datum=WGS84" -ts 1024 1024',
    )

    line = out_ds.GetRasterBand(1).ReadRaster(0, 0, out_ds.RasterXSize, 1)
    line = struct.unpack("B" * out_ds.RasterXSize, line)
    # Fail if the first line is completely black
    assert line.count(0) != out_ds.RasterXSize, "first line is completely black"

    line = out_ds.GetRasterBand(1).ReadRaster(
        0, out_ds.RasterYSize - 1, out_ds.RasterXSize, 1
    )
    line = struct.unpack("B" * out_ds.RasterXSize, line)
    # Fail if the last line is completely black
    assert line.count(0) != out_ds.RasterXSize, "last line is completely black"


###############################################################################
def test_gdalwarp_lib_insufficient_dst_band_count():

    src_ds = gdal.Translate("", "../gcore/data/byte.tif", options="-of MEM -b 1 -b 1")
    dst_ds = gdal.Translate("", "../gcore/data/byte.tif", options="-of MEM")
    with pytest.raises(Exception):
        gdal.Warp(dst_ds, src_ds)


###############################################################################
# Test -ct


def test_gdalwarp_lib_ct():

    dstDS = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        options='-r cubic -f MEM -t_srs EPSG:4326 -ct "proj=pipeline step inv proj=utm zone=11 ellps=clrk66 step proj=unitconvert xy_in=rad xy_out=deg step proj=axisswap order=2,1"',
    )

    assert dstDS.GetRasterBand(1).Checksum() == 4772


def test_gdalwarp_lib_ct_wkt():

    wkt = """CONCATENATEDOPERATION["Inverse of UTM zone 11N + Null geographic offset from NAD27 to WGS 84",
    SOURCECRS[
        PROJCRS["NAD27 / UTM zone 11N",
            BASEGEOGCRS["NAD27",
                DATUM["North American Datum 1927",
                    ELLIPSOID["Clarke 1866",6378206.4,294.978698213898,
                        LENGTHUNIT["metre",1]]],
                PRIMEM["Greenwich",0,
                    ANGLEUNIT["degree",0.0174532925199433]]],
            CONVERSION["UTM zone 11N",
                METHOD["Transverse Mercator",
                    ID["EPSG",9807]],
                PARAMETER["Latitude of natural origin",0,
                    ANGLEUNIT["degree",0.0174532925199433],
                    ID["EPSG",8801]],
                PARAMETER["Longitude of natural origin",-117,
                    ANGLEUNIT["degree",0.0174532925199433],
                    ID["EPSG",8802]],
                PARAMETER["Scale factor at natural origin",0.9996,
                    SCALEUNIT["unity",1],
                    ID["EPSG",8805]],
                PARAMETER["False easting",500000,
                    LENGTHUNIT["metre",1],
                    ID["EPSG",8806]],
                PARAMETER["False northing",0,
                    LENGTHUNIT["metre",1],
                    ID["EPSG",8807]]],
            CS[Cartesian,2],
                AXIS["(E)",east,
                    ORDER[1],
                    LENGTHUNIT["metre",1]],
                AXIS["(N)",north,
                    ORDER[2],
                    LENGTHUNIT["metre",1]],
            USAGE[
                SCOPE["unknown"],
                AREA["North America - 120°W to 114°W and NAD27 by country - onshore"],
                BBOX[26.93,-120,78.13,-114]],
            ID["EPSG",26711]]],
    TARGETCRS[
        GEOGCRS["WGS 84",
            DATUM["World Geodetic System 1984",
                ELLIPSOID["WGS 84",6378137,298.257223563,
                    LENGTHUNIT["metre",1]]],
            PRIMEM["Greenwich",0,
                ANGLEUNIT["degree",0.0174532925199433]],
            CS[ellipsoidal,2],
                AXIS["geodetic latitude (Lat)",north,
                    ORDER[1],
                    ANGLEUNIT["degree",0.0174532925199433]],
                AXIS["geodetic longitude (Lon)",east,
                    ORDER[2],
                    ANGLEUNIT["degree",0.0174532925199433]],
            USAGE[
                SCOPE["unknown"],
                AREA["World"],
                BBOX[-90,-180,90,180]],
            ID["EPSG",4326]]],
    STEP[
        CONVERSION["Inverse of UTM zone 11N",
            METHOD["Inverse of Transverse Mercator",
                ID["INVERSE(EPSG)",9807]],
            PARAMETER["Latitude of natural origin",0,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8801]],
            PARAMETER["Longitude of natural origin",-117,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8802]],
            PARAMETER["Scale factor at natural origin",0.9996,
                SCALEUNIT["unity",1],
                ID["EPSG",8805]],
            PARAMETER["False easting",500000,
                LENGTHUNIT["metre",1],
                ID["EPSG",8806]],
            PARAMETER["False northing",0,
                LENGTHUNIT["metre",1],
                ID["EPSG",8807]],
            ID["INVERSE(EPSG)",16011]]],
    STEP[
        COORDINATEOPERATION["Null geographic offset from NAD27 to WGS 84",
            SOURCECRS[
                GEOGCRS["NAD27",
                    DATUM["North American Datum 1927",
                        ELLIPSOID["Clarke 1866",6378206.4,294.978698213898,
                            LENGTHUNIT["metre",1]]],
                    PRIMEM["Greenwich",0,
                        ANGLEUNIT["degree",0.0174532925199433]],
                    CS[ellipsoidal,2],
                        AXIS["geodetic latitude (Lat)",north,
                            ORDER[1],
                            ANGLEUNIT["degree",0.0174532925199433]],
                        AXIS["geodetic longitude (Lon)",east,
                            ORDER[2],
                            ANGLEUNIT["degree",0.0174532925199433]],
                    USAGE[
                        SCOPE["unknown"],
                        AREA["North America - NAD27"],
                        BBOX[7.15,167.65,83.17,-47.74]],
                    ID["EPSG",4267]]],
            TARGETCRS[
                GEOGCRS["WGS 84",
                    DATUM["World Geodetic System 1984",
                        ELLIPSOID["WGS 84",6378137,298.257223563,
                            LENGTHUNIT["metre",1]]],
                    PRIMEM["Greenwich",0,
                        ANGLEUNIT["degree",0.0174532925199433]],
                    CS[ellipsoidal,2],
                        AXIS["geodetic latitude (Lat)",north,
                            ORDER[1],
                            ANGLEUNIT["degree",0.0174532925199433]],
                        AXIS["geodetic longitude (Lon)",east,
                            ORDER[2],
                            ANGLEUNIT["degree",0.0174532925199433]],
                    USAGE[
                        SCOPE["unknown"],
                        AREA["World"],
                        BBOX[-90,-180,90,180]],
                    ID["EPSG",4326]]],
            METHOD["Geographic2D offsets",
                ID["EPSG",9619]],
            PARAMETER["Latitude offset",0,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8601]],
            PARAMETER["Longitude offset",0,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8602]],
            USAGE[
                SCOPE["unknown"],
                AREA["World"],
                BBOX[-90,-180,90,180]]]],
    USAGE[
        SCOPE["unknown"],
        AREA["World"],
        BBOX[-90,-180,90,180]]]"""

    dstDS = gdal.Warp(
        "",
        "../gcore/data/byte.tif",
        resampleAlg=gdal.GRA_Cubic,
        format="MEM",
        dstSRS="EPSG:4326",
        coordinateOperation=wkt,
    )

    assert dstDS.GetRasterBand(1).Checksum() == 4772


###############################################################################
# Test warping from a RPC dataset to a new dataset larger than needed


def test_gdalwarp_lib_restrict_output_dataset_warp_rpc_new():

    dstDS = gdal.Warp(
        "",
        "data/unstable_rpc_with_dem_source.tif",
        options="-f MEM -et 0 -to RPC_DEM=data/unstable_rpc_with_dem_elevation.tif -to RPC_MAX_ITERATIONS=40 -to RPC_DEM_MISSING_VALUE=0 -t_srs EPSG:3857 -te 12693400.445 2547311.740 12700666.740 2553269.051 -ts 380 311",
    )
    cs = dstDS.GetRasterBand(1).Checksum()
    assert cs == 53230

    with gdaltest.config_option("RESTRICT_OUTPUT_DATASET_UPDATE", "NO"):
        dstDS = gdal.Warp(
            "",
            "data/unstable_rpc_with_dem_source.tif",
            options="-f MEM -et 0 -to RPC_DEM=data/unstable_rpc_with_dem_elevation.tif -to RPC_MAX_ITERATIONS=40 -to RPC_DEM_MISSING_VALUE=0 -t_srs EPSG:3857 -te 12693400.445 2547311.740 12700666.740 2553269.051 -ts 380 311",
        )
    cs = dstDS.GetRasterBand(1).Checksum()
    assert cs != 53230


###############################################################################
# Test warping from a RPC dataset to an existing dataset


def test_gdalwarp_lib_restrict_output_dataset_warp_rpc_existing():

    dstDS = gdal.Translate(
        "", "data/unstable_rpc_with_dem_blank_output.tif", format="MEM"
    )
    gdal.Warp(
        dstDS,
        "data/unstable_rpc_with_dem_source.tif",
        options="-et 0 -to RPC_DEM=data/unstable_rpc_with_dem_elevation.tif -to RPC_MAX_ITERATIONS=40 -to RPC_DEM_MISSING_VALUE=0",
    )
    cs = dstDS.GetRasterBand(1).Checksum()
    assert cs == 53230


###############################################################################
# Test warping from a RPC dataset to an existing dataset that doesn't intersect
# source extent defined by RPC


def test_gdalwarp_lib_restrict_output_dataset_warp_rpc_existing_no_intersection():

    dstDS = gdal.Translate(
        "",
        "data/unstable_rpc_with_dem_blank_output.tif",
        format="MEM",
        outputBounds=[112693400.445, 2553269.051, 112700666.740, 2547311.740],
    )
    gdal.ErrorReset()
    assert (
        gdal.Warp(
            dstDS,
            "data/unstable_rpc_with_dem_source.tif",
            options="-et 0 -to RPC_DEM=data/unstable_rpc_with_dem_elevation.tif "
            + "-to RPC_MAX_ITERATIONS=40 -to RPC_DEM_MISSING_VALUE=0",
        )
        == 1
    )
    assert gdal.GetLastErrorType() == gdal.CE_None
    cs = dstDS.GetRasterBand(1).Checksum()
    assert cs == 0


###############################################################################
# Test warping from a RPC dataset to an existing dataset, with using RPC_FOOTPRINT


@pytest.mark.require_geos
def test_gdalwarp_lib_restrict_output_dataset_warp_rpc_existing_RPC_FOOTPRINT():

    with gdaltest.config_option("RESTRICT_OUTPUT_DATASET_UPDATE", "NO"):
        dstDS = gdal.Translate(
            "", "data/unstable_rpc_with_dem_blank_output.tif", format="MEM"
        )
        gdal.Warp(
            dstDS,
            "data/unstable_rpc_with_dem_source.tif",
            options='-et 0 -to RPC_DEM=data/unstable_rpc_with_dem_elevation.tif -to RPC_MAX_ITERATIONS=40 -to RPC_DEM_MISSING_VALUE=0 -to "RPC_FOOTPRINT=POLYGON ((114.070906445526 22.329620213341,114.085953272341 22.3088955493586,114.075520805749 22.3027084861851,114.060942102434 22.3236815197571,114.060942102434 22.3236815197571,114.060942102434 22.3236815197571,114.060942102434 22.3236815197571,114.070906445526 22.329620213341))"',
        )
        cs = dstDS.GetRasterBand(1).Checksum()
        assert cs == 53230


###############################################################################
# Test warping from EPSG:4326 to EPSG:3857


def test_gdalwarp_lib_bug_4326_to_3857():

    ds = gdal.Warp(
        "",
        "data/test_bug_4326_to_3857.tif",
        options="-f MEM -t_srs EPSG:3857 -ts 20 20",
    )
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4672


###############################################################################
# Test warping full world from EPSG:4326 to EPSG:3857


def test_gdalwarp_lib_full_world_4326_to_3857():
    class MyHandler:
        def __init__(self):
            self.warning_raised = False

        def callback(self, err_type, err_no, err_msg):
            if err_type == gdal.CE_Warning and "Clamping output bounds to" in err_msg:
                self.warning_raised = True

    my_error_handler = MyHandler()
    with gdaltest.error_handler(my_error_handler.callback):
        ds = gdal.Warp(
            "",
            "../gdrivers/data/small_world.tif",
            options="-f MEM -t_srs EPSG:3857",
        )
    assert my_error_handler.warning_raised
    assert ds.GetGeoTransform() == pytest.approx(
        (
            -20037508.342789244,
            103286.12547829507,
            0.0,
            20037508.342789248,
            0.0,
            -103286.12547829509,
        )
    )
    assert ds.RasterXSize == 388
    assert ds.RasterYSize == 388


###############################################################################
# Test warping of single source to COG


def test_gdalwarp_lib_to_cog(tmp_vsimem):

    tmpfilename = tmp_vsimem / "cog.tif"
    ds = gdal.Warp(
        tmpfilename,
        "../gcore/data/byte.tif",
        options="-f COG -t_srs EPSG:3857 -ts 20 20 -r near",
    )
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None


###############################################################################
# Test warping of single source to COG with reprojection options


def test_gdalwarp_lib_to_cog_reprojection_options(tmp_vsimem):

    tmpfilename = tmp_vsimem / "cog.tif"
    ds = gdal.Warp(
        tmpfilename,
        "../gcore/data/byte.tif",
        options="-f COG -co TILING_SCHEME=GoogleMapsCompatible",
    )
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 256
    assert ds.GetGeoTransform() == pytest.approx(
        (
            -13110479.09147343,
            76.43702828517416,
            0.0,
            4030983.1236470547,
            0.0,
            -76.43702828517416,
        )
    )
    assert ds.GetRasterBand(1).Checksum() in (
        4187,
        4300,
        4186,
    )  # 4300 on Mac, 4186 on Mac / Conda
    assert ds.GetRasterBand(2).Checksum() == 4415
    ds = None
    gdal.Unlink(tmpfilename)


###############################################################################
# Test warping of single source to COG with reprojection options and -te


def test_gdalwarp_lib_to_cog_reprojection_options_and_te(tmp_vsimem):

    tmpfilename = tmp_vsimem / "cog.tif"
    ds = gdal.Warp(
        tmpfilename,
        "../gcore/data/byte.tif",
        options="-f COG -co TILING_SCHEME=GoogleMapsCompatible -te -13110479.091 4011415.244 -13090911.212 4030983.124",
    )
    assert ds.GetRasterBand(1).Checksum() != 0
    ds = None
    gdal.Unlink(tmpfilename)


###############################################################################


@pytest.mark.require_driver("COG")
def test_gdalwarp_to_cog_with_s_srs_and_t_srs(tmp_vsimem):

    out_ds = gdal.Warp(
        tmp_vsimem / "out.tif",
        "../gcore/data/byte.tif",
        options="-s_srs EPSG:32611 -t_srs EPSG:4326 -of COG",
    )
    assert out_ds.RasterXSize == 22
    assert out_ds.RasterYSize == 18
    assert out_ds.GetGeoTransform() == pytest.approx(
        (
            -117.64116991516866,
            0.0005981056256842434,
            0.0,
            33.9006687039261,
            0.0,
            -0.0005981056256842434,
        )
    )
    assert out_ds.GetRasterBand(1).Checksum() != 0


###############################################################################


@pytest.mark.require_driver("COG")
def test_gdalwarp_to_cog_with_s_srs_and_tiling_scheme(tmp_vsimem):

    out_ds = gdal.Warp(
        tmp_vsimem / "out.tif",
        "../gcore/data/byte.tif",
        options="-s_srs EPSG:32611 -co TILING_SCHEME=GoogleMapsCompatible -of COG",
    )
    assert out_ds.RasterXSize == 256
    assert out_ds.RasterYSize == 256
    assert out_ds.GetGeoTransform() == pytest.approx(
        (
            -13110479.09147343,
            76.43702828517416,
            0.0,
            4030983.1236470547,
            0.0,
            -76.43702828517416,
        )
    )
    assert out_ds.GetRasterBand(1).Checksum() != 0


###############################################################################
# Test warping of single source to COG with reprojection options and conflicting
# -t_srs


def test_gdalwarp_lib_to_cog_reprojection_options_and_conflicting_t_srs(tmp_vsimem):

    tmpfilename = tmp_vsimem / "cog.tif"
    with pytest.raises(Exception):
        gdal.Warp(
            tmpfilename,
            "../gcore/data/byte.tif",
            options="-f COG -co TILING_SCHEME=GoogleMapsCompatible -t_srs EPSG:4326",
        )


###############################################################################
# Test warping of single source to COG with reprojection options and conflicting
# -t_srs


def test_gdalwarp_lib_to_cog_reprojection_options_te_and_conflicting_t_srs(tmp_vsimem):

    tmpfilename = tmp_vsimem / "cog.tif"
    with pytest.raises(Exception):
        gdal.Warp(
            tmpfilename,
            "../gcore/data/byte.tif",
            options="-f COG -co TILING_SCHEME=GoogleMapsCompatible -te -13110479.091 4011415.244 -13090911.212 4030983.124 -t_srs EPSG:4326",
        )


###############################################################################
# Test warping of multiple source, compatible of BuildVRT mosaicing, to COG


def test_gdalwarp_lib_multiple_source_compatible_buildvrt_to_cog(tmp_vsimem):

    tmpfilename = tmp_vsimem / "cog.tif"
    left_ds = gdal.Translate(
        tmp_vsimem / "left.tif", "../gcore/data/byte.tif", options="-srcwin 0 0 10 20"
    )
    right_ds = gdal.Translate(
        tmp_vsimem / "right.tif", "../gcore/data/byte.tif", options="-srcwin 10 0 10 20"
    )
    ds = gdal.Warp(tmpfilename, [left_ds, right_ds], options="-f COG")
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None


###############################################################################
# Test warping of multiple source, compatible of BuildVRT mosaicing, to COG,
# with reprojection options


def test_gdalwarp_lib_multiple_source_compatible_buildvrt_to_cog_reprojection_options(
    tmp_vsimem,
):

    tmpfilename = tmp_vsimem / "cog.tif"
    left_ds = gdal.Translate(
        tmp_vsimem / "left.tif", "../gcore/data/byte.tif", options="-srcwin 0 0 10 20"
    )
    right_ds = gdal.Translate(
        tmp_vsimem / "right.tif", "../gcore/data/byte.tif", options="-srcwin 10 0 10 20"
    )
    ds = gdal.Warp(
        tmpfilename,
        [left_ds, right_ds],
        options="-f COG -co TILING_SCHEME=GoogleMapsCompatible",
    )
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() in (
        4187,
        4300,
        4186,
    )  # 4300 on Mac, 4186 on Mac / Conda
    assert ds.GetRasterBand(2).Checksum() == 4415
    ds = None


###############################################################################
# Test warping of multiple source, incompatible of BuildVRT mosaicing, to COG


def test_gdalwarp_lib_multiple_source_incompatible_buildvrt_to_cog(tmp_vsimem):

    tmpfilename = tmp_vsimem / "cog.tif"
    left_ds = gdal.Translate(
        tmp_vsimem / "left.tif",
        "../gcore/data/byte.tif",
        options="-srcwin 0 0 15 20 -b 1 -b 1 -colorinterp_2 alpha -scale_2 0 255 255 255",
    )
    right_ds = gdal.Translate(
        tmp_vsimem / "right.tif",
        "../gcore/data/byte.tif",
        options="-srcwin 5 0 15 20 -b 1 -b 1 -colorinterp_2 alpha -scale_2 0 255 255 255",
    )
    ds = gdal.Warp(tmpfilename, [left_ds, right_ds], options="-f COG")
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(2).Checksum() == 4873
    ds = None


###############################################################################
# Test warping of multiple source, incompatible of BuildVRT mosaicing, to COG,
# with reprojection options


def test_gdalwarp_lib_multiple_source_incompatible_buildvrt_to_cog_reprojection_options(
    tmp_vsimem,
):

    tmpfilename = tmp_vsimem / "cog.tif"
    left_ds = gdal.Translate(
        tmp_vsimem / "left.tif",
        "../gcore/data/byte.tif",
        options="-srcwin 0 0 15 20 -b 1 -b 1 -colorinterp_2 alpha -scale_2 0 255 255 255",
    )
    right_ds = gdal.Translate(
        tmp_vsimem / "right.tif",
        "../gcore/data/byte.tif",
        options="-srcwin 5 0 15 20 -b 1 -b 1 -colorinterp_2 alpha -scale_2 0 255 255 255",
    )
    ds = gdal.Warp(
        tmpfilename,
        [left_ds, right_ds],
        options="-f COG -co TILING_SCHEME=GoogleMapsCompatible",
    )
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() in (
        4207,
        4315,
        4206,
    )  # 4300 on Mac, 4206 on Mac / Conda
    assert ds.GetRasterBand(2).Checksum() == 4415
    ds = None


###############################################################################


def test_gdalwarp_lib_no_crs():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([0, 10, 0, 0, 0, -10])
    out_ds = gdal.Warp(
        "", src_ds, options='-of MEM -ct "+proj=unitconvert +xy_in=1 +xy_out=2"'
    )
    assert out_ds.GetGeoTransform() == (0.0, 5.0, 0.0, 0.0, 0.0, -5.0)


###############################################################################
# Test that the warp kernel properly computes the resampling kernel xsize
# when wrapping along the antimeridian (related to #2754)


def test_gdalwarp_lib_xscale_antimeridian(tmp_vsimem):

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")

    src1_ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src1.tif", 1000, 1000)
    src1_ds.SetGeoTransform([179, 0.001, 0, 50, 0, -0.001])
    src1_ds.SetProjection(sr.ExportToWkt())
    src1_ds.GetRasterBand(1).Fill(100)
    src1_ds = None

    src2_ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src2.tif", 1000, 1000)
    src2_ds.SetGeoTransform([-180, 0.001, 0, 50, 0, -0.001])
    src2_ds.SetProjection(sr.ExportToWkt())
    src2_ds.GetRasterBand(1).Fill(200)
    src2_ds = None

    source = gdal.BuildVRT("", [tmp_vsimem / "src1.tif", tmp_vsimem / "src2.tif"])
    # Wrap to UTM zone 1 across the antimeridian
    ds = gdal.Warp(
        "",
        source,
        options="-of MEM -t_srs EPSG:32601 -te 276000 5464000 290000 5510000 -tr 1000 1000 -r cubic",
    )
    vals = struct.unpack("B" * ds.RasterXSize * ds.RasterYSize, ds.ReadRaster())
    assert vals[0] == 100
    assert vals[ds.RasterXSize - 1] == 200
    # Check that the set of values is just 100 and 200. If the xscale was wrong,
    # we would take intou account 0 values outsize of the 2 tiles.
    assert set(vals) == set([100, 200])


###############################################################################
# Test gdalwarp preserves scale & offset of bands


def test_gdalwarp_lib_scale_offset():

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).SetScale(1.5)
    src_ds.GetRasterBand(1).SetOffset(2.5)

    ds = gdal.Warp("", src_ds, format="MEM")
    assert ds.GetRasterBand(1).GetScale() == 1.5
    assert ds.GetRasterBand(1).GetOffset() == 2.5


###############################################################################
# Test cutline with zero-width sliver


@pytest.mark.require_driver("GeoJSON")
def test_gdalwarp_lib_cutline_zero_width_sliver(tmp_vsimem):

    # Geometry valid in EPSG:4326, but that has a zero-width sliver
    # at point [-90.783634, 33.612466] that results in an invalid geometry in UTM
    geojson = '{"type": "MultiPolygon", "coordinates": [[[[-90.789474, 33.608456], [-90.789675, 33.609965], [-90.789688, 33.610022], [-90.789668, 33.610318], [-90.78966, 33.610722], [-90.789598, 33.612225], [-90.789593, 33.612305], [-90.78956, 33.612358], [-90.789475, 33.612365], [-90.789072, 33.61237], [-90.788643, 33.612367], [-90.787938, 33.612375], [-90.787155, 33.612393], [-90.785787, 33.612403], [-90.785132, 33.612425], [-90.784582, 33.612435], [-90.783712, 33.612472],     [-90.783634, 33.612466],     [-90.783647, 33.612467], [-90.783198, 33.612472], [-90.781774, 33.61249], [-90.78104, 33.612511], [-90.780976, 33.612288], [-90.781022, 33.612023], [-90.781033, 33.61179], [-90.781019, 33.611549], [-90.781033, 33.611299], [-90.781055, 33.610906], [-90.781055, 33.610575], [-90.781094, 33.610042], [-90.781084, 33.608534], [-90.781924, 33.608439], [-90.781946, 33.607715], [-90.782421, 33.607559], [-90.78367, 33.607845], [-90.783573, 33.609717], [-90.783741, 33.609384], [-90.784017, 33.607994], [-90.784507, 33.608018], [-90.785483, 33.608138], [-90.787171, 33.608301], [-90.789474, 33.608456]]]]}'
    gdal.FileFromMemBuffer(tmp_vsimem / "cutline.geojson", geojson)
    src_ds = gdal.GetDriverByName("MEM").Create("", 968, 751)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32615)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([690129, 30, 0, 3723432, 0, -30])
    ds = gdal.Warp(
        "", src_ds, format="MEM", cutlineDSName=tmp_vsimem / "cutline.geojson"
    )
    assert ds is not None


###############################################################################
# Test cutline with zero-width sliver


@pytest.mark.require_driver("GeoJSON")
def test_gdalwarp_lib_cutline_zero_width_sliver_remove_empty_polygon(tmp_vsimem):

    geojson = {
        "type": "MultiPolygon",
        "coordinates": [
            [
                [
                    [-101.43346, 36.91886],
                    [-101.43337, 36.91864],
                    [-101.43332, 36.91865],
                    [-101.43342, 36.91888],
                    [-101.43346, 36.91886],
                ]
            ],
            # The below polygon has a zero-width sliver
            [
                [
                    [-101.4311, 36.91909],
                    [-101.43106, 36.91913],
                    [-101.43111, 36.91908],
                    [-101.4311, 36.91909],
                ]
            ],
        ],
    }
    gdal.FileFromMemBuffer(tmp_vsimem / "cutline.geojson", json.dumps(geojson))
    src_ds = gdal.GetDriverByName("MEM").Create("", 100, 100)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([-101.5, 0.1, 0, 37, 0, -0.1])
    ds = gdal.Warp(
        "", src_ds, format="MEM", cutlineDSName=tmp_vsimem / "cutline.geojson"
    )
    assert ds is not None


###############################################################################
# Test cutline with zero-width sliver


@pytest.mark.require_driver("GeoJSON")
def test_gdalwarp_lib_cutline_zero_width_sliver_remove_empty_inner_ring(tmp_vsimem):

    geojson = {
        "type": "MultiPolygon",
        "coordinates": [
            [
                [
                    [-101.5, 37],
                    [-101.4, 37],
                    [-101.4, 36.9],
                    [-101.5, 36.9],
                    [-101.5, 37],
                ],
                # The below ring has a zero-width sliver
                [
                    [-101.4311, 36.91909],
                    [-101.43106, 36.91913],
                    [-101.43111, 36.91908],
                    [-101.4311, 36.91909],
                ],
                # This one is OK
                [
                    [-101.49, 36.95],
                    [-101.48, 36.95],
                    [-101.48, 36.94],
                    [-101.49, 36.94],
                    [-101.49, 36.95],
                ],
            ]
        ],
    }
    gdal.FileFromMemBuffer(tmp_vsimem / "cutline.geojson", json.dumps(geojson))
    src_ds = gdal.GetDriverByName("MEM").Create("", 100, 100)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([-101.6, 0.1, 0, 37.1, 0, -0.1])
    ds = gdal.Warp(
        "", src_ds, format="MEM", cutlineDSName=tmp_vsimem / "cutline.geojson"
    )
    assert ds is not None


###############################################################################
# Test support for propagating coordinate epoch


def test_gdalwarp_lib_propagating_coordinate_epoch():

    src_ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        options="-of MEM -a_srs EPSG:32611 -a_coord_epoch 2021.3",
    )
    ds = gdal.Warp("", src_ds, format="MEM")
    srs = ds.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3
    ds = None


###############################################################################
# Test support for -s_coord_epoch


@pytest.mark.require_proj(7, 2)
def test_gdalwarp_lib_s_coord_epoch():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.SetGeoTransform([120, 1e-7, 0, -40, 0, -1e-7])

    # ITRF2014 to GDA2020
    ds = gdal.Warp(
        "",
        src_ds,
        options="-of MEM -s_srs EPSG:9000 -s_coord_epoch 2030 -t_srs EPSG:7844",
    )
    srs = ds.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 0
    gt = ds.GetGeoTransform()
    assert abs(gt[0] - 120) > 1e-15 and abs(gt[0] - 120) < 1e-5
    assert abs(gt[3] - -40) > 1e-15 and abs(gt[3] - -40) < 1e-5
    ds = None


###############################################################################
# Test support for -s_coord_epoch


@pytest.mark.require_proj(7, 2)
def test_gdalwarp_lib_t_coord_epoch():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.SetGeoTransform([120, 1e-7, 0, -40, 0, -1e-7])

    # GDA2020 to ITRF2014
    ds = gdal.Warp(
        "",
        src_ds,
        options="-of MEM -t_srs EPSG:9000 -t_coord_epoch 2030 -s_srs EPSG:7844",
    )
    srs = ds.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2030.0
    gt = ds.GetGeoTransform()
    assert abs(gt[0] - 120) > 1e-15 and abs(gt[0] - 120) < 1e-5
    assert abs(gt[3] - -40) > 1e-15 and abs(gt[3] - -40) < 1e-5
    ds = None


###############################################################################
# Test automatic grid sampling


def test_gdalwarp_lib_automatic_grid_sampling():

    ds = gdal.Warp(
        "",
        "../gdrivers/data/small_world.tif",
        format="MEM",
        outputBounds=[-7655830, -6385994, 7152182, 8423302],
        dstSRS="+proj=laea +lat_0=48.514 +lon_0=-145.204 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs",
    )

    assert ds.GetGeoTransform() == pytest.approx(
        (-7655830.0, 30850.025, 0.0, 8423302.0, 0.0, -30852.7)
    )
    assert ds.GetRasterBand(1).Checksum() == 35573


###############################################################################
# Test source nodata with destination alpha


def test_gdalwarp_lib_src_nodata_with_dstalpha():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 3)
    src_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32615)
    src_ds.SetSpatialRef(srs)
    src_ds.GetRasterBand(1).SetNoDataValue(1)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, struct.pack("B" * 3, 10, 1, 1))
    src_ds.GetRasterBand(2).SetNoDataValue(2)
    src_ds.GetRasterBand(2).WriteRaster(0, 0, 3, 1, struct.pack("B" * 3, 20, 2, 2))
    src_ds.GetRasterBand(3).SetNoDataValue(3)
    src_ds.GetRasterBand(3).WriteRaster(0, 0, 3, 1, struct.pack("B" * 3, 30, 3, 127))

    # By default, a target pixel is invalid if all source pixels are invalid,
    # but when warping each band, its individual nodata status is taken into
    # account
    ds = gdal.Warp("", src_ds, format="MEM", dstAlpha=True)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert struct.unpack("B" * 3, ds.GetRasterBand(1).ReadRaster()) == (10, 0, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(2).ReadRaster()) == (20, 0, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(3).ReadRaster()) == (30, 0, 127)
    assert struct.unpack("B" * 3, ds.GetRasterBand(4).ReadRaster()) == (255, 0, 255)

    # Same as above
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        dstAlpha=True,
        warpOptions=["UNIFIED_SRC_NODATA=PARTIAL"],
    )
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert struct.unpack("B" * 3, ds.GetRasterBand(1).ReadRaster()) == (10, 0, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(2).ReadRaster()) == (20, 0, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(3).ReadRaster()) == (30, 0, 127)
    assert struct.unpack("B" * 3, ds.GetRasterBand(4).ReadRaster()) == (255, 0, 255)

    # In UNIFIED_SRC_NODATA=NO, target pixels will always be valid
    ds = gdal.Warp(
        "", src_ds, format="MEM", dstAlpha=True, warpOptions=["UNIFIED_SRC_NODATA=NO"]
    )
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert struct.unpack("B" * 3, ds.GetRasterBand(1).ReadRaster()) == (10, 0, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(2).ReadRaster()) == (20, 0, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(3).ReadRaster()) == (30, 0, 127)
    assert struct.unpack("B" * 3, ds.GetRasterBand(4).ReadRaster()) == (255, 255, 255)

    # In UNIFIED_SRC_NODATA=YES, a target pixel is invalid if all source pixels are invalid,
    # and the validty status of each band is determined by this unified validity
    ds = gdal.Warp(
        "", src_ds, format="MEM", dstAlpha=True, warpOptions=["UNIFIED_SRC_NODATA=YES"]
    )
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert struct.unpack("B" * 3, ds.GetRasterBand(1).ReadRaster()) == (10, 0, 1)
    assert struct.unpack("B" * 3, ds.GetRasterBand(2).ReadRaster()) == (20, 0, 2)
    assert struct.unpack("B" * 3, ds.GetRasterBand(3).ReadRaster()) == (30, 0, 127)
    assert struct.unpack("B" * 3, ds.GetRasterBand(4).ReadRaster()) == (255, 0, 255)

    # Specifying srcNoData implies UNIFIED_SRC_NODATA=YES
    ds = gdal.Warp("", src_ds, format="MEM", srcNodata="1 2 3", dstAlpha=True)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert struct.unpack("B" * 3, ds.GetRasterBand(1).ReadRaster()) == (10, 0, 1)
    assert struct.unpack("B" * 3, ds.GetRasterBand(2).ReadRaster()) == (20, 0, 2)
    assert struct.unpack("B" * 3, ds.GetRasterBand(3).ReadRaster()) == (30, 0, 127)
    assert struct.unpack("B" * 3, ds.GetRasterBand(4).ReadRaster()) == (255, 0, 255)


###############################################################################
# Test warping from a dataset with points outside of Earth (fixes #4934)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalwarp_lib_src_points_outside_of_earth():
    class MyHandler:
        def __init__(self):
            self.failure_raised = False

        def callback(self, err_type, err_no, err_msg):
            if err_type == gdal.CE_Failure:
                print(err_type, err_no, err_msg)
                self.failure_raised = True

    my_error_handler = MyHandler()
    with gdaltest.error_handler(my_error_handler.callback):
        gdal.Warp("", "../gdrivers/data/vrt/bug4997_intermediary.vrt", format="VRT")
    assert not my_error_handler.failure_raised


###############################################################################
# Test warping from a dataset in rotated pole projection, including the North
# pole to geographic


# Not completely sure about the minimum version to have ob_tran working fine.
@pytest.mark.require_proj(7)
def test_gdalwarp_lib_from_ob_tran_including_north_pole_to_geographic():

    ds = gdal.Warp(
        "",
        "../gdrivers/data/small_world.tif",
        format="VRT",
        dstSRS="+proj=ob_tran +o_proj=longlat +o_lon_p=189.477233886719 +o_lat_p=31.7581653594971 +lon_0=267.596992492676 +datum=WGS84 +no_defs",
        outputBounds=[32.4624074, -53.5375933, 327.538, 53.538],
        warpOptions=["SAMPLE_GRID=YES", "SOURCE_EXTRA=5"],
    )
    out_ds = gdal.Warp("", ds, format="VRT", dstSRS="EPSG:4326")
    gt = out_ds.GetGeoTransform()
    assert gt[0] == -180
    assert gt[3] == 90
    assert gt[0] + gt[1] * out_ds.RasterXSize == pytest.approx(180, abs=0.1)


###############################################################################
# Test warping from a dataset in geographic with longitudes outside [-180,180]
# to the same CRS doesn't alter the extent
# (https://github.com/OSGeo/gdal/issues/8194)


def test_gdalwarp_lib_geographic_outside_180_no_crs_change():

    src_ds = gdal.GetDriverByName("MEM").Create("", 36, 18)
    src_ds.SetGeoTransform([0, 10, 0, 90, 0, -10])
    srs = osr.SpatialReference()
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs.ImportFromEPSG(4326)
    src_ds.SetSpatialRef(srs)
    src_ds.GetRasterBand(1).Fill(1)

    ds = gdal.Warp("", src_ds, format="MEM", xRes=20, yRes=20)
    gt = ds.GetGeoTransform()
    assert ds.RasterXSize == 18
    assert ds.RasterYSize == 9
    assert gt == pytest.approx((0, 20, 0, 90, 0, -20))
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (1, 1)


###############################################################################
# Test gdalwarp foo.tif foo.tif.ovr


def test_gdalwarp_lib_generate_ovr(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "foo.tif", open("../gcore/data/byte.tif", "rb").read()
    )
    gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "foo.tif.ovr", 10, 10)
    ds = gdal.Warp(
        tmp_vsimem / "foo.tif.ovr",
        tmp_vsimem / "foo.tif",
        options="-of GTiff -r average -ts 10 10 -overwrite",
    )
    assert ds
    assert ds.GetRasterBand(1).Checksum() != 0, "Bad checksum"
    ds = None


###############################################################################
# Test not deleting auxiliary files shared by the source and a target being
# overwritten (https://github.com/OSGeo/gdal/issues/5633)


def test_gdalwarp_lib_not_delete_shared_auxiliary_files(tmp_path):

    img_foo_r1c1_jp2 = str(tmp_path / "IMG_foo_R1C1.JP2")
    img_foo_r1c1_tif = str(tmp_path / "IMG_foo_R1C1.tif")
    dim_foo_xml = str(tmp_path / "DIM_foo.XML")

    # Yes, we do intend to copy a .TIF as a fake .JP2
    shutil.copy("../gdrivers/data/dimap2/bundle/IMG_foo_R1C1.TIF", img_foo_r1c1_jp2)
    shutil.copy("../gdrivers/data/dimap2/bundle/DIM_foo.XML", dim_foo_xml)

    gdal.Warp(img_foo_r1c1_tif, img_foo_r1c1_jp2)

    ds = gdal.Open(img_foo_r1c1_tif)
    assert len(ds.GetFileList()) == 2
    ds = None

    gdal.Warp(img_foo_r1c1_tif, img_foo_r1c1_jp2, format="GTiff")

    ds = gdal.Open(img_foo_r1c1_tif)
    assert len(ds.GetFileList()) == 2
    ds = None


###############################################################################


def test_gdalwarp_lib_issue_with_te_and_geographic_crs_world_coverage():

    # Representative of georeferencing on gfs.t18z.pgrb2.0p25.f033
    src_ds = gdal.GetDriverByName("MEM").Create("", 1440, 721)
    srs = osr.SpatialReference()
    srs.SetFromUserInput("+proj=longlat +R=6371229")
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([-180.125, 0.25, 0, 90.125, 0, -0.25])
    out_ds = gdal.Warp(
        "", src_ds, format="MEM", dstSRS="EPSG:4326", outputBounds=[-180, -90, 180, 90]
    )
    # Check that the resolution is 0.25
    assert out_ds.GetGeoTransform() == (-180, 0.25, 0, 90, 0, -0.25)


###############################################################################


def test_gdalwarp_lib_epsg_4326_to_esri_53037():

    # Scenario of https://github.com/OSGeo/gdal/issues/6155
    src_ds = gdal.GetDriverByName("MEM").Create("", 10800, 5400)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([-180, 0.033333333333330, 0, 90, 0, -0.033333333333330])
    # Expension of ESRI:53037 (proj.db of old PROJ releases don't know it)
    out_ds = gdal.Warp(
        "",
        src_ds,
        format="VRT",
        dstSRS="+proj=eqearth +lon_0=150 +x_0=0 +y_0=0 +R=6371008.7714 +units=m +type=crs",
    )
    assert out_ds.GetGeoTransform() == pytest.approx(
        (
            -17243961.61291527,
            3176.540597910281,
            0.0,
            8392929.693707585,
            0.0,
            -3176.540597910281,
        )
    )


###############################################################################


@pytest.mark.parametrize(
    "resampleAlg", ["average", "mode", "min", "max", "med", "q1", "q3", "sum", "rms"]
)
def test_gdalwarp_lib_epsg_4326_to_esri_102020(resampleAlg):

    # Scenario of https://github.com/OSGeo/gdal/issues/6155
    out_ds = gdal.Warp(
        "",
        "../gdrivers/data/small_world.tif",
        options="-f MEM -t_srs ESRI:102020 -te -3000000 -3000000 3000000 0 -ts 300 150 -r "
        + resampleAlg,
    )
    # Test we don't have a weird spike at the antimeridian, "below" south pole
    if resampleAlg != "sum":
        assert struct.unpack("B" * 3, out_ds.ReadRaster(149, 100, 1, 1)) == (11, 10, 50)


###############################################################################


@pytest.mark.parametrize(
    "options",
    [
        "",
        "-ts 1 1",
        "-ts 10 10",
        "-ts 40 40",
        "-t_srs EPSG:4326",
        "-t_srs EPSG:4326 -ts 11 9",
        "-t_srs EPSG:4326 -ts 100 100",
    ],
)
def test_gdalwarp_lib_sum_preserving(options):

    src_ds = gdal.Open("../gdrivers/data/byte.tif")
    out_ds = gdal.Warp("", src_ds, options="-f MEM -ot Float32 -r sum " + options)
    source_values = struct.unpack(
        "B" * (src_ds.RasterXSize * src_ds.RasterYSize), src_ds.ReadRaster()
    )
    values = struct.unpack(
        "f" * (out_ds.RasterXSize * out_ds.RasterYSize), out_ds.ReadRaster()
    )
    assert sum(source_values) == pytest.approx(sum(values), rel=1e-5)

    # Check nodata handling
    minval = src_ds.GetRasterBand(1).ComputeRasterMinMax()[0]
    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-f MEM -ot Float32 -srcnodata "
        + str(minval)
        + " -dstnodata -12345 -r sum "
        + options,
    )
    source_values = struct.unpack(
        "B" * (src_ds.RasterXSize * src_ds.RasterYSize), src_ds.ReadRaster()
    )
    source_values = [x if x != minval else 0 for x in source_values]
    values = struct.unpack(
        "f" * (out_ds.RasterXSize * out_ds.RasterYSize), out_ds.ReadRaster()
    )
    values = [x if x != -12345 else 0 for x in values]
    assert sum(source_values) == pytest.approx(sum(values), rel=1e-5)


###############################################################################


def test_gdalwarp_lib_sum_preserving_multiband():

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    out_ds = gdal.Warp("", src_ds, options="-f MEM -ot Float32 -r sum -ts 500 300")
    for i in range(src_ds.RasterCount):
        source_values = struct.unpack(
            "B" * (src_ds.RasterXSize * src_ds.RasterYSize),
            src_ds.GetRasterBand(i + 1).ReadRaster(),
        )
        values = struct.unpack(
            "f" * (out_ds.RasterXSize * out_ds.RasterYSize),
            out_ds.GetRasterBand(i + 1).ReadRaster(),
        )
        assert sum(source_values) == pytest.approx(sum(values), rel=1e-5)


###############################################################################


def test_gdalwarp_lib_sum_preserving_across_antimeridian():

    src_ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        options="-f MEM -a_srs EPSG:32601 -a_ullr 165000 5000 170000 -5000",
    )
    out1_ds = gdal.Warp(
        "",
        src_ds,
        options="-f MEM -r sum -t_srs EPSG:4326 -overwrite -te 179.97 -0.05 180 0.05 -ot Float32",
    )
    out2_ds = gdal.Warp(
        "",
        src_ds,
        options="-f MEM -r sum -t_srs EPSG:4326 -overwrite -te -180 -0.05 -179.95 0.05 -ot Float32",
    )
    source_values = struct.unpack(
        "B" * (src_ds.RasterXSize * src_ds.RasterYSize), src_ds.ReadRaster()
    )
    values1 = struct.unpack(
        "f" * (out1_ds.RasterXSize * out1_ds.RasterYSize), out1_ds.ReadRaster()
    )
    values2 = struct.unpack(
        "f" * (out2_ds.RasterXSize * out2_ds.RasterYSize), out2_ds.ReadRaster()
    )
    assert sum(source_values) == pytest.approx(sum(values1) + sum(values2), rel=1e-5)


###############################################################################


def test_gdalwarp_lib_srcBands():

    # Test with single band input dataset
    ds = gdal.Warp("", "../gcore/data/byte.tif", format="MEM", srcBands=[1])
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = gdal.Warp(
        "", "../gcore/data/byte.tif", format="MEM", srcBands=[1], dstBands=[1]
    )
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = gdal.Warp("", "../gcore/data/byte.tif", format="MEM", srcBands=[1, 1])
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(2).Checksum() == 4672

    # Error: len(dstBands) != len(srcBands)
    with gdal.quiet_errors():
        with pytest.raises(Exception):
            gdal.Warp(
                "",
                "../gcore/data/byte.tif",
                format="MEM",
                srcBands=[1, 1],
                dstBands=[1],
            )

    # Invalid srcBands[0] value
    with pytest.raises(Exception):
        gdal.Warp("", "../gcore/data/byte.tif", format="MEM", srcBands=[2])

    # Invalid dstBands[0] value
    with pytest.raises(Exception):
        gdal.Warp(
            "", "../gcore/data/byte.tif", format="MEM", srcBands=[1], dstBands=[2]
        )

    # Test with RGB input dataset
    src_ds = gdal.Open("../gcore/data/rgbsmall.tif")
    ds = gdal.Warp("", src_ds, format="MEM", srcBands=[1])
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()

    ds = gdal.Warp("", src_ds, format="MEM", srcBands=[2])
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(2).Checksum()

    ds = gdal.Warp(
        "", src_ds, format="MEM", setColorInterpretation=True, srcBands=[2, 1]
    )
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(2).Checksum()
    assert (
        ds.GetRasterBand(1).GetColorInterpretation()
        == src_ds.GetRasterBand(2).GetColorInterpretation()
    )
    assert ds.GetRasterBand(2).Checksum() == src_ds.GetRasterBand(1).Checksum()

    ds = gdal.Warp("", src_ds, format="MEM", srcBands=[3, 2, 1])
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(3).Checksum()
    assert ds.GetRasterBand(2).Checksum() == src_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(3).Checksum() == src_ds.GetRasterBand(1).Checksum()

    ds = gdal.Warp("", src_ds, format="MEM", srcBands=[1, 2, 3], dstBands=[3, 2, 1])
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(3).Checksum()
    assert ds.GetRasterBand(2).Checksum() == src_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(3).Checksum() == src_ds.GetRasterBand(1).Checksum()

    ds = gdal.Warp("", src_ds, format="MEM", srcBands=[1, 2, 3])
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(2).Checksum() == src_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(3).Checksum() == src_ds.GetRasterBand(3).Checksum()

    ds = gdal.Warp("", src_ds, format="MEM", srcBands=[1, 2, 3, 1])
    assert ds.RasterCount == 4
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(2).Checksum() == src_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(3).Checksum() == src_ds.GetRasterBand(3).Checksum()
    assert ds.GetRasterBand(4).Checksum() == src_ds.GetRasterBand(1).Checksum()

    # Warp into existing dataset
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    assert gdal.Warp(ds, src_ds, srcBands=[2], dstBands=[3])
    assert ds.GetRasterBand(1).Checksum() == 0
    assert ds.GetRasterBand(2).Checksum() == 0
    assert ds.GetRasterBand(3).Checksum() == src_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(4).Checksum() == 0

    # Test with RGBA input dataset
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    ref_ds = gdal.Warp(
        "", src_ds, format="MEM", transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"]
    )

    # No srcAlpha nor dstAlpha ==> set implicitly
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
        srcBands=[2],
    )
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() == ref_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(2).Checksum() == ref_ds.GetRasterBand(4).Checksum()
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand

    # srcAlpha=True
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
        srcBands=[2],
        srcAlpha=True,
    )
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() == ref_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(2).Checksum() == ref_ds.GetRasterBand(4).Checksum()
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand

    # dstAlpha=True
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
        srcBands=[2],
        dstAlpha=True,
    )
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() == ref_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(2).Checksum() == ref_ds.GetRasterBand(4).Checksum()
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand

    # Disable alpha
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
        srcAlpha=False,
        srcBands=[2],
    )
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == ref_ds.GetRasterBand(2).Checksum()

    # srcAlpha=False, dstAlpha=True
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
        srcBands=[2],
        srcAlpha=False,
        dstAlpha=True,
    )
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() == ref_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(2).Checksum() != 0
    assert ds.GetRasterBand(2).Checksum() != ref_ds.GetRasterBand(4).Checksum()
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand


###############################################################################


def test_gdalwarp_lib_preserve_non_square_pixels_if_no_reprojection():

    src_ds = gdal.Translate("", "../gcore/data/byte.tif", options="-f MEM -tr 60 30")
    out_ds = gdal.Warp("", src_ds, format="MEM")
    assert out_ds.RasterXSize == src_ds.RasterXSize
    assert out_ds.RasterYSize == src_ds.RasterYSize
    assert out_ds.GetGeoTransform() == src_ds.GetGeoTransform()

    out_ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        outputBounds=[440720.000, 3750120.000, 441920.000, 3751320.000],
    )
    assert out_ds.RasterXSize == src_ds.RasterXSize
    assert out_ds.RasterYSize == src_ds.RasterYSize
    assert out_ds.GetGeoTransform() == src_ds.GetGeoTransform()


###############################################################################


def test_gdalwarp_lib_preserve_non_square_pixels_same_horizontal_crs():

    src_ds = gdal.GetDriverByName("MEM").Create("", 20, 10)
    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:4326+3855")
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.2])
    out_ds = gdal.Warp("", src_ds, options="-f MEM -t_srs EPSG:4979")
    assert out_ds.RasterXSize == src_ds.RasterXSize
    assert out_ds.RasterYSize == src_ds.RasterYSize
    assert out_ds.GetGeoTransform() == src_ds.GetGeoTransform()


###############################################################################


def test_gdalwarp_lib_preserve_non_square_pixels_if_no_reprojection_multi_sources():

    src_ds = gdal.Translate("", "../gcore/data/byte.tif", options="-f MEM -tr 60 30")
    left_ds = gdal.Translate("", src_ds, options="-f MEM -srcwin 0 0 10 40")
    right_ds = gdal.Translate("", src_ds, options="-f MEM -srcwin 10 0 10 40")
    out_ds = gdal.Warp("", [left_ds, right_ds], format="MEM")
    assert out_ds.RasterXSize == src_ds.RasterXSize
    assert out_ds.RasterYSize == src_ds.RasterYSize
    assert out_ds.GetGeoTransform() == src_ds.GetGeoTransform()

    out_ds = gdal.Warp(
        "",
        [left_ds, right_ds],
        format="MEM",
        outputBounds=[440720.000, 3750120.000, 441920.000, 3751320.000],
    )
    assert out_ds.RasterXSize == src_ds.RasterXSize
    assert out_ds.RasterYSize == src_ds.RasterYSize
    assert out_ds.GetGeoTransform() == src_ds.GetGeoTransform()


###############################################################################


def test_gdalwarp_lib_tr_square():

    src_ds = gdal.Translate("", "../gcore/data/byte.tif", options="-f MEM -tr 60 30")
    out_ds = gdal.Warp("", src_ds, options="-f MEM -tr square")
    gt = out_ds.GetGeoTransform()
    assert gt[1] == abs(gt[5])


###############################################################################
# Test that we auto enable OPTIMIZE_SIZE warping option when it is reasonable


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_gdalwarp_lib_auto_optimize_size(tmp_vsimem):

    src_ds = gdal.Translate(
        "", "../gcore/data/byte.tif", options="-f MEM -outsize 1500 1500 -r bilinear"
    )

    tmpfilename = tmp_vsimem / "test_gdalwarp_lib_auto_optimize_size.tif"
    # Warp to tiled GeoTIFF with low warp memory and block cache size, to
    # potentially exhibit we wouldn't write to output tile boundaries.
    with gdaltest.SetCacheMax(256 * 256):
        gdal.Warp(tmpfilename, src_ds, options="-co TILED=YES -co COMPRESS=JPEG -wm 1")

    # Warp to MEM and then translate to tiled GeoTIFF
    ref_ds_src = gdal.Warp("", src_ds, options="-f MEM")
    tmpfilename2 = tmp_vsimem / "test_gdalwarp_lib_auto_optimize_size2.tif"
    gdal.Translate(tmpfilename2, ref_ds_src, options="-co TILED=YES -co COMPRESS=JPEG")

    ds = gdal.Open(tmpfilename)
    ds_ref = gdal.Open(tmpfilename2)

    assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()


###############################################################################
# Test proper computation of working data type


def test_gdalwarp_lib_working_data_type_with_source_dataset_of_different_types():

    int16_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Int16)
    int16_ds.GetRasterBand(1).Fill(1)
    int16_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])

    float32_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    nd_value = struct.unpack("f", struct.pack("f", -3.4028235e38))[0]
    float32_ds.GetRasterBand(1).Fill(nd_value)
    float32_ds.GetRasterBand(1).SetNoDataValue(nd_value)
    float32_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])

    out_ds = gdal.Warp("", [int16_ds, float32_ds], options="-f MEM -dstnodata -32768")
    minval, maxval = out_ds.GetRasterBand(1).ComputeRasterMinMax()
    assert minval == 1
    assert maxval == 1

    out_ds = gdal.Warp("", [float32_ds, int16_ds], options="-f MEM -dstnodata -32768")
    minval, maxval = out_ds.GetRasterBand(1).ComputeRasterMinMax()
    assert minval == 1
    assert maxval == 1


###############################################################################
# Test scenario of https://github.com/OSGeo/gdal/issues/8163
# (we need GEOS for the logic to split the cutline geometry in two parts)


@pytest.mark.require_geos
@pytest.mark.require_driver("GeoJSON")
def test_gdalwarp_lib_cutline_crossing_antimeridian_in_EPSG_32601_and_raster_in_EPSG_4326(
    tmp_vsimem,
):

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32601)
    cutline_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(
        tmp_vsimem / "cutline.json"
    )
    cutline_lyr = cutline_ds.CreateLayer("cutline", srs=srs)
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((300000 7200000,409800 7200000,409800 7090200,300000 7090200,300000 7200000))"
        )
    )
    cutline_lyr.CreateFeature(f)
    cutline_ds = None

    ds = gdal.Warp(
        "",
        "../gdrivers/data/small_world.tif",
        format="MEM",
        cutlineDSName=tmp_vsimem / "cutline.json",
        cropToCutline=True,
    )

    assert ds.RasterXSize == 400
    assert ds.RasterYSize == 1
    assert ds.GetGeoTransform() == pytest.approx((-180.0, 0.9, 0.0, 64.8, 0.0, -0.9))
    # Check that left-most and right-most pixels are non zero
    assert struct.unpack("B", ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1))[0] == 11
    assert struct.unpack("B", ds.GetRasterBand(1).ReadRaster(1, 0, 1, 1))[0] == 0
    assert (
        struct.unpack("B", ds.GetRasterBand(1).ReadRaster(ds.RasterXSize - 2, 0, 1, 1))[
            0
        ]
        == 0
    )
    assert (
        struct.unpack("B", ds.GetRasterBand(1).ReadRaster(ds.RasterXSize - 1, 0, 1, 1))[
            0
        ]
        == 9
    )


###############################################################################
# Test option argument handling


def test_gdalwarp_lib_dict_arguments():

    opt = gdal.WarpOptions(
        "__RETURN_OPTION_LIST__",
        creationOptions=collections.OrderedDict(
            (("COMPRESS", "DEFLATE"), ("LEVEL", 4))
        ),
    )

    assert opt == ["-co", "COMPRESS=DEFLATE", "-co", "LEVEL=4"]

    opt = gdal.WarpOptions(
        "__RETURN_OPTION_LIST__",
        warpOptions=collections.OrderedDict(
            (("SKIP_NOSOURCE", "YES"), ("NUM_THREADS", 2))
        ),
    )

    assert opt == ["-wo", "SKIP_NOSOURCE=YES", "-wo", "NUM_THREADS=2"]


def test_gdalwarp_lib_str_arguments():

    opt = gdal.WarpOptions(
        "__RETURN_OPTION_LIST__",
        creationOptions="COMPRESS=DEFLATE",
        warpOptions="SKIP_NOSOURCE=YES",
    )

    assert opt == ["-wo", "SKIP_NOSOURCE=YES", "-co", "COMPRESS=DEFLATE"]


###############################################################################
# Test warping from long/lat to ortho


def test_gdalwarp_lib_long_lat_to_ortho():

    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", options="-f VRT -outsize 10800 5400"
    )
    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        width=2138,
        height=2063,
        outputBounds=[-6378137, 3178376, -3189246, 6356752],
        dstSRS="+proj=ortho +datum=WGS84 +units=m +no_defs",
    )
    assert ds.GetRasterBand(1).ReadRaster()[2100 + 600 * 2138] != 0


###############################################################################
# Test warping from ortho to long/lat


def test_gdalwarp_lib_ortho_to_long_lat():

    src_ds = gdal.Warp(
        "",
        "../gdrivers/data/small_world.tif",
        format="MEM",
        dstSRS="+proj=ortho +datum=WGS84 +units=m +no_defs",
    )

    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        dstSRS="EPSG:4326",
        # The fact that we have srcNoData set is a key for the heuristics
        # that detects edges of the projection domain
        srcNodata=0,
    )
    gt = ds.GetGeoTransform()
    assert gt[0] == pytest.approx(-90, abs=gt[1])
    assert gt[0] + gt[1] * ds.RasterXSize == pytest.approx(90, abs=gt[1])
    data1 = ds.GetRasterBand(1).ReadRaster()
    data2 = ds.GetRasterBand(2).ReadRaster()
    data3 = ds.GetRasterBand(3).ReadRaster()
    for j in range(ds.RasterYSize):
        max_val = max(
            data1[j * ds.RasterXSize],
            data2[j * ds.RasterXSize],
            data3[j * ds.RasterXSize],
        )
        assert max_val != 0, "line %d" % j

    ds = gdal.Warp(
        "",
        src_ds,
        format="MEM",
        dstSRS="EPSG:4326",
        resampleAlg=gdal.GRA_Cubic,
        # The fact that we have srcNoData set is a key for the heuristics
        # that detects edges of the projection domain
        srcNodata=0,
    )
    data1 = ds.GetRasterBand(1).ReadRaster()
    data2 = ds.GetRasterBand(2).ReadRaster()
    data3 = ds.GetRasterBand(3).ReadRaster()
    for j in range(ds.RasterYSize):
        max_val = max(
            data1[j * ds.RasterXSize],
            data2[j * ds.RasterXSize],
            data3[j * ds.RasterXSize],
        )
        assert max_val != 0, "line %d" % j


###############################################################################
# Test warping to a projection that has no inverse
# Note: this test will break if PROJ get support for inverse isea !
# Note: disabled since it will actually break with PROJ 9.5 which implements


@pytest.mark.require_proj(8, 0, 0)
@gdaltest.enable_exceptions()
def DISABLED_test_gdalwarp_lib_to_projection_without_inverse_method():

    with pytest.raises(Exception, match="No inverse operation"):
        gdal.Warp(
            "",
            "../gdrivers/data/byte.tif",
            format="MEM",
            dstSRS="+proj=isea +datum=WGS84 +no_defs",
        )

    with pytest.raises(Exception, match="No inverse operation"):
        gdal.Warp(
            "",
            "../gdrivers/data/byte.tif",
            format="MEM",
            dstSRS="+proj=isea +datum=WGS84 +no_defs",
            xRes=60,
            yRes=60,
            outputBounds=[
                -14787944.0360835,
                1216599.66706888,
                -14787210.0893576,
                1218250.2778614,
            ],
        )


def test_gdalwarp_lib_no_crash_on_none_dst():

    ds1 = gdal.Open("../gcore/data/byte.tif")
    with pytest.raises(Exception):
        gdal.Warp(None, ds1)


###############################################################################
# Test conflicting source metadata


def test_gdalwarp_lib_conflicting_source_metadata(tmp_vsimem):

    src_ds1 = gdal.Translate(
        "", "../gcore/data/byte.tif", options="-of MEM -mo FOO=BAR -mo BAR=BAZ"
    )
    src_ds2 = gdal.Translate(
        "", "../gcore/data/byte.tif", options="-of MEM -mo FOO=BAZ -mo BAR=BAZ"
    )

    out_ds = gdal.Warp("", [src_ds1, src_ds2], format="MEM")
    assert out_ds.GetMetadataItem("FOO") == "*"
    assert out_ds.GetMetadataItem("BAR") == "BAZ"

    out_ds = gdal.Warp("", [src_ds1, src_ds2], options="-of MEM -cvmd conflicting")
    assert out_ds.GetMetadataItem("FOO") == "conflicting"
    assert out_ds.GetMetadataItem("BAR") == "BAZ"


###############################################################################
# Test issue GH #9467


def test_target_extent_consistent_size():
    """Test issue GH #9467 where the output size is not consistent when using target extent
    with different input datasets having the same resolution and CRS but different extent.
    """

    # Create a source dataset with CRS 32613
    src_ds_1 = gdal.GetDriverByName("MEM").Create("", 10980, 10980)
    src_ds_1.SetProjection("EPSG:32613")
    src_ds_1.SetGeoTransform((300000.0, 10.0, 0.0, 3900000.0, 0.0, -10.0))

    src_ds_2 = gdal.GetDriverByName("MEM").Create("", 10980, 10980)
    src_ds_2.SetProjection("EPSG:32613")
    src_ds_2.SetGeoTransform((300000.0, 10.0, 0.0, 4000020.0, 0.0, -10.0))

    bbox = (
        -106.40856573808874,
        35.10139620198477,
        -105.92962613828232,
        35.51543260935861,
    )

    ds = gdal.Warp(
        "",
        [src_ds_1, src_ds_2],
        options=gdal.WarpOptions(
            multithread=True,
            dstSRS="EPSG:4326",
            outputBounds=bbox,
            resampleAlg="lanczos",
            dstNodata=0,
            srcNodata=0,
            format="MEM",
        ),
    )

    assert ds.GetGeoTransform() == pytest.approx(
        (
            -106.40856573808874,
            9.992480696983594e-05,
            0.0,
            35.51543260935861,
            0.0,
            -9.99363763876038e-05,
        )
    )
    assert ds.RasterXSize == 4793
    assert ds.RasterYSize == 4143

    # Create a source dataset with CRS 32613
    src_ds_1 = gdal.GetDriverByName("MEM").Create("", 10980, 10980)
    src_ds_1.SetProjection("EPSG:32613")
    src_ds_1.SetGeoTransform((399960.0, 10.0, 0.0, 4000020.0, 0.0, -10.0))

    src_ds_2 = gdal.GetDriverByName("MEM").Create("", 10980, 10980)
    src_ds_2.SetProjection("EPSG:32613")
    src_ds_2.SetGeoTransform((399960.0, 10.0, 0.0, 3900000.0, 0.0, -10.0))

    ds = gdal.Warp(
        "",
        [src_ds_1, src_ds_2],
        options=gdal.WarpOptions(
            multithread=True,
            dstSRS="EPSG:4326",
            outputBounds=bbox,
            resampleAlg="lanczos",
            dstNodata=0,
            srcNodata=0,
            format="MEM",
        ),
    )

    assert ds.GetGeoTransform() == pytest.approx(
        (
            -106.40856573808874,
            9.992480696983594e-05,
            0.0,
            35.51543260935861,
            0.0,
            -9.99363763876038e-05,
        )
    )

    assert ds.RasterXSize == 4793
    assert ds.RasterYSize == 4143


###############################################################################
# Test warping an image with [-180,180] longitude to [180 - X, 180 + X]


@pytest.mark.parametrize("extra_column", [False, True])
def test_gdalwarp_lib_minus_180_plus_180_to_span_over_180(tmp_vsimem, extra_column):

    dst_filename = str(tmp_vsimem / "out.tif")
    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    if extra_column:
        tmp_ds = gdal.GetDriverByName("MEM").Create(
            "", src_ds.RasterXSize + 1, src_ds.RasterYSize
        )
        tmp_ds.SetGeoTransform(src_ds.GetGeoTransform())
        tmp_ds.SetSpatialRef(src_ds.GetSpatialRef())
        tmp_ds.WriteRaster(
            0,
            0,
            src_ds.RasterXSize,
            src_ds.RasterYSize,
            src_ds.GetRasterBand(1).ReadRaster(),
        )
        tmp_ds.WriteRaster(
            src_ds.RasterXSize,
            0,
            1,
            src_ds.RasterYSize,
            src_ds.GetRasterBand(1).ReadRaster(0, 0, 1, src_ds.RasterYSize),
        )
        src_ds = tmp_ds
    out_ds = gdal.Warp(dst_filename, src_ds, outputBounds=[0, -90, 360, 90])
    # Check that east/west hemispheres have been switched
    assert out_ds.GetRasterBand(1).ReadRaster(
        0, 0, src_ds.RasterXSize // 2, src_ds.RasterYSize
    ) == src_ds.GetRasterBand(1).ReadRaster(
        src_ds.RasterXSize // 2, 0, src_ds.RasterXSize // 2, src_ds.RasterYSize
    )
    assert out_ds.GetRasterBand(1).ReadRaster(
        src_ds.RasterXSize // 2, 0, src_ds.RasterXSize // 2, src_ds.RasterYSize
    ) == src_ds.GetRasterBand(1).ReadRaster(
        0, 0, src_ds.RasterXSize // 2, src_ds.RasterYSize
    )


###############################################################################
# Test warping an image with [-180-something,180+something] longitude to
# WebMercator


@pytest.mark.parametrize("extra_column", [False, True])
def test_gdalwarp_lib_minus_180_plus_180_to_span_over_180_to_webmercator(
    tmp_path, extra_column
):

    dst_filename = tmp_path / "out.tif"
    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    if extra_column:
        tmp_ds = gdal.GetDriverByName("MEM").Create(
            "", src_ds.RasterXSize + 1, src_ds.RasterYSize
        )
        tmp_ds.SetGeoTransform(src_ds.GetGeoTransform())
        tmp_ds.SetSpatialRef(src_ds.GetSpatialRef())
        tmp_ds.WriteRaster(
            0,
            0,
            src_ds.RasterXSize,
            src_ds.RasterYSize,
            src_ds.GetRasterBand(1).ReadRaster(),
        )
        tmp_ds.WriteRaster(
            src_ds.RasterXSize,
            0,
            1,
            src_ds.RasterYSize,
            src_ds.GetRasterBand(1).ReadRaster(0, 0, 1, src_ds.RasterYSize),
        )
        src_ds = tmp_ds
    else:
        src_ds = gdal.Translate("", src_ds, format="MEM", bandList=[1])
    gt = list(src_ds.GetGeoTransform())
    gt[0] -= gt[1] / 2
    src_ds.SetGeoTransform(gt)
    out_ds = gdal.Warp(
        dst_filename,
        src_ds,
        dstSRS="EPSG:3857",
        outputBounds=[-20044030.997, -20037508.343, 20044201.984, 20037508.343],
    )
    assert out_ds.GetRasterBand(1).Checksum() == 47957


###############################################################################
# Test bugfix for https://lists.osgeo.org/pipermail/gdal-dev/2024-September/059512.html


@pytest.mark.parametrize("with_tap", [True, False])
def test_gdalwarp_lib_blank_edge_one_by_one(with_tap):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([6.8688, 0.0009, 0, 51.3747, 0, -0.0009])
    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_ds.SetSpatialRef(srs)
    options = "-f MEM -tr 1000 1000 -t_srs EPSG:32631"
    if with_tap:
        options += " -tap"
    out_ds = gdal.Warp("", src_ds, options=options)
    assert out_ds.RasterXSize == 1
    assert out_ds.RasterYSize == 1
    gt = out_ds.GetGeoTransform()
    if with_tap:
        assert gt == pytest.approx((769000.0, 1000.0, 0.0, 5699000.0, 0.0, -1000.0))
    else:
        assert gt == pytest.approx(
            (769234.6506516202, 1000.0, 0.0, 5698603.782217737, 0.0, -1000.0)
        )


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/10892


def test_gdalwarp_lib_average_ten_ten_to_one_one():

    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_ds.SetSpatialRef(srs)
    src_ds.GetRasterBand(1).Fill(1)
    out_ds = gdal.Warp(
        "", src_ds, width=1, height=1, resampleAlg=gdal.GRIORA_Average, format="MEM"
    )
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (1, 1)


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/10975


def test_gdalwarp_lib_src_is_geog_arc_second():

    out_ds = gdal.Warp(
        "",
        "data/geog_arc_second.tif",
        options="-f MEM -srcnodata 0 -te -81.5 28.5 -81.0 29.0 -t_srs EPSG:4326",
    )
    assert out_ds.RasterXSize == 5464
    assert out_ds.RasterYSize == 5464
    assert out_ds.GetRasterBand(1).Checksum() == 31856


###############################################################################
# Test GWKCubicResampleNoMasks4MultiBandT<Byte>()


def test_gdalwarp_lib_cubic_multiband_byte_4sample_optim():

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")

    # RGB only
    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-f MEM -tr 0.9 0.9 -te -10 40.1 8.9 59 -r cubic",
    )
    assert out_ds.RasterXSize == 21
    assert out_ds.RasterYSize == 21
    assert [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
        4785,
        4689,
        5007,
    ]

    # With dest alpha
    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-f MEM -tr 0.9 0.9 -te -10 40.1 8.9 59 -r cubic -dstalpha",
    )
    assert out_ds.RasterXSize == 21
    assert out_ds.RasterYSize == 21
    assert [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
        4785,
        4689,
        5007,
    ]
    assert out_ds.GetRasterBand(4).ComputeRasterMinMax() == (255, 255)

    # Test edge effects
    # (slightly change the target resolution so that the nearest approximation
    # doesn't kick in)
    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-f MEM -r cubic -tr 0.9000001 0.9000001 -wo XSCALE=1 -wo YSCALE=1",
    )
    assert out_ds.RasterXSize == 400
    assert out_ds.RasterYSize == 200
    assert out_ds.ReadRaster() == src_ds.ReadRaster()


###############################################################################
# Test GWKCubicResampleNoMasks4MultiBandT<GUInt16>()


def test_gdalwarp_lib_cubic_multiband_uint16_4sample_optim():

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    src_ds = gdal.Translate(
        "", src_ds, options="-f MEM -ot UInt16 -scale 0 255 0 65535"
    )

    # RGB only
    out_ds = gdal.Warp(
        "",
        src_ds,
        options="-f MEM -tr 0.9 0.9 -te -10 40.1 8.9 59 -r cubic",
    )
    out_ds = gdal.Translate("", out_ds, options="-f MEM -ot Byte -scale 0 65535 0 255")
    assert out_ds.RasterXSize == 21
    assert out_ds.RasterYSize == 21
    assert [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
        4785,
        4689,
        5007,
    ]


###############################################################################
# Test invalid values of INIT_DEST


@pytest.mark.parametrize("init_dest", ("NODATA", "32.6x"))
def test_gdalwarp_lib_init_dest_invalid(tmp_vsimem, init_dest):

    with pytest.raises(Exception, match="Error parsing INIT_DEST"):
        gdal.Warp(
            tmp_vsimem / "out.tif",
            "../gcore/data/byte.tif",
            outputBounds=(440000, 3750120, 441920, 3751320),
            warpOptions={"INIT_DEST": init_dest},
        )


def test_gdalwarp_lib_init_dest_nodata_invalid(tmp_vsimem):

    # TODO: switch from warning to failure in GDAL 3.12
    # with pytest.raises(Exception, match="NoData value was not defined"):
    with gdaltest.error_raised(gdal.CE_Warning, "NoData value was not defined"):
        gdal.Warp(
            tmp_vsimem / "out.tif",
            "../gcore/data/byte.tif",
            outputBounds=(440000, 3750120, 441920, 3751320),
            warpOptions={"INIT_DEST": "NO_DATA"},
        )


###############################################################################
# Test scenario of https://github.com/OSGeo/gdal/issues/11992


def test_gdalwarp_lib_init_dest_no_source_window_mem():

    # Extract a target area that is in space.
    with gdal.quiet_errors():
        ds = gdal.Warp(
            "",
            "../gdrivers/data/small_world.tif",
            options="-t_srs +proj=ortho -overwrite -te -6378137 6356000 -6378000 6356752 -ts 100 100 -of MEM -dstnodata 255",
        )
    assert ds.GetRasterBand(1).GetNoDataValue() == 255
    ds.GetRasterBand(1).SetNoDataValue(0)
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (255, 255)


###############################################################################
# Test ALLOW_BALLPARK=NO transformer option


def test_gdalwarp_lib_allow_ballpark_no():

    src_ds = gdal.Open("../gcore/data/byte.tif")

    with pytest.raises(
        Exception,
        match="Cannot find coordinate operations from `EPSG:26711' to `EPSG:4258'",
    ):
        gdal.Warp(
            "",
            src_ds,
            format="MEM",
            dstSRS="EPSG:4258",  # ETRS89
            transformerOptions={"ALLOW_BALLPARK": "NO"},
        )


###############################################################################
# Test ONLY_BEST=YES transformer option


def test_gdalwarp_lib_only_best_yes():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4746)  # PD/83
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([15, 1, 0, 47, 0, -1])

    with pytest.raises(
        Exception,
        match="Cannot find coordinate operations from `EPSG:4746' to `EPSG:4326'",
    ):
        gdal.Warp(
            "",
            src_ds,
            format="MEM",
            dstSRS="EPSG:4326",  # WGS 84
            transformerOptions={"ALLOW_BALLPARK": "NO", "ONLY_BEST": "YES"},
        )


###############################################################################
# Test that we warn if different coordinate operations are used


@gdaltest.disable_exceptions()
@pytest.mark.require_proj(9, 1)
def test_gdalwarp_lib_warn_different_coordinate_operations():
    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4267)  # NAD27
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([-100, 2, 0, 60, 0, -2])

    with gdal.quiet_errors():
        gdal.Warp(
            "",
            src_ds,
            format="MEM",
            dstSRS="EPSG:4326",  # WGS 84
        )
        assert (
            gdal.GetLastErrorMsg()
            == "Several coordinate operations are going to be used. Artifacts may appear. You may consider using the -to ALLOW_BALLPARK=NO and/or -to ONLY_BEST=YES transform options, or specify a particular coordinate operation with -ct"
        )


###############################################################################
# Test that invalid NoData values cause an error


def test_gdalwarp_lib_invalid_dstnodata(tmp_vsimem):

    with pytest.raises(RuntimeError, match="Error parsing dstnodata"):
        gdal.Warp(tmp_vsimem / "out.tif", "../gcore/data/byte.tif", dstNodata="bad")


def test_gdalwarp_lib_invalid_srcnodata(tmp_vsimem):

    with pytest.raises(RuntimeError, match="Error parsing srcnodata"):
        gdal.Warp(tmp_vsimem / "out.tif", "../gcore/data/byte.tif", srcNodata="bad")


###############################################################################


def test_gdalwarp_lib_int_max_sized_raster(tmp_vsimem):

    content = """<VRTDataset rasterXSize="2147483647" rasterYSize="2147483647">
  <VRTRasterBand dataType="Byte" band="1" />
</VRTDataset>"""

    with pytest.raises(Exception, match="Too large output raster size"):
        gdal.Warp(
            tmp_vsimem / "out.tif",
            content,
            transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
            xRes=0.5,
            yRes=0.5,
        )

    with pytest.raises(Exception, match="Too large output raster size"):
        gdal.Warp(
            tmp_vsimem / "out.tif",
            content,
            transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
            xRes=0.5,
            yRes=0.5,
            outputBounds=[0, 0, 4e9, 4e9],
        )

    with pytest.raises(Exception, match="Too large output raster size"):
        gdal.Warp(
            tmp_vsimem / "out.tif",
            content,
            transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
            outputBounds=[0, 0, 4e9, 4e9],
        )

    content = """<VRTDataset rasterXSize="1" rasterYSize="2147483647">
  <VRTRasterBand dataType="Byte" band="1" />
</VRTDataset>"""

    with pytest.raises(Exception, match="Too large output raster size"):
        gdal.Warp(
            tmp_vsimem / "out.tif",
            content,
            transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
            width=2147483647,
        )

    content = """<VRTDataset rasterXSize="2147483647" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1" />
</VRTDataset>"""

    with pytest.raises(Exception, match="Too large output raster size"):
        gdal.Warp(
            tmp_vsimem / "out.tif",
            content,
            transformerOptions=["SRC_METHOD=NO_GEOTRANSFORM"],
            height=2147483647,
        )


###############################################################################
# Check fix for https://github.com/OSGeo/gdal/issues/12583


def test_gdalwarp_te_srs_check_extent():

    out_ds = gdal.Warp(
        "",
        "../gdrivers/data/small_world.tif",
        options="-te 0 -90 6 0 -te_srs EPSG:4326 -t_srs EPSG:32631 -f MEM",
    )
    assert out_ds.RasterXSize == 18
    assert out_ds.RasterYSize == 273
    assert out_ds.GetGeoTransform() == pytest.approx(
        (166021, 37108, 0.0, 0.0, 0.0, -36622), abs=1000
    )


###############################################################################
# Check fix for https://github.com/OSGeo/gdal/issues/12965


def test_gdalwarplib_on_huge_raster():

    src_ds = gdal.Open("""<VRTDataset rasterXSize="1073741766" rasterYSize="1070224430">
  <SRS dataAxisToSRSAxisMapping="1,2">PROJCS["WGS 84 / Pseudo-Mercator",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Mercator_1SP"],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs"],AUTHORITY["EPSG","3857"]]</SRS>
  <GeoTransform> -2.0037507260426737e+07,  3.7322767705947384e-02,  0.0000000000000000e+00,  1.9971868903190855e+07,  0.0000000000000000e+00, -3.7322767705947384e-02</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">invalid</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    out_ds = gdal.Warp(
        "",
        src_ds,
        options='-f VRT -t_srs "+proj=laea +lon_0=2.3 +lat_0=-40 +datum=WGS84" -ts 24 0 -te -4000 -4000 4000 4000',
    )
    assert out_ds.RasterXSize == 24
    assert out_ds.RasterYSize == 24


###############################################################################
# Just reflect the current behavior. We might decide to adopt a new behavior


def test_gdalwarp_lib_mask_band_and_src_nodata():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    src_ds.GetRasterBand(1).SetNoDataValue(2)
    src_ds.GetRasterBand(1).Fill(2)
    src_ds.GetRasterBand(1).GetMaskBand().Fill(255)

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Source dataset has both a per-dataset mask band and the warper has been also configured with a source nodata value. Only taking into account the latter",
    ):
        out_ds = gdal.Warp("", src_ds, options="-f MEM -dstnodata 5")
        assert out_ds.GetRasterBand(1).ReadRaster() == b"\x05"


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/13539


def test_gdalwarp_lib_sum_preserving_non_discontinuity(tmp_vsimem):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "in.tif", 17857, 8472, 1, gdal.GDT_Float64
    )
    src_ds.GetRasterBand(1).SetNoDataValue(float("nan"))
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    dstX = 2231
    dstY = 2208
    srcX = dstX * 2
    srcY = dstY * 2
    src_val = 1.5
    src_ds.WriteRaster(srcX, srcY, 2, 2, struct.pack("d", src_val) * (2 * 2))

    # When the bug occurred, this pixel (among others) was erroneously taken
    # into account
    src_ds.WriteRaster(4461, 4236, 1, 1, struct.pack("d", 1e300))

    out_ds = gdal.Warp("", src_ds, options="-of VRT -tr 2 2 -r sum -wm 60")
    out_xoff = 1116
    out_yoff = 2118
    out_xsize = 1116
    out_ysize = 1059
    got_data = out_ds.ReadRaster(out_xoff, out_yoff, out_xsize, out_ysize)
    assert (
        array.array("d", got_data)[(dstY - out_yoff) * out_xsize + (dstX - out_xoff)]
        == 4 * src_val
    )


###############################################################################
# Test RESET_DEST_PIXELS warping option


@pytest.mark.parametrize("dstNodata", [255, None])
def test_gdalwarp_lib_RESET_DEST_PIXELS(dstNodata):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.GetRasterBand(1).Fill(1)

    out_ds = gdal.Warp("", src_ds, format="MEM", dstNodata=dstNodata)

    assert out_ds.ReadRaster() == b"\x01" * (3 * 3)

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([1, 1, 0, -1, 0, -1])
    src_ds.GetRasterBand(1).Fill(2)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src2_ds.SetGeoTransform([2, 1, 0, -1, 0, -1])
    src2_ds.GetRasterBand(1).Fill(3)

    gdal.Warp(out_ds, [src_ds, src2_ds], warpOptions={"RESET_DEST_PIXELS": "YES"})

    if dstNodata is None:
        assert out_ds.ReadRaster() == b"\x00\x00\x00\x00\x02\x03\x00\x00\x00"
    else:
        assert out_ds.ReadRaster() == b"\xff\xff\xff\xff\x02\x03\xff\xff\xff"
