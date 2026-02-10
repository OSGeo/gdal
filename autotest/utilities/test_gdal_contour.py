#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_contour testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import struct

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdal_contour_path() is None,
    reason="gdal_contour not available",
)


@pytest.fixture()
def gdal_contour_path():
    return test_cli_utilities.get_gdal_contour_path()


@pytest.fixture()
def testdata_tif(tmp_path):
    tif_fname = str(tmp_path / "gdal_contour.tif")

    drv = gdal.GetDriverByName("GTiff")
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    wkt = sr.ExportToWkt()

    size = 160
    precision = 1.0 / size

    ds = drv.Create(tif_fname, size, size, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([1, precision, 0, 50, 0, -precision])

    raw_data = struct.pack("h", 10) * int(size / 2)
    for i in range(int(size / 2)):
        ds.WriteRaster(
            int(size / 4),
            i + int(size / 4),
            int(size / 2),
            1,
            raw_data,
            buf_type=gdal.GDT_Int16,
            band_list=[1],
        )

    raw_data = struct.pack("h", 20) * int(size / 2)
    for i in range(int(size / 4)):
        ds.WriteRaster(
            int(size / 4) + int(size / 8),
            i + int(size / 4) + int(size / 8),
            int(size / 4),
            1,
            raw_data,
            buf_type=gdal.GDT_Int16,
            band_list=[1],
        )

    raw_data = struct.pack("h", 25) * int(size / 4)
    for i in range(int(size / 8)):
        ds.WriteRaster(
            int(size / 4) + int(size / 8) + int(size / 16),
            i + int(size / 4) + int(size / 8) + int(size / 16),
            int(size / 8),
            1,
            raw_data,
            buf_type=gdal.GDT_Int16,
            band_list=[1],
        )

    ds = None

    yield tif_fname


###############################################################################
# Test with -a and -i options


def test_gdal_contour_1(gdal_contour_path, testdata_tif, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_contour_path + f" -a elev -i 10 {testdata_tif} {contour_shp}"
    )

    assert err is None or err == "", "got error/warning %s" % err

    ds = ogr.Open(contour_shp)

    expected_envelopes = [
        [1.253125, 1.746875, 49.253125, 49.746875],
        [1.378125, 1.621875, 49.378125, 49.621875],
    ]
    expected_height = [10, 20]

    with ds.ExecuteSQL("select * from contour order by elev asc") as lyr:

        raster_srs_wkt = gdal.Open(testdata_tif).GetSpatialRef().ExportToWkt()

        assert (
            lyr.GetSpatialRef().ExportToWkt() == raster_srs_wkt
        ), "Did not get expected spatial ref"

        assert lyr.GetFeatureCount() == len(expected_envelopes)

        size = 160
        precision = 1.0 / size

        i = 0
        for feat in lyr:
            geom = feat.GetGeometryRef()
            envelope = geom.GetEnvelope()
            assert feat.GetField("elev") == expected_height[i]
            for j in range(4):
                if expected_envelopes[i][j] != pytest.approx(envelope[j], rel=1e-8):
                    print("i=%d, wkt=%s" % (i, geom.ExportToWkt()))
                    print(geom.GetEnvelope())
                    pytest.fail(
                        "%f, %f"
                        % (expected_envelopes[i][j] - envelope[j], precision / 2)
                    )
            i = i + 1


###############################################################################
# Test with -fl option and -3d option


def test_gdal_contour_2(gdal_contour_path, testdata_tif, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    # put -3d just after -fl to test #2793
    _, err = gdaltest.runexternal_out_and_err(
        gdal_contour_path + f" -a elev -fl 10 20 25 -3d {testdata_tif} {contour_shp}"
    )
    assert not err

    size = 160
    precision = 1.0 / size

    ds = ogr.Open(contour_shp)

    expected_envelopes = [
        [1.25, 1.75, 49.25, 49.75],
        [1.25 + 0.125, 1.75 - 0.125, 49.25 + 0.125, 49.75 - 0.125],
        [
            1.25 + 0.125 + 0.0625,
            1.75 - 0.125 - 0.0625,
            49.25 + 0.125 + 0.0625,
            49.75 - 0.125 - 0.0625,
        ],
    ]
    expected_height = [10, 20, 25]

    with ds.ExecuteSQL("select * from contour order by elev asc") as lyr:

        assert lyr.GetFeatureCount() == len(expected_envelopes)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetGeometryRef().GetZ(0) == expected_height[i]
            envelope = feat.GetGeometryRef().GetEnvelope()
            assert feat.GetField("elev") == expected_height[i]
            for j in range(4):
                if expected_envelopes[i][j] != pytest.approx(
                    envelope[j], abs=precision / 2 * 1.001
                ):
                    print("i=%d, wkt=%s" % (i, feat.GetGeometryRef().ExportToWkt()))
                    print(feat.GetGeometryRef().GetEnvelope())
                    pytest.fail(
                        "%f, %f"
                        % (expected_envelopes[i][j] - envelope[j], precision / 2)
                    )
            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test on a real DEM


def test_gdal_contour_3(gdal_contour_path, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    # put -3d just after -fl to test #2793
    gdaltest.runexternal(
        gdal_contour_path + f" -a elev -i 50 ../gdrivers/data/n43.tif {contour_shp}"
    )

    ds = ogr.Open(contour_shp)

    with ds.ExecuteSQL("select distinct elev from contour order by elev asc") as lyr:

        expected_heights = [100, 150, 200, 250, 300, 350, 400, 450]
        assert lyr.GetFeatureCount() == len(expected_heights)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetField("elev") == expected_heights[i]
            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test contour orientation


def test_gdal_contour_4(gdal_contour_path, tmp_path):

    contour_orientation1_shp = str(tmp_path / "contour_orientation1.shp")
    contour_orientation_tif = str(tmp_path / "contour_orientation.tif")

    drv = gdal.GetDriverByName("GTiff")
    wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]]'

    size = 160
    precision = 1.0 / size

    ds = drv.Create(contour_orientation_tif, size, size, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([1, precision, 0, 50, 0, -precision])

    # Make the elevation 15 for the whole image
    raw_data = struct.pack("h", 15) * size
    for i in range(int(size)):
        ds.WriteRaster(
            0, i, int(size), 1, raw_data, buf_type=gdal.GDT_Int16, band_list=[1]
        )

    # Create a hill with elevation 25
    raw_data = struct.pack("h", 25) * 2
    for i in range(2):
        ds.WriteRaster(
            int(size / 4) + int(size / 8) - 1,
            i + int(size / 2) - 1,
            2,
            1,
            raw_data,
            buf_type=gdal.GDT_Int16,
            band_list=[1],
        )

    # Create a depression with elevation 5
    raw_data = struct.pack("h", 5) * 2
    for i in range(2):
        ds.WriteRaster(
            int(size / 2) + int(size / 8) - 1,
            i + int(size / 2) - 1,
            2,
            1,
            raw_data,
            buf_type=gdal.GDT_Int16,
            band_list=[1],
        )

    ds = None

    gdaltest.runexternal(
        gdal_contour_path
        + f" -a elev -i 10 {contour_orientation_tif} {contour_orientation1_shp}"
    )

    ds = ogr.Open(contour_orientation1_shp)

    expected_contours = [
        "LINESTRING ("
        + "1.628125 49.493749999999999,"
        + "1.63125 49.496875000000003,"
        + "1.63125 49.503124999999997,"
        + "1.628125 49.50625,"
        + "1.621875 49.50625,"
        + "1.61875 49.503124999999997,"
        + "1.61875 49.496875000000003,"
        + "1.621875 49.493749999999999,"
        + "1.628125 49.493749999999999)",
        "LINESTRING ("
        + "1.38125 49.496875000000003,"
        + "1.378125 49.493749999999999,"
        + "1.371875 49.493749999999999,"
        + "1.36875 49.496875000000003,"
        + "1.36875 49.503124999999997,"
        + "1.371875 49.50625,"
        + "1.378125 49.50625,"
        + "1.38125 49.503124999999997,"
        + "1.38125 49.496875000000003)",
    ]
    expected_elev = [10, 20]

    with ds.ExecuteSQL("select * from contour_orientation1 order by elev asc") as lyr:

        assert lyr.GetFeatureCount() == len(expected_contours)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            expected_geom = ogr.CreateGeometryFromWkt(expected_contours[i])
            assert feat.GetField("elev") == expected_elev[i]
            ogrtest.check_feature_geometry(feat, expected_geom, 0.01)

            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test contour orientation


def test_gdal_contour_5(gdal_contour_path, tmp_path):

    ds = None

    contour_orientation2_shp = str(tmp_path / "contour_orientation2.shp")

    gdaltest.runexternal(
        gdal_contour_path
        + f" -a elev -i 10 data/contour_orientation.tif {contour_orientation2_shp}"
    )

    ds = ogr.Open(contour_orientation2_shp)

    expected_contours = [
        "LINESTRING (0.0 1.999999,"
        + "0.5 1.999999,"
        + "1.5 1.999999,"
        + "1.95454293244555 2.5,"
        + "2.1249976158233 3.5,"
        + "1.5 3.9545460850748,"
        + "0.5 4.06666564941406,"
        + "0.0 4.06666564941406)"
    ]
    expected_elev = [140]

    with ds.ExecuteSQL("select * from contour_orientation2 order by elev asc") as lyr:

        assert lyr.GetFeatureCount() == len(expected_contours)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            expected_geom = ogr.CreateGeometryFromWkt(expected_contours[i])
            assert feat.GetField("elev") == expected_elev[i]
            ogrtest.check_feature_geometry(feat, expected_geom)

            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test missing -fl, -i or -e


def test_gdal_contour_missing_fl_i_or_e(gdal_contour_path, testdata_tif, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_contour_path + f" {testdata_tif} {contour_shp}"
    )
    assert "One of -i, -fl or -e must be specified." in err


###############################################################################
# Test -fl can be used with -i


def test_gdal_contour_fl_and_i(gdal_contour_path, testdata_tif, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_contour_path + f" -a elev -fl 6 16 -i 10 {testdata_tif} {contour_shp}"
    )

    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(contour_shp)

    with ds.ExecuteSQL("select elev from contour order by elev asc") as lyr:

        expected_heights = [6, 10, 16, 20]

        assert lyr.GetFeatureCount() == len(expected_heights)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetField("elev") == expected_heights[i]
            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test -fl can be used with -e real DEM


def test_gdal_contour_fl_e(gdal_contour_path, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    gdaltest.runexternal(
        gdal_contour_path
        + f" -a elev -fl 76 112 441 -e 3 ../gdrivers/data/n43.tif {contour_shp}"
    )

    ds = ogr.Open(contour_shp)

    with ds.ExecuteSQL("select distinct elev from contour order by elev asc") as lyr:

        expected_heights = [76, 81, 112, 243, 441]

        assert lyr.GetFeatureCount() == len(expected_heights)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetField("elev") == expected_heights[i]
            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test -off does not apply to -fl


def test_gdal_contour_fl_ignore_off(gdal_contour_path, testdata_tif, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_contour_path
        + f" -a elev -fl 6 16 -off 2 -i 10 {testdata_tif} {contour_shp}"
    )

    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(contour_shp)

    with ds.ExecuteSQL("select elev from contour order by elev asc") as lyr:

        expected_heights = [2, 6, 12, 16, 22]

        assert lyr.GetFeatureCount() == len(expected_heights)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetField("elev") == expected_heights[i]
            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test there are no duplicated levels when -fl is used together with -i


def test_gdal_contour_fl_and_i_no_dups(gdal_contour_path, testdata_tif, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_contour_path + f" -a elev -fl 6 16 20 -i 10 {testdata_tif} {contour_shp}"
    )

    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(contour_shp)

    with ds.ExecuteSQL("select elev from contour order by elev asc") as lyr:

        expected_heights = [6, 10, 16, 20]

        assert lyr.GetFeatureCount() == len(expected_heights)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetField("elev") == expected_heights[i]
            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test interval with polygonize


def test_gdal_contour_i_polygonize(gdal_contour_path, testdata_tif, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_contour_path
        + f" -amin elev -amax elev2 -i 5 -p {testdata_tif} {contour_shp}"
    )

    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(contour_shp)

    with ds.ExecuteSQL("select elev, elev2 from contour order by elev asc") as lyr:

        # Raster max is 25 so the last contour is 20 (with amax of 25)
        expected_heights = [0, 5, 10, 15, 20]

        assert lyr.GetFeatureCount() == len(expected_heights)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetField("elev") == expected_heights[i]
            assert feat.GetField("elev2") == expected_heights[i] + 5
            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test there are no duplicated levels when -fl is used together with -i
# and polygonize


def test_gdal_contour_fl_and_i_no_dups_polygonize(
    gdal_contour_path, testdata_tif, tmp_path
):

    contour_shp = str(tmp_path / "contour.shp")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_contour_path
        + f" -amin elev -amax elev2 -fl 6 16 20 -i 5 -p {testdata_tif} {contour_shp}"
    )

    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(contour_shp)

    with ds.ExecuteSQL("select elev, elev2 from contour order by elev asc") as lyr:

        # Raster max is 25 so the last contour is 20 (with amax of 25)
        expected_heights = [0, 5, 6, 10, 15, 16, 20]

        assert lyr.GetFeatureCount() == len(expected_heights)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetField("elev") == expected_heights[i]
            assert (
                feat.GetField("elev2") == expected_heights[i + 1]
                if i < len(expected_heights) - 2
                else expected_heights[i] + 5
            )
            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test -e with -fl and polygonize


def test_gdal_contour_fl_e_polygonize(gdal_contour_path, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    gdaltest.runexternal(
        gdal_contour_path
        + f" -p -amin elev -amax elev2 -fl 76 112 441 -e 3 ../gdrivers/data/n43.tif {contour_shp}"
    )

    ds = ogr.Open(contour_shp)

    with ds.ExecuteSQL("select elev, elev2 from contour order by elev asc") as lyr:

        # Raster min is 75, max is 460
        expected_heights = [75, 76, 81, 112, 243, 441]

        assert lyr.GetFeatureCount() == len(expected_heights)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetField("elev") == expected_heights[i]
            assert (
                feat.GetField("elev2") == expected_heights[i + 1]
                if i < len(expected_heights) - 2
                else 460
            )
            i = i + 1
            feat = lyr.GetNextFeature()


###############################################################################
# Test -gt


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("gt", ["0", "1", "unlimited"])
def test_gdal_contour_gt(gdal_contour_path, tmp_path, gt):

    out_filename = str(tmp_path / "contour.gpkg")

    gdaltest.runexternal(
        gdal_contour_path
        + f" -p -amin elev -amax elev2 -fl 76 112 441 -e 3 -gt {gt} ../gdrivers/data/n43.tif {out_filename}"
    )

    ds = ogr.Open(out_filename)

    with ds.ExecuteSQL("select elev, elev2 from contour order by elev asc") as lyr:

        # Raster min is 75, max is 460
        expected_heights = [75, 76, 81, 112, 243, 441]

        assert lyr.GetFeatureCount() == len(expected_heights)


###############################################################################
# Test there MIN and MAX values are correctly computed with polygonize fixed
# levels


def test_gdal_contour_fl_and_i__polygonize(gdal_contour_path, testdata_tif, tmp_path):

    contour_shp = str(tmp_path / "contour.shp")

    try:
        os.remove(contour_shp)
    except OSError:
        pass

    _, err = gdaltest.runexternal_out_and_err(
        gdal_contour_path
        + f" -amin elev -amax elev2 -fl MIN 6 16 20 MAX -p {testdata_tif} {contour_shp}"
    )

    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(contour_shp)

    with ds.ExecuteSQL("select elev, elev2 from contour order by elev asc") as lyr:

        expected_heights = [0, 6, 16, 20]

        assert lyr.GetFeatureCount() == len(expected_heights)

        i = 0
        feat = lyr.GetNextFeature()
        while feat is not None:
            assert feat.GetField("elev") == expected_heights[i]
            assert (
                feat.GetField("elev2") == expected_heights[i + 1]
                if i < len(expected_heights) - 2
                else expected_heights[i] + 5
            )
            i = i + 1
            feat = lyr.GetNextFeature()
