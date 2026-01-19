#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ContourGenerate() testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr

size = 160
precision = 1.0 / size


@pytest.fixture()
def input_tif(tmp_path):

    tif_fname = str(tmp_path / "gdal_contour.tif")

    drv = gdal.GetDriverByName("GTiff")
    wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]]'

    ds = drv.Create(tif_fname, size, size, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([1, precision, 0, 50, 0, -precision])

    ds.GetRasterBand(1).Fill(1)

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

    return tif_fname


###############################################################################
# Test with -a and -i options


def test_contour_1(input_tif, tmp_path):

    output_shp = str(tmp_path / "contour.shp")

    ogr_ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(output_shp)
    ogr_lyr = ogr_ds.CreateLayer("contour")
    field_defn = ogr.FieldDefn("ID", ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("elev", ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open(input_tif)

    gdal.ContourGenerate(ds.GetRasterBand(1), 10, 0, [], 0, 0, ogr_lyr, 0, 1)

    ds = None

    expected_envelopes = [
        [1.25, 1.75, 49.25, 49.75],
        [1.25 + 0.125, 1.75 - 0.125, 49.25 + 0.125, 49.75 - 0.125],
    ]
    expected_height = [10, 20]

    lyr = ogr_ds.ExecuteSQL("select * from contour order by elev asc")

    assert lyr.GetFeatureCount() == len(expected_envelopes)

    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        envelope = feat.GetGeometryRef().GetEnvelope()
        assert feat.GetField("elev") == expected_height[i]
        for j in range(4):
            if expected_envelopes[i][j] != pytest.approx(
                envelope[j], abs=precision / 2 * 1.001
            ):
                print("i=%d, wkt=%s" % (i, feat.GetGeometryRef().ExportToWkt()))
                print(feat.GetGeometryRef().GetEnvelope())
                pytest.fail(
                    "%f, %f" % (expected_envelopes[i][j] - envelope[j], precision / 2)
                )
        i = i + 1
        feat = lyr.GetNextFeature()

    ogr_ds.ReleaseResultSet(lyr)


###############################################################################
# Test with -fl option and -3d option


def test_contour_2(input_tif, tmp_path):

    output_shp = str(tmp_path / "contour.shp")

    ogr_ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(output_shp)
    ogr_lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbLineString25D)
    field_defn = ogr.FieldDefn("ID", ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("elev", ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open(input_tif)
    gdal.ContourGenerate(ds.GetRasterBand(1), 0, 0, [10, 20, 25], 0, 0, ogr_lyr, 0, 1)
    ds = None

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

    lyr = ogr_ds.ExecuteSQL("select * from contour order by elev asc")

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
                    "%f, %f" % (expected_envelopes[i][j] - envelope[j], precision / 2)
                )
        i = i + 1
        feat = lyr.GetNextFeature()

    ogr_ds.ReleaseResultSet(lyr)


###############################################################################
#


def test_contour_real_world_case():

    ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    ogr_lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbLineString)
    field_defn = ogr.FieldDefn("ID", ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("elev", ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open("data/contour_in.tif")
    gdal.ContourGenerate(ds.GetRasterBand(1), 10, 0, [], 0, 0, ogr_lyr, 0, 1)
    ds = None

    ogr_lyr.SetAttributeFilter("elev = 330")
    assert ogr_lyr.GetFeatureCount() == 1
    f = ogr_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING (4.50497512437811 11.5,4.5 11.501996007984,3.5 11.8333333333333,2.5 11.5049751243781,2.490099009901 11.5,2.0 10.5,2.5 10.1666666666667,3.0 9.5,3.5 9.21428571428571,4.49800399201597 8.5,4.5 8.49857346647646,5.5 8.16666666666667,6.5 8.0,7.5 8.0,8.0 7.5,8.5 7.0,9.490099009901 6.5,9.5 6.49667774086379,10.5 6.16666666666667,11.4950248756219 5.5,11.5 5.49833610648919,12.5 5.49667774086379,13.5 5.49800399201597,13.501996007984 5.5,13.5 5.50199600798403,12.501996007984 6.5,12.5 6.50142653352354,11.5 6.509900990099,10.509900990099 7.5,10.5 7.50142653352354,9.5 7.9,8.50332225913621 8.5,8.5 8.50249376558603,7.83333333333333 9.5,7.5 10.0,7.0 10.5,6.5 10.7857142857143,5.5 11.1666666666667,4.50497512437811 11.5)",
        0.01,
    )


# Test with -p option (polygonize)
@pytest.mark.parametrize(
    "fixed_levels, expected_min, expected_max",
    [
        ("10,20", [10], [20]),
        ("0,20", [0], [20]),
        ("20,1000", [20], [1000]),
        ("20", [], []),  # Nothing to do here!
        ("min,20", [1], [20]),
        ("min,max", [1], [25]),
        ("min,25", [1], [25]),
        ("min,10,max", [1, 10], [10, 25]),
    ],
)
def test_contour_polygonize(
    input_tif, tmp_path, fixed_levels, expected_min, expected_max
):
    """This tests the min/max values of the polygonize option in simple cases
    without testing the geometry itself."""

    output_shp = str(tmp_path / "contour_polygonize.shp")
    ogr_ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(output_shp)
    ogr_lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbMultiPolygon)
    field_defn = ogr.FieldDefn("ID", ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("elevMin", ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("elevMax", ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open(input_tif)
    gdal.ContourGenerateEx(
        ds.GetRasterBand(1),
        ogr_lyr,
        options=[
            "FIXED_LEVELS=" + fixed_levels,
            "ID_FIELD=0",
            "ELEV_FIELD_MIN=1",
            "ELEV_FIELD_MAX=2",
            "POLYGONIZE=TRUE",
        ],
    )
    ds = None

    with ogr_ds.ExecuteSQL(
        "select * from contour_polygonize order by elevMin asc"
    ) as lyr:

        # Get the values from the layer
        values = [[], [], []]
        for feat in lyr:
            values[0].append(feat.GetField("elevMin"))
            values[1].append(feat.GetField("elevMax"))
            values[2].append(feat.GetGeometryRef().GetEnvelope())

        assert len(values[0]) == len(expected_min), (
            values[0],
            values[1],
            values[2],
            expected_min,
            expected_max,
        )

        i = 0
        for valMin, valMax in zip(values[0], values[1]):

            assert valMin == pytest.approx(
                expected_min[i], abs=precision / 2 * 1.001
            ), i
            assert valMax == pytest.approx(
                expected_max[i], abs=precision / 2 * 1.001
            ), i
            i = i + 1


@pytest.mark.parametrize(
    "fixed_levels, expected_min, expected_max",
    [
        ("-10,0,10,20,25,30,40", [0, 10, 20, 25], [10, 20, 25, 30]),
        ("-10,0,10,20,25,30", [0, 10, 20, 25], [10, 20, 25, 30]),
        ("0,10,20,25,30", [0, 10, 20, 25], [10, 20, 25, 30]),
        ("1,10,20,25,30", [1, 10, 20, 25], [10, 20, 25, 30]),
        ("0,10,20,24,25", [0, 10, 20, 24], [10, 20, 24, 25]),
        ("0,10,20,25", [0, 10, 20], [10, 20, 25]),
    ],
)
def test_contour_3(input_tif, tmp_path, fixed_levels, expected_min, expected_max):
    """This tests the min/max values of the polygonize option and the geometry itself."""

    output_shp = str(tmp_path / "contour.shp")

    ogr_ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(output_shp)
    ogr_lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbMultiPolygon)
    field_defn = ogr.FieldDefn("ID", ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("elevMin", ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn("elevMax", ogr.OFTReal)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open(input_tif)
    gdal.ContourGenerateEx(
        ds.GetRasterBand(1),
        ogr_lyr,
        options=[
            "FIXED_LEVELS=" + fixed_levels,
            "ID_FIELD=0",
            "ELEV_FIELD_MIN=1",
            "ELEV_FIELD_MAX=2",
            "POLYGONIZE=TRUE",
        ],
    )
    ds = None

    expected_envelopes = [
        [1.0, 2.0, 49.0, 50.0],
        [1.25, 1.75, 49.25, 49.75],
        [1.25 + 0.125, 1.75 - 0.125, 49.25 + 0.125, 49.75 - 0.125],
        [
            1.25 + 0.125 + 0.0625,
            1.75 - 0.125 - 0.0625,
            49.25 + 0.125 + 0.0625,
            49.75 - 0.125 - 0.0625,
        ],
    ]
    if len(expected_min) < len(expected_envelopes):
        expected_envelopes = expected_envelopes[0 : len(expected_min)]

    with ogr_ds.ExecuteSQL("select * from contour order by elevMin asc") as lyr:

        assert lyr.GetFeatureCount() == len(expected_envelopes)

        i = 0
        for feat in lyr:

            assert feat.GetField("elevMin") == pytest.approx(
                expected_min[i], abs=precision / 2 * 1.001
            ), i
            assert feat.GetField("elevMax") == pytest.approx(
                expected_max[i], abs=precision / 2 * 1.001
            ), i

            envelope = feat.GetGeometryRef().GetEnvelope()
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


# Check behaviour when the nodata value as a double isn't exactly the Float32 pixel value
def test_contour_nodata_precision_issue_float32():

    ogr_ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        "/vsimem/contour.shp"
    )
    ogr_lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbLineString)
    field_defn = ogr.FieldDefn("ID", ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)

    ds = gdal.Open("data/nodata_precision_issue_float32.tif")
    gdal.ContourGenerateEx(
        ds.GetRasterBand(1),
        ogr_lyr,
        options=[
            "LEVEL_INTERVAL=0.1",
            "ID_FIELD=0",
            "NODATA=%.19g" % ds.GetRasterBand(1).GetNoDataValue(),
        ],
    )
    ds = None
    assert ogr_lyr.GetFeatureCount() == 0
    ogr_ds = None
    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource("/vsimem/contour.shp")


@pytest.mark.require_driver("AAIGRID")
def test_contour_too_many_levels():

    ogr_ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        "/vsimem/contour.shp"
    )
    ogr_lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbLineString)
    field_defn = ogr.FieldDefn("ID", ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)

    content1 = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
 1e30 0
 0 0"""

    content2 = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
 1e6 0
 0 0"""
    for content in (content1, content2):

        with gdaltest.tempfile("/vsimem/test.asc", content):
            ds = gdal.Open("/vsimem/test.asc")
            with pytest.raises(Exception):
                gdal.ContourGenerateEx(
                    ds.GetRasterBand(1),
                    ogr_lyr,
                    options=["LEVEL_INTERVAL=1", "ID_FIELD=0"],
                )

        with gdaltest.tempfile("/vsimem/test.asc", content):
            ds = gdal.Open("/vsimem/test.asc")
            with pytest.raises(Exception):
                gdal.ContourGenerateEx(
                    ds.GetRasterBand(1),
                    ogr_lyr,
                    options=[
                        "LEVEL_INTERVAL=1",
                        "LEVEL_EXP_BASE=1.0001",
                        "ID_FIELD=0",
                    ],
                )

    ogr_ds = None
    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource("/vsimem/contour.shp")


###############################################################################


def test_contour_raster_acquisition_error():

    ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    ogr_lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbLineString)
    field_defn = ogr.FieldDefn("ID", ogr.OFTInteger)
    ogr_lyr.CreateField(field_defn)
    ds = gdal.Open("../gcore/data/byte_truncated.tif")

    with pytest.raises(Exception):
        gdal.ContourGenerateEx(
            ds.GetRasterBand(1), ogr_lyr, options=["LEVEL_INTERVAL=1", "ID_FIELD=0"]
        )


###############################################################################


def test_contour_invalid_LEVEL_INTERVAL():

    ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    ogr_lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbLineString)
    ds = gdal.Open("../gcore/data/byte.tif")

    with pytest.raises(Exception, match="Invalid value for LEVEL_INTERVAL"):
        gdal.ContourGenerateEx(
            ds.GetRasterBand(1), ogr_lyr, options=["LEVEL_INTERVAL=-1"]
        )


###############################################################################
# Test scenario of https://github.com/OSGeo/gdal/issues/10167


@pytest.mark.require_driver("AAIGRID")
def test_contour_min_value_is_multiple_of_interval(tmp_vsimem):

    ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbLineString)
    lyr.CreateField(ogr.FieldDefn("ID", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("ELEV", ogr.OFTReal))

    content = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
1 3
1 3"""

    srcfilename = str(tmp_vsimem / "test.asc")
    with gdaltest.tempfile(srcfilename, content):
        ds = gdal.Open(srcfilename)
        gdal.ContourGenerateEx(
            ds.GetRasterBand(1),
            lyr,
            options=["LEVEL_INTERVAL=1", "ID_FIELD=0", "ELEV_FIELD=1"],
        )

    f = lyr.GetNextFeature()
    assert f["ELEV"] == 2
    ogrtest.check_feature_geometry(f, "LINESTRING (1.0 0.0,1.0 0.5,1.0 1.5,1.0 2.0)")

    f = lyr.GetNextFeature()
    assert f["ELEV"] == 3
    ogrtest.check_feature_geometry(f, "LINESTRING (1.5 0.0,1.5 0.5,1.5 1.5,1.5 2.0)")


###############################################################################
# Test scenario of https://github.com/OSGeo/gdal/issues/11340


def test_contour_constant_raster_value(tmp_vsimem):

    ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbLineString)
    lyr.CreateField(ogr.FieldDefn("ID", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("ELEV", ogr.OFTReal))

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    assert (
        gdal.ContourGenerateEx(
            src_ds.GetRasterBand(1),
            lyr,
            options=["LEVEL_INTERVAL=10", "ID_FIELD=0", "ELEV_FIELD=1"],
        )
        == gdal.CE_None
    )

    f = lyr.GetNextFeature()
    assert f is None


###############################################################################
# Test scenario of https://github.com/OSGeo/gdal/issues/11564


@pytest.mark.parametrize(
    "options, polygonize, expected_elev_values",
    [
        (["LEVEL_INTERVAL=10"], "TRUE", [(4, 10), (10, 20), (20, 30), (30, 36)]),
        (
            ["FIXED_LEVELS=15", "LEVEL_INTERVAL=10"],
            "TRUE",
            [(4, 10), (10, 15), (15, 20), (20, 30), (30, 36)],
        ),
    ],
)
@pytest.mark.require_driver("AAIGRID")
def test_contour_lowest_fixed_value(
    tmp_vsimem, options, polygonize, expected_elev_values
):

    content = """ncols        2
nrows        2
xllcorner    0
yllcorner    0
cellsize     1
4 15
25 36"""

    srcfilename = str(tmp_vsimem / "test.asc")

    def _create_output_ds():
        ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
        lyr = ogr_ds.CreateLayer("contour", geom_type=ogr.wkbLineString)
        lyr.CreateField(ogr.FieldDefn("ID", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("ELEV_MIN", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("ELEV_MAX", ogr.OFTReal))
        return ogr_ds, lyr

    with gdaltest.tempfile(srcfilename, content):

        src_ds = gdal.Open(srcfilename)
        ogr_ds, lyr = _create_output_ds()

        options.extend(
            [
                "ID_FIELD=0",
                "ELEV_FIELD_MIN=1",
                "ELEV_FIELD_MAX=2",
                f"POLYGONIZE={polygonize}",
            ]
        )

        assert (
            gdal.ContourGenerateEx(src_ds.GetRasterBand(1), lyr, options=options)
            == gdal.CE_None
        )

        # Get all elev values
        elev_values = []
        for f in lyr:
            elev_values.append((f["ELEV_MIN"], f["ELEV_MAX"]))

        assert elev_values == expected_elev_values, (elev_values, expected_elev_values)
