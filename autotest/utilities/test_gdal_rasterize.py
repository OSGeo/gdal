#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_rasterize testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

import pytest

sys.path.append("../gcore")

import gdaltest
import test_cli_utilities

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdal_rasterize_path() is None,
    reason="gdal_rasterize not available",
)


@pytest.fixture()
def gdal_rasterize_path():
    return test_cli_utilities.get_gdal_rasterize_path()


###############################################################################
# Simple polygon rasterization (adapted from alg/rasterize.py).


@pytest.mark.require_driver("MapInfo File")
def test_gdal_rasterize_1(gdal_rasterize_path, tmp_path):

    output_tif = str(tmp_path / "rast1.tif")
    input_tab = str(tmp_path / "rast1.tab")

    # Setup working spatial reference
    # sr_wkt = 'LOCAL_CS["arbitrary"]'
    # sr = osr.SpatialReference( sr_wkt )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    sr_wkt = sr.ExportToWkt()

    # Create a raster to rasterize into.

    target_ds = gdal.GetDriverByName("GTiff").Create(
        output_tif, 100, 100, 3, gdal.GDT_UInt8
    )
    target_ds.SetGeoTransform((1000, 1, 0, 1100, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Close TIF file
    target_ds = None

    # Create a layer to rasterize from.

    rast_ogr_ds = ogr.GetDriverByName("MapInfo File").CreateDataSource(input_tab)
    rast_lyr = rast_ogr_ds.CreateLayer("rast1", srs=sr)

    rast_lyr.GetLayerDefn()
    field_defn = ogr.FieldDefn("foo")
    rast_lyr.CreateField(field_defn)

    # Add a polygon.

    wkt_geom = "POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))"

    feat = ogr.Feature(rast_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))

    rast_lyr.CreateFeature(feat)

    # Add feature without geometry to test fix for #3310
    feat = ogr.Feature(rast_lyr.GetLayerDefn())
    rast_lyr.CreateFeature(feat)

    # Add a linestring.

    wkt_geom = "LINESTRING(1000 1000, 1100 1050)"

    feat = ogr.Feature(rast_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))

    rast_lyr.CreateFeature(feat)

    # Close file
    rast_ogr_ds.Close()

    # Run the algorithm.
    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_rasterize_path} -b 3 -b 2 -b 1 -burn 200 -burn 220 -burn 240 -l rast1 {input_tab} {output_tif}"
    )
    assert err is None or err == "", "got error/warning"

    # Check results.

    target_ds = gdal.Open(output_tif)
    expected = 6452
    checksum = target_ds.GetRasterBand(2).Checksum()
    assert checksum == expected, "Did not get expected image checksum"

    target_ds = None


###############################################################################
# Test rasterization with ALL_TOUCHED (adapted from alg/rasterize.py).


@pytest.mark.require_driver("CSV")
def test_gdal_rasterize_2(gdal_rasterize_path, tmp_path):

    output_tif = str(tmp_path / "rast2.tif")

    # Create a raster to rasterize into.
    target_ds = gdal.GetDriverByName("GTiff").Create(
        output_tif, 12, 12, 3, gdal.GDT_UInt8
    )
    target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))

    # Close TIF file
    target_ds = None

    # Run the algorithm.
    gdaltest.runexternal(
        f"{gdal_rasterize_path} -at -b 3 -b 2 -b 1 -burn 200 -burn 220 -burn 240 -l cutline ../alg/data/cutline.csv {output_tif}"
    )

    # Check results.

    target_ds = gdal.Open(output_tif)
    expected = 121
    checksum = target_ds.GetRasterBand(2).Checksum()
    assert checksum == expected, "Did not get expected image checksum"

    target_ds = None


###############################################################################
# Test creating an output file


def test_gdal_rasterize_3(gdal_rasterize_path, tmp_path):

    contour_shp = str(tmp_path / "n43dt0.shp")
    output_tif = str(tmp_path / "n43dt0.tif")

    if test_cli_utilities.get_gdal_contour_path() is None:
        pytest.skip("gdal_contour missing")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_contour_path()
        + f" ../gdrivers/data/n43.tif {contour_shp} -i 10 -3d"
    )

    gdaltest.runexternal(
        gdal_rasterize_path
        + f" -3d {contour_shp} {output_tif} -l n43dt0 -ts 121 121 -a_nodata 0 -q"
    )

    ds_ref = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.Open(output_tif)

    assert (
        ds.GetRasterBand(1).GetNoDataValue() == 0.0
    ), "did not get expected nodata value"

    assert (
        ds.RasterXSize == 121 and ds.RasterYSize == 121
    ), "did not get expected dimensions"

    gt_ref = ds_ref.GetGeoTransform()
    gt = ds.GetGeoTransform()
    for i in range(6):
        assert gt[i] == pytest.approx(
            gt_ref[i], abs=1e-6
        ), "did not get expected geotransform"

    wkt = ds.GetProjectionRef()
    assert wkt.find("WGS_1984") != -1, "did not get expected SRS"
    ds = None


###############################################################################
# Same but with -tr argument


def test_gdal_rasterize_4(gdal_rasterize_path, tmp_path):

    contour_shp = str(tmp_path / "n43dt0.shp")
    output_tif = str(tmp_path / "n43dt0.tif")

    if test_cli_utilities.get_gdal_contour_path() is None:
        pytest.skip("gdal_contour missing")

    gdaltest.runexternal(
        test_cli_utilities.get_gdal_contour_path()
        + f" ../gdrivers/data/n43.tif {contour_shp} -i 10 -3d"
    )

    gdaltest.runexternal(
        gdal_rasterize_path
        + f" -3d {contour_shp} {output_tif} -l n43dt0 -tr 0.008333333333333  0.008333333333333 -a_nodata 0 -a_srs EPSG:4326"
    )

    ds_ref = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.Open(output_tif)

    assert (
        ds.GetRasterBand(1).GetNoDataValue() == 0.0
    ), "did not get expected nodata value"

    # Allow output to grow by 1/2 cell, as per #6058
    assert (
        ds.RasterXSize == 122 and ds.RasterYSize == 122
    ), "did not get expected dimensions"

    gt_ref = ds_ref.GetGeoTransform()
    gt = ds.GetGeoTransform()
    assert gt[1] == pytest.approx(gt_ref[1], abs=1e-6) and gt[5] == pytest.approx(
        gt_ref[5], abs=1e-6
    ), "did not get expected geotransform(dx/dy)"

    # Allow output to grow by 1/2 cell, as per #6058
    assert (
        abs(gt[0] + (gt[1] / 2) - gt_ref[0]) <= 1e-6
        and abs(gt[3] + (gt[5] / 2) - gt_ref[3]) <= 1e-6
    ), "did not get expected geotransform"

    wkt = ds.GetProjectionRef()
    assert wkt.find("WGS_1984") != -1, "did not get expected SRS"
    ds = None


###############################################################################
# Test point rasterization (#3774)


@pytest.mark.require_driver("CSV")
def test_gdal_rasterize_5(gdal_rasterize_path, tmp_path):

    input_csv = str(tmp_path / "test_gdal_rasterize_5.csv")
    output_tif = str(tmp_path / "test_gdal_rasterize_5.tif")

    f = open(input_csv, "wb")
    f.write("""x,y,Value
0.5,0.5,1
0.5,2.5,2
2.5,2.5,3
2.5,0.5,4
1.5,1.5,5""".encode("ascii"))
    f.close()

    gdaltest.runexternal(
        gdal_rasterize_path
        + " -l test_gdal_rasterize_5 -oo X_POSSIBLE_NAMES=x -oo Y_POSSIBLE_NAMES=y "
        + f" {input_csv} {output_tif} "
        + " -a Value -tr 1 1 -ot Byte"
    )

    ds = gdal.Open(output_tif)
    assert (
        ds.RasterXSize == 3 and ds.RasterYSize == 3
    ), "did not get expected dimensions"

    gt_ref = [0, 1, 0, 3, 0, -1]
    gt = ds.GetGeoTransform()
    for i in range(6):
        assert gt[i] == pytest.approx(
            gt_ref[i], abs=1e-6
        ), "did not get expected geotransform"

    data = ds.GetRasterBand(1).ReadRaster(0, 0, 3, 3)
    assert (
        data.decode("iso-8859-1") == "\x02\x00\x03\x00\x05\x00\x01\x00\x04"
    ), "did not get expected values"

    ds = None


###############################################################################
# Test on the fly reprojection of input data


@pytest.mark.require_driver("CSV")
def test_gdal_rasterize_6(gdal_rasterize_path, tmp_path):

    input_csv = str(tmp_path / "test_gdal_rasterize_6.csv")
    input_prj = str(tmp_path / "test_gdal_rasterize_6.prj")
    output_tif = str(tmp_path / "test_gdal_rasterize_6.tif")

    f = open(input_csv, "wb")
    f.write("""WKT,Value
"POLYGON((2 49,2 50,3 50,3 49,2 49))",255
""".encode("ascii"))
    f.close()

    f = open(input_prj, "wb")
    f.write("""EPSG:4326""".encode("ascii"))
    f.close()

    ds = gdal.GetDriverByName("GTiff").Create(output_tif, 100, 100)
    ds.SetGeoTransform(
        [200000, (400000 - 200000) / 100, 0, 6500000, 0, -(6500000 - 6200000) / 100]
    )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    gdaltest.runexternal(
        gdal_rasterize_path
        + f" -l test_gdal_rasterize_6 {input_csv} {output_tif} -a Value"
    )

    ds = gdal.Open(output_tif)
    assert ds.GetRasterBand(1).Checksum() == 39190, "did not get expected checksum"

    ds = None


###############################################################################
# Test SQLITE dialect in SQL


@pytest.mark.require_driver("SQLite")
@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("sql_in_file", [False, True])
def test_gdal_rasterize_7(gdal_rasterize_path, sql_in_file, tmp_path):

    gdaltest.importorskip_gdal_array()

    input_csv = str(tmp_path / "test_gdal_rasterize_7.csv")
    output_tif = str(tmp_path / "test_gdal_rasterize_7.tif")
    sql_txt = str(tmp_path / "sql.txt")

    try:
        drv = ogr.GetDriverByName("SQLITE")
        drv.CreateDataSource("/vsimem/foo.db", options=["SPATIALITE=YES"])
        gdal.Unlink("/vsimem/foo.db")
    except Exception:
        pytest.skip("Spatialite not available")

    f = open(input_csv, "wb")
    x = (0, 0, 50, 50, 25)
    y = (0, 50, 0, 50, 25)
    f.write("WKT,Value\n".encode("ascii"))
    for i, xi in enumerate(x):
        r = "POINT(%d %d),1\n" % (xi, y[i])
        f.write(r.encode("ascii"))

    f.close()

    sql = "SELECT ST_Buffer(GEOMETRY, 2) FROM test_gdal_rasterize_7"
    if sql_in_file:
        open(sql_txt, "wt").write(sql)
        sql = f"@{sql_txt}"
    else:
        sql = '"' + sql + '"'
    cmds = (
        f"{input_csv} "
        + f"{output_tif} "
        + "-init 0 -burn 1 "
        + f"-sql {sql} "
        + "-dialect sqlite -tr 1 1 -te -1 -1 51 51"
    )

    gdaltest.runexternal(gdal_rasterize_path + " " + cmds)

    ds = gdal.Open(output_tif)
    data = ds.GetRasterBand(1).ReadAsArray()
    assert data.sum() > 5, "Only rasterized 5 pixels or less."

    ds = None


###############################################################################
# Make sure we create output that encompasses all the input points on a point
# layer, #6058.


@pytest.mark.require_driver("CSV")
def test_gdal_rasterize_8(gdal_rasterize_path, tmp_path):

    input_csv = str(tmp_path / "test_gdal_rasterize_8.csv")
    output_tif = str(tmp_path / "test_gdal_rasterize_8.tif")

    f = open(input_csv, "wb")
    f.write("WKT,Value\n".encode("ascii"))
    f.write('"LINESTRING (0 0, 5 5, 10 0, 10 10)",1'.encode("ascii"))
    f.close()

    cmds = f"""{input_csv} {output_tif} -tr 1 1 -init 0 -burn 1"""

    gdaltest.runexternal(gdal_rasterize_path + " " + cmds)

    ds = gdal.Open(output_tif)
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 21, "Did not rasterize line data properly"

    ds = None


###############################################################################
# Test that -ts also accepts double and warns if not integer


@pytest.mark.require_driver("CSV")
def test_gdal_rasterize_ts_1(tmp_path, gdal_rasterize_path):

    output_tif = str(tmp_path / "rast2.tif")

    # Create a raster to rasterize into.
    target_ds = gdal.GetDriverByName("GTiff").Create(
        output_tif, 12, 12, 3, gdal.GDT_UInt8
    )
    target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))

    # Close TIF file
    target_ds = None

    # Run the algorithm.
    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_rasterize_path} -at -burn 200 -ts 100.0 200.0 ../alg/data/cutline.csv {output_tif}"
    )
    assert err is None or err == "", f"got error/warning {err}"

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_rasterize_path} -at -burn 200 -ts 100.4 200.6 ../alg/data/cutline.csv {output_tif}"
    )
    assert "-ts values parsed as 100 200" in err
